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
// Holds state for wintox_race_challenge_menu_time
new g_RaceChallengeMenuTime = 20;
// Holds state for wintox_race_challenge_taunts
new g_RaceChallengeTaunts = true;

new g_RaceOpponents[MAXPLAYERS];
// For each player, stores the client that initiated the challenge
new g_RaceChallengedBy[MAXPLAYERS];

new String:g_RaceTauntsDecline[][] = {
    "You could race a rock and lose!",
    "Slow and steady wins the race. Slow and terrible loses."
};
new String:g_RaceTauntsAccept[][] = {
    "You could race a rock and lose!"
};


Race_Init()
{
    RegisterCommand("sm_race", ConCommand_Race, "sm_race | Challenge another player to a race!");
    
    new Handle:cvar_race_challenge_multiple = CreateConVar("wintox_race_challenge_multiple", "0", "Whether to allow players to challenge multiple other players at one time.");
    g_RaceAllowMultipleChallenges = GetConVarBool(cvar_race_challenge_multiple);
    HookConVarChange(cvar_race_challenge_multiple, ConVar_RaceAllowMultipleChallenges);
    
    new Handle:cvar_race_challenge_menu_time = CreateConVar("wintox_race_challenge_menu_time", "20", "The length of time to display the challenge menu.");
    g_RaceChallengeMenuTime = GetConVarInt(cvar_race_challenge_menu_time);
    HookConVarChange(cvar_race_challenge_menu_time, ConVar_RaceChallengeMenuTime);
    
    new Handle:cvar_race_challenge_taunts = CreateConVar("wintox_race_challenge_taunts", "1", "Whether to allow taunts when accepting or declining a race challenge.");
    g_RaceChallengeTaunts = GetConVarInt(cvar_race_challenge_taunts);
    HookConVarChange(cvar_race_challenge_taunts, ConVar_RaceChallengeTaunts);
}

Race_DeinitPlayer(client)
{
    if (IS_IN_RACE(client))
        StopRace(client, g_RaceOpponents[client], "Your opponent left!");
    g_RaceOpponents[client] = 0;
}

public ConVar_RaceAllowMultipleChallenges(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RaceAllowMultipleChallenges = GetConVarBool(cvar);
}

public ConVar_RaceChallengeMenuTime(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RaceChallengeMenuTime = GetConVarInt(cvar);
}

public ConVar_RaceChallengeTaunts(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RaceChallengeTaunts = GetConVarBool(cvar);
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
    if (!g_RaceAllowMultipleChallenges && HAS_OUTSTANDING_CHALLENGE(challenger))
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
        
        // Don't allow challenging players with an open race challenge
        if (g_RaceChallengedBy[client] != 0)
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
        
        g_RaceChallengedBy[challengee] = param1;
        
        if (!g_RaceAllowMultipleChallenges)
            g_RaceOpponents[param1] = challengee;
        
        AskRace(param1, challengee);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

// Show a dialog on the challengee's screen asking to accept or decline the race
AskRace(challenger, challengee)
{
    new Handle:menu = CreateMenu(MenuHandler_AskRace);
    SetMenuTitle(menu, "%N has challenged you to a race", challenger);
    
    AddMenuItem(menu, "no", "Decline");
    AddMenuItem(menu, "yes", "Accept");
    
    if (g_RaceChallengeTaunts)
    {
        AddMenuItem(menu, "notaunt", "Decline and Taunt");
        AddMenuItem(menu, "yestaunt", "Accept and Taunt");
    }
    
    DisplaySubmenu(menu, challengee, g_RaceChallengeMenuTime);
}

public MenuHandler_AskRace(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
        new challenger = g_RaceChallengedBy[param1];
        
        new bool:accepted = false;
        if (strncmp("no", info, 2) == 0)
            ChallengeeDeclined(challenger, param1);
        else if (strncmp("yes", info, 3) == 0)
        {
            accepted = true;
            ChallengeeAccepted(challenger, param1);
        }
        
        if (StrEqual("taunt", info[2]))
        {
            new num_taunts = 0;
            if (accepted)
                num_taunts = sizeof(g_RaceTauntsAccept);
            else
                num_taunts = sizeof(g_RaceTauntsDecline);
            
            new taunt_idx = GetRandomInt(0, num_taunts-1);
            
            Color_ChatSetSubject(challenger);
            Race_PrintToChat(challenger, "{T}%N{N} says: {OG}%s{N}", param1, accepted ? g_RaceTauntsAccept[taunt_idx] : g_RaceTauntsDecline[taunt_idx]);
        }
    }
    else if (action == MenuAction_Cancel)
    {
        if (!g_RaceAllowMultipleChallenges)
            g_RaceOpponents[param1] = 0;
        
        HANDLE_CANCEL();
    }
}

// Setup the players at the start zone and begin the countdown timer
InitiateRace(challenger, challengee)
{
}

// Stop a race prematurely
StopRace(challenger, challengee, const String:reason[]="")
{
}

// The challengee declined a race challenge
ChallengeeDeclined(challenger, challengee)
{
    Color_ChatSetSubject(challenger);
    Race_PrintToChat(challenger, "{T}%N{N} has {R}declined{N} your race challenge {OG}:({N}", challengee);
    
    if (!g_RaceAllowMultipleChallenges)
        g_RaceOpponents[challenger] = 0;
    g_RaceChallengedBy[challengee] = 0;
}

// The challengee accepted a race challenge
ChallengeeAccepted(challenger, challengee)
{
    Color_ChatSetSubject(challenger);
    Race_PrintToChat(challenger, "{T}%N{N} has {R}accepted{N} your race challenge!", challengee);
    
    InitiateRace(challenger, challengee);
}
