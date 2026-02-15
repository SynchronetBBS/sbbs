/*	top user upload and download stats. 
	completely created by Gemini with
	direction from Nelgin. 

	To use, add it to a menu or a door program with the options
	of your chosing. How to do this can be found on the
	Synchronet Wiki at https://wiki.synchro.net

	Given no options, the script will show you the top 10
	uploaders and downloaders relating to total file size.

	-u	Uploaders only
	-d	Downloaders only

	-g	Do not include guest in the results
	-s	Do not include sysop in the results

	-n xx	Return this numebr of results (10 is the default)
	-o	Omit the header box
	-z	Hide lines if users have 0 bytes downloaded

	if the file /sbbs/text/topuldlheader.asc is present then
	it will be displayed at the top of the output.

	This script is generally unsupported but if you want to
	suggest additions or fixes, feel free to open a gitlab
	issue and assign it to "Nigel Reed".


	the default is equivalent to using -u -d -n 10
*/

"use strict";

load("sbbsdefs.js");
load("string.js");
load("gettext.js");

var is_bbs = (js.global.console !== undefined) ? true : false;
var term = "\r\n";

var C_HEAD  = is_bbs ? "\x01h\x01c" : "\x1b[1;36m";
var C_SUB   = is_bbs ? "\x01h\x01y" : "\x1b[1;33m";
var C_DATA  = is_bbs ? "\x01h\x01g" : "\x1b[1;32m";
var C_DATA2 = is_bbs ? "\x01h\x01c" : "\x1b[1;36m";
var C_RANK  = is_bbs ? "\x01h\x01w" : "\x1b[1;37m";
var C_GRAY  = is_bbs ? "\x01h\x01k" : "\x1b[1;30m";
var C_RESET = is_bbs ? "\x01n"      : "\x1b[0m";

// Locked - NEVER CHANGE
var BOX = is_bbs ? {
    tl: "\xDA", tr: "\xBF", bl: "\xC0", br: "\xD9", hz: "\xC4", vt: "\xB3"
} : {
    tl: "\xE2\x94\x8C", tr: "\xE2\x94\x90", bl: "\xE2\x94\x94", br: "\xE2\x94\x98", hz: "\xE2\x94\x80", vt: "\xE2\x94\x82"
};

function formatBytes(bytes) {
    bytes = parseFloat(bytes) || 0;
    if (bytes <= 0) return gettext("0 KB", "stats_zero_kb");
    var k = 1024;
    var sizes = [
        gettext("B", "unit_bytes"),
        gettext("KB", "unit_kb"),
        gettext("MB", "unit_mb"),
        gettext("GB", "unit_gb"),
        gettext("TB", "unit_tb")
    ];
    var i = Math.floor(Math.log(bytes) / Math.log(k));
    if (i < 1) i = 1; 
    return (bytes / Math.pow(k, i)).toFixed(2) + ' ' + sizes[i];
}

function displayTopStats() {
    var numResults = 10;
    var filterUl = false, filterDl = false;
    var noGuest = false, noSysop = false, hideZero = false, noBox = false;

    if (typeof argv !== 'undefined') {
        for (var i = 0; i < argv.length; i++) {
            var arg = argv[i];
            if (arg === "-h" || arg === "-?" || arg === "--help") {
                writeln(C_HEAD + gettext("Usage: jsexec topuldl.js [options]", "help_usage"));
                writeln(C_SUB + gettext("Options:", "help_options_title"));
                writeln("  -u     " + gettext("Show Uploads only", "help_opt_u"));
                writeln("  -d     " + gettext("Show Downloads only", "help_opt_d"));
                writeln("  -g     " + gettext("Hide Guest", "help_opt_g"));
                writeln("  -s     " + gettext("Hide Sysop", "help_opt_s"));
                writeln("  -z     " + gettext("Hide users with 0 bandwidth", "help_opt_z"));
                writeln("  -o     " + gettext("Hide ONLY the dynamic box header", "help_opt_o"));
                writeln("  -n X   " + gettext("Show X results (default 10)", "help_opt_n") + C_RESET);
                exit(); 
            }
            if (arg === "-u") filterUl = true;
            if (arg === "-d") filterDl = true;
            if (arg === "-g") noGuest = true;
            if (arg === "-s") noSysop = true;
            if (arg === "-z") hideZero = true;
            if (arg === "-o") noBox = true;
            if (arg === "-n" && argv[i+1]) numResults = parseInt(argv[++i], 10) || 10;
        }
    }

    // Logic: If both are selected or neither are selected, show both.
    var showUl = (filterUl || !filterDl);
    var showDl = (filterDl || !filterUl);

    var header_path = system.text_dir + "topuldlheader.asc";
    if (file_exists(header_path)) {
        var f = new File(header_path);
        if (f.open("r")) {
            writeln(f.read());
            f.close();
        }
    }

    if (!noBox) {
        var title = system.name.toUpperCase() + " " + gettext("TOP USER STATS", "box_title_text");
        var width = 76;
        var padding = Math.max(0, Math.floor((width - title.length) / 2));
        var centeredTitle = " ".repeat(padding) + title;
        centeredTitle = centeredTitle.padEnd(width);

        writeln(C_HEAD + BOX.tl + BOX.hz.repeat(width) + BOX.tr);
        writeln(C_HEAD + BOX.vt + C_RANK + centeredTitle + C_HEAD + BOX.vt);
        writeln(C_HEAD + BOX.bl + BOX.hz.repeat(width) + BOX.br + C_RESET);
    }

    var userList = [];
    for (var j = 1; j <= system.lastuser; j++) {
        var u = new User(j);
        if (!u || !u.alias || u.compare_ars("RESTRICT D")) continue;
        if (noGuest && u.compare_ars("GUEST")) continue;
        if (noSysop && u.compare_ars("SYSOP")) continue;
        
        userList.push({
            name: u.alias,
            handle: u.handle || u.alias,
            ul: (u.stats && u.stats.bytes_uploaded) ? u.stats.bytes_uploaded : 0,
            dl: (u.stats && u.stats.bytes_downloaded) ? u.stats.bytes_downloaded : 0,
            last: (u.stats && u.stats.laston_date) ? strftime("%m/%d/%y", u.stats.laston_date) : gettext("Never", "stats_never")
        });
    }

    var colHeader = gettext("  Rank  Username             Handle               Bandwidth    Last Seen", "table_col_headers");
    var separator = gettext("  ----  -------------------- -------------------- -----------  ----------", "table_separator");

    if (showUl) {
        var ulData = userList.slice().filter(function(x) { return !hideZero || x.ul > 0; }).sort(function(a, b) { return b.ul - a.ul; });
        writeln("");
        writeln(C_SUB + " " + gettext("[ TOP UPLOADERS ]", "hdr_top_ul"));
        writeln(C_GRAY + colHeader);
        writeln(C_SUB + separator + C_RESET);
        for (var k = 0; k < Math.min(numResults, ulData.length); k++) {
            writeln("  " + C_RANK + (k + 1).toString().padEnd(4) + "  " + C_RESET + 
                  ulData[k].name.substring(0, 20).padEnd(20) + " " + 
                  C_GRAY + ulData[k].handle.substring(0, 20).padEnd(20) + " " + 
                  C_DATA + formatBytes(ulData[k].ul).padStart(11) + "  " +
                  C_GRAY + ulData[k].last.padStart(10) + C_RESET);
        }
    }

    if (showDl) {
        var dlData = userList.slice().filter(function(x) { return !hideZero || x.dl > 0; }).sort(function(a, b) { return b.dl - a.dl; });
        writeln("");
        writeln(C_SUB + " " + gettext("[ TOP DOWNLOADERS ]", "hdr_top_dl"));
        writeln(C_GRAY + colHeader);
        writeln(C_SUB + separator + C_RESET);
        for (var l = 0; l < Math.min(numResults, dlData.length); l++) {
            writeln("  " + C_RANK + (l + 1).toString().padEnd(4) + "  " + C_RESET + 
                  dlData[l].name.substring(0, 20).padEnd(20) + " " + 
                  C_GRAY + dlData[l].handle.substring(0, 20).padEnd(20) + " " + 
                  C_DATA2 + formatBytes(dlData[l].dl).padStart(11) + "  " +
                  C_GRAY + dlData[l].last.padStart(10) + C_RESET);
        }
    }
}

displayTopStats();
