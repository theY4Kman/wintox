/**
 * Zones and stages and map-related functions
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_zones_included
 #endinput
#endif
#define _wintox_zones_included

#include <sdktools>
#include "./include/commands"
#include "./include/sql"

#define ZONE_NEXT_NEW_ZONE -2
#define INVALID_ZONE -1
#define DEFAULT_ZONE_HEIGHT 64

#define ZONES_MENU_TITLE "Zones Menu (cur %d zone%s)"

// Current map info
new g_CurMapID = -1;
new String:g_CurMapName[256];
new String:g_CurMapHumanName[256];
new bool:g_CurMapStaged;
new g_CurMapTier = 1;

// Create a static array cache for zones
// Capacity: 32 zones, 31* points each
// * The first point is used to store information about the zone, including
//   how many points are in the zone.
#define MAX_ZONES 32
#define MAX_POINTS 31
new Float:g_ZonesCache[MAX_ZONES][MAX_POINTS+1][3];
new g_ZoneIDs[MAX_ZONES] = { -1, ... };
new String:g_ZoneNames[MAX_ZONES][64];
new g_ZonesCount = 0;
new g_NextZone = 0;

#define ZONE_POINTS[%1] (_:g_ZonesCache[(%1)][0][0])
#define ZONE_HEIGHT[%1] (_:g_ZonesCache[(%1)][0][1])
#define ZONE_EDITING[%1] (bool:g_ZonesCache[(%1)][0][2])

new g_ClientEditZone[MAXPLAYERS] = { INVALID_ZONE, ... };
new Handle:g_ZoneMenu = INVALID_HANDLE;

new bool:g_DrawZones = true;

new g_mdlBeam = 0;
new g_mdlHalo = 0;

///////////////////////////////////////////////////////
// These may be needed later (heh), and I hate warnings
///////////////////////////////////////////////////////
#pragma unused g_CurMapStaged
#pragma unused g_CurMapTier

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
    RegisterCommand("sm_reloadzones", ConCommand_ReloadZones, "sm_reloadzones | Reload all the zones from the database. Note: this will overwrite any unfinished zones.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_synczones", ConCommand_SyncZones, "sm_synczones | Sync the zone cache to the database, including any unfinished zones.",  0, ADMFLAG_GENERIC);
    
    new Handle:draw_zones = CreateConVar("wintox_drawzones", "1", "Whether or not to draw zones (they will be drawn while editing zones, regardless)");
    g_DrawZones = GetConVarBool(draw_zones);
    HookConVarChange(draw_zones, ConVar_DrawZones);
    
    new Function:on_map_loaded = GetFunctionByName(GetMyHandle(), "fwd_Zone_OnMapLoaded");
    AddToForward(g_fwdMapLoaded, INVALID_HANDLE, on_map_loaded);
    
    BuildZoneMenu();

    if (!(g_mdlBeam = PrecacheModel("materials/sprites/laser.vmt")) ||
        !(g_mdlHalo = PrecacheModel("materials/sprites/halo01.vmt")))
	{
        LogError("Couldn't precache model file(s)!");
	}
    
    CreateTimer(0.1, Timer_DrawZones, INVALID_HANDLE, TIMER_REPEAT);
}

public ConVar_DrawZones(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DrawZones = GetConVarBool(cvar);
}

public fwd_Zone_OnMapLoaded(map_id, const String:mapname[], const String:human_name[], tier, bool:is_staged)
{
    CacheZones(0);
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

public Action:ConCommand_ReloadZones(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_ReloadFromDB(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_SyncZones(client, args)
{
    if (args > 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_SyncToDB(client);
    
    return Plugin_Handled;
}



/* ========================================================================
                          ZONE MANIPULATION MENU
   ======================================================================== */

#define ZMENU_INFO_NEW_ZONE "0"
#define ZMENU_INFO_REMOVE_ZONE "1"
#define ZMENU_INFO_CLEAR_ALL_ZONES "2"
#define ZMENU_INFO_RELOAD_ZONES "3"
#define ZMENU_INFO_SYNC_ZONES "4"

BuildZoneMenu()
{
    g_ZoneMenu = CreateMenu(MenuHandler_RootZoneMenu);
    
    SetZoneMenuTitle();
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_NEW_ZONE, "Create or edit zone");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_REMOVE_ZONE, "Remove zone");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_CLEAR_ALL_ZONES, "Clear ALL zones");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_RELOAD_ZONES, "Reload zones from DB");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_SYNC_ZONES, "Sync zones to DB");
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
            
            AddMenuItem(zone_id_submenu, "-2", "Next new zone");
            
            decl String:buffer[32];
            Format(STR(buffer), "0, start zone (%s)", ZONE_POINTS[0] ? "edit" : "create");
            AddMenuItem(zone_id_submenu, "0", buffer);
            
            for (new i=1; i<MAX_ZONES; i++)
            {
                Format(STR(buffer), "%d (%s)", i, ZONE_POINTS[i] ? "edit" : "create");
                AddMenuItem(zone_id_submenu, buffer, buffer);
            }
            
            DisplaySubmenu(zone_id_submenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
        }
        else if (StrEqual(info, ZMENU_INFO_REMOVE_ZONE))
        {
            if (g_ZonesCount == 0)
            {
                PrintToChat(param1, "There are no zones to delete!");
                DisplayMenu(menu, param1, MENU_TIME_FOREVER);
                return;
            }
            
            new Handle:zone_id_submenu = CreateMenu(MenuHandler_RemoveZoneIndex);
            
            SetMenuExitButton(zone_id_submenu, true);
            SetMenuExitBackButton(zone_id_submenu, true);
            SetMenuTitle(zone_id_submenu, "Select a zone index to remove");
            
            for (new i=0; i<MAX_ZONES; i++)
            {
                decl String:buffer[32];
                
                if (ZONE_POINTS[i] == 0)
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
        else if (StrEqual(info, ZMENU_INFO_RELOAD_ZONES))
        {
            Zone_ReloadFromDB(param1);
            DisplayMenu(menu, param1, MENU_TIME_FOREVER);
        }
        else if (StrEqual(info, ZMENU_INFO_SYNC_ZONES))
        {
            Zone_SyncToDB(param1);
            DisplayMenu(menu, param1, MENU_TIME_FOREVER);
        }
    }
    else if (action == MenuAction_Cancel)
        PrevSubmenu(param1);
}

#define ZMENU_EDIT_NEW_POINT "new"
#define ZMENU_EDIT_REMOVE_LAST "removelast"
#define ZMENU_EDIT_CLEAR_ALL "clear"
#define ZMENU_EDIT_FINISH "finish"

public MenuHandler_ZoneIndex(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:sz_zone_id[16];
        GetMenuItem(menu, param2, STR(sz_zone_id));
        new zone_id = StringToInt(sz_zone_id);
        
        if (zone_id == ZONE_NEXT_NEW_ZONE)
        {
            zone_id = g_NextZone++;
        }
        
        Zone_BeginNewZone(param1, zone_id);
        
        new Handle:zone_menu = CreateMenu(MenuHandler_EditZone);
        SetMenuExitButton(zone_menu, true);
        SetMenuExitBackButton(zone_menu, true);
        
        SetMenuTitle(zone_menu, "Editing zone %d", zone_id);
        AddMenuItem(zone_menu, ZMENU_EDIT_NEW_POINT, "New point");
        AddMenuItem(zone_menu, ZMENU_EDIT_REMOVE_LAST, "Remove last point");
        AddMenuItem(zone_menu, ZMENU_EDIT_CLEAR_ALL, "Clear all points");
        AddMenuItem(zone_menu, ZMENU_EDIT_FINISH, "Finish and sync DB");
        
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
        else if (StrEqual(info, ZMENU_EDIT_FINISH))
        {
            Zone_FinishZone(param1);
            PrevSubmenu(param1);
            return;
        }
        
        DisplayMenuAtItem(menu, param1, GetMenuSelectionPosition(), MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

SetZoneMenuTitle()
{
    SetMenuTitle(g_ZoneMenu, ZONES_MENU_TITLE, g_ZonesCount, g_ZonesCount == 1 ? "" : "s");
}


/* ========================================================================
                        ZONE MANIPULATION FUNCTIONS
   ======================================================================== */

WipeZoneCache()
{
    for (new i=0; i<MAX_ZONES; i++)
    {
        for (new j=0; j<MAX_POINTS+1; j++)
        {
            g_ZonesCache[i][j][0] = 0.0;
            g_ZonesCache[i][j][1] = 0.0;
            g_ZonesCache[i][j][2] = 0.0;
        }
        
        g_ZoneIDs[i] = -1;
        g_ZoneNames[i][0] = '\0';
    }
}

Zone_BeginNewZone(client, zone_id=ZONE_NEXT_NEW_ZONE)
{
    if (zone_id == ZONE_NEXT_NEW_ZONE)
    {
        zone_id = g_NextZone;
        if (zone_id >= MAX_ZONES)
        {
            ReplyToCommand(client, "You've hit the maximum number of zones (%d).", MAX_ZONES);
            return;
        }
    }
    
    if (zone_id >= MAX_ZONES || zone_id < 0)
    {
        ReplyToCommand(client, "That zone index (%d) is invalid.", zone_id);
        return;
    }
    
    g_ZonesCount++;
    g_NextZone = zone_id + 1;
    
    g_ClientEditZone[client] = zone_id;
    ZONE_HEIGHT[zone_id] = DEFAULT_ZONE_HEIGHT;
    ZONE_EDITING[zone_id] = true;
    
    // TODO: zone name
    win_SQL_UpsertZone(zone_id, "Zone");
}

Zone_NewZonePoint(client)
{
    new zone_id = g_ClientEditZone[client];
    if (zone_id == INVALID_ZONE)
    {
        ReplyToCommand(client, "You're not currently editing a zone. Use sm_newzone or /zonemenu to begin editing.");
        return;
    }
    
    new point = ZONE_POINTS[zone_id] + 1;
    
    if (point >= MAX_POINTS)
    {
        ReplyToCommand(client, "You've hit the maximum number of zone points (%d).", MAX_POINTS);
        return;
    }
    
    GetClientAbsOrigin(client, g_ZonesCache[zone_id][point]);
    ZONE_POINTS[zone_id]++;
}

Zone_ZoneHeight(client, height)
{
    new zone_id = g_ClientEditZone[client];
    if (zone_id == INVALID_ZONE)
    {
        ReplyToCommand(client, "You're not currently editing a zone. Use sm_newzone or /zonemenu to begin editing.");
        return;
    }
    
    ZONE_HEIGHT[zone_id] = height;
}

Zone_RemoveLastPoint(client)
{
    new zone_id = g_ClientEditZone[client];
    if (zone_id == INVALID_ZONE)
    {
        ReplyToCommand(client, "You're not currently editing a zone. Use sm_newzone or /zonemenu to begin editing.");
        return;
    }
    
    new point = ZONE_POINTS[zone_id];
    if (point == 0)
    {
        ReplyToCommand(client, "There are no points to remove.");
        return;
    }
    
    g_ZonesCache[zone_id][point][0] = 0.0;
    g_ZonesCache[zone_id][point][1] = 0.0;
    g_ZonesCache[zone_id][point][2] = 0.0;
    ZONE_POINTS[zone_id]--;
}

Zone_ClearAllPoints(client)
{
    new zone_id = g_ClientEditZone[client];
    if (zone_id == INVALID_ZONE)
    {
        ReplyToCommand(client, "You're not currently editing a zone. Use sm_newzone or /zonemenu to begin editing.");
        return;
    }
    
    for (new i=1; i<MAX_POINTS; i++)
    {
        g_ZonesCache[zone_id][i][0] = 0.0;
        g_ZonesCache[zone_id][i][1] = 0.0;
        g_ZonesCache[zone_id][i][2] = 0.0;
    }
    
    ZONE_POINTS[zone_id] = 0;
    
    ReplyToCommand(client, "Cleared all points in zone %d.", zone_id);
}

Zone_FinishZone(client)
{
    new zone_id = g_ClientEditZone[client];
    if (zone_id == INVALID_ZONE)
    {
        ReplyToCommand(client, "You're not currently editing a zone. Use sm_newzone or /zonemenu to begin editing.");
        return;
    }
    
    ZONE_EDITING[zone_id] = false;
    g_ClientEditZone[client] = INVALID_ZONE;
    
    SetZoneMenuTitle();
    
    win_SQL_UpsertZone(zone_id);
    win_SQL_ClearZonePoints(zone_id);
    win_SQL_InsertZonePoints(zone_id);
}

Zone_RemoveZone(client, zone_id)
{
    for (new i=0; i<MAX_POINTS+1; i++)
    {
        g_ZonesCache[zone_id][i][0] = 0.0;
        g_ZonesCache[zone_id][i][1] = 0.0;
        g_ZonesCache[zone_id][i][2] = 0.0;
    }
    
    win_SQL_DeleteZone(zone_id);
    
    ReplyToCommand(client, "Removed zone %d.", zone_id);
}

Zone_ClearAllZones(client)
{
    WipeZoneCache();
    
    g_ZonesCount = 0;
    g_NextZone = 0;
    SetZoneMenuTitle();
    
    win_SQL_ClearAllZonePoints();
    win_SQL_ClearAllZones();
    
    ReplyToCommand(client, "Cleared all zones! Phew, that's a load off.");
}

// Inserts/updates all the zones in the cache to the database
Zone_SyncToDB(client)
{
    new zones_synced = 0;
    for (new i=0; i<MAX_ZONES; i++)
    {
        if (ZONE_POINTS[i] <= 0)
            continue;
        
        win_SQL_UpsertZone(i);
        win_SQL_ClearZonePoints(i);
        win_SQL_InsertZonePoints(i);
        
        zones_synced++;
    }
    
    ReplyToCommand(client, "Successfully synced %d zones to the database!", zones_synced);
}

// Grabs all the zones from the database and puts them in the cache
Zone_ReloadFromDB(client)
{
    CacheZones(client);
}


/* ========================================================================
                          ZONE DISPLAY/TEMPENTS
   ======================================================================== */

public Action:Timer_DrawZones(Handle:timer)
{
    new color[4] = {255, 255, 255, 150};
    
    for (new i=0; i<MAX_ZONES; i++)
    {
        if (!g_DrawZones && !ZONE_EDITING[i])
                continue;
        
        // Only draw lines, so no less than 2 points
        if (ZONE_POINTS[i] <= 1)
            continue;
        
        #define BEAM(%1,%2) TE_SetupBeamPoints(%1, %2, g_mdlBeam, g_mdlHalo, 0, 0, 0.2, 4.0, 4.0, 10, 0.0, color, 0); \
                             TE_SendToAll()
        
        decl j;
        for (j=2; j<ZONE_POINTS[i]+1; j++)
        {
            BEAM(g_ZonesCache[i][j-1], g_ZonesCache[i][j]);
        }
        
        // Connect first and last point (if 3 or more points)
        if (j > 2)
        {
            BEAM(g_ZonesCache[i][j-1], g_ZonesCache[i][1]);
        }
    }
}


/* ========================================================================
                            SQL SHIT. GURGLE.
   ======================================================================== */

// Grabs all the zones and their points for the current map from the database
// and caches them in the local static arrays.
CacheZones(const client=-1)
{
    decl String:query[256];
    
    Format(STR(query), SQL_QUERY[GetZones], g_CurMapID);
    SQLT(query, T_CacheZones, client);
}

public T_CacheZones(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl String:buffer[512];
    new client = data;
    decl rows;
    
    if (hndl == INVALID_HANDLE)
    {
        Format(STR(buffer), "Error retrieving zones from database: %s", error);
        LogError("%s", buffer);
        if (client != -1)
            ReplyToCommand(client, "[WTX] %s", buffer);
        return;
    }
    
    rows = SQL_GetRowCount(hndl);
    if (rows == 0)
    {
        if (client != -1)
        {
            ReplyToCommand(client, "[WTX] Successfully retrieved zones from DB, but there were none :'(");
            ReplyToCommand(client, "[WTX] Use sm_newzone%s to create some.", client == 0 ? "" : " or /zonemenu");
        }
        
        return;
    }
    
    WipeZoneCache();
    
    // Cache zones
    new zone_id_max = 0;
    g_ZonesCount = 0;
    while (SQL_FetchRow(hndl))
    {
        decl zone_dbid;
        decl zone_id;
        decl String:zone_name[64];
        
        zone_dbid = SQL_FetchInt(hndl, 0);
        zone_id = SQL_FetchInt(hndl, 1);
        SQL_FetchString(hndl, 2, STR(zone_name));
        
        g_ZoneIDs[zone_id] = zone_dbid;
        strcopy(g_ZoneNames[zone_id], sizeof(g_ZoneNames[]), zone_name);
        
        g_ZonesCount++;
        if (zone_id > zone_id_max)
            zone_id_max = zone_id;
    }
    
    g_NextZone = zone_id_max + 1;
    SetZoneMenuTitle();
    
    if (client != -1)
        ReplyToCommand(client, "[WTX] Successfully retrieved %d rows of zones from DB! Getting zone points...", rows);
    
    Format(STR(buffer), SQL_QUERY[GetZonePoints], g_CurMapID);
    SQLT(buffer, T_CacheZonePoints, client);
}

public T_CacheZonePoints(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl String:buffer[512];
    decl rows;
    new client = data;
    
    if (hndl == INVALID_HANDLE)
    {
        Format(STR(buffer), "Error retrieving zone points from database: %s", error);
        LogError("%s", buffer);
        if (client != -1)
            ReplyToCommand(client, "[WTX] %s", buffer);
        return;
    }
    
    rows = SQL_GetRowCount(hndl);
    if (rows == 0)
    {
        if (client != -1)
            ReplyToCommand(client, "[WTX] Successfully retrieved zone points from DB, but there were none.");
        return;
    }
    
    // Cache zone points
    while (SQL_FetchRow(hndl))
    {
        decl zone_id;
        decl zp_number;
        decl Float:x, Float:y, Float:z;
        
        zone_id = SQL_FetchInt(hndl, 5);
        zp_number = SQL_FetchInt(hndl, 1);
        x = SQL_FetchFloat(hndl, 2);
        y = SQL_FetchFloat(hndl, 3);
        z = SQL_FetchFloat(hndl, 4);
        
        if (zp_number > 0)
        {
            g_ZonesCache[zone_id][zp_number][0] = x;
            g_ZonesCache[zone_id][zp_number][1] = y;
            g_ZonesCache[zone_id][zp_number][2] = z;
        }
        // Metadata is all integers
        else
        {
            (_:g_ZonesCache[zone_id][zp_number][0]) = RoundToFloor(x);
            (_:g_ZonesCache[zone_id][zp_number][1]) = RoundToFloor(y);
            (_:g_ZonesCache[zone_id][zp_number][2]) = RoundToFloor(z);
        }
    }
    
    if (client != -1)
        ReplyToCommand(client, "[WTX] Successfully retrieved %d rows of zone points from DB!", rows);
}

bool:win_SQL_UpsertZone(zone_id, const String:name[]="Zone")
{
    decl String:buffer[256];
    decl String:esc_name[256];
    new String:sz_zone_dbid[16] = "NULL";
    
    if (g_ZoneIDs[zone_id] != -1)
        IntToString(g_ZoneIDs[zone_id], STR(sz_zone_dbid));
    
    SQL_EscapeString(g_Database, name, STR(esc_name));
    Format(STR(buffer), SQL_QUERY[InsertZone], sz_zone_dbid, g_CurMapID, zone_id, esc_name);
    SQLT(buffer, T_UpsertZone, zone_id);
    
    return true;
}

public T_UpsertZone(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new zone_id = data;
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error upserting zone %d to database: %s", zone_id, error);
        return;
    }
    
    g_ZoneIDs[zone_id] = SQL_GetInsertId(g_Database);
}

bool:win_SQL_DeleteZone(zone_id)
{
    decl String:buffer[256];
    Format(STR(buffer), SQL_QUERY[DeleteZone], g_CurMapID, zone_id);
    
    if (!SQL_FastQuery(g_Database, buffer))
    {
        SQL_GetError(g_Database, STR(buffer));
        LogError("Error deleting zone %d from database: %s", zone_id, buffer);
        return false;
    }
    
    return true;
}

win_SQL_InsertZonePoints(zone_id)
{
    new zone_dbid = g_ZoneIDs[zone_id];
    
    if (zone_dbid == -1)
    {
        decl String:buffer[256];
        Format(STR(buffer), SQL_QUERY[GetZone], g_CurMapID, zone_id);
        SQLT(buffer, T_GetZoneDBIDForInsertZonePoints, zone_id);
    }
    else
        __win_SQL_InsertZonePoints(zone_id, zone_dbid);
}

public T_GetZoneDBIDForInsertZonePoints(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new zone_id = data;
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving zone %d DB ID for points insertion: %s", error);
        return;
    }
    
    if (!SQL_FetchRow(hndl))
        return;
    
    new zone_dbid = SQL_FetchInt(hndl, 0);
    
    __win_SQL_InsertZonePoints(zone_id, zone_dbid);
}

__win_SQL_InsertZonePoints(zone_id, zone_dbid)
{
    new Handle:pack = CreateDataPack();
    
    WritePackCell(pack, zone_id);
    WritePackCell(pack, zone_dbid);
    WritePackCell(pack, 0);
    ResetPack(pack);
    
    T_InsertZonePoints(INVALID_HANDLE, INVALID_HANDLE, "", pack);
}

public T_InsertZonePoints(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new Handle:pack = Handle:data;
    
    ResetPack(pack);
    new zone_id = ReadPackCell(pack);
    new zone_dbid = ReadPackCell(pack);
    new zp_number = ReadPackCell(pack);
    CloseHandle(pack);
    
    if (zp_number > ZONE_POINTS[zone_id]+1)
        return;
    
    new Float:x = g_ZonesCache[zone_id][zp_number][0];
    new Float:y = g_ZonesCache[zone_id][zp_number][1];
    new Float:z = g_ZonesCache[zone_id][zp_number][2];
    
    // Need to convert metadata from ints to floats
    if (zp_number == 0)
    {
        x = float(_:x);
        y = float(_:y);
        z = float(_:z);
    }
    
    decl String:buffer[256];
    Format(STR(buffer), SQL_QUERY[InsertZonePoint], zone_dbid, zp_number, x, y, z);
    
    zp_number++;
    
    pack = CreateDataPack();
    WritePackCell(pack, zone_id);
    WritePackCell(pack, zone_dbid);
    WritePackCell(pack, zp_number);
    ResetPack(pack);
    
    SQLT(buffer, T_InsertZonePoints, pack);
}

bool:win_SQL_ClearZonePoints(zone_id)
{
    decl String:buffer[256];
    Format(STR(buffer), SQL_QUERY[ClearZonePoints], g_CurMapID, zone_id);
    
    if (!SQL_FastQuery(g_Database, buffer))
    {
        SQL_GetError(g_Database, STR(buffer));
        LogError("Error clearing zone %d points from database: %s", zone_id, buffer);
        return false;
    }
    
    return true;
}

bool:win_SQL_ClearAllZonePoints()
{
    decl String:buffer[256];
    Format(STR(buffer), SQL_QUERY[ClearAllZonePoints], g_CurMapID);
    
    if (!SQL_FastQuery(g_Database, buffer))
    {
        SQL_GetError(g_Database, STR(buffer));
        LogError("Error clearing all zone points from database: %s", buffer);
        return false;
    }
    
    return true;
}

bool:win_SQL_ClearAllZones()
{
    decl String:buffer[256];
    Format(STR(buffer), SQL_QUERY[ClearAllZones], g_CurMapID);
    
    if (!SQL_FastQuery(g_Database, buffer))
    {
        SQL_GetError(g_Database, STR(buffer));
        LogError("Error clearing all zones from database: %s", buffer);
        return false;
    }
    
    return true;
}
