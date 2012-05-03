/**
 * Timer fun!
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_timer_included
 #endinput
#endif
#define _wintox_timer_included

#include "./include/zones"
#include "./include/menus"
#include "./include/sql"

#define INVALID_ZONE_RECORD -1.0

#if defined(WINTOX_BHOP)
    #define STYLE_NORMAL 0
    #define STYLE_SIDEWAYS 1
    #define STYLE_WONLY 2
    #define MAX_STYLES 3
    
    new String:STYLE_NAMES[MAX_STYLES][] = {
        "normal",
        "sideways",
        "w-only"
    };
    new String:STYLE_NAMES_C[MAX_STYLES][] = {
        "Normal",
        "Sideways",
        "W-only"
    };
#else
    #define MAX_STYLES 1
    
    new String:STYLE_NAMES[MAX_STYLES][] = {
        ""
    };
    new String:STYLE_NAMES_C[MAX_STYLES][] = {
        ""
    };
#endif

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR (SECONDS_IN_MINUTE*60)
#define SECONDS_TO_TIME(%1) RoundToFloor(%1) / SECONDS_IN_HOUR, (RoundToFloor(%1) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE, ((%1) - (RoundToFloor(%1) / SECONDS_IN_MINUTE * SECONDS_IN_MINUTE))

// Timestamps for entering each zone (or for exiting zone 0)
new Float:g_StartTimes[MAXPLAYERS][MAX_ZONES];
// Number of jumps in the last attempt
new g_Jumps[MAXPLAYERS];
// The current playing style of the player
new g_PlayerCurrentStyle[MAXPLAYERS] = { MAX_STYLES-1, ... };

// Best times, in seconds, of each zone (zone 0 will always be 0.0)
new Float:g_ZoneRecords[MAXPLAYERS][MAX_TRACKS][MAX_ZONES][MAX_STYLES];

// Whether the player's timer has started
new bool:g_IsTiming[MAXPLAYERS] = { false, ... };
// When the player paused their timer (0.0 for not paused)
new Float:g_PauseStart[MAXPLAYERS] = { 0.0, ... };

new bool:g_TimerEnabled[MAXPLAYERS] = { true, ... };
new bool:g_SideStageInfoEnabled[MAXPLAYERS] = { true, ... };
new bool:g_PointsToChatEnabled[MAXPLAYERS] = { true, ... };

#define StartTimer(%1) g_StartTimes[(%1)][0] = GetEngineTime()
#define ZoneTime(%1,%2) g_StartTimes[(%1)][(%2)] = GetEngineTime()
#define TIMER_STARTED(%1) (g_StartTimes[(%1)][0] != 0.0)
#define ZONE_TIME[%1][%2] g_StartTimes[(%1)][(%2)]

#define IS_PAUSED(%1) (g_PauseStart[(%1)] != 0.0)
#define IS_FINISHED(%1) (g_StartTimes[(%1)][g_EndZone[g_PlayerCurrentTrack[(%1)]]] != 0.0)

Timer_Init()
{
    // PAWN YOU BASTARD
    for (new client=0; client<MAXPLAYERS; client++)
        for (new track=0; track<MAX_TRACKS; track++)
            for (new zone=1; zone<MAX_ZONES; zone++)
                for (new style=0; style<MAX_STYLES; style++)
                    g_ZoneRecords[client][track][zone][style] = INVALID_ZONE_RECORD;
    
    RegisterCommand("sm_timer", ConCommand_Timer, "sm_timer | Shows the timer settings menu.");
    RegisterCommand("sm_restart", ConCommand_Restart, "sm_restart | Teleports you to the start zone and resets your timer.");
    RegisterCommand("sm_stop", ConCommand_Stop, "sm_stop | Disables the timer until you pass through the start zone.");
    RegisterCommand("sm_respawn", ConCommand_Respawn, "sm_respawn | If you are dead, this respawns you.");
    RegisterCommand("sm_pause", ConCommand_Pause, "sm_pause | Pauses or unpauses you and the timer.");
    
    new Function:on_player_reached_zone = GetFunctionByName(INVALID_HANDLE, "fwd_Timer_OnPlayerReachedZone");
    AddToForward(g_fwdPlayerReachedZone, INVALID_HANDLE, on_player_reached_zone);
}

public fwd_Timer_OnPlayerReachedZone(client, track, zone_id, Float:elapsed, style)
{
    if (g_ZoneRecords[client][track][zone_id][style] == INVALID_ZONE_RECORD ||
        elapsed < g_ZoneRecords[client][track][zone_id][style])
    {
        g_ZoneRecords[client][track][zone_id][style] = elapsed;
        win_SQL_UpsertZoneRecord(client, track, zone_id, elapsed, STYLE_NAMES[style]);
    }
}

win_SQL_UpsertZoneRecord(client, track, zone_id, Float:time, const String:type[]="")
{
    decl String:query[512];
    decl String:esc_type[32];
    
    // The player has no database ID; we can't record their progress!
    if (g_PlayerIDs[client] == -1)
        return;
    
    SQL_EscapeString(g_Database, type, STR(esc_type));
    
    Format(STR(query), SQL_QUERY[UpsertZoneRecord], g_CurMapID, g_PlayerIDs[client], g_ZoneIDs[track][zone_id], esc_type, time, GetTime());
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, track);
    WritePackCell(pack, zone_id);
    ResetPack(pack);
    
    SQLT(query, T_UpsertZoneRecord, pack);
}

public T_UpsertZoneRecord(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl client;
    decl track;
    decl zone_id;
    
    new Handle:pack = Handle:data;
    
    if (hndl == INVALID_HANDLE)
    {
        client = ReadPackCell(pack);
        track = ReadPackCell(pack);
        zone_id = ReadPackCell(pack);
        
        LogError("Error upserting zone record (client %d %L, track %d, zone %d): %s", client, client, track, zone_id, error);
    }
    
    CloseHandle(pack);
}

// This function requires the client's DB ID, so this must be called after
// that has been retrieved (after InsertOrUpdatePlayerInfo and its threaded
// queries are complete)
CacheZoneRecords(client)
{
    decl String:query[256];
    
    if (g_PlayerIDs[client] == -1)
    {
        LogError("Attempted to retrieve the zone records of player %L with bad DB ID %d.", client, g_PlayerIDs[client]);
        return;
    }
    
    Format(STR(query), SQL_QUERY[GetZoneRecords], g_CurMapID, g_PlayerIDs[client]);
    SQLT(query, T_CacheZoneRecords, client);
}

public T_CacheZoneRecords(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new client = data;
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving zone records for client %d (%L): %s", client, client, error);
        return;
    }
    
    while (SQL_FetchRow(hndl))
    {
        decl zone_id;
        decl track;
        decl String:stylename[16];
        decl Float:time;
        
        zone_id = SQL_FetchInt(hndl, 1);
        track = SQL_FetchInt(hndl, 2);
        SQL_FetchString(hndl, 3, STR(stylename));
        time = SQL_FetchFloat(hndl, 4);
        
        g_ZoneRecords[client][track][zone_id][StyleNameToIndex(stylename)] = time;
    }
}

#define TSET_MENU_MIDDLE "middle"
#define TSET_MENU_SIDE "side"
#define TSET_MENU_POINTS "points"

Handle:BuildTimerSettingsMenu(client)
{
    decl String:buffer[64];
    new Handle:menu = CreateMenu(MenuHandler_TimerSettings);
    
    SetMenuTitle(menu, "Timer settings");
    SetMenuExitButton(menu, true);
    
    Format(STR(buffer), "%s middle timer", g_TimerEnabled[client] ? "Disable" : "Enable");
    AddMenuItem(menu, TSET_MENU_MIDDLE, buffer);
    
    Format(STR(buffer), "%s side stage info", g_SideStageInfoEnabled[client] ? "Disable" : "Enable");
    AddMenuItem(menu, TSET_MENU_SIDE, buffer);
    
    Format(STR(buffer), "%s points and +/- printed to chat", g_PointsToChatEnabled[client] ? "Disable" : "Enable");
    AddMenuItem(menu, TSET_MENU_POINTS, buffer);
    
    return menu;
}

public MenuHandler_TimerSettings(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
        if (StrEqual(info, TSET_MENU_MIDDLE))
        {
            g_TimerEnabled[param1] = !g_TimerEnabled[param1];
            if (!g_TimerEnabled[param1])
                HideHintText(param1);
        }
        else if (StrEqual(info, TSET_MENU_SIDE))
            g_SideStageInfoEnabled[param1] = !g_SideStageInfoEnabled[param1];
        else if (StrEqual(info, TSET_MENU_POINTS))
            g_PointsToChatEnabled[param1] = !g_PointsToChatEnabled[param1];
        
        // Cancel this menu without redisplaying previous submenu
        PrevSubmenu(param1, false);
        // Rebuild the menu based on the client's new settings and display it
        DisplaySubmenu(BuildTimerSettingsMenu(param1), param1, MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public Action:ConCommand_Timer(client, args)
{
    NO_ARGS()
    
    new Handle:menu = BuildTimerSettingsMenu(client);
    DisplaySubmenu(menu, client, MENU_TIME_FOREVER);
    
    return Plugin_Handled;
}

public Action:ConCommand_Restart(client, args)
{
    NO_ARGS()
    
    if (!IS_PLAYING_TEAM(GetClientTeam(client)))
    {
        Reply(client, "You are not on a playing team and have no run to restart.");
        return Plugin_Handled;
    }
    
    ResetTimer(client);
    ZoneTeleport(client, 0);
    
    // Clear pause, just in case
    UnpauseTimer(client);
    SetEntityMoveType(client, MOVETYPE_ISOMETRIC);
    
    Reply(client, "Reset timer and teleported to start zone.");
    
    return Plugin_Handled;
}

public Action:ConCommand_Stop(client, args)
{
    NO_ARGS()
    
    if (!IS_PLAYING_TEAM(GetClientTeam(client)))
    {
        Reply(client, "You are not on a playing team and have no timer to stop.");
        return Plugin_Handled;
    }
    
    ResetTimer(client);
    Reply(client, "Stopped timer. Pass through the start zone to restart it.");
    
    return Plugin_Handled;
}

public Action:ConCommand_Respawn(client, args)
{
    NO_ARGS()
    
    if (!IS_PLAYING_TEAM(GetClientTeam(client)))
    {
        Reply(client, "You are not on a playing team and can't be respawned. Nice try.");
        return Plugin_Handled;
    }
    
    if (IsPlayerAlive(client))
    {
        Reply(client, "Living players cannot be respawned. Perhaps you were looking for /restart?");
        return Plugin_Handled;
    }
    
    RESPAWN_PLAYER(client);
    Reply(client, "You have been given new life. IT'S ALIVE!");
    
    return Plugin_Handled;
}

public Action:ConCommand_Pause(client, args)
{
    NO_ARGS()
    
    if (!g_IsTiming[client])
    {
        Reply(client, "The timer is not running. There's no need to pause.");
        return Plugin_Handled;
    }
    
    if (IS_PAUSED(client))
    {
        UnpauseTimer(client);
        SetEntityMoveType(client, MOVETYPE_ISOMETRIC);
        
        Reply(client, "Unpaused timer and your movement.");
    }
    else
    {
        PauseTimer(client);
        SetEntityMoveType(client, MOVETYPE_NONE);
        
        Reply(client, "Paused timer and your movement.");
    }
    
    return Plugin_Handled;
}

// Resets all the zone times and jumps and sets IsTiming false
ResetTimer(client)
{
    for (new i=0; i<MAX_ZONES; i++)
        g_StartTimes[client][i] = 0.0;
    
    g_PlayerCurrentTrack[client] = MAIN_TRACK;
    g_PlayerCurrentZone[client] = INVALID_ZONE;
    g_PlayerCurrentPath[client] = INVALID_PATH;
    g_PlayerCurrentStyle[client] = MAX_STYLES-1;
    g_IsTiming[client] = false;
    g_Jumps[client] = false;
}

// Pauses the timer, recording the pause time for use in correcting zone times on unpause
PauseTimer(client)
{
    g_PauseStart[client] = GetEngineTime();
}

// Unpauses the timer, correcting zone times for pause time elapsed
UnpauseTimer(client)
{
    if (g_IsTiming[client])
    {
        new Float:pause_time_elapsed = GetEngineTime() - g_PauseStart[client];
        for (new i=0; i<=g_PlayerCurrentZone[client]; i++)
            ZONE_TIME[client][i] += pause_time_elapsed;
    }
    
    g_PauseStart[client] = 0.0;
}

// Checks whether the player is allowed to begin timing
PlayerCanTime(client)
{
#if defined(WINTOX_BHOP)
    if (g_PlayerLowGrav[client])
        return false;
#endif
    
    return true;
}

StyleNameToIndex(const String:stylename[])
{
    for (new style=0; style<MAX_STYLES; style++)
        if (StrEqual(stylename, STYLE_NAMES[style]))
            return style;
    
    return 0;
}
