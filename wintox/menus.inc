/**
 * Menus framework, allowing for submenus.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_menus_included
 #endinput
#endif
#define _wintox_menus_included

/**
 * Submenu handling is done using stacks. Thus, we keep an array of handles to
 * stacks, one handle for each potential client index. For each menu displayed
 * to a client, we store the index of the first item displayed on the previous
 * menu (to redisplay on the correct page), the time limit of the current menu,
 * whether or not to close the handle to the current menu upon cancellation or
 * pressing 'Back', and finally the handle to the menu.
 */
new Handle:g_ClientSubmenus[MAXPLAYERS] = { INVALID_HANDLE, ... };

enum Submenu
{
    Submenu_Handle = 0,
    Submenu_CloseHandle,
    Submenu_Time,
    Submenu_LastIndex
};

#define MENU_STACK[%1] g_ClientSubmenus[(%1)]
#define MENUS_NO_LAST_INDEX -1
#define MENUS_EMPTY(%1) (MENU_STACK[%1] == INVALID_HANDLE || IsStackEmpty(MENU_STACK[%1]))

Menus_Init()
{
}

stock __InitSubmenuStack(client)
{
    if (MENU_STACK[client] != INVALID_HANDLE)
        return;
    
    MENU_STACK[client] = CreateStack(4);
}

stock __PushSubmenu(Handle:menu, client, time, close_handle, last_index)
{
    __InitSubmenuStack(client);
    
    decl submenu[Submenu];
    submenu[Submenu_Handle] = _:menu;
    submenu[Submenu_CloseHandle] = _:close_handle;
    submenu[Submenu_Time] = time;
    submenu[Submenu_LastIndex] = last_index;
    
    PushStackArray(MENU_STACK[client], submenu[0]);
}

stock DisplaySubmenu(Handle:menu, client, time, bool:close_handle=true, last_index=MENUS_NO_LAST_INDEX)
{
    __PushSubmenu(menu, client, time, close_handle, last_index);
    DisplayMenu(menu, client, time);
}

stock PrevSubmenu(client, bool:redisplay=true)
{
    // Our menu stack handles are invalid when the plug-in ends, and if there
    // are still menus open when we unload, their MenuAction_Cancel handler
    // still gets called, usually calling PrevSubmenu. So, do nothing.
    if (g_wintoxExiting)
        return;
    
    if (MENUS_EMPTY(client) || g_wintoxExiting)
        return;
    
    // Pop the info of the menu currently being displayed, so we can close its
    // handle if need be
    decl submenu[Submenu];
    
    PopStackArray(MENU_STACK[client], submenu[0]);
    
    if (submenu[Submenu_CloseHandle])
        CloseHandle(Handle:submenu[Submenu_Handle]);
    
    // Check if the menu that was just displayed was the root menu
    if (IsStackEmpty(MENU_STACK[client]))
        return;
    
    // Used to simply pop a menu from the stack and display a different one
    // For instance, the zone index selection menu (see zones.inc)
    if (!redisplay)
        return;
    
    // Pop the info of the previous menu to redisplay it at the right index
    PopStackArray(MENU_STACK[client], submenu[0]);
    
    DisplayMenuAtItem(Handle:submenu[Submenu_Handle], client, submenu[Submenu_LastIndex], submenu[Submenu_Time]);
    
    // Reinsert the now current menu to the stack, keeping the last index of
    // the previous menu
    PushStackArray(MENU_STACK[client], submenu[0]);
}

stock CancelSubmenus(client)
{
    if (MENUS_EMPTY(client))
        return;
    
    while (!IsStackEmpty(MENU_STACK[client]))
    {
        decl submenu[Submenu];
        PopStackArray(MENU_STACK[client], submenu[0]);
        
        if (submenu[Submenu_CloseHandle])
            CloseHandle(Handle:submenu[Submenu_Handle]);
    }
}

// Retrieves the menu position of the topmost submenu
// Useful when multiple submenus are popped without redisplaying
// (e.g. the track menu -> zone index menu)
stock GetLastMenuPosition(client)
{
    decl submenu[Submenu];
    PopStackArray(MENU_STACK[client], submenu[0]);
    PushStackArray(MENU_STACK[client], submenu[0]);
    
    return submenu[Submenu_LastIndex];
}

#define HANDLE_CANCEL() HandleCancel(menu, param1, param2)
stock HandleCancel(Handle:menu, client, reason)
{
    if (g_wintoxExiting)
    {
        CloseHandle(menu);
        return;
    }
    
    if (reason == MenuCancel_Exit)
        CancelSubmenus(client);
    else
        PrevSubmenu(client);
}

// For debugging purposes
stock PrintMenuStack(Handle:stack)
{
    new Handle:temp = CreateStack(4);
    decl submenu[Submenu];
    new length = 0;
    
    PrintToServer("%-8s %-10s %-8s %-6s %-10s", "Index", "Handle", "Close", "Time", "Last Index");
    
    while (PopStackArray(stack, submenu[0]) && ++length)
        PushStackArray(temp, submenu[0]);
    
    for (new i=0; PopStackArray(temp, submenu[0]); i++)
    {
        PrintToServer("%-8d %-10x %-8s %-6d %-10d", i, submenu[Submenu_Handle], submenu[Submenu_CloseHandle] ? "yes" : "no", submenu[Submenu_Time], submenu[Submenu_LastIndex]);
        PushStackArray(stack, submenu[0]);
    }
}
