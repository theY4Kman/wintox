/**
 * Game-specific defines
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_game_included
 #endinput
#endif
#define _wintox_game_included

#if !defined(__GAME_CSTRIKE) && !defined(__GAME_TF2)
    #define __GAME_CSTRIKE
#endif

#if defined(__GAME_CSTRIKE)
    #include <cstrike>
    #define SPECTATOR_TEAM CS_TEAM_SPECTATOR
    #define IS_PLAYING_TEAM(%1) ((%1) == CS_TEAM_T || (%1) == CS_TEAM_CT)
    #define RESPAWN_PLAYER CS_RespawnPlayer

#elseif defined(__GAME_TF2)
    #include <tf2>
    #define SPECTATOR_TEAM TFTeam_Spectator
    #define IS_PLAYING_TEAM(%1) ((%1) == TFTeam_Red || (%1) == TFTeam_Blue)
    #define RESPAWN_PLAYER TF2_RespawnPlayer

#endif
