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
#include "./sql"
#include "./stocks"
#include "./zones"

public OnMapStart()
{
    // Grab the map name and database that shit
    decl String:mapname[256];
    GetCurrentMap(STR(mapname));
    String_ToLower(mapname, STR(g_CurMapName));
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
    new bool:curmap = ReadPackCell(Handle:data);
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
        
        SQL_FetchString(hndl, 1, STR(human_name));
        tier = SQL_FetchInt(hndl, 2);
        is_staged = bool:SQL_FetchInt(hndl, 3);
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
