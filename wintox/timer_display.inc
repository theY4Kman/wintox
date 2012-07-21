/**
 * Contains all the timer display templates, filter callbacks, and rendering
 *   functions to show a player's middle/hint-text timer.
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#include "./wintox/zones"
#include "./wintox/race"


// The timer display filters handle
new Handle:g_fwdTimerDisplayFilters = INVALID_HANDLE;


#define MAX_TIMER_LINES 12

#define TIMER_DISPLAY_ITEM_MASK 0x0000ffff
#define TIMER_DISPLAY_FLAGS_MASK 0xffff0000


/* ========================================================================
                     COMMON TIMER DISPLAY OPT NAMES
   ======================================================================== */

#define TIMER_OPT_TRACK "track"
#define TIMER_OPT_ZONE "zone"
#define TIMER_OPT_STYLE "style"
#define TIMER_OPT_PATH "path"
#define TIMER_OPT_JUMPS "path"

#define TIMER_OPT_RENDER_CLIENT "render_client"
#define TIMER_OPT_DISPLAY_CLIENT "display_client"
#define TIMER_OPT_STAGENAME "stagename"
#define TIMER_OPT_TIME "time"
#define TIMER_OPT_TIME_STRING "timestr"

#define TIMER_OPT_OPPONENT "opponent"
#define TIMER_OPT_OPP_STYLE "opponent_style"
#define TIMER_OPT_OPP_JUMPS "opponent_jumps"

#define TIMER_OPT_STRING0 "string0"
#define TIMER_OPT_STRING1 "string1"
#define TIMER_OPT_STRING2 "string2"
#define TIMER_OPT_STRING3 "string3"
#define TIMER_OPT_STRING4 "string4"
#define TIMER_OPT_STRING5 "string5"
#define TIMER_OPT_STRING6 "string6"
#define TIMER_OPT_STRING7 "string7"
#define TIMER_OPT_STRING8 "string8"
#define TIMER_OPT_STRING9 "string9"
#define TIMER_OPT_STRING10 "string10"
#define TIMER_OPT_STRING11 "string11"


/* ========================================================================
                          TIMER DISPLAY FLAGS
   ======================================================================== */

enum TimerDisplayFlags
{
    DisplayFlag_Brackets=(1<<16),   /* Place square brackets around item */
    DisplayFlag_ShowStage=(1<<17),  /* Prepends "Stage", lowers first char */
    DisplayFlag_ShowFinal=(1<<18),  /* Prepends "Final", lowers first char */
    DisplayFlag_ShowTotal=(1<<19),  /* Prepends "Total", lowers first char */
    DisplayFlag_Opponent=(1<<20),   /* Show opponent's value as well, uses opt's "opponent" client index */
    DisplayFlag_NoNewline=(1<<21),  /* Don't print a newline after this char */
};


/* ========================================================================
                          TIMER DISPLAY ITEMS
   ======================================================================== */

enum TimerDisplay
{
    TimerDisplay_AllHeaders=0,
    TimerDisplay_Paused,
    TimerDisplay_Practice,
    TimerDisplay_Race,
    TimerDisplay_StageName,
    TimerDisplay_Speed,
    TimerDisplay_Style,
    TimerDisplay_AllPlusMinus,
    TimerDisplay_PlusMinusZone,
    TimerDisplay_PlusMinusTotal,
    TimerDisplay_Time,
    TimerDisplay_Jumps,
    
    // Draws from opt's "stringx", where x=string number
    // Allows for arbitrary string rendering
    TimerDisplay_String0=100,
    TimerDisplay_String1,
    TimerDisplay_String2,
    TimerDisplay_String3,
    TimerDisplay_String4,
    TimerDisplay_String5,
    TimerDisplay_String6,
    TimerDisplay_String7,
    TimerDisplay_String8,
    TimerDisplay_String9,
    TimerDisplay_String10,
    TimerDisplay_String11
};



/* ========================================================================
                 CONVENIENCE MACROS FOR FILTER CALLBACKS
   ======================================================================== */

#define FILTERFUNC(%1) public Action:fwd_TimerTmpl_%1(client, Handle:opts, TimerDisplay:template[MAX_TIMER_LINES], &template_size)
#define TEMPLATE(%1) new TimerDisplay:g_TimerTmpl_%1[] = 

#define SELECT_FILTER_N(%1) template = (%1); \
    template_size = sizeof((%1))
#define RETURN_FILTER_N(%1) SELECT_FILTER_N(%1); \
    return Plugin_Stop
#define SELECT_FILTER(%1) SELECT_FILTER_N(g_TimerTmpl_%1)
#define RETURN_FILTER(%1) SELECT_FILTER(%1); \
    return Plugin_Stop

#define FILTER_CONDITION(%1) if (!(%1)) return Plugin_Continue



/* ========================================================================
              REGISTER ALL FILTER CALLBACKS TO THE FORWARD
   ======================================================================== */

// It's THE happening place to add your timer display filter callbacks to the forward
RegisterTimerDisplayFilters()
{
#define FILTER(%1) AddToForward(g_fwdTimerDisplayFilters, INVALID_HANDLE, fwd_TimerTmpl_%1)

    FILTER(EditingZones);
    
#if defined(WINTOX_BHOP)
    FILTER(DisableGravity);
#endif
    
    FILTER(EnterStartZone);
    FILTER(LeaveStartZone);
    
    FILTER(RegularTimerRun);

#undef FILTER
}



/* ========================================================================
                    [Timer disabled when editing zones]
   ======================================================================== */

TEMPLATE(EditingZones)
{
    TimerDisplay_String0
};

FILTERFUNC(EditingZones)
{
    FILTER_CONDITION(g_ClientEditZone[client] != INVALID_ZONE);
    SetTrieString(opts, TIMER_OPT_STRING0, "[Timer disabled when editing zones]");
    RETURN_FILTER(EditingZones);
}



/* ========================================================================
                    [Disable low gravity to begin timing]
   ======================================================================== */

#if defined(WINTOX_BHOP)
TEMPLATE(DisableGravity)
{
    TimerDisplay_String0
};

FILTERFUNC(DisableGravity)
{
    FILTER_CONDITION(g_PlayerLowGrav[client]);
    SetTrieString(opts, TIMER_OPT_STRING0, "[Disable low gravity to begin timing]");
    RETURN_FILTER(DisableGravity);
}
#endif



/* ========================================================================
                         [ENTER THE START ZONE]
   ======================================================================== */

TEMPLATE(EnterStartZone)
{
    TimerDisplay_String0
};

FILTERFUNC(EnterStartZone)
{
    FILTER_CONDITION(!HasPlayerInitiatedTiming(client));
    SetTrieString(opts, TIMER_OPT_STRING0, "[Enter the start zone]");
    RETURN_FILTER(EnterStartZone);
}



/* ========================================================================
                         [LEAVE THE START ZONE]
   ======================================================================== */

TEMPLATE(LeaveStartZone)
{
    TimerDisplay_String0
};

FILTERFUNC(LeaveStartZone)
{
    FILTER_CONDITION(g_PlayerCurrentlyInZone[client] && GetPlayerCurrentZone(client) == 0 && !TIMER_STARTED(client));
    SetTrieString(opts, TIMER_OPT_STRING0, "[Leave the start zone]");
    RETURN_FILTER(LeaveStartZone);
}



/* ========================================================================
                   Timer currently timing a regular run
   ======================================================================== */

#if defined(WINTOX_BHOP)
TEMPLATE(RegularTimerRun)
{
    TimerDisplay_AllHeaders,
    TimerDisplay_Time,
    TimerDisplay_Style,
    TimerDisplay_StageName,
    TimerDisplay_AllPlusMinus,
    TimerDisplay_Jumps,
    TimerDisplay_Speed
};

#else
TEMPLATE(RegularTimerRun)
{
    TimerDisplay_AllHeaders,
    TimerDisplay_Time,
    TimerDisplay_Style,
    TimerDisplay_StageName,
    TimerDisplay_AllPlusMinus,
    TimerDisplay_Speed
};

#endif

FILTERFUNC(RegularTimerRun)
{
    decl track;
    GetTrieValue(opts, TIMER_OPT_TRACK, track);
    FILTER_CONDITION(g_StartTimes[client][g_EndZone[track]] == 0.0);
    
    new Float:time = GetEngineTime() - g_StartTimes[client][0];
    SetTrieValue(opts, TIMER_OPT_TIME, time);
    
    RETURN_FILTER(RegularTimerRun);
}



/* ========================================================================
                   Timer currently timing a regular run
   ======================================================================== */

#if defined(WINTOX_BHOP)
TEMPLATE(RegularTimerRun)
{
    TimerDisplay_AllHeaders,
    TimerDisplay_Time,
    TimerDisplay_Style,
    TimerDisplay_StageName,
    TimerDisplay_AllPlusMinus,
    TimerDisplay_Jumps,
    TimerDisplay_Speed
};

#else
TEMPLATE(RegularTimerRun)
{
    TimerDisplay_AllHeaders,
    TimerDisplay_Time,
    TimerDisplay_Style,
    TimerDisplay_StageName,
    TimerDisplay_AllPlusMinus,
    TimerDisplay_Speed
};

#endif

FILTERFUNC(RegularTimerRun)
{
    decl track;
    GetTrieValue(opts, TIMER_OPT_TRACK, track);
    FILTER_CONDITION(g_StartTimes[client][g_EndZone[track]] == 0.0);
    
    new Float:time = GetEngineTime() - g_StartTimes[client][0];
    SetTrieValue(opts, TIMER_OPT_TIME, time);
    
    RETURN_FILTER(RegularTimerRun);
}



// Returns true if the item could be rendered, false if the renderer decided
// there was nothing to display
bool:RenderTimerTemplateItem(render_client, Handle:opts, TimerDisplay:item, TimerDisplayFlags:flags, String:sz_item[], maxlength, bool:is_last)
{
    switch (item)
    {
        case TimerDisplay_String0,
            TimerDisplay_String1,
            TimerDisplay_String2,
            TimerDisplay_String3,
            TimerDisplay_String4,
            TimerDisplay_String5,
            TimerDisplay_String6,
            TimerDisplay_String7,
            TimerDisplay_String8,
            TimerDisplay_String9,
            TimerDisplay_String10,
            TimerDisplay_String11:
            {
                static String:stringname[] = "string0";
                stringname[6] = '0' + String:(item - TimerDisplay_String0);
                
                GetTrieString(opts, stringname, MSTR(sz_item));
            }
        
        case TimerDisplay_Paused:
        {
            if (IS_PAUSED(render_client))
                strcopy(MSTR(sz_item), "[PAUSED]");
            else
                return false;
        }
        
        case TimerDisplay_Practice:
        {
            if (g_IsPracticing[render_client])
                strcopy(MSTR(sz_item), "[PRACTICE]");
            else
                return false;
        }
        
        case TimerDisplay_Race:
        {
            if (IsRacing(render_client))
                strcopy(MSTR(sz_item), "[RACE]");
            else
                return false;
        }
        
        case TimerDisplay_StageName:
            GetTrieString(opts, TIMER_OPT_STAGENAME, MSTR(sz_item));
        
        case TimerDisplay_Speed:
        {
            if (g_AllowVelocityDisplay && g_TimerSpeedEnabled[render_client])
                Format(MSTR(sz_item), "Speed: %.1f", GetPlayerVelocity(render_client));
        }
        
        case TimerDisplay_Style:
        {
            new style = STYLE_NORMAL;
            GetTrieValue(opts, TIMER_OPT_STYLE, style);
            if (style == INVALID_STYLE)
                return false;
            
            Format(MSTR(sz_item), "Style: %s", STYLE_NAMES_C[style]);
            
            if (HAS_FLAG(flags, DisplayFlag_Opponent))
            {
                new opp_style = STYLE_NORMAL;
                if (!GetTrieValue(opts, TIMER_OPT_OPP_STYLE, opp_style) || opp_style == INVALID_STYLE)
                {
                    decl opponent;
                    GetTrieValue(opts, TIMER_OPT_OPPONENT, opponent);
                    opp_style = g_PlayerCurrentStyle[opponent];
                }
                
                Format(MSTR(sz_item), "%s (opp %s)", sz_item, STYLE_NAMES_C[opp_style]);
            }
        }
        
        case TimerDisplay_PlusMinusZone,
            TimerDisplay_PlusMinusTotal:
        {
            decl zone;
            decl track;
            decl pm_style;
            decl Float:zone_time;
            decl String:sz_time[32];
            
            GetTrieValue(opts, TIMER_OPT_ZONE, zone);
            GetTrieValue(opts, TIMER_OPT_TRACK, track);
            
            if (!g_DisplayPlusMinus[render_client] || zone < 1)
                return false;
            
            pm_style = g_PlusMinusStyle[render_client];
            
            if (g_CurMapStaged)
                zone_time = g_LastRunZoneElapsed[render_client][zone];
            else
                zone_time = g_StartTimes[render_client][zone] - g_StartTimes[render_client][zone-1];
            
            // Compare to the fastest stage time
            if (item == TimerDisplay_PlusMinusZone)
            {
                new Float:zone_pm = zone_time - g_BestZoneRecords[track][zone][pm_style];
                
                SecondsToTimeString(zone_pm, STR(sz_time), true);
                FormatEx(MSTR(sz_item), "Zone (%s)", sz_time);
            }
            
            // Compare to the track world record run
            else if (item == TimerDisplay_PlusMinusTotal)
            {
                new Float:wr_pm = zone_time - g_TrackWorldRecord[track][zone][pm_style];
                
                SecondsToTimeString(wr_pm, STR(sz_time), true);
                FormatEx(MSTR(sz_item), "Total (%s)", sz_time);
            }
        }
        
        case TimerDisplay_Time:
        {
            decl String:sz_time[32];
            
            sz_time[0] = '\0';
            if(!GetTrieString(opts, TIMER_OPT_TIME_STRING, STR(sz_time)) || IS_STR_EMPTY(sz_time))
            {
                decl Float:time;
                GetTrieValue(opts, TIMER_OPT_TIME, time);
                
                SecondsToTimeString(time, STR(sz_time));
            }
            
            FormatEx(MSTR(sz_item), "Time: %s", sz_time);
        }
        
        case TimerDisplay_Jumps:
        {
            new jumps = 0;
            GetTrieValue(opts, TIMER_OPT_JUMPS, jumps);
            if (jumps == -1)
                return false;
            
            Format(MSTR(sz_item), "Jumps: %d", jumps);
            
            if (HAS_FLAG(flags, DisplayFlag_Opponent))
            {
                new opp_jumps = -1;
                if (!GetTrieValue(opts, TIMER_OPT_OPP_JUMPS, opp_jumps) || opp_jumps == -1)
                {
                    decl opponent;
                    GetTrieValue(opts, TIMER_OPT_OPPONENT, opponent);
                    
                    opp_jumps = g_Jumps[opponent];
                }
                
                Format(MSTR(sz_item), "%s (opp %d)", sz_item, opp_jumps);
            }
        }
        
        default:
            return false;
    }
    
    if (HAS_FLAG(flags, DisplayFlag_ShowStage))
        PrependCapitals(sz_item, "Stage", MSTR(sz_item));
    
    else if (HAS_FLAG(flags, DisplayFlag_ShowFinal))
        PrependCapitals(sz_item, "Final", MSTR(sz_item));
        
    else if (HAS_FLAG(flags, DisplayFlag_ShowTotal))
        PrependCapitals(sz_item, "Total", MSTR(sz_item));
    
    if (HAS_FLAG(flags, DisplayFlag_Brackets))
        Format(MSTR(sz_item), "[%s]", sz_item);
    
    if (!HAS_FLAG(flags, DisplayFlag_NoNewline) && !is_last)
        Format(MSTR(sz_item), "%s\n", sz_item);
    
    return true;
}

// Returns whether or not the template was rendered correctly
// (or if blank)
bool:RenderTimerDisplayTemplate(Handle:opts, const TimerDisplay:template[], template_size, String:buffer[], maxlength)
{
    decl String:output[512];
    output[0] = '\0';
    
    decl display_client;
    decl render_client;
    
    if (template_size == 0)
        return false;
    
    // Pull some common values for easy access
    GetTrieValue(opts, TIMER_OPT_DISPLAY_CLIENT, display_client);
    GetTrieValue(opts, TIMER_OPT_RENDER_CLIENT, render_client);
    
    
    for (new i=0,
            TimerDisplay:raw_item=template[0],
            bool:last=(template_size == 1);
        i<template_size;
        raw_item=template[++i],
            last=(i == (template_size)))
    {
        decl String:sz_item[128];
        decl TimerDisplay:item;
        decl TimerDisplayFlags:flags;
        
        sz_item[0] = '\0';
        item = TimerDisplay:(_:raw_item & TIMER_DISPLAY_ITEM_MASK);
        flags = TimerDisplayFlags:((_:raw_item & TIMER_DISPLAY_FLAGS_MASK) >> 16);
        
        // switch here to handle inclusive items (AllHeaders, etc)
        // TODO: figure out is_last for inclusive items
        switch (item)
        {
#define RENDER_ITEM(%1) RenderTimerTemplateItem(render_client, opts, (%1), flags, sz_item, sizeof(sz_item), last); \
    StrCat(STR(output), sz_item); \
    sz_item[0] = '\0'
            
            case TimerDisplay_AllHeaders:
            {
                RENDER_ITEM(TimerDisplay_Paused);
                RENDER_ITEM(TimerDisplay_Practice);
                RENDER_ITEM(TimerDisplay_Race);
            }
            
            case TimerDisplay_AllPlusMinus:
            {
                RENDER_ITEM(TimerDisplay_PlusMinusZone);
                RENDER_ITEM(TimerDisplay_PlusMinusTotal);
            }
            
            default:
            {
                RENDER_ITEM(item);
            }
        
#undef RENDER_ITEM
        }
        
    }
    
    // Meh, I have no reason for a newline at the end of the hint-text
    new len = strlen(output);
    if (len >= 1 && output[len-1] == '\n')
        output[len-1] = '\0';
    
    strcopy(buffer, maxlength, output);
    return true;
}

/* The most expensive operation in a Trie is insertion, so instead of creating
 * a new opts Trie for every player, we just blank it out before each usage.
 */ 
FillTimerDisplayOpts(display_client, render_client, Handle:opts)
{
    if (opts == INVALID_HANDLE)
        return;
    
    // Client values
    SetTrieValue(opts, TIMER_OPT_RENDER_CLIENT, render_client);
    SetTrieValue(opts, TIMER_OPT_DISPLAY_CLIENT, display_client);
    
    
    // Opponent values
    SetTrieValue(opts, TIMER_OPT_OPPONENT, -1);
    SetTrieValue(opts, TIMER_OPT_OPP_STYLE, INVALID_STYLE);
    SetTrieValue(opts, TIMER_OPT_OPP_JUMPS, -1);
    
    
    // Current timer state values
    new zone = GetPlayerCurrentZone(render_client);
    new track = GetPlayerCurrentTrack(render_client);
    
    SetTrieValue(opts, TIMER_OPT_PATH, GetPlayerCurrentPathIndex(render_client));
    SetTrieValue(opts, TIMER_OPT_TRACK, track);
    SetTrieValue(opts, TIMER_OPT_ZONE, zone);
    SetTrieValue(opts, TIMER_OPT_STYLE, g_PlayerCurrentStyle[render_client]);
    
#if defined(WINTOX_BHOP)
    SetTrieValue(opts, TIMER_OPT_JUMPS, g_Jumps[render_client]);
#endif
    
    
    // Stagename
    if (zone >= 0)
    {
        if (!IS_STR_EMPTY(g_ZoneNames[track][zone]))
            SetTrieString(opts, TIMER_OPT_STAGENAME, g_ZoneNames[track][zone]);
        else if (zone > 0)
        {
            decl String:stagename[72];
            Format(STR(stagename), "Stage %d", zone + 1);
            SetTrieString(opts, TIMER_OPT_STAGENAME, stagename);
        }
        else
            SetTrieString(opts, TIMER_OPT_STAGENAME, "Start");
    }
    else
        SetTrieString(opts, TIMER_OPT_STAGENAME, "");
    
    
    // Time
    SetTrieValue(opts, TIMER_OPT_TIME, INVALID_TIME);
    SetTrieString(opts, TIMER_OPT_TIME_STRING, NULL_STRING);
    
    
    // Arbitrary strings
    SetTrieString(opts, TIMER_OPT_STRING0, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING1, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING2, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING3, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING4, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING5, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING6, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING7, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING8, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING9, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING10, NULL_STRING);
    SetTrieString(opts, TIMER_OPT_STRING11, NULL_STRING);
}

DisplayTimer(display_client)
{
    decl String:sz_timer[512];
    sz_timer[0] = '\0';
    
    // See if we should render the timer of the player we're observing
    new render_client = display_client;
    new Obs_Mode:obsmode = Client_GetObserverMode(display_client);
    if (obsmode != OBS_MODE_NONE)
    {
        new obs_target = Client_GetObserverTarget(display_client);
        if (obs_target > 0 && obs_target < MaxClients)
            render_client = obs_target;
    }
    
    // Fill out our opts trie (overwrite old common values)
    static Handle:opts = INVALID_HANDLE;
    if (opts == INVALID_HANDLE)
        opts = CreateTrie();
    FillTimerDisplayOpts(display_client, render_client, opts);
    
    new TimerDisplay:template[MAX_TIMER_LINES];
    new template_size = -1;
    
    // Call the filter callbacks
    Call_StartForward(g_fwdTimerDisplayFilters);
    Call_PushCell(render_client);
    Call_PushCell(opts);
    Call_PushArrayEx(template, MAX_TIMER_LINES, SM_PARAM_COPYBACK);
    Call_PushCellRef(template_size);
    Call_Finish();
    
    // Rrrrrender (in a Nixon voice)
    RenderTimerDisplayTemplate(opts, template, template_size, STR(sz_timer));
    
    // And now we send it! Yay! Pop some bottles
    PrintHintText(display_client, "%s", sz_timer);
}
