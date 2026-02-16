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
                writeln(C_HEAD + "Usage: jsexec lastlogins.js [options]");
		writeln(C_HEAD + "Usage: jsexec lastlogins.js [options]");
		writeln(C_SUB + "Options:");
		writeln("  -o     Hide ONLY the dynamic box header");
		writeln("  -n x   Show x results (default 10)");
		writeln("  -g     Omit GUEST accounts");
		writeln("  -s     Omit SYSOP accounts");
		writeln("  -j     Save results to last10logins.json" + C_RESET);
                exit();
            }
            if (arg === "-o") noBox = true;
            if (arg === "-j") saveJson = true;
            if (arg === "-g") noGuest = true;
            if (arg === "-s") noSysop = true;
            if (arg === "-n" && argv[i+1]) limit = parseInt(argv[++i], 10) || 10;
        }
    }

    var logins = [];
    var lastUser = system.lastuser; // Local variable for speed
    
    // Collect full user objects with ARS filtering
    for (var j = 1; j <= lastUser; j++) {
        var u = new User(j);
        if (!u || !u.alias || !u.stats.laston_date) continue;
        
        if (noGuest && u.compare_ars("GUEST")) continue;
        if (noSysop && u.compare_ars("SYSOP")) continue;
        
        logins.push(u);
    }

    // Sort: Newest first
    logins.sort(function(a, b) { return b.stats.laston_date - a.stats.laston_date; });
    var finalResults = logins.slice(0, limit);

    // Save to JSON using system.data_dir
    if (saveJson) {
        var jsonFile = new File(system.data_dir + "last10logins.json");
        if (jsonFile.open("w")) {
            jsonFile.write(JSON.stringify(finalResults, null, 4));
            jsonFile.close();
        }
        if (argCount === 1) exit(); 
    }

    // --- DISPLAY LOGIC ---
    var header_path = system.text_dir + "last10loginsheader.asc";
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

    var colHeader = gettext("  Rank  Date                 Username             Handle               Protocol", "ll_col_headers");
    var separator = gettext("  ----  ----------           -------------------- -------------------- --------", "ll_separator");

    writeln("");
    writeln(C_GRAY + colHeader);
    writeln(C_SUB + separator + C_RESET);

    for (var k = 0; k < finalResults.length; k++) {
        var l = finalResults[k];
        // Using system.datestr for system-configured date format
        var dateStr = system.datestr(l.stats.laston_date);
        
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
