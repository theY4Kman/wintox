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

#define ZonePoints Float
#define ZONE_POINTS[%1] g_ZonesCache[(%1)][0][0]

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
    RegisterCommand("sm_newzonepoint", ConCommand_NewZonePoint, "sm_newzonepoint | Create a new point in the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_zoneheight", ConCommand_ZoneHeight, "sm_zoneheight <height> | Sets the height of the zone currently being edited in map units.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_removelastpoint", ConCommand_RemoveLastPoint, "sm_removelastpoint | Remove the last point of the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_clearallpoints", ConCommand_ClearAllPoints, "sm_clearallpoints | Clear all the points of the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_finishzone", ConCommand_FinishZone, "sm_finishzone | Finalize the zone and its points and sync with the database.",  0, ADMFLAG_GENERIC);
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

public Action:ConCommand_RemoveLastPoint(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_RemoveLastPoint(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_ClearAllPoints(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_ClearAllPoints(client);
    
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
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_NEW_ZONE, "Create or edit zone");
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
            
            AddMenuItem(zone_id_submenu, "-1", "Next new zone");
            AddMenuItem(zone_id_submenu, "0", "0 (start zone)");
            for (new i=1; i<MAX_ZONES; i++)
            {
                decl String:buffer[32];
                
                Format(STR(buffer), "%d (%s)", i, _:ZONE_POINTS[i] ? "edit" : "create");
                AddMenuItem(zone_id_submenu, buffer, buffer);
            }
            
            DisplaySubmenu(zone_id_submenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
        }
        else if (StrEqual(info, ZMENU_INFO_REMOVE_ZONE))
        {
            new Handle:zone_id_submenu = CreateMenu(MenuHandler_RemoveZoneIndex);
            
            SetMenuExitButton(zone_id_submenu, true);
            SetMenuExitBackButton(zone_id_submenu, true);
            SetMenuTitle(zone_id_submenu, "Select a zone index to remove");
            
            for (new i=0; i<MAX_ZONES; i++)
            {
                decl String:buffer[32];
                
                if (_:ZONE_POINTS[i] == 0)
                    continue;
                
                IntToString(i, STR(buffer));
                AddMenuItem(zone_id_submenu, buffer, buffer);
            }
            
            DisplaySubmenu(zone_id_submenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
        }
        else if (StrEqual(info, ZMENU_INFO_CLEAR_ALL_ZONES))
        {
            new Handle:confirm_menu = CreateMenu(MenuHandler_ConfirmClearZones);
            SetMenuExitButton(confirm_menu, true);
            SetMenuExitBackButton(confirm_menu, true);
            SetMenuTitle(confirm_menu, "Are you sure you want to delete ALL zones?");
            
            AddMenuItem(confirm_menu, "no", "No");
            AddMenuItem(confirm_menu, "kinda yes", "Yes");
            
            DisplaySubmenu(confirm_menu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
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
        
        if (zone_id == ZONE_NEXT_NEW_ZONE)
        {
            zone_id = g_ZonesCount;
            g_ZonesCount++;
        }
        
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

public MenuHandler_RemoveZoneIndex(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:sz_zone_id[16];
        GetMenuItem(menu, param2, STR(sz_zone_id));
        new zone_id = StringToInt(sz_zone_id);
        
        Zone_RemoveZone(param1, zone_id);
        PrevSubmenu(param1);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public MenuHandler_ConfirmClearZones(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:answer[16];
        GetMenuItem(menu, param2, STR(answer));
        
        if (StrEqual(answer, "no"))
            PrevSubmenu(param1);
        else if (StrEqual(answer, "kinda yes"))
        {
            new Handle:confirm_menu = CreateMenu(MenuHandler_ConfirmClearZones);
            SetMenuExitButton(confirm_menu, true);
            SetMenuExitBackButton(confirm_menu, true);
            SetMenuTitle(confirm_menu, "Are you REALLY sure? There's no going back!");
            
            AddMenuItem(confirm_menu, "no", "No");
            AddMenuItem(confirm_menu, "yes", "Yes, dammit");
            
            PrevSubmenu(param1, false);
            DisplaySubmenu(confirm_menu, param1, MENU_TIME_FOREVER);
        }
        else if (StrEqual(answer, "yes"))
        {
            Zone_ClearAllZones(param1);
            PrevSubmenu(param1);
        }
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public MenuHandler_EditZone(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[32];
        GetMenuItem(menu, param2, STR(info));
        
        if (StrEqual(info, ZMENU_EDIT_NEW_POINT))
            Zone_NewZonePoint(param1);
        else if (StrEqual(info, ZMENU_EDIT_REMOVE_LAST))
            Zone_RemoveLastPoint(param1);
        else if (StrEqual(info, ZMENU_EDIT_CLEAR_ALL))
            Zone_ClearAllPoints(param1);
        
        DisplayMenuAtItem(menu, param1, GetMenuSelectionPosition(), MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}


/* ========================================================================
                        ZONE MANIPULATION FUNCTIONS
   ======================================================================== */

Zone_BeginNewZone(client, zone_id=ZONE_NEXT_NEW_ZONE)
{
    if (zone_id == ZONE_NEXT_NEW_ZONE)
    {
        zone_id = g_ZonesCount;
        g_ZonesCount++;
    }
    
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

Zone_RemoveLastPoint(client)
{
    PrintToServer("[wtx-dbg] CLIENT %d REMOVED LAST POINT", client);/////////////////
}

Zone_ClearAllPoints(client)
{
    PrintToServer("[wtx-dbg] CLIENT %d REMOVED LAST POINT", client);/////////////////
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
