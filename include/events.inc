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
#include "./include/sql"
#include "./include/stocks"
#include "./include/zones"

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

// Tries to retrieve the current map info 
GetOrInsertMap(const String:mapname[], bool:curmap=true)
{
    decl String:query[512];
    decl String:buffer[256];
    SQL_EscapeString(g_Database, mapname, STR(buffer));
    Format(STR(query), SQL_QUERY(GetMap), buffer);
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, _:curmap);
    WritePackString(pack, mapname);
    ResetPack(pack);
    
    SQL_TQuery(g_Database, T_GetOrInsertMapInfo, query, pack);
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
    
    if (SQL_GetRowCount(hndl) >= 1)
    {
        // row (bsp_name, human_name, tier, is_staged)
        SQL_FetchRow(hndl);
        
        SQL_FetchString(hndl, 2, STR(human_name));
        tier = SQL_FetchInt(hndl, 3);
        is_staged = bool:SQL_FetchInt(hndl, 4);
    }
    else
    {
        Format(STR(buffer), SQL_QUERY(InsertMap), mapname, human_name, tier, is_staged);
        
        if (!SQL_FastQuery(g_Database, buffer))
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
        strcopy(STR(g_CurMapHumanName), human_name);
        g_CurMapTier = tier;
        g_CurMapStaged = is_staged;
    }
}

// Inserts or updates a player's info
InsertOrUpdatePlayerInfo(client, const String:steamid[])
{
    decl String:query[256];
    Format(STR(query), SQL_QUERY(GetPlayer), steamid);
    
    SQL_TQuery(g_Database, T_GetPlayerInfo, query, client);
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
            Format(STR(buffer), SQL_QUERY(UpdatePlayer), db_latest_name, latest_ip, country, steamid);
            
            if (!SQL_FastQuery(g_Database, buffer))
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
        Format(STR(buffer), SQL_QUERY(InsertPlayer), escaped_name, latest_ip, steamid, country);
        
        if (!SQL_FastQuery(g_Database, buffer))
        {
            SQL_GetError(g_Database, STR(buffer));
            LogError("Error inserting player info: %s", buffer);
            WarnPlayerNotRecording(client, false);
            
            return;
        }
    }
}
