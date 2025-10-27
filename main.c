#pragma c9x on
#include <Dialogs.h>
#include <Fonts.h>
#include <MacWindows.h>
#include <Menus.h>
#include <QuickDraw.h>
#include <TextEdit.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "resource.h"
#include "launcher.h"

//==================================================================
// globals
WindowPtr p_window_main;
TEHandle  h_textedit_main;
Rect      rect_window, rect_match, rect_textedit;
FSSpec*   fs_match;
IconRef   ir_match;

//==================================================================
// functions
void initToolbox();
void initMenu();
void initWindow();
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventLoop();
void eventActivate(EventRecord* event);
void eventOS(EventRecord* event);
void eventMouseDown(EventRecord* event);
void eventKeyDown(EventRecord* event);
void eventUpdate(EventRecord* event);
void eventMenuActivate(int32_t menu_item);
void eventMatchUpdate(FSSpec* match);
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void redrawWindow();
void drawMatch();
void checkForMatches();

//==================================================================
// main
void main() {
  p_window_main   = NULL;
  h_textedit_main = NULL;
  fs_match        = NULL;
  ir_match        = NULL;

  initToolbox();
  initMenu();
  initWindow();
  if(launcherInit("\plawn prawn", eventMatchUpdate) == false) {
    // launcherInit() should display any needed error messages
    ExitToShell();
  }

  eventLoop();
}

//==================================================================
// initialization
void initToolbox() {
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(nil);
  FlushEvents(everyEvent, 0);
  InitCursor();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void initMenu() {
  Handle h_mbar;
  MenuHandle h_menu_apple;

  h_mbar = GetNewMBar(RSRC_MBAR_MAIN);
  SetMenuBar(h_mbar);
  DisposeHandle(h_mbar);

  h_menu_apple = GetMenuHandle(RSRC_MENU_APPLE);
  AppendResMenu(h_menu_apple, 'DRVR');
  DrawMenuBar();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void initWindow() {
  int16_t screen_width, screen_height;
  int16_t window_width, window_height, window_padding_x, window_padding_y;

  // get main screen size (so we can size & center our window)
  {
    Rect    *monitor_main_bounds;
    GDHandle monitor_main = GetMainDevice();
    if(monitor_main == NULL) {
      showAlertDynamic(kAlertStopAlert, "\pCouldn't find main monitor?");
      ExitToShell();
    }
    monitor_main_bounds = &((**monitor_main).gdRect);
    screen_width  = monitor_main_bounds->right  - monitor_main_bounds->left;
    screen_height = monitor_main_bounds->bottom - monitor_main_bounds->top;
  }

  // calculate our dimensions
  {
    /*
               vertical screen padding
           |-----------------------------|
           |     top padding (4px)       |
      horz | match icon + text (32px)    |
      scrn |    middle padding (4px)     | horizontal screen padding
      pdng |   text editor (16px/12pt)   |
           |     bottom padding (4px)    |
           |-----------------------------|
               vertical screen padding
    */
    window_width  = screen_width >> 1;
    window_height = 60;
    window_padding_x = (screen_width  - window_width)  >> 1;
    window_padding_y = (screen_height - window_height) >> 1;

    rect_window.left   = window_padding_x;
    rect_window.right  = rect_window.left + window_width;
    rect_window.top    = window_padding_y;
    rect_window.bottom = rect_window.top + window_height;

    rect_match.left   = 4;
    rect_match.top    = 4;
    rect_match.right  = window_width - 4;
    rect_match.bottom = rect_match.top + 32;

    rect_textedit.left   = rect_match.left;
    rect_textedit.right  = rect_match.right;
    rect_textedit.top    = rect_match.bottom + 4;
    rect_textedit.bottom = rect_textedit.top + 16;
  }

  // place & size window
  p_window_main = GetNewWindow(RSRC_WIND_MAIN, NULL, (WindowPtr)-1L);
  SizeWindow(p_window_main, window_width,     window_height,    true);
  MoveWindow(p_window_main, window_padding_x, window_padding_y, true);

  // initialize textedit
  SetPort(p_window_main);
  p_window_main->txSize = 12;
  h_textedit_main = TENew(&rect_textedit, &rect_textedit);
  TESetSelect(0, 0, h_textedit_main);
  TESetAlignment(teCenter, h_textedit_main);
  TEActivate(h_textedit_main);

  ShowWindow(p_window_main);
}

//==================================================================
// events
void eventLoop() {
  EventRecord event;
  uint32_t sleep_ticks;

  sleep_ticks = 15L;
  while(true) {
    if(!WaitNextEvent(everyEvent, &event, 15L, nil)) {
      //TEIdle(h_textedit_main);
      //continue;
    }

    switch(event.what) {
      case nullEvent       : TEIdle(h_textedit_main);     break;
      case kHighLevelEvent : AEProcessAppleEvent(&event); break;
      case activateEvt     : eventActivate(&event);       break;
      case osEvt           : eventOS(&event);             break;
      case mouseDown       : eventMouseDown(&event);      break;
      case autoKey         : eventKeyDown(&event);        break;
      case keyDown         : eventKeyDown(&event);        break;
      case updateEvt       : eventUpdate(&event);         break;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventActivate(EventRecord* event) {
  WindowPtr window = (WindowPtr)event->message;
  bool isUs = (window == p_window_main);
  bool isActivate = (event->modifiers & 1) == 1;

  if(isUs && isActivate) { TEActivate(h_textedit_main); }
  SelectWindow(window);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventOS(EventRecord* event) {
  // exit if we're suspended
  if(((event->message & osEvtMessageMask) >> 24) == suspendResumeMessage) {
    if((event->message & resumeFlag) == 0) {
      ExitToShell();
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventMouseDown(EventRecord* event) {
  WindowPtr window;
  int16_t   window_part_code;
  Point     coords;

  window_part_code = FindWindow(event->where, &window);
  switch(window_part_code) {
    case inSysWindow:
      SystemClick(event, window);
      break;
    case inContent:
      {
        SetPort(p_window_main);
        coords = event->where;
        GlobalToLocal(&coords);
        if(PtInRect(coords, &((*h_textedit_main)->viewRect))) {
          TEClick(coords, (event->modifiers & shiftKey) > 0, h_textedit_main);
        }
      }
      break;
    case inMenuBar:
      {
        int32_t menu_choice;
        menu_choice = MenuSelect(event->where);
        if(menu_choice > 0) { eventMenuActivate(menu_choice); }
      }
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventKeyDown(EventRecord* event) {
  uint8_t keycode = event->message & charCodeMask;

  if(event->modifiers & cmdKey) {
    if(event->what == autoKey) { return; }
    switch(keycode) {
      case 'a':
        // cmd+a : select all
        TESetSelect(0, (*h_textedit_main)->teLength, h_textedit_main);
        break;
      case 'c':
        // cmd+c : copy (to system clipboard)
        TECopy(h_textedit_main);
        ZeroScrap();
        TEToScrap();
        break;
      case 'x':
        // cmd+x : cut (to system clipboard)
        TECut(h_textedit_main);
        ZeroScrap();
        TEToScrap();
        break;
      case 'v':
        // TODO: use GetScrap first to ensure not too long (63 characters max)
        TEFromScrap();
        TEPaste(h_textedit_main);
        checkForMatches();
        break;
      default:
        // may be a menu shortcut
        eventMenuActivate(MenuKey(event->message & charCodeMask));
        break;
    }
    return;
  }

  switch(keycode) {
    case kReturnCharCode:
    case kEnterCharCode:
      // on enter/return tell launcher to activate match
      launcherActivate();
      break;
    case kEscapeCharCode:
      // exit on escape
      ExitToShell();
      return;
    default:
      // if text is too long, don't allow new characters
      {
        short te_length = (*h_textedit_main)->teLength;
        if(te_length >= 63) {
          // only allow control characters and delete
          if((keycode >= 32) && (keycode != 127)) { SysBeep(100); break; }
        }
      }
      TEKey(keycode, h_textedit_main);
      // check for matches, only AFTER textedit has updated
      checkForMatches();
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventUpdate(EventRecord* event) {
  if((WindowPtr)(event->message) != p_window_main) { return; }

  BeginUpdate(p_window_main);
  redrawWindow();
  EndUpdate(p_window_main);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventMenuActivate(int32_t menu_item) {
  int16_t menu;
  int16_t item;
  Str255  apple_menu_name;
  MenuHandle h_mnu_apple;

  menu = HiWord(menu_item);
  item = LoWord(menu_item);

  switch(menu) {
    case RSRC_MENU_APPLE:
      switch(item) {
        case MENU_APPLE_ABOUT:
          SysBeep(1);
          break;
        default:
          h_mnu_apple = GetMenuHandle(RSRC_MENU_APPLE);
          GetMenuItemText(h_mnu_apple, item, apple_menu_name);
          OpenDeskAcc(apple_menu_name);
      }
      break;
    case RSRC_MENU_FILE:
      switch(item) {
        case MENU_FILE_OPEN:
          launcherOpenFolder();
          ExitToShell();
          break;
        case MENU_FILE_QUIT:
          ExitToShell();
          break;
      }
      break;
  }

  HiliteMenu(0);}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void eventMatchUpdate(FSSpec* match) {
  if(match == fs_match) { return; } // nothing new
  fs_match = match;

  if(fs_match == NULL) {
    if(ir_match != NULL) {
      ReleaseIconRef(ir_match);
      ir_match = NULL;
    }
  } else {
    int16_t label;
    GetIconRefFromFile(fs_match, &ir_match, &label);
  }

  redrawWindow();
}

//==================================================================
// drawing
void redrawWindow() {
  SetPort(p_window_main);
  EraseRect(&(p_window_main->portRect));
  TEUpdate(&(p_window_main->portRect), h_textedit_main);
  drawMatch();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void drawMatch() {
  Str255 match_name;
  int16_t text_width, text_x, text_y;

  if(fs_match != NULL) {
    memcpy(match_name, fs_match->name, 64);
  } else {
    match_name[0] = 3;
    match_name[1] = '.';
    match_name[2] = '.';
    match_name[3] = '.';
  }

  TextFont(kFontIDMonaco);
  TextSize(12);
  text_width = StringWidth(match_name);
  text_x = ((rect_match.right - rect_match.left) - text_width) >> 1;
  text_y = rect_match.top + 16;

  if(ir_match) {
    Rect icon_rect;
    icon_rect.left = text_x - 36;
    icon_rect.top  = rect_match.top;
    icon_rect.right = icon_rect.left + 32;
    icon_rect.bottom = icon_rect.top + 32;
    PlotIconRef(&icon_rect, atTopLeft, kTransformNone, kIconServicesNormalUsageFlag, ir_match);
  }
  
  MoveTo(text_x, text_y);
  DrawString(match_name);
}

//==================================================================
// utility
void checkForMatches() {
  char* textedit_string = *TEGetText(h_textedit_main);
  short te_length = (*h_textedit_main)->teLength;
  if(te_length > 255) { return; }
  launcherMatch(textedit_string, (uint8_t)(te_length & 0xFF));
}

//==================================================================
