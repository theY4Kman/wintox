/**
 * Timer fun!
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_timer_included
 #endinput
#endif
#define _wintox_timer_included

#include <clientprefs>
#include "./wintox/zones"
#include "./wintox/menus"
#include "./wintox/sql"
#include "./wintox/race"

#define INVALID_ZONE_RECORD -1.0
#define INVALID_TRACK_RECORD -1.0
#define INVALID_POINTS -1

#if defined(WINTOX_BHOP)
    #define STYLE_NORMAL 0
    #define STYLE_SIDEWAYS 1
    #define STYLE_WONLY 2
    #define MAX_STYLES 3
    
    #define DEFAULT_STYLE STYLE_WONLY
    
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
    #define STYLE_SIDEWAYS 1
    #define MAX_STYLES 2
    
    #define DEFAULT_STYLE STYLE_SIDEWAYS
    
    new String:STYLE_NAMES[MAX_STYLES][] = {
        "normal",
        "sideways"
    };
    new String:STYLE_NAMES_C[MAX_STYLES][] = {
        "Normal",
        "Sideways"
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
new g_PlayerCurrentStyle[MAXPLAYERS] = { DEFAULT_STYLE, ... };

// Style upon reaching each zone during the player's last run
new g_LastRunZoneStyle[MAXPLAYERS][MAX_ZONES];
// Elapsed times for each zone during the player's last run
new Float:g_LastRunZoneElapsed[MAXPLAYERS][MAX_ZONES];

// Best times, in seconds, of each zone (zone 0 will always be 0.0)
new Float:g_ZoneRecords[MAXPLAYERS][MAX_TRACKS][MAX_ZONES][MAX_STYLES];
// Best times, in seconds, of each track
new Float:g_TrackRecords[MAXPLAYERS][MAX_TRACKS][MAX_STYLES];
// Total points earned by each player
new g_PlayerPoints[MAXPLAYERS] = { INVALID_POINTS, ... };

// The zone completion times of the fastest track run
// Zone 0 will be the TOTAL ELAPSED TIME OF THE RUN
new Float:g_TrackWorldRecord[MAX_TRACKS][MAX_ZONES][MAX_STYLES];
// The fastest zone completion times for each zone and style
new Float:g_BestZoneRecords[MAX_TRACKS][MAX_ZONES][MAX_STYLES];

// Stores state of wintox_allow_velocity_display, whether to allow players
// to show/hide their speed in the middle/hint-text timer
new bool:g_AllowVelocityDisplay = true;

// Whether the player's timer has started
new bool:g_IsTiming[MAXPLAYERS] = { false, ... };
// When the player paused their timer (0.0 for not paused)
new Float:g_PauseStart[MAXPLAYERS] = { 0.0, ... };

// Whether the player is practicing (i.e. not recording main map time)
new bool:g_IsPracticing[MAXPLAYERS] = { false, ... };
// The global handle to the menu listing stages for !practice
new Handle:g_PracticeMenu = INVALID_HANDLE;
// The zone the player is currently practicing on
new g_PracticeZone[MAXPLAYERS] = { INVALID_ZONE, ... };

// Whether to display the timer +/-
new bool:g_DisplayPlusMinus[MAXPLAYERS] = { false, ... };
// The style to display for +/-
new g_PlusMinusStyle[MAXPLAYERS] = { STYLE_NORMAL, ... };

// Holds the state of wintox_display_plus_minus_time
new Float:g_DisplayPlusMinusTime = 3.0;
// Stores the disable plus minus timer handles, so they can be closed if the 
// player reaches another zone before g_DisplayPlusMinusTime
new Handle:g_DisablePlusMinusTimer[MAXPLAYERS] = { INVALID_HANDLE, ... };

// Stores the start index for the !top menus
new g_TimerTopStart[MAXPLAYERS];
// Stores the passed mapname for the !top menus
new String:g_TimerTopMapName[MAXPLAYERS][64];
// Stores the passed style for the !top menus
new g_TimerTopStyle[MAXPLAYERS];

new bool:g_TimerEnabled[MAXPLAYERS] = { true, ... };
new bool:g_SideStageInfoEnabled[MAXPLAYERS] = { true, ... };
new bool:g_PointsToChatEnabled[MAXPLAYERS] = { true, ... };
new bool:g_TimerSpeedEnabled[MAXPLAYERS] = { true, ... };

#define COOKIE_TIMER_ENABLED "WTX_TimerEnabled"
#define COOKIE_SIDE_STAGE_INFO_ENABLED "WTX_SideStageInfoEnabled"
#define COOKIE_POINTS_TO_CHAT_ENABLED "WTX_PointsToChatEnabled"
#define COOKIE_TIMER_SPEED_ENABLED "WTX_TimerSpeedEnabled"


#define TIMER_STARTED(%1) (g_StartTimes[(%1)][0] != 0.0)
#define ZONE_TIME[%1][%2] g_StartTimes[(%1)][(%2)]

#define IS_PAUSED(%1) (g_PauseStart[(%1)] != 0.0)
#define IS_FINISHED(%1) (g_StartTimes[(%1)][g_EndZone[g_PlayerCurrentTrack[(%1)]]] != 0.0)

Timer_Init()
{
    // PAWN YOU BASTARD
    for (new client=0; client<MAXPLAYERS; client++)
        for (new track=0; track<MAX_TRACKS; track++)
            for (new style=0; style<MAX_STYLES; style++)
            {
                for (new zone=0; zone<MAX_ZONES; zone++)
                {
                    g_ZoneRecords[client][track][zone][style] = INVALID_ZONE_RECORD;
                    
                    if (track == 0)
                    {
                        g_LastRunZoneStyle[client][zone] = STYLE_NORMAL;
                        g_LastRunZoneElapsed[client][zone] = 0.0;
                    }
                    
                    // Doesn't warrant a separate loop
                    if (client == 0)
                    {
                        g_BestZoneRecords[track][zone][style] = INVALID_ZONE_RECORD;
                        g_TrackWorldRecord[track][zone][style] = INVALID_TRACK_RECORD;
                    }
                }
                
                g_TrackRecords[client][track][style] = INVALID_TRACK_RECORD;
            }
    
    RegisterCommand("sm_timer", ConCommand_Timer, "sm_timer | Shows the timer settings menu.");
    RegisterCommand("sm_restart", ConCommand_Restart, "sm_restart | Teleports you to the start zone and resets your timer.");
    RegisterCommand("sm_stop", ConCommand_Stop, "sm_stop | Disables the timer until you pass through the start zone.");
    RegisterCommand("sm_respawn", ConCommand_Respawn, "sm_respawn | If you are dead, this respawns you.");
    RegisterCommand("sm_pause", ConCommand_Pause, "sm_pause | Pauses or unpauses you and the timer.");
    RegisterCommand("sm_top", ConCommand_Top, "sm_top [<partial map name>] | Shows the top normal style times for the specified map (or the current map if none specified).");
    RegisterCommand("sm_wr", ConCommand_Wr, "sm_wr [<partial map name>] | Shows the current normal world record holder for the specified map (or the current map if none specified).");
    RegisterCommand("sm_rank", ConCommand_Rank, "sm_rank [<partial player name>] | Shows the rank of the specified player (or you if no name specified) based on points earned on any map.");
    RegisterCommand("sm_crank", ConCommand_CRank, "sm_crank [<partial player name>] | Shows the rank of the specified player (or you if no name specified) based on points earned on any map, restricted to players of the same country.");
    RegisterCommand("sm_pc", ConCommand_Pc, "sm_pc [<partial player name>] | Shows the percent completion of all maps and bonuses for the specified player (or you if no player specified).");

#if defined(WINTOX_BHOP) || defined(WINTOX_SURF)
    RegisterCommand("sm_topsw", ConCommand_TopSW, "sm_topsw [<partial map name>] | Shows the top sideways times for specified map (or the current map if none specified).");
#endif

#if defined(WINTOX_BHOP)
    RegisterCommand("sm_cp", ConCommand_Cp, "sm_cp <partial player name> | Displays the specified player's elapsed time of their run, type, and jumps.");
    RegisterCommand("sm_topw", ConCommand_TopW, "sm_topw [<partial map name>] | Shows the top W-only times for specified map (or the current map if none specified).");
    RegisterCommand("sm_wrw", ConCommand_WrW, "sm_wrw [<partial map name>] | Shows the current W-only world record holder for the specified map (or the current map if none specified).");
    RegisterCommand("sm_wrsw", ConCommand_WrSW, "sm_wrsw [<partial map name>] | Shows the current sideways world record holder for the specified map (or the current map if none specified).");
#endif

#if defined(WINTOX_SURF)
    RegisterCommand("sm_wrb", ConCommand_WrB, "sm_wrb <bonus #> [<partial map name>] | Shows the current normal bonus record holder for the specified bonus on the specified map (or the current map if none specified).");
    RegisterCommand("sm_wrbsw", ConCommand_WrBSW, "sm_wrbsw <bonus #> [<partial map name>] | Shows the current sideways bonus record holder for the specified bonus on the specified map (or the current map if none specified).");
    RegisterCommand("sm_wrs", ConCommand_WrS, "sm_wrs <stage #> [<partial map name>] | Shows the current normal stage record holder for the specified stage on the specified map (or the current map if none specified).");
    RegisterCommand("sm_wrssw", ConCommand_WrSSW, "sm_wrssw <stage #> [<partial map name>] | Shows the current sideways stage record holder for the specified stage on the specified map (or the current map if none specified).");
    RegisterCommand("sm_practice", ConCommand_Practice, "sm_practice | Brings up a menu listing all the stages, allowing you to teleport and begin practicing (map completion times not recorded).");
#endif

#if defined(WINTOX_DEBUG)
    RegisterCommand("sm_set_track_wr", ConCommand_SetTrackWR, "sm_set_track_wr <track #> <style index> [zone #] <time in seconds> | Sets the value of g_TrackWorldRecord, for debugging purposes. If zone is omitted, it defaults to the end zone of the track, setting the total elapsed time of the record, instead of the zone time.", 0, ADMFLAG_ROOT, "wtxdebug");
    RegisterCommand("sm_list_styles", ConCommand_ListStyles, "sm_list_styles | Prints the names and indexes of the styles for the current gametype, to chat and console.", 0, ADMFLAG_ROOT, "wtxdebug");
#endif
    
    AddToForward(g_fwdPlayerReachedZone, INVALID_HANDLE, fwd_Timer_OnPlayerReachedZone);
    AddToForward(g_fwdPlayerReachedEnd, INVALID_HANDLE, fwd_Timer_OnPlayerReachedEnd);
    AddToForward(g_fwdMapLoaded, INVALID_HANDLE, fwd_Timer_OnMapLoaded);
    
    RegClientCookie(COOKIE_TIMER_ENABLED, "Whether to display the middle (hint text) timer.", CookieAccess_Public);
    RegClientCookie(COOKIE_SIDE_STAGE_INFO_ENABLED, "Whether to display side stage info.", CookieAccess_Public);
    RegClientCookie(COOKIE_POINTS_TO_CHAT_ENABLED, "Whether to print points and +/- timers to chat.", CookieAccess_Public);
    RegClientCookie(COOKIE_TIMER_SPEED_ENABLED, "Whether to display current speed in the middle (hint text) timer.", CookieAccess_Public);
}

Timer_RegisterConVars()
{
    new Handle:cvar_display_plus_minus_time = CreateConVar("wintox_display_plus_minus_time", "3.0", "The amount of time to display +/- times in the timer display", 0, true, 0.0);
    g_DisplayPlusMinusTime = GetConVarFloat(cvar_display_plus_minus_time);
    HookConVarChange(cvar_display_plus_minus_time, ConVar_DisplayPlusMinusTime);
    
    new Handle:cvar_allow_velocity_display = CreateConVar("wintox_allow_velocity_display", "1", "Whether to allow players the option to show/hide speed in their middle/hint-text timer.", 0, true, 0.0, true, 1.0);
    g_AllowVelocityDisplay = GetConVarBool(cvar_allow_velocity_display);
    HookConVarChange(cvar_allow_velocity_display, ConVar_AllowVelocityDisplay);
}

Timer_DeinitPlayer(client)
{
    for (new track=0; track<MAX_TRACKS; track++)
    {
        for (new zone=0; zone<MAX_ZONES; zone++)
        {
            if (track == 0)
                g_StartTimes[client][zone] = 0.0;
            
            for (new style=0; style<MAX_STYLES; style++)
            {
                if (zone == 0)
                    g_TrackRecords[client][track][style] = INVALID_TRACK_RECORD;
                g_ZoneRecords[client][track][zone][style] = INVALID_ZONE_RECORD;
            }
        }
    }
    
    g_Jumps[client] = 0;
    g_PauseStart[client] = 0.0;
    g_IsTiming[client] = false;
    g_IsPracticing[client] = false;
    
    g_TimerEnabled[client] = true;
    g_SideStageInfoEnabled[client] = true;
    g_PointsToChatEnabled[client] = true;
    g_TimerSpeedEnabled[client] = true;
    
    g_PlayerCurrentPath[client] = INVALID_PATH;
    g_PlayerCurrentStyle[client] = STYLE_NORMAL;
    g_PlayerEnteredStart[client] = false;
    
    g_DisplayPlusMinus[client] = false;
    g_PlusMinusStyle[client] = STYLE_NORMAL;
    
    g_TimerTopStart[client] = 0;
}

public fwd_Timer_OnMapLoaded(map_id, const String:mapname[], const String:human_name[], tier, bool:is_staged)
{
    CacheBestZoneRecords();
    CacheTrackTopRecords();
    CacheTrackTopRecordsZoneRecords();
}

public ConVar_DisplayPlusMinusTime(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DisplayPlusMinusTime = GetConVarFloat(cvar);
}

public ConVar_AllowVelocityDisplay(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_AllowVelocityDisplay = GetConVarBool(cvar);
}

GetJumps(client)
{
    return g_Jumps[client];
}

ZoneTime(client, zone)
{
    g_StartTimes[client][zone] = GetEngineTime();
}

StopTiming(client)
{
    g_IsTiming[client] = false;
}

DisplayTimer(client, is_in_zone)
{
    new track = GetPlayerCurrentTrack(client);
    new zone = GetPlayerCurrentZone(client);
    new style = g_PlayerCurrentStyle[client];
    
    decl String:stagename[72];
    stagename[0] = '\0';
    
    if (zone >= 0)
    {
        if (!IS_STR_EMPTY(g_ZoneNames[track][zone]))
            strcopy(STR(stagename), g_ZoneNames[track][zone]);
        else if (zone > 0)
            Format(STR(stagename), "Stage %d", zone + 1);
        else
            strcopy(STR(stagename), "Start");
    }
    
    if (!IsRacing(client))
    {
        if (g_ClientEditZone[client] != INVALID_ZONE)   
            PrintHintText(client, "[Timer disabled when editing zones]");
        
#if defined(WINTOX_BHOP)
        else if (g_PlayerLowGrav[client])
            PrintHintText(client, "[Disable low gravity to begin timing]");
#endif
        
        else if (zone == INVALID_ZONE)
            PrintHintText(client, "[Enter the start zone]");
        
        else if (is_in_zone && PLAYER_IN_ZONE(client,0) && !TIMER_STARTED(client))
            PrintHintText(client, "[Leave the start zone]");
        
        else if (TIMER_STARTED(client))
        {
            decl Float:time;
            if (g_CurMapStaged && IS_END_ZONE_T(track, zone) && g_TrackRecords[client][track][style] != INVALID_TRACK_RECORD)
                time = g_TrackRecords[client][track][style];
            if (g_CurMapStaged && zone > 0 && g_LastRunZoneElapsed[client][zone] != 0.0 && is_in_zone)
                time = g_LastRunZoneElapsed[client][zone];
            else
            {
                new Float:relative_from = GetEngineTime();
                new Float:relative_to = g_StartTimes[client][0];
                if (IS_FINISHED(client))
                    relative_from = g_StartTimes[client][g_EndZone[track]];
                else if (IS_PAUSED(client))
                    relative_from = g_PauseStart[client];
                else if (g_CurMapStaged && zone > 0)
                    relative_to = ZONE_TIME[client][zone];
                
                time = relative_from - relative_to;
            }
            
            decl String:sz_time[24];
            
            decl String:plus_minus[64];
            plus_minus[0] = '\0';
            if (g_DisplayPlusMinus[client] && zone >= 1)
            {
                decl Float:zone_time;
                if (!g_CurMapStaged)
                    zone_time = g_StartTimes[client][zone] - g_StartTimes[client][zone-1];
                else
                    zone_time = g_LastRunZoneElapsed[client][zone];
                
                new pm_style = g_PlusMinusStyle[client];
                
                if (g_BestZoneRecords[track][zone][pm_style] != INVALID_ZONE_RECORD)
                {
                    new Float:zone_pm = zone_time - g_BestZoneRecords[track][zone][pm_style];
                    
                    SecondsToTimeString(zone_pm, STR(sz_time), true);
                    Format(STR(plus_minus), "\nZone (%s)", sz_time);
                }
                
                if (g_TrackWorldRecord[track][zone][pm_style] != INVALID_TRACK_RECORD)
                {
                    new Float:wr_pm = zone_time - g_TrackWorldRecord[track][zone][pm_style];
                    
                    SecondsToTimeString(wr_pm, STR(sz_time), true);
                    Format(STR(plus_minus), "%s\nTotal (%s)", plus_minus, sz_time);
                }
            }
            
            if (g_CurMapStaged && zone > 0 && g_LastRunZoneElapsed[client][zone] == 0.0 && is_in_zone)
                strcopy(STR(sz_time), "<not set>");
            else
                SecondsToTimeString(time, STR(sz_time));
            
            new Float:velocity = GetPlayerVelocity(client);
            decl String:sz_speed[32];
            sz_speed[0] = '\0';
            if (g_AllowVelocityDisplay && g_TimerSpeedEnabled[client])
                Format(STR(sz_speed), "\nSpeed: %.1f", velocity);
            
#if defined(WINTOX_BHOP)

            if (g_StartTimes[client][g_EndZone[track]] == 0.0)
            {
                
                PrintHintText(client, "\
                    %s\
                    Time: %s\n\
                    Style: %s\n\
                    %s%s\n\
                    Jumps: %d%s",
                IS_PAUSED(client) ? "[PAUSED]\n" : "", 
                sz_time,
                STYLE_NAMES_C[style],
                stagename, plus_minus,
                g_Jumps[client], sz_speed);
            }
            else
                PrintHintText(client, "\
                    Final time: %s\n\
                    Style: %s\n\
                    Jumps: %d",
                sz_time,
                STYLE_NAMES_C[style],
                g_Jumps[client]);

#elseif defined(WINTOX_SURF)

            if (g_StartTimes[client][g_EndZone[track]] == 0.0 || (g_CurMapStaged && is_in_zone))
            {
                decl String:header[64];
                header[0] = '\0';
                
                if (g_IsPracticing[client])
                    strcopy(STR(header), "[PRACTICE]\n");
                if (IS_PAUSED(client))
                    Format(STR(header), "[PAUSED]\n%s", header);
                
                if (!g_CurMapStaged || (ZONE_TIME[client][zone] != 0.0 && !is_in_zone))
                    PrintHintText(client, "\
                        %sTime: %s\n\
                        Style: %s\n\
                        %s%s%s",
                    header, 
                    sz_time,
                    STYLE_NAMES_C[style],
                    stagename, plus_minus, sz_speed);
                
                else
                    PrintHintText(client, "\
                        %sStage time: %s\n\
                        Stage style: %s\n\
                        %s%s%s",
                    header, 
                    sz_time,
                    STYLE_NAMES_C[g_LastRunZoneStyle[client][zone]],
                    stagename, plus_minus, sz_speed);
            }
            else
                PrintHintText(client, "\
                    %s time: %s\n\
                    Style: %s",
                g_CurMapStaged ? "Total" : "Final", sz_time,
                STYLE_NAMES_C[style]);

#endif

        }
    }
    else
    {
        new opponent = g_RaceOpponents[client];
        
        decl Float:time;
        if (g_RaceEndElapsed[client] > 0.0)
            time = g_RaceEndElapsed[client];
        else
            time = GetEngineTime() - g_RaceStart[client];
        decl String:sz_time[24];
            
        decl String:plus_minus[64];
        plus_minus[0] = '\0';
        if (g_DisplayPlusMinus[client] && zone >= 1)
        {
            decl Float:opponent_pm;
            if (zone < GetPlayerCurrentZone(opponent))
            {
                new Float:zone_time = g_StartTimes[client][zone] - g_StartTimes[client][zone-1];
                new Float:opponent_time = g_StartTimes[opponent][zone] - g_StartTimes[opponent][zone-1];
                
                opponent_pm = zone_time - opponent_time;
            }
            else
                opponent_pm = MAX(g_StartTimes[opponent][GetPlayerCurrentZone(client, 0)], g_RaceStart[opponent]) - g_StartTimes[client][zone];
            
            SecondsToTimeString(opponent_pm, STR(sz_time), true);
            Format(STR(plus_minus), "\n Opponent (%s)", sz_time);
        }
        
        SecondsToTimeString(time, STR(sz_time));
        
        decl String:winner[64];
        if (IsRaceEnded(client))
        {
            if (g_RaceWinner[client] == client)
                strcopy(STR(winner), "You");
            else if (IsClientConnected(client))
                GetClientName(g_RaceWinner[client], STR(winner));
            else
                strcopy(STR(winner), "Your opponent");
        }
        
        if (time < 0.0)
            PrintHintText(client, "\
                [RACE COUNTDOWN]\n\
                %s%s",
            sz_time,
            (time > -1.0) ? "\nSet..." : ((time > -2.0) ? "\nReady..." : ""));
        else if (g_RaceEndElapsed[client] == 0.0)
        {
#if defined(WINTOX_BHOP)
            decl String:sz_speed[48];
            sz_speed[0] = '\0';
            if (g_AllowVelocityDisplay && g_TimerSpeedEnabled[client])
                Format(STR(sz_speed), "\nSpeed: %.1f (opp %.1f)", GetPlayerVelocity(client), GetPlayerVelocity(opponent));
            
            PrintHintText(client, "\
                [RACE]\n\
                Time: %s\n\
                Style: %s (opp %s)\n\
                %s%s\n\
                Jumps: %d (opp %d)%s",
            sz_time,
            STYLE_NAMES_C[style], STYLE_NAMES_C[g_PlayerCurrentStyle[opponent]],
            stagename, plus_minus,
            g_Jumps[client], g_Jumps[opponent], sz_speed);

#elseif defined(WINTOX_SURF)
            PrintHintText(client, "\
                [RACE]\n\
                Time: %s\n\
                Style: %s (opp %s)\n\
                %s%s",
            sz_time,
            STYLE_NAMES_C[style], STYLE_NAMES_C[g_PlayerCurrentStyle[opponent]],
            stagename, plus_minus);
#endif
        }
        else
        {
#if defined(WINTOX_BHOP)
            PrintHintText(client, "\
                %s won!\n\
                Final time: %s\n\
                Style: %s (opp %s)\n\
                Jumps: %d (opp %d)",
            winner,
            sz_time,
            STYLE_NAMES_C[style], STYLE_NAMES_C[g_PlayerCurrentStyle[opponent]],
            g_Jumps[client], g_Jumps[opponent]);

#elseif defined(WINTOX_SURF)
            PrintHintText(client, "\
                %s won!\n\
                Final time: %s\n\
                Style: %s (opp %s)",
            winner,
            sz_time,
            STYLE_NAMES_C[style], STYLE_NAMES_C[g_PlayerCurrentStyle[opponent]]);
            
#endif
        }
    }
}

Timer_DisplayPlusMinus(client)
{
    // Cancel the last disable +/- timer, so we don't end the +/- prematurely
    if (g_DisablePlusMinusTimer[client] != INVALID_HANDLE)
        CloseHandle(g_DisablePlusMinusTimer[client]);
    
    g_DisplayPlusMinus[client] = true;
    g_PlusMinusStyle[client] = g_PlayerCurrentStyle[client];
    g_DisablePlusMinusTimer[client] = CreateTimer(g_DisplayPlusMinusTime, Timer_Callback_DisableDisplayPlusMinus, client);
}

Timer_PlayerReachedZone(client, track, zone, style)
{
    ZoneTime(client, zone);
    g_PlayerCurrentPath[client]++;
    
    Timer_DisplayPlusMinus(client);
    
    new prev_zone = GetPlayerLastZone(client, 0);
    
    Call_StartForward(g_fwdPlayerReachedZone);
    Call_PushCell(client);
    Call_PushCell(track);
    Call_PushCell(zone);
    Call_PushFloat(g_StartTimes[client][zone] - g_StartTimes[client][prev_zone]);
    Call_PushCell(style);
    Call_Finish();
}

Timer_PlayerReachedEnd(client, track, style)
{
    g_IsTiming[client] = false;
    
    Call_StartForward(g_fwdPlayerReachedEnd);
    Call_PushCell(client);
    Call_PushCell(track);
    Call_PushFloat(g_StartTimes[client][g_EndZone[track]] - g_StartTimes[client][0]);
    Call_PushCell(style);
    Call_Finish();
}

public Action:Timer_Callback_DisableDisplayPlusMinus(Handle:timer, any:data)
{
    g_DisplayPlusMinus[data] = false;
    g_DisablePlusMinusTimer[data] = INVALID_HANDLE;
    
    return Plugin_Stop;
}

public fwd_Timer_OnPlayerReachedZone(client, track, zone_id, Float:elapsed, style)
{
    if (g_IsPracticing[client])
        g_PracticeZone[client] = zone_id;
    
    if (track == MAIN_TRACK && g_ZoneRecords[client][track][zone_id][style] == INVALID_ZONE_RECORD)
    {
        g_PlayerPoints[client] += g_CurMapTier;
        win_SQL_UpdatePlayerPoints(client);
    }
    
    // Display +/-
    if (g_PointsToChatEnabled[client] && g_EndZone[track] != zone_id)
    {
        decl String:sz_time[64];
        decl String:plus_minus[84];
        plus_minus[0] = '\0';
        
        new Float:zone_time = g_StartTimes[client][zone_id] - g_StartTimes[client][zone_id-1];
        
        if (g_BestZoneRecords[track][zone_id][style] != INVALID_ZONE_RECORD)
        {
            new Float:zone_pm = zone_time - g_BestZoneRecords[track][zone_id][style];
            
            SecondsToTimeString(zone_pm, STR(sz_time), true);
            Format(STR(plus_minus), " Zone ({G}%s{N})", sz_time);
        }
        
        if (g_TrackWorldRecord[track][zone_id][style] != INVALID_TRACK_RECORD)
        {
            new Float:wr_pm = zone_time - g_TrackWorldRecord[track][zone_id][style];
            
            SecondsToTimeString(wr_pm, STR(sz_time), true);
            Format(STR(plus_minus), "%s | Total ({G}%s{N})", plus_minus, sz_time);
        }
        
        decl String:sz_zone[84];
        if (IS_STR_EMPTY(g_ZoneNames[track][zone_id]))
            Format(STR(sz_zone), "zone {G}%d{N}", zone_id+1);
        else
            Format(STR(sz_zone), "{G}%s{N}", g_ZoneNames[track][zone_id]);
        
        Timer_PrintToChat(client, "{OG}%s{N} | Reached %s%s%s", STYLE_NAMES_C[style], sz_zone, IS_STR_EMPTY(plus_minus) ? "" : " |", plus_minus);
    }
    
    new Float:top_zone_record = g_BestZoneRecords[track][zone_id][style];
    if (top_zone_record == INVALID_ZONE_RECORD || elapsed < top_zone_record)
        g_BestZoneRecords[track][zone_id][style] = elapsed;
    
    if (g_ZoneRecords[client][track][zone_id][style] == INVALID_ZONE_RECORD ||
        elapsed < g_ZoneRecords[client][track][zone_id][style])
    {
        g_ZoneRecords[client][track][zone_id][style] = elapsed;
        win_SQL_UpsertZoneRecord(client, track, zone_id, elapsed, STYLE_NAMES[style]);
    }
    
    if (g_CurMapStaged)
    {
        if (!IS_END_ZONE_T(track, zone_id))
            ZONE_TIME[client][zone_id] = 0.0;
        g_LastRunZoneStyle[client][zone_id] = style;
        g_LastRunZoneElapsed[client][zone_id] = elapsed;
    }
}

public fwd_Timer_OnPlayerReachedEnd(client, track, Float:total_time, style)
{
    // We don't record map completion times when practicing
    if (g_IsPracticing[client])
        return;
    
    // On staged maps, we need to make sure the player has completed ALL stages
    // before they can have a map completion. Plus, we need to add up the
    // elapsed times of each zone, because the zone timestamps are non-linear
    if (g_CurMapStaged)
    {
        // This must be set for the Total time to display in the middle timer
        ZONE_TIME[client][g_EndZone[track]] = GetEngineTime();
        
        total_time = 0.0;
        for (new i=1; i<MAX_ZONES; i++)
        {
            if (ZONE_EMPTY_T(track, i))
                continue;
            
            if (g_ZoneRecords[client][track][i][style] == INVALID_ZONE_RECORD)
                return;
            
            total_time += g_ZoneRecords[client][track][i][style];
            
            if (IS_END_ZONE_T(track, i))
                break;
        }
    }
    
    Client_SetScore(client, Client_GetScore(client) + 1);
    
    new Float:record = g_TrackRecords[client][track][style];
    
    // Global top records
    new Float:top_record = g_TrackWorldRecord[track][0][style];
    new bool:had_top = top_record != INVALID_TRACK_RECORD;
    new top_cmp = FloatComparePlaces(total_time, top_record);
    new bool:is_top = !had_top || total_time < top_record;
    
    // Though we want to show the times as being the same if they're
    // equal up to MAX_TIMER_PLACES, we only save the time if it's
    // actually faster past MAX_TIMER_PLACES.
    if (is_top)
        g_TrackWorldRecord[track][0][style] = total_time;
    
    // Player's best records
    if (record == INVALID_TRACK_RECORD || total_time < record)
    {
        g_TrackRecords[client][track][style] = total_time;
        win_SQL_UpsertTrackRecord(client, track, style, total_time, is_top);
        
        if (is_top)
            SetTrackWorldRecordCache(client, track, style);
    }
    
    // Gross points awww yeah
    if (track == MAIN_TRACK)
    {
        if (record == INVALID_TRACK_RECORD)
        {
            decl earned;
            if (g_ZonesCount[track] < 10)
                earned = (10 - g_ZonesCount[track]) * g_CurMapTier;
            else
                earned = 5;
            
            g_PlayerPoints[client] += earned;
            Timer_PrintToChat(client, "You have earned {G}%d{N} points for completing the map!", earned);
            
            win_SQL_UpdatePlayerPoints(client);
        }
        
        decl String:sz_time[32];
        SecondsToTimeString(total_time, STR(sz_time));
        
        decl String:sz_rel_wr[128];
        if (had_top)
        {
            if (top_cmp != 0)
            {
                SecondsToTimeString(total_time - top_record, STR(sz_rel_wr), true, true, "G");
                Format(STR(sz_rel_wr), "and was %s {OG}%s{N} than the world record%c", sz_rel_wr, is_top ? "faster" : "slower", is_top ? '!' : '.');
            }
            else
                strcopy(STR(sz_rel_wr), "and {OG}matched{N} the world record!");
        }
        else
            strcopy(STR(sz_rel_wr), "and {OG}set{N} the world record!");
        
        Color_ChatSetSubject(client);
#if defined(WINTOX_BHOP)
        Timer_PrintToChatAll("{OG}%s{N} | {T}%N{N} finished the map in {G}%s{N} with {G}%d{N} jumps %s", STYLE_NAMES_C[style], client, sz_time, g_Jumps[client], sz_rel_wr);
#elseif defined(WINTOX_SURF)
        Timer_PrintToChatAll("{OG}%s{N} | {T}%N{N} finished the map in {G}%s{N} %s", STYLE_NAMES_C[style], client, sz_time, sz_rel_wr);
#endif
    }
    else
    {
        decl String:bonus_name[96];
        bonus_name[0] = '\0';
        if (!IS_STR_EMPTY(g_TrackNames[track]))
            Format(STR(bonus_name), " ({OG}%s{N})", g_TrackNames[track]);
        
        if (record == INVALID_TRACK_RECORD)
        {
            new earned = 5 * g_TrackTier[track];
            g_PlayerPoints[client] += earned;
            
            Timer_PrintToChat(client, "You have earned {G}%d{N} points for completing bonus {G}%d{N}%s!", earned, track, bonus_name);
            
            win_SQL_UpdatePlayerPoints(client);
        }
        
        decl String:sz_time[32];
        SecondsToTimeString(total_time, STR(sz_time));
        
#if defined(WINTOX_BHOP)
        Color_ChatSetSubject(client);
        Timer_PrintToChatAll("{OG}%s{N} | {T}%N{N} finished bonus %d%s in {G}%s{N} with {G}%d{N} jumps!", STYLE_NAMES_C[style], client, track, bonus_name, sz_time, g_Jumps[client]);
#endif
    }
}



/* ========================================================================
                                    MENUS
   ======================================================================== */

#define TSET_MENU_MIDDLE "middle"
#define TSET_MENU_SIDE "side"
#define TSET_MENU_POINTS "points"
#define TSET_MENU_SPEED "speed"

Handle:BuildTimerSettingsMenu(client)
{
    decl String:buffer[64];
    new Handle:menu = CreateMenu(MenuHandler_TimerSettings);
    
    SetMenuTitle(menu, "Timer settings");
    SetMenuExitButton(menu, true);
    
    Format(STR(buffer), "Middle timer: %s", g_TimerEnabled[client] ? "On" : "Off");
    AddMenuItem(menu, TSET_MENU_MIDDLE, buffer);
    
    Format(STR(buffer), "Side stage info: %s", g_SideStageInfoEnabled[client] ? "On" : "Off");
    AddMenuItem(menu, TSET_MENU_SIDE, buffer);
    
    Format(STR(buffer), "Points and +/- printed to chat: %s", g_PointsToChatEnabled[client] ? "On" : "Off");
    AddMenuItem(menu, TSET_MENU_POINTS, buffer);
    
    if (g_AllowVelocityDisplay)
    {
        Format(STR(buffer), "Show speed in middle timer: %s", g_TimerSpeedEnabled[client] ? "On" : "Off");
        AddMenuItem(menu, TSET_MENU_SPEED, buffer);
    }
    
    return menu;
}

public MenuHandler_TimerSettings(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
#define SET_COOKIE(%1,%2) \
    new Handle:cookie = FindClientCookie((%1)); \
    if (cookie != INVALID_HANDLE) \
    { \
        decl String:sz_cookie[8]; \
        IntToString(_:%2[param1], STR(sz_cookie)); \
        SetClientCookie(param1, cookie, sz_cookie); \
        CloseHandle(cookie); \
    }
        
        if (StrEqual(info, TSET_MENU_MIDDLE))
        {
            g_TimerEnabled[param1] = !g_TimerEnabled[param1];
            SET_COOKIE(COOKIE_TIMER_ENABLED,g_TimerEnabled)
            
            if (!g_TimerEnabled[param1])
                HideHintText(param1);
        }
        else if (StrEqual(info, TSET_MENU_SIDE))
        {
            g_SideStageInfoEnabled[param1] = !g_SideStageInfoEnabled[param1];
            SET_COOKIE(COOKIE_SIDE_STAGE_INFO_ENABLED,g_SideStageInfoEnabled)
        }
        else if (StrEqual(info, TSET_MENU_POINTS))
        {
            g_PointsToChatEnabled[param1] = !g_PointsToChatEnabled[param1];
            SET_COOKIE(COOKIE_POINTS_TO_CHAT_ENABLED,g_PointsToChatEnabled)
        }
        
        else if (g_AllowVelocityDisplay && StrEqual(info, TSET_MENU_SPEED))
        {
            g_TimerSpeedEnabled[param1] = !g_TimerSpeedEnabled[param1];
            SET_COOKIE(COOKIE_TIMER_SPEED_ENABLED,g_TimerSpeedEnabled)
        }
#undef SET_COOKIE
        
        // Cancel this menu without redisplaying previous submenu
        PrevSubmenu(param1, false);
        // Rebuild the menu based on the client's new settings and display it
        DisplaySubmenu(BuildTimerSettingsMenu(param1), param1, MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

ShowTopStyle(client, style, const String:mapname[]="", start=0)
{
    decl String:esc_mapname[128];
    decl bool:partial;
    
    if (IS_STR_EMPTY(mapname))
    {
        strcopy(g_TimerTopMapName[client], sizeof(g_TimerTopMapName[]), g_CurMapName);
        partial = false;
    }
    else
    {
        strcopy(g_TimerTopMapName[client], sizeof(g_TimerTopMapName[]), mapname);
        partial = true;
    }
    
    SQL_EscapeString(g_Database, g_TimerTopMapName[client], STR(esc_mapname));
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, start);
    WritePackString(pack, g_TimerTopMapName[client]);
    WritePackCell(pack, style);
    ResetPack(pack);
    
    g_TimerTopStyle[client] = style;
    
    if (partial)
    {
        SQLTH_H(GetTopPartial, T_GetTop, pack, WINTOX_GAMETYPE, esc_mapname, MAIN_TRACK, STYLE_NAMES[style], start);
    }
    else
    {
        SQLTH(GetTop, pack, WINTOX_GAMETYPE, esc_mapname, MAIN_TRACK, STYLE_NAMES[style], start);
    }
}

public T_GetTop(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl client;
    decl start;
    decl String:mapname[64];
    decl style;
    
    new Handle:pack = Handle:data;
    client = ReadPackCell(pack);
    start = ReadPackCell(pack);
    ReadPackString(pack, STR(mapname));
    style = ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving top times for %s, %s style (for %L, starting at %d): %s", mapname, STYLE_NAMES[style], client, start, error);
        Timer_Reply(client, "There was a {OG}database error{N} retrieving race records. Please try again.");
        return;
    }
    
    if (client == 0 || !IsClientInGame(client))
        return;
    
    new rows = SQL_GetRowCount(hndl);
    if (rows == 0)
    {
        Timer_Reply(client, "There are no top {OG}%s{N} times! {G}Go make some{N}!", STYLE_NAMES[style]);
        return;
    }
    
    new Handle:panel = CreatePanel(GetMenuStyleHandle(MenuStyle_Default));
    
    new i = 0;
    decl String:records[512];
    
    while (SQL_FetchRow(hndl) && i<10)
    {
        decl String:latest_name[64];
        decl String:bsp_name[64];
        decl String:sz_time[32];
        decl Float:time;
#if defined(WINTOX_BHOP)
        decl jumps;
#endif
        
        SQL_FetchString(hndl, 1, STR(latest_name));
        time = SQL_FetchFloat(hndl, 2);
#if defined(WINTOX_BHOP)
        jumps = SQL_FetchInt(hndl, 3);
#endif
        
        if (i == 0)
        {
            SQL_FetchString(hndl, 0, STR(bsp_name));
            Format(STR(records), "Top %s times on %s, %d to %d\n \n", STYLE_NAMES[style], bsp_name, start+1, start+rows);
        }
        
        SecondsToTimeString(time, STR(sz_time));
        
#if defined(WINTOX_BHOP)
        Format(STR(records), "%s%d. %s - %s - Jumps: %d\n", records, start + i + 1, latest_name, sz_time, jumps);
        
#elseif defined(WINTOX_SURF)
        Format(STR(records), "%s%d. %s - %s\n", records, start + i + 1, latest_name, sz_time);
#endif
        
        i++;
    }
    
    DrawPanelItem(panel, records);
    DrawPanelText(panel, " ");
    
    new keys = (1<<0) | (1<<9);
    
    decl String:buffer[32];
    if (start > 0)
    {
        Format(STR(buffer), "Previous");
        SetPanelCurrentKey(panel, 8);
        DrawPanelItem(panel, buffer);
        
        keys |= (1<<7);
    }
    
    if (rows > 10)
    {
        Format(STR(buffer), "Next");
        SetPanelCurrentKey(panel, 9);
        DrawPanelItem(panel, buffer);
        
        keys |= (1<<8);
    }
    
    SetPanelCurrentKey(panel, 10);
    Format(STR(buffer), "Exit");
    DrawPanelItem(panel, buffer);
    
    SetPanelKeys(panel, keys);
    
    g_TimerTopStart[client] = start;
    SendPanelToClient(panel, client, MenuHandler_Top, MENU_TIME_FOREVER);
}

public MenuHandler_Top(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        new start = g_TimerTopStart[param1];
        
        if (param2 == 8)
            ShowTopStyle(param1, g_TimerTopStyle[param1], g_TimerTopMapName[param1], MAX(start - 10, 0));
        else if (param2 == 9)
            ShowTopStyle(param1, g_TimerTopStyle[param1], g_TimerTopMapName[param1], start + 10);
        
        if (param2 == 10)
            MenuPlayExitSound(param1);
        else
            MenuPlaySelectSound(param1);
    }
}

Handle:GetPracticeMenu()
{
    if (g_PracticeMenu == INVALID_HANDLE)
        RebuildPracticeMenu();
    
    return g_PracticeMenu;
}

RebuildPracticeMenu()
{
    if (g_PracticeMenu != INVALID_HANDLE)
        CloseHandle(g_PracticeMenu);
    
    g_PracticeMenu = CreateMenu(MenuHandler_Practice);
    SetMenuTitle(g_PracticeMenu, "Select a stage to practice");
    
    new bool:has_items_drawn = false;
    for (new zone=0; zone<g_EndZone[MAIN_TRACK]; zone++)
    {
        if (ZONE_EMPTY_T(MAIN_TRACK, zone))
            continue;
        
        decl String:buffer[64];
        GetStageName(MAIN_TRACK, zone, STR(buffer));
        
        decl String:sz_zone[8];
        IntToString(zone, STR(sz_zone));
        
        AddMenuItem(g_PracticeMenu, sz_zone, buffer);
        has_items_drawn = true;
    }
    
    // Add an item if the menu is empty to prevent crashes
    if (!has_items_drawn)
        AddMenuItem(g_PracticeMenu, "", "-");
}

DisplayPracticeMenu(client)
{
    DisplaySubmenu(GetPracticeMenu(), client, MENU_TIME_FOREVER, false);
}

public MenuHandler_Practice(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:sz_info[8];
        GetMenuItem(menu, param2, STR(sz_info));
        
        if (IS_STR_EMPTY(sz_info))
            return;
        
        decl zone;
        if (StringToIntEx(sz_info, zone) == 0)
        {
            Timer_Reply(param1, "Invalid stage selection. Please try again.");
            FakeClientCommand(param1, "sm_practice");
            return;
        }
        
        StartTimer(param1);
        g_PracticeZone[param1] = zone;
        g_PlayerEnteredStart[param1] = true;
        g_PlayerCurrentTrack[param1] = MAIN_TRACK;
        g_PlayerCurrentZone[param1] = zone;
        g_PlayerCurrentPath[param1] = GetPathOfZone(MAIN_TRACK, zone);
        g_IsPracticing[param1] = true;
        g_IsTiming[param1] = true;
        
        ZoneTeleport(param1, zone, MAIN_TRACK);
        
        decl String:stagename[64];
        GetStageName(MAIN_TRACK, zone, STR(stagename));
        Timer_Reply(param1, "Began practicing on {G}%s{N}, but you may practice on any stage.", stagename);
        Timer_Reply(param1, "Use {OG}!restart{N} to stop practicing.");
    }
    else
        HANDLE_CANCEL();
}



/* ========================================================================
                                 CONCOMMANDS
   ======================================================================== */

#if defined(WINTOX_DEBUG)
public Action:ConCommand_SetTrackWR(client, args)
{
    CHECK_ARGS_SPAN(3,4)
    
    decl track;
    decl style;
    decl zone;
    decl Float:time;
    
    decl String:buffer[64];
    GetCmdArg(1, STR(buffer));
    if (StringToIntEx(buffer, track) == 0 || track < 0 || track >= MAX_TRACKS)
    {
        Timer_Reply(client, "Track index must be an integer between 0 and %d, inclusive", MAX_TRACKS-1);
        return Plugin_Handled;
    }
    
    GetCmdArg(2, STR(buffer));
    if (StringToIntEx(buffer, style) == 0 || style < 0 || style >= MAX_STYLES)
    {
        Timer_Reply(client, "Style index must be an integer between 0 and %d, inclusive", MAX_STYLES-1);
        return Plugin_Handled;
    }
    
    new time_arg = 3;
    if (args == 4)
    {
        time_arg = 4;
        GetCmdArg(3, STR(buffer));
        if (StringToIntEx(buffer, zone) == 0 || zone < 0 || zone >= MAX_ZONES)
        {
            Timer_Reply(client, "Zone index must be an integer between 0 and %d, inclusive", MAX_ZONES-1);
            return Plugin_Handled;
        }
    }
    else
        zone = g_EndZone[track];
    
    GetCmdArg(time_arg, STR(buffer));
    if (StringToFloatEx(buffer, time) == 0)
    {
        Timer_Reply(client, "Time must be a valid floating point number");
        return Plugin_Handled;
    }
    
    g_TrackWorldRecord[track][zone][style] = time;
    Timer_Reply(client, "g_TrackWorldRecord[%d][%d][%d] = %f", track, zone, style, time);
    
    return Plugin_Handled;
}

public Action:ConCommand_ListStyles(client, args)
{
    NO_ARGS()
    
    new String:plural[2];
#if MAX_STYLES != 1
    plural[0] = 's';
#endif
    
    Timer_Reply(client,  "Printing %d style%s", MAX_STYLES, plural);
    
    for (new i=0; i<MAX_STYLES; i++)
        Timer_Reply(client, "{G}%3d {OG}%-10s{N} (DB type \"{OG}%s{N}\")", i, STYLE_NAMES_C[i], STYLE_NAMES[i]);
    
    return Plugin_Handled;
}
#endif//WINTOX_DEBUG

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
    
    if (!IsOnPlayingTeam(client))
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
    
    if (!IsOnPlayingTeam(client))
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
    
    if (!IsOnPlayingTeam(client))
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

public Action:ConCommand_Top(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:mapname[64];
    GetCmdArg(1, STR(mapname));
    
    ShowTopStyle(client, STYLE_NORMAL, mapname, 0);
    
    return Plugin_Handled;
}

public Action:ConCommand_Wr(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:partial[64];
    GetCmdArgString(STR(partial));
    
    win_SQL_GetWorldRecord(client, MAIN_TRACK, STYLE_NORMAL, partial);
    
    return Plugin_Handled;
}

#if defined(WINTOX_BHOP) || defined(WINTOX_SURF)
public Action:ConCommand_TopSW(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:mapname[64];
    GetCmdArg(1, STR(mapname));
    
    ShowTopStyle(client, STYLE_SIDEWAYS, mapname, 0);
    
    return Plugin_Handled;
}
#endif

#if defined(WINTOX_BHOP)
public Action:ConCommand_TopW(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:mapname[64];
    GetCmdArg(1, STR(mapname));
    
    ShowTopStyle(client, STYLE_WONLY, mapname, 0);
    
    return Plugin_Handled;
}

public Action:ConCommand_WrW(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:partial[64];
    GetCmdArgString(STR(partial));
    
    win_SQL_GetWorldRecord(client, MAIN_TRACK, STYLE_WONLY, partial);
    
    return Plugin_Handled;
}

public Action:ConCommand_WrSW(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:partial[64];
    GetCmdArgString(STR(partial));
    
    win_SQL_GetWorldRecord(client, MAIN_TRACK, STYLE_SIDEWAYS, partial);
    
    return Plugin_Handled;
}
#endif // defined(WINTOX_BHOP)

public Action:ConCommand_Rank(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:partial[64];
    GetCmdArgString(STR(partial));
    
    win_SQL_GetPlayerRank(client, partial);
    
    return Plugin_Handled;
}

public Action:ConCommand_CRank(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:partial[64];
    GetCmdArgString(STR(partial));
    
    win_SQL_GetPlayerCountryRank(client, partial);
    
    return Plugin_Handled;
}

public Action:ConCommand_Pc(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    decl String:partial[64];
    GetCmdArgString(STR(partial));
    
    win_SQL_GetPercentCompletion(client, partial);
    
    return Plugin_Handled;
}

#if defined(WINTOX_SURF)
public Action:ConCommand_WrB(client, args)
{
    return Handle_ConCommand_WrB_WrBSW(client, args, STYLE_NORMAL);
}

public Action:ConCommand_WrBSW(client, args)
{
    return Handle_ConCommand_WrB_WrBSW(client, args, STYLE_SIDEWAYS);
}

Action:Handle_ConCommand_WrB_WrBSW(client, args, style)
{
    CHECK_ARGS_LOWER(1)
    
    decl String:sz_bonus[12];
    decl String:partial[96];
    decl bonus;
    
    GetCmdArg(1, STR(sz_bonus));
    new bytes = StringToIntEx(sz_bonus, bonus);
    if (bytes == 0 || bonus < 1 || bonus >= MAX_TRACKS)
    {
        Timer_Reply(client, "\"{G}%s{N}\" is an invalid bonus index.", sz_bonus);
        return Plugin_Handled;
    }
    
    partial[0] = '\0';
    if (args >= 2)
    {
        GetCmdArgString(STR(partial));
        strcopy(STR(partial), partial[bytes + 1]);
    }
    
    if (IS_STR_EMPTY(partial) && TRACK_EMPTY(bonus))
    {
            Timer_Reply(client, "{OG}Bonus {G}%d{N} doesn't exist.", bonus);
            return Plugin_Handled;
    }
    
    win_SQL_GetWorldRecord(client, bonus, style, partial);
    
    return Plugin_Handled;
}

public Action:ConCommand_WrS(client, args)
{
    return Handle_ConCommand_WrS_WrSSW(client, args, STYLE_NORMAL);
}

public Action:ConCommand_WrSSW(client, args)
{
    return Handle_ConCommand_WrS_WrSSW(client, args, STYLE_SIDEWAYS);
}

Action:Handle_ConCommand_WrS_WrSSW(client, args, style)
{
    CHECK_ARGS_LOWER(1)
    
    decl String:sz_stage[12];
    decl String:partial[96];
    decl stage;
    
    GetCmdArg(1, STR(sz_stage));
    new bytes = StringToIntEx(sz_stage, stage);
    if (bytes == 0 || stage <= 0)
    {
        Timer_Reply(client, "\"{G}%s{N}\" is an invalid stage number.", sz_stage);
        return Plugin_Handled;
    }
    
    partial[0] = '\0';
    if (args >= 2)
    {
        GetCmdArgString(STR(partial));
        strcopy(STR(partial), partial[bytes + 1]);
    }
    
    if (IS_STR_EMPTY(partial) && g_ZonesCount[MAIN_TRACK] <= stage)
    {
        Timer_Reply(client, "{OG}Stage {G}%d{N} doesn't exist.", stage);
        return Plugin_Handled;
    }
    
    win_SQL_GetWorldRecord(client, MAIN_TRACK, style, partial, stage - 1);
    
    return Plugin_Handled;
}

public Action:ConCommand_Practice(client, args)
{
    NO_ARGS()
    NO_FAKECLIENTS()
    
    if (client == 0)
    {
        Timer_Reply(client, "You know you can't practice on the server console, BUB!");
        return Plugin_Handled;
    }
    
    DisplayPracticeMenu(client);
    
    return Plugin_Handled;
}
#endif//WINTOX_SURF



/* ========================================================================
                              UTILITY FUNCTIONS
   ======================================================================== */

// Resets all the zone times and jumps and sets IsTiming false
ResetTimer(client)
{
    for (new i=0; i<MAX_ZONES; i++)
        g_StartTimes[client][i] = 0.0;
    
    if (g_PracticeZone[client] != INVALID_ZONE && g_PracticeZone[client] != g_PlayerCurrentZone[client])
        g_IsPracticing[client] = false;
    
    g_PlayerCurrentTrack[client] = MAIN_TRACK;
    g_PlayerCurrentZone[client] = INVALID_ZONE;
    g_PlayerCurrentPath[client] = INVALID_PATH;
    g_PlayerCurrentStyle[client] = MAX_STYLES-1;
    g_PlayerEnteredStart[client] = false;
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
    
    if (g_ClientEditZone[client] != INVALID_ZONE)
        return false;
    
    return true;
}

WipeBestZoneRecords()
{
    for (new track=0; track<MAX_TRACKS; track++)
        for (new zone=0; zone<MAX_ZONES; zone++)
            for (new style=0; style<MAX_STYLES; style++)
                g_BestZoneRecords[track][zone][style] = INVALID_ZONE_RECORD;
}

StyleNameToIndex(const String:stylename[])
{
    for (new style=0; style<MAX_STYLES; style++)
        if (StrEqual(stylename, STYLE_NAMES[style]))
            return style;
    
    return 0;
}

stock SecondsToTime(Float:time, &hours, &minutes, &Float:seconds)
{
    time = FloatAbs(time);
    
    hours = RoundToFloor(time) / SECONDS_IN_HOUR;
    minutes = (RoundToFloor(time) % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
    seconds = time - (hours * SECONDS_IN_HOUR) - (minutes * SECONDS_IN_MINUTE);
}

stock SecondsToTimeString(Float:time, String:buffer[], maxlength, bool:plus=false, bool:parens=false, const String:time_color[]="", const String:end_color[]="N")
{
    decl String:sz_hours[8];
    decl hours, minutes, Float:seconds;
    SecondsToTime(time, hours, minutes, seconds);
    
    sz_hours[0] = '\0';
    if (hours != 0)
        Format(STR(sz_hours), "%02d:", hours);
    
    decl String:time_color_code[8];
    decl String:end_color_code[8];
    time_color_code[0] = '\0';
    end_color_code[0] = '\0';
    
    if (!IS_STR_EMPTY(time_color))
    {
        Format(STR(time_color_code), "{%s}", time_color);
        Format(STR(end_color_code), "{%s}", end_color);
    }
    
    Format(buffer, maxlength, "%s%s%s%s%02d:%06.3f%s%s", parens ? "(" : "", time_color_code, (time >= 0) ? (plus ? "+" : "") : "-", sz_hours, minutes, seconds, end_color_code, parens ? ")" : "");
}

stock GetClosestStyleForBestRecord(track, zone, style)
{
    if (g_BestZoneRecords[track][zone][style] != INVALID_ZONE_RECORD)
        return style;
    
    for (new i=MAX_STYLES; i>=0; i--)
        if (g_BestZoneRecords[track][zone][i] != INVALID_ZONE_RECORD)
            return i;
    
    return STYLE_NORMAL;
}

stock SetTrackWorldRecordCache(client, track, style)
{
    new end_zone = g_EndZone[track];
    for (new i=1; i<end_zone; i++)
        g_TrackWorldRecord[track][i][style] = g_ZoneRecords[client][track][i][style];
    
    g_TrackWorldRecord[track][0][style] = g_TrackRecords[client][track][style];
}



/* ========================================================================
                                  SQL SHIT
   ======================================================================== */

// Retrieve and store the fastest times for each zone in each track in each style
CacheBestZoneRecords(bool:wipe=false)
{
    decl String:query[256];
    
    if (wipe)
        WipeBestZoneRecords();
    
    Format(STR(query), SQL_QUERY[GetBestZoneRecords], g_CurMapID);
    SQLT(query, T_GetBestZoneRecords, INVALID_HANDLE);
}

public T_GetBestZoneRecords(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving best zone records: %s", error);
        return;
    }
    
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl zone;
        decl String:sz_type[16];
        decl style;
        decl Float:time;
        
        track = SQL_FetchInt(hndl, 0);
        zone = SQL_FetchInt(hndl, 1);
        SQL_FetchString(hndl, 2, STR(sz_type));
        time = SQL_FetchFloat(hndl, 3);
        
        style = StyleNameToIndex(sz_type);
        
        g_BestZoneRecords[track][zone][style] = time;
    }
}

win_SQL_GetPercentCompletion(client, const String:partial[64]="")
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackString(pack, partial);
    WritePackCell(pack, _:GetCmdReplySource());
    ResetPack(pack);
    
    decl String:query[1152];
    if (IS_STR_EMPTY(partial))
    {
        if (g_PlayerIDs[client] == -1)
        {
#if defined(WINTOX_BHOP)
            LogError("Invalid player ID for client %L trying to retrieve %% completion", client);
#endif
            return;
        }
        
        SQLTH_D(GetPercentCompletion, pack, MAX_STYLES, WINTOX_GAMETYPE, g_PlayerIDs[client]);
    }
    else
    {
        decl String:esc_partial[128];
        SQL_EscapeString(g_Database, partial, STR(esc_partial));
        
        SQLTH_H_D(GetPercentCompletionPartial, T_GetPercentCompletion, pack, MAX_STYLES, WINTOX_GAMETYPE, esc_partial);
    }
}

public T_GetPercentCompletion(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl client;
    decl String:partial[64];
    decl ReplySource:reply_to;
    
    new Handle:pack = Handle:data;
    client = ReadPackCell(pack);
    ReadPackString(pack, STR(partial));
    reply_to = ReplySource:ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving percent completion (for display to client %L)%s%s: %s", client, IS_STR_EMPTY(partial) ? " matching the caller." : " matching partial: ", IS_STR_EMPTY(partial) ? "" : partial, error);
        
        ENTER_REPLY(reply_to);
        Timer_Reply(client, "Database error retrieving percent completion. Please try again.");
        EXIT_REPLY();
        
        return;
    }
    
    if (SQL_GetRowCount(hndl) == 0)
    {
        decl String:buffer[128];
        if (IS_STR_EMPTY(partial))
            Format(STR(buffer), "for {T}you{N}. {OG}Go set some{N}!");
        else
            Format(STR(buffer), "matching \"{G}%s{N}\".", partial);
        
        Color_ChatSetSubject(client);
        ENTER_REPLY(reply_to);
        Timer_Reply(client, "There are {G}no records{N} stored %s", buffer);
        EXIT_REPLY();
        
        return;
    }
    
    decl Float:pc;
    decl String:latest_name[64];
    SQL_FetchRow(hndl);
    
    SQL_FetchString(hndl, 0, STR(latest_name));
    pc = SQL_FetchFloat(hndl, 1);
    
    Color_ChatSetSubject(client);
    ENTER_REPLY(reply_to);
    Timer_Reply(client, "%s%s{N} has completed {G}%.1f%%{N} of all maps and bonuses.", IS_STR_EMPTY(partial) ? "{T}" : "{G}", latest_name, pc);
    EXIT_REPLY();
}

win_SQL_UpsertTrackRecord(client, track, style, Float:total_time, bool:is_top=false)
{
    if (g_PlayerIDs[client] == -1)
    {
#if defined(WINTOX_DEBUG)
        LogError("UpsertTrackRecord called on invalid client. Params (%d, %d, %d, %f)", client, track, style, total_time);
#endif
        return;
    }
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, track);
    WritePackCell(pack, style);
    WritePackCell(pack, _:is_top);
    ResetPack(pack);
    
    SQLTH(UpsertTrackRecord, pack, g_PlayerIDs[client], g_CurMapID, track, STYLE_NAMES[style], total_time, g_Jumps[client]);
}

public T_UpsertTrackRecord(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl Handle:pack;
    decl client;
    decl track;
    decl style;
    decl bool:is_top;
    
    pack = Handle:data;
    client = ReadPackCell(pack);
    track = ReadPackCell(pack);
    style = ReadPackCell(pack);
    is_top = bool:ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error updating player %L track %d record: %s", client, track, error);
        return;
    }
    
    if (is_top)
    {
        new fk_track_record = SQL_GetInsertId(hndl);
        SQLTH(UpsertTopRecord, client, g_CurMapID, track, STYLE_NAMES[style], fk_track_record);
    }
}
SQLT_HANDLER(UpsertTopRecord, "Error upserting top record for %L: %s", data)

win_SQL_UpdatePlayerPoints(client)
{
    decl String:query[256];
    
    // The player isn't in the database, so an update would be pointless
    if (g_PlayerIDs[client] == -1)
    {
#if defined(WINTOX_DEBUG)
        LogError("UpdatePlayerPoints called on invalid client. Params (%d)", client);
#endif
        return;
    }
    
    Format(STR(query), SQL_QUERY[UpdatePlayerPoints],
        g_PlayerPoints[client], g_PlayerIDs[client]);
    SQLT(query, T_UpdatePlayerPoints, client);
}
SQLT_HANDLER(UpdatePlayerPoints, "Error updating player %L points: %s", data)

win_SQL_UpsertZoneRecord(client, track, zone_id, Float:time, const String:type[]="")
{
    decl String:query[512];
    decl String:esc_type[32];
    
    // The player has no database ID; we can't record their progress!
    if (g_PlayerIDs[client] == -1)
    {
#if defined(WINTOX_DEBUG)
        LogError("UpsertTrackRecord called on invalid client. Params (%d, %d, %d, %f, \"%s\")", client, track, zone_id, time, type);
#endif
        return;
    }
    
    SQL_EscapeString(g_Database, type, STR(esc_type));
    
    Format(STR(query), SQL_QUERY[UpsertZoneRecord],
        g_CurMapID, g_PlayerIDs[client], g_ZoneIDs[track][zone_id], esc_type,
        time, GetTime());
    
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
        
        LogError("Error upserting zone record (client %d %L, track %d, zone %d): %s",
            client, client, track, zone_id, error);
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
        LogError("Attempted to retrieve the zone records of player %L with bad DB ID %d.",
            client, g_PlayerIDs[client]);
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
        LogError("Error retrieving zone records for client %d (%L): %s",
            client, client, error);
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
        
        // More info is always good for a player!
        if (track == MAIN_TRACK)
            g_LastRunZoneElapsed[client][zone_id] = time;
    }
}

CacheTrackRecords(client)
{
    if (g_PlayerIDs[client] == -1)
    {
        LogError("Attempted to retrieve the track records of player %L with bad DB ID %d.",
            client, g_PlayerIDs[client]);
        return;
    }
    
    SQLTH_H(GetTrackRecords, T_CacheTrackRecords, client, g_CurMapID, g_PlayerIDs[client]);
}

public T_CacheTrackRecords(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new client = data;
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving track records for client %d (%L): %s", client, client, error);
        return;
    }
    
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl String:sz_type[16];
        decl Float:time;
        
        track = SQL_FetchInt(hndl, 0);
        SQL_FetchString(hndl, 1, STR(sz_type));
        time = SQL_FetchFloat(hndl, 2);
        
        new style = StyleNameToIndex(sz_type);
        g_TrackRecords[client][track][style] = time;
    }
}

CacheTrackTopRecords()
{
    SQLTH(GetTrackTotalTopRecords, INVALID_HANDLE, g_CurMapID);
}

public T_GetTrackTotalTopRecords(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving track top records: %s", error);
        return;
    }
    
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl String:sz_type[16];
        decl Float:time;
        
        track = SQL_FetchInt(hndl, 0);
        SQL_FetchString(hndl, 1, STR(sz_type));
        time = SQL_FetchFloat(hndl, 2);
        
        new style = StyleNameToIndex(sz_type);
        g_TrackWorldRecord[track][0][style] = time;
    }
}

CacheTrackTopRecordsZoneRecords()
{
    SQLTH(GetTopRecordsZoneRecords, INVALID_HANDLE, g_CurMapID);
}

public T_GetTopRecordsZoneRecords(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving top records' zone records: %s", error);
        return;
    }
    
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl zone_id;
        decl String:sz_type[16];
        decl Float:time;
        
        track = SQL_FetchInt(hndl, 0);
        zone_id = SQL_FetchInt(hndl, 1);
        SQL_FetchString(hndl, 2, STR(sz_type));
        time = SQL_FetchFloat(hndl, 3);
        
        new style = StyleNameToIndex(sz_type);
        g_TrackWorldRecord[track][zone_id][style] = time;
    }
}

win_SQL_GetWorldRecord(client, track=MAIN_TRACK, style=STYLE_NORMAL,
        const String:mapname[]="", zone=INVALID_ZONE)
{   
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, track);
    WritePackCell(pack, style);
    WritePackString(pack, mapname);
    WritePackCell(pack, _:GetCmdReplySource());
#if defined(WINTOX_SURF)
    WritePackCell(pack, zone);
#endif
    ResetPack(pack);
    
    if (IS_STR_EMPTY(mapname))
    {
        if (zone == INVALID_ZONE)
        {
            SQLTH(GetWorldRecord, pack, g_CurMapID, track, STYLE_NAMES[style]);
        }
        else
        {
            SQLTH_H(GetWorldRecordStage, T_GetWorldRecord, pack, \
                g_CurMapID, track, zone, STYLE_NAMES[style]);
        }
    }
    else
    {
        decl String:esc_mapname[128];
        SQL_EscapeString(g_Database, mapname, STR(esc_mapname));
        
        if (zone == INVALID_ZONE)
        {
            SQLTH_H(GetWorldRecordPartial, T_GetWorldRecord, pack, WINTOX_GAMETYPE, \
                esc_mapname, track, STYLE_NAMES[style]);
        }
        else
        {
            SQLTH_H(GetWorldRecordStagePartial, T_GetWorldRecord, pack, WINTOX_GAMETYPE, \
                esc_mapname, track, zone, STYLE_NAMES[style]);
        }
    }
}

public T_GetWorldRecord(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl client;
    decl track;
    decl style;
    decl String:mapname[64];
    decl ReplySource:reply_to;
    decl zone;
    
    new Handle:pack = Handle:data;
    client = ReadPackCell(pack);
    track = ReadPackCell(pack);
    style = ReadPackCell(pack);
    ReadPackString(pack, STR(mapname));
    reply_to = ReplySource:ReadPackCell(pack);
#if defined(WINTOX_SURF)
    zone = ReadPackCell(pack);
#else
    zone = INVALID_ZONE;
#endif
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving %s world record for \"%s\", track %d, zone %d: %s",
            STYLE_NAMES_C[style], IS_STR_EMPTY(mapname) ? g_CurMapName : mapname,
            track, zone, error);
        
        if (client != -1)
        {
            ENTER_REPLY(reply_to);
            Timer_Reply(client, "Database error retrieving world record for \"{OG}%s{N}\". Please try again.", \
                IS_STR_EMPTY(mapname) ? g_CurMapName : mapname);
            EXIT_REPLY();
        }
        
        return;
    }
    
    new bool:has_result = SQL_FetchRow(hndl);
    if (!has_result || SQL_IsFieldNull(hndl, 0))
    {
        ENTER_REPLY(reply_to);
        
#if defined(WINTOX_SURF)
        if (track > 0)
            Timer_Reply(client, "{OG}%s{N} | There are no {OG}Bonus %d{N} world records for \"{OG}%s{N}\"", \
                STYLE_NAMES_C[style], track, IS_STR_EMPTY(mapname) ? g_CurMapName : mapname);
        else if (zone != INVALID_ZONE)
        {
            decl String:sz_stage[64];
            sz_stage[0] = '\0';
            
            if (has_result)
                SQL_FetchString(hndl, 2, STR(sz_stage));
            
            if (IS_STR_EMPTY(sz_stage))
                Format(STR(sz_stage), "Stage %d", zone + 1);
            
            Timer_Reply(client, "{OG}%s{N} | There are no {OG}%s{N} world records for \"{OG}%s{N}\"", \
                STYLE_NAMES_C[style], sz_stage, IS_STR_EMPTY(mapname) ? g_CurMapName : mapname);
        }
        else
            Timer_Reply(client, "{OG}%s{N} | There are no world records for \"{OG}%s{N}\"", \
                STYLE_NAMES_C[style], IS_STR_EMPTY(mapname) ? g_CurMapName : mapname);
        
#else
        Timer_Reply(client, "There are no {OG}%s{N} world records for \"{OG}%s{N}\"", \
            STYLE_NAMES_C[style], IS_STR_EMPTY(mapname) ? g_CurMapName : mapname);
#endif
        
        EXIT_REPLY();
        
        return;
    }
    
    decl String:latest_name[64];
    decl Float:time;
    
    SQL_FetchString(hndl, 0, STR(latest_name));
    time = SQL_FetchFloat(hndl, 1);
    
    decl String:sz_time[32];
    SecondsToTimeString(time, STR(sz_time));
    
#if defined(WINTOX_BHOP)
    new jumps = SQL_FetchInt(hndl, 2);
    
    Timer_Reply(client, "{OG}%s{N} | World record holder is {G}%s{N} with a time of {G}%s{N} and {G}%d{N} jumps!", \
        STYLE_NAMES_C[style], latest_name, sz_time, jumps);
        
#elseif defined(WINTOX_SURF)
    if (track > 0)
        Timer_Reply(client, "{OG}%s{N} | World record holder for {OG}Bonus %d{N} is {G}%s{N} with a time of {G}%s{N}", \
            STYLE_NAMES_C[style], track, latest_name, sz_time);
    else if (zone != INVALID_ZONE)
    {
        decl String:sz_stage[64];
        SQL_FetchString(hndl, 2, STR(sz_stage));
        
        if (IS_STR_EMPTY(sz_stage))
            Format(STR(sz_stage), "Stage %d", zone + 1);
        
        Timer_Reply(client, "{OG}%s{N} | World record holder for {OG}%s{N} is {G}%s{N} with a time of {G}%s{N}", \
            STYLE_NAMES_C[style], sz_stage, latest_name, sz_time);
    }
    else
        Timer_Reply(client, "{OG}%s{N} | World record holder is {G}%s{N} with a time of {G}%s{N}", \
            STYLE_NAMES_C[style], latest_name, sz_time);
#endif
}

win_SQL_GetPlayerRank(client, const String:partial[]="")
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackString(pack, partial);
    ResetPack(pack);
    
    decl String:query[1440];
    
    if (IS_STR_EMPTY(partial))
    {
        if (g_PlayerIDs[client] == -1)
        {
            Timer_Reply(client, "You are not in the database, yet. How'd that happen?");
            return;
        }
        
        SQLTH_D(GetPlayerRank, pack, g_PlayerIDs[client]);
    }
    else
    {
        decl String:esc_name[128];
        SQL_EscapeString(g_Database, partial, STR(esc_name));
        
        SQLTH_H_D(GetPartialPlayerRank, T_GetPlayerRank, pack, WINTOX_GAMETYPE, esc_name);
    }
}

public T_GetPlayerRank(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl Handle:pack;
    decl client;
    decl String:partial[64];
    
    pack = Handle:data;
    client = ReadPackCell(pack);
    ReadPackString(pack, STR(partial));
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error getting player rank for client %d (partial: \"%s\"): %s", client, partial, error);
        if (IS_STR_EMPTY(partial))
            Timer_Reply(client, "There was a database error retrieving your rank. Please try again.");
        else
            Timer_Reply(client, "There was a database error retrieving the player rank for \"%s\". Please try again.", partial);
        return;
    }
    
    if (!SQL_GetRowCount(hndl))
    {
        if (IS_STR_EMPTY(partial))
            Timer_Reply(client, "We couldn't find you in the database! Uh-oh.");
        else
            Timer_Reply(client, "There were no matches for \"%s\"", partial);
        return;
    }
    
    SQL_FetchRow(hndl);
    
    decl String:latest_name[64];
    decl rank;
    decl grosspoints;
    decl maps_completed;
    decl map_records;
#if defined(WINTOX_SURF)
    decl bonuses_completed;
    decl bonus_wr;
#endif
    
    SQL_FetchString(hndl, 0, STR(latest_name));
    rank = SQL_FetchInt(hndl, 1);
    grosspoints = SQL_FetchInt(hndl, 2);
    maps_completed = SQL_FetchInt(hndl, 3);
    map_records = SQL_FetchInt(hndl, 4);
    
#if defined(WINTOX_SURF)
    bonuses_completed = SQL_FetchInt(hndl, 5);
    bonus_wr = SQL_FetchInt(hndl, 6);
#endif
    
#if defined(WINTOX_BHOP)
    Timer_Reply(client, "{G}%s{N}, Rank: {G}%d{N} | Points: {G}%d{N} | MC: {G}%d{N} | MR: {G}%d{N}", \
        latest_name, rank, grosspoints, maps_completed, map_records);

#elseif defined(WINTOX_SURF)
    Timer_Reply(client, "{G}%s{N}, Rank: {G}%d{N} | Points: {G}%d{N} | MC: {G}%d{N} | MR: {G}%d{N} | BC: {G}%d{N} | BR: {G}%d{N}", \
        latest_name, rank, grosspoints, maps_completed, map_records, bonuses_completed, bonus_wr);
#endif
}

win_SQL_GetPlayerCountryRank(client, const String:partial[]="")
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackString(pack, partial);
    ResetPack(pack);
    
    decl String:query[1440];
    
    if (IS_STR_EMPTY(partial))
    {
        if (g_PlayerIDs[client] == -1)
        {
            Timer_Reply(client, "You are not in the database, yet. How'd that happen?");
            return;
        }
        
        SQLTH_D(GetPlayerCountryRank, pack, g_PlayerIDs[client]);
    }
    else
    {
        decl String:esc_name[128];
        SQL_EscapeString(g_Database, partial, STR(esc_name));
        
        SQLTH_H_D(GetPartialPlayerCountryRank, T_GetPlayerCountryRank, pack, WINTOX_GAMETYPE, esc_name);
    }
}

public T_GetPlayerCountryRank(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl Handle:pack;
    decl client;
    decl String:partial[64];
    
    pack = Handle:data;
    client = ReadPackCell(pack);
    ReadPackString(pack, STR(partial));
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error getting country rank for client %d (partial: \"%s\"): %s", client, partial, error);
        if (IS_STR_EMPTY(partial))
            Timer_Reply(client, "There was a database error retrieving your country rank. Please try again.");
        else
            Timer_Reply(client, "There was a database error retrieving the country rank for \"%s\". Please try again.", partial);
        return;
    }
    
    if (!SQL_GetRowCount(hndl))
    {
        if (IS_STR_EMPTY(partial))
            Timer_Reply(client, "We couldn't find you in the database! Uh-oh.");
        else
            Timer_Reply(client, "There were no matches for \"%s\"", partial);
        return;
    }
    
    decl String:latest_name[64];
    decl rank;
    decl grosspoints;
    decl maps_completed;
    decl map_records;
    decl String:country[4];
#if defined(WINTOX_SURF)
    decl bonuses_completed;
    decl bonus_wr;
#endif
    
    SQL_FetchRow(hndl);
    
    SQL_FetchString(hndl, 0, STR(latest_name));
    rank = SQL_FetchInt(hndl, 1);
    grosspoints = SQL_FetchInt(hndl, 2);
    maps_completed = SQL_FetchInt(hndl, 3);
    map_records = SQL_FetchInt(hndl, 4);
    SQL_FetchString(hndl, 5, STR(country));
    
#if defined(WINTOX_SURF)
    bonuses_completed = SQL_FetchInt(hndl, 6);
    bonus_wr = SQL_FetchInt(hndl, 7);
#endif
    
    decl String:sz_country[16];
    sz_country[0] = '\0';
    if (!IS_STR_EMPTY(country))
        Format(STR(sz_country), " ({OG}%s{N})", country);
    
#if defined(WINTOX_BHOP)
    Timer_Reply(client, "{G}%s{N}, Country%s Rank: {G}%d{N} | Points: {G}%d{N} | MC: {G}%d{N} | MR: {G}%d{N}", \
        latest_name, sz_country, rank, grosspoints, maps_completed, map_records);
    
#elseif defined(WINTOX_SURF)
    Timer_Reply(client, "{G}%s{N}, Country%s Rank: {G}%d{N} | Points: {G}%d{N} | MC: {G}%d{N} | MR: {G}%d{N} | BC: {G}%d{N} | BR: {G}%d{N}", \
        latest_name, sz_country, rank, grosspoints, maps_completed, map_records, bonuses_completed, bonus_wr);
#endif
}


/* ========================================================================
                          TIMER UTILITY FUNCTIONS
   ======================================================================== */

stock GetPlayerCurrentTrack(client, if_invalid=INVALID_TRACK)
{
    new track = g_PlayerCurrentTrack[client];
    if (track == INVALID_TRACK)
        return if_invalid;
    
    return track;
}

stock GetPlayerCurrentPathIndex(client, if_invalid=INVALID_PATH)
{
    new path = g_PlayerCurrentPath[client];
    if (path == INVALID_PATH)
        return if_invalid;
    
    return path;
}

/**
 * Returns the zone index in the player's current path, at offset indices away
 *
 * @param   client      The client index of the player
 * @param   offset      The number of indices to jump in the player's path
 * @param   if_invalid  The value to return if the zone is invalid
 * @return  A zone index if there is a valid zone at offset places away in the
 *              player's current path; otherwise, returns `if_invalid`.
 */
stock GetPlayerOffsetZone(client, offset, if_invalid=INVALID_ZONE, &save_path)
{
    new track = GetPlayerCurrentTrack(client);
    if (track == INVALID_TRACK || track < 0 || track >= MAX_TRACKS)
        return if_invalid;
    
    new path = GetPlayerCurrentPathIndex(client);
    if (path == INVALID_PATH)
    {
        save_path = 0;
        return 0;
    }
    
    new offset_path = path + offset;
    if (offset_path < 0 || offset_path >= MAX_PATH_LENGTH)
        return if_invalid;
    
    new zone = g_TrackPaths[track][offset_path];
    if (zone == INVALID_ZONE)
        return if_invalid;
    
    save_path = offset_path;
    
    return zone;
}

stock GetPlayerCurrentZone(client, if_invalid=INVALID_ZONE)
{
    decl save_path;
    return GetPlayerOffsetZone(client, 0, if_invalid, save_path);
}

stock GetPlayerNextZone(client, if_invalid=INVALID_ZONE)
{
    decl save_path;
    return GetPlayerOffsetZone(client, 1, if_invalid, save_path);
}

stock GetPlayerLastZone(client, if_invalid=INVALID_ZONE)
{
    decl save_path;
    return GetPlayerOffsetZone(client, -1, if_invalid, save_path);
}

stock GetPlayerLastZone_Path(client, if_invalid=INVALID_ZONE, &save_path)
{
    return GetPlayerOffsetZone(client, -1, if_invalid, save_path);
}

stock bool:IsZoneEndZone(track, zone)
{
    if (track < 0 || track >= MAX_TRACKS)
        return false;
    
    if (zone < 0 || zone >= MAX_ZONES)
        return false;
    
    return g_EndZone[track] == zone;
}

stock StartTimer(client)
{
    g_StartTimes[client][0] = GetEngineTime();
    g_PlayerCurrentPath[client] = 0;
}
