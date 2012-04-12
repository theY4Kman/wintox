/**
 * Game events handling
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

new Handle:g_fwdMapLoaded = INVALID_HANDLE;
new Handle:g_fwdPointsSyncFinished = INVALID_HANDLE;
new Handle:g_fwdPlayerReachedEnd = INVALID_HANDLE;
new Handle:g_fwdPlayerReachedZone = INVALID_HANDLE;

#include "./include/sql"
#include "./include/stocks"
#include "./include/zones"
#include "./include/timer"

Events_Init()
{
    // public(map_id, const String:mapname[], const String:human_name[], tier, bool:is_staged)
    g_fwdMapLoaded = CreateForward(ET_Ignore, Param_Cell, Param_String, Param_String, Param_Cell, Param_Cell);
    
    // public(zone_id, points)
    g_fwdPointsSyncFinished = CreateForward(ET_Ignore, Param_Cell, Param_Cell);
    
    // public(client, Float:elapsed)
    g_fwdPlayerReachedEnd = CreateForward(ET_Ignore, Param_Cell, Param_Float);
    
    // public(client, zone_id, Float:elapsed)
    g_fwdPlayerReachedZone = CreateForward(ET_Ignore, Param_Cell, Param_Cell, Param_Float);
    
#if defined(WINTOX_BHOP)
    HookEvent("player_jump", Event_PlayerJump);
#endif
    HookEvent("player_team", Event_PlayerTeam);
    HookEvent("player_spawn", Event_PlayerSpawn);
    HookEvent("player_death", Event_PlayerDeath);
}

public OnMapStart()
{
    // Grab the map name and database that shit
    decl String:mapname[256];
    GetCurrentMap(STR(mapname));
    String_ToLower(mapname, STR(g_CurMapName));
}

public OnClientAuthorized(client, const String:auth[])
{
    InsertOrUpdatePlayerInfo(client, auth);
}

public OnClientDisconnect(client)
{
    g_IsTiming[client] = false;
}

public OnClientDisconnected(client)
{
    new editing = g_ClientEditZone[client];
    if (editing != INVALID_ZONE)
    {
        ZONE_EDITING[editing] = false;
        g_ClientEditZone[client] = INVALID_ZONE;
    }
    
    for (new i=0; i<MAX_ZONES; i++)
    {
        g_StartTimes[client][i] = 0.0;
        g_ZoneRecords[client][i] = 0.0;
    }
    
    g_TimerEnabled[client] = true;
    g_SideStageInfoEnabled[client] = true;
    g_PointsToChatEnabled[client] = true;
    g_PlayerCurrentZone[client] = 0;
    g_Jumps[client] = 0;
}

#if defined(WINTOX_BHOP)
public Action:Event_PlayerJump(Handle:event, String:name[], bool:dontBroadcast)
{
    new client = GetClientOfUserId(GetEventInt(event, "userid"));
    
    if (client < 0 || client > MaxClients)
        return;
    
    // How the FUCK did they just jump? Aliens...
    if (!g_IsTiming[client])
        return;
    
    // Don't record jumps if the timer hasn't started
    if (!TIMER_STARTED(client))
        return;
    
    g_Jumps[client]++;
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
    new client = GetClientOfUserId(GetEventInt(event, "userid"));
    g_IsTiming[client] = false;
}

// Tries to retrieve the current map info 
GetOrInsertMap(const String:mapname[], bool:curmap=true)
{
    decl String:query[512];
    decl String:buffer[256];
    SQL_EscapeString(g_Database, mapname, STR(buffer));
    Format(STR(query), SQL_QUERY[GetMap], buffer);
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, _:curmap);
    WritePackString(pack, mapname);
    ResetPack(pack);
    
    SQLT(query, T_GetOrInsertMapInfo, pack);
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
        Format(STR(buffer), SQL_QUERY[InsertMap], mapname, human_name, tier, is_staged);
        
        if (!SQLF(buffer))
        {
            SQL_GetError(g_Database, STR(buffer));
            if (curmap)
                SetFailState("Error inserting map into database: %s", buffer);
            else
                LogError("Error inserting map into database: %s", buffer);
        }
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

// Inserts or updates a player's info
InsertOrUpdatePlayerInfo(client, const String:steamid[])
{
    decl String:query[256];
    Format(STR(query), SQL_QUERY[GetPlayer], steamid);
    
    SQLT(query, T_GetPlayerInfo, client);
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

public T_GetPlayerInfo(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl String:buffer[512];
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
        SQL_FetchString(hndl, 1, STR(db_latest_name));
        SQL_FetchString(hndl, 2, STR(db_latest_ip));
        SQL_FetchString(hndl, 4, STR(db_country));
        
        // Check if we need to update the player's name, country, and/or IP
        if (!StrEqual(latest_name, db_latest_name) || !StrEqual(latest_ip, db_latest_ip) ||
            !StrEqual(country, db_country))
        {
            SQL_EscapeString(g_Database, latest_name, STR(db_latest_name));
            Format(STR(buffer), SQL_QUERY[UpdatePlayer], db_latest_name, latest_ip, country, steamid);
            
            if (!SQLF(buffer))
            {
                SQL_GetError(g_Database, STR(buffer));
                LogError("Error updating player info: %s", buffer);
                
                return;
            }
        }
    }
    
    // Insert all the player's info
    else
    {
        decl String:escaped_name[512];
        
        GeoipCode3(latest_ip, country);
        SQL_EscapeString(g_Database, latest_name, STR(escaped_name));
        Format(STR(buffer), SQL_QUERY[InsertPlayer], escaped_name, latest_ip, steamid, country);
        
        if (!SQLF(buffer))
        {
            SQL_GetError(g_Database, STR(buffer));
            LogError("Error inserting player info: %s", buffer);
            WarnPlayerNotRecording(client, false);
            
            return;
        }
    }
}
