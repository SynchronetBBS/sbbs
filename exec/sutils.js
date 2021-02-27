"use strict";

const REVISION = "$Revision: 1.0 $".split(' ')[1];

load("sbbsdefs.js");

print("******************************************************************************");
print("*                         Synchronet Utilities v" + format("%-28s", REVISION) + " *");
print("*                 Use Ctrl-C to abort the process if desired                 *");
print("******************************************************************************");

var file = new File(system.exec_dir + "sutils.ini");
var categories;
if(file.open("r")) {
    categories = file.iniGetObject('categories');
    file.close();
}


var which, dopause;
var categoryitems = [];
//while((!which || which < 1) && !aborted()) {
while((!which || which < 1) && !aborted()) {
    dopause = false;
    print("");
    print(">>>> Main Menu");
    print("");
    if (typeof categories !== "undefined") {
        var x = 1;
        for (var catid in categories) {
            printf("%2d. %s\r\n", x, categories[catid]);
            categoryitems[x] = catid;
            x++;
        };
        print("");
        print(" Q. Quit");
    }
    print("");
    var str = prompt("Which Selection? ");
    if (str && str.toUpperCase() == 'Q') {
        exit(0);
    }
    which = parseInt(str, 10);
    if (which && (typeof categoryitems[which] !== "undefined")) {
        docategorymenu(categoryitems[which], categories[categoryitems[which]]);
    }

    which = -1; // redisplay menu
}

function docategorymenu(catid, catname) {
    var which, utilitemids;
    var menuitems = [];
    var utilitems = [];

    if (file.open("r")) {
        utilitemids = file.iniGetSections(catid);
    }

    for (var i in utilitemids) {
        utilitems[utilitemids[i]] = file.iniGetObject(utilitemids[i]);
    }

    file.close();

    while ((!which || which < 1) && !aborted()) {
        print("");
        print(">>>> " + catname);
        print("");

        if (typeof utilitems !== "undefined") {
            var x = 1;
            for (var iniid in utilitems) {
                if (file_exists(js.exec_dir + utilitems[iniid].filename)) {
                    printf("%2d. %-20s %s\r\n", x, utilitems[iniid].name, utilitems[iniid].desc);
                    menuitems[x] = iniid;
                    x++;
                }
            }
            print("");
            print(" Q. Return to Prior Menu");
        }

        print("");
        var str = prompt("Which Selection? ");
        if (str && str.toUpperCase() == 'Q') {
            return;
        }
        which = parseInt(str, 10);
        if (which && (typeof utilitems[menuitems[which]] !== "undefined")) {
            var program = utilitems[menuitems[which]];

            if (program.showhelpcmd) {
                print("");
                system.exec(js.exec_dir + program.showhelpcmd);
                print("");
            }
            if (program.showhelptext) {
                print("");
                print("Program Help: ");
                var helptexts = program.showhelptext.split("||");
                for (var h in helptexts) {
                    print(helptexts[h]);
                }
                print("");
            }
            if (program.promptforargs) {
                var consoleargs = prompt("Enter command line arguments (Q to quit): ");
                if (!consoleargs || (consoleargs && consoleargs.toUpperCase() != 'Q')) {
                    print("Running " + js.exec_dir + program.cmd + " " + consoleargs);
                    system.exec(js.exec_dir + program.cmd + " " + consoleargs);
                }
            } else {
                print("Running " + js.exec_dir + program.cmd);
                system.exec(js.exec_dir + program.cmd);
            }
            // pause so the redisplayed menu doesn't wipe out the prior program's output
            if (program.dopause) {
                var cont = prompt("Press ENTER to Continue");
            }
        }

        which = -1; // redisplay menu4
    }

}

function aborted()
{
    if (js.terminated || (js.global.console && console.aborted)) {
        exit(1);
    }
    return false;
}
