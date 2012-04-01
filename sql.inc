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
