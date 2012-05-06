/**
 * Bunny-hop plug-in.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#pragma semicolon 1

#define WINTOX_BHOP
#define WINTOX_BHOP_VERSION "0.1dev"

#define WINTOX_GAMETYPE "bhop"

#include <sourcemod>
#include "./wintox/version_build"
#include "./wintox/wintox"

public Plugin:myinfo = 
{
#if defined(WINTOX_DEBUG)
    name = "[Wintox] Bhop DEBUG",
#else
    name = "[Wintox] Bhop",
#endif
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
