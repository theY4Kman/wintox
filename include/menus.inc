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

#define MENU_STACK[%1] g_ClientSubmenus[(%1)]
#define MENUS_NO_LAST_INDEX -1
#define MENUS_EMPTY(%1) (MENU_STACK[%1] == INVALID_HANDLE || IsStackEmpty(MENU_STACK[%1]))

Menus_Init()
{
}

stock __InitSubmenuStack(client)
{
    if (g_ClientSubmenus[client] != INVALID_HANDLE)
        return;
    
    g_ClientSubmenus[client] = CreateStack();
}

stock __PushSubmenu(Handle:menu, client, time, close_handle, last_index)
{
    __InitSubmenuStack(client);
    if (last_index != MENUS_NO_LAST_INDEX)
        PushStackCell(MENU_STACK[client], last_index);
    PushStackCell(MENU_STACK[client], time);
    PushStackCell(MENU_STACK[client], _:close_handle);
    PushStackCell(MENU_STACK[client], _:menu);
}

stock DisplaySubmenu(Handle:menu, client, time, bool:close_handle=true, last_index=MENUS_NO_LAST_INDEX)
{
    __PushSubmenu(menu, client, time, close_handle, last_index);
    DisplayMenu(menu, client, time);
}

stock PrevSubmenu(client)
{
    if (MENUS_EMPTY(client))
        return;
    
    // Pop the info of the menu currently being displayed, so we can close its
    // handle if need be
    decl Handle:menu;
    decl bool:close_handle;
    decl last_index;
    
    PopStackCell(MENU_STACK[client], menu);
    PopStackCell(MENU_STACK[client], close_handle);
    PopStack(MENU_STACK[client]); // time
    PopStackCell(MENU_STACK[client], last_index);
    
    if (close_handle)
        CloseHandle(menu);
    
    // Check if the menu that was just displayed was the root menu
    if (IsStackEmpty(MENU_STACK[client]))
        return;
    
    // Pop the info of the previous menu to redisplay it at the right index
    decl time;
    
    PopStackCell(MENU_STACK[client], menu);
    PopStackCell(MENU_STACK[client], close_handle);
    PopStackCell(MENU_STACK[client], time);
    
    DisplayMenuAtItem(menu, client, last_index, time);
    
    // Reinsert the current menu to the stack, keeping the last index of the
    // previous menu
    __PushSubmenu(menu, client, time, close_handle, MENUS_NO_LAST_INDEX);
}

stock CancelSubmenus(client)
{
    if (MENUS_EMPTY(client))
        return;
    
    while (!IsStackEmpty(MENU_STACK[client]))
    {
        decl Handle:menu;
        decl bool:close_handle;
        
        PopStackCell(MENU_STACK[client], menu);
        PopStackCell(MENU_STACK[client], close_handle);
        PopStack(MENU_STACK[client]); // time
        PopStack(MENU_STACK[client]); // last_index
        
        if (close_handle)
            CloseHandle(menu);
    }
}
