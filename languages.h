/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright (C) 2015    Stefan Sundin                                   *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 or later.              *
 * Modified By Raymond Gillibert in 2020                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef ALTDRAG_STRINGS_H
#define ALTDRAG_STRINGS_H

struct langinfoitem {
  wchar_t *code;
  wchar_t *lang_english;
  wchar_t *lang;
  wchar_t *author;
  wchar_t *fn;
};

struct strings {
  wchar_t *code;
  wchar_t *lang_english;
  wchar_t *lang;
  wchar_t *author;

  // menu
  wchar_t *menu_enable;
  wchar_t *menu_disable;
  wchar_t *menu_hide;
  wchar_t *menu_config;
  wchar_t *menu_about;
  wchar_t *menu_exit;

  // update
  wchar_t *update_balloon;
  wchar_t *update_dialog;
  wchar_t *update_nonew;

  // config
  wchar_t *title;
  wchar_t *tab_general;
  wchar_t *tab_input;
  wchar_t *tab_blacklist;
  wchar_t *tab_advanced;
  wchar_t *tab_about;

  // general
  wchar_t *general_box;
  wchar_t *general_autofocus;
  wchar_t *general_aero;
  wchar_t *general_inactivescroll;
  wchar_t *general_mdi;
  wchar_t *general_autosnap;
  wchar_t *general_autosnap0;
  wchar_t *general_autosnap1;
  wchar_t *general_autosnap2;
  wchar_t *general_autosnap3;
  wchar_t *general_language;
  wchar_t *general_fullwin;
  wchar_t *general_resizeall;
  wchar_t *general_resizecenter;
  wchar_t *general_resizecenter_norm;
  wchar_t *general_resizecenter_br;
  wchar_t *general_resizecenter_move;

  // general autostart
  wchar_t *general_autostart_box;
  wchar_t *general_autostart;
  wchar_t *general_autostart_hide;
  wchar_t *general_autostart_elevate;
  wchar_t *general_autostart_elevate_tip;
  wchar_t *general_elevate;
  wchar_t *general_elevated;
  wchar_t *general_elevation_aborted;
  wchar_t *general_autosave;

  // input
  // mouse
  wchar_t *input_mouse_box;
  wchar_t *input_mouse_lmb;
  wchar_t *input_mouse_mmb;
  wchar_t *input_mouse_rmb;
  wchar_t *input_mouse_mb4;
  wchar_t *input_mouse_mb5;
  wchar_t *input_mouse_scroll;
  wchar_t *input_mouse_lowerwithmmb;
  wchar_t *input_aggressive_pause;

  // actions
  wchar_t *input_actions_move;
  wchar_t *input_actions_resize;
  wchar_t *input_actions_close;
  wchar_t *input_actions_minimize;
  wchar_t *input_actions_maximize;
  wchar_t *input_actions_lower;
  wchar_t *input_actions_alwaysontop;
  wchar_t *input_actions_borderless;
  wchar_t *input_actions_center;
  wchar_t *input_actions_nothing;
  wchar_t *input_actions_alttab;
  wchar_t *input_actions_volume;
  wchar_t *input_actions_transparency;

  // hotkeys
  wchar_t *input_hotkeys_box;
  wchar_t *input_hotkeys_leftalt;
  wchar_t *input_hotkeys_rightalt;
  wchar_t *input_hotkeys_leftwinkey;
  wchar_t *input_hotkeys_rightwinkey;
  wchar_t *input_hotkeys_leftctrl;
  wchar_t *input_hotkeys_rightctrl;
  wchar_t *input_hotkeys_more;

  // blacklist
  wchar_t *blacklist_box;
  wchar_t *blacklist_processblacklist;
  wchar_t *blacklist_blacklist;
  wchar_t *blacklist_snaplist;
  wchar_t *blacklist_mdis;
  wchar_t *blacklist_pause;
  wchar_t *blacklist_findwindow_box;

  // about
  wchar_t *about_box;
  wchar_t *about_version;
  wchar_t *about_author;
  wchar_t *about_license;
  wchar_t *about_translation_credit;

  /* misc */
  wchar_t *unhook_error;
};

struct strings l10n_ini;

struct {
  wchar_t **str;
  wchar_t *name;
} l10n_mapping[] = {
  { &l10n_ini.code,                            L"Code" },
  { &l10n_ini.lang_english,                    L"LangEnglish" },
  { &l10n_ini.lang,                            L"Lang" },
  { &l10n_ini.author,                          L"Author" },

  { &l10n_ini.menu_enable,                     L"MenuEnable" },
  { &l10n_ini.menu_disable,                    L"MenuDisable" },
  { &l10n_ini.menu_hide,                       L"MenuHideTray" },
  { &l10n_ini.menu_config,                     L"MenuConfigure" },
  { &l10n_ini.menu_about,                      L"MenuAbout" },
  { &l10n_ini.menu_exit,                       L"MenuExit" },
  { &l10n_ini.update_balloon,                  L"UpdateBalloon" },
  { &l10n_ini.update_dialog,                   L"UpdateDialog" },
  { &l10n_ini.update_nonew,                    L"UpdateLatest" },

  { &l10n_ini.title,                           L"ConfigTitle" },
  { &l10n_ini.tab_general,                     L"ConfigTabGeneral" },
  { &l10n_ini.tab_input,                       L"ConfigTabInput" },
  { &l10n_ini.tab_blacklist,                   L"ConfigTabBlacklist" },
  { &l10n_ini.tab_advanced,                    L"ConfigTabAdvanced" },
  { &l10n_ini.tab_about,                       L"ConfigTabAbout" },

  { &l10n_ini.general_box,                     L"GeneralBox" },
  { &l10n_ini.general_autofocus,               L"GeneralAutoFocus" },
  { &l10n_ini.general_aero,                    L"GeneralAero" },
  { &l10n_ini.general_inactivescroll,          L"GeneralInactiveScroll" },
  { &l10n_ini.general_mdi,                     L"GeneralMDI" },
  { &l10n_ini.general_autosnap,                L"GeneralAutoSnap" },
  { &l10n_ini.general_autosnap0,               L"GeneralAutoSnap0" },
  { &l10n_ini.general_autosnap1,               L"GeneralAutoSnap1" },
  { &l10n_ini.general_autosnap2,               L"GeneralAutoSnap2" },
  { &l10n_ini.general_autosnap3,               L"GeneralAutoSnap3" },
  { &l10n_ini.general_language,                L"GeneralLanguage" },
  { &l10n_ini.general_fullwin,                 L"GeneralFullWin" },
  { &l10n_ini.general_resizeall,               L"GeneralResizeAll" },
  { &l10n_ini.general_resizecenter,            L"GeneralResizeCenter" },
  { &l10n_ini.general_resizecenter_norm,       L"GeneralResizeCenterNorm" },
  { &l10n_ini.general_resizecenter_br,         L"GeneralResizeCenterBr" },
  { &l10n_ini.general_resizecenter_move,       L"GeneralResizeCenterMove" },

  { &l10n_ini.general_autostart_box,           L"GeneralAutostartBox" },
  { &l10n_ini.general_autostart,               L"GeneralAutostart" },
  { &l10n_ini.general_autostart_hide,          L"GeneralAutostartHide" },
  { &l10n_ini.general_autostart_elevate,       L"GeneralAutostartElevate" },
  { &l10n_ini.general_autostart_elevate_tip,   L"GeneralAutostartElevateTip" },
  { &l10n_ini.general_elevate,                 L"GeneralElevate" },
  { &l10n_ini.general_elevated,                L"GeneralElevated" },
  { &l10n_ini.general_elevation_aborted,       L"GeneralElevationAborted" },
  { &l10n_ini.general_autosave,                L"GeneralAutosave" },

  { &l10n_ini.input_mouse_box,                 L"InputMouseBox" },
  { &l10n_ini.input_mouse_lmb,                 L"InputMouseLMB" },
  { &l10n_ini.input_mouse_mmb,                 L"InputMouseMMB" },
  { &l10n_ini.input_mouse_rmb,                 L"InputMouseRMB" },
  { &l10n_ini.input_mouse_mb4,                 L"InputMouseMB4" },
  { &l10n_ini.input_mouse_mb5,                 L"InputMouseMB5" },
  { &l10n_ini.input_mouse_scroll,              L"InputMouseScroll" },
  { &l10n_ini.input_mouse_lowerwithmmb,        L"InputMouseLowerWithMMB" },
  { &l10n_ini.input_aggressive_pause,          L"InputAggressivePause" },

  { &l10n_ini.input_actions_move,              L"InputActionMove" },
  { &l10n_ini.input_actions_resize,            L"InputActionResize" },
  { &l10n_ini.input_actions_close,             L"InputActionClose" },
  { &l10n_ini.input_actions_minimize,          L"InputActionMinimize" },
  { &l10n_ini.input_actions_maximize,          L"InputActionMaximize" },
  { &l10n_ini.input_actions_lower,             L"InputActionLower" },
  { &l10n_ini.input_actions_alwaysontop,       L"InputActionAlwaysOnTop" },
  { &l10n_ini.input_actions_borderless,        L"InputActionBorderless" },
  { &l10n_ini.input_actions_center,            L"InputActionCenter" },
  { &l10n_ini.input_actions_nothing,           L"InputActionNothing" },
  { &l10n_ini.input_actions_alttab,            L"InputActionAltTab" },
  { &l10n_ini.input_actions_volume,            L"InputActionVolume" },
  { &l10n_ini.input_actions_transparency,      L"InputActionTransparency" },

  { &l10n_ini.input_hotkeys_box,               L"InputHotkeysBox" },
  { &l10n_ini.input_hotkeys_leftalt,           L"InputHotkeysLeftAlt" },
  { &l10n_ini.input_hotkeys_rightalt,          L"InputHotkeysRightAlt" },
  { &l10n_ini.input_hotkeys_leftwinkey,        L"InputHotkeysLeftWinkey" },
  { &l10n_ini.input_hotkeys_rightwinkey,       L"InputHotkeysRightWinkey" },
  { &l10n_ini.input_hotkeys_leftctrl,          L"InputHotkeysLeftCtrl" },
  { &l10n_ini.input_hotkeys_rightctrl,         L"InputHotkeysRightCtrl" },
  { &l10n_ini.input_hotkeys_more,              L"InputHotkeysMore" },

  { &l10n_ini.blacklist_box,                   L"BlacklistBox" },
  { &l10n_ini.blacklist_processblacklist,      L"BlacklistProcessBlacklist" },
  { &l10n_ini.blacklist_blacklist,             L"BlacklistBlacklist" },
  { &l10n_ini.blacklist_snaplist,              L"BlacklistSnaplist" },
  { &l10n_ini.blacklist_mdis,                  L"BlacklistMDIs" },
  { &l10n_ini.blacklist_pause,                 L"BlacklistPause" },
  { &l10n_ini.blacklist_findwindow_box,        L"BlacklistFindWindowBox" },

  { &l10n_ini.about_box,                       L"AboutBox" },
  { &l10n_ini.about_version,                   L"AboutVersion" },
  { &l10n_ini.about_author,                    L"AboutAuthor" },
  { &l10n_ini.about_license,                   L"AboutLicense" },
  { &l10n_ini.about_translation_credit,        L"AboutTranslationCredit" },

  { &l10n_ini.unhook_error,                    L"MiscUnhookError" },
};

struct strings en_US = {
 /* === translation info === */
 /* code               */ L"en-US",
 /* lang_english       */ L"English",
 /* lang               */ L"English",
 /* author             */ L"Stefan Sundin",

 /* === app === */
 /* menu */
 /* enable             */ L"&Enable",
 /* disable            */ L"&Disable",
 /* hide               */ L"&Hide tray",
 /* config             */ L"&Configure",
 /* about              */ L"&About",
 /* exit               */ L"E&xit",

 /* update */
 /* balloon            */ L"New version found!",
 /* dialog             */ L"A new version is available. Go to website?",
 /* nonew              */ L"No update available.",

 /* === config === */
 /* title              */ APP_NAME L" Configuration",
 /* tabs */
 /* general            */ L"General",
 /* input              */ L"Mouse and keyboard",
 /* blacklist          */ L"Blacklist",
 /* advanced           */ L"Advanced",
 /* about              */ L"About",

 /* general tab */
 /* box                */ L"General settings",
 /* autofocus          */ L"&Focus windows when dragging.\nYou can also press Ctrl to focus windows.",
 /* aero               */ L"Mimic &Aero Snap",
 /* inactivescroll     */ L"&Scroll inactive windows",
 /* mdi                */ L"&MDI support",
 /* autosnap           */ L"S&nap window edges to:",
 /* autosnap0          */ L"Disabled",
 /* autosnap1          */ L"To screen borders",
 /* autosnap2          */ L"+ outside of windows",
 /* autosnap3          */ L"+ inside of windows",
 /* language           */ L"&Language:",
 /* FullWin            */ L"&Drag full windows",
 /* ResizeAll          */ L"&Resize all windows",
 /* ResizeCenter       */ L"&Center resize mode",
 /* ResizeCenterNorm   */ L"All d&irections",
 /* ResizeCenterBr     */ L"B&ottom right",
 /* ResizeCenterMove   */ L"Mo&ve",

 /* autostart_box      */ L"Autostart",
 /* autostart          */ L"S&tart "APP_NAME" when logging on",
 /* autostart_hide     */ L"&Hide tray",
 /* autostart_elevate  */ L"&Elevate to administrator privileges",
 /* elevate_tip        */ L"Note that a UAC prompt will appear every time you log in, unless you disable UAC completely.",
 /* elevate            */ L"E&levate",
 /* elevated           */ L"Elevated",
 /* elevation_aborted  */ L"Elevation aborted.",
 /* autosave           */ L"Note: Settings are saved and applied instantly!",

 /* input tab */
 /* mouse */
 /* box                */ L"Mouse actions",
 /* lmb                */ L"Left mouse &button:",
 /* mmb                */ L"&Middle mouse button:",
 /* rmb                */ L"Ri&ght mouse button:",
 /* mb4                */ L"Mouse button &4:",
 /* mb5                */ L"Mouse button &5:",
 /* scroll             */ L"&Scroll wheel:",
 /* lowerwithmmb       */ L"&Lower windows by middle clicking on title bars",
 /* Aggressive Pause   */ L"&Pause process on Alt+Shift+Pause (Alt+Pause to resume)",

 /* actions */
 /* move               */ L"Move window",
 /* resize             */ L"Resize window",
 /* close              */ L"Close window",
 /* minimize           */ L"Minimize window",
 /* maximize           */ L"Maximize window",
 /* lower              */ L"Lower window",
 /* alwaysontop        */ L"Toggle always on top",
 /* borderless         */ L"Toggle borderless",
 /* center             */ L"Center window on screen",
 /* nothing            */ L"Nothing",
 /* alttab             */ L"Alt+Tab",
 /* volume             */ L"Volume",
 /* transparency       */ L"Transparency",

 /* hotkeys */
 /* box                */ L"Hotkeys",
 /* leftalt            */ L"L&eft Alt",
 /* rightalt           */ L"&Right Alt",
 /* leftwinkey         */ L"Left &Winkey",
 /* rightwinkey        */ L"Right W&inkey",
 /* leftctrl           */ L"Left &Ctrl",
 /* rightctrl          */ L"Right C&trl",
 /* more               */ L"You can add any key by editing the ini file.\nUse the shift key to snap windows to each other.",

 /* blacklist tab */
 /* box                */ L"Blacklist settings",
 /* processblacklist   */ L"&Process blacklist:",
 /* blacklist          */ L"&Windows blacklist:",
 /* snaplist           */ L"&Windows that should be &snapped to:",
 /* MDIs bl            */ L"&MDIs not to be treated as such:",
 /* Pause list         */ L"Processes not to be pa&used:",
 /* findwindow_box     */ L"Identify window",

 /* about tab */
 /* box                */ L"About "APP_NAME,
 /* version            */ L"Version "APP_VERSION,
 /* author             */ L"Created by Stefan Sundin",
 /* license            */ APP_NAME L" is free and open source software!\nFeel free to redistribute!",
 /* translation_credit */ L"Translation credit",

 /* === misc === */
 /* unhook_error       */ L"There was an error disabling "APP_NAME". This was most likely caused by Windows having already disabled "APP_NAME"'s hooks.\n\nIf this is the first time this has happened, you can safely ignore it and continue using "APP_NAME".\n\nIf this is happening repeatedly, you can read on the website how to prevent this from happening again (look for '"APP_NAME" mysteriously stops working' in the documentation).",
};

#endif
