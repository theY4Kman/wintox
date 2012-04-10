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

#elseif defined(__GAME_TF2)
    #include <tf2>
    #define SPECTATOR_TEAM TFTeam_Spectator

#endif
