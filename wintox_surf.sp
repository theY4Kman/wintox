/**
 * Wintox surf plug-in.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#pragma semicolon 1

#define WINTOX_SURF
#define WINTOX_SURF_VERSION "0.1dev"

#define WINTOX_GAMETYPE "surf"

#include <sourcemod>
#include "./wintox/version_build"
#include "./wintox/wintox"

public Plugin:myinfo = 
{
#if defined(WINTOX_DEBUG)
    name = "[Wintox] Surf DEBUG",
#else
    name = "[Wintox] Surf",
#endif
    author = "Zach \"theY4Kman\" Kanzler",
    description = "Surf timer and functionality.",
    version = WINTOX_VERSION,
    url = "http://intoxgaming.com/"
};

public OnPluginStart()
{
    Wintox_Init();
    
    ServerCommand("exec sourcemod/wintox_surf.cfg");
}

public OnPluginEnd()
{
    Wintox_Exit();
}
