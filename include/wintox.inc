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
        
        current_time = GetEngineTime();
        time_elapsed = current_time - g_StartTimes[client][0];
        
        // Display the timer hint text every other frame
        if (g_CurrentTick % 2 == 0)
        {
            if (g_TimerEnabled[client] && g_StartTimes[client][0] != 0.0)
            {
                new hours = RoundToFloor(time_elapsed) / SECONDS_IN_HOUR;
                new minutes = (RoundToFloor(time_elapsed) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
                new Float:seconds = time_elapsed - (hours * SECONDS_IN_HOUR) - (minutes * SECONDS_IN_MINUTE);
                
                PrintHintText(client, "Time: %02d:%02d:%07.4f\nJumps: %d", hours, minutes, seconds, g_Jumps[client]);
            }
        }
    }
}
