/*      Last 10 logins across all methods
        completely created by Gemini with
        direction from Nelgin.

        To use, add it to a menu or a door program with the options
        of your chosing. How to do this can be found on the
        Synchronet Wiki at https://wiki.synchro.net

        Given no options, the script will show you the last 10
        logins.

        -j      Create a JSON file will all user details. Can be
                used in scripts to customize display

        -g      Do not include guest in the results
        -s      Do not include sysop in the results

        -n xx   Return this numebr of results (10 is the default)
        -o      Omit the header box

        if the file /sbbs/text/last10callersheader.asc is present then
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

// Locked BOX variable - DO NOT CHANGE
var BOX = is_bbs ? {
    tl: "\xDA", tr: "\xBF", bl: "\xC0", br: "\xD9", hz: "\xC4", vt: "\xB3"
} : {
    tl: "\xE2\x94\x8C", tr: "\xE2\x94\x90", bl: "\xE2\x94\x94", br: "\xE2\x94\x98", hz: "\xE2\x94\x80", vt: "\xE2\x94\x82"
};

var C_HEAD  = is_bbs ? "\x01h\x01c" : "\x1b[1;36m";
var C_SUB   = is_bbs ? "\x01h\x01y" : "\x1b[1;33m";
var C_RANK  = is_bbs ? "\x01h\x01w" : "\x1b[1;37m";
var C_GRAY  = is_bbs ? "\x01h\x01k" : "\x1b[1;30m";
var C_RESET = is_bbs ? "\x01n"      : "\x1b[0m";

function displayLastLogins() {
    var limit = 10;
    var noBox = false;
    var saveJson = false;
    var noGuest = false;
    var noSysop = false;
    var argCount = 0;

    // Command line argument processing
    if (typeof argv !== 'undefined') {
        argCount = argv.length;
        for (var i = 0; i < argv.length; i++) {
            var arg = argv[i];
            if (arg === "-h" || arg === "-?" || arg === "--help") {
                writeln(C_HEAD + gettext("Usage: jsexec lastlogins.js [options]", "ll_help_usage"));
                writeln(C_SUB + gettext("Options:", "ll_help_options_title"));
                writeln("  -o     " + gettext("Hide ONLY the dynamic box header", "ll_help_opt_o"));
                writeln("  -n X   " + gettext("Show X results (default 10)", "ll_help_opt_n"));
                writeln("  -g     " + gettext("Omit GUEST accounts", "ll_help_opt_g"));
                writeln("  -y/-s  " + gettext("Omit SYSOP accounts", "ll_help_opt_y"));
                writeln("  -j     " + gettext("Save results to last10logins.json", "ll_help_opt_j") + C_RESET);
                exit();
            }
            if (arg === "-o") noBox = true;
            if (arg === "-j") saveJson = true;
            if (arg === "-g") noGuest = true;
            if (arg === "-y" || arg === "-s") noSysop = true;
            if (arg === "-n" && argv[i+1]) limit = parseInt(argv[++i], 10) || 10;
        }
    }

    var logins = [];
    for (var j = 1; j <= system.lastuser; j++) {
        var u = new User(j);
        if (!u || !u.alias || !u.stats.laston_date) continue;
        
        if (noGuest && u.compare_ars("GUEST")) continue;
        if (noSysop && u.compare_ars("SYSOP")) continue;
        
        logins.push(u);
    }

    logins.sort(function(a, b) { return b.stats.laston_date - a.stats.laston_date; });
    var finalResults = logins.slice(0, limit);

    // Save to JSON
    if (saveJson) {
        var jsonFile = new File("/sbbs/data/last10logins.json");
        if (jsonFile.open("w")) {
            jsonFile.write(JSON.stringify(finalResults, null, 4));
            jsonFile.close();
        }
        // If -j was the ONLY argument, exit now. Otherwise, proceed to print.
        if (argCount === 1) exit(); 
    }

    // --- DISPLAY LOGIC ---
    var header_path = "/sbbs/text/last10loginsheader.asc";
    if (file_exists(header_path)) {
        var f = new File(header_path);
        if (f.open("r")) {
            writeln(f.read());
            f.close();
        }
    }

    if (!noBox) {
        var title = system.name.toUpperCase() + " " + gettext("RECENT LOGINS", "ll_box_title");
        var width = 76;
        var padding = Math.max(0, Math.floor((width - title.length) / 2));
        var centeredTitle = " ".repeat(padding) + title;
        centeredTitle = centeredTitle.padEnd(width);

        writeln(C_HEAD + BOX.tl + BOX.hz.repeat(width) + BOX.tr);
        writeln(C_HEAD + BOX.vt + C_RANK + centeredTitle + C_HEAD + BOX.vt);
        writeln(C_HEAD + BOX.bl + BOX.hz.repeat(width) + BOX.br + C_RESET);
    }

    var colHeader = gettext("  Rank  Date/Time            Username             Handle               Protocol", "ll_col_headers");
    var separator = gettext("  ----  -------------------  -------------------- -------------------- --------", "ll_separator");

    writeln("");
    writeln(C_GRAY + colHeader);
    writeln(C_SUB + separator + C_RESET);

    for (var k = 0; k < finalResults.length; k++) {
        var l = finalResults[k];
        var dateStr = strftime("%m/%d/%y %H:%M:%S", l.stats.laston_date);
        
        writeln(
            "  " + C_RANK + (k + 1).toString().padEnd(4) + "  " +
            C_GRAY + dateStr.padEnd(19) + "  " +
            C_RESET + l.alias.substring(0, 20).padEnd(20) + " " + 
            C_GRAY + (l.handle || l.alias).substring(0, 20).padEnd(20) + " " + 
            C_SUB + (l.connection || "???").substring(0, 8).toUpperCase() + C_RESET
        );
    }
}

displayLastLogins();
