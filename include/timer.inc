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
#define INVALID_POINTS -1

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
    #define STYLE_NORMAL 0
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

#define GET_TIME(%1) decl hours, minutes, Float:seconds; \
    SecondsToTime((%1), hours, minutes, seconds)
// GET_TIME without the declarations
#define GET_TIME_D(%1) SecondsToTime((%1), hours, minutes, seconds)

// Timestamps for entering each zone (or for exiting zone 0)
new Float:g_StartTimes[MAXPLAYERS][MAX_ZONES];
// Number of jumps in the last attempt
new g_Jumps[MAXPLAYERS];
// The current playing style of the player
new g_PlayerCurrentStyle[MAXPLAYERS] = { MAX_STYLES-1, ... };

// Best times, in seconds, of each zone (zone 0 will always be 0.0)
new Float:g_ZoneRecords[MAXPLAYERS][MAX_TRACKS][MAX_ZONES][MAX_STYLES];
// Total points earned by each player
new g_PlayerPoints[MAXPLAYERS] = { INVALID_POINTS, ... };

// The zone completion times of the fastest track run
new Float:g_TrackWorldRecord[MAX_TRACKS][MAX_ZONES][MAX_STYLES];
// The fastest zone completion times for each zone and style
new Float:g_BestZoneRecords[MAX_TRACKS][MAX_ZONES][MAX_STYLES];

// Whether the player's timer has started
new bool:g_IsTiming[MAXPLAYERS] = { false, ... };
// When the player paused their timer (0.0 for not paused)
new Float:g_PauseStart[MAXPLAYERS] = { 0.0, ... };

// Whether to display the timer +/-
new bool:g_DisplayPlusMinus[MAXPLAYERS] = { false, ... };

// Holds the state of wintox_display_plus_minus_time
new Float:g_DisplayPlusMinusTime = 3.0;
// Stores the disable plus minus timer handles, so they can be closed if the 
// player reaches another zone before g_DisplayPlusMinusTime
new Handle:g_DisablePlusMinusTimer[MAXPLAYERS] = { INVALID_HANDLE, ... };

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
    
    new Handle:cvar_display_plus_minus_time = CreateConVar("wintox_display_plus_minus_time", "3.0", "The amount of time to display +/- times in the timer display", 0, true, 0.0);
    g_DisplayPlusMinusTime = GetConVarFloat(cvar_display_plus_minus_time);
    HookConVarChange(cvar_display_plus_minus_time, ConVar_DisplayPlusMinusTime);
    
    RegisterCommand("sm_timer", ConCommand_Timer, "sm_timer | Shows the timer settings menu.");
    RegisterCommand("sm_restart", ConCommand_Restart, "sm_restart | Teleports you to the start zone and resets your timer.");
    RegisterCommand("sm_stop", ConCommand_Stop, "sm_stop | Disables the timer until you pass through the start zone.");
    RegisterCommand("sm_respawn", ConCommand_Respawn, "sm_respawn | If you are dead, this respawns you.");
    RegisterCommand("sm_pause", ConCommand_Pause, "sm_pause | Pauses or unpauses you and the timer.");

#if defined(WINTOX_BHOP)
    RegisterCommand("sm_cp", ConCommand_Cp, "sm_cp <partial player name> | Displays the specified player's elapsed time of their run, type, and jumps.");
#endif
    
    AddToForward(g_fwdPlayerReachedZone, INVALID_HANDLE, fwd_Timer_OnPlayerReachedZone);
    AddToForward(g_fwdPlayerReachedEnd, INVALID_HANDLE, fwd_Timer_OnPlayerReachedEnd);
}

Timer_DeinitPlayer(client)
{
    for (new track=0; track<MAX_TRACKS; track++)
    {
        for (new i=0; i<MAX_ZONES; i++)
        {
            if (track == 0)
                g_StartTimes[client][i] = 0.0;
            
            for (new style=0; style<MAX_STYLES; style++)
                g_ZoneRecords[client][track][i][style] = INVALID_ZONE_RECORD;
        }
    }
    
    g_Jumps[client] = 0;
    g_PauseStart[client] = 0.0;
    
    g_TimerEnabled[client] = true;
    g_SideStageInfoEnabled[client] = true;
    g_PointsToChatEnabled[client] = true;
    
    g_PlayerCurrentStyle[client] = 0;
}

Timer_PlayerReachedZone(client, track, zone)
{
    ZoneTime(client, zone);
    g_PlayerCurrentPath[client]++;
    
    // Cancel the last disable +/- timer, so we don't end the +/- prematurely
    if (g_DisablePlusMinusTimer[client] != INVALID_HANDLE)
        CloseHandle(g_DisablePlusMinusTimer[client]);
    
    g_DisplayPlusMinus[client] = true;
    g_DisablePlusMinusTimer[client] = CreateTimer(g_DisplayPlusMinusTime, Timer_Callback_DisableDisplayPlusMinus, client);
    
    Call_StartForward(g_fwdPlayerReachedZone);
    Call_PushCell(client);
    Call_PushCell(track);
    Call_PushCell(zone);
    Call_PushFloat(g_StartTimes[client][zone] - g_StartTimes[client][zone-1]);
    Call_PushCell(g_PlayerCurrentStyle[client]);
    Call_Finish();
}

Timer_PlayerReachedEnd(client, track)
{
    g_IsTiming[client] = false;
    
    Call_StartForward(g_fwdPlayerReachedEnd);
    Call_PushCell(client);
    Call_PushCell(track);
    Call_PushFloat(g_StartTimes[client][g_EndZone[track]] - g_StartTimes[client][0]);
    Call_PushCell(g_PlayerCurrentStyle[client]);
    Call_Finish();
}

public ConVar_DisplayPlusMinusTime(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DisplayPlusMinusTime = GetConVarFloat(cvar);
}

public Action:Timer_Callback_DisableDisplayPlusMinus(Handle:timer, any:data)
{
    g_DisplayPlusMinus[data] = false;
    g_DisablePlusMinusTimer[data] = INVALID_HANDLE;
    
    return Plugin_Stop;
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

public fwd_Timer_OnPlayerReachedEnd(client, track, Float:elapsed, style)
{
    Client_SetScore(client, Client_GetScore(client) + 1);
}

win_SQL_UpdatePlayerPoints(client)
{
    decl String:query[256];
    
    // The player isn't in the database, so an update would be pointless
    if (g_PlayerIDs[client] == -1)
        return;
    
    Format(STR(query), SQL_QUERY[UpdatePlayerPoints], g_PlayerPoints[client], g_PlayerIDs[client]);
    SQLT(query, T_UpdatePlayerPoints, client);
}
SQLT_HANDLER(UpdatePlayerPoints, "Error updating player %L points: %s", data)

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
    NO_FAKECLIENTS()
    
    new Handle:menu = BuildTimerSettingsMenu(client);
    DisplaySubmenu(menu, client, MENU_TIME_FOREVER);
    
    return Plugin_Handled;
}

public Action:ConCommand_Restart(client, args)
{
    NO_ARGS()
    
    if (!IS_PLAYING_TEAM(GetClientTeam(client)))
    {
        Timer_Reply(client, "You are not on a playing team and have no run to restart.");
        return Plugin_Handled;
    }
    
    ResetTimer(client);
    ZoneTeleport(client, 0);
    
    // Clear pause, just in case
    UnpauseTimer(client);
    SetEntityMoveType(client, MOVETYPE_ISOMETRIC);
    
    Timer_Reply(client, "Reset timer and teleported to start zone.");
    
    return Plugin_Handled;
}

public Action:ConCommand_Stop(client, args)
{
    NO_ARGS()
    
    if (!IS_PLAYING_TEAM(GetClientTeam(client)))
    {
        Timer_Reply(client, "You are not on a playing team and have no timer to stop.");
        return Plugin_Handled;
    }
    
    ResetTimer(client);
    Timer_Reply(client, "Stopped timer. Pass through the {T}start zone{N} to restart it.");
    
    return Plugin_Handled;
}

public Action:ConCommand_Respawn(client, args)
{
    NO_ARGS()
    
    if (!IS_PLAYING_TEAM(GetClientTeam(client)))
    {
        Timer_Reply(client, "You are not on a playing team and can't be respawned. {OG}Nice try{N}.");
        return Plugin_Handled;
    }
    
    if (IsPlayerAlive(client))
    {
        Timer_Reply(client, "Living players cannot be respawned. Perhaps you were looking for {T}/restart{N}?");
        return Plugin_Handled;
    }
    
    RESPAWN_PLAYER(client);
    Timer_Reply(client, "You have been given new life. {T}IT'S ALIVE!{N}");
    
    return Plugin_Handled;
}

public Action:ConCommand_Pause(client, args)
{
    NO_ARGS()
    
    if (!g_IsTiming[client])
    {
        Timer_Reply(client, "The timer is not running. There's no need to pause.");
        return Plugin_Handled;
    }
    
    if (IS_PAUSED(client))
    {
        UnpauseTimer(client);
        SetEntityMoveType(client, MOVETYPE_ISOMETRIC);
        
        Timer_Reply(client, "Unpaused timer and your movement.");
    }
    else
    {
        PauseTimer(client);
        SetEntityMoveType(client, MOVETYPE_NONE);
        
        Timer_Reply(client, "Paused timer and your movement.");
    }
    
    return Plugin_Handled;
}

#if defined(WINTOX_BHOP)
public Action:ConCommand_Cp(client, args)
{
    CHECK_ARGS_LOWER(1)
    
    decl String:filter[128];
    GetCmdArgString(STR(filter));
    
    new target = FindTarget(client, filter, true, false);
    if (target == -1)
        return Plugin_Handled;
    
    Color_ChatSetSubject(target);
    
    if (g_IsTiming[target] || IS_FINISHED(target))
    {
        decl Float:relative_from;
        if (g_IsTiming[target])
            relative_from = GetEngineTime();
        else
            relative_from = g_StartTimes[target][g_EndZone[g_PlayerCurrentTrack[target]]];
        
        new Float:time = relative_from - g_StartTimes[target][0];
        new hours = RoundToFloor(time) / SECONDS_IN_HOUR;
        new minutes = (RoundToFloor(time) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
        new Float:seconds = time - (hours * SECONDS_IN_HOUR) - (minutes * SECONDS_IN_MINUTE);
        
        Timer_PrintToChat(client, g_IsTiming[target] ? "{OG}%s{N} | {T}%N{N} is {R}%02d:%02d:%07.4f{N} into the map with {R}%d{N} jump%s." : "{OG}%s{N} | {T}%N{N} completed the map in {R}%02d:%02d:%07.4f{N} with {R}%d{N} jump%s.", STYLE_NAMES_C[g_PlayerCurrentStyle[target]], target, hours, minutes, seconds, g_Jumps[target], g_Jumps[target] == 1 ? "" : "s");
    }
    else
        Timer_PrintToChat(client, "{T}%N{N} is not currently being timed.", target);
    
    return Plugin_Handled;
}
#endif

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

SecondsToTime(Float:time, &hours, &minutes, &Float:seconds)
{
    hours = RoundToFloor(time) / SECONDS_IN_HOUR;
    minutes = (RoundToFloor(time) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
    seconds = time - (hours * SECONDS_IN_HOUR) - (minutes * SECONDS_IN_MINUTE);
}

SecondsToTimeString(Float:time, String:buffer[], maxlength, bool:plusminus=false)
{
    decl hours, minutes, Float:seconds;
    SecondsToTime(time, hours, minutes, seconds);
    
    Format(buffer, maxlength, "%s%02d:%02d:%07.4f", plusminus ? ((time >= 0) ? "+" : "-") : "", hours, minutes, seconds);
}
