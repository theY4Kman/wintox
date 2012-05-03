/**
 * Teleportation commands and menus
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_teleport_included
 #endinput
#endif
#define _wintox_teleport_included

#include "./include/stocks"
#include "./include/commands"

#define INVALID_TELEPORT -1
#define MAX_TELEPORT_SAVES 3

#define TELEPORT_EMPTY(%1,%2) (g_PlayerTele[(%1)][(%2)][0] == 0.0 && g_PlayerTele[(%1)][(%2)][1] == 0.0 && g_PlayerTele[(%1)][(%2)][2] == 0.0)

new Float:g_PlayerTele[MAXPLAYERS][MAX_TELEPORT_SAVES][3];
new g_PlayerCurrentTele[MAXPLAYERS] = { INVALID_TELEPORT, ... };
new g_PlayerTeleCount[MAXPLAYERS];
new Handle:g_TeleMenu[MAXPLAYERS] = { INVALID_HANDLE, ... };


Teleport_Init()
{
    RegisterCommand("sm_telemenu", ConCommand_TeleMenu, "sm_telemenu | Opens a menu providing easy access to the teleport commands.");
    RegisterCommand("sm_save", ConCommand_Save, "sm_save | Saves your current location for use with !tele (maximum  of 3 saves).");
    RegisterCommand("sm_tele", ConCommand_Tele, "sm_tele | Teleports to the currently selected save location.");
    RegisterCommand("sm_next", ConCommand_Next, "sm_next | Selects the next saved location for use with !tele.");
    RegisterCommand("sm_previous", ConCommand_Previous, "sm_previous | Selects the previous saved location for use with !tele.");
    RegisterCommand("sm_delete", ConCommand_Delete, "sm_delete | Deletes the currently selected saved location and selects the next available one.");
}


/* ========================================================================
                               CONCOMMANDS
   ======================================================================== */

public Action:ConCommand_TeleMenu(client, args)
{
    NO_ARGS()
    
    DisplayTeleMenu(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_Save(client, args)
{
    NO_ARGS()
    
    Teleport_Save(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_Tele(client, args)
{
    NO_ARGS()
    
    Teleport_Tele(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_Next(client, args)
{
    NO_ARGS()
    
    Teleport_Next(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_Previous(client, args)
{
    NO_ARGS()
    
    Teleport_Previous(client);
    
    return Plugin_Handled;
}

public Action:ConCommand_Delete(client, args)
{
    NO_ARGS()
    
    Teleport_Delete(client);
    
    return Plugin_Handled;
}



/* ========================================================================
                               MENU SHAZBOT
   ======================================================================== */

#define TELE_MENU_SAVE "save"
#define TELE_MENU_TELE "tele"
#define TELE_MENU_NEXT "next"
#define TELE_MENU_PREV "prev"
#define TELE_MENU_DELETE "delete"

BuildTeleMenu(client)
{
    if (g_TeleMenu[client] != INVALID_HANDLE)
        CloseHandle(g_TeleMenu[client]);
    
    g_TeleMenu[client] = CreateMenu(MenuHandler_TeleMenu);
    SetMenuTitle(g_TeleMenu[client], "Teleport Menu (%d of 3 slots used)", g_PlayerTeleCount[client]);
    
    AddMenuItem(g_TeleMenu[client], TELE_MENU_SAVE, "Save - save location to empty slot");
    AddMenuItem(g_TeleMenu[client], TELE_MENU_TELE, "Teleport - teleport to selected slot");
    AddMenuItem(g_TeleMenu[client], TELE_MENU_NEXT, "Next - choose next slot");
    AddMenuItem(g_TeleMenu[client], TELE_MENU_PREV, "Previous - choose previous slot");
    AddMenuItem(g_TeleMenu[client], TELE_MENU_DELETE, "Delete - clear current slot");
}

DisplayTeleMenu(client)
{
    if (g_TeleMenu[client] == INVALID_HANDLE)
        BuildTeleMenu(client);
    
    DisplaySubmenu(g_TeleMenu[client], client, MENU_TIME_FOREVER, false);
}

public MenuHandler_TeleMenu(Handle:menu, MenuAction:action, param1, param2)
{
    if (action == MenuAction_Select)
    {
        decl String:info[16];
        GetMenuItem(menu, param2, STR(info));
        ENTER_REPLY(SM_REPLY_TO_CHAT);
        
        PrevSubmenu(param1, false);
        
        if (StrEqual(info, TELE_MENU_SAVE))
            Teleport_Save(param1);
        else if (StrEqual(info, TELE_MENU_TELE))
            Teleport_Tele(param1);
        else if (StrEqual(info, TELE_MENU_NEXT))
            Teleport_Next(param1);
        else if (StrEqual(info, TELE_MENU_PREV))
            Teleport_Previous(param1);
        else if (StrEqual(info, TELE_MENU_DELETE))
            Teleport_Delete(param1);
        
        EXIT_REPLY();
        DisplaySubmenu(g_TeleMenu[param1], param1, MENU_TIME_FOREVER, false);
    }
    else if (action == MenuAction_Cancel)
        CancelSubmenus(param1);
}


/* ========================================================================
                             HELPER FUNCTIONS
   ======================================================================== */

GetNextTeleForSave(client, start=0)
{
    start = MAX(start, 0);
    
    for (new i=start; i<MAX_TELEPORT_SAVES; i++)
        if (TELEPORT_EMPTY(client, i))
            return i;
    
    for (new i=0; i<start; i++)
        if (TELEPORT_EMPTY(client, i))
            return i;
    
    return INVALID_TELEPORT;
}

GetNextTele(client, start=0)
{
    start = MAX(start, 0);
    
    for (new i=start+1; i<MAX_TELEPORT_SAVES; i++)
        if (!TELEPORT_EMPTY(client, i))
            return i;
    
    for (new i=0; i<start; i++)
        if (!TELEPORT_EMPTY(client, i))
            return i;
    
    return INVALID_TELEPORT;
}

GetPreviousTele(client, start=0)
{
    start = MAX(start, 0);
    
    for (new i=start-1; i>=0; i--)
        if (!TELEPORT_EMPTY(client, i))
            return i;
    
    for (new i=MAX_TELEPORT_SAVES-1; i>start; i--)
        if (!TELEPORT_EMPTY(client, i))
            return i;
    
    return INVALID_TELEPORT;
}

Teleport_Save(client)
{
    new next_save = GetNextTeleForSave(client, g_PlayerCurrentTele[client]);
    if (next_save == INVALID_TELEPORT)
    {
        Tele_Reply(client, "All {T}3{N} of your location slots have been saved to. Use {T}!delete{N} to clear one.");
        return;
    }
    
    GetClientAbsOrigin(client, g_PlayerTele[client][next_save]);
    g_PlayerTeleCount[client]++;
    
    Tele_Reply(client, "Saved ({OG}%.3f{N},{OG}%.3f{N},{OG}%.3f{N}), {T}%d{N} of {T}3{N} slots filled", g_PlayerTele[client][next_save][0], g_PlayerTele[client][next_save][1], g_PlayerTele[client][next_save][2], g_PlayerTeleCount[client]);
    
    BuildTeleMenu(client);
    
    // Set the selected teleport save to this one if the player doesn't have
    // a selected teleport, yet.
    if (g_PlayerCurrentTele[client] == INVALID_TELEPORT)
        g_PlayerCurrentTele[client] = next_save;
}

Teleport_Tele(client)
{
    if (g_PlayerCurrentTele[client] == INVALID_TELEPORT)
    {
        Tele_Reply(client, "You do not currently have any saved locations. Use {T}!save{N} to resolve that.");
        return;
    }
    
    TeleportEntity(client, g_PlayerTele[client][g_PlayerCurrentTele[client]], NULL_VECTOR, NULL_VECTOR);
    Tele_Reply(client, "Teleported to ({OG}%.3f{N},{OG}%.3f{N},{OG}%.3f{N})", g_PlayerTele[client][g_PlayerCurrentTele[client]][0], g_PlayerTele[client][g_PlayerCurrentTele[client]][1], g_PlayerTele[client][g_PlayerCurrentTele[client]][2]);
}

Teleport_Next(client)
{
    new next_tele = GetNextTele(client, g_PlayerCurrentTele[client]);
    if (next_tele == INVALID_TELEPORT)
    {
        Tele_Reply(client, "You do not currently have other saved locations. Use {T}!save{N} to resolve that.");
        return;
    }
    else if (next_tele == g_PlayerCurrentTele[client])
    {
        Tele_Reply(client, "You only have {OG}one{N} saved location. Use {T}!save{N} to store more.");
        return;
    }
    
    g_PlayerCurrentTele[client] = next_tele;
    Tele_Reply(client, "Switched to save slot {T}%d{N} of {T}3{N}.", next_tele+1);
}

Teleport_Previous(client)
{
    new prev_tele = GetPreviousTele(client, g_PlayerCurrentTele[client]);
    if (prev_tele == INVALID_TELEPORT)
    {
        Tele_Reply(client, "You do not currently have any other saved locations. Use {T}!save{N} to resolve that.");
        return;
    }
    else if (prev_tele == g_PlayerCurrentTele[client])
    {
        Tele_Reply(client, "You only have one saved location. Use {T}!save{N} to store more.");
        return;
    }
    
    g_PlayerCurrentTele[client] = prev_tele;
    Tele_Reply(client, "Switched to save slot {T}%d{N} of {T}3{N}.", prev_tele+1);
}

Teleport_Delete(client)
{
    if (g_PlayerCurrentTele[client] == INVALID_TELEPORT)
    {
        Tele_Reply(client, "You do not currently have any saved locations. Use {T}!save{N} to resolve that.");
        return;
    }
    
    for (new i=0; i<3; i++)
        g_PlayerTele[client][g_PlayerCurrentTele[client]][i] = 0.0;
    
    new old_slot = g_PlayerCurrentTele[client];
    g_PlayerCurrentTele[client] = GetNextTele(client, g_PlayerCurrentTele[client]);
    g_PlayerTeleCount[client]--;
    
    Tele_Reply(client, "Cleared save slot {T}%d{N} of {T}3{N}. You can now save {T}%d{N} location%s.", old_slot+1, MAX_TELEPORT_SAVES - g_PlayerTeleCount[client], (MAX_TELEPORT_SAVES - g_PlayerTeleCount[client]) == 1 ? "" : "s");
    
    BuildTeleMenu(client);
}
