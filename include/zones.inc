/**
 * Zones and stages and map-related functions
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_zones_included
 #endinput
#endif
#define _wintox_zones_included

#include "./include/commands"

#define ZONE_NEXT_NEW_ZONE -1

// Current map info
new String:g_CurMapName[256];
new String:g_CurMapHumanName[256];
new bool:g_CurMapStaged;
new g_CurMapTier = 1;

// Create a static array cache for zones
// Capacity: 32 zones, 31* points each
// * The first point is used to store information about the zone, including
//   how many points are in the zone.
#define MAX_ZONES 32
new Float:g_ZonesCache[MAX_ZONES][32][3];
new g_ZonesCount = 0;
new g_ClientEditZone[MAXPLAYERS] = { -1, ... };

new Handle:g_ZoneMenu = INVALID_HANDLE;

///////////////////////////////////////////////////////
// These may be needed later (heh), and I hate warnings
///////////////////////////////////////////////////////
#pragma unused g_CurMapStaged
#pragma unused g_CurMapTier
#pragma unused g_ZonesCache
#pragma unused g_ZonesCount

Zones_Init()
{
    RegisterCommand("sm_zonemenu", ConCommand_ZoneMenu, "sm_zonemenu | Bring up the zone menu.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_newzone", ConCommand_NewZone, "sm_newzone [<zone index>] | Create a new zone.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_newzonepoint", ConCommand_NewZonePoint, "sm_newzonepoint | Create a new point in a zone.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_zoneheight", ConCommand_ZoneHeight, "sm_zoneheight <height> | Sets the height of the zone in map units.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_finishzone", ConCommand_FinishZone, "sm_finishzone | Finalize the zone and its points.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_removezone", ConCommand_RemoveZone, "sm_removezone <zone index> | Remove the zone with the specified zone ID.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_clearallzones", ConCommand_ClearAllZones, "sm_clearallzones | Clear all zones for the current map.",  0, ADMFLAG_GENERIC);
    
    BuildZoneMenu();
}



/* ========================================================================
                      ZONE MANIPULATION CONCOMMANDS
   ======================================================================== */

public Action:ConCommand_ZoneMenu(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    DisplayZoneMenu(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_NewZone(client, args)
{
    if (args > 1)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    new zone_id = ZONE_NEXT_NEW_ZONE;
    
    if (args == 1)
    {
        decl String:sz_zone_id[32];
        GetCmdArg(1, STR(sz_zone_id));
        
        if (StringToIntEx(sz_zone_id, zone_id) == 0)
        {
            ReplyWithDescription(client);
            return Plugin_Handled;
        }
    }
    
    Zone_BeginNewZone(client, zone_id);
    
    return Plugin_Handled;
}

public Action:ConCommand_NewZonePoint(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_NewZonePoint(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_ZoneHeight(client, args)
{
    if (args != 1)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    decl height;
    decl String:sz_height[32];
    GetCmdArg(1, STR(sz_height));
    
    if (StringToIntEx(sz_height, height) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_ZoneHeight(client, height);
    
    return Plugin_Handled;
}

public Action:ConCommand_FinishZone(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_FinishZone(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_RemoveZone(client, args)
{
    if (args != 1)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    decl zone_id;
    decl String:sz_zone_id[32];
    GetCmdArg(1, STR(sz_zone_id));
    
    if (StringToIntEx(sz_zone_id, zone_id) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_RemoveZone(client, zone_id);
    
    return Plugin_Handled;
}

public Action:ConCommand_ClearAllZones(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_ClearAllZones(client);
    
    return Plugin_Handled;
}



/* ========================================================================
                          ZONE MANIPULATION MENU
   ======================================================================== */

#define ZMENU_INFO_NEW_ZONE "0"
#define ZMENU_INFO_REMOVE_ZONE "1"
#define ZMENU_INFO_CLEAR_ALL_ZONES "2"

BuildZoneMenu()
{
    g_ZoneMenu = CreateMenu(MenuHandler_RootZoneMenu);
    
    SetMenuTitle(g_ZoneMenu, "Zones Menu (cur %d zones)", g_ZonesCount);
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_NEW_ZONE, "New zone");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_REMOVE_ZONE, "Remove zone");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_CLEAR_ALL_ZONES, "Clear ALL zones");
}

DisplayZoneMenu(client)
{
    DisplaySubmenu(g_ZoneMenu, client, MENU_TIME_FOREVER, false);
}

public MenuHandler_RootZoneMenu(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
        if (StrEqual(info, ZMENU_INFO_NEW_ZONE))
        {
            // We need to find out what zone index the user wants this zone to be
            // Let's make a menu! YEAH!
            new Handle:zone_id_submenu = CreateMenu(MenuHandler_ZoneIndex);
            
            SetMenuExitButton(zone_id_submenu, true);
            SetMenuExitBackButton(zone_id_submenu, true);
            SetMenuTitle(zone_id_submenu, "Select a zone index");
            
            AddMenuItem(zone_id_submenu, "0", "0 (start zone)");
            for (new i=1; i<MAX_ZONES; i++)
            {
                decl String:buffer[16];
                
                IntToString(i, STR(buffer));
                AddMenuItem(zone_id_submenu, buffer, buffer);
            }
            
            DisplaySubmenu(zone_id_submenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
        }
    }
    else if (action == MenuAction_Cancel)
        PrevSubmenu(param1);
}

#define ZMENU_EDIT_NEW_POINT "new"
#define ZMENU_EDIT_REMOVE_LAST "removelast"
#define ZMENU_EDIT_CLEAR_ALL "clear"

public MenuHandler_ZoneIndex(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:sz_zone_id[16];
        GetMenuItem(menu, param2, STR(sz_zone_id));
        new zone_id = StringToInt(sz_zone_id);
        
        new Handle:zone_menu = CreateMenu(MenuHandler_EditZone);
        SetMenuExitButton(zone_menu, true);
        SetMenuExitBackButton(zone_menu, true);
        
        SetMenuTitle(zone_menu, "Editing zone %d", zone_id);
        AddMenuItem(zone_menu, ZMENU_EDIT_NEW_POINT, "New point");
        AddMenuItem(zone_menu, ZMENU_EDIT_REMOVE_LAST, "Remove last point");
        AddMenuItem(zone_menu, ZMENU_EDIT_CLEAR_ALL, "Clear all points");
        
        // Pop the zone index submenu, but don't redisplay the root zone menu
        PrevSubmenu(param1, false);
        DisplaySubmenu(zone_menu, param1, MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public MenuHandler_EditZone(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}


/* ========================================================================
                        ZONE MANIPULATION FUNCTIONS
   ======================================================================== */

Zone_BeginNewZone(client, zone_id=ZONE_NEXT_NEW_ZONE)
{
    PrintToServer("[wtx-dbg] CLIENT %d BEGINNING ZONE %d", client, zone_id);/////////////////
}

Zone_NewZonePoint(client)
{
    PrintToServer("[wtx-dbg] CLIENT %d CREATING ZONE POINT", client);/////////////////
}

Zone_ZoneHeight(client, height)
{
    PrintToServer("[wtx-dbg] CLIENT %d SET ZONE HEIGHT AT %d", client, height);/////////////////
}

Zone_FinishZone(client)
{
    PrintToServer("[wtx-dbg] CLIENT %d FINISHING ZONE", client);/////////////////
}

Zone_RemoveZone(client, zone_id)
{
    PrintToServer("[wtx-dbg] CLIENT %d REMOVING ZONE %d", client, zone_id);/////////////////
}

Zone_ClearAllZones(client)
{
    PrintToServer("[wtx-dbg] CLIENT %d CLEARING ALL ZONES", client);/////////////////
}
