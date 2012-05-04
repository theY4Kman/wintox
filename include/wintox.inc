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

// Amount of ticks to skip in between sending hint texts
new g_TimerDisplaySkipTicks = 8;

// This is true after OnPluginEnd (used to properly discard menu handles, as
// even after OnPluginEnd, their MenuHandlers are still called with MenuAction_Cancel)
new bool:g_wintoxExiting = false;
// True when the plug-in is late-loaded
new bool:g_wintoxLateLoad = false;
new g_CurrentTick = 0;

// For each player, whether other players should be hidden (not transmitted)
new bool:g_HideOtherPlayers[MAXPLAYERS] = { false, ... };
// Holds the state of wintox_remove_dropped_weapons, whether to remove dropped weapon entities
new bool:g_RemoveDroppedWeapons = true;
// Holds the state of wintox_suppress_hint, whether to actiely suppress hint text
// if the player disables the hint text timer
new bool:g_SuppressHintText = true;

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

#include <sourcemod>
#include <sdkhooks>

#if defined(WINTOX_BHOP)
#include "./include/teleport"
#endif

#include "./include/game"
#include "./include/commands"
#include "./include/sql"
#include "./include/utils"
#include "./include/events"
#include "./include/menus"
#include "./include/zones"
#include "./include/race"

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
    
    AutoExecConfig(true, "wintox");
    
    new Handle:timer_skip_ticks = CreateConVar("wintox_timer_display_skip_ticks", "8", "Amount of ticks to skip in between refreshing the timer display (the higher the number, the slower the refresh rate)", 0, true, 1.0);
    g_TimerDisplaySkipTicks = GetConVarInt(timer_skip_ticks);
    HookConVarChange(timer_skip_ticks, ConVar_TimerDisplaySkipTicks);
    
    new Handle:remove_dropped_weapons = CreateConVar("wintox_remove_dropped_weapons", "1", "Whether or not to remove weapons that are dropped by players", 0, true, 0.0, true, 1.0);
    g_RemoveDroppedWeapons = GetConVarBool(remove_dropped_weapons);
    HookConVarChange(remove_dropped_weapons, ConVar_RemoveDroppedWeapons);
    
    new Handle:suppress_hint_text = CreateConVar("wintox_suppress_hint_text", "1", "Whether or not to actively suppress the hint text box if the user disables the timer display (if false, hint text boxes take a few seconds to disappear)", 0, true, 0.0, true, 1.0);
    g_SuppressHintText = GetConVarBool(suppress_hint_text);
    HookConVarChange(suppress_hint_text, ConVar_SuppressHintText);
    
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
            
            if (GetEntPropFloat(client, Prop_Data, "m_flGravity") == g_LowGravMultiplier)
                g_PlayerLowGrav[client] = true;
        }
    }
}

Wintox_Exit()
{
    g_wintoxExiting = true;
}

Wintox_DeinitPlayer(client)
{
    g_ClientFpsMax[client] = -1;
    
#if defined(WINTOX_BHOP)
    g_PlayerLowGrav[client] = false;
#endif
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
        
        // Check if the player is in their current or next zone only
        if (!IS_PAUSED(client) && PlayerCanTime(client))
        {
            new start_zone = g_PlayerCurrentPath[client] == INVALID_PATH ? 0 :g_TrackPaths[g_PlayerCurrentTrack[client]][g_PlayerCurrentPath[client]];
            new end_zone = g_TrackPaths[g_PlayerCurrentTrack[client]][g_PlayerCurrentPath[client] + 1];
            for (new i=start_zone; i<=end_zone; i++)
            {
                new sides = ZONE_POINTS_T[track][i];
                new j = sides-1 + ZONE_FIRST_POINT;
                new bool:odd_nodes = false;
                
                for (new h=ZONE_FIRST_POINT; h<(ZONE_META_POINTS + sides); h++)
                {
                    if ((ZONE_POINT_T[track][i][h][1] < origin[1] && ZONE_POINT_T[track][i][j][1] >= origin[1]) ||
                        (ZONE_POINT_T[track][i][j][1] < origin[1] && ZONE_POINT_T[track][i][h][1] >= origin[1]))
                        if (ZONE_POINT_T[track][i][h][0] + (origin[1] - ZONE_POINT_T[track][i][h][1]) / (ZONE_POINT[i][j][1] -
                            ZONE_POINT_T[track][i][h][1]) * (ZONE_POINT_T[track][i][j][0] - ZONE_POINT_T[track][i][h][0]) < origin[0])
                            odd_nodes = !odd_nodes;
                    
                    j = h;
                }
                
                // In zone AND in zone height
                if (odd_nodes &&
                    origin[2] >= (ZONE_POINT_T[track][i][ZONE_FIRST_POINT][2] - ZONE_HEIGHT_FUDGE) &&
                    origin[2] <= (ZONE_POINT_T[track][i][ZONE_FIRST_POINT][2] + ZONE_HEIGHT[i] + ZONE_HEIGHT_FUDGE))
                {
                    if (!PLAYER_IN_ZONE(client,i) && i != 0)
                        Timer_PlayerReachedZone(client, track, i);
                    
                    // Yeah, you go, player! YOU ROCK!
                    if (IS_END_ZONE(i) && g_IsTiming[client])
                        Timer_PlayerReachedEnd(client, track);
                    
                    is_in_zone = true;
                    g_PlayerCurrentZone[client] = i;
                    break;
                }
                else if (g_PlayerCurrentZone[client] == 0 && g_StartTimes[client][0] == 0.0)
                {
                    StartTimer(client);
                    g_IsTiming[client] = true;
                }
            }
        }
        
        // Display the timer hint text every other frame
        if (g_CurrentTick % g_TimerDisplaySkipTicks == 0)
        {
            if (g_TimerEnabled[client])
            {
                if (g_PlayerLowGrav[client])
                    PrintHintText(client, "[Disable low gravity to begin timing]");
                
                else if (g_PlayerCurrentZone[client] == INVALID_ZONE)
                    PrintHintText(client, "[Enter the start zone]");
                
                else if (is_in_zone && PLAYER_IN_ZONE(client,0) && !TIMER_STARTED(client))
                    PrintHintText(client, "[Leave the start zone]");
                
                else if (TIMER_STARTED(client))
                {
                    new Float:relative_from = GetEngineTime();
                    if (IS_FINISHED(client))
                        relative_from = g_StartTimes[client][g_EndZone[track]];
                    else if (IS_PAUSED(client))
                        relative_from = g_PauseStart[client];
                    
                    new Float:time = relative_from - g_StartTimes[client][0];
                    decl String:sz_time[24];
                    
                    decl String:plus_minus[64];
                    plus_minus[0] = '\0';
                    if (g_DisplayPlusMinus[client])
                    {
                        new Float:zone_time = g_StartTimes[client][g_PlayerCurrentZone[client]] - g_StartTimes[client][g_PlayerCurrentZone[client]-1];
                        
                        // TODO: select closest style
                        if (g_BestZoneRecords[track][g_PlayerCurrentZone[client]][g_PlayerCurrentStyle[client]] != 0.0)
                        {
                            new Float:zone_pm = zone_time - g_BestZoneRecords[track][g_PlayerCurrentZone[client]][g_PlayerCurrentStyle[client]];
                            
                            SecondsToTimeString(zone_pm, STR(sz_time), true);
                            Format(STR(plus_minus), " (%s)", sz_time);
                        }
                        
                        new Float:wr_pm = zone_time - g_TrackWorldRecord[track][g_PlayerCurrentZone[client]][g_PlayerCurrentStyle[client]];
                        
                        SecondsToTimeString(wr_pm, STR(sz_time), true);
                        Format(STR(plus_minus), "\nTotal (%s)", sz_time);
                    }
                    
                    SecondsToTimeString(time, STR(sz_time));
                    
#if defined(WINTOX_BHOP)

                    if (g_StartTimes[client][g_EndZone[track]] == 0.0)
                        PrintHintText(client, "\
%s\
Time: %s\n\
Style: %s\n\
Stage %d (%s)%s\n\
Jumps: %d",
                        IS_PAUSED(client) ? "[PAUSED]\n" : "", 
                        sz_time,
                        STYLE_NAMES_C[g_PlayerCurrentStyle[client]],
                        g_PlayerCurrentZone[client], g_ZoneNames[g_PlayerCurrentTrack[client]][g_PlayerCurrentZone[client]], plus_minus,
                        g_Jumps[client]);
                    else
                        PrintHintText(client, "\
Final time: %s\n\
Style: %s\n\
Final stage: %s\n\
Jumps: %d",
                        sz_time,
                        STYLE_NAMES_C[g_PlayerCurrentStyle[client]],
                        g_ZoneNames[track][g_PlayerCurrentZone[client]],
                        g_Jumps[client]);

#elseif defined(WINTOX_SURF)
#endif

                }
            }
            
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
    SetMenuTitle(menu, "Players spectating you");
    
    LOOP_OBSERVERS(client,observer)
    {
        decl String:name[64];
        decl String:sz_userid[16];
        
        IntToString(GetClientUserId(observer), STR(sz_userid));
        GetClientName(observer, STR(name));
        
        AddMenuItem(menu, sz_userid, name);
    }
    
    return menu;
}

public MenuHandler_SpecMenu(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
        DisplayMenuAtItem(menu, param1, GetMenuSelectionPosition(), MENU_TIME_FOREVER);
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
    if (client != entity && (0 < entity <= MaxClients) && g_HideOtherPlayers[client]) 
        return Plugin_Handled; 
     
    return Plugin_Continue; 
}

public Hook_WeaponDropPost(client, weapon) 
{ 
    if (g_RemoveDroppedWeapons)
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
