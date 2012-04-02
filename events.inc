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
    String_ToLower(mapname, STR(mapname));
    
    GetOrInsertMap(mapname);
}

// Tries to retrieve the current map info 
GetOrInsertMap(const String:mapname[])
{
}
