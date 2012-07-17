/**
 * Common/shared functionality
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_base_included
 #endinput
#endif
#define _wintox_base_included

#pragma tabsize 4
#pragma semicolon 1

// Include this first, so our declarations can use game-specific defines
#include "./wintox/game"

// Amount of ticks to skip in between sending hint texts
new g_TimerDisplaySkipTicks = 8;

// This is true after OnPluginEnd (used to properly discard menu handles, as
// even after OnPluginEnd, their MenuHandlers are still called with MenuAction_Cancel)
new bool:g_wintoxExiting = false;
// True when the plug-in is late-loaded
new bool:g_wintoxLateLoad = false;
// True when the game mode supports TempEnts (available some time after Wintox_Init)
new bool:g_wintoxTempEnts = false;
new g_CurrentTick = 0;

// For each player, whether other players should be hidden (not transmitted)
new bool:g_HideOtherPlayers[MAXPLAYERS] = { false, ... };
// For each player, whether other players (except the other racer) should be hidden (not transmitted)
new bool:g_RaceHideOtherPlayers[MAXPLAYERS] = { false, ... };
// Holds the state of wintox_remove_dropped_weapons, whether to remove dropped weapon entities
new bool:g_RemoveDroppedWeapons = true;
// Holds the state of wintox_suppress_hint, whether to actiely suppress hint text
// if the player disables the hint text timer
new bool:g_SuppressHintText = true;
// Holds the state of wintox_max_prespeed_velocity, the max velocity allowed in a start zone
new Float:g_MaxPrespeedVelocity = 378.0;

// Database IDs for each player
new g_PlayerIDs[MAXPLAYERS] = { -1, ... };

// fps_max setting for each client
new g_ClientFpsMax[MAXPLAYERS] = { -1, ... };

#if defined(WINTOX_BHOP)
// Stores convar state for wintox_lowgrav_multiplier
new Float:g_LowGravMultiplier = 0.5;
// Keeps track of who has low gravity enabled
new bool:g_PlayerLowGrav[MAXPLAYERS] = false;
#endif

#if !defined(FEATURECAP_TEMPENTS)
    #define FEATURECAP_TEMPENTS "tempents"
#endif

#include <sourcemod>
#include <sdkhooks>

#if defined(WINTOX_BHOP)
#include "./wintox/teleport"
#endif

#include "./wintox/commands"
#include "./wintox/sql"
#include "./wintox/utils"
#include "./wintox/events"
#include "./wintox/menus"
#include "./wintox/zones"
#include "./wintox/race"


Wintox_Init()
{
    LoadTranslations("common.phrases");
    
    Menus_Init();
    Commands_Init();
    
#if defined(WINTOX_BHOP)
    Teleport_Init();
#endif
    
    win_SQL_Init();
    Events_Init();
    Utils_Init();
    Zones_Init();
    Timer_Init();
    Race_Init();
    
    RegisterCommand("sm_hide", ConCommand_Hide, "sm_hide | Toggle hiding all other players (useful for demos).");
    RegisterCommand("sm_show", ConCommand_Show, "sm_show | Show all other players (counteracts sm_hide).");
    RegisterCommand("sm_fps", ConCommand_Fps, "sm_fps | Show a menu displaying the fps_max setting for all the players in the server.");
    RegisterCommand("sm_specinfo", ConCommand_SpecInfo, "sm_specinfo | Show a menu displaying all the players spectating you");

#if defined(WINTOX_BHOP)
    RegisterCommand("sm_scout", ConCommand_Scout, "sm_scout | Gives a scout.");
    RegisterCommand("sm_usp", ConCommand_USP, "sm_usp | Gives a scout.");
    RegisterCommand("sm_lowgrav", ConCommand_LowGrav, "sm_lowgrav | Toggles low gravity. Low gravity stops your timer.");
    RegisterCommand("sm_normalgrav", ConCommand_NormalGrav, "sm_normalgrav | Disables low gravity.");
    
    new Handle:cvar_low_grav = CreateConVar("wintox_lowgrav_multiplier", "0.5", "The gravity multiplier for the !lowgrav command (1.0 is normal, 0.5 is half grav)");
    g_LowGravMultiplier = GetConVarFloat(cvar_low_grav);
    HookConVarChange(cvar_low_grav, ConVar_LowGravMultiplier);
#endif
    
    if (g_wintoxLateLoad)
    {
        // Just for good measure, let's discover the low grav state of all the clients
        for (new client=1; client<MaxClients; client++)
        {
            if (!IsClientConnected(client) || !IsClientInGame(client))
                continue;
            
#if defined(WINTOX_BHOP)
            if (GetEntPropFloat(client, Prop_Data, "m_flGravity") == g_LowGravMultiplier)
                g_PlayerLowGrav[client] = true;
#endif
        }
    }
    
    new bool:manual_test = true;
    if (CanTestFeatures())
    {
        new FeatureStatus:status = GetFeatureStatus(FeatureType_Capability, FEATURECAP_TEMPENTS);
        
        if (status != FeatureStatus_Unknown)
        {
            g_wintoxTempEnts = (status == FeatureStatus_Available);
            manual_test = false;
        }
    }
    
    /* Manual test. This is done in a separate call stack (timer thread), so
     * as not to derail the current call stack if a TempEnt native raises an
     * error (which should occur when tempents are unsupported)
     */
    if (manual_test)
        CreateTimer(0.1, Timer_TestTempEnts);
}

Wintox_Exit()
{
    g_wintoxExiting = true;
}

Wintox_RegisterConVars()
{
    CreateConVar("wintox_gametype", "", "The game type of Wintox which should run on this server. This is to prevent the loading of multiple game types. Leave blank to disable.");
    
    new Handle:debug_level = CreateConVar("wintox_debug_level", "0", "The level of debug messages to print. The higher the number, the more is printed. -1 = none (not recommended), 0 = error, 1 = warning, 2 = info, 3 = debug, 4 = CRAAAZY debug", 0, true, -1.0, true, 4.0);
    g_DebugLevel = GetConVarInt(debug_level);
    HookConVarChange(debug_level, ConVar_DebugLevel);
    
    new Handle:timer_skip_ticks = CreateConVar("wintox_timer_display_skip_ticks", "8", "Amount of ticks to skip in between refreshing the timer display (the higher the number, the slower the refresh rate)", 0, true, 1.0);
    g_TimerDisplaySkipTicks = GetConVarInt(timer_skip_ticks);
    HookConVarChange(timer_skip_ticks, ConVar_TimerDisplaySkipTicks);
    
    new Handle:remove_dropped_weapons = CreateConVar("wintox_remove_dropped_weapons", "1", "Whether or not to remove weapons that are dropped by players", 0, true, 0.0, true, 1.0);
    g_RemoveDroppedWeapons = GetConVarBool(remove_dropped_weapons);
    HookConVarChange(remove_dropped_weapons, ConVar_RemoveDroppedWeapons);
    
    new Handle:suppress_hint_text = CreateConVar("wintox_suppress_hint_text", "1", "Whether or not to actively suppress the hint text box if the user disables the timer display (if false, hint text boxes take a few seconds to disappear)", 0, true, 0.0, true, 1.0);
    g_SuppressHintText = GetConVarBool(suppress_hint_text);
    HookConVarChange(suppress_hint_text, ConVar_SuppressHintText);
    
    new Handle:max_prespeed_velocity = CreateConVar("wintox_max_prespeed_velocity", "378.0", "The maximum velocity allowed in a start zone.");
    g_MaxPrespeedVelocity = GetConVarFloat(max_prespeed_velocity);
    HookConVarChange(max_prespeed_velocity, ConVar_MaxPrespeedVelocity);
    
    Timer_RegisterConVars();
    Zone_RegisterConVars();
    Menus_RegisterConVars();
    Race_RegisterConVars();
    win_SQL_RegisterConVars();
}

Wintox_DeinitPlayer(client)
{
    g_ClientFpsMax[client] = -1;
    
#if defined(WINTOX_BHOP)
    g_PlayerLowGrav[client] = false;
#endif
}

public Action:Timer_TestTempEnts(Handle:timer)
{
    new clients[1];
    
    PrintToServer("[WTX] Testing TempEnts system. If they're unsupported, an error will be printed by SourceMod. This is expected.");
    
    // The simplest test
    TE_Start("BeamPoints");
    TE_Send(clients, 0);
    
    g_wintoxTempEnts = true;
    
    return Plugin_Continue;
}

public ConVar_DebugLevel(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DebugLevel = GetConVarInt(cvar);
}

public ConVar_TimerDisplaySkipTicks(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    new rate = GetConVarInt(cvar);
    g_TimerDisplaySkipTicks = MAX(rate, 1);
}

public ConVar_RemoveDroppedWeapons(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RemoveDroppedWeapons = GetConVarBool(cvar);
}

public ConVar_SuppressHintText(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_SuppressHintText = GetConVarBool(cvar);
}

#if defined(WINTOX_BHOP)
public ConVar_LowGravMultiplier(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_LowGravMultiplier = GetConVarFloat(cvar);
}
#endif

public ConVar_MaxPrespeedVelocity(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_MaxPrespeedVelocity = GetConVarFloat(cvar);
}

public OnGameFrame()
{
    g_CurrentTick++;
    
    for (new client = 1; client <= MaxClients; client++)
    {
        if (!IsClientConnected(client) || !IsClientInGame(client) || IsFakeClient(client))
            continue;
        
        if (!IS_PLAYING_TEAM(GetClientTeam(client)))
            continue;
        
        decl Float:origin[3];
        new bool:is_in_zone = false;
        
        GetClientAbsOrigin(client, origin);
        
        new track = g_PlayerCurrentTrack[client];
        
        if (!IS_PAUSED(client) && PlayerCanTime(client))
        {
            for (new check_track=0; check_track<MAX_TRACKS; check_track++)
            {
                if (PointInZone(origin, check_track, 0))
                {
                    // They've switched tracks
                    if (check_track != track)
                    {
                        ResetTimer(client);
                        g_PlayerCurrentTrack[client] = check_track;
                        track = check_track;
                        
                        if (check_track == 0)
                            Timer_PrintToChat(client, "You've switched to the {G}main track{N}. Your timer has been reset.");
                        else
                        {
                            decl String:bonus_name[96];
                            bonus_name[0] = '\0';
                            if (!IS_STR_EMPTY(g_TrackNames[track]))
                                Format(STR(bonus_name), " ({OG}%s{N})", g_TrackNames[track]);
                            
                            Timer_PrintToChat(client, "You've switched to {G}bonus track %d{N}%s. Your timer has been reset.", check_track, bonus_name);
                        }
                    }
                    else if (((g_PlayerCurrentZone[client] >= 0 && g_IsTiming[client]) || (IS_END_ZONE_T(track,g_PlayerCurrentZone[client])) && g_PlayerCurrentZone[client] > 0) && !IsRacing(client))
                    {
                        ResetTimer(client);
                        g_PlayerCurrentTrack[client] = track;
                        Timer_PrintToChat(client, "Your timer has been reset by passing through the start zone.");
                    }
                    
                    break;
                }
            }
            
            new start_zone = g_PlayerCurrentPath[client] == INVALID_PATH ? 0 : g_TrackPaths[g_PlayerCurrentTrack[client]][g_PlayerCurrentPath[client]];
            new end_zone = g_TrackPaths[g_PlayerCurrentTrack[client]][g_PlayerCurrentPath[client] + 1];
            for (new i=start_zone, _t=-1; _t<1 && ((i=_t ? start_zone : end_zone) || 1); _t++)
            {
                if ((g_PlayerEnteredStart[client] || i == 0) && PointInZone(origin, track, i))
                {
                    g_PlayerCurrentlyInZone[client] = true;
                    
                    if (!PLAYER_IN_ZONE(client,i) && i != 0)
                    {
                        if (!IsRacing(client))
                            Timer_PlayerReachedZone(client, track, i, g_PlayerCurrentStyle[client]);
                        else
                            Race_PlayerReachedZone(client, track, i);
                    }
                    
                    if (i == 0)
                    {
                        g_PlayerEnteredStart[client] = true;
                        
                        // Anti-pre-speed
                        decl Float:velocity[3];
                        Entity_GetAbsVelocity(client, velocity);
                        
                        if (GetVectorLength(velocity) > g_MaxPrespeedVelocity)
                        {
                            NormalizeVector(velocity, velocity);
                            ScaleVector(velocity, g_MaxPrespeedVelocity);
                            Entity_SetAbsVelocity(client, velocity);
                        }
                    }
                    
                    // Yeah, you go, player! YOU ROCK!
                    if (IS_END_ZONE_T(track,i) && (g_IsTiming[client] || IsRacing(client)))
                    {
                        if (!IsRacing(client))
                            Timer_PlayerReachedEnd(client, track, g_PlayerCurrentStyle[client]);
                        else
                            Race_PlayerReachedEnd(client, track);
                    }
                    
                    is_in_zone = true;
                    g_PlayerCurrentZone[client] = i;
                    break;
                }
                else if (g_PlayerCurrentZone[client] == 0 && g_StartTimes[client][0] == 0.0 && !IsRacing(client))
                {
                    // Start the timer
                    StartTimer(client);
                    g_IsTiming[client] = true;
                    
                    g_PlayerCurrentlyInZone[client] = true;
                }
                else
                    g_PlayerCurrentlyInZone[client] = false;
            }
        }
        
        // Display the timer hint text every other frame
        if (g_CurrentTick % g_TimerDisplaySkipTicks == 0)
        {
            if (g_TimerEnabled[client])
                DisplayTimer(client, is_in_zone);
            
            // Actively hide the hint text
            else if (g_SuppressHintText)
                HideHintText(client);
        }
    }
}


Handle:BuildFpsMenu()
{
    new Handle:menu = CreateMenu(MenuHandler_FpsMenu);
    SetMenuTitle(menu, "Player fps_max settings");
    
    for (new client=1; client<MaxClients; client++)
    {
        decl String:buffer[96];
        decl String:sz_userid[16];
        
        if (!IsClientConnected(client) || !IsClientInGame(client) || IsFakeClient(client))
            continue;
        
        IntToString(GetClientUserId(client), STR(sz_userid));
        if (g_ClientFpsMax[client] != -1)
            Format(STR(buffer), "%N - %d", client, g_ClientFpsMax[client]);
        else
            Format(STR(buffer), "%N - ?", client);
        
        AddMenuItem(menu, sz_userid, buffer);
    }
    
    return menu;
}

public MenuHandler_FpsMenu(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
        DisplayMenuAtItem(menu, param1, GetMenuSelectionPosition(), MENU_TIME_FOREVER);
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

Handle:BuildSpecMenu(client)
{
    new Handle:menu = CreateMenu(MenuHandler_FpsMenu);
    
    new observers = 0;
    LOOP_OBSERVERS(client,observer)
    {
        decl String:name[64];
        decl String:sz_userid[16];
        
        IntToString(GetClientUserId(observer), STR(sz_userid));
        GetClientName(observer, STR(name));
        
        AddMenuItem(menu, sz_userid, name);
        observers++;
    }
    
    // Add a menu item if there were no observers, to prevent crashes
    if (observers == 0)
        AddMenuItem(menu, "", "-");
    
    SetMenuTitle(menu, "%d players spectating you", observers);
    
    return menu;
}

public MenuHandler_SpecMenu(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        PrevSubmenu(param1, false);
        DisplaySubmenu(BuildSpecMenu(param1), param1, MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}


#define SHOW_OTHER_PLAYERS_MESSAGE "{OG}Showing{N} all other players."
public Action:ConCommand_Hide(client, args)
{
    NO_ARGS()
    
    g_HideOtherPlayers[client] = !g_HideOtherPlayers[client];
    
    if (g_HideOtherPlayers[client])
        Reply(client, "{OG}Hiding{N} all other players.");
    else
        Reply(client, SHOW_OTHER_PLAYERS_MESSAGE);
    
    return Plugin_Handled;
}

public Action:ConCommand_Show(client, args)
{
    NO_ARGS()
    
    g_HideOtherPlayers[client] = false;
    Reply(client, SHOW_OTHER_PLAYERS_MESSAGE);
    
    return Plugin_Handled;
}

public Action:Hook_SetTransmit(entity, client) 
{ 
    if (client != entity && (0 < entity <= MaxClients))
    {
        if (g_RaceHideOtherPlayers[client])
        {
            if (entity != g_RaceOpponents[client])
                return Plugin_Handled;
            else
                return Plugin_Continue;
        }
        
        if (g_HideOtherPlayers[client])
            return Plugin_Handled;
    }
     
    return Plugin_Continue; 
}

public Hook_WeaponDropPost(client, weapon) 
{ 
    if (g_RemoveDroppedWeapons && weapon != -1)
        Entity_Kill(weapon);
}

public Action:ConCommand_Fps(client, args)
{
    NO_ARGS()
    NO_FAKECLIENTS()
    
    DisplaySubmenu(BuildFpsMenu(), client, MENU_TIME_FOREVER);
    
    return Plugin_Handled;
}

public Action:ConCommand_SpecInfo(client, args)
{
    NO_ARGS()
    NO_FAKECLIENTS()
    
    DisplaySubmenu(BuildSpecMenu(client), client, MENU_TIME_FOREVER);
    
    return Plugin_Handled;
}

#if defined(WINTOX_BHOP)
public Action:ConCommand_Scout(client, args)
{
    NO_ARGS()
    
    Client_GiveWeaponAndAmmo(client, "weapon_scout", true, 90);
    Reply(client, "You got a {T}scout{N}. YES!");
    
    return Plugin_Handled;
}

public Action:ConCommand_USP(client, args)
{
    NO_ARGS()
    
    Client_GiveWeaponAndAmmo(client, "weapon_usp", true, 100);
    Reply(client, "You got a {T}USP{N}. BOOYAH!");
    
    return Plugin_Handled;
}

public Action:ConCommand_LowGrav(client, args)
{
    NO_ARGS()
    
    g_PlayerLowGrav[client] = !g_PlayerLowGrav[client];
    
    if (g_PlayerLowGrav[client])
    {
        ResetTimer(client);
        SetEntPropFloat(client, Prop_Data, "m_flGravity", g_LowGravMultiplier);
        Reply(client, "Low gravity enabled. Timer disabled. Use {T}!lowgrav{N} again or {T}!normalgrav{N} and enter the {T}start zone{N} to begin timing again.");
    }
    else
    {
        Reply(client, "Low gravity disabled. Enter the {T}start zone{N} to begin timing.");
        SetEntPropFloat(client, Prop_Data, "m_flGravity", 1.0);
    }
    
    return Plugin_Handled;
}

public Action:ConCommand_NormalGrav(client, args)
{
    NO_ARGS()
    
    SetEntPropFloat(client, Prop_Data, "m_flGravity", 1.0);
    g_PlayerLowGrav[client] = false;
    Reply(client, "Low gravity disabled. Enter the {T}start zone{N} to begin timing.");
    
    return Plugin_Handled;
}
#endif
