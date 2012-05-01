/**
 * Database related calls and SQL queries.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_sql_included
 #endinput
#endif
#define _wintox_sql_included

#include "./include/stocks"

/**
 *  Database structure
 * ====================
 * 
 * TABLE wintox_players
 *   id                 pk
 *   latest_name        VARCHAR(64)
 *   latest_ip          VARCHAR(48)
 *   steamid            VARCHAR(64)
 *   country            VARCHAR(3)          NOTE: only recorded on first join
 * 
 * TABLE wintox_maps
 *   id                 pk
 *   bsp_name           VARCHAR(255),UNIQUE NOTE: filename of map, without ".bsp"
 *   human_name         VARCHAR(255),NULL   NOTE: optional human readable name
 *   tier               TINYINT             NOTE: ranges from 1-6
 *   is_staged          BOOLEAN             NOTE: whether map is linear or staged
 * 
 * TABLE wintox_zones
 *   id                 pk
 *   fk_map             INT,FK(wintox_maps)
 *   track              INT,DEFAULT 0
 *   number             INT
 *   name               VARCHAR(64),NULL
 *   UNIQUE(fk_map, number)
 * 
 * TABLE wintox_zone_points
 *   id                 pk
 *   fk_zone            INT,FK(wintox_zones),UNIQUE
 *   number             INT
 *   x                  FLOAT
 *   y                  FLOAT
 *   z                  FLOAT
 *   UNIQUE(fk_zone, number)
 * 
 * TABLE wintox_zone_records
 *   id                 pk
 *   fk_player          INT,FK(wintox_players)
 *   fk_map             INT,FK(wintox_maps)
 *   fk_zone            INT,FK(wintox_zones)
 *   type               VARCHAR(16)         NOTE: bhop: normal, sideways, w-only
 *   time               FLOAT
 *   date_set           FLOAT
 *   UNIQUE(fk_player, fk_map, fk_zone, type)
 */

enum DatabaseType
{
    DB_Unknown = -1,
    DB_SQLite = 0,
    DB_MySQL
};

new DatabaseType:g_DatabaseType = DB_Unknown;
new Handle:g_cv_DBConfig = INVALID_HANDLE;

new Handle:g_Database = INVALID_HANDLE;
new Handle:g_DatabaseDriver = INVALID_HANDLE;



// ===========================================================================
//   Helper Macros
// ===========================================================================
#define NEW_SQL_QUERY(%1) new String:sql_%1[DatabaseType][]
#define NEW_SQL_QUERY_PARAMS(%1,%2) new String:sql_%1[DatabaseType][]

#define SQL_QUERY[%1] sql_%1[g_DatabaseType]
#define SQLF(%1) SQL_FastQuery(g_Database, (%1))
#define SQLT(%1,%2,%3) SQL_TQuery(g_Database, (%2), (%1), (%3))


// ===========================================================================
//   Table Creation
// ===========================================================================
NEW_SQL_QUERY(CreatePlayers) = {
    "CREATE TABLE IF NOT EXISTS wintox_players ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       latest_name      TEXT NOT NULL, \
       latest_ip        TEXT NOT NULL, \
       steamid          TEXT NOT NULL, \
       country          TEXT NOT NULL \
     );",
    
    ""
};
NEW_SQL_QUERY(CreateMaps) = {
    "CREATE TABLE IF NOT EXISTS wintox_maps ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       bsp_name         TEXT NOT NULL UNIQUE, \
       human_name       TEXT NULL, \
       tier             TINYINT DEFAULT 1 NOT NULL, \
       is_staged        BOOLEAN DEFAULT 0 NOT NULL \
     );",
    
    ""
};
NEW_SQL_QUERY(CreateZones) = {
    "CREATE TABLE IF NOT EXISTS wintox_zones ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       track            INTEGER NOT NULL DEFAULT 0, \
       number           INTEGER NOT NULL, \
       name             TEXT DEFAULT 'Zone' NOT NULL, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id) ON DELETE CASCADE \
       UNIQUE(fk_map, track, number) \
     );",
    
    ""
};
NEW_SQL_QUERY(CreateZonePoints) = {
    "CREATE TABLE IF NOT EXISTS wintox_zone_points ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_zone          INTEGER NOT NULL, \
       number           INTEGER NOT NULL, \
       x                FLOAT NOT NULL, \
       y                FLOAT NOT NULL, \
       z                FLOAT NOT NULL, \
       FOREIGN KEY(fk_zone) REFERENCES wintox_zones(id) ON DELETE CASCADE, \
       UNIQUE(fk_zone, number) \
     );",
    
    ""
};
NEW_SQL_QUERY(CreateZoneRecords) = {
    "CREATE TABLE IF NOT EXISTS wintox_zone_records ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_player        INTEGER NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       fk_zone          INTEGER NOT NULL, \
       type             TEXT DEFAULT NULL, \
       time             FLOAT NOT NULL, \
       date_set         FLOAT NOT NULL, \
       FOREIGN KEY(fk_player) REFERENCES wintox_players(id) ON DELETE CASCADE, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id) ON DELETE CASCADE, \
       FOREIGN KEY(fk_zone) REFERENCES wintox_zones(id) ON DELETE CASCADE, \
       UNIQUE(fk_player, fk_map, fk_zone, type) \
     );",
    
    ""
};

// ===========================================================================
//   Map records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertMap,4) = {
    "INSERT INTO wintox_maps (bsp_name,human_name,tier,is_staged) VALUES('%s','%s','%d','%d');",
    ""
};
NEW_SQL_QUERY_PARAMS(GetMap,1) = {
    "SELECT id,bsp_name,human_name,tier,is_staged FROM `wintox_maps` WHERE `bsp_name` = '%s';",
    ""
};

// ===========================================================================
//   Player records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertPlayer,4) = {
    "INSERT INTO wintox_players (latest_name,latest_ip,steamid,country) VALUES('%s','%s','%s','%s');",
    ""
};
NEW_SQL_QUERY_PARAMS(UpdatePlayer,4) = {
    "UPDATE wintox_players SET latest_name='%s', latest_ip='%s', country='%s' WHERE steamid = '%s';",
    ""
};
NEW_SQL_QUERY_PARAMS(GetPlayer,1) = {
    "SELECT id,latest_name,latest_ip,steamid,country FROM wintox_players WHERE steamid = '%s';",
    ""
};

// ===========================================================================
//   Zones and points
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertZone,5) = {
    "INSERT OR REPLACE INTO wintox_zones (id, fk_map, track, number, name) VALUES(%s, %d, %d, %d, '%s');",
    ""
};
NEW_SQL_QUERY_PARAMS(DeleteZone,2) = {
    "DELETE FROM wintox_zones WHERE fk_map = %d AND number = %d;",
    ""
};
NEW_SQL_QUERY_PARAMS(ClearAllZones,1) = {
    "DELETE FROM wintox_zones WHERE fk_map = %d;",
    ""
};
NEW_SQL_QUERY_PARAMS(ClearAllZonePoints,1) = {
    "DELETE FROM wintox_zone_points WHERE fk_zone IN ( SELECT id FROM wintox_zones WHERE fk_map = %d );",
    ""
};
NEW_SQL_QUERY_PARAMS(ClearZonePoints,3) = {
    "DELETE FROM wintox_zone_points WHERE fk_zone IN ( SELECT id FROM wintox_zones WHERE fk_map = %d AND track = %d AND number = %d );",
    ""
};
NEW_SQL_QUERY_PARAMS(GetZones,1) = {
    "SELECT id,number,name,track FROM wintox_zones WHERE fk_map = %d;",
    ""
};
NEW_SQL_QUERY_PARAMS(GetZone,1) = {
    "SELECT id,name FROM wintox_zones WHERE fk_map = %d AND track = %d AND number = %d;",
    ""
};
NEW_SQL_QUERY_PARAMS(GetZonePoints,1) = {
    "SELECT wzp.id,wzp.number,x,y,z,wz.number,wz.track AS zone_id FROM wintox_zone_points AS wzp JOIN wintox_zones AS wz ON wzp.fk_zone = wz.id WHERE fk_zone IN (SELECT id FROM wintox_zones WHERE fk_map = %d);",
    ""
};
NEW_SQL_QUERY_PARAMS(InsertZonePoint,5) = {
    "INSERT INTO wintox_zone_points (fk_zone, number, x, y, z) VALUES(%d, %d, %f, %f, %f);",
    ""
};

// ===========================================================================
//   Zone records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertZoneRecord,6) = {
    "INSERT INTO wintox_zone_records (fk_map, fk_player, fk_zone, type, time, date_set) VALUES(%d, %d, %d, '%s', %d, %d);",
    ""
};


// SQL helper functions
win_SQL_Init()
{
    g_cv_DBConfig = CreateConVar("wintox_dbconfig", "default", "The database.cfg configuration to use with Wintox");
}

// Because this uses the `wintox_dbconfig` cvar, this must be called from OnConfigsExecuted (or later)
win_SQL_Connect()
{
    decl String:config[256];
    decl String:error[256];
    decl String:driver[64];
    
    GetConVarString(g_cv_DBConfig, STR(config));
    g_Database = SQL_Connect(config, true, STR(error));
    
    if (g_Database == INVALID_HANDLE)
        SetFailState("Error connecting to database config \"%s\": %s", config, error);
    
    g_DatabaseDriver = SQL_ReadDriver(g_Database, STR(driver));
    if (StrEqual(driver, "mysql"))
        g_DatabaseType = DB_MySQL;
    else if (StrEqual(driver, "sqlite"))
        g_DatabaseType = DB_SQLite;
    else
        SetFailState("Unknown DB type \"%s\"", driver);
    
    // Get the product name for display
    SQL_GetDriverProduct(g_DatabaseDriver, STR(driver));
    
    LogMessage("Successfully connected to %s database config \"%s\"", driver, config);
}

win_SQL_CreateTables()
{
    decl String:buffer[256];
    
    #define CREATE_TABLE(%1,%2) if (!SQL_FastQuery(g_Database, SQL_QUERY[%1])) { \
        SQL_GetError(g_Database, STR(buffer)); \
        SetFailState("Error executing %s: %s", (%2), buffer); \
    }
    
    CREATE_TABLE(CreatePlayers, "CreatePlayers")
    CREATE_TABLE(CreateMaps, "CreateMaps")
    CREATE_TABLE(CreateZones, "CreateZones")
    CREATE_TABLE(CreateZonePoints, "CreateZonePoints")
    CREATE_TABLE(CreateZoneRecords, "CreateZoneRecords")
}
