/**
 * Common concommand functionality, including registration and !commands menu.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_commands_included
 #endinput
#endif
#define _wintox_commands_included

#pragma semicolon 1

#include <sourcemod>
#include "./include/stocks"
#include "./include/version"
#include "./include/menus"

new Handle:g_PublicCommands = INVALID_HANDLE;
new Handle:g_AdminCommands = INVALID_HANDLE;
new Handle:g_CommandDescriptions = INVALID_HANDLE;

new Handle:g_PubCmdsMenu = INVALID_HANDLE;
new Handle:g_AdmCmdsMenu = INVALID_HANDLE;

Commands_Init()
{
    g_PublicCommands = CreateArray();
    g_AdminCommands = CreateArray();
    g_CommandDescriptions = CreateTrie();
    
    RegisterCommand("sm_commands", Command_Commands, "Displays all the public commands");
    RegisterCommand("sm_admcommands", Command_AdmCommands, "Displays all the admin commands", 0, ADMFLAG_GENERIC);
    
    CreateConVar("wintox_version", WINTOX_VERSION, "Version of Wintox core", FCVAR_SPONLY|FCVAR_DONTRECORD);
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

public Action:Command_Commands(client, args)
{
    DisplayCommands(client);
    return Plugin_Handled;
}

public Action:Command_AdmCommands(client, args)
{
    DisplayCommands(client, false);
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
        PrintToServer("[Wintox] %s", title);
        
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
                g_PubCmdsMenu = BuildCommandsMenu(commands, title);
            menu = g_PubCmdsMenu;
        }
        else
        {
            if (g_AdmCmdsMenu == INVALID_HANDLE)
                g_AdmCmdsMenu = BuildCommandsMenu(commands, title);
            menu = g_AdmCmdsMenu;
        }
        
        DisplaySubmenu(menu, client, MENU_TIME_FOREVER, false);
    }
}

Handle:BuildCommandsMenu(Handle:commands, const String:title[])
{
    new Handle:menu = CreateMenu(MenuHandler_DisplayCommands);
    SetMenuTitle(menu, title);
    
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
        AddMenuItem(descmenu, info, description, ITEMDRAW_DISABLED);
        
        DisplaySubmenu(descmenu, param1, MENU_TIME_FOREVER, true, GetMenuSelectionPosition());
    }
    else if (action == MenuAction_Cancel)
        CancelSubmenus(param1);
}

public MenuHandler_DisplayDescription(Handle:menu, MenuAction:action, param1, param2)
{
    // Redisplay the menu if the description item was selected
    if (action == MenuAction_Select)
        DisplayMenu(menu, param1, MENU_TIME_FOREVER);
    
    // Redisplay the commands menu if Back was selected
    else if (action == MenuAction_Cancel && param2 == MenuCancel_ExitBack)
        PrevSubmenu(param1);
}
