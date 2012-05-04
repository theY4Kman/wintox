/**
 * VROOOOM RACING
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_race_included
 #endinput
#endif
#define _wintox_race_included

#include "./include/menus"
#include "./include/game"
#include "./include/commands"
#include "./include/stocks"


// Whether a player has an outstanding challenge or race
#define HAS_OUTSTANDING_CHALLENGE(%1) (g_RaceOpponents[(%1)] != 0)
// Whether a player is currently in a race
#define IS_IN_RACE(%1) (HAS_OUTSTANDING_CHALLENGE(%1) && g_RaceOpponents[g_RaceOpponents[(%1)]] == (%1))

// Holds state for wintox_race_challenge_multiple
new g_RaceAllowMultipleChallenges = false;
new g_RaceChallengeMenuTime = 20;
new g_RaceOpponents[MAXPLAYERS];


Race_Init()
{
    RegisterCommand("sm_race", ConCommand_Race, "sm_race | Challenge another player to a race!");
}

Race_DeinitPlayer(client)
{
    if (IS_IN_RACE(client))
        StopRace(client, g_RaceOpponents[client], "Your opponent left!");
    g_RaceOpponents[client] = 0;
}


public Action:ConCommand_Race(client, args)
{
    NO_ARGS()
    
    RaceMenu(client);
    
    return Plugin_Handled;
}


// Bring up a menu of all players in the server for the challenger to choose
// their challengee.
RaceMenu(challenger)
{
    if (HAS_OUTSTANDING_CHALLENGE(challenger))
    {
        // TODO: Handle this more elegantly
        Color_ChatSetSubject(g_RaceOpponents[challenger]);
        Race_Reply(challenger, "You already have an outstanding challenge against {T}%N{N}!", g_RaceOpponents[challenger]);
        return;
    }
    
    new Handle:menu = CreateMenu(MenuHandler_RaceMenu);
    SetMenuTitle(menu, "Who will you challenge?");
    
    new bool:any_challengees = false;
    for (new client=1; client<MaxClients; client++)
    {
        decl String:name[MAX_NAME_LENGTH];
        decl String:sz_userid[16];
        
        if (client == challenger)
            continue;
        
        if (!IsClientConnected(client) || !IsClientInGame(client) || IsFakeClient(client))
            continue;
        
        if (!IS_PLAYING_TEAM(GetClientTeam(client)))
            continue;

        IntToString(GetClientUserId(client), STR(sz_userid));
        GetClientName(client, STR(name));
        
        AddMenuItem(menu, sz_userid, name);
        any_challengees = true;
    }
    
    if (any_challengees)
        DisplaySubmenu(menu, challenger, MENU_TIME_FOREVER);
    else
        Race_Reply(challenger, "There's no one to challenge {OG}:({N}");
}

public MenuHandler_RaceMenu(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
        new userid = StringToInt(info);
        new challengee = GetClientOfUserId(userid);
        
        if (challengee == 0)
        {
            Race_Reply(param1, "Unfortunately, something happened to that player and they cannot be challenged. Try someone else.");
            
            PrevSubmenu(param1, false);
            RaceMenu(param1);
            
            return;
        }
        
        g_RaceOpponents[param1] = challengee;
        AskRace(param1, challengee);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

// Show a dialog on the challengee's screen asking to accept or decline the race
AskRace(challenger, challengee)
{
}

// Setup the players at the start zone and begin the countdown timer
InitiateRace(challenger, challengee)
{
}

// Stop a race prematurely
StopRace(challenger, challengee, const String:reason[]="")
{
}
