/**
 * Common concommand functionality, including registration and !commands menu.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_commands_included
 #endinput
#endif
#define _wintox_commands_included

#include <sourcemod>
#include "./wintox/stocks"
#include "./wintox/version_build"
#include "./wintox/menus"

#define RESET_REPLY() //SetCmdReplySource(SM_REPLY_TO_CONSOLE);

#define EXACT_ARGS(%1) \
    if (args != (%1)) \
    { \
        ReplyWithDescription(client); \
        return Plugin_Handled; \
    }

#define CHECK_ARGS_UPPER(%1) \
    if (args > (%1)) \
    { \
        ReplyWithDescription(client); \
        return Plugin_Handled; \
    }

#define CHECK_ARGS_LOWER(%1) \
    if (args < (%1)) \
    { \
        ReplyWithDescription(client); \
        return Plugin_Handled; \
    }

#define CHECK_ARGS_SPAN(%1,%2) \
    if (args < (%1) || args > (%2)) \
    { \
        ReplyWithDescription(client); \
        return Plugin_Handled; \
    }

#define NO_ARGS() CHECK_ARGS_UPPER(0)

#define NO_FAKECLIENTS() \
    if (IsFakeClient(client)) \
        return Plugin_Handled;

new Handle:g_PublicCommands = INVALID_HANDLE;
new Handle:g_AdminCommands = INVALID_HANDLE;
new Handle:g_CommandDescriptions = INVALID_HANDLE;

new Handle:g_PubCmdsMenu = INVALID_HANDLE;

Commands_Init()
{
    g_PublicCommands = CreateArray();
    g_AdminCommands = CreateArray();
    g_CommandDescriptions = CreateTrie();
    
    RegisterCommand("sm_commands", Command_Commands, "sm_commands | Displays all the public commands");
    RegisterCommand("sm_help", Command_Commands, "sm_help | Displays all the public commands");
    RegisterCommand("sm_admcommands", Command_AdmCommands, "sm_admcommands | Displays all the admin commands", 0, ADMFLAG_GENERIC);
    RegisterCommand("sm_cmd", Command_Cmd, "sm_cmd <command> | Prints the usage of specified command.");
}

RegisterCommand(const String:cmd[], ConCmd:callback, const String:description[]="", flags=0, adminflags=0, const String:group[]="")
{
    new Handle:pack = CreateDataPack();
    WritePackString(pack, cmd);
    WritePackString(pack, description);
    ResetPack(pack);
    
    SetTrieString(g_CommandDescriptions, cmd, description);
    
    if (adminflags == 0)
    {
        RegConsoleCmd(cmd, callback, description, flags);
        PushArrayCell(g_PublicCommands, pack);
    }
    else
    {
        RegAdminCmd(cmd, callback, adminflags, description, group, flags);
        PushArrayCell(g_AdminCommands, pack);
    }
}

ReplyWithCmdDescription(client, const String:cmd[])
{
    decl String:buffer[512];
    
    StringToLower(cmd, STR(buffer));
    if (!GetTrieString(g_CommandDescriptions, buffer, STR(buffer)))
    {
        Reply(client, "Command {OG}%s{N} doesn't exist, or isn't a part of Wintox.", buffer);
        return;
    }
    
    if (!CheckCommandAccess(client, buffer, 0))
    {
        Reply(client, "You do not have access to {OG}%s{N}.", buffer);
        return;
    }
    
    // If our description has a pipe, let's give it some flair
    new pipe = FindCharInString(buffer, '|');
    if (pipe != -1)
    {
        buffer[pipe] = '\0';
        Format(STR(buffer), "{OG}%s{N}|%s", buffer, buffer[pipe+1]);
    }
    
    Reply(client, buffer);
}

/**
 * Inside a concommand callback, uses ReplyToCommand to display the command
 * description to the client.
 */
ReplyWithDescription(client)
{
    decl String:buffer[64];
    GetCmdArg(0, STR(buffer));
    
    ReplyWithCmdDescription(client, buffer);
}

public Action:Command_Commands(client, args)
{
    NO_ARGS()
    NO_FAKECLIENTS()
    
    DisplayCommands(client);
    return Plugin_Handled;
}

public Action:Command_AdmCommands(client, args)
{
    NO_ARGS()
    NO_FAKECLIENTS()
    
    DisplayCommands(client, false);
    return Plugin_Handled;
}

public Action:Command_Cmd(client, args)
{
    EXACT_ARGS(1)
    
    decl String:sz_cmd[64];
    GetCmdArg(1, STR(sz_cmd));
    
    if (sz_cmd[0] == '/' || sz_cmd[0] == '!')
        strcopy(STR(sz_cmd), sz_cmd[1]);
    
    if (strncmp("sm_", sz_cmd, 3) != 0)
        Format(STR(sz_cmd), "sm_%s", sz_cmd);
    
    if (!Convar_IsValidName(sz_cmd))
    {
        Reply(client, "\"{OG}%s{N}\" is an invalid command.", sz_cmd);
        return Plugin_Handled;
    }
    
    ReplyWithCmdDescription(client, sz_cmd);
    return Plugin_Handled;
}

DisplayCommands(client, bool:pub=true)
{
    new Handle:commands = pub ? g_PublicCommands : g_AdminCommands;
    decl String:title[16];
    Format(title, sizeof(title), "%s Commands", pub ? "Public" : "Admin");
    
    // Output a text list of the commands
    if (client == 0)
    {
        PrintToServer("[WTX] %s", title);
        
        new length = GetArraySize(commands);
        for (new i=0; i<length; i++)
        {
            decl String:cmd[256];
            decl String:description[512];
            
            new Handle:pack = GetArrayCell(commands, i);
            ResetPack(pack);
            ReadPackString(pack, cmd, sizeof(cmd));
            ReadPackString(pack, description, sizeof(description));
            ResetPack(pack);
            
            PrintToServer("  %s - %s", cmd, description);
        }
    }
    else
    {
        decl Handle:menu;
        
        if (pub)
        {
            if (g_PubCmdsMenu == INVALID_HANDLE)
                g_PubCmdsMenu = BuildCommandsMenu(client, commands, title);
            menu = g_PubCmdsMenu;
            DisplaySubmenu(menu, client, MENU_TIME_FOREVER, false);
        }
        else
        {
            menu = BuildCommandsMenu(client, commands, title, true);
            DisplaySubmenu(menu, client, MENU_TIME_FOREVER);
        }
        
    }
}

Handle:BuildCommandsMenu(client, Handle:commands, const String:title[], bool:admin=false)
{
    new Handle:menu = CreateMenu(MenuHandler_DisplayCommands);
    SetMenuTitle(menu, title);
    
    new length = GetArraySize(commands);
    for (new i=0; i<length; i++)
    {
        decl String:cmd[256];
        decl String:description[512];
        
        new Handle:pack = ARRAY(commands)[i];
        ResetPack(pack);
        ReadPackString(pack, cmd, sizeof(cmd));
        ReadPackString(pack, description, sizeof(description));
        ResetPack(pack);
        
        if (admin && !CheckCommandAccess(client, cmd, ADMFLAG_GENERIC))
            continue;
        
        decl String:display[256];
        Format(display, sizeof(display), "/%s", cmd[3]);
        AddMenuItem(menu, cmd, display);
    }
    
    return menu;
}

public MenuHandler_DisplayCommands(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:description[512];
        decl String:info[256];
        
        GetMenuItem(menu, param2, info, sizeof(info));
        GetTrieString(g_CommandDescriptions, info, description, sizeof(description));
        
        new Handle:descmenu = CreateMenu(MenuHandler_DisplayDescription);
        SetMenuTitle(descmenu, "%s Description", info);
        SetMenuExitButton(descmenu, true);
        SetMenuExitBackButton(descmenu, true);
        AddMenuItem(descmenu, "", description, ITEMDRAW_DISABLED);
        AddMenuItem(descmenu, info, "Execute this command");
        
        DisplaySubmenu(descmenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
    }
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}

public MenuHandler_DisplayDescription(Handle:menu, MenuAction:action, param1, param2)
{
    // Redisplay the menu if the description item was selected
    if (action == MenuAction_Select)
    {
        decl String:cmd[256];
        GetMenuItem(menu, param2, STR(cmd));
        
        // The client selected "Execute this command"
        if (strlen(cmd) > 0)
            FakeClientCommand(param1, "say \"/%s\"", cmd[3]);
        else
            DisplayMenu(menu, param1, MENU_TIME_FOREVER);
    }
    
    // Redisplay the commands menu if Back was selected
    else if (action == MenuAction_Cancel)
        HANDLE_CANCEL();
}
