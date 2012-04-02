/**
 * Bunny-hop plug-in.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#pragma semicolon 1

#include <sourcemod>
#include "./commands"
#include "./version"
#include "./sql"

#define WINTOX_BHOP_VERSION "0.1dev"

public Plugin:myinfo = 
{
    name = "[Wintox] Bhop",
    author = "Zach \"theY4Kman\" Kanzler",
    description = "Bunnyhop timer and functionality.",
    version = WINTOX_BHOP_VERSION,
    url = "http://intoxgaming.com/"
};

public OnPluginStart()
{
    Commands_Init();
    win_SQL_Init();
    
    AutoExecConfig(true, "wintox");
}

public OnConfigsExecuted()
{
    win_SQL_Connect();
}

public OnGameFrame()
{
}
