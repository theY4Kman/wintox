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

new bool:g_wintoxExiting = false;
new g_CurrentTick = 0;

#include "./include/game"
#include "./include/commands"
#include "./include/sql"
#include "./include/events"
#include "./include/menus"
#include "./include/zones"

Wintox_Init()
{
    Menus_Init();
    Commands_Init();
    win_SQL_Init();
    Events_Init();
    Zones_Init();
    Timer_Init();
    
    AutoExecConfig(true, "wintox");
}

Wintox_Exit()
{
    g_wintoxExiting = true;
}

public OnConfigsExecuted()
{
    // Initialize database connections and the such
    win_SQL_Connect();
    win_SQL_CreateTables();
    
    // Cache map info from database
    GetOrInsertMap(g_CurMapName);
}

public OnGameFrame()
{
    g_CurrentTick++;
    
    for (new client = 1; client <= MaxClients; client++)
    {
        decl Float:current_time;
        decl Float:time_elapsed;
        
        if (!IsClientConnected(client) || !IsClientInGame(client) || IsFakeClient(client))
            continue;
        
        decl Float:origin[3];
        new bool:is_in_zone = false;
        
        GetClientAbsOrigin(client, origin);
        
        // Check if the player is in their current or next zone only
        new start_zone = MAX(g_PlayerCurrentZone[client], 0);
        new end_zone = start_zone + _:(g_PlayerCurrentZone[client] != -1);
        for (new i=start_zone; i<=end_zone; i++)
        {
            new sides = ZONE_POINTS[i];
            new j = sides-1 + ZONE_FIRST_POINT;
            new bool:odd_nodes = false;
            
            for (new h=ZONE_FIRST_POINT; h<(ZONE_META_POINTS + sides); h++)
            {
                if ((ZONE_POINT[i][h][1] < origin[1] && ZONE_POINT[i][j][1] >= origin[1]) ||
                    (ZONE_POINT[i][j][1] < origin[1] && ZONE_POINT[i][h][1] >= origin[1]))
                    if (ZONE_POINT[i][h][0] + (origin[1] - ZONE_POINT[i][h][1]) / (ZONE_POINT[i][j][1] -
                        ZONE_POINT[i][h][1]) * (ZONE_POINT[i][j][0] - ZONE_POINT[i][h][0]) < origin[0])
                        odd_nodes = !odd_nodes;
                
                j = h;
            }
            
            if (odd_nodes)
            {
                is_in_zone = true;
                g_PlayerCurrentZone[client] = i;
                break;
            }
            else if (g_PlayerCurrentZone[client] == 0)
            {
                StartTimer(client);
                g_IsPlaying[client] = true;
            }
        }
        
        current_time = GetEngineTime();
        time_elapsed = current_time - g_StartTimes[client][0];
        
        // Display the timer hint text every other frame
        if (g_CurrentTick % 2 == 0)
        {
            if (g_TimerEnabled[client])
            {
                if (g_PlayerCurrentZone[client] == INVALID_ZONE)
                    PrintHintText(client, "[Enter the start zone]");
                
                else if (is_in_zone && g_PlayerCurrentZone[client] == 0)
                    PrintHintText(client, "[Leave the start zone]");
                
                else if (g_StartTimes[client][0] != 0.0)
                {
                    new hours = RoundToFloor(time_elapsed) / SECONDS_IN_HOUR;
                    new minutes = (RoundToFloor(time_elapsed) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
                    new Float:seconds = time_elapsed - (hours * SECONDS_IN_HOUR) - (minutes * SECONDS_IN_MINUTE);
                    
                    PrintHintText(client, "Time: %02d:%02d:%07.4f\nStage %d\nJumps: %d", hours, minutes, seconds, g_PlayerCurrentZone[client], g_Jumps[client]);
                }
            }
        }
    }
}
