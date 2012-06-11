/**
 * Game events and plug-in-created forwards.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_events_included
 #endinput
#endif
#define _wintox_events_included

#pragma semicolon 1

#include <sourcemod>
#include <geoip>
#include <sdkhooks>
#include <clientprefs>

new Handle:g_fwdMapLoaded = INVALID_HANDLE;
new Handle:g_fwdPointsSyncFinished = INVALID_HANDLE;
new Handle:g_fwdPlayerReachedEnd = INVALID_HANDLE;
new Handle:g_fwdPlayerReachedZone = INVALID_HANDLE;
new Handle:g_fwdTextEntryComplete = INVALID_HANDLE;

#include "./wintox/sql"
#include "./wintox/stocks"
#include "./wintox/zones"
#include "./wintox/timer"

Events_Init()
{
    // Called when a map is loaded and its information is grabbed from the database
    // public(map_id, const String:mapname[], const String:human_name[], tier, bool:is_staged)
    g_fwdMapLoaded = CreateForward(ET_Ignore, Param_Cell, Param_String, Param_String, Param_Cell, Param_Cell);
    
    // Called when all points are synced to the database
    // public(zone_id, track, points)
    g_fwdPointsSyncFinished = CreateForward(ET_Ignore, Param_Cell, Param_Cell, Param_Cell);
    
    // Called when a player reaches the end zone
    // public(client, track, Float:elapsed, style)
    g_fwdPlayerReachedEnd = CreateForward(ET_Ignore, Param_Cell, Param_Cell, Param_Float, Param_Cell);
    
    // Called when a player reaches any new zone
    // public(client, track, zone_id, Float:elapsed, style)
    g_fwdPlayerReachedZone = CreateForward(ET_Ignore, Param_Cell, Param_Cell, Param_Cell, Param_Float, Param_Cell);
    
    // Called when a player has completed their text entry
    // public(client, const String:text[], any:data)
    g_fwdTextEntryComplete = CreateForward(ET_Ignore, Param_Cell, Param_String, Param_Any);
    
#if defined(WINTOX_BHOP)
    HookEvent("player_jump", Event_PlayerJump);
#endif
    HookEvent("player_team", Event_PlayerTeam);
    HookEvent("player_spawn", Event_PlayerSpawn);
    HookEvent("player_death", Event_PlayerDeath);
}

public OnConfigsExecuted()
{
    // Initialize database connections and the such
    win_SQL_Connect();
    win_SQL_CreateTables();
    
    // Cache map info from database
    Timer_GetMapInfo(INVALID_HANDLE);
    
    if (g_wintoxLateLoad)
    {
        // Just for good measure, let's discover the low grav state of all the clients
        for (new client=1; client<MaxClients; client++)
        {
            if (!IsClientConnected(client) || !IsClientInGame(client))
                continue;
            
            OnClientPutInServer(client);
            
            if (!IsClientAuthorized(client))
                continue;
            
            decl String:steamid[64];
            
            GetClientAuthString(client, STR(steamid));
            OnClientAuthorized(client, steamid);
        }
    }
}

public OnMapStart()
{
    // Grab the map name and database that shit
    decl String:mapname[256];
    GetCurrentMap(STR(mapname));
    String_ToLower(mapname, STR(g_CurMapName));
}

public Action:Timer_GetMapInfo(Handle:timer)
{
    if (g_wintoxDBConnected)
        GetOrInsertMap(g_CurMapName);
    else
        CreateTimer(0.1, Timer_GetMapInfo);
    
    return Plugin_Continue;
}

public APLRes:AskPluginLoad2(Handle:myself, bool:late, String:error[], err_max)
{
    g_wintoxLateLoad = late;
    
    return APLRes_Success;
}

public OnClientAuthorized(client, const String:auth[])
{
    InsertOrUpdatePlayerInfo(client, auth);
}

public OnClientPutInServer(client)
{
    g_HideOtherPlayers[client] = false;
    if (!SDKHookEx(client, SDKHook_SetTransmit, Hook_SetTransmit))
        LogError("Could not hook SetTransmit on %L", client);
    if (!SDKHookEx(client, SDKHook_WeaponDropPost, Hook_WeaponDropPost))
        LogError("Could not hook WeaponDropPost on %L", client);
    
    QueryClientConVar(client, "fps_max", QueryConVar_FpsMax);
}

public OnClientDisconnect(client)
{
    g_IsTiming[client] = false;
    g_HideOtherPlayers[client] = false;
    
    if (IsRacing(client))
        StopRace(g_RaceOpponents[client], client, "Your opponent has left the server!");
    
    win_SQL_UpdatePlayerTimestamp(client);
    ResetPlayerVariables(client);
}


public QueryConVar_FpsMax(QueryCookie:cookie, client, ConVarQueryResult:result, const String:cvarName[], const String:cvarValue[])
{
    g_ClientFpsMax[client] = StringToInt(cvarValue);
}

public OnClientCookiesCached(client)
{
#define GET_COOKIE(%1,%2) { \
        new Handle:cookie = FindClientCookie((%1)); \
        if (cookie != INVALID_HANDLE) \
        { \
            decl String:sz_cookie[8]; \
            GetClientCookie(client, cookie, STR(sz_cookie)); \
            %2 = bool:StringToInt(sz_cookie); \
        } \
    }

    GET_COOKIE(COOKIE_TIMER_ENABLED,g_TimerEnabled[client])
    GET_COOKIE(COOKIE_SIDE_STAGE_INFO_ENABLED,g_SideStageInfoEnabled[client])
    GET_COOKIE(COOKIE_POINTS_TO_CHAT_ENABLED,g_PointsToChatEnabled[client])

#undef GET_COOKIE
}


#if defined(WINTOX_BHOP)
public Action:Event_PlayerJump(Handle:event, String:name[], bool:dontBroadcast)
{
    new client = GetClientOfUserId(GetEventInt(event, "userid"));
    
    if (client < 0 || client > MaxClients)
        return;
    
    // Don't record jumps if the timer hasn't started
    if ((!g_IsTiming[client] || !TIMER_STARTED(client)) &&
        (!IsRacing(client) || !IsRaceStarted(client)))
        return;
    
    g_Jumps[client]++;
}

public Action:OnPlayerRunCmd(client, &buttons, &impulse, Float:vel[3], Float:angles[3], &weapon)
{
    if ((!IS_PAUSED(client) && TIMER_STARTED(client) && !IS_FINISHED(client)) || (IsRacing(client) && IsRaceStarted(client)))
    {
        if (g_PlayerCurrentStyle[client] >= STYLE_SIDEWAYS && (buttons & IN_MOVELEFT || buttons & IN_MOVERIGHT))
            g_PlayerCurrentStyle[client] = STYLE_NORMAL;
        else if (g_PlayerCurrentStyle[client] >= STYLE_WONLY && (buttons & IN_BACK))
            g_PlayerCurrentStyle[client] = STYLE_SIDEWAYS;
    }
    
    return Plugin_Continue;
}
#endif // defined(WINTOX_BHOP)

public Action:Event_PlayerTeam(Handle:event, String:name[], bool:dontBroadcast)
{
    new client = GetClientOfUserId(GetEventInt(event, "userid"));
    new team = GetEventInt(event, "team");
    
    if (!IS_PLAYING_TEAM(team))
        ResetTimer(client);
}

public Action:Event_PlayerSpawn(Handle:event, String:name[], bool:dontBroadcast)
{
    new client = GetClientOfUserId(GetEventInt(event, "userid"));
    g_IsTiming[client] = true;
}

public Action:Event_PlayerDeath(Handle:event, String:name[], bool:dontBroadcast)
{
    new userid = GetEventInt(event, "userid");
    new attacker = GetEventInt(event, "attacker");
    new client = GetClientOfUserId(userid);
    g_IsTiming[client] = false;
    
    // Reset the player's timer to prevent map exploits, as per request of cLutch
    ResetTimer(client);
    
    if (attacker == userid || attacker == 0)
    {
        decl Handle:pack;
        CreateDataTimer(0.1, Timer_Callback_DeathFragRollback, pack);
        WritePackCell(pack, client);
        WritePackCell(pack, GetClientTeam(client));
        ResetPack(pack);
    }
}

public Action:Timer_Callback_DeathFragRollback(Handle:timer, Handle:pack)
{
    new client = ReadPackCell(pack);
    new old_team = ReadPackCell(pack);
    
    // This is to counteract losing a point when you kill yourself
    if (old_team == GetClientTeam(client))
        Client_SetScore(client, Client_GetScore(client) + 1);
}

win_SQL_UpdatePlayerTimestamp(client)
{
    if (g_PlayerIDs[client] == -1)
    {
#if defined(WINTOX_DEBUG)
        LogError("Tried to update `last_join` for invalid player %L", client);
#endif
        return;
    }
    
    SQLTH(UpdatePlayerTimestamp, g_PlayerIDs[client], g_PlayerIDs[client]);
}
SQLT_HANDLER(UpdatePlayerTimestamp, "Error updating `last_join` for player ID %d: %s", data)

// Tries to retrieve the current map info 
GetOrInsertMap(const String:mapname[], bool:curmap=true)
{
    decl String:esc_mapname[256];
    SQL_EscapeString(g_Database, mapname, STR(esc_mapname));
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, _:curmap);
    WritePackString(pack, mapname);
    ResetPack(pack);
    
    SQLTH_H(GetMap, T_GetOrInsertMapInfo, pack, esc_mapname, WINTOX_GAMETYPE);
}

public T_GetOrInsertMapInfo(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl String:buffer[512];
    decl String:mapname[256];
    
    new bool:curmap = bool:ReadPackCell(Handle:data);
    ReadPackString(Handle:data, STR(mapname));
    CloseHandle(Handle:data);
    
    if (hndl == INVALID_HANDLE)
    {
        SQL_GetError(owner, STR(buffer));
        if (curmap)
            SetFailState("Error retrieving map info: %s", buffer);
        else
            LogError("Error retrieving map info: %s", buffer);
    }
    
    decl String:human_name[256];
    human_name[0] = '\0';
    new tier = 1;
    new bool:is_staged = false;
    new map_id = -1;
    
    if (SQL_GetRowCount(hndl) >= 1)
    {
        // row (bsp_name, human_name, tier, is_staged)
        SQL_FetchRow(hndl);
        
        map_id = SQL_FetchInt(hndl, 0);
        SQL_FetchString(hndl, 2, STR(human_name));
        tier = SQL_FetchInt(hndl, 3);
        is_staged = bool:SQL_FetchInt(hndl, 4);
    }
    else
    {
        Format(STR(buffer), SQL_QUERY[InsertMap], mapname, human_name, tier, is_staged, WINTOX_GAMETYPE);
        
        if (!SQLF(buffer))
        {
            SQL_GetError(g_Database, STR(buffer));
            if (curmap)
                SetFailState("Error inserting map into database: %s", buffer);
            else
            {
                LogError("Error inserting map into database: %s", buffer);
                return;
            }
        }
        
        map_id = SQL_GetInsertId(owner);
    }
    
    if (curmap)
    {
        g_CurMapID = map_id;
        strcopy(STR(g_CurMapHumanName), human_name);
        g_CurMapTier = tier;
        g_CurMapStaged = is_staged;
        
        // Fire the wintox_map_loaded forward
        Call_StartForward(g_fwdMapLoaded);
        Call_PushCell(g_CurMapID);
        Call_PushString(g_CurMapName);
        Call_PushString(g_CurMapHumanName);
        Call_PushCell(g_CurMapTier);
        Call_PushCell(g_CurMapStaged);
        Call_Finish();
    }
}

ResetPlayerVariables(client)
{
    Zone_DeinitPlayer(client);
    Wintox_DeinitPlayer(client);
    Timer_DeinitPlayer(client);
    Utils_DeinitPlayer(client);
    Teleport_DeinitPlayer(client);
    Race_DeinitPlayer(client);
}

WarnPlayerNotRecording(client, bool:retrieve=true)
{
    if (retrieve)
        PrintToChat(client, "There was an error trying to retrieve your info from the database.");
    else
        PrintToChat(client, "There was an error trying to insert your info into the database.");
    
    PrintToChat(client, "Your score will not be recorded. Please try rejoining.");
    PrintToChat(client, "Contact an administrator if this continues. Thanks!");
}

// Inserts or updates a player's info
InsertOrUpdatePlayerInfo(client, const String:steamid[])
{
    SQLTH(GetPlayer, client, steamid, WINTOX_GAMETYPE);
}

public T_GetPlayer(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl String:query[512];
    decl String:latest_name[256];
    decl String:latest_ip[48];
    decl String:steamid[64];
    decl String:country[4];
    
    new client = _:data;
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving player info: %s", error);
        WarnPlayerNotRecording(client);
        
        return;
    }
    
    GetClientAuthString(client, STR(steamid));
    GetClientName(client, STR(latest_name));
    GetClientIP(client, STR(latest_ip));
    GeoipCode3(latest_ip, country);
    
    if (SQL_GetRowCount(hndl) >= 1)
    {
        decl String:db_latest_name[256];
        decl String:db_latest_ip[48];
        decl String:db_country[4];
        
        SQL_FetchRow(hndl);
        
        g_PlayerIDs[client] = SQL_FetchInt(hndl, 0);
        SQL_FetchString(hndl, 1, STR(db_latest_name));
        SQL_FetchString(hndl, 2, STR(db_latest_ip));
        SQL_FetchString(hndl, 4, STR(db_country));
        
        // Check if we need to update the player's name, country, and/or IP
        if (!StrEqual(latest_name, db_latest_name) || !StrEqual(latest_ip, db_latest_ip) ||
            !StrEqual(country, db_country))
        {
            SQL_EscapeString(g_Database, latest_name, STR(db_latest_name));
            
            SQLTH_D(UpdatePlayer, client, db_latest_name, latest_ip, country, steamid, WINTOX_GAMETYPE);
        }
        else
            win_SQL_UpdatePlayerTimestamp(client);
        
        CacheZoneRecords(client);
        CacheTrackRecords(client);
    }
    
    // Insert all the player's info
    else
    {
        decl String:escaped_name[512];
        
        GeoipCode3(latest_ip, country);
        SQL_EscapeString(g_Database, latest_name, STR(escaped_name));
        
        SQLTH_D(InsertPlayer, client, escaped_name, latest_ip, steamid, country, WINTOX_GAMETYPE);
    }
}
SQLT_HANDLER(UpdatePlayer,"Error updating player (%L) info: %s", data)

public T_InsertPlayer(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new client = data;
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error inserting player (%L) info: %s", client, error);
        WarnPlayerNotRecording(client, false);
        return;
    }
    
    g_PlayerIDs[client] = SQL_GetInsertId(hndl);
    g_PlayerPoints[client] = 0;
    
    CacheZoneRecords(client);
    CacheTrackRecords(client);
}
