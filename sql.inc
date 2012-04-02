/**
 * Database related calls and SQL queries.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_sql_included
 #endinput
#endif
#define _wintox_sql_included

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
 *   bsp_name           VARCHAR(255)        NOTE: filename of map, without ".bsp"
 *   human_name         VARCHAR(255),NULL   NOTE: optional human readable name
 *   tier               TINYINT             NOTE: ranges from 1-6
 *   is_staged          BOOLEAN             NOTE: whether map is linear or staged
 * 
 * TABLE wintox_zones
 *   id                 pk
 *   fk_map             INT,FK(wintox_maps)
 *   index              INT
 *   name               VARCHAR(255)
 * 
 * TABLE wintox_zone_points
 *   id                 pk
 *   fk_zone            INT,FK(wintox_zones)
 *   x                  FLOAT
 *   y                  FLOAT
 *   z                  FLOAT
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


// SQL Queries
#define SQL_QUERY(%1) new sql_%1[DatabaseType][]

SQL_QUERY(CreatePlayers) = {
    "CREATE TABLE IF NOT EXISTS wintox_players ( \
       id               INTEGER PRIMARY KEY ASC, \
       latest_name      TEXT, \
       latest_ip        TEXT, \
       steamid          TEXT, \
       country          TEXT \
     );",
    
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
    decl String:buffer[256];
    decl String:driver[64];
    
    GetConVarString(g_cv_DBConfig, STR(buffer));
    g_Database = SQL_Connect(buffer, true, STR(buffer));
    
    if (g_Database == INVALID_HANDLE)
        SetFailState("%s", buffer);
    
    g_DatabaseDriver = SQL_ReadDriver(g_Database, STR(driver));
    if (StrEqual(driver, "mysql"))
        g_DatabaseType = DB_MySQL;
    else if (StrEqual(driver, "sqlite"))
        g_DatabaseType = DB_SQLite;
    else
        SetFailState("Unknown DB type \"%s\"", driver);
    
    LogMessage("Successfully connected to DB config \"%s\" of type \"%s\"", buffer, driver);
}

CreateTables()
{
    
}
