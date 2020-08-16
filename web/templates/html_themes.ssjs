/* $Id: html_themes.ssjs,v 1.11 2006/04/13 18:58:32 rswindell Exp $ */

/* Set default theme name */

var DefaultTheme="Default";

/* Edit this bit to add/remove/modify theme descriptions, dirs */

Themes["Default"]=new Object;
Themes["Default"].desc="Default Synchronet Theme";
Themes["Default"].theme_dir="default";
Themes["Default"].image_dir="/images/default";
Themes["Default"].lib_dir="../web/lib";
Themes["Default"].css="/default.css";
Themes["Default"].do_header=true;
Themes["Default"].do_topnav=true;
Themes["Default"].do_leftnav=true;
Themes["Default"].do_rightnav=false;
Themes["Default"].do_footer=true;
Themes["Default"].do_extra=false;
Themes["Default"].do_forumlook=false;
Themes["Default"].msgs_displayed=1;


Themes["NightShade"]=new Object;
Themes["NightShade"].desc="NightShade";
Themes["NightShade"].theme_dir="nightshade";
Themes["NightShade"].image_dir="/images/nightshade";
Themes["NightShade"].lib_dir="../web/lib/nightshade";
Themes["NightShade"].css="/nightshade.css";
Themes["NightShade"].do_header=true;
Themes["NightShade"].do_topnav=true;
Themes["NightShade"].do_leftnav=true;
Themes["NightShade"].do_rightnav=true;
Themes["NightShade"].do_footer=true;
Themes["NightShade"].do_extra=true;
Themes["NightShade"].do_forumlook=true;
Themes["NightShade"].msgs_displayed=5;

