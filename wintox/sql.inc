/**
 * SQL queries, DB helper macros, DB connection/create tables
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_sql_included
 #endinput
#endif
#define _wintox_sql_included

#include "./wintox/stocks"

/**
 *  Database structure
 * ====================
 * 
 * TABLE wintox_players
 *   id                 pk
 *   latest_name        VARCHAR(64)
 *   latest_ip          VARCHAR(48)
 *   last_join          INTEGER                 NOTE: timestamp
 *   steamid            VARCHAR(64)
 *   country            VARCHAR(3)              NOTE: only recorded on first join
 *   gametype           VARCHAR(4)              NOTE: surf or bhop
 *   grosspoints        INTEGER DEFAULT 0
 *   racewins           INTEGER DEFAULT 0
 *   racelosses         INTEGER DEFAULT 0
 *   UNIQUE(steamid, gametype)
 * 
 * TABLE wintox_maps
 *   id                 pk
 *   bsp_name           VARCHAR(255)            NOTE: filename of map, without ".bsp"
 *   human_name         VARCHAR(255),NULL       NOTE: optional human readable name
 *   tier               TINYINT                 NOTE: ranges from 1-6
 *   is_staged          BOOLEAN                 NOTE: whether map is linear or staged
 *   gametype           VARCHAR(4)              NOTE: surf or bhop
 *   UNIQUE(bsp_name, gametype)
 * 
 * TABLE wintox_tracks
 *   id                 pk
 *   fk_map             INT,FK(wintox_maps)
 *   track              INTEGER
 *   tier               TINYINT,DEFAULT 1       NOTE: ranges from 1-6
 *   name               VARCHAR(64),DEFAULT ''
 *   zones              INTEGER DEFAULT 0       NOTE: number of zones in track
 *   UNIQUE(fk_map, track)
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
 *   type               VARCHAR(16),DEFAULT ''  NOTE: bhop: normal, sideways, w-only
 *   time               FLOAT
 *   date_set           INTEGER                 NOTE: timestamp
 *   UNIQUE(fk_player, fk_map, fk_zone, type)
 * 
 * TABLE wintox_track_records
 *   id                 pk
 *   fk_player          INT,FK(wintox_players)
 *   fk_map             INT,FK(wintox_maps)
 *   track              INT
 *   type               VARCHAR(16),DEFAULT ''
 *   time               FLOAT
 *   jumps              INTEGER,DEFAULT 0
 *   UNIQUE(fk_player, fk_map, track, type)
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

#define SQLITE_MYSQL(%1) %1, \
    %1

#define SQL_QUERY[%1] sql_%1[g_DatabaseType]
#define SQLF(%1) SQL_FastQuery(g_Database, (%1))
#define SQLT(%1,%2,%3) SQL_TQuery(g_Database, (%2), (%1), (%3))

// If using SQLT_HANDLER, SQLTH can handle both the formatting and querying
// SQLTH(SQL_QUERY[?], any:data_passed_to_SQLT, any:...)
#define SQLTH(%1,%2,%3) decl String:query[512]; \
    Format(STR(query), SQL_QUERY[%1], %3); \
    SQLT(query, T_%1, (%2))
#define SQLTH_D(%1,%2,%3) \
    Format(STR(query), SQL_QUERY[%1], %3); \
    SQLT(query, T_%1, (%2))

// Also explicitly pass the handler name
// SQLTH_H(SQL_QUERY[?], handler, any:data_passed_to_SQLT, any:...)
#define SQLTH_H(%1,%2,%3,%4) decl String:query[512]; \
    Format(STR(query), SQL_QUERY[%1], %4); \
    SQLT(query, %2, (%3))
#define SQLTH_H_D(%1,%2,%3,%4) \
    Format(STR(query), SQL_QUERY[%1], %4); \
    SQLT(query, %2, (%3))

#define SQLT_HANDLER(%1,%2) public T_%1(Handle:owner, Handle:hndl, const String:error[], any:data) \
{ \
    if (hndl == INVALID_HANDLE) \
        LogError(%2, error); \
}


// ===========================================================================
//   Table Creation
// ===========================================================================
NEW_SQL_QUERY(CreatePlayers) = {
    "CREATE TABLE IF NOT EXISTS wintox_players ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       latest_name      TEXT NOT NULL, \
       latest_ip        TEXT NOT NULL, \
       last_join        INTEGER NOT NULL, \
       steamid          TEXT NOT NULL, \
       country          TEXT NOT NULL, \
       gametype         TEXT NOT NULL, \
       grosspoints      INTEGER NOT NULL DEFAULT 0, \
       racewins         INTEGER NOT NULL DEFAULT 0, \
       racelosses       INTEGER NOT NULL DEFAULT 0, \
       UNIQUE(steamid, gametype) \
     );",
    
    "CREATE TABLE IF NOT EXISTS wintox_players ( \
       id               INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
       latest_name      VARCHAR(64) NOT NULL, \
       latest_ip        VARCHAR(48) NOT NULL, \
       steamid          VARCHAR(64) NOT NULL, \
       last_join        INTEGER NOT NULL, \
       country          VARCHAR(3) NOT NULL, \
       gametype         VARCHAR(4) NOT NULL, \
       grosspoints      INTEGER NOT NULL DEFAULT 0, \
       racewins         INTEGER NOT NULL DEFAULT 0, \
       racelosses       INTEGER NOT NULL DEFAULT 0, \
       UNIQUE(steamid, gametype) \
    );"
};
NEW_SQL_QUERY(CreateMaps) = {
    "CREATE TABLE IF NOT EXISTS wintox_maps ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       bsp_name         TEXT NOT NULL, \
       human_name       TEXT NULL, \
       tier             TINYINT DEFAULT 1 NOT NULL, \
       is_staged        BOOLEAN DEFAULT 0 NOT NULL, \
       gametype         TEXT NOT NULL, \
       UNIQUE(bsp_name, gametype) \
     );",
    
    "CREATE TABLE IF NOT EXISTS wintox_maps ( \
       id               INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
       bsp_name         VARCHAR(255) NOT NULL, \
       human_name       VARCHAR(255) NULL, \
       tier             TINYINT(3) DEFAULT 1 NOT NULL, \
       is_staged        BOOLEAN DEFAULT 0 NOT NULL, \
       gametype         VARCHAR(4) NOT NULL, \
       UNIQUE(bsp_name, gametype) \
     );"
};
NEW_SQL_QUERY(CreateTracks) = {
    "CREATE TABLE IF NOT EXISTS wintox_tracks ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       track            INTEGER NOT NULL, \
       tier             INTEGER NOT NULL DEFAULT 1, \
       name             TEXT NOT NULL DEFAULT '', \
       zones            INTEGER NOT NULL DEFAULT 0, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id), \
       UNIQUE(fk_map, track) \
    );",
   
    "CREATE TABLE IF NOT EXISTS wintox_tracks ( \
       id               INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       track            INTEGER NOT NULL, \
       tier             INTEGER NOT NULL DEFAULT 1, \
       name             VARCHAR(64) NOT NULL DEFAULT '', \
       zones            INTEGER NOT NULL DEFAULT 0, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id) ON DELETE CASCADE ON UPDATE CASCADE, \
       UNIQUE(fk_map, track) \
    );"
};
NEW_SQL_QUERY(CreateZones) = {
    "CREATE TABLE IF NOT EXISTS wintox_zones ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       track            INTEGER NOT NULL DEFAULT 0, \
       number           INTEGER NOT NULL, \
       name             TEXT DEFAULT NULL, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id), \
       UNIQUE(fk_map, track, number) \
     );",
    
    "CREATE TABLE IF NOT EXISTS wintox_zones ( \
       id               INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       track            TINYINT NOT NULL DEFAULT 0, \
       number           INTEGER NOT NULL, \
       name             VARCHAR(64) DEFAULT NULL, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id) ON DELETE CASCADE ON UPDATE CASCADE, \
       UNIQUE(fk_map, track, number) \
     );"
};
NEW_SQL_QUERY(CreateZonePoints) = {
    "CREATE TABLE IF NOT EXISTS wintox_zone_points ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_zone          INTEGER NOT NULL, \
       number           INTEGER NOT NULL, \
       x                FLOAT NOT NULL, \
       y                FLOAT NOT NULL, \
       z                FLOAT NOT NULL, \
       FOREIGN KEY(fk_zone) REFERENCES wintox_zones(id), \
       UNIQUE(fk_zone, number) \
     );",
    
    "CREATE TABLE IF NOT EXISTS wintox_zone_points ( \
       id               INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
       fk_zone          INTEGER NOT NULL, \
       number           INTEGER NOT NULL, \
       x                FLOAT NOT NULL, \
       y                FLOAT NOT NULL, \
       z                FLOAT NOT NULL, \
       FOREIGN KEY(fk_zone) REFERENCES wintox_zones(id) ON DELETE CASCADE ON UPDATE CASCADE, \
       UNIQUE(fk_zone, number) \
     );"
};
NEW_SQL_QUERY(CreateZoneRecords) = {
    "CREATE TABLE IF NOT EXISTS wintox_zone_records ( \
       id               INTEGER PRIMARY KEY ASC NOT NULL, \
       fk_player        INTEGER NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       fk_zone          INTEGER NOT NULL, \
       type             TEXT NOT NULL DEFAULT '', \
       time             FLOAT NOT NULL, \
       date_set         INTEGER NOT NULL, \
       FOREIGN KEY(fk_player) REFERENCES wintox_players(id), \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id), \
       FOREIGN KEY(fk_zone) REFERENCES wintox_zones(id), \
       UNIQUE(fk_player, fk_map, fk_zone, type) \
     );",
    
    "CREATE TABLE IF NOT EXISTS wintox_zone_records ( \
       id               INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
       fk_player        INTEGER NOT NULL, \
       fk_map           INTEGER NOT NULL, \
       fk_zone          INTEGER NOT NULL, \
       type             VARCHAR(16) NOT NULL DEFAULT '', \
       time             FLOAT NOT NULL, \
       date_set         INTEGER NOT NULL, \
       FOREIGN KEY(fk_player) REFERENCES wintox_players(id) ON DELETE CASCADE ON UPDATE CASCADE, \
       FOREIGN KEY(fk_map) REFERENCES wintox_maps(id) ON DELETE CASCADE ON UPDATE CASCADE, \
       FOREIGN KEY(fk_zone) REFERENCES wintox_zones(id) ON DELETE CASCADE ON UPDATE CASCADE, \
       UNIQUE(fk_player, fk_map, fk_zone, type) \
     );"
};
NEW_SQL_QUERY(CreateTrackRecords) = {
    "CREATE TABLE IF NOT EXISTS wintox_track_records ( \
        id              INTEGER PRIMARY KEY ASC NOT NULL, \
        fk_player       INTEGER NOT NULL, \
        fk_map          INTEGER NOT NULL, \
        track           INTEGER NOT NULL, \
        type            TEXT NOT NULL DEFAULT '', \
        time            FLOAT NOT NULL, \
        jumps           INTEGER NOT NULL DEFAULT 0, \
        UNIQUE(fk_player, fk_map, track, type) \
    );",
    
    "CREATE TABLE IF NOT EXISTS wintox_track_records ( \
        id              INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, \
        fk_player       INTEGER NOT NULL, \
        fk_map          INTEGER NOT NULL, \
        track           INTEGER NOT NULL, \
        type            VARCHAR(16) NOT NULL DEFAULT '', \
        time            FLOAT, \
        jumps           INTEGER NOT NULL DEFAULT 0, \
        UNIQUE(fk_player, fk_map, track, type) \
    );"
};

// ===========================================================================
//   Map records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertMap,5) = {
    SQLITE_MYSQL("INSERT INTO wintox_maps (bsp_name,human_name,tier,is_staged,gametype) VALUES('%s','%s',%d,%d,'%s');")
};
NEW_SQL_QUERY_PARAMS(GetMap,2) = {
    SQLITE_MYSQL("SELECT id,bsp_name,human_name,tier,is_staged FROM `wintox_maps` WHERE `bsp_name` = '%s' AND `gametype` = '%s';")
};
NEW_SQL_QUERY_PARAMS(UpdateMapTier,2) = {
    SQLITE_MYSQL("UPDATE wintox_maps SET tier = %d WHERE id = %d;")
};

// ===========================================================================
//   Player records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertPlayer,5) = {
    "INSERT INTO wintox_players (latest_name,latest_ip,steamid,country,gametype,last_join) VALUES('%s','%s','%s','%s','%s',strftime('%%s', 'now'));",
    "INSERT INTO wintox_players (latest_name,latest_ip,steamid,country,gametype,last_join) VALUES('%s','%s','%s','%s','%s',UNIX_TIMESTAMP(NOW()));"
};
NEW_SQL_QUERY_PARAMS(UpdatePlayer,5) = {
    "UPDATE wintox_players SET latest_name='%s', latest_ip='%s', country='%s', last_join=strftime('%%s', 'now') WHERE steamid = '%s' AND gametype = '%s';",
    "UPDATE wintox_players SET latest_name='%s', latest_ip='%s', country='%s', last_join=UNIX_TIMESTAMP(NOW()) WHERE steamid = '%s' AND gametype = '%s';"
};
NEW_SQL_QUERY_PARAMS(GetPlayer,1) = {
    SQLITE_MYSQL("SELECT id,latest_name,latest_ip,steamid,country,grosspoints,racewins,racelosses FROM wintox_players WHERE steamid = '%s' AND gametype = '%s';")
};
NEW_SQL_QUERY_PARAMS(UpdatePlayerTimestamp,1) = {
    "UPDATE wintox_players SET last_join=strftime('%%s', 'now') WHERE id = %d;",
    "UPDATE wintox_players SET last_join=UNIX_TIMESTAMP(NOW()) WHERE id = %d;"
};

// ===========================================================================
//   Tracks
// ===========================================================================
NEW_SQL_QUERY_PARAMS(UpsertTrack,5) = {
    "INSERT OR REPLACE INTO wintox_tracks (fk_map, track, tier, name, zones) VALUES(%d, %d, %d, '%s', %d);",
    "INSERT INTO wintox_tracks (fk_map, track, tier, name, zones) VALUES(%d, %d, %d, '%s', %d) \
        ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id), tier = VALUES(tier), name = VALUES(name), zones = VALUES(zones);"
};
NEW_SQL_QUERY_PARAMS(GetTracks,1) = {
    SQLITE_MYSQL("SELECT track, tier, name, zones FROM wintox_tracks WHERE fk_map = %d;")
};

// ===========================================================================
//   Zones and zone points
// ===========================================================================
NEW_SQL_QUERY_PARAMS(InsertZone,5) = {
    "INSERT OR REPLACE INTO wintox_zones (id, fk_map, track, number, name) VALUES(%s, %d, %d, %d, '%s');",
    
    "INSERT INTO wintox_zones (id, fk_map, track, number, name) VALUES(%s, %d, %d, %d, '%s') \
        ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id), name = VALUES(name);"
};
NEW_SQL_QUERY_PARAMS(DeleteZone,2) = {
    SQLITE_MYSQL("DELETE FROM wintox_zones WHERE fk_map = %d AND track = %d AND number = %d;")
};
NEW_SQL_QUERY_PARAMS(ClearAllZones,1) = {
    SQLITE_MYSQL("DELETE FROM wintox_zones WHERE fk_map = %d;")
};
NEW_SQL_QUERY_PARAMS(ClearAllZonePoints,1) = {
    SQLITE_MYSQL("DELETE FROM wintox_zone_points WHERE fk_zone IN ( SELECT id FROM wintox_zones WHERE fk_map = %d );")
};
NEW_SQL_QUERY_PARAMS(ClearZonePoints,3) = {
    SQLITE_MYSQL("DELETE FROM wintox_zone_points WHERE fk_zone IN ( SELECT id FROM wintox_zones WHERE fk_map = %d AND track = %d AND number = %d );")
};
NEW_SQL_QUERY_PARAMS(ClearTrackZones,1) = {
    SQLITE_MYSQL("DELETE FROM wintox_zones WHERE fk_map = %d AND track = %d;")
};
NEW_SQL_QUERY_PARAMS(ClearTrackZonePoints,3) = {
    SQLITE_MYSQL("DELETE FROM wintox_zone_points WHERE fk_zone IN ( SELECT id FROM wintox_zones WHERE fk_map = %d AND track = %d );" )
};
NEW_SQL_QUERY_PARAMS(GetZones,1) = {
    SQLITE_MYSQL("SELECT id,number,name,track FROM wintox_zones WHERE fk_map = %d;")
};
NEW_SQL_QUERY_PARAMS(GetZone,1) = {
    SQLITE_MYSQL("SELECT id,name FROM wintox_zones WHERE fk_map = %d AND track = %d AND number = %d;")
};
NEW_SQL_QUERY_PARAMS(GetZonePoints,1) = {
    SQLITE_MYSQL("SELECT wzp.id,wzp.number,x,y,z,wz.number,wz.track AS zone_id \
            FROM wintox_zone_points AS wzp \
            JOIN wintox_zones AS wz ON wzp.fk_zone = wz.id \
            WHERE fk_zone IN (SELECT id FROM wintox_zones WHERE fk_map = %d);")
};
NEW_SQL_QUERY_PARAMS(InsertZonePoint,5) = {
    SQLITE_MYSQL("INSERT INTO wintox_zone_points (fk_zone, number, x, y, z) VALUES(%d, %d, %f, %f, %f);")
};

// ===========================================================================
//   Zone records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(UpsertZoneRecord,6) = {
    "INSERT OR REPLACE INTO wintox_zone_records (fk_map, fk_player, fk_zone, type, time, date_set) VALUES(%d, %d, %d, '%s', %f, %d);",
    "INSERT INTO wintox_zone_records (fk_map, fk_player, fk_zone, type, time, date_set) VALUES(%d, %d, %d, '%s', %f, %d) \
        ON DUPLICATE KEY UPDATE \
            id = LAST_INSERT_ID(id), \
            time = VALUES(time), \
            date_set = VALUES(date_set);"
};
NEW_SQL_QUERY_PARAMS(GetZoneRecords,2) = {
    SQLITE_MYSQL( \
        "SELECT wzr.id,wz.number,wz.track,type,time,date_set \
            FROM wintox_zone_records AS wzr \
            JOIN wintox_zones AS wz ON wzr.fk_zone = wz.id \
            WHERE wzr.fk_map = %d AND fk_player = %d")
};
NEW_SQL_QUERY_PARAMS(GetBestZoneRecords,1) = {
    SQLITE_MYSQL( \
        "SELECT wz.track, wz.number, type, MIN(time) \
            FROM `wintox_zone_records` AS wzr \
            JOIN wintox_zones AS wz ON wz.id = wzr.fk_zone \
            WHERE wzr.fk_map = %d \
            GROUP BY wz.track, wz.number, type")
};

// ===========================================================================
//   Player points
// ===========================================================================
NEW_SQL_QUERY_PARAMS(UpdatePlayerPoints,2) = {
    SQLITE_MYSQL("UPDATE wintox_players SET grosspoints = %d WHERE id = %d;")
};

// ===========================================================================
//   Player races
// ===========================================================================
NEW_SQL_QUERY_PARAMS(IncrementPlayerRaceWins,1) = {
    SQLITE_MYSQL("UPDATE wintox_players SET racewins = racewins + 1 WHERE id = %d;")
};
NEW_SQL_QUERY_PARAMS(IncrementPlayerRaceLosses,1) = {
    SQLITE_MYSQL("UPDATE wintox_players SET racelosses = racelosses + 1 WHERE id = %d;")
};
NEW_SQL_QUERY_PARAMS(GetPlayerRaceRecords,1) = {
    SQLITE_MYSQL("SELECT latest_name, racewins, racelosses FROM wintox_players WHERE id = %d")
};
NEW_SQL_QUERY_PARAMS(GetPartialPlayerRaceRecords,2) = {
    SQLITE_MYSQL("SELECT latest_name, racewins, racelosses FROM wintox_players WHERE latest_name LIKE '%%%s%%' AND gametype = '%s'")
};
NEW_SQL_QUERY_PARAMS(GetRtop,2) = {
    SQLITE_MYSQL("SELECT latest_name, racewins, racelosses FROM wintox_players WHERE gametype = '%s' ORDER BY racewins DESC LIMIT %d,11;")
};

// ===========================================================================
//   Track records
// ===========================================================================
NEW_SQL_QUERY_PARAMS(UpsertTrackRecord,6) = {
    "INSERT OR REPLACE INTO wintox_track_records (fk_player, fk_map, track, type, time, jumps) VALUES(%d, %d, %d, '%s', %f, %d);",
    
    "INSERT INTO wintox_track_records (fk_player, fk_map, track, type, time, jumps) VALUES(%d, %d, %d, '%s', %f, %d) \
        ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id), time = VALUES(time), jumps = VALUES(jumps);"
};
NEW_SQL_QUERY_PARAMS(GetTrackRecords,2) = {
    SQLITE_MYSQL("SELECT track, type, time,jumps FROM wintox_track_records WHERE fk_map = %d AND fk_player = %d")
};
NEW_SQL_QUERY_PARAMS(GetTop,4) = {
    SQLITE_MYSQL("SELECT wzm2.bsp_name, wzp.latest_name, time, jumps FROM wintox_track_records \
                         JOIN wintox_players AS wzp ON fk_player = wzp.id \
                         JOIN wintox_maps AS wzm ON fk_map = wzm.id \
                         JOIN ( \
                            SELECT DISTINCT bsp_name, id FROM wintox_maps \
                            WHERE bsp_name = '%s' \
                            LIMIT 1 \
                         ) AS wzm2 ON wzm2.id = wzm.id \
                         WHERE wzm.bsp_name = wzm2.bsp_name AND track = %d AND type = '%s' \
                         ORDER BY time ASC \
                         LIMIT %d,11;")
};
NEW_SQL_QUERY_PARAMS(GetTopPartial,4) = {
    SQLITE_MYSQL("SELECT wzm2.bsp_name, wzp.latest_name, time, jumps FROM wintox_track_records \
                         JOIN wintox_players AS wzp ON fk_player = wzp.id \
                         JOIN wintox_maps AS wzm ON fk_map = wzm.id \
                         JOIN ( \
                            SELECT DISTINCT bsp_name, id FROM wintox_maps \
                            WHERE bsp_name LIKE '%%%s%%' \
                            LIMIT 1 \
                         ) AS wzm2 ON wzm2.id = wzm.id \
                         WHERE wzm.bsp_name = wzm2.bsp_name AND track = %d AND type = '%s' \
                         ORDER BY time ASC \
                         LIMIT %d,11;")
};

new String:sql_GetPercentCompletion[DatabaseType][1152];
new String:sql_GetPercentCompletionPartial[DatabaseType][1152];

NEW_SQL_QUERY_PARAMS(GetWorldRecord,3) = {
    SQLITE_MYSQL("SELECT wp.latest_name, time, jumps FROM wintox_track_records \
                         JOIN wintox_players AS wp ON wp.id = fk_player \
                         WHERE fk_map = %d AND track = %d AND type = '%s' \
                         ORDER BY time ASC \
                         LIMIT 1;")
};
NEW_SQL_QUERY_PARAMS(GetWorldRecordPartial,3) = {
    SQLITE_MYSQL("SELECT wp.latest_name, time, jumps FROM wintox_track_records \
                         JOIN wintox_players AS wp ON wp.id = fk_player \
                         JOIN wintox_maps AS wm ON wm.id = fk_map \
                         WHERE wm.bsp_name LIKE '%%%s%%' AND track = %d AND type = '%s' \
                         ORDER BY time ASC \
                         LIMIT 1;")
};

// ===========================================================================
//   Rank
// ===========================================================================

NEW_SQL_QUERY_PARAMS(GetPlayerRank,1) = {
    "",
    "SELECT id, latest_name, rank FROM ( \
        SELECT id, latest_name, @curRank := @curRank + 1 AS rank \
            FROM wintox_players, (SELECT @curRank := 0) AS r \
            ORDER BY grosspoints DESC, last_join DESC \
        ) AS ranks \
        WHERE id = %d \
        LIMIT 0,1;"
};
NEW_SQL_QUERY_PARAMS(GetPartialPlayerRank,1) = {
    "",
    "SELECT latest_name, rank FROM ( \
        SELECT latest_name, @curRank := @curRank + 1 AS rank \
            FROM wintox_players, (SELECT @curRank := 0) AS r \
            ORDER BY grosspoints DESC, last_join DESC \
        ) AS ranks \
        WHERE latest_name LIKE '%%%s%%' \
        LIMIT 0,1;"
};
NEW_SQL_QUERY_PARAMS(GetPlayerCountryRank,2) = {
    "",
    "SELECT `latest_name`, `rank`, `country` FROM ( \
        SELECT `latest_name`, `country`.`country`, @curRank := @curRank + 1 AS rank \
            FROM `wintox_players` AS wp, (SELECT @curRank := 0) AS r \
            JOIN ( \
                SELECT `id`, `country` \
                    FROM `wintox_players` \
                    WHERE `id` = %d \
                    LIMIT 0,1 \
            ) AS country ON `country`.`id` = id \
            WHERE wp.`country` = `country`.`country` \
            ORDER BY `grosspoints` DESC, `last_join` DESC \
        ) AS ranks \
        WHERE `id` = %d \
        LIMIT 0,1;"
};
NEW_SQL_QUERY_PARAMS(GetPartialPlayerCountryRank,2) = {
    "",
    "SELECT `latest_name`, `rank`, `country` FROM ( \
        SELECT `latest_name`, `country`.`country`, @curRank := @curRank + 1 AS rank \
            FROM `wintox_players` AS wp, (SELECT @curRank := 0) AS r \
            JOIN ( \
                SELECT `id`, `country` \
                    FROM `wintox_players` \
                    WHERE `latest_name` LIKE '%%%s%%' \
                    LIMIT 0,1 \
            ) AS country ON `country`.`id` = id \
            WHERE wp.`country` = `country`.`country` \
            ORDER BY `grosspoints` DESC, `last_join` DESC \
        ) AS ranks \
        WHERE `latest_name` LIKE '%%%s%%' \
        LIMIT 0,1;"
};
NEW_SQL_QUERY_PARAMS(GetAllRanks,1) = {
    "",
    "SELECT `latest_name`, `rank` FROM ( \
        SELECT `latest_name`, @curRank := @curRank + 1 AS rank \
            FROM `wintox_players` AS wp, (SELECT @curRank := 0) AS r \
            ORDER BY `grosspoints` DESC, `last_join` DESC \
        ) AS ranks \
        LIMIT %d,11;"
};
NEW_SQL_QUERY_PARAMS(GetAllCountryRanks,2) = {
    "",
    "SELECT `latest_name`, `rank` FROM ( \
        SELECT `latest_name`, @curRank := @curRank + 1 AS rank \
            FROM `wintox_players` AS wp, (SELECT @curRank := 0) AS r \
            WHERE wp.`country` = '%s' \
            ORDER BY `grosspoints` DESC, `last_join` DESC \
        ) AS ranks \
        LIMIT %d,11;"
};


// SQL helper functions
win_SQL_Init()
{
    g_cv_DBConfig = CreateConVar("wintox_dbconfig", "default", "The database.cfg configuration to use with Wintox");
    
    // The query was so big, spcomp froze. Aww yeah.
    for (new i=0; i<_:DatabaseType; i++)
    {
        new written = strcopy(sql_GetPercentCompletion[i], sizeof(sql_GetPercentCompletion[]), "\
            SELECT records.latest_name, ((records.num_records / (trackscnt.num_tracks * %d)) * 100) AS pc \
                FROM `wintox_track_records` \
                JOIN ( \
                    SELECT COUNT(wt.id) AS num_tracks, gametype \
                        FROM wintox_tracks AS wt \
                        JOIN wintox_maps AS wzm ON wzm.id = fk_map \
                        WHERE wzm.gametype = '%s' \
                ) AS trackscnt ");
        strcopy(sql_GetPercentCompletionPartial[i], sizeof(sql_GetPercentCompletionPartial[]), sql_GetPercentCompletion[i]);
        
        strcopy(sql_GetPercentCompletion[i][written], sizeof(sql_GetPercentCompletion[])-written, "\
            JOIN ( \
                SELECT COUNT(wtr.id) AS num_records, latest_name, wzm.gametype \
                    FROM wintox_track_records AS wtr \
                    JOIN wintox_maps AS wzm ON wzm.id = fk_map \
                    JOIN wintox_players AS wp ON wp.id = fk_player \
                    WHERE fk_player = %d \
            ) AS records \
            WHERE records.gametype = trackscnt.gametype \
            LIMIT 1;");
        
        strcopy(sql_GetPercentCompletionPartial[i][written], sizeof(sql_GetPercentCompletionPartial[]), "\
            JOIN ( \
                SELECT COUNT(wtr.id) AS num_records, latest_name, wzm.gametype \
                    FROM wintox_track_records AS wtr \
                    JOIN wintox_maps AS wzm ON wzm.id = fk_map \
                    JOIN wintox_players AS wp ON wp.id = fk_player \
                    WHERE wp.latest_name LIKE '%%%s%%' \
            ) AS records \
            WHERE records.gametype = trackscnt.gametype \
            LIMIT 1;");
    }
}

// Because this uses the `wintox_dbconfig` cvar, it must be called from OnConfigsExecuted (or later)
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
        SetFailState("Error creating table %s: %s", (%2), buffer); \
    }
    
    CREATE_TABLE(CreatePlayers, "wintox_players")
    CREATE_TABLE(CreateMaps, "wintox_maps")
    CREATE_TABLE(CreateTracks, "wintox_tracks")
    CREATE_TABLE(CreateZones, "wintox_zones")
    CREATE_TABLE(CreateZonePoints, "wintox_zone_points")
    CREATE_TABLE(CreateZoneRecords, "wintox_zone_records")
    CREATE_TABLE(CreateTrackRecords, "wintox_track_records")
}
