/**
 * Utilities that don't belong in stocks.inc
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_utils_included
 #endinput
#endif
#define _wintox_utils_included

new bool:g_ClientTextEntry[MAXPLAYERS] = { false, ... };
new any:g_ClientTextEntryData[MAXPLAYERS] = { INVALID_HANDLE, ... };
new bool:g_ClientTextEntryRegurgitate[MAXPLAYERS] = { true, ... };

#include "./wintox/stocks"
#include "./wintox/events"
#include "./wintox/game"


Utils_Init()
{
    AddCommandListener(CmdHook_Say_TextEntry, "say");
    AddCommandListener(CmdHook_Say_TextEntry, "say_team");
}

Utils_DeinitPlayer(client)
{
    g_ClientTextEntry[client] = false;
    g_ClientTextEntryData[client] = INVALID_HANDLE;
    g_ClientTextEntryRegurgitate[client] = true;
}

// Intercepts the next message the player enters and calls the OnTextEntryComplete forward.
// This is useful when an arbitrary string is required of the player in a menu.
// If `regurgitate` is true, the text entry hook will show the user what they entered.
stock BeginTextEntry(client, const String:reason[]="", any:data=INVALID_HANDLE, bool:regurgitate=true)
{
    WTX_PrintToChat(client, "A text entry is required. Your next chat message will be used.");
    if (!IS_STR_EMPTY(reason))
        WTX_PrintToChat(client, "Reason: %s", reason);
    
    g_ClientTextEntry[client] = true;
    g_ClientTextEntryData[client] = data;
    g_ClientTextEntryRegurgitate[client] = regurgitate;
}

public Action:CmdHook_Say_TextEntry(client, const String:command[], argc)
{
    if (!g_ClientTextEntry[client])
        return Plugin_Continue;
    
    decl String:text[256];
    GetCmdArg(1, STR(text));
    
    if (text[0] == '"')
        strcopy(STR(text), text[1]);
    
    new len = strlen(text);
    if (text[len-1] == '"')
        text[len-1] = '\0';
    
    g_ClientTextEntry[client] = false;
    
    if (g_ClientTextEntryRegurgitate[client])
        WTX_PrintToChat(client, "Text entry complete! You entered: \"%s\"", text);
    
    Call_StartForward(g_fwdTextEntryComplete);
    Call_PushCell(client);
    Call_PushString(text);
    Call_PushCell(g_ClientTextEntryData[client]);
    Call_Finish();
    
    g_ClientTextEntryData[client] = INVALID_HANDLE;
    g_ClientTextEntryRegurgitate[client] = true;
    
    // Return Plugin_Handled, so the message isn't displayed
    return Plugin_Handled;
}

