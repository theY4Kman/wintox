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
    
    AutoExecConfig(true, "wintox");
}

public OnConfigsExecuted()
{
    // Initialize database connections and the such
    win_SQL_Connect();
    win_SQL_CreateTables();
    
    // Cache map info from database
    GetOrInsertMap(g_CurMapName);
}
