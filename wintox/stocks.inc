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

#define MIN(%1,%2) ((%1) < (%2) ? (%1) : (%2))
#define MAX(%1,%2) ((%1) > (%2) ? (%1) : (%2))

#define IS_STR_EMPTY(%1) ((%1)[0] == '\0')
#define STR(%1) (%1), sizeof((%1))

#define ARRAY(%1)[%2] GetArrayCell((%1), (%2))
#define ARRAY_SET(%1)[%2]\32=\32%3 SetArrayCell((%1), (%2), (%3))

#define VEC_PRINT(%1) (%1)[0], (%1)[1], (%1)[2]

#define NOT_INVALID_HANDLE Handle:-1

#define ENTER_REPLY(%1) new ReplySource:__old_reply_to = GetCmdReplySource(); \
    SetCmdReplySource((%1))
#define EXIT_REPLY() SetCmdReplySource(__old_reply_to)


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
    
    if (client != 0)
        Client_PrintToChat(client, false, "[{R}WTX{N}] %s", buffer);
    Client_PrintToConsole(client, "[WTX] %s", buffer);
}

#define Timer_Reply(%1,%2) ReplyTag((%1), "Timer", %2)
#define Race_Reply(%1,%2) ReplyTag((%1), "Race", %2)
#define Tele_Reply(%1,%2) ReplyTag((%1), "Tele", %2)
stock ReplyTag(client, const String:tag[], const String:format[], any:...)
{
    decl String:buffer[1024];
    VFormat(STR(buffer), format, 4);
    
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