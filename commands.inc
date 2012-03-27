/**
 * Common concommand functionality, including registration and !commands menu.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#pragma semicolon 1

#include <sourcemod>
#include "./stocks"

new Handle:g_PublicCommands = CreateArray();
new Handle:g_AdminCommands = CreateArray();

Commands_Init()
{
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
    }
}