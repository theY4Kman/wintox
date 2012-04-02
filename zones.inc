/**
 * Zones and stages
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_zones_included
 #endinput
#endif
#define _wintox_zones_included

// Current map info
new String:g_CurMapName[256];
new bool:g_CurMapStaged;
new g_CurMapTier = 1;

// Create a static array cache for zones
// Capacity: 32 zones, 32 points each
new Float:g_ZonesCache[32][32][3];
new g_ZonesCount = 0;
