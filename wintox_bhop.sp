/**
 * Bunny-hop plug-in.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#pragma semicolon 1

#define WINTOX_BHOP
#define WINTOX_BHOP_VERSION "0.1dev"

#include <sourcemod>
#include "./include/version"
#include "./include/wintox"

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
    Wintox_Init();
    
    ServerCommand("exec sourcemod/wintox_bhop.cfg");
}

public OnPluginEnd()
{
    Wintox_Exit();
}
