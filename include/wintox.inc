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

new g_TimerDisplayRefreshRate = 5;
new bool:g_wintoxExiting = false;
new bool:g_wintoxLateLoad = false;
new g_CurrentTick = 0;

new bool:g_HideOtherPlayers[MAXPLAYERS] = { false, ... };
new g_PlayerIDs[MAXPLAYERS] = { -1, ... };

#include <sourcemod>
#include <sdkhooks>

#include "./include/game"
#include "./include/commands"
#include "./include/sql"
#include "./include/utils"
#include "./include/events"
#include "./include/menus"
#include "./include/zones"

Wintox_Init()
{
    Menus_Init();
    Commands_Init();
    win_SQL_Init();
    Events_Init();
    Utils_Init();
    Zones_Init();
    Timer_Init();
    
    AutoExecConfig(true, "wintox");
    
    new Handle:timer_refresh = CreateConVar("wintox_timerdisplayrefreshrate", "5", "On which frame to refresh the timer display (the higher the number, the slower the refresh rate)", 0, true, 1.0);
    g_TimerDisplayRefreshRate = GetConVarBool(timer_refresh);
    HookConVarChange(timer_refresh, ConVar_TimerDisplayRefreshRate);
    
    RegisterCommand("sm_hide", ConCommand_Hide, "sm_hide | Toggle hiding all other players (useful for demos).");
    RegisterCommand("sm_show", ConCommand_Show, "sm_show | Show all other players (counteracts sm_hide).");

#if defined(WINTOX_BHOP)
    RegisterCommand("sm_scout", ConCommand_Scout, "sm_scout | Gives a scout.");
    RegisterCommand("sm_usp", ConCommand_USP, "sm_usp | Gives a scout.");
#endif
}

Wintox_Exit()
{
    g_wintoxExiting = true;
}

public ConVar_TimerDisplayRefreshRate(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_TimerDisplayRefreshRate = MAX(GetConVarInt(cvar), 1);
}

public OnConfigsExecuted()
{
    // Initialize database connections and the such
    win_SQL_Connect();
    win_SQL_CreateTables();
    
    // Cache map info from database
    GetOrInsertMap(g_CurMapName);
    
    // If we're late loaded, we need to grab all player's info
    if (g_wintoxLateLoad)
    {
        for (new client=1; client<=MaxClients; client++)
        {
            if (IsClientConnected(client) && IsClientAuthorized(client))
            {
                decl String:steamid[64];
                
                GetClientAuthString(client, STR(steamid));
                InsertOrUpdatePlayerInfo(client, steamid);
            }
        }
    }
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
        
        // Check if the player is in their current or next zone only
        if (!IS_PAUSED(client))
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
                    {
                        ZoneTime(client, i);
                        g_PlayerCurrentPath[client]++;
                        
                        Call_StartForward(g_fwdPlayerReachedZone);
                        Call_PushCell(client);
                        Call_PushCell(track);
                        Call_PushCell(i);
                        Call_PushFloat(g_StartTimes[client][i] - g_StartTimes[client][i-1]);
                        Call_PushCell(g_PlayerCurrentStyle[client]);
                        Call_Finish();
                    }
                    
                    // Yeah, you go, player! YOU ROCK!
                    if (IS_END_ZONE(i) && g_IsTiming[client])
                    {
                        g_IsTiming[client] = false;
                        
                        Call_StartForward(g_fwdPlayerReachedEnd);
                        Call_PushCell(client);
                        Call_PushCell(track);
                        Call_PushFloat(g_StartTimes[client][g_EndZone[track]] - g_StartTimes[client][0]);
                        Call_PushCell(0); // TODO: style
                        Call_Finish();
                    }
                    
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
        if (g_CurrentTick % g_TimerDisplayRefreshRate == 0)
        {
            if (g_TimerEnabled[client])
            {
                if (g_PlayerCurrentZone[client] == INVALID_ZONE)
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
                    new hours = RoundToFloor(time) / SECONDS_IN_HOUR;
                    new minutes = (RoundToFloor(time) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
                    new Float:seconds = time - (hours * SECONDS_IN_HOUR) - (minutes * SECONDS_IN_MINUTE);
                    
#if defined(WINTOX_BHOP)

                    if (g_StartTimes[client][g_EndZone[track]] == 0.0)
                        PrintHintText(client, "%sTime: %02d:%02d:%07.4f\nStyle: %s\nLast stage %d (%s) of %d\nJumps: %d", IS_PAUSED(client) ? "[PAUSED]\n" : "", hours, minutes, seconds, STYLE_NAMES_C[g_PlayerCurrentStyle[client]], g_PlayerCurrentZone[client], g_ZoneNames[g_PlayerCurrentTrack[client]][g_PlayerCurrentZone[client]], g_ZonesCount[track]-1, g_Jumps[client]);
                    else
                        PrintHintText(client, "Final time: %02d:%02d:%07.4f\nStyle: %s\nJumps: %d", hours, minutes, seconds, STYLE_NAMES_C[g_PlayerCurrentStyle[client]], g_Jumps[client]);

#elseif defined(WINTOX_SURF)
#endif

                }
            }
            
            // Actively hide the hint text
            else
                HideHintText(client);
        }
    }
}


#define SHOW_OTHER_PLAYERS_MESSAGE "Showing all other players."
public Action:ConCommand_Hide(client, args)
{
    NO_ARGS()
    
    g_HideOtherPlayers[client] = !g_HideOtherPlayers[client];
    
    if (g_HideOtherPlayers[client])
        Reply(client, "Hiding all other players.");
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

#if defined(WINTOX_BHOP)
public Action:ConCommand_Scout(client, args)
{
    NO_ARGS()
    
    Client_GiveWeaponAndAmmo(client, "weapon_scout");
    
    return Plugin_Handled;
}

public Action:ConCommand_USP(client, args)
{
    NO_ARGS()
    
    Client_GiveWeaponAndAmmo(client, "weapon_usp");
    
    return Plugin_Handled;
}
#endif
