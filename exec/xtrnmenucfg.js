"use strict"

/**
 * Menu editor for Custom External Program Menus
 * by Michael Long mlong  innerrealmbbs.us
 *
 * This edits the file xtrnmenu.cfg
 */

load("sbbsdefs.js");
load("uifcdefs.js");

var log = function(msg) {
    var f = new File("uifc.log");
    f.open("a");
    f.writeln(system.timestr() + ": " + msg);
    f.close();
}

/**
 * Write the menus out to a file
 */
var saveMenus = function() {

    var filename = system.ctrl_dir + "xtrnmenu.cfg";

    file_backup(filename, 5);

    var config_file = new File(filename);
    if (!config_file.open('w+')) {
        uifc.msg("ERROR: Could not write to " + filename);
        return;
    }
    config_file.write(JSON.stringify(menuconfig, null, '    '));
    config_file.close();

    uifc.msg("Config Saved");
}

/**
 * Edit a custom menu
 * @param menuid
 */
var editMenu = function(menuid) {
    var menuindex, menu, menuindex, selection, selection2, editproperty;
    var last = 0;
    var selections = [], displayoptions = [], displayoptionids = [];
    var menusize = menuconfig.menus.length;

    // new menu but no code given, make one
    if ((typeof menuid === "undefined") || (!menuid)) {
        menuid = time();
    }

    // look for existing menu
    for (var i in menuconfig.menus) {
        if (menuconfig.menus[i].id == menuid) {
            menu = menuconfig.menus[i];
            menuindex = i;
        }
    }

    if (typeof menu  === "undefined") {
        menuindex = menusize;
        menuconfig.menus[menuindex] = {
            'id': menuid,
            'title': "New Generated Menu " + menuid,
            "sort_type": "title",
            'items': []
        };
        menu = menuconfig.menus[menuindex];
    }

    while(1) {
        uifc.help_text = word_wrap("This screen allows you to edit the configuration options for the custom menu.\r\n\r\nMost options default or are set in modopts.ini, but here you can define them on a per-menu basis.\r\n\r\nClick Edit Items to edit the individual entries (programs, menus, etc.)");

        selections = [];
        for (var j in menu.items) {
            selections.push(menu.items.index);
        }

        displayoptions = [];
        displayoptionids = [];

        // setup display menu
        displayoptions.push(format("%23s: %s", "id",
            ("id" in menu ? menu.id : time())));
        displayoptionids.push("id");

        displayoptions.push(format("%23s: %s", "title",
            ("title" in menu ? menu.title : "")));
        displayoptionids.push("title");

        displayoptions.push(format("%23s: %s", "sort_type",
            ("sort_type" in menu ? menu.sort_type : "(default)")));
        displayoptionids.push("sort_type");

        displayoptions.push(format("%23s: %s", "Edit Items", "[...]"));
        displayoptionids.push("items");
        
        selection = uifc.list(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC, 0, 0, 0, last, last,
            menu.title + ": Options", displayoptions);

        if (selection < 0) {
            // escape key
            break;
        }

        editproperty = displayoptionids[selection];

        switch (editproperty) {
            case 'id':
                uifc.help_text = word_wrap("This is a unique ID for the menu, which can be used as the target to\r\ncall the menu from other menus.\r\n\r\nFor the top-level menu, the id should be 'main'.");
                var selection2 = uifc.input(WIN_MID, "Menu ID", menu.id, 50, K_EDIT);
                if ((selection2 < 0) || (selection2 == null)) {
                    // escape key
                    break;
                }
                menu.id = selection2;
                break;
            case 'title':
                uifc.help_text = word_wrap("Title for the menu, to be shown at the top of the menu");
                selection2 = uifc.input(WIN_MID, "Menu Title", menu.title, 255, K_EDIT);
                if ((selection2 < 0) || (selection2 == null)) {
                    // escape key
                    break;
                }
                menu.title = selection2;
                break;

            case 'sort_type':
                uifc.help_text = word_wrap("How to sort the menu:\r\nby input key\r\nby title\r\nnone (the items remain in the order they are in the config)");

                switch (uifc.list(WIN_ORG | WIN_MID, "Sort Type", ["key", "title", "none"])) {
                    case 0:
                        menu.sort_type = "key";
                        break;
                    case 1:
                        menu.sort_type = "title";
                        break;
                    case 2:
                        delete menu.sort_type;
                        break;
                    default:
                        //esc key
                        break;
                }

            case 'items':
                editItems(menuid);
                break;

            default:
                // this isn't supposed to happen
                uifc.msg("Unknown option");
                break;
        }

        last = Math.max(selection, 0);
    }
}

/**
 * Edit menu items in a menu
 * @param menuid
 */
var editItems = function(menuid) {
    var menuindex, menu, selection, selection2, keyused, items = [], itemids = [];
    var i, last = 0;

    // cur bar top left width
    var ctxm = new uifc.list.CTX(0, 0, 0, 0, 0);
    
    if (typeof menuid === "undefined") {
        uifc.msg("Menu could not be found");
        return;
    } else {
        for (i in menuconfig.menus) {
            if (menuconfig.menus[i].id == menuid) {
                menu = menuconfig.menus[i];
                menuindex = i;
            }
        }
    }
    
    if ((typeof menu.items == "undefined") || (menu.items.length == 0)) {
        // no items, prompt them to make one
        editItem(menu.id, 0);
    }
    

    uifc.help_text = word_wrap("This menu allows editing the various items in this menu.\r\n\r\n"
        + "If you leave input key blank, it will use an auto-generated number at display time.\r\n\r\n"
        + "Choose a type first and the dropdown to choose tha target will allow you to select your target.\r\n\r\n"
        + "Access string only applies to custom menu items. For external sections or external programs, use the access settings in scfg.\r\n\r\n");

    while(1) {
        items = [];
        itemids = [];
        for(i in menu.items) {
            items.push(format(
                "%6s %10s %s",
                menu.items[i].input ? menu.items[i].input : '(auto)',
                menu.items[i].type,
                menu.items[i].title
            ));
            itemids.push(i);
        }
        // WIN_ORG = original menu
        // WIN_MID = centered mid
        // WIN_ACT = menu remains active after a selection
        // WIN_ESC = screen is active when escape is pressed
        // WIN_XTR = blank line to insert
        // WIN_INS = insert key
        // WIN_DEL = delete
        // WIN_CUT = cut ctrl-x
        // WIN_COPY = copy ctrl-c
        // WIN_PUT = paste ctrl-v
        // WIN_SAV = use context/save position
        selection = uifc.list(
            WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC|WIN_XTR|WIN_INS|WIN_DEL|WIN_CUT|WIN_COPY|WIN_PASTE|WIN_SAV,
            menu.title + ": Items",
            items,
            ctxm
        );
        
        if (selection == -1) {
            // esc key
            break;
        }

        if ((selection & MSK_ON) == MSK_DEL) {
            // delete item
            selection &= MSK_OFF;
            //renumber array so there are no gaps
            var menuitems2 = [];
            for (var i in menu.items) {
                if (i != itemids[selection]) {
                    menuitems2.push(menu.items[i]);
                }
            }
            menu.items = menuitems2;
        } else if (((selection & MSK_ON) == MSK_INS)) {
            // new item from INSERT KEY
            editItem(menuid, null);
        } else if ((selection & MSK_ON) == MSK_COPY) {
            // copy item
            selection &= MSK_OFF;
            copyitem = JSON.parse(JSON.stringify(menu.items[itemids[selection]])); // make copy
        } else if ((selection & MSK_ON) == MSK_CUT) {
            // cut item
            selection &= MSK_OFF;
            copyitem = menu.items[itemids[selection]];

            //renumber array so there are no gaps
            var menuitems2 = [];
            for (var i in menu.items) {
                if (i != itemids[selection]) {
                    menuitems2.push(menu.items[i]);
                }
            }
            menu.items = menuitems2;
        } else if ((selection & MSK_ON) == MSK_PASTE) {
            // paste item
            selection &= MSK_OFF;

            var oktopaste = true;

            // only paste if there is an item copied
            if ("type" in copyitem) {
                // if item already exists in list, modify if since you can't have dupes (except empty input keys)
                for (var i in menu.items) {
                    if ((menu.items[i].input == copyitem.input) && (copyitem.input !== null) && (copyitem.input !== "")) {
                        oktopaste = true;
                        while(1) {
                            selection2 = uifc.input(WIN_MID, "Enter New Input Key", "", 3, K_EDIT);
                            if ((selection2 == -1) || (selection2 === "") || (selection2 === null)) {
                                // escape key
                                copyitem.input = null;
                                break;
                            }
                            if (selection2 || selection2 === 0) {
                                selection2 = selection2.toUpperCase();
                                keyused = false;
                                for (var j in menu.items) {
                                    if (menu.items[j].input && (menu.items[j].input.toUpperCase() == selection2)) {
                                        keyused = true;
                                    }
                                }
                                if (keyused) {
                                    uifc.msg("This input key is alread used for another item");
                                    oktopaste = false;
                                } else {
                                    copyitem.input = selection2;
                                    oktopaste = true;
                                    break;
                                }
                            }
                        }
                        copyitem.input = selection2;
                    }
                }
                if ((oktopaste) || (copyitem.input === "null") || (copyitem.input === "")) {
                    var menuitems2 = [];
                    for (i in menu.items) {
                        menuitems2.push(menu.items[i]);
                        // paste copied item after selected item
						if (i == itemids[selection]) {
							menuitems2.push(copyitem);
                            ctxm.cur = i-1;
						}
                    }
                    menu.items = menuitems2;
                }
            }
        } else if (selection >= menu.items.length) {
            // new item from blank line
            editItem(menuid, null);
        } else {
            editItem(menuid, itemids[selection]);
        }
        last = Math.max(selection, 0);
    }
}

/**
 * Edit a specific menu item entry
 * @param menuid
 * @param itemindex
 */
var editItem = function(menuid, itemindex) {
    var menu, menuindex, item;
    var keyused, selection, selection2, i, last = 0;
    var displayoptions = [], displayoptionids = [], newitems = [];
    // used for building target selection
    var custommenuitems = [], custommenuitemsids = [], custommenunames = [];

    if (typeof menuid === "undefined") {
        uifc.msg("Menu could not be found");
        return;
    } else {
        for (i in menuconfig.menus) {
            if (menuconfig.menus[i].id == menuid) {
                menu = menuconfig.menus[i];
                menuindex = i;
            }
        }
    }

    if (typeof menu.items[itemindex] === "undefined") {
        // new item
        menu.items.push({
            "input": null,
            "title": "New Item " + time(),
            "type": null,
            "target": null,
            "access_string": null,
        });
        itemindex = menu.items.length - 1;
        present_select_targettype(menu.items[itemindex]);
    }
    item = menu.items[itemindex];

    var itemctx = new uifc.list.CTX(0,0,0,0,0);
    while(1) {
        displayoptions = [];
        displayoptionids = [];

        // setup display menu
        displayoptions.push(format("%23s: %s", "input",
            (("input" in item) && (item.input !== null) && (item.input !== "") ? item.input : "(auto)")));
        displayoptionids.push("input");

        displayoptions.push(format("%23s: %s", "title",
            ("title" in item ? item.title : "")));
        displayoptionids.push("title");

        displayoptions.push(format("%23s: %s", "type",
            ("type" in item ? item.type : "")));
        displayoptionids.push("type");

        displayoptions.push(format("%23s: %s", "target",
            ("target" in item ? item.target : "")));
        displayoptionids.push("target");

        if (item.type == "custommenu") {
            displayoptions.push(format("%23s: %s", "access_string",
                ("access_string" in item ? item.access_string : "(default)")));
            displayoptionids.push("access_string");
        }

        selection = uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_ESC,
            menu.title + ": Item " + itemindex, displayoptions, itemctx);

        if (selection < 0) {
            if (!item.title || !item.type || !item.target) {
                if (uifc.list(WIN_ORG | WIN_MID, "This item is missing required items.", ["Remove Item", "Edit Item"]) == 0) {
                    // delete item and continue
                    newitems = [];
                    for (i in menu.items) {
                        if (i != itemindex) {
                            newitems.push(menu.items[i]);
                        }
                    }
                    menu.items = newitems;
                    break;
                }
            } else {
                // leave menu
                break;
            }
        }

        switch (displayoptionids[selection]) {
            
            case 'input':
                uifc.help_text = word_wrap("The input key to access this item. Can be anything except Q. Leave blank to auto-generate a number.");
                selection2 = uifc.input(WIN_MID, "Input Key", item.input, 3, K_EDIT);
                if ((selection2 < 0) || (selection2 == null)) {
                    // escape key
                    break;
                }

                if (selection2 !== "") {
                    selection2 = selection2.toUpperCase();

                    keyused = false;
                    for (i in menu.items) {
                        if ((menu.items[i].input === null) || (menu.items[i].input === "")) {
                            // continue here as toUpperCase would break it, and they don't need to match
                            // anyway because you can have multiple auto-assigned input items
                            continue;
                        }
                        if ((menu.items[i].input.toUpperCase() == selection2) && (i != itemindex)) {
                            keyused = true;
                        }
                    }

                    if (keyused) {
                        uifc.msg("This input key is already used by another item.");
                    } else {
                        item.input = selection2;
                    }
                } else {
                    // save blank
                    item.input = selection2;
                }
                break;

            case 'title':
                uifc.help_text = word_wrap("The menu item title.");
                selection2 = uifc.input(WIN_MID, "Title", item.title, 255, K_EDIT);
                if ((selection2 < 0) || (selection2 == null)) {
                    // escape key
                    break;
                }
                if (!selection2 && selection2 !== 0) {
                    uifc.msg("Title is required.");
                } else {
                    item.title = selection2;
                }
                break;

            case 'type':
                present_select_targettype(item);
                break;

            case 'target':
                present_select_target(item);
                break;

            case 'access_string':
                uifc.help_text = word_wrap("The access string for the custom menu.\r\n\r\nOnly applies to custom menu items.\r\n\r\nExample: LEVEL 60");
                selection2 = uifc.input(WIN_MID, "Access String", item.access_string, 255, K_EDIT);
                if ((selection2 < 0) || (selection2 == null)) {
                    // escape key
                    break;
                }
                item.access_string = selection2;
                break;
                
        }
        last = Math.max(selection, 0);
    }
}

function present_select_targettype(item)
{
    uifc.help_text = word_wrap(
        "This is the type of target this item points to.\r\n\r\n"
        + "custommenu is a custom menu defined in this tool.\r\n\r\n"
        + "xtrnmenu is a standard Syncrhonet External Section Menu (refer to the scfg tool).\r\n\r\n"
        + "xtrnprog is a direct link to an external program (refer to the scfg tool)");

    var targetypectx = uifc.list.CTX(0, 0, 0, 0, 0);
    if (typeof item.type !== "undefined") {
        switch (item.type) {
            case 'custommenu':
                targetypectx.cur = 0;
                targetypectx.bar = 0;
                break;
            case 'xtrnmenu':
                targetypectx.cur = 1;
                targetypectx.bar = 1;
                break;
            case 'xtrnprog':
                targetypectx.cur = 2;
                targetypectx.bar = 2;
                break;
        }
    }
    switch (uifc.list(WIN_ORG | WIN_MID | WIN_SAV,
        "Target Type", ["custommenu", "xtrnmenu", "xtrnprog"], targetypectx)) {
        case 0:
            item.type = "custommenu";
            break;
        case 1:
            item.type = "xtrnmenu"
            break;
        case 2:
            item.type = "xtrnprog";
            break;
        default:
            // includes escape key
            break;
    }

    // convienence... enter target selection
    present_select_target(item)    
}

function present_select_target(item)
{
    uifc.help_text = word_wrap("This is the ID of the custom menu, external program section, or external program to link to.");

    var targetctx = uifc.list.CTX(0, 0, 0, 0, 0);

    var custommenuitems = [];
    var custommenuitemsids = [];
    var custommenunames = [];
    
    var selection2;
    
    switch (item.type) {
        case "custommenu":
            // present list of custom menus
            for (i in menuconfig.menus) {
                custommenuitems.push(format("%23s: %s", menuconfig.menus[i].id, menuconfig.menus[i].title));
                custommenuitemsids.push(menuconfig.menus[i].id);
                custommenunames.push(menuconfig.menus[i].title);
            }

            if ((typeof item.target !== "undefined") && item.target) {
                for (var p in custommenuitemsids) {
                    if (custommenuitemsids[p] == item.target) {
                        targetctx.cur = p;
                        targetctx.bar = p;
                    }
                }
            }

            selection2 = uifc.list(WIN_ORG | WIN_MID | WIN_SAV, "Target", custommenuitems, targetctx);
            if ((selection2 < 0) || (selection2 == null)) {
                // escape key
                break;
            }

            item.target = custommenuitemsids[selection2];

            while(1) {
                if (uifc.list(WIN_ORG | WIN_MID, "Replace item title with sections's name?", ["Yes", "No"]) == 0) {
                    item.title = custommenunames[selection2]; // for external program, change title to program name
                }
                break;
            }
            break;

        case "xtrnmenu":
            // present list of external program sections
            var seclist = [];
            for (i in xtrn_area.sec_list) {
                seclist.push({ code: xtrn_area.sec_list[i].code, name: xtrn_area.sec_list[i].name});
            };
            seclist.sort(sort_by_code);
            
            for (i in seclist) {
                custommenuitems.push(format("%23s: %s", seclist[i].code, seclist[i].name));
                custommenuitemsids.push(seclist[i].code);
                custommenunames.push(seclist[i].name);
            }

            if ((typeof item.target !== "undefined") && item.target) {
                for (var p in custommenuitemsids) {
                    if (custommenuitemsids[p].toLowerCase() == item.target.toLowerCase()) {
                        targetctx.cur = p;
                        targetctx.bar = p;
                    }
                }
            }

            selection2 = uifc.list(WIN_ORG | WIN_MID | WIN_SAV, "Target", custommenuitems, targetctx);
            if ((selection2 < 0) || (selection2 == null)) {
                // escape key
                break;
            }

            item.target = custommenuitemsids[selection2];

            while(1) {
                if (uifc.list(WIN_ORG | WIN_MID, "Replace item title with sections's name?", ["Yes", "No"]) == 0) {
                    item.title = custommenunames[selection2]; // for external program, change title to program name
                }
                break;
            }
            break;

        case "xtrnprog":

            // present list of external programs
            // create sorted list
            var proglist = [];

            for (i in xtrn_area.prog) {
                proglist.push({ code: xtrn_area.prog[i].code, name: xtrn_area.prog[i].name});
            };
            proglist.sort(sort_by_code);
            for (i in proglist) {
                custommenuitems.push(format("%23s: %s", proglist[i].code, proglist[i].name));
                custommenuitemsids.push(proglist[i].code);
                custommenunames.push(proglist[i].name);
            }

            if ((typeof item.target !== "undefined") && item.target) {
                for (var p in custommenuitemsids) {
                    if (custommenuitemsids[p].toLowerCase() == item.target.toLowerCase()) {
                        targetctx.cur = p;
                        targetctx.bar = p;
                    }
                }
            }

            selection2 = uifc.list(WIN_ORG | WIN_MID | WIN_SAV, "Target", custommenuitems, targetctx);
            if ((selection2 < 0) || (selection2 == null)) {
                // escape key
                break;
            }
            if (selection2 || selection2 === 0) {
                item.target = custommenuitemsids[selection2];
                while(1) {
                    if (uifc.list(WIN_ORG | WIN_MID, "Replace item title with sections's name?", ["Yes", "No"]) == 0) {
                        item.title = custommenunames[selection2]; // for external program, change title to program name
                    }
                    break;
                }
            }
            break;

        default:
            selection2 = uifc.input(WIN_ORG | WIN_MID, "Target", item.target, 50, K_EDIT);
            if ((selection2 < 0) || (selection2 == null)) {
                // escape key
                break;
            }

            item.target = selection2;
            break;
    }    
}

function sort_by_name(a, b)
{
    if (a.name.toLowerCase() > b.name.toLowerCase()) return 1;
    if (a.name.toLowerCase() < b.name.toLowerCase()) return -1;
    return 0;
}

function sort_by_code(a, b)
{
    if (a.code.toLowerCase() > b.code.toLowerCase()) return 1;
    if (a.code.toLowerCase() < b.code.toLowerCase()) return -1;
    return 0;
}

function sort_by_id(a, b)
{
    if (a.id.toLowerCase() > b.id.toLowerCase()) return 1;
    if (a.id.toLowerCase() < b.id.toLowerCase()) return -1;
    return 0;
}


// MAIN
try {
    var menuconfig = {};
    var copyitem = {}; // for menu item copy/paste
    var config_file = new File(file_cfgname(system.ctrl_dir, "xtrnmenu.cfg"));
    if (config_file.open('r+')) {
        var config_src = config_file.read();
        try {
            menuconfig = JSON.parse(config_src.toString());
            if (!menuconfig) {
                writeln("ERROR! Could not parse xtrnmenu.cfg. JSON may be invalid.");
                exit();
            }
        } catch (e) {
            writeln("ERROR! Could not parse xtrnmenu.cfg. JSON may be invalid. " + e.toString());
            exit();
        }
    }
    config_file.close();
    
    if (typeof menuconfig.menus === "undefined") {
        menuconfig.menus = [];
    }

    uifc.init("Enhanced External Program Menus Configurator");
    uifc.lightbar_color = 120;
    uifc.background_color = 21; 
    uifc.frame_color = 15;
    js.on_exit("if (uifc.initialized) uifc.bail()");

    // cur bar top left width
    var ctx = new uifc.list.CTX(0, 0, 0, 0, 0);

    while(1) {
        uifc.help_text = word_wrap("This program allows managing the Enhanced External Program Menu feature.");

        // no menus or no main menu
        var mainmenufound = false;
        for (var m in menuconfig.menus) {
            if (menuconfig.menus[m].id.toLowerCase() == "main") {
                mainmenufound = true;
            }
        }
        if (!mainmenufound || (menuconfig.menus.length < 1)) {
            uifc.msg("No menus defined and/or missing the main menu. Setting up now.");
            editMenu("main");
        }

        // menus is array of menuconfig menu ids
        var menus = [];
        var menuTitles = [];
        menuconfig.menus.sort(sort_by_id);
        for (var m in menuconfig.menus) {
            menus.push(menuconfig.menus[m].id);
            menuTitles.push(format("%20s: %s", menuconfig.menus[m].id, menuconfig.menus[m].title));
        }
        
        menuTitles.push(format("%20s  %s", '', "[Save Config Without Exit]"));
        
        // WIN_ORG = original menu, destroy valid screen area
        // WIN_MID = place window in middle of screen
        // WIN_XTR = add extra line at end for inserting at end
        // WIN_DEL = allow user to use delete key
        // WIN_ACT = menu remains active after a selection
        // WIN_ESC = screen is active when escape is hit
        // WIN_INS = allow user to use insert key
        var selection = uifc.list(
            WIN_ORG|WIN_MID|WIN_XTR|WIN_DEL|WIN_ACT|WIN_ESC|WIN_INS|WIN_SAV,
            "Enhanced External Menus",
            menuTitles,
            ctx
        );

        if (selection == (menuTitles.length - 1)) {
            // last item - save config
            saveMenus();
            uifc.pop("Config saved.");
        } else if (selection < 0) {
            while (1) {
                var ret = uifc.list(WIN_ORG | WIN_MID, "Save Changes Before Exit?", ["Yes", "No", "Cancel"]);
                if (ret == 0) {
                    saveMenus();
                    exit();
                } else if (ret == 1) {
                    // no - exit
                    exit();
                } else {
                    // cancel
                    break;
                }
            }

        } else if ((selection & MSK_ON) == MSK_DEL) {
            selection &= MSK_OFF;
            var menus2 = [];
            for (var m in menuconfig.menus) {
                if (menuconfig.menus[m].id != menus[selection]) {
                    menus2.push(menuconfig.menus[m]);
                }
            }
            menuconfig.menus = menus2;
            //selection--;
        } else if (((selection & MSK_ON) == MSK_INS) || (selection >= menuconfig.menus.length)) {
            // new menu
            var newid = uifc.input(
                WIN_MID,
                "Enter a short unique id for the menu",
                "",
                0
            );
            if (typeof newid !== "undefined") {
                var menufound = false;
                for (var mf in menuconfig.menus) {
                    if (menuconfig.menus[mf].id == newid) {
                        menufound = true;
                    }
                }
                if (menufound) {
                    uifc.msg("That ID is already in use. Please choose another.");
                } else {
                    editMenu(newid);
                }
            }
        } else {
            editMenu(menuconfig.menus[selection].id);
        }
    }
} catch(err) {
    if ((typeof uifc !== "undefined") && uifc.initialized) {
        uifc.bail();
    }
    writeln(err);
    log(err);
    if (typeof console !== "undefined") {
        console.pause();
    }
}
