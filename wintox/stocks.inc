/**
 * Stocks and other useful utilities for Wintox
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_stocks_included
 #endinput
#endif
#define _wintox_stocks_included

#include <smlib>
#include "./wintox/game"

new g_DebugLevel = 0;

#define DEBUG_ERROR 0
#define DEBUG_WARN 1
#define DEBUG_INFO 2
#define DEBUG_DEBUG 3
#define DEBUG_CRAZYDEBUG 4
#define MAX_DEBUG_LEVELS 5

new String:DEBUG_NAMES[][] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "CRAZYDBG"
};


#define MIN(%1,%2) ((%1) < (%2) ? (%1) : (%2))
#define MAX(%1,%2) ((%1) > (%2) ? (%1) : (%2))

#define IS_STR_EMPTY(%1) ((%1)[0] == '\0')
#define STR(%1) (%1), sizeof((%1))
#define MSTR(%1) (%1), maxlength

#define ARRAY(%1)[%2] GetArrayCell((%1), (%2))
#define ARRAY_SET(%1)[%2]\32=\32%3 SetArrayCell((%1), (%2), (%3))

#define VEC_PRINT(%1) (%1)[0], (%1)[1], (%1)[2]

#define NOT_INVALID_HANDLE Handle:-1

#define PLURAL(%1) ((%1) == 1 ? "" : "s")

#define ENTER_REPLY(%1) new ReplySource:__old_reply_to = GetCmdReplySource(); \
    SetCmdReplySource((%1))
#define EXIT_REPLY() SetCmdReplySource(__old_reply_to)

#define HAS_FLAG(%1,%2) (((%1) & (%2)) == (%2))
#define REMOVE_FLAG(%1,%2) ((%1) & ~(%2))
#define SET_REMOVE_FLAG(%1,%2) (%1) = REMOVE_FLAG(%1,%2)
#define ADD_FLAG(%1,%2) ((%1) | (%2))
#define SET_ADD_FLAG(%1,%2) (%1) = ADD_FLAG(%1,%2)

stock void() { }

// For debugging stacks
stock PrintStack(Handle:stack)
{
    new Handle:temp = CreateStack();
    decl data;
    new length = 0;
    
    while (PopStackCell(stack, data) && ++length)
        PushStackCell(temp, data);
    
    for (new i=0; PopStackCell(temp, data); i++)
    {
        PrintToServer("%3d %d", i, data);
        PushStackCell(stack, data);
    }
}

stock Reply(client, const String:format[], any:...)
{
    decl String:buffer[1024];
    VFormat(STR(buffer), format, 3);
    
    Color_ChatSetSubject(client);
    
    if (client != 0)
        Client_PrintToChat(client, false, "[{R}WTX{N}] %s", buffer);
    Client_PrintToConsole(client, "[WTX] %s", buffer);
}

#define Timer_Reply(%1,%2) ReplyTag((%1), "Timer", %2)
#define Race_Reply(%1,%2) ReplyTag((%1), "Race", %2)
#define Tele_Reply(%1,%2) ReplyTag((%1), "Tele", %2)
#define Zone_Reply(%1,%2) ReplyTag((%1), "Stage", %2)
stock ReplyTag(client, const String:tag[], const String:format[], any:...)
{
    decl String:buffer[1024];
    VFormat(STR(buffer), format, 4);
    
    Color_ChatSetSubject(client);
    
    if (client != 0)
        Client_PrintToChat(client, false, "{OG}[{R}%s{OG}]{N} %s", tag, buffer);
    Client_PrintToConsole(client, "{OG}[{R}%s{OG}]{N} %s", tag, buffer);
}

#define WTX_PrintToChat(%1,%2) Tag_PrintToChat((%1), "WTX", %2)
#define Timer_PrintToChat(%1,%2) Tag_PrintToChat((%1), "Timer", %2)
#define Race_PrintToChat(%1,%2) Tag_PrintToChat((%1), "Race", %2)
stock Tag_PrintToChat(client, const String:tag[], const String:format[], any:...)
{
    decl String:buffer[1024];
    VFormat(STR(buffer), format, 4);
    
    Color_ChatSetSubject(client);
    
    Client_PrintToChat(client, false, "{OG}[{R}%s{OG}]{N} %s", tag, buffer);
}

#define Race_PrintToChatAll(%1) Tag_PrintToChatAll("Race", %1)
#define Timer_PrintToChatAll(%1) Tag_PrintToChatAll("Timer", %1)
stock Tag_PrintToChatAll(const String:tag[], const String:format[], any:...)
{
    decl String:buffer[1024];
    VFormat(STR(buffer), format, 3);
    
    Client_PrintToChatAll(false, "{OG}[{R}%s{OG}]{N} %s", tag, buffer);
}

stock HideHintText(client)
{
    new Handle:userMessage = StartMessageOne("HintText", client);
    BfWriteByte(userMessage, 0);
    EndMessage();
}

stock Float:FloatMod(Float:value, Float:divisor)
{
    return value - RoundToFloor(value / divisor) * divisor;
}

stock Float:FloatRoundPlaces(Float:f, places=TIMER_MAX_PLACES)
{
    new Float:frac = FloatFraction(f);
    new base = RoundToFloor(f);
    
    new places_val = places * 10;
    new Float:new_frac = float(RoundToFloor(frac * places_val)) / places_val;
    
    return float(base) + new_frac;
}

stock FloatComparePlaces(Float:fOne, Float:fTwo, places=TIMER_MAX_PLACES)
{
    new Float:res = FloatRoundPlaces(fOne - fTwo, places);
    return FloatCompare(res, 0.0);
}

stock IsOnPlayingTeam(client)
{
    return IS_PLAYING_TEAM(GetClientTeam(client));
}

stock Float:GetPlayerVelocity(client)
{
    decl Float:velocity[3];
    Entity_GetAbsVelocity(client, velocity);
    
    return GetVectorLength(velocity);
}

#if defined(WINTOX_DEBUG)
    #define ERROR(%1) DebugPrintLevel(DEBUG_ERROR, %1)
    #define WARN(%1) DebugPrintLevel(DEBUG_WARN, %1)
    #define INFO(%1) DebugPrintLevel(DEBUG_INFO, %1)
    #define DEBUG(%1) DebugPrintLevel(DEBUG_DEBUG, %1)
    #define CRAZYDBG(%1) DebugPrintLevel(DEBUG_CRAZYDEBUG, %1)
#else
    #define ERROR(%1) void()
    #define WARN(%1) void()
    #define INFO(%1) void()
    #define DEBUG(%1) void()
    #define CRAZYDBG(%1) void()
#endif

stock DebugPrintLevel(level, const String:format[], any:...)
{
    if (g_DebugLevel < level)
        return;
    
    decl String:buffer[1024];
    VFormat(STR(buffer), format, 3);
    
    LogMessage("[%s] %s", DEBUG_NAMES[level], buffer);
}

stock StringToLower(const String:input[], String:output[], size)
{
	size--;

	new x=0;
	while (input[x] != '\0' && x < size) {

		if (IsCharUpper(input[x])) {
			output[x] = CharToLower(input[x]);
		}
		else {
			output[x] = input[x];
		}

		x++;
	}

	output[x] = '\0';
}

// Lowercases the first word in a string, then prepends another string
// e.g. PrependCapitals("Time: 00:00", "Stage") -> "Stage time: 00:00"
// Be damn sure strlen(orig) >= 1
stock PrependCapitals(const String:orig[], const String:prepend[], String:buffer[], maxlength)
{
    Format(MSTR(buffer), "%s %c%s", prepend, CharToLower(orig[0]), orig[1]);
}
