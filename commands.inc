/**
 * Common concommand functionality, including registration and !commands menu.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#pragma semicolon 1

#include <sourcemod>
#include "./stocks"

new Handle:g_PublicCommands = INVALID_HANDLE;
new Handle:g_AdminCommands = INVALID_HANDLE;

Commands_Init()
{
    g_PublicCommands = CreateArray();
    g_AdminCommands = CreateArray();
    
    RegisterCommand("sm_commands", Command_Commands, "Displays all the public commands");
    RegisterCommand("sm_admcommands", Command_AdmCommands, "Displays all the admin commands", 0, ADMFLAG_GENERIC);
}

RegisterCommand(const String:cmd[], ConCmd:callback, const String:description[]="", flags=0, adminflags=0, const String:group[]="")
{
    new Handle:pack = CreateDataPack();
    WritePackString(pack, cmd);
    WritePackString(pack, description);
    ResetPack(pack);
    
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
    DisplayCommands(client, g_PublicCommands);
    return Plugin_Handled;
}

public Action:Command_AdmCommands(client, args)
{
    DisplayCommands(client, g_AdminCommands);
    return Plugin_Handled;
}

DisplayCommands(client, Handle:commands)
{
    new length = GetArraySize(commands);
    
    // Output a text list of the commands
    if (client == 0)
    {
        PrintToServer("[Wintox] Public Commands");
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
}
