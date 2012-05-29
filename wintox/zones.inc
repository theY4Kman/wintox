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

#include "./wintox/commands"
#include "./wintox/sql"
#include "./wintox/stocks"
#include "./wintox/events"
#include "./wintox/utils"

#define ZONE_NEXT_NEW_ZONE -2
#define INVALID_ZONE -1
#define INVALID_TRACK -1
#define INVALID_PATH -1
#define DEFAULT_ZONE_HEIGHT 80
#define MIN_FLAG_HEIGHT 80.0
#define TRACK_MENU_NEXT_INVALID -1

#define ZONES_MENU_TITLE "Zones Menu (cur %d zone%s in %d track%s)"
#define EDIT_ZONE_MENU_TITLE "Editing zone %d, track %d (%s)"

// An arbitrary, unique number to identify a particular text entry routine
#define TEXT_ENTRY_ZONE_NAME 97

// Current map info
new g_CurMapID = -1;
new String:g_CurMapName[256];
new String:g_CurMapHumanName[256];
new bool:g_CurMapStaged;
new g_CurMapTier = 1;

#define ZONE_METADATA_POINT 0
#define ZONE_FLAG_POINT 1
#define ZONE_META_POINTS 2
#define ZONE_FIRST_POINT ZONE_META_POINTS

#define MAX_TRACKS 8
#define MAX_ZONES 64
#define MAX_POINTS 30
#define POINTS_LENGTH (MAX_POINTS + ZONE_META_POINTS)

#define MAIN_TRACK 0

#define ZONE_POINT[%1][%2] (g_ZonesCache[MAIN_TRACK][(%1)][(%2)])
#define ZONE_POINTS[%1] (_:g_ZonesCache[MAIN_TRACK][(%1)][0][0])
#define ZONE_HEIGHT[%1] (_:g_ZonesCache[MAIN_TRACK][(%1)][0][1])
#define ZONE_EDITING[%1] (bool:g_ZonesCache[MAIN_TRACK][(%1)][0][2])
#define ZONE_POINT_EMPTY(%1,%2) (g_ZonesCache[MAIN_TRACK][%1][%2][0] == 0.0 && g_ZonesCache[MAIN_TRACK][%1][%2][1] == 0.0 && g_ZonesCache[MAIN_TRACK][%1][%2][2] == 0.0)
#define ZONE_EMPTY(%1) (ZONE_POINTS[%1] == 0 || ZONE_POINT_EMPTY(%1,ZONE_FIRST_POINT))

// Ugh. I should've planned ahead of time for tracks
#define ZONE_POINT_T[%1][%2][%3] g_ZonesCache[(%1)][(%2)][(%3)]
#define ZONE_POINTS_T[%1][%2] (_:g_ZonesCache[(%1)][(%2)][0][0])
#define ZONE_HEIGHT_T[%1][%2] (_:g_ZonesCache[(%1)][(%2)][0][1])
#define ZONE_EDITING_T[%1][%2] (bool:g_ZonesCache[(%1)][(%2)][0][2])
#define ZONE_POINT_EMPTY_T(%1,%2,%3) (g_ZonesCache[(%1)][(%2)][(%3)][0] == 0.0 && g_ZonesCache[(%1)][(%2)][(%3)][1] == 0.0 && g_ZonesCache[(%1)][(%2)][(%3)][2] == 0.0)
#define ZONE_EMPTY_T(%1,%2) (ZONE_POINTS_T[%1][%2] == 0 || ZONE_POINT_EMPTY_T(%1,%2,ZONE_FIRST_POINT))

#define IS_EDITING_BOX(%1) (g_ClientEditBox[(%1)][0] != 0.0 || g_ClientEditBox[(%1)][1] != 0.0 || g_ClientEditBox[(%1)][2] != 0.0)

#define ZONE_FLAG[%1][%2] (g_ZonesCache[MAIN_TRACK][(%1)][ZONE_FLAG_POINT][(%2)])
#define ZONE_FLAG_EMPTY(%1) (ZONE_FLAG[%1][0] == 0.0 && ZONE_FLAG[%1][1] == 0.0 && ZONE_FLAG[%1][2] == 0.0)

#define ZONE_FLAG_T[%1][%2][%3] (g_ZonesCache[(%1)][(%2)][ZONE_FLAG_POINT][(%3)])
#define ZONE_FLAG_EMPTY_T(%1,%2) (ZONE_FLAG_T[%1][%2][0] == 0.0 && ZONE_FLAG_T[%1][%2][1] == 0.0 && ZONE_FLAG_T[%1][%2][2] == 0.0)

#define IS_END_ZONE(%1) ((%1) == g_EndZone[MAIN_TRACK])
#define IS_END_ZONE_T(%1,%2) ((%2) == g_EndZone[(%1)])

#define FLAG_COLOR_START { 0, 255, 50, 240 }
#define FLAG_COLOR_END { 255, 100, 0, 240 }
#define FLAG_COLOR_ZONE { 255, 255, 255, 200 }
#define FLAG_COLOR_BONUS_START { 245, 0, 150, 180 }
#define FLAG_COLOR(%1) ((%1) == 0 ? FLAG_COLOR_START : (IS_END_ZONE(%1) ? FLAG_COLOR_END : FLAG_COLOR_ZONE))

#define FLAG_ROTATE_STEP 0.0981747704   // PI / 32
#define FLAG_ROTATE_INITIAL 1.57079633 // PI / 2

// Amount of leeway in zone heights, just for good measure. In-game zone creation ain't perfect.
#define ZONE_HEIGHT_FUDGE 4.0

#define PLAYER_IN_ZONE(%1,%2) (g_PlayerCurrentZone[(%1)] == (%2))

#define CREATE_ZONE_TRACK_PACK() \
    new Handle:pack = CreateDataPack(); \
    WritePackCell(pack, track); \
    WritePackCell(pack, zone_id); \
    ResetPack(pack)

#define READ_TRACK_PACK() \
    new Handle:pack = Handle:data; \
    new track = ReadPackCell(pack); \
    new zone_id = ReadPackCell(pack); \
    CloseHandle(pack)

#define NOT_EDITING_RETURN() { \
        Reply(client, "You're not currently editing a zone. Use {T}sm_newzone{N} or /zonemenu to begin editing."); \
        return; \
    }

#define IS_TRACK_EMPTY(%1) (g_TrackPath[(%1)][0] == 0)

// Create a static array cache for zones
// Capacity: 64 zones, 30* points each
// * The first point is used to store information about the zone, including
//   how many points are in the zone. The second point is used to store the
//   location of the flag. So there are technically 32 points.
new Float:g_ZonesCache[MAX_TRACKS][MAX_ZONES][POINTS_LENGTH][3];

// The database IDs of each zone
new g_ZoneIDs[MAX_TRACKS][MAX_ZONES];
// The names of the zones
new String:g_ZoneNames[MAX_TRACKS][MAX_ZONES][64];

// The total number of tracks cached
new g_TracksCount = 1;
// The total number of zones cached
new g_ZonesCount[MAX_TRACKS];
// The zone index of the last zone in each track
new g_EndZone[MAX_TRACKS];

// Stores the zone indexes in proper order for each track, beginning at the first zone
// (not the start zone), so 0 is an invalid/empty zone
new g_TrackPaths[MAX_TRACKS][MAX_ZONES];
// The tier of each track (used for giving points on bonus track completion)
new g_TrackTier[MAX_TRACKS] = { 1, ... };
// The name of each track
new String:g_TrackNames[MAX_TRACKS][64];

// The zone ID the player was last inside of
new g_PlayerCurrentZone[MAXPLAYERS] = { INVALID_ZONE, ... };
// The track ID the player is currently playing in
new g_PlayerCurrentTrack[MAXPLAYERS] = { MAIN_TRACK, ... };
// The index into the track path the player is currently on
new g_PlayerCurrentPath[MAXPLAYERS] = { INVALID_PATH, ... };

// Which zone the player is currently editing
new g_ClientEditZone[MAXPLAYERS] = { INVALID_ZONE, ... };
// The track the zone is on which the player is currently editing
new g_ClientEditTrack[MAXPLAYERS] = { INVALID_TRACK, ... };
// The start point for zone point box creation
new Float:g_ClientEditBox[MAXPLAYERS][3];

new Handle:g_ZoneMenu = INVALID_HANDLE;
new Handle:g_TrackMenu = INVALID_HANDLE;
// Stores the action to execute next when the player selects a track in the track menu
new g_TrackMenuNext[MAXPLAYERS] = { TRACK_MENU_NEXT_INVALID, ... };
// The track the player selected in the track menu
new g_TrackMenuSelected[MAXPLAYERS] = { INVALID_TRACK, ... };

// The menu of the player's current zone edit menu (used for updating the title
// when the zone title changes)
new Handle:g_ClientEditZoneMenu[MAXPLAYERS] = { INVALID_HANDLE, ... };

// Stores the state of the cvar wintox_draw_zones, whether to draw zones
new bool:g_DrawZones = false;
// Stores the state of the cvar wintox_draw_flags, whether to draw flags
new bool:g_DrawFlags = true;
// Stores the state of wintox_draw_bonus_flags, whether to draw bonus flags
new bool:g_DrawBonusFlags = true;
// Stores the state of wintox_zone_height_fudge, the amount of leeway in player-zone detection
new Float:g_ZoneHeightFudge = ZONE_HEIGHT_FUDGE;

// Stores the amount of rotation for each flag
new Float:g_FlagRotation[MAX_TRACKS][MAX_ZONES];
new g_mdlBeam = 0;

new bool:g_ZoneCacheInProgress = false;
new bool:g_ZoneSyncInProgress = false;

new g_ZoneSyncLastZone = -1;
new g_ZoneSyncLastTrack = -1;
new g_ZoneSyncNotifyClient = -1;
new ReplySource:g_ZoneSyncReplySource = SM_REPLY_TO_CHAT;
new g_ZonesSynced[MAX_TRACKS] = 0;
new g_TracksSynced = 0;
new g_TotalZonesSynced = 0;


//////////////////////////////////////////////////////////
// These may be needed later (heh), and I hate warnings //
//////////////////////////////////////////////////////////
#pragma unused g_CurMapStaged

Zones_Init()
{
    // Initialize g_ZoneIDs, because I can't initialize a multi-dimensional array at declaration >:(
    for (new i=0; i<MAX_TRACKS; i++)
    {
        for (new j=0; j<MAX_ZONES; j++)
        {
            g_ZoneIDs[i][j] = -1;
            g_FlagRotation[i][j] = j * FLAG_ROTATE_INITIAL;
        }
    }
    
    RegisterCommand("sm_zonemenu", ConCommand_ZoneMenu, "sm_zonemenu | Bring up the zone menu.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_newzone", ConCommand_NewZone, "sm_newzone [<zone index>] [<track=0>] | Create a new zone.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_beginbox", ConCommand_BeginBox, "sm_beginbox | Start creating the current zone using a box (rectangular prism).",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_endbox", ConCommand_EndBox, "sm_endbox | End box creation, saving the points and height to the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_newzonepoint", ConCommand_NewZonePoint, "sm_newzonepoint | Create a new point in the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_zoneheight", ConCommand_ZoneHeight, "sm_zoneheight <height> | Sets the height of the zone currently being edited in map units.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_setflag", ConCommand_SetFlag, "sm_setflag | Set the location of the flag for the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_setname", ConCommand_SetName, "sm_setname | Set the name of the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_removelastpoint", ConCommand_RemoveLastPoint, "sm_removelastpoint | Remove the last point of the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_clearallpoints", ConCommand_ClearAllPoints, "sm_clearallpoints | Clear all the points of the zone currently being edited.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_finishzone", ConCommand_FinishZone, "sm_finishzone | Finalize the zone and its points and sync with the database.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_removezone", ConCommand_RemoveZone, "sm_removezone <zone index> [<track=0>] | Remove the zone with the specified zone ID.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_cleartrack", ConCommand_ClearTrack, "sm_cleartrack <track> | Clear all zones in the specified track.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_clearallzones", ConCommand_ClearAllZones, "sm_clearallzones | Clear all zones for the current map.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_reloadzones", ConCommand_ReloadZones, "sm_reloadzones | Reload all the zones from the database. Note: this will overwrite any unfinished zones.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_synczones", ConCommand_SyncZones, "sm_synczones | Sync the zone cache to the database, including any unfinished zones.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_tracktier", ConCommand_TrackTier, "sm_tracktier <track #> <tier (1-6)> | Set the tier of the specified track.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_trackname", ConCommand_TrackName, "sm_trackname <track #> <name> | Set the name of the specified track.",  0, ADMFLAG_GENERIC);
    RegisterCommand("sm_maptier", ConCommand_MapTier, "sm_maptier <tier (1-6)> | Set the tier of the current map.",  0, ADMFLAG_GENERIC);
    
    new Handle:draw_zones = CreateConVar("wintox_draw_zones", "0", "Whether or not to draw all zones (they will be drawn while editing zones, regardless)");
    g_DrawZones = GetConVarBool(draw_zones);
    HookConVarChange(draw_zones, ConVar_DrawZones);
    
    new Handle:draw_flags = CreateConVar("wintox_draw_flags", "0", "Whether or not to draw all flags (they will be drawn while editing zones, regardless)");
    g_DrawFlags = GetConVarBool(draw_flags);
    HookConVarChange(draw_flags, ConVar_DrawFlags);
    
    new Handle:draw_bonus_flags = CreateConVar("wintox_draw_bonus_flags", "1", "Whether or not to draw the start zone flags of bonus tracks");
    g_DrawBonusFlags = GetConVarBool(draw_bonus_flags);
    HookConVarChange(draw_bonus_flags, ConVar_DrawBonusFlags);
    
    new Handle:zone_height_fudge = CreateConVar("wintox_zone_height_fudge", "10.0", "The amount, in map units, of leeway used when detecting if a player is at the right height for a zone.");
    g_ZoneHeightFudge = GetConVarFloat(zone_height_fudge);
    HookConVarChange(zone_height_fudge, ConVar_ZoneHeightFudge);
    
    AddToForward(g_fwdMapLoaded, INVALID_HANDLE, fwd_Zone_OnMapLoaded);
    AddToForward(g_fwdPointsSyncFinished, INVALID_HANDLE, fwd_Zone_OnPointsSyncFinished);
    AddToForward(g_fwdTextEntryComplete, INVALID_HANDLE, fwd_Zone_OnTextEntryComplete);
    
    BuildTrackMenu();
    for (new track=0; track<MAX_TRACKS; track++)
        BuildZoneMenu();

    if (!(g_mdlBeam = PrecacheModel("materials/sprites/laser.vmt")))
        LogError("Couldn't precache model file(s)!");
    
    CreateTimer(0.1, Timer_DrawZones, INVALID_HANDLE, TIMER_REPEAT);
}

Zone_DeinitPlayer(client)
{
    g_PlayerCurrentZone[client] = INVALID_ZONE;
    g_PlayerCurrentTrack[client] = MAIN_TRACK;
    g_PlayerCurrentPath[client] = INVALID_PATH;
    
    g_TrackMenuNext[client] = TRACK_MENU_NEXT_INVALID;
    g_TrackMenuSelected[client] = INVALID_TRACK;
    
    new editing = g_ClientEditZone[client];
    new editing_track = g_ClientEditTrack[client];
    if (editing != INVALID_ZONE)
        ZONE_EDITING_T[editing_track][editing] = false;
    
    g_ClientEditZone[client] = INVALID_ZONE;
    g_ClientEditTrack[client] = INVALID_TRACK;
    
    g_ClientEditBox[client][0] = 0.0;
    g_ClientEditBox[client][1] = 0.0;
    g_ClientEditBox[client][2] = 0.0;
}

bool:PointInZone(const Float:point[3], track, zone)
{
    new sides = ZONE_POINTS_T[track][zone];
    new j = sides-1 + ZONE_FIRST_POINT;
    new bool:odd_nodes = false;
    
    for (new h=ZONE_FIRST_POINT; h<(ZONE_META_POINTS + sides); h++)
    {
        if ((ZONE_POINT_T[track][zone][h][1] < point[1] && ZONE_POINT_T[track][zone][j][1] >= point[1]) ||
            (ZONE_POINT_T[track][zone][j][1] < point[1] && ZONE_POINT_T[track][zone][h][1] >= point[1]))
            if (ZONE_POINT_T[track][zone][h][0] + (point[1] - ZONE_POINT_T[track][zone][h][1]) / (ZONE_POINT_T[track][zone][j][1] -
                ZONE_POINT_T[track][zone][h][1]) * (ZONE_POINT_T[track][zone][j][0] - ZONE_POINT_T[track][zone][h][0]) < point[0])
                odd_nodes = !odd_nodes;
        
        j = h;
    }
    
    // In zone AND in zone height
    if (odd_nodes &&
        point[2] >= (ZONE_POINT_T[track][zone][ZONE_FIRST_POINT][2] - g_ZoneHeightFudge) &&
        point[2] <= (ZONE_POINT_T[track][zone][ZONE_FIRST_POINT][2] + ZONE_HEIGHT_T[track][zone] + g_ZoneHeightFudge))
        return true;
    
    return false;
}

public ConVar_DrawZones(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DrawZones = GetConVarBool(cvar);
}

public ConVar_DrawFlags(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DrawFlags = GetConVarBool(cvar);
}

public ConVar_DrawBonusFlags(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_DrawBonusFlags = GetConVarBool(cvar);
}

public ConVar_ZoneHeightFudge(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_ZoneHeightFudge = GetConVarFloat(cvar);
}

public fwd_Zone_OnMapLoaded(map_id, const String:mapname[], const String:human_name[], tier, bool:is_staged)
{
    CacheTracks(0);
    CacheZones(0);
}

// TODO: refactor TextEntry to accept a Function argument
public fwd_Zone_OnTextEntryComplete(client, const String:text[], any:data)
{
    if (data == TEXT_ENTRY_ZONE_NAME)
    {
        Zone_SetName(client, text);
        ResetClientZoneEditMenu(client);
    }
}



/* ========================================================================
                      ZONE MANIPULATION CONCOMMANDS
   ======================================================================== */

public Action:ConCommand_ZoneMenu(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    DisplayZoneMenu(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_NewZone(client, args)
{
    CHECK_ARGS_UPPER(2)
    RESET_REPLY()
    
    new zone_id = ZONE_NEXT_NEW_ZONE;
    new track_id = MAIN_TRACK;
    
    if (args >= 1)
    {
        decl String:sz_zone_id[32];
        GetCmdArg(1, STR(sz_zone_id));
        
        if (StringToIntEx(sz_zone_id, zone_id) == 0)
        {
            ReplyWithDescription(client);
            return Plugin_Handled;
        }
        
        if (args == 2)
        {
            decl String:sz_track_id[32];
            GetCmdArg(2, STR(sz_track_id));
            
            if (StringToIntEx(sz_track_id, track_id) == 0)
            {
                ReplyWithDescription(client);
                return Plugin_Handled;
            }
        }
    }
    
    Zone_BeginNewZone(client, zone_id, track_id);
    
    return Plugin_Handled;
}

public Action:ConCommand_NewZonePoint(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_NewZonePoint(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_BeginBox(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_BeginBox(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_EndBox(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_EndBox(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_ZoneHeight(client, args)
{
    EXACT_ARGS(1)
    RESET_REPLY()
    
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

public Action:ConCommand_SetFlag(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_SetFlag(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_SetName(client, args)
{
    EXACT_ARGS(1)
    RESET_REPLY()
    
    decl String:name[64];
    GetCmdArg(1, STR(name));
    
    Zone_SetName(client, name);
    
    return Plugin_Handled;
}

public Action:ConCommand_RemoveLastPoint(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_RemoveLastPoint(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_ClearAllPoints(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_ClearAllPoints(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_FinishZone(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_FinishZone(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_RemoveZone(client, args)
{
    CHECK_ARGS_SPAN(1,2)
    RESET_REPLY()
    
    new track_id = MAIN_TRACK;

    decl zone_id;
    decl String:sz_zone_id[32];
    GetCmdArg(1, STR(sz_zone_id));
    
    if (StringToIntEx(sz_zone_id, zone_id) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    if (args == 2)
    {
        decl String:sz_track_id[32];
        GetCmdArg(2, STR(sz_track_id));
        
        if (StringToIntEx(sz_track_id, track_id) == 0)
        {
            ReplyWithDescription(client);
            return Plugin_Handled;
        }
    }
    
    Zone_RemoveZone(client, zone_id, track_id);
    
    return Plugin_Handled;
}

public Action:ConCommand_ClearTrack(client, args)
{
    EXACT_ARGS(1)
    RESET_REPLY()
    
    decl track_id;
    decl String:sz_track_id[32];
    GetCmdArg(1, STR(sz_track_id));
    
    if (StringToIntEx(sz_track_id, track_id) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    Zone_ClearTrack(client, track_id);
    
    return Plugin_Handled;
}

public Action:ConCommand_ClearAllZones(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_ClearAllZones(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_ReloadZones(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_ReloadFromDB(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_SyncZones(client, args)
{
    NO_ARGS()
    RESET_REPLY()
    
    Zone_SyncToDB(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_TrackTier(client, args)
{
    EXACT_ARGS(2)
    RESET_REPLY()
    
    decl String:sz_track[8];
    decl String:sz_tier[8];
    decl track, tier;
    
    GetCmdArg(1, STR(sz_track));
    if (StringToIntEx(sz_track, track) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    if (track < 0 || track >= MAX_TRACKS)
    {
        Reply(client, "Invalid track (%d). Track must be from 0 to %d", track, MAX_TRACKS-1);
        return Plugin_Handled;
    }
    
    GetCmdArg(2, STR(sz_tier));
    if (StringToIntEx(sz_tier, tier) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    if (tier < 1 || tier > 6)
    {
        Reply(client, "Invalid tier (%d). Tier values must be 1-6.", tier);
        return Plugin_Handled;
    }
    
    Track_SetTier(client, track, tier);
    
    return Plugin_Handled;
}

public Action:ConCommand_TrackName(client, args)
{
    EXACT_ARGS(2)
    RESET_REPLY()
    
    decl String:sz_track[8];
    decl String:name[64];
    decl track;
    
    GetCmdArg(1, STR(sz_track));
    if (StringToIntEx(sz_track, track) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    if (track < 0 || track >= MAX_TRACKS)
    {
        Reply(client, "Invalid track (%d). Track must be from 0 to %d", track, MAX_TRACKS-1);
        return Plugin_Handled;
    }
    
    GetCmdArg(2, STR(name));
    
    Track_SetName(client, track, name);
    
    return Plugin_Handled;
}

public Action:ConCommand_MapTier(client, args)
{
    EXACT_ARGS(1)
    RESET_REPLY()
    
    decl String:sz_tier[8];
    decl tier;
    
    GetCmdArg(1, STR(sz_tier));
    if (StringToIntEx(sz_tier, tier) == 0)
    {
        ReplyWithDescription(client);
        return Plugin_Handled;
    }
    
    if (tier < 1 || tier > 6)
    {
        Reply(client, "Invalid tier (%d). Tier values must be 1-6.", tier);
        return Plugin_Handled;
    }
    
    Map_SetTier(client, tier);
    
    return Plugin_Handled;
}



/* ========================================================================
                          ZONE MANIPULATION MENU
   ======================================================================== */

#define ZMENU_INFO_NEW_ZONE "new"
#define ZMENU_INFO_REMOVE_ZONE "remove"
#define ZMENU_INFO_CLEAR_ALL_ZONES "clear"
#define ZMENU_INFO_RELOAD_ZONES "reload"
#define ZMENU_INFO_SYNC_ZONES "sync"

BuildZoneMenu()
{
    if (g_ZoneMenu != INVALID_HANDLE)
        CloseHandle(g_ZoneMenu);
    
    g_ZoneMenu = CreateMenu(MenuHandler_RootZoneMenu);
    
    SetZoneMenuTitle();
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_NEW_ZONE, "Create or edit zone");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_REMOVE_ZONE, "Remove zone");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_CLEAR_ALL_ZONES, "Clear ALL zones");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_RELOAD_ZONES, "Reload zones from DB");
    AddMenuItem(g_ZoneMenu, ZMENU_INFO_SYNC_ZONES, "Sync zones to DB (slow)");
}

BuildTrackMenu()
{
    if (g_TrackMenu != INVALID_HANDLE)
        CloseHandle(g_TrackMenu);
    
    g_TrackMenu = CreateMenu(MenuHandler_TrackIndex);
    
    SetMenuExitButton(g_TrackMenu, true);
    SetMenuExitBackButton(g_TrackMenu, true);
    SetMenuTitle(g_TrackMenu, "Select a track index");
    
    decl String:buffer[32];
    Format(STR(buffer), "0, main (%d zones)", g_ZonesCount[0]);
    AddMenuItem(g_TrackMenu, "0", buffer);
    
    for (new i=1; i<MAX_TRACKS; i++)
    {
        Format(STR(buffer), "%d, bonus (%d zones)", i, g_ZonesCount[i]);
        AddMenuItem(g_TrackMenu, buffer, buffer);
    }
}

#define ZMENU_EDIT_NEW_POINT "new"
#define ZMENU_EDIT_BOX "box"
#define ZMENU_EDIT_REMOVE_LAST "removelast"
#define ZMENU_EDIT_CLEAR_ALL "clear"
#define ZMENU_EDIT_FINISH "finish"
#define ZMENU_EDIT_SET_FLAG "flag"
#define ZMENU_EDIT_SET_NAME "name"

Handle:BuildEditZoneMenu(client, zone_id, track)
{
    decl String:buffer[64];
    
    new Handle:zone_menu = CreateMenu(MenuHandler_EditZone);
    SetMenuExitButton(zone_menu, true);
    SetMenuExitBackButton(zone_menu, true);
    
    SetMenuTitle(zone_menu, EDIT_ZONE_MENU_TITLE, zone_id, track, g_ZoneNames[track][zone_id]);
    AddMenuItem(zone_menu, ZMENU_EDIT_NEW_POINT, "New point");
    
    Format(STR(buffer), "%s box", IS_EDITING_BOX(client) ? "Finish" : "Begin");
    AddMenuItem(zone_menu, ZMENU_EDIT_BOX, buffer);
    
    AddMenuItem(zone_menu, ZMENU_EDIT_REMOVE_LAST, "Remove last point");
    AddMenuItem(zone_menu, ZMENU_EDIT_CLEAR_ALL, "Clear all points");
    AddMenuItem(zone_menu, ZMENU_EDIT_SET_FLAG, "Set flag location");
    AddMenuItem(zone_menu, ZMENU_EDIT_SET_NAME, "Set name (in chat)");
    AddMenuItem(zone_menu, ZMENU_EDIT_FINISH, "Finish and sync DB");
    
    return zone_menu;
}

ResetClientZoneEditMenu(client)
{
    // Set to something other than INVALID_HANDLE (see MenuAction_Cancel of zone edit menu)
    g_ClientEditZoneMenu[client] = NOT_INVALID_HANDLE;
    PrevSubmenu(client, false);
    
    g_ClientEditZoneMenu[client] = BuildEditZoneMenu(client, g_ClientEditZone[client], g_ClientEditTrack[client]);
    DisplaySubmenu(g_ClientEditZoneMenu[client], client, MENU_TIME_FOREVER);
    
    // Finally revert to INVALID_HANDLE, so zone edit menu cancel calls PrevSubmenu
    g_ClientEditZoneMenu[client] = INVALID_HANDLE;
}

DisplayZoneMenu(client)
{
    DisplaySubmenu(g_ZoneMenu, client, MENU_TIME_FOREVER, false);
}

DisplayTrackMenu(client)
{
    DisplaySubmenu(g_TrackMenu, client, MENU_TIME_FOREVER, false, GetMenuSelectionPosition());
}

SetZoneMenuTitle()
{
    // I should really just keep track of g_ZonesCount better
    // But FUCK THAT, I'M YAK.
    new total_zones = 0;
    for (new track=0; track<MAX_TRACKS; track++)
    {
        g_ZonesCount[track] = CountZones(track);
        total_zones += g_ZonesCount[track];
    }
    
    SetMenuTitle(g_ZoneMenu, ZONES_MENU_TITLE, total_zones, total_zones == 1 ? "" : "s", g_TracksCount, g_TracksCount == 1 ? "" : "s");
}

#define TRACK_MENU_NEXT_CREATE 0
#define TRACK_MENU_NEXT_REMOVE 1

public MenuHandler_RootZoneMenu(Handle:menu, MenuAction:action, param1, param2)
{
    // Force output to chat
    ENTER_REPLY(SM_REPLY_TO_CHAT);
    
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
        if (StrEqual(info, ZMENU_INFO_NEW_ZONE))
        {
            g_TrackMenuNext[param1] = TRACK_MENU_NEXT_CREATE;
            DisplayTrackMenu(param1);
        }
        else if (StrEqual(info, ZMENU_INFO_REMOVE_ZONE))
        {
            g_TrackMenuNext[param1] = TRACK_MENU_NEXT_REMOVE;
            DisplayTrackMenu(param1);
        }
        else if (StrEqual(info, ZMENU_INFO_CLEAR_ALL_ZONES))
        {
            new Handle:confirm_menu = CreateMenu(MenuHandler_ConfirmClearZones);
            SetMenuExitButton(confirm_menu, true);
            SetMenuExitBackButton(confirm_menu, true);
            SetMenuTitle(confirm_menu, "Are you sure you want to delete ALL zones?");
            
            AddMenuItem(confirm_menu, "no", "No");
            AddMenuItem(confirm_menu, "kinda yes", "Yes");
            
            DisplaySubmenu(confirm_menu, param1, MENU_TIME_FOREVER, true);
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
    
    EXIT_REPLY();
}

public MenuHandler_TrackIndex(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:sz_track[16];
        GetMenuItem(menu, param2, STR(sz_track));
        g_TrackMenuSelected[param1] = StringToInt(sz_track);
        
        if (g_TrackMenuNext[param1] == TRACK_MENU_NEXT_CREATE)
        {
            g_ClientEditTrack[param1] = g_TrackMenuSelected[param1];
            
            // We need to find out what zone index the user wants this zone to be
            // Let's make a menu! YEAH!
            new Handle:zone_id_submenu = CreateMenu(MenuHandler_ZoneIndex);
            
            SetMenuExitButton(zone_id_submenu, true);
            SetMenuExitBackButton(zone_id_submenu, true);
            SetMenuTitle(zone_id_submenu, "Select a zone index");
            
            AddMenuItem(zone_id_submenu, "-2", "Next new zone");
            
            decl String:buffer[96];
            Format(STR(buffer), "0, start (%s) - %s", g_ZoneNames[g_TrackMenuSelected[param1]][0], !ZONE_EMPTY_T(g_TrackMenuSelected[param1],0) ? "edit" : "create");
            AddMenuItem(zone_id_submenu, "0", buffer);
            
            for (new i=1; i<MAX_ZONES; i++)
            {
                Format(STR(buffer), "%d (%s) - %s", i, g_ZoneNames[g_TrackMenuSelected[param1]][i], !ZONE_EMPTY_T(g_TrackMenuSelected[param1],i) ? "edit" : "create");
                AddMenuItem(zone_id_submenu, buffer, buffer);
            }
            
            PrevSubmenu(param1, false);
            DisplaySubmenu(zone_id_submenu, param1, MENU_TIME_FOREVER, true, GetLastMenuPosition(param1));
        }
        else if (g_TrackMenuNext[param1] == TRACK_MENU_NEXT_REMOVE)
        {
            new track = g_ZonesCount[g_TrackMenuSelected[param1]];
            if (track == 0)
            {
                PrintToChat(param1, "There are no zones in track %d to delete!", track);
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
                
                if (ZONE_POINTS_T[g_ZonesCount[track]][i] == 0)
                    continue;
                
                Format(STR(buffer), "%d (%s)", i, g_ZoneNames[track][i]);
                AddMenuItem(zone_id_submenu, buffer, buffer);
            }
            
            PrevSubmenu(param1, false);
            DisplaySubmenu(zone_id_submenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
        }
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public MenuHandler_ZoneIndex(Handle:menu, MenuAction:action, param1, param2)
{
    // Force output to chat
    ENTER_REPLY(SM_REPLY_TO_CHAT);
    
    if (action == MenuAction_Select)
    {
        decl String:sz_zone_id[16];
        GetMenuItem(menu, param2, STR(sz_zone_id));
        new zone_id = StringToInt(sz_zone_id);
        
        if (zone_id == ZONE_NEXT_NEW_ZONE)
            zone_id = GetNextNewZone(g_TrackMenuSelected[param1]);
        
        Zone_BeginNewZone(param1, zone_id, g_TrackMenuSelected[param1]);
        
        new Handle:zone_menu = BuildEditZoneMenu(param1, zone_id, g_TrackMenuSelected[param1]);
        
        // Pop the zone index submenu, but don't redisplay the root zone menu
        PrevSubmenu(param1, false);
        DisplaySubmenu(zone_menu, param1, MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
    {
        g_ClientEditZone[param1] = INVALID_ZONE;
        g_ClientEditTrack[param1] = INVALID_TRACK;
        
        HANDLE_CANCEL();
    }
    
    EXIT_REPLY();
}

public MenuHandler_RemoveZoneIndex(Handle:menu, MenuAction:action, param1, param2)
{
    // Force output to chat
    SetCmdReplySource(SM_REPLY_TO_CHAT);
    
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
    // Force output to chat
    SetCmdReplySource(SM_REPLY_TO_CHAT);
    
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
        
        // Force output to chat
        SetCmdReplySource(SM_REPLY_TO_CHAT);
        
        if (StrEqual(info, ZMENU_EDIT_NEW_POINT))
            Zone_NewZonePoint(param1);
        else if (StrEqual(info, ZMENU_EDIT_BOX))
        {
            if (IS_EDITING_BOX(param1))
                Zone_EndBox(param1);
            else
                Zone_BeginBox(param1);
            
            ResetClientZoneEditMenu(param1);
            return;
        }
        else if (StrEqual(info, ZMENU_EDIT_REMOVE_LAST))
            Zone_RemoveLastPoint(param1);
        else if (StrEqual(info, ZMENU_EDIT_CLEAR_ALL))
            Zone_ClearAllPoints(param1);
        else if (StrEqual(info, ZMENU_EDIT_SET_FLAG))
            Zone_SetFlag(param1);
        else if (StrEqual(info, ZMENU_EDIT_SET_NAME))
        {
            decl String:reason[32];
            
            new zone_id = g_ClientEditZone[param1];
            if (zone_id == INVALID_ZONE)
                Reply(param1, "You're not currently editing a zone. Use sm_newzone or /zonemenu to begin editing.");
            else
            {
                g_ClientEditZoneMenu[param1] = menu;
                Format(STR(reason), "set zone %d name", zone_id);
                BeginTextEntry(param1, reason, TEXT_ENTRY_ZONE_NAME, false);
            }
        }
        else if (StrEqual(info, ZMENU_EDIT_FINISH))
        {
            Zone_FinishZone(param1);
            PrevSubmenu(param1);
            return;
        }
        
        DisplayMenuAtItem(menu, param1, GetMenuSelectionPosition(), MENU_TIME_FOREVER);
    }
    else if (action == MenuAction_Cancel)
    {
        // Somehow the menu is cancelled when its handle is closed, which leads
        // to double-popping when PrevSubmenu is called upon changing zone name
        // So we won't handle the cancel (calling PrevSubmenu) in that case
        if (g_ClientEditZoneMenu[param1] != INVALID_HANDLE)
            g_ClientEditZoneMenu[param1] = INVALID_HANDLE;
        else
            HANDLE_CANCEL();
    }
}


/* ========================================================================
                        MAP MANIPULATION FUNCTIONS
   ======================================================================== */

Map_SetTier(client, tier)
{
    g_CurMapTier = tier;
    win_SQL_UpdateMapTier(client, tier);
}


/* ========================================================================
                       TRACK MANIPULATION FUNCTIONS
   ======================================================================== */

Track_SetTier(client, track, tier)
{
    g_TrackTier[track] = tier;
    
    win_SQL_UpdateTrackTier(client, track, tier);
}

Track_SetName(client, track, const String:name[])
{
    strcopy(g_TrackNames[track], sizeof(g_TrackNames[]), name);
    
    win_SQL_UpdateTrack(client, track);
}


/* ========================================================================
                        ZONE MANIPULATION FUNCTIONS
   ======================================================================== */

// For debugging zones
stock PrintZone(zone_id, track=MAIN_TRACK)
{
    LogMessage("ZONE %d, %sEMPTY", zone_id, ZONE_EMPTY_T(track,zone_id) ? "" : "NOT ");
    
    for (new i=0; i<POINTS_LENGTH; i++)
    {
        if (i == 0)
            LogMessage("POINT %d = { %11d %11d %11d }", i,
                ZONE_POINTS_T[track][zone_id], ZONE_HEIGHT_T[track][zone_id], ZONE_EDITING_T[track][zone_id]);
        else
            LogMessage("POINT %d = { %11.6f %11.6f %11.6f }", i,
                ZONE_POINT_T[track][zone_id][i][0], ZONE_POINT_T[track][zone_id][i][1], ZONE_POINT_T[track][zone_id][i][2]);
    }
    
    LogMessage("\n");
}

WipeZoneCache()
{
    for (new track=0; track<MAX_TRACKS; track++)
        WipeTrackCache(track);
}

WipeTrackCache(track)
{
    for (new i=0; i<MAX_ZONES; i++)
    {
        for (new j=0; j<POINTS_LENGTH; j++)
        {
            g_ZonesCache[track][i][j][0] = 0.0;
            g_ZonesCache[track][i][j][1] = 0.0;
            g_ZonesCache[track][i][j][2] = 0.0;
        }
        
        g_ZoneIDs[track][i] = -1;
        g_ZoneNames[track][i][0] = '\0';
        g_TrackPaths[track][i] = 0;
    }
}

WipeTrackPath(track)
{
    for (new zone=0; zone<MAX_ZONES; zone++)
        g_TrackPaths[track][zone] = 0;
}

CountTracks()
{
    g_TracksCount = 0;
    for (new track=0; track<MAX_TRACKS; track++)
        if (g_ZonesCount[track] > 0)
            g_TracksCount++;
}

CountZones(track)
{
    new zones_count = 0;
    for (new i=0; i<MAX_ZONES; i++)
    {
        if (!ZONE_EMPTY_T(track,i))
            zones_count++;
    }
    
    return zones_count;
}

CountZonePoints(zone_id, track)
{
    ZONE_POINTS_T[track][zone_id] = 0;
    
    for (new i=ZONE_FIRST_POINT; i<POINTS_LENGTH; i++)
    {
        if (!ZONE_POINT_EMPTY_T(track,zone_id,i))
            ZONE_POINTS_T[track][zone_id]++;
    }
}

GetNextNewZone(track=MAIN_TRACK)
{
    for (new i=0; i<MAX_ZONES; i++)
    {
        if (ZONE_EMPTY_T(track,i))
            return i;
    }
    
    return -1;
}

FindEndZone(track=MAIN_TRACK)
{
    for (new i=0; i<MAX_ZONES; i++)
    {
        if (!ZONE_EMPTY_T(track,i))
            g_EndZone[track] = i;
    }
}

GetZoneCenter(zone_id, Float:vec[3], track=MAIN_TRACK)
{
    if (ZONE_POINTS_T[track][zone_id] == 0)
        return;
    else if (ZONE_POINTS_T[track][zone_id] == 1)
        vec = g_ZonesCache[track][zone_id][ZONE_FIRST_POINT];
    else
    {
        new Float:sum[3];
        
        for (new i=ZONE_FIRST_POINT; i<ZONE_META_POINTS + ZONE_POINTS_T[track][zone_id]; i++)
        {
            sum[0] += ZONE_POINT_T[track][zone_id][i][0];
            sum[1] += ZONE_POINT_T[track][zone_id][i][1];
            sum[2] += ZONE_POINT_T[track][zone_id][i][2];
        }
        
        vec[0] = sum[0] / ZONE_POINTS_T[track][zone_id];
        vec[1] = sum[1] / ZONE_POINTS_T[track][zone_id];
        vec[2] = sum[2] / ZONE_POINTS_T[track][zone_id];
    }
}

// Set's the flag location at the center of all the points
SetZoneFlagCenter(zone_id, track=MAIN_TRACK)
{
    GetZoneCenter(zone_id, g_ZonesCache[track][zone_id][ZONE_FLAG_POINT], track);
}

FindTrackPath(track)
{
    new next_path = 0;
    for (new zone=1; zone<MAX_ZONES; zone++)
        if (!ZONE_EMPTY_T(track, zone))
            g_TrackPaths[track][next_path++] = zone;
    
    // Stick an empty delimiter on the end, for good measure
    g_TrackPaths[track][next_path] = 0;
}

Zone_BeginNewZone(client, zone_id=ZONE_NEXT_NEW_ZONE, track=MAIN_TRACK)
{
    if (zone_id == ZONE_NEXT_NEW_ZONE)
    {
        zone_id = GetNextNewZone(track);
        if (zone_id >= MAX_ZONES)
        {
            Reply(client, "You've hit the maximum number of zones ({T}%d{N}).", MAX_ZONES);
            return;
        }
    }
    
    if (zone_id >= MAX_ZONES || zone_id < 0)
    {
        Reply(client, "That zone index ({T}%d{N}) is invalid.", zone_id);
        return;
    }
    
    if (ZONE_EMPTY_T(track,zone_id))
        g_ZonesCount[track]++;
    
    g_ClientEditZone[client] = zone_id;
    g_ClientEditTrack[client] = track;
    ZONE_HEIGHT_T[track][zone_id] = DEFAULT_ZONE_HEIGHT;
    ZONE_EDITING_T[track][zone_id] = true;
    
    win_SQL_UpsertZone(zone_id, track);
}

Zone_NewZonePoint(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    new point = ZONE_POINTS_T[track][zone_id] + ZONE_META_POINTS;
    if (point >= POINTS_LENGTH)
    {
        Reply(client, "You've hit the maximum number of zone points ({T}%d{N}).", MAX_POINTS);
        return;
    }
    
    GetClientAbsOrigin(client, g_ZonesCache[track][zone_id][point]);
    ZONE_POINTS_T[track][zone_id]++;
    
    Reply(client, "New zone {T}%d{N} (track {T}%d{N}) point at ({OG}%f{N}, {OG}%f{N}, {OG}%f{N})", zone_id, track,
        ZONE_POINT_T[track][zone_id][point][0],
        ZONE_POINT_T[track][zone_id][point][1],
        ZONE_POINT_T[track][zone_id][point][2]);
}

Zone_BeginBox(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    if (IS_EDITING_BOX(client))
    {
        Reply(client, "You already have a box started. Use {G}sm_endbox{N} to complete it.");
        return;
    }
    
    GetClientAbsOrigin(client, g_ClientEditBox[client]);
    
    Reply(client, "Began zone {T}%d{N} (track {T}%d{N}) box at ({OG}%f{N}, {OG}%f{N}, {OG}%f{N})",
        zone_id, track,
        g_ClientEditBox[0],
        g_ClientEditBox[1],
        g_ClientEditBox[2]);
}

Zone_EndBox(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    if (!IS_EDITING_BOX(client))
    {
        Reply(client, "You're not currently creating a box. Use {G}sm_beginbox{N} to start.");
        return;
    }
    
    // Clear the zone's points
    for (new i=ZONE_FIRST_POINT; i<POINTS_LENGTH; i++)
    {
        g_ZonesCache[track][zone_id][i][0] = 0.0;
        g_ZonesCache[track][zone_id][i][1] = 0.0;
        g_ZonesCache[track][zone_id][i][2] = 0.0;
    }
    ZONE_POINTS_T[track][zone_id] = 0;
    
    // Grab the end point vector
    decl Float:end[3];
    GetClientAbsOrigin(client, end);
    
    new Float:length = end[0] - g_ClientEditBox[client][0];
    new Float:width = end[1] - g_ClientEditBox[client][1];
    new Float:height = end[2] - g_ClientEditBox[client][2];
    
    new point = ZONE_META_POINTS;
    
    /* Overhead view:
     * Begin
     *   A ----- B
     *   |       |
     *   |       |
     *   D ----- C
     *          End
     */
    
    // Point A
    ZONE_POINT_T[track][zone_id][point][0] = g_ClientEditBox[client][0];
    ZONE_POINT_T[track][zone_id][point][1] = g_ClientEditBox[client][1];
    ZONE_POINT_T[track][zone_id][point][2] = g_ClientEditBox[client][2];
    ZONE_POINTS_T[track][zone_id]++;
    point++;
    
    // Point B
    ZONE_POINT_T[track][zone_id][point][0] = g_ClientEditBox[client][0] + length;
    ZONE_POINT_T[track][zone_id][point][1] = g_ClientEditBox[client][1];
    ZONE_POINT_T[track][zone_id][point][2] = g_ClientEditBox[client][2];
    ZONE_POINTS_T[track][zone_id]++;
    point++;
    
    // Point C
    ZONE_POINT_T[track][zone_id][point][0] = end[0];
    ZONE_POINT_T[track][zone_id][point][1] = end[1];
    ZONE_POINT_T[track][zone_id][point][2] = g_ClientEditBox[client][2];
    ZONE_POINTS_T[track][zone_id]++;
    point++;
    
    // Point D
    ZONE_POINT_T[track][zone_id][point][0] = g_ClientEditBox[client][0];
    ZONE_POINT_T[track][zone_id][point][1] = g_ClientEditBox[client][1] + width;
    ZONE_POINT_T[track][zone_id][point][2] = g_ClientEditBox[client][2];
    ZONE_POINTS_T[track][zone_id]++;
    
    // Set the height
    ZONE_HEIGHT_T[track][zone_id] = RoundToFloor(height);
    
    // Clear the start point for the box, resetting IS_EDITING_BOX
    g_ClientEditBox[client][0] = 0.0;
    g_ClientEditBox[client][1] = 0.0;
    g_ClientEditBox[client][2] = 0.0;
    
    Reply(client, "Completed zone {T}%d{N} (track {T}%d{N}) box.", zone_id, track);
}

Zone_ZoneHeight(client, height)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    ZONE_HEIGHT_T[track][zone_id] = height;
    
    Reply(client, "Set zone {T}%d{N} (track {T}%d{N}) max height at {OG}%d{N} map units.", zone_id, track, height);
}

Zone_SetFlag(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    GetClientAbsOrigin(client, g_ZonesCache[track][zone_id][ZONE_FLAG_POINT]);
    
    Reply(client, "Set zone {T}%d{N} (track {T}%d{N}) flag at point ({OG}%f{N}, {OG}%f{N}, {OG}%f{N})", zone_id, track,
        ZONE_FLAG_T[track][zone_id][0], ZONE_FLAG_T[track][zone_id][1], ZONE_FLAG_T[track][zone_id][2]);
}

Zone_SetName(client, const String:name[])
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    strcopy(g_ZoneNames[track][zone_id], sizeof(g_ZoneNames[][]), name);
    
    Reply(client, "Set zone {T}%d{N} (track {T}%d{N}) name to \"{OG}%s{N}\"", zone_id, track, g_ZoneNames[track][zone_id]);
}

Zone_RemoveLastPoint(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    new point = (ZONE_POINTS_T[track][zone_id] - 1) + ZONE_META_POINTS;
    if (point == 0)
    {
        Reply(client, "There are {OG}no points{N} to remove.");
        return;
    }
    
    g_ZonesCache[track][zone_id][point][0] = 0.0;
    g_ZonesCache[track][zone_id][point][1] = 0.0;
    g_ZonesCache[track][zone_id][point][2] = 0.0;
    ZONE_POINTS_T[track][zone_id]--;
}

Zone_ClearAllPoints(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    for (new i=ZONE_FIRST_POINT; i<POINTS_LENGTH; i++)
    {
        g_ZonesCache[track][zone_id][i][0] = 0.0;
        g_ZonesCache[track][zone_id][i][1] = 0.0;
        g_ZonesCache[track][zone_id][i][2] = 0.0;
    }
    
    ZONE_POINTS_T[track][zone_id] = 0;
    
    Reply(client, "Cleared all points in zone {T}%d{N} (track {T}%d{N}).", zone_id, track);
}

Zone_FinishZone(client)
{
    new zone_id = g_ClientEditZone[client];
    new track = g_ClientEditTrack[client];
    if (zone_id == INVALID_ZONE)
        NOT_EDITING_RETURN()
    
    ZONE_EDITING_T[track][zone_id] = false;
    g_ClientEditZone[client] = INVALID_ZONE;
    g_ClientEditTrack[client] = INVALID_TRACK;
    
    if (ZONE_FLAG_EMPTY_T(track,zone_id))
        SetZoneFlagCenter(zone_id, track);
    
    CountZonePoints(zone_id, track);
    FindEndZone(track);
    FindTrackPath(track);
    CountTracks();
    
    SetZoneMenuTitle();
    BuildTrackMenu();
    
    win_SQL_UpsertZone(zone_id, track);
    win_SQL_ClearZonePoints(zone_id, track);
    win_SQL_InsertZonePoints(zone_id, track);
}

Zone_RemoveZone(client, zone_id, track=MAIN_TRACK)
{
    for (new i=0; i<POINTS_LENGTH; i++)
    {
        g_ZonesCache[track][zone_id][i][0] = 0.0;
        g_ZonesCache[track][zone_id][i][1] = 0.0;
        g_ZonesCache[track][zone_id][i][2] = 0.0;
    }
    
    FindEndZone(track);
    FindTrackPath(track);
    CountTracks();
    BuildTrackMenu();
    
    win_SQL_DeleteZone(zone_id, track);
    
    Reply(client, "Removed zone {T}%d{N} (track {T}%d{N}).", zone_id, track);
}

Zone_ClearTrack(client, track)
{
    WipeTrackCache(track);
    
    g_ZonesCount[track] = 0;
    g_EndZone[track] = 0;
    SetZoneMenuTitle();
    
    FindEndZone(track);
    WipeTrackPath(track);
    CountTracks();
    BuildTrackMenu();
    
    win_SQL_ClearTrackZonePoints(track);
    win_SQL_ClearTrackZones(track);
    
    Reply(client, "Cleared track {T}%d{N}! I feel lighter already.", track);
}

Zone_ClearAllZones(client)
{
    WipeZoneCache();
    
    for (new track=0; track<MAX_TRACKS; track++)
    {
        g_ZonesCount[track] = 0;
        g_EndZone[track] = 0;
        
        WipeTrackPath(track);
    }
    
    SetZoneMenuTitle();
    BuildTrackMenu();
    
    win_SQL_ClearAllZonePoints();
    win_SQL_ClearAllZones();
    
    Reply(client, "{T}Cleared all zones{N}! Phew, that's a load off.");
}

// Inserts/updates all the zones in the cache to the database
Zone_SyncToDB(client)
{
    if (g_ZoneCacheInProgress)
    {
        Reply(client, "There is a {OG}zone reload{N} in progress. If a sync occurs during a reload, data loss can occur.");
        Reply(client, "Please wait till the zone reload completes.");
        return;
    }
    
    if (g_ZoneSyncInProgress)
    {
        Reply(client, "There is already a {OG}zone sync{N} in progress. Please wait till it completes.");
        return;
    }
    
    g_ZoneSyncInProgress = true;
    g_ZoneSyncNotifyClient = client;
    g_ZoneSyncReplySource = GetCmdReplySource();
    g_TracksSynced = 0;
    g_TotalZonesSynced = 0;
    
    for (new track=0; track<MAX_TRACKS; track++)
    {
        new bool:track_empty = true;
        g_ZonesSynced[track] = 0;
        
        for (new i=0; i<MAX_ZONES; i++)
        {
            if (ZONE_EMPTY_T(track,i))
                continue;
            
            track_empty = false;
            
            win_SQL_UpsertZone(i, track);
            win_SQL_ClearZonePoints(i, track);
            win_SQL_InsertZonePoints(i, track);
            
            g_ZonesSynced[track]++;
            g_TotalZonesSynced++;
            
            // This will totally cause race conditions, as I'm assuming the
            // InsertZonePoints callback will complete after the last (or at least
            // next) iteration. This variable is used to check the last zone synced
            // in the OnPointsSyncFinished forward to reset g_ZoneSyncInProgress.
            g_ZoneSyncLastZone = i;
        }
        
        if (!track_empty)
        {
            g_TracksSynced++;
            g_ZoneSyncLastTrack = track;
        }
    }
    
    // We don't want to freeze later syncs because we forgot to turn off the in-progress indicator
    if (g_TotalZonesSynced == 0)
    {
        g_ZoneSyncInProgress = false;
        Reply(client, "There are no zones to sync! Use {T}sm_newzone{N} or {T}/zonemenu{N} to create some.");
    }
}

public fwd_Zone_OnPointsSyncFinished(zone_id, track, points)
{
    if (zone_id == g_ZoneSyncLastZone && track == g_ZoneSyncLastTrack)
    {
        g_ZoneSyncInProgress = false;
        g_ZoneSyncLastZone = -1;
    
        if (g_ZoneSyncNotifyClient != -1)
        {
            new ReplySource:prev = SetCmdReplySource(g_ZoneSyncReplySource);
            Reply(g_ZoneSyncNotifyClient, "Successfully synced {T}%d{N} zone%s (in {T}%d{N} track%s) to the database!", g_TotalZonesSynced, g_TotalZonesSynced == 1 ? "" : "s", g_TracksSynced, g_TracksSynced == 1 ? "" : "s");
            SetCmdReplySource(prev);
            
            g_ZoneSyncNotifyClient = -1;
        }
    }
    else if (g_ZoneSyncNotifyClient != -1)
    {
        new ReplySource:prev = SetCmdReplySource(g_ZoneSyncReplySource);
        Reply(g_ZoneSyncNotifyClient, "Synced zone {T}%d{N} (id {OG}%d{N}) of {T}%d{N}, track {T}%d{N} (id {OG}%d{N}) of {T}%d{N}...", zone_id+1, zone_id, g_ZonesSynced[track], track+1, track, g_TracksSynced);
        SetCmdReplySource(prev);
    }
}

// Grabs all the zones from the database and puts them in the cache
Zone_ReloadFromDB(client)
{
    if (g_ZoneCacheInProgress)
    {
        Reply(client, "There is already a {OG}zone reload{N} in progress. Please wait till it completes");
        return;
    }
    
    if (g_ZoneSyncInProgress)
    {
        Reply(client, "There is a {OG}zone sync{N} in progress. If a reload occurs during a sync, data loss can occur.");
        Reply(client, "Please wait till the zone sync completes.");
        return;
    }
    
    CacheZones(client);
}


/* ========================================================================
                          ZONE DISPLAY/TEMPENTS
   ======================================================================== */


DrawFlag(zone_id, client, const color[4]=FLAG_COLOR_ZONE, track=MAIN_TRACK)
{
    /**
     *  
     * Z  |\        ---
     * o  |   \      |
     * n  |      \   | 5/6 of zone height
     * e  |      /   |
     *    |   /      |
     * H  |/        ---
     * e  |
     * i  |
     * g  |
     * h  |
     * t  |
     *    |
     */
    
    #define BEAM(%1,%2) { \
        TE_SetupBeamPoints(%1, %2, g_mdlBeam, g_mdlBeam, 0, 0, 0.2, 1.0, 10.0, 10, 0.0, color, 0); \
        TE_SendToClient(client); \
    }
    
    if (ZONE_FLAG_EMPTY_T(track,zone_id))
        return;
    
    new Float:flag_pole_height = MAX(float(ZONE_HEIGHT_T[track][zone_id]), MIN_FLAG_HEIGHT);
    new Float:flag_height = flag_pole_height * (1.0 / 6.0);
    
    new Float:vec_a[3];
    new Float:vec_b[3];
    
    vec_a[0] = ZONE_FLAG_T[track][zone_id][0];
    vec_a[1] = ZONE_FLAG_T[track][zone_id][1];
    vec_a[2] = ZONE_FLAG_T[track][zone_id][2] + flag_pole_height;
    BEAM(g_ZonesCache[track][zone_id][ZONE_FLAG_POINT], vec_a)
    
    vec_b[0] = vec_a[0] + (Cosine(g_FlagRotation[track][zone_id]) * flag_height * 2.0);
    vec_b[1] = vec_a[1] + (Sine(g_FlagRotation[track][zone_id]) * flag_height * 2.0);
    vec_b[2] = vec_a[2] - flag_height / 2.0;
    BEAM(vec_a, vec_b)
    
    vec_a[2] -= flag_height;
    BEAM(vec_b, vec_a)
    
    g_FlagRotation[track][zone_id] = FloatMod(g_FlagRotation[track][zone_id] + FLAG_ROTATE_STEP, 2 * FLOAT_PI);
}

DrawBox(client, const color[4]=FLAG_COLOR_ZONE)
{
    decl Float:end[3];
    decl Float:vec[6][3];
    GetClientAbsOrigin(client, end);
    
    new Float:length = end[0] - g_ClientEditBox[client][0];
    new Float:width = end[1] - g_ClientEditBox[client][1];
    new Float:height = end[2] - g_ClientEditBox[client][2];
    
    // Vertex A
    vec[0][0] = g_ClientEditBox[client][0];
    vec[0][1] = g_ClientEditBox[client][1];
    vec[0][2] = g_ClientEditBox[client][2];
    
    // Vertex B
    vec[1][0] = g_ClientEditBox[client][0] + length;
    vec[1][1] = g_ClientEditBox[client][1];
    vec[1][2] = g_ClientEditBox[client][2];
    
    // Vertex C
    vec[2][0] = end[0];
    vec[2][1] = end[1];
    vec[2][2] = g_ClientEditBox[client][2];
    
    // Vertex D
    vec[3][0] = g_ClientEditBox[client][0];
    vec[3][1] = g_ClientEditBox[client][1] + width;
    vec[3][2] = g_ClientEditBox[client][2];
    
    for (new j=0, nj=1; j<4; j++, nj=(j+1)%4)
    {
        // Draw the ground rectangle
        BEAM(vec[j], vec[nj])
        
        vec[4][0] = vec[j][0];
        vec[4][1] = vec[j][1];
        vec[4][2] = vec[j][2] + height;
        
        // Draw the vertical edges
        BEAM(vec[j], vec[4])
        
        vec[5][0] = vec[nj][0];
        vec[5][1] = vec[nj][1];
        vec[5][2] = vec[nj][2] + height;
        
        // Draw the top rectangle
        BEAM(vec[4], vec[5])
    }
}

DrawZone(zone_id, client, const color[4]=FLAG_COLOR_ZONE, track=MAIN_TRACK)
{
    #if defined(BEAM)
        #undef BEAM
    #endif
    #define BEAM(%1,%2) { \
        TE_SetupBeamPoints(%1, %2, g_mdlBeam, g_mdlBeam, 0, 0, 0.2, 4.0, 4.0, 10, 0.0, color, 0); \
        TE_SendToClient(client); \
    }
    
    // We can only draw if there are 2 or more points
    if (ZONE_POINTS_T[track][zone_id] <= 1)
        return;
        
    decl i;
    for (i=ZONE_FIRST_POINT+1; i<ZONE_POINTS_T[track][zone_id] + ZONE_META_POINTS; i++)
        BEAM(g_ZonesCache[track][zone_id][i-1], g_ZonesCache[track][zone_id][i])
    
    // Connect first and last point (if 3 or more points)
    if (i > ZONE_META_POINTS + 1)
        BEAM(g_ZonesCache[track][zone_id][i-1], g_ZonesCache[track][zone_id][ZONE_FIRST_POINT])
}

public Action:Timer_DrawZones(Handle:timer)
{
    // In order to stop a flurry of error messages, we won't draw anything
    // if TempEnts are not supported.
    if (!g_wintoxTempEnts)
        return;
    
    for (new client=1; client<MaxClients; client++)
    {
        if (!IsClientConnected(client) || !IsClientInGame(client) || IsFakeClient(client))
            continue;
        
        new cur_zone = MAX(g_PlayerCurrentZone[client], 0);
        new cur_track = g_PlayerCurrentTrack[client];
        
        if (!g_DrawFlags)
        {
            // Draw start zone flag
            DrawFlag(0, client, FLAG_COLOR_START, cur_track);
            
            // Draw current and next zone flags
            for (new i=cur_zone; i<=cur_zone+1; i++)
                if (i != 0 && !IS_END_ZONE_T(cur_track,i))
                    DrawFlag(i, client, FLAG_COLOR_ZONE, cur_track);
            
            // Draw end flag
            if (g_EndZone[cur_track] != 0)
                DrawFlag(g_EndZone[cur_track], client, FLAG_COLOR_END, cur_track);
            
            if (g_DrawBonusFlags)
                for (new track=1; track<MAX_TRACKS; track++)
                    if (g_ZonesCount[track] > 0 && cur_track != track)
                        DrawFlag(0, client, FLAG_COLOR_BONUS_START, track);
        }
        
        if (g_DrawZones || g_DrawFlags)
        {
            for (new track=0; track<MAX_TRACKS; track++)
            {
                for (new i=0; i<MAX_ZONES; i++)
                {
                    if (g_DrawFlags)
                        DrawFlag(i, client, FLAG_COLOR(i), track);
                    
                    if (!g_DrawZones)
                        continue;
                    
                    // Only draw lines, so no less than 2 points
                    if (ZONE_POINTS_T[track][i] <= 1)
                        continue;
                    
                    DrawZone(i, client, FLAG_COLOR(i), track);
                }
            }
        }
        
        if (g_ClientEditZone[client] != INVALID_ZONE)
        {
            new zone_id = g_ClientEditZone[client];
            new track = g_ClientEditTrack[client];
            
            if (IS_EDITING_BOX(client))
                DrawBox(client, FLAG_COLOR(zone_id));
            else if (!g_DrawZones)
                DrawZone(zone_id, client, FLAG_COLOR(zone_id), track);
            
            // Don't draw a flag twice
            if (zone_id != 0 && zone_id != g_EndZone[track] &&
                zone_id != cur_zone && zone_id != cur_zone+1 || cur_track != track)
                DrawFlag(zone_id, client, FLAG_COLOR(zone_id), track);
        }
    }
}


/* ========================================================================
                          PLAYER-ZONE INTERACTION
   ======================================================================== */

ZoneTeleport(client, zone_id, track=MAIN_TRACK)
{
    if (ZONE_FLAG_EMPTY(zone_id))
    {
        decl Float:tele[3];
        GetZoneCenter(zone_id, tele, track);
        TeleportEntity(client, tele, NULL_VECTOR, NULL_VECTOR);
    }
    else
        TeleportEntity(client, g_ZonesCache[track][zone_id][ZONE_FLAG_POINT], NULL_VECTOR, NULL_VECTOR);
}


/* ========================================================================
                            SQL SHIT. GURGLE.
   ======================================================================== */

// Grabs all the zones and their points for the current map from the database
// and caches them in the local static arrays.
CacheZones(const client=-1)
{
    decl String:query[256];
    
    g_ZoneCacheInProgress = true;
    
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
            Reply(client, "%s", buffer);
        
        g_ZoneCacheInProgress = false;
        return;
    }
    
    rows = SQL_GetRowCount(hndl);
    if (rows == 0)
    {
        if (client != -1)
        {
            Reply(client, "Successfully retrieved zones from DB, but there were {OG}none{N} :'(");
            Reply(client, "Use sm_newzone%s to create some.", client == 0 ? "" : " or /zonemenu");
        }
        
        g_ZoneCacheInProgress = false;
        return;
    }
    
    WipeZoneCache();
    
    // Reset zone counts to 0 and wipe track paths
    for (new track=0; track<MAX_TRACKS; track++)
    {
        g_ZonesCount[track] = 0;
        WipeTrackPath(track);
    }
    
    // Cache zones
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl zone_dbid;
        decl zone_id;
        decl String:zone_name[64];
        
        zone_dbid = SQL_FetchInt(hndl, 0);
        zone_id = SQL_FetchInt(hndl, 1);
        SQL_FetchString(hndl, 2, STR(zone_name));
        track = SQL_FetchInt(hndl, 3);
        
        g_ZoneIDs[track][zone_id] = zone_dbid;
        strcopy(g_ZoneNames[track][zone_id], sizeof(g_ZoneNames[][]), zone_name);
        
        g_ZonesCount[track]++;
    }
    
    // Rebuild the track menu to display the new number of zones
    BuildTrackMenu();
    
    if (client != -1)
        Reply(client, "Successfully retrieved {OG}%d{N} rows of zones from DB! Getting zone points...", rows);
    
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
            Reply(client, "%s", buffer);
        
        g_ZoneCacheInProgress = false;
        return;
    }
    
    rows = SQL_GetRowCount(hndl);
    if (rows == 0)
    {
        if (client != -1)
            Reply(client, "Successfully retrieved zone points from DB, but there were {OG}none{N}.");
        
        g_ZoneCacheInProgress = false;
        return;
    }
    
    // Cache zone points
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl zone_id;
        decl zp_number;
        decl Float:x, Float:y, Float:z;
        
        zone_id = SQL_FetchInt(hndl, 5);
        zp_number = SQL_FetchInt(hndl, 1);
        x = SQL_FetchFloat(hndl, 2);
        y = SQL_FetchFloat(hndl, 3);
        z = SQL_FetchFloat(hndl, 4);
        track = SQL_FetchInt(hndl, 6);
        
        if (zp_number == 0)
        {
            x = Float:RoundToFloor(x);
            y = Float:RoundToFloor(y);
            z = Float:RoundToFloor(z);
        }
        
        g_ZonesCache[track][zone_id][zp_number][0] = x;
        g_ZonesCache[track][zone_id][zp_number][1] = y;
        g_ZonesCache[track][zone_id][zp_number][2] = z;
    }
    
    // Don't trust the database. It's not _one of us_
    g_TracksCount = 0;
    for (new track=0; track<MAX_TRACKS; track++)
    {
        for (new i=0; i<MAX_ZONES; i++)
            CountZonePoints(i, track);
        
        SetZoneMenuTitle();
        FindEndZone(track);
        
        if (g_ZonesCount[track])
        {
            FindTrackPath(track);
            g_TracksCount++;
        }
    }
    
    g_ZoneCacheInProgress = false;
    if (client != -1)
        Reply(client, "Successfully retrieved {T}%d{N} rows of zone points from DB!", rows);
}

CacheTracks(const client=-1)
{
    SQLTH(GetTracks, client, g_CurMapID);
}

public T_GetTracks(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new client = data;
    
    if (hndl == INVALID_HANDLE)
    {
        decl String:error_msg[256];
        Format(STR(error_msg), "Error retrieving tracks: %s", error);
        
        LogError("%s", error_msg);
        if (client != -1)
            Reply(client, "%s", error_msg);
        
        return;
    }
    
    new tracks = 0;
    while (SQL_FetchRow(hndl))
    {
        decl track;
        decl tier;
        decl String:name[64];
        decl zones;
        
        track = SQL_FetchInt(hndl, 0);
        tier = SQL_FetchInt(hndl, 1);
        SQL_FetchString(hndl, 2, STR(name));
        zones = SQL_FetchInt(hndl, 3);
        
        g_TrackTier[track] = tier;
        strcopy(g_TrackNames[track], sizeof(g_TrackNames[]), name);
        g_ZonesCount[track] = zones;
        
        tracks++;
    }
    
    if (client != -1)
        Reply(client, "Successfully retrieved {G}%d{N} track%s!", tracks, tracks == 1 ? "" : "s");
}

win_SQL_UpsertZone(zone_id, track)
{
    decl String:buffer[256];
    decl String:esc_name[256];
    new String:sz_zone_dbid[16] = "NULL";
    
    if (g_ZoneIDs[track][zone_id] != -1)
        IntToString(g_ZoneIDs[track][zone_id], STR(sz_zone_dbid));
    
    SQL_EscapeString(g_Database, g_ZoneNames[track][zone_id], STR(esc_name));
    Format(STR(buffer), SQL_QUERY[InsertZone], sz_zone_dbid, g_CurMapID, track, zone_id, esc_name);
    
    CREATE_ZONE_TRACK_PACK();
    SQLT(buffer, T_UpsertZone, pack);
}

public T_UpsertZone(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    READ_TRACK_PACK();
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error upserting zone %d (track %d) to database: %s", zone_id, track, error);
        return;
    }
    
    g_ZoneIDs[track][zone_id] = SQL_GetInsertId(hndl);
    
    win_SQL_UpdateTrack(-1, track);
}

win_SQL_DeleteZone(zone_id, track)
{
    decl String:query[256];
    Format(STR(query), SQL_QUERY[DeleteZone], g_CurMapID, track, zone_id);
    
    CREATE_ZONE_TRACK_PACK();
    SQLT(query, T_DeleteZone, pack);
}

public T_DeleteZone(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    READ_TRACK_PACK();
    if (hndl == INVALID_HANDLE)
        LogError("Error deleting zone %d (track %d) from database: %s", zone_id, track, error);
}

win_SQL_InsertZonePoints(zone_id, track)
{
    new zone_dbid = g_ZoneIDs[track][zone_id];
    
    if (zone_dbid == -1)
    {
        decl String:buffer[256];
        Format(STR(buffer), SQL_QUERY[GetZone], g_CurMapID, track, zone_id);
        
        CREATE_ZONE_TRACK_PACK();
        SQLT(buffer, T_GetZoneDBIDForInsertZonePoints, pack);
    }
    else
        __win_SQL_InsertZonePoints(zone_id, zone_dbid, track);
}

public T_GetZoneDBIDForInsertZonePoints(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    READ_TRACK_PACK();
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving zone %d (track %d) DB ID for points insertion: %s", zone_id, track, error);
        return;
    }
    
    if (!SQL_FetchRow(hndl))
        return;
    
    new zone_dbid = SQL_FetchInt(hndl, 0);
    __win_SQL_InsertZonePoints(zone_id, zone_dbid, track);
}

__win_SQL_InsertZonePoints(zone_id, zone_dbid, track)
{
    // Just a last check to make sure our points count is accurate before it's final
    CountZonePoints(zone_id, track);
    
    for (new zp_number=0; zp_number < (ZONE_POINTS_T[track][zone_id] + ZONE_META_POINTS); zp_number++)
    {
        decl String:buffer[256];
        
        new Float:x = g_ZonesCache[track][zone_id][zp_number][0];
        new Float:y = g_ZonesCache[track][zone_id][zp_number][1];
        new Float:z = g_ZonesCache[track][zone_id][zp_number][2];
        
        // Need to convert metadata from ints to floats
        if (zp_number == 0)
        {
            x = float(_:x);
            y = float(_:y);
            z = float(_:z);
        }
        
        Format(STR(buffer), SQL_QUERY[InsertZonePoint], zone_dbid, zp_number, x, y, z);
        
        new Handle:pack = CreateDataPack();
        WritePackCell(pack, zone_id);
        WritePackCell(pack, zp_number);
        WritePackCell(pack, track);
        ResetPack(pack);
        
        SQLT(buffer, T_InsertZonePoints, pack);
    }
}

public T_InsertZonePoints(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new Handle:pack = Handle:data;
    new zone_id = ReadPackCell(pack);
    new zp_number = ReadPackCell(pack);
    new track = ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error inserting zone %d (track %d) point %d: %s", zone_id, track, zp_number, error);
        return;
    }
    
    if (zp_number >= (ZONE_POINTS_T[track][zone_id] + ZONE_META_POINTS - 1))
    {
        Call_StartForward(g_fwdPointsSyncFinished);
        Call_PushCell(zone_id);
        Call_PushCell(track);
        Call_PushCell(zp_number + 1);
        Call_Finish();
    }
}

win_SQL_ClearTrackZonePoints(track)
{
    decl String:query[256];
    Format(STR(query), SQL_QUERY[ClearTrackZonePoints], g_CurMapID, track);
    
    SQLT(query, T_ClearTrackZonePoints, track);
}
SQLT_HANDLER(ClearTrackZonePoints, "Error clearing track %d points from database: %s", data, error)

win_SQL_ClearTrackZones(track)
{
    decl String:query[256];
    Format(STR(query), SQL_QUERY[ClearTrackZones], g_CurMapID, track);
    
    SQLT(query, T_ClearTrackZones, track);
}
SQLT_HANDLER(ClearTrackZones, "Error clearing track %d zones from database: %s", data, error)

win_SQL_ClearZonePoints(zone_id, track)
{
    SQLTH(ClearZonePoints, zone_id, g_CurMapID, track, zone_id);
}
SQLT_HANDLER(ClearZonePoints, "Error clearing zone %d points from database: %s", data, error)

win_SQL_ClearAllZonePoints()
{
    SQLTH(ClearAllZonePoints, INVALID_HANDLE, g_CurMapID);
}
SQLT_HANDLER(ClearAllZonePoints, "Error clearing all zone points from database: %s", error)

win_SQL_ClearAllZones()
{
    SQLTH(ClearAllZones, INVALID_HANDLE, g_CurMapID);
}
SQLT_HANDLER(ClearAllZones, "Error clearing all zones from database: %s")

win_SQL_UpdateTrackTier(client, track, tier)
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, track);
    WritePackCell(pack, tier);
    WritePackCell(pack, _:GetCmdReplySource());
    ResetPack(pack);
    
    decl String:query[512];
    decl String:esc_name[128];
    SQL_EscapeString(g_Database, g_TrackNames[track], STR(esc_name));
    
    Format(STR(query), SQL_QUERY[UpsertTrack], g_CurMapID, track, tier, esc_name, g_ZonesCount[track]);
    SQLT(query, T_UpdateTrackTier, pack);
}

public T_UpdateTrackTier(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new Handle:pack = Handle:data;
    new client = ReadPackCell(pack);
    new track = ReadPackCell(pack);
    new tier = ReadPackCell(pack);
    new ReplySource:reply_to = ReplySource:ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        decl String:error_msg[256];
        Format(STR(error_msg), "Error updating track %d tier to %d: %s", track, tier, error);
        
        LogError("%s", error_msg);
        if (client != -1)
        {
            ENTER_REPLY(reply_to);
            Reply(client, "%s", error_msg);
            EXIT_REPLY();
        }
        
        return;
    }
    
    if (client != -1)
    {
        ENTER_REPLY(reply_to);
        Reply(client, "Successfully set track {G}%d{N} tier to {G}%d{N}!", track, tier);
        EXIT_REPLY();
    }
}

win_SQL_UpdateTrack(client, track)
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, track);
    WritePackCell(pack, _:GetCmdReplySource());
    ResetPack(pack);
    
    decl String:query[512];
    decl String:esc_name[128];
    SQL_EscapeString(g_Database, g_TrackNames[track], STR(esc_name));
    
    Format(STR(query), SQL_QUERY[UpsertTrack], g_CurMapID, track, g_TrackTier[track], esc_name, g_ZonesCount[track]);
    SQLT(query, T_UpdateTrack, pack);
}

public T_UpdateTrack(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new Handle:pack = Handle:data;
    new client = ReadPackCell(pack);
    new track = ReadPackCell(pack);
    new ReplySource:reply_to = ReplySource:ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        decl String:error_msg[256];
        Format(STR(error_msg), "Error updating track %d: %s", track, error);
        
        LogError("%s", error_msg);
        if (client != -1)
        {
            ENTER_REPLY(reply_to);
            Reply(client, "%s", error_msg);
            EXIT_REPLY();
        }
        
        return;
    }
    
    if (client != -1)
    {
        ENTER_REPLY(reply_to);
        Reply(client, "Successfully updated track {G}%d{N}!", track);
        EXIT_REPLY();
    }
}

win_SQL_UpdateMapTier(client, tier)
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, tier);
    WritePackCell(pack, _:GetCmdReplySource());
    ResetPack(pack);
    
    SQLTH(UpdateMapTier, pack, tier, g_CurMapID);
}

public T_UpdateMapTier(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new Handle:pack = Handle:data;
    new client = ReadPackCell(pack);
    new tier = ReadPackCell(pack);
    new ReplySource:reply_to = ReplySource:ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        decl String:error_msg[256];
        Format(STR(error_msg), "Error updating map tier to %d: %s", tier, error);
        
        LogError("%s", error_msg);
        if (client != -1)
        {
            ENTER_REPLY(reply_to);
            Reply(client, "%s", error_msg);
            EXIT_REPLY();
        }
        
        return;
    }
    
    if (client != -1)
    {
        ENTER_REPLY(reply_to);
        Reply(client, "Successfully set map tier to {G}%d{N}!", tier);
        EXIT_REPLY();
    }
}
