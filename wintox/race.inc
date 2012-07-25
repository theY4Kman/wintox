/**
 * VROOOOM RACING
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_race_included
 #endinput
#endif
#define _wintox_race_included


enum RaceType
{
    RaceType_Invalid=-1,
    RaceType_Linear=0,  // Default -- from start zone to end zone on MAIN_TRACK
    RaceType_Bonus,     // From start zone to end zone, MAIN_TRACK < track < MAX_TRACKS
    RaceType_Stage      // From one zone to the next on MAIN_TRACK
};


// Whether a player has an outstanding challenge or race
#define HAS_OUTSTANDING_CHALLENGE(%1) (g_RaceOutstandingChallenges[(%1)] > 0)
// Loops over the challenger and challengee vars, making them available in `client`
#define DO_PARTICIPANTS for (new client=challenger, i=-1; i<1; client=i++ ? challengee : challenger)

// Holds state for wintox_race_challenges_amount
new g_RaceChallengesAmount = 1;
// Holds state for wintox_race_challenge_menu_time
new g_RaceChallengeMenuTime = 20;
// Holds state for wintox_race_challenge_taunts
new g_RaceChallengeTaunts = true;

new g_RaceOutstandingChallenges[MAXPLAYERS];
new g_RaceOpponents[MAXPLAYERS];
// For each player, stores the client that initiated the challenge
new g_RaceChallengedBy[MAXPLAYERS];

// If the race is in progress
new bool:g_Racing[MAXPLAYERS];
// The time the race started
new Float:g_RaceStart[MAXPLAYERS];
// The total time the race took
new Float:g_RaceEndElapsed[MAXPLAYERS];
// The client who won
new g_RaceWinner[MAXPLAYERS];
// The style of the player at the end of the race
new g_RaceEndStyle[MAXPLAYERS] = { DEFAULT_STYLE, ... };
// The style of the player at the end of the race
new g_RaceEndJumps[MAXPLAYERS] = { -1, ... };

// Stores the type of race
new RaceType:g_RaceType[MAXPLAYERS] = { RaceType_Invalid, ... };
// The track the race is on
new g_RaceTrack[MAXPLAYERS] = { INVALID_TRACK, ... };
// The zone the race begins in
new g_RaceStartZone[MAXPLAYERS] = { INVALID_ZONE, ... };
// The zone the race will end in (used to check if the race is finished)
new g_RaceEndZone[MAXPLAYERS] = { INVALID_ZONE, ... };


// Keeps track of the start index for each client's !rtop panel
new g_RaceRtopStart[MAXPLAYERS];

new Handle:g_RaceCountdownTimer[MAXPLAYERS] = { INVALID_HANDLE, ... };


#include "./wintox/menus"
#include "./wintox/game"
#include "./wintox/commands"
#include "./wintox/stocks"


new String:g_RaceTauntsDecline[][] = {
    "You could race a rock and lose!",
    "Slow and steady wins the race. Slow and terrible loses.",
    "I'd race you, but I don't want crap on my shoes.",
    "I didn't know this server supported the Special Olympics.",
    "It would be like a Kenyan sprinting against a toddler.",
    "I don't want you to be embarrassed."
};
new String:g_RaceTauntsAccept[][] = {
    "It's on.",
    "Bring it.",
    "May the best man win, and I intend to.",
    "Do I get a plaque when I win?",
    "When I win, I'll buy you a Ramen dinner.",
    "Just don't embarrass yourself."
};


Race_Init()
{
    RegisterCommand("sm_race", ConCommand_Race, "sm_race | Challenge another player to a race!");
#if defined(WINTOX_SURF)
    RegisterCommand("sm_srace", ConCommand_SRace, "sm_srace [<stage #>] | Challenge another player to a race on a certain stage.");
    RegisterCommand("sm_brace", ConCommand_BRace, "sm_brace [<bonus #>] | Challenge another player to a race in a bonus area.");
#endif
    RegisterCommand("sm_racehide", ConCommand_RaceHide, "sm_racehide | Hide all other players but the other racer");
    RegisterCommand("sm_raceshow", ConCommand_RaceShow, "sm_raceshow | Show all other players (or hide your race opponent if you've run !hide)");
    RegisterCommand("sm_rstats", ConCommand_Rstats, "sm_rstats [<partial player name>] | Show the race wins, losses, and ratio for the specified player (or yours if omitted)");
    RegisterCommand("sm_rtop", ConCommand_Rtop, "sm_rtop [<start #>] | Displays a menu listing the top racers and their stats, optionally starting at <start #>");
    
#if defined(WINTOX_DEBUG)
    RegisterCommand("sm_forcechallenge", ConCommand_ForceChallenge, "sm_forcechallenge <player 1> <player 2> | Emulates <player 1> running !race and selecting <player 2>", 0, ADMFLAG_GENERIC, "wtxdebug");
#endif
}

Race_RegisterConVars()
{
    new Handle:cvar_race_challenges_amount = CreateConVar("wintox_race_challenges_amount", "1", "The number of people a player can challenge to a race at once.");
    g_RaceChallengesAmount = GetConVarInt(cvar_race_challenges_amount);
    HookConVarChange(cvar_race_challenges_amount, ConVar_RaceChallengesAmount);
    
    new Handle:cvar_race_challenge_menu_time = CreateConVar("wintox_race_challenge_menu_time", "20", "The length of time to display the challenge menu.", 0, true, 0.0);
    g_RaceChallengeMenuTime = GetConVarInt(cvar_race_challenge_menu_time);
    HookConVarChange(cvar_race_challenge_menu_time, ConVar_RaceChallengeMenuTime);
    
    new Handle:cvar_race_challenge_taunts = CreateConVar("wintox_race_challenge_taunts", "1", "Whether to allow taunts when accepting or declining a race challenge.");
    g_RaceChallengeTaunts = GetConVarInt(cvar_race_challenge_taunts);
    HookConVarChange(cvar_race_challenge_taunts, ConVar_RaceChallengeTaunts);
}

Race_DeinitPlayer(client)
{
    if (IsRacing(client))
        StopRace(client, g_RaceOpponents[client], "Your opponent left!");
    
    ResetRaceVars(client);
    
    g_RaceHideOtherPlayers[client] = false;
    g_RaceRtopStart[client] = 0;
}

public ConVar_RaceChallengesAmount(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RaceChallengesAmount = GetConVarInt(cvar);
}

public ConVar_RaceChallengeMenuTime(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RaceChallengeMenuTime = GetConVarInt(cvar);
}

public ConVar_RaceChallengeTaunts(Handle:cvar, const String:oldVal[], const String:newVal[])
{
    g_RaceChallengeTaunts = GetConVarBool(cvar);
}



/* ========================================================================
                              CONCOMMANDS
   ======================================================================== */

public Action:ConCommand_RaceHide(client, args)
{
    NO_ARGS()
    
    g_RaceHideOtherPlayers[client] = !g_RaceHideOtherPlayers[client];
    
    if (g_RaceHideOtherPlayers[client])
        Race_Reply(client, "{OG}Hiding{N} all other players, except your race opponent.");
    else
        RaceShowHideMessage(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_RaceShow(client, args)
{
    NO_ARGS()
    
    g_RaceHideOtherPlayers[client] = false;
    RaceShowHideMessage(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_Race(client, args)
{
    NO_ARGS()
    NO_FAKECLIENTS()
    
    g_RaceTrack[client] = MAIN_TRACK;
    g_RaceStartZone[client] = START_ZONE;
    g_RaceType[client] = RaceType_Linear;
    
    RaceMenu(client);
    
    return Plugin_Handled;
}

#if defined(WINTOX_SURF)
public Action:ConCommand_SRace(client, args)
{
    CHECK_ARGS_UPPER(1)
    NO_FAKECLIENTS()
    
    if (0 == g_ZonesCount[MAIN_TRACK] || START_ZONE == g_EndZone[MAIN_TRACK])
    {
        Race_Reply(client, "Sorry, there are no stages to race on {OG}:({N}");
        return Plugin_Handled;
    }
    
    if (args == 1)
    {
        decl String:sz_stage[32];
        decl stage;
        GetCmdArg(1, STR(sz_stage));
        
        if (0 == StringToIntEx(sz_stage, stage))
        {
            Race_Reply(client, "\"{G}%s{N}\" is an invalid stage number.", sz_stage);
            return Plugin_Handled;
        }
        
        new zone = stage - 1;
        if (zone < START_ZONE || zone > g_EndZone[MAIN_TRACK])
        {
            Race_Reply(client, "Stage {G}%d{N} doesn't exist.", stage);
            return Plugin_Handled;
        }
        
        if (zone == g_EndZone[MAIN_TRACK])
        {
            Race_Reply(client, "I'm afraid I can't let you race on the {G}last stage{N}, {OG}Dave{N}.");
            return Plugin_Handled;
        }
        
        g_RaceTrack[client] = MAIN_TRACK;
        g_RaceStartZone[client] = zone;
        g_RaceType[client] = RaceType_Stage;
        
        RaceMenu(client);
        
        return Plugin_Handled;
    }
    
    RaceStageMenu(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_BRace(client, args)
{
    CHECK_ARGS_UPPER(1)
    NO_FAKECLIENTS()
    
    // TracksCount always counts the main track, so we test for <=1
    if (g_TracksCount <= 1)
    {
        Race_Reply(client, "Sorry, there are no bonuses to race on {OG}:({N}");
        return Plugin_Handled;
    }
    
    if (args == 1)
    {
        decl String:sz_track[32];
        decl track;
        GetCmdArg(1, STR(sz_track));
        
        if (0 == StringToIntEx(sz_track, track))
        {
            Race_Reply(client, "\"{G}%s{N}\" is an invalid bonus number.", sz_track);
            return Plugin_Handled;
        }
        
        if (track < 0 || track >= MAX_TRACKS)
        {
            Race_Reply(client, "Bonus {G}%d{N} doesn't exist.", track);
            return Plugin_Handled;
        }
        
        if (track == MAIN_TRACK)
        {
            Race_Reply(client, "Bonus {G}%d{N} is too extreme for you, and I can't let you race on it.", track);
            return Plugin_Handled;
        }
        
        g_RaceTrack[client] = track;
        g_RaceStartZone[client] = START_ZONE;
        g_RaceType[client] = RaceType_Bonus;
        
        RaceMenu(client);
        
        return Plugin_Handled;
    }
    
    RaceBonusMenu(client);
    
    return Plugin_Handled;
}
#endif//WINTOX_SURF

public Action:ConCommand_Rstats(client, args)
{
    new target = -1;
    decl String:partial[64];
    decl String:query[128];
    
    if (args == 0)
        target = client;
    else
    {
        GetCmdArgString(STR(partial));
        
        decl String:target_name[MAX_TARGET_LENGTH];
        decl target_list[1], bool:tn_is_ml;

        if (ProcessTargetString(partial, client, target_list, 1,
            COMMAND_FILTER_NO_IMMUNITY|COMMAND_FILTER_NO_MULTI,
            target_name, sizeof(target_name), tn_is_ml) > 0)
        {
            target = target_list[0];
        }
    }
    
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, target);
    WritePackString(pack, partial);
    ResetPack(pack);
    
    if (target == -1 || g_PlayerIDs[target] == -1)
    {
        Format(STR(query), SQL_QUERY[GetPartialPlayerRaceRecords], partial, WINTOX_GAMETYPE);
        SQLT(query, T_Rstats, pack);
    }
    else
    {
        Format(STR(query), SQL_QUERY[GetPlayerRaceRecords], g_PlayerIDs[target]);
        SQLT(query, T_Rstats, pack);
    }
    
    return Plugin_Handled;
}

public T_Rstats(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    decl String:partial[64];
    new Handle:pack = Handle:data;
    
    new client = ReadPackCell(pack);
    new target = ReadPackCell(pack);
    ReadPackString(pack, STR(partial));
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        decl String:req_name[64];
        if (target == -1)
            strcopy(STR(req_name), partial);
        else
        {
            Color_ChatSetSubject(target);
            GetClientName(target, STR(req_name));
        }
        
        LogError("Error retrieving race stats for \"%s\", requested by %L: %s", req_name, client, error);
        Race_Reply(client, "There was a {R}database error{N} requesting race stats for \"%s%s{N}\". Please try again.", (target == -1) ? "{G}" : "{T}", req_name);
        return;
    }
    
    new rows = SQL_GetRowCount(hndl);
    if (rows == 0)
        Race_Reply(client, "No players matched \"%s\"", partial);
    else if (rows == 1)
    {
        decl String:latest_name[64];
        decl racewins;
        decl racelosses;
        
        SQL_FetchRow(hndl);
        SQL_FetchString(hndl, 0, STR(latest_name));
        racewins = SQL_FetchInt(hndl, 1);
        racelosses = SQL_FetchInt(hndl, 2);
        
        Race_Reply(client, "{G}%s{N} - Wins: {G}%d{N}, Losses: {G}%d{N}, Ratio: {G}%.3f{N}", latest_name, racewins, racelosses, float(racewins) / MAX(racelosses, 1));
    }
    else
    {
        new Handle:menu = CreateMenu(MenuHandler_Rstats);
        SetMenuTitle(menu, "Race stats for players matching \"%s\"", partial);
        
        while (SQL_FetchRow(hndl))
        {
            decl String:menu_item[96];
            decl String:latest_name[64];
            decl racewins;
            decl racelosses;
            
            SQL_FetchString(hndl, 0, STR(latest_name));
            racewins = SQL_FetchInt(hndl, 1);
            racelosses = SQL_FetchInt(hndl, 2);
            
            Format(STR(menu_item), "%s - W: %d L: %d %.3f", latest_name, racewins, racelosses, float(racewins) / MAX(racelosses, 1));
            AddMenuItem(menu, latest_name, menu_item);
        }
        
        DisplaySubmenu(menu, client, MENU_TIME_FOREVER);
    }
}

public MenuHandler_Rstats(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public Action:ConCommand_Rtop(client, args)
{
    CHECK_ARGS_UPPER(1)
    
    new start = 0;
    if (args == 1)
    {
        decl String:sz_start[8];
        GetCmdArg(1, STR(sz_start));
        
        if (StringToIntEx(sz_start, start) == 0)
        {
            ReplyWithDescription(client);
            return Plugin_Handled;
        }
        
        // Correct index to 0-based
        start -= 1;
    }
    
    ShowRtop(client, MAX(start, 0));
    
    return Plugin_Handled;
}

ShowRtop(client, start=0)
{
    new Handle:pack = CreateDataPack();
    WritePackCell(pack, client);
    WritePackCell(pack, start);
    ResetPack(pack);
    
    SQLTH(GetRtop, pack, WINTOX_GAMETYPE, start);
}

public T_GetRtop(Handle:owner, Handle:hndl, const String:error[], any:data)
{
    new Handle:pack = Handle:data;
    new client = ReadPackCell(pack);
    new start = ReadPackCell(pack);
    CloseHandle(pack);
    
    if (hndl == INVALID_HANDLE)
    {
        LogError("Error retrieving race top records (for %L, starting at %d): %s", client, start, error);
        Race_Reply(client, "There was a {OG}database error{N} retrieving race records. Please try again.");
        return;
    }
    
    if (client == 0 || !IsClientInGame(client))
        return;
    
    new rows = SQL_GetRowCount(hndl);
    if (rows == 0)
    {
        Race_Reply(client, "There are no race records! {G}Go make some{N}!");
        return;
    }
    
    new Handle:panel = CreatePanel(GetMenuStyleHandle(MenuStyle_Default));
    
    new i = 0;
    decl String:records[512];
    Format(STR(records), "Top racers %d to %d\n \n", start+1, start+rows);
    
    while (SQL_FetchRow(hndl) && i<10)
    {
        decl String:latest_name[64];
        decl racewins;
        decl racelosses;
        
        SQL_FetchString(hndl, 0, STR(latest_name));
        racewins = SQL_FetchInt(hndl, 1);
        racelosses = SQL_FetchInt(hndl, 2);
        
        Format(STR(records), "%s%d. %s - W: %d L: %d %.3f\n", records, start + i + 1, latest_name, racewins, racelosses, float(racewins) / MAX(racelosses, 0));
        
        i++;
    }
    
    DrawPanelItem(panel, records);
    DrawPanelText(panel, " ");
    
    new keys = (1<<0) | (1<<9);
    
    decl String:buffer[32];
    if (start > 0)
    {
        Format(STR(buffer), "Previous");
        SetPanelCurrentKey(panel, 8);
        DrawPanelItem(panel, buffer);
        
        keys |= (1<<7);
    }
    
    if (rows > 10)
    {
        Format(STR(buffer), "Next");
        SetPanelCurrentKey(panel, 9);
        DrawPanelItem(panel, buffer);
        
        keys |= (1<<8);
    }
    
    SetPanelCurrentKey(panel, 10);
    Format(STR(buffer), "Exit");
    DrawPanelItem(panel, buffer);
    
    SetPanelKeys(panel, keys);
    
    g_RaceRtopStart[client] = start;
    SendPanelToClient(panel, client, MenuHandler_Rtop, MENU_TIME_FOREVER);
}

public MenuHandler_Rtop(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        new start = g_RaceRtopStart[param1];
        
        if (param2 == 8)
            ShowRtop(param1, MAX(start - 10, 0));
        else if (param2 == 9)
            ShowRtop(param1, start + 10);
        
        if (param2 == 10)
            MenuPlayExitSound(param1);
        else
            MenuPlaySelectSound(param1);
    }
}

#if defined(WINTOX_DEBUG)
public Action:ConCommand_ForceChallenge(client, args)
{
    EXACT_ARGS(2)
    
    decl String:player[64];
    decl player1, player2;
    
    GetCmdArg(1, STR(player));
    player1 = FindTarget(client, player, false, false);
    if (player1 == -1)
        return Plugin_Handled;
    
    GetCmdArg(2, STR(player));
    player2 = FindTarget(client, player, false, false);
    if (player2 == -1)
        return Plugin_Handled;
    
    new Handle:menu = CreateMenu(MenuHandler_RaceMenu);
    
    new String:sz_userid[8];
    IntToString(GetClientUserId(player2), STR(sz_userid));
    AddMenuItem(menu, sz_userid, "");
    
    MenuHandler_RaceMenu(menu, MenuAction_Select, player1, 0);
    CloseHandle(menu);
    
    return Plugin_Handled;
}
#endif



/* ========================================================================
                                CHALLENGES
   ======================================================================== */

// The challengee declined a race challenge
ChallengeeDeclined(challenger, challengee)
{
    Color_ChatSetSubject(challenger);
    Race_PrintToChat(challenger, "{T}%N{N} has {R}declined{N} your race challenge {OG}:({N}", challengee);
    
    g_RaceChallengedBy[challengee] = 0;
}

// The challengee accepted a race challenge
ChallengeeAccepted(challenger, challengee, RaceType:type, track, start_zone, end_zone)
{
    Color_ChatSetSubject(challenger);
    Race_PrintToChat(challenger, "{T}%N{N} has {R}accepted{N} your race challenge!", challengee);
    Race_PrintToChatAll("{R}%N{N} challenged {R}%N{N} to a race!", challenger, challengee);
    
    g_RaceOpponents[challenger] = challengee;
    g_RaceOpponents[challengee] = challenger;
    g_RaceChallengedBy[challengee] = challenger;
    
    DO_PARTICIPANTS
    {
        g_RaceType[client] = type;
        g_RaceTrack[client] = track;
        g_RaceStartZone[client] = start_zone;
        g_RaceEndZone[client] = end_zone;
    }
    
    InitiateRace(challenger, challengee);
}

#if defined(WINTOX_SURF)
RaceStageMenu(client)
{
    new Handle:menu = CreateMenu(MenuHandler_RaceStages);
    SetMenuTitle(menu, "Which stage will you race on?");
    
    new bool:items_drawn = false;
    for (new zone=START_ZONE; zone<g_EndZone[MAIN_TRACK]; zone++)
    {
        decl String:sz_item[96];
        decl String:sz_info[8];
        
        if (ZONE_EMPTY_T(MAIN_TRACK, zone))
            continue;
        
        IntToString(zone, STR(sz_info));
        
        GetStageName(MAIN_TRACK, zone, STR(sz_item));
        if (zone == g_PlayerCurrentZone[client])
            StrCat(STR(sz_item), " (cur)");
        
        if (AddMenuItem(menu, sz_info, sz_item))
            items_drawn = true;
    }
    
    if (!items_drawn)
        AddMenuItem(menu, "-", "-");
    
    DisplaySubmenu(menu, client, MENU_TIME_FOREVER);
}

public MenuHandler_RaceStages(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        new zone = INVALID_ZONE;
        
        GetMenuItem(menu, param2, STR(info));
        zone = StringToInt(info);
        
        if (zone < START_ZONE || zone >= g_EndZone[MAIN_TRACK])
        {
            Race_Reply(param1, "Somehow, you chose an invalid stage. Sorry. Try that again.");
            FakeClientCommand(param1, "sm_srace");
            return;
        }
        
        g_RaceTrack[param1] = MAIN_TRACK;
        g_RaceStartZone[param1] = zone;
        g_RaceType[param1] = RaceType_Stage;
        
        RaceMenu(param1);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

RaceBonusMenu(client)
{
    new Handle:menu = CreateMenu(MenuHandler_RaceBonus);
    SetMenuTitle(menu, "Which bonus will you race on?");
    
    new bool:items_drawn = false;
    for (new track=FIRST_BONUS_TRACK; track<MAX_TRACKS; track++)
    {
        decl String:sz_item[96];
        decl String:sz_info[8];
        
        if (IS_TRACK_EMPTY(track) || TRACK_EMPTY(track))
            continue;
        
        IntToString(track, STR(sz_info));
        
        GetTrackName(track, STR(sz_item));
        if (track == g_PlayerCurrentTrack[client])
            StrCat(STR(sz_item), " (cur)");
        
        if (AddMenuItem(menu, sz_info, sz_item))
            items_drawn = true;
    }
    
    if (!items_drawn)
        AddMenuItem(menu, "-", "-");
    
    DisplaySubmenu(menu, client, MENU_TIME_FOREVER);
}

public MenuHandler_RaceBonus(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        new track = INVALID_TRACK;
        
        GetMenuItem(menu, param2, STR(info));
        track = StringToInt(info);
        
        if (track <= MAIN_TRACK || track >= MAX_TRACKS || 
            TRACK_EMPTY(track) || IS_TRACK_EMPTY(track))
        {
            Race_Reply(param1, "Somehow, you chose an invalid bonus. Sorry. Try that again.");
            FakeClientCommand(param1, "sm_brace");
            return;
        }
        
        g_RaceTrack[param1] = track;
        g_RaceStartZone[param1] = START_ZONE;
        g_RaceType[param1] = RaceType_Bonus;
        
        RaceMenu(param1);
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}
#endif//WINTOX_SURF

// Bring up a menu of all players in the server for the challenger to choose
// their challengee.
RaceMenu(challenger)
{
    if (g_RaceChallengesAmount == g_RaceOutstandingChallenges[challenger])
    {
        Race_Reply(challenger, "You have reached the maximum amount of simultaneous race challenges ({G}%d{N})!", g_RaceChallengesAmount);
        Race_Reply(challenger, "Please wait for a challenge to {OG}expire{N} or be {G}accepted{N}/{R}declined{N}.");
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
        
        if (!IsClientConnected(client) || !IsClientInGame(client))
            continue;

#if !defined(WINTOX_DEBUG)
        if (IsFakeClient(client))
            continue;
#endif
        
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
        
        if (challengee == 0 || IsRacing(challengee))
        {
            if (challengee == 0)
                Race_Reply(param1, "Unfortunately, that player disappeared and they cannot be challenged. Try someone else.");
            else
            {
                Color_ChatSetSubject(challengee);
                Race_Reply(param1, "{T}%N{N} is in a race. Choose someone else.");
            }
            
            PrevSubmenu(param1, false);
            RaceMenu(param1);
            
            return;
        }
        
        g_RaceChallengedBy[challengee] = param1;
        
        new RaceType:type = g_RaceType[param1];
        new track = g_RaceTrack[param1];
        new start_zone = g_RaceStartZone[param1];
        
        AskRace(param1, challengee, type, track, start_zone);
        g_RaceOutstandingChallenges[param1]++;
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

// Show a dialog on the challengee's screen asking to accept or decline the race
AskRace(challenger, challengee, RaceType:type=RaceType_Linear, track=MAIN_TRACK, zone=START_ZONE)
{
    decl Handle:menu;
    decl end_zone;
    decl String:sz_userid[8];
            
    end_zone = g_EndZone[track];
    if (type == RaceType_Stage)
        end_zone = GetPathOffsetZone(track, zone, 1, end_zone);
    
#if defined(WINTOX_DEBUG)
    if (IsFakeClient(challengee))
    {
        ChallengeeAccepted(challenger, challengee, type, track, zone, end_zone);
        g_RaceOutstandingChallenges[challenger]--;
        return;
    }
#endif
    
    menu = CreateMenu(MenuHandler_AskRace);
    if (type == RaceType_Bonus)
    {
        decl String:track_name[64];
        GetTrackName(track, STR(track_name));
        
        SetMenuTitle(menu, "%N has challenged you to a race on {G}%s{N}", challenger, track_name);
    }
    else if (type == RaceType_Stage)
    {
        decl String:stagename[64];
        GetStageName(track, zone, STR(stagename));
        
        SetMenuTitle(menu, "%N has challenged you to a race on {G}%s{N}", challenger, stagename);
    }
    else
        SetMenuTitle(menu, "%N has challenged you to a race", challenger);
    
    g_RaceType[challengee] = type;
    g_RaceTrack[challengee] = track;
    g_RaceStartZone[challengee] = zone;
    g_RaceEndZone[challengee] = end_zone;
    
    // Add a hidden item containing the challenger's userid
    IntToString(GetClientUserId(challenger), STR(sz_userid));
    AddMenuItem(menu, sz_userid, "", ITEMDRAW_NOTEXT|ITEMDRAW_DISABLED|ITEMDRAW_IGNORE);
    
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
    decl String:sz_challenger_userid[8];
    GetMenuItem(menu, 0, STR(sz_challenger_userid));
    new challenger_userid = StringToInt(sz_challenger_userid);
    new challenger = GetClientOfUserId(challenger_userid);
    
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        
        if (challenger == 0)
        {
            Race_PrintToChat(param1, "Unfortunately, your challenger has left {OG}:(");
            return;
        }
        
        new bool:accepted = false;
        if (strncmp("no", info, 2) == 0)
            ChallengeeDeclined(challenger, param1);
        else if (strncmp("yes", info, 3) == 0)
        {
            accepted = true;
            ChallengeeAccepted(challenger, param1, g_RaceType[param1], g_RaceTrack[param1], g_RaceStartZone[param1], g_RaceEndZone[param1]);
        }
        
        if (StrEqual("taunt", info[2]))
        {
            decl String:sz_taunt[256];
            GetRaceRandomTaunt(accepted, STR(sz_taunt));
            
            Color_ChatSetSubject(challenger);
            Race_PrintToChat(challenger, "{T}%N{N} says: {OG}%s{N}", param1, sz_taunt);
        }
        
        g_RaceOutstandingChallenges[challenger]--;
    }
    else if (action == MenuAction_Cancel)
    {
        g_RaceOutstandingChallenges[challenger]--;
        
        HANDLE_CANCEL();
    }
}




/* ========================================================================
                           RACE MANIPULATION
   ======================================================================== */

// Resets the vars used in racing players (not settings)
ResetRaceVars(client)
{
    g_RaceOpponents[client] = 0;
    g_RaceChallengedBy[client] = 0;
    g_RaceOutstandingChallenges[client] = 0;
    
    g_RaceStart[client] = 0.0;
    g_Racing[client] = false;
    
    g_RaceType[client] = RaceType_Invalid;
    g_RaceTrack[client] = INVALID_TRACK;
    g_RaceStartZone[client] = INVALID_ZONE;
    g_RaceEndZone[client] = INVALID_ZONE;
    
    g_RaceEndElapsed[client] = 0.0;
    g_RaceEndJumps[client] = -1;
    g_RaceEndStyle[client] = STYLE_NORMAL;
}

// Begin the race!
StartRace(challenger, challengee)
{
    
    DO_PARTICIPANTS
    {
        SetEntityMoveType(client, MOVETYPE_ISOMETRIC);
        
        g_RaceStart[client] = GetEngineTime();
        g_Racing[client] = true;
    }
}

// Setup the players at the start zone and begin the countdown timer
InitiateRace(challenger, challengee)
{
    decl Handle:pack;
    new Handle:timer = CreateDataTimer(1.0, Timer_Callback_RaceCountdown, pack, TIMER_REPEAT|TIMER_FLAG_NO_MAPCHANGE);
    
    WritePackCell(pack, 5);
    WritePackCell(pack, challenger);
    WritePackCell(pack, challengee);
    ResetPack(pack);
    
    DO_PARTICIPANTS
    {
        ZoneTeleport(client, g_RaceStartZone[client], g_RaceTrack[client]);
        SetEntityMoveType(client, MOVETYPE_NONE);
        
        g_RaceCountdownTimer[client] = timer;
        g_RaceStart[client] = GetEngineTime() + 6.0;
    }
}

public Action:Timer_Callback_RaceCountdown(Handle:timer, Handle:pack)
{
    new countdown = ReadPackCell(pack);
    new challenger = ReadPackCell(pack);
    new challengee = ReadPackCell(pack);
    ResetPack(pack);
    
    if (countdown == 0)
    {
        DO_PARTICIPANTS Race_PrintToChat(client, "{G}GO!");
        StartRace(challenger, challengee);
        
        return Plugin_Stop;
    }
    else
    {
        DO_PARTICIPANTS
        {
            Color_ChatSetSubject(countdown % 2 == 0 ? challenger : challengee);
            Race_PrintToChat(client, "{OG}<{T}%d{OG}>", countdown);
        }
        
        WritePackCell(pack, --countdown);
        ResetPack(pack);
    }
    
    return Plugin_Continue;
}

// End the race gracefully
EndRace(winner, loser, challenger, challengee)
{
    decl Float:win_time;
    decl String:sz_win_time[32];
    
    win_SQL_IncrementPlayerRaceWins(winner);
    win_SQL_IncrementPlayerRaceLosses(loser);
    
    win_time = GetEngineTime() - g_RaceStart[winner];
    SecondsToTimeString(win_time, STR(sz_win_time));
    
    DO_PARTICIPANTS
    {
        g_RaceWinner[client] = winner;
        
        g_RaceEndJumps[client] = g_Jumps[client];
        g_RaceEndStyle[client] = g_PlayerCurrentStyle[client];
        g_RaceEndElapsed[client] = win_time;
        
        ResetRaceVars(client);
        ResetTimer(client, false);
    }
    
    Color_ChatSetSubject(winner);
#if defined(WINTOX_BHOP)
    Race_PrintToChatAll("{OG}%s{N} | {T}%N{N} won the race in {G}%s{N} with {G}%d{N} jumps!", GET_STYLE_NAME(g_RaceEndStyle[winner]), winner, sz_win_time, g_RaceEndJumps[winner]);
#else
    Race_PrintToChatAll("{OG}%s{N} | {T}%N{N} won the race in {G}%s{N}!", GET_STYLE_NAME(g_RaceEndStyle[winner]), winner, sz_win_time);
#endif
}

// Stop a race prematurely
StopRace(challenger, challengee, const String:reason[]="")
{
    DO_PARTICIPANTS
    {
        ResetRaceVars(client);
        StopTiming(client);
        
        if (IsClientConnected(client))
        {
            decl String:buffer[256];
            strcopy(STR(buffer), "The race has {R}stopped{N}!");
            
            if (!IS_STR_EMPTY(reason))
                Format(STR(buffer), "%s Reason: {OG}%s{N}", buffer, reason);
            
            Race_PrintToChat(client, "%s", buffer);
        }
    }
}




/* ========================================================================
                          ZONE DETECTION HANDLING
   ======================================================================== */

bool:RaceEndConditionsMet(client, track, zone)
{
    return track == g_RaceTrack[client] && zone == g_RaceEndZone[client];
}

Race_PlayerReachedZone(client, track, zone)
{
    if (RaceEndConditionsMet(client, track, zone))
    {
        Race_PlayerReachedEnd(client, track);
        return;
    }

    ZoneTime(client, zone);
    g_PlayerCurrentPath[client]++;
    
    Timer_DisplayPlusMinus(client);
}

Race_PlayerReachedEnd(client, track)
{
// I hate warnings
#pragma unused track

    StopTiming(client);
    
    g_PlayerCompletedRun[client] = true;
    
    decl challenger, challengee, loser;
    if (g_RaceChallengedBy[client] == 0)
    {
        challenger = client;
        loser = challengee = g_RaceOpponents[client];
    }
    else
    {
        loser = challenger = g_RaceChallengedBy[client];
        challengee = client;
    }
    
    EndRace(client, loser, challenger, challengee);
}



/* ========================================================================
                                SQL BIZNIZ
   ======================================================================== */

win_SQL_IncrementPlayerRaceWins(client)
{
    decl String:query[128];
    
    if (g_PlayerIDs[client] == -1)
    {
#if defined(WINTOX_DEBUG)
        LogError("Tried to increment player race wins on invalid client.");
#endif
        return;
    }
    
    Format(STR(query), SQL_QUERY[IncrementPlayerRaceWins], g_PlayerIDs[client]);
    SQLT(query, T_IncrementPlayerRaceWins, client);
}
SQLT_HANDLER(IncrementPlayerRaceWins, "Error incrementing player %L race wins: %s", data)


win_SQL_IncrementPlayerRaceLosses(client)
{
    decl String:query[128];
    
    if (g_PlayerIDs[client] == -1)
    {
#if defined(WINTOX_DEBUG)
        LogError("Tried to increment player race losses on invalid client.");
#endif
        return;
    }
    
    Format(STR(query), SQL_QUERY[IncrementPlayerRaceLosses], g_PlayerIDs[client]);
    SQLT(query, T_IncrementPlayerRaceLosses, client);
}
SQLT_HANDLER(IncrementPlayerRaceLosses, "Error incrementing player %L race losses: %s", data)



/* ========================================================================
                            UTILITY FUNCTIONS
   ======================================================================== */

stock IsRacing(client)
{
    return g_RaceOpponents[g_RaceOpponents[client]] == client;
}

stock IsRaceStarted(client)
{
    return g_Racing[client];
}

stock IsRaceEnded(client)
{
    return g_RaceEndElapsed[client] > 0.0;
}

stock RaceShowHideMessage(client)
{
    if (g_HideOtherPlayers[client])
    {
        Color_ChatSetSubject(g_RaceOpponents[client]);
        Race_Reply(client, "No longer {OG}showing{N} your {T}race opponent{N}.");
    }
    else
        Race_Reply(client, "{OG}Showing{N} all other players.");
}

stock GetRaceRandomTaunt(bool:accepted, String:buffer[], maxlength)
{
    new num_taunts = 0;
    if (accepted)
        num_taunts = sizeof(g_RaceTauntsAccept);
    else
        num_taunts = sizeof(g_RaceTauntsDecline);
    
    new taunt_idx = GetRandomInt(0, num_taunts-1);
    return strcopy(MSTR(buffer), accepted ? g_RaceTauntsAccept[taunt_idx] : g_RaceTauntsDecline[taunt_idx]);
}

