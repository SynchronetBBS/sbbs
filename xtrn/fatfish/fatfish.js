/* FatFish
 *
 * $Id: fatfish.js,v 1.12 2014/01/12 03:29:31 echicken Exp $
 *
 * WEBSITE:
 *  http://art.poorcoding.com/fatfish
 *
 * COPYRIGHT:
 *  No part of FatFish may be used modified without consent from the author.
 *
 * AUTHOR:
 *  Art, at Fatcats BBS, dot com.
 */

/* Path to FatFish. */
var PATH_FATFISH = js.exec_dir;
// Note: the above doesn't work when calling within another JS program. :|
// If you are calling it from within JS, uncomment the override below and
// set the appropriate path.
//PATH_FATFISH = "/sbbs/xtrn/fatfish/"

/* Set to true for slow processor or low memory. Restricts lake size to 80x25 maximum. */
var SHITTY_BOX = false;

/* Set to true to enable JSON -- recommended by author. */
var USING_JSON = true;

/* Set to true to use Global High Scores server (FatCats BBS) -- recommended by author. */
var USING_GLOBAL_SERVER = false;

/* Default: 1.
     Set to 0 if you see black lines render on the map.
	 We will fallback to an uglier, safer render method. :|
	 */
var RENDER_MODE = 1;

load("sbbsdefs.js");
load("event-timer.js");
load("json-client.js");
load("mapgenerator.js");
load("frame.js");
load(PATH_FATFISH + "ANSI.js");
load(PATH_FATFISH + "cast.js");
load(PATH_FATFISH + "Fish.js");
load(PATH_FATFISH + "Rod.js");
load(PATH_FATFISH + "safe_pause.js");
load(PATH_FATFISH + "shop.js");

console.write(ANSI.DEFAULT);
console.clear();

/* Version. */
var FF_VER = "0.6b";

/* Set InterBBS Server. */
if (USING_JSON) {
    try {
        var serverAddr;
        var serverPort;

        if (!USING_GLOBAL_SERVER) {
            var server_file = new File(file_cfgname(PATH_FATFISH,"server.ini"));
            server_file.open('r',true);
            serverAddr=server_file.iniGetValue(null,"host","localhost");
            serverPort=server_file.iniGetValue(null,"port",10088);
            server_file.close();
        } else {
            /* Use global server if enabled. */
            serverAddr = "RomulusBBS.com";
            serverPort = 10088;
        }

        /* JSON client object. */
        var json = new JSONClient(serverAddr, serverPort);
    } catch (e) {
        //console.writeln("Exception: " + e.message);
        //console.pause();
    }
}



var lake_x = console.screen_columns;
var lake_y = console.screen_rows - 1;

if (SHITTY_BOX) {
    /* Restrict terminal size, for low-end machines (Raspberry Pi). */
    if (lake_x > 80)
        lake_x = 80;

    if (lake_y > 25)
        lake_y = 25;
}

/* top-level frame */
var frame = new Frame();
/* lake data frame */
var lakeFrame = new Frame(1, 1, lake_x, lake_y, undefined, frame);
/* status bar frame */
var statusFrame = new Frame(1, lake_y + 1, lake_x, 1, undefined, frame);
/* boat frame */
var boatFrame = new Frame(1, 1, 1, 1, undefined, frame);
/* fish finder frame */
var fishFrame = new Frame(1, 1, lake_x, lake_y, undefined, frame);
/* set fish frame to transparent */
fishFrame.transparent = true;
/* pop-up frame */
var popupFrame = new Frame(1, 1, lake_x, lake_y, undefined, frame);
popupFrame.transparent = true;
/* Shop frame */
var shopFrame = new Frame(1, 1, parseInt(lake_x / 2), parseInt(lake_y / 2), undefined, frame);
//shopFrame.transparent = true;
shopFrame.clear(BG_LIGHTGRAY);

/* start this shit up */
console.writeln("Starting FatFish:");

/* Smooth water rendering?
     Set to false if you see black lines render on the map.
	 We will fallback to an uglier, safer render method. :|
	 */
var smoothing;

/* Smooth land rendering? 
     Set to false if you see black lines render on the map.
	 We will fallback to an uglier, safer render method. :|
	 */
var smoothing_land;

switch (RENDER_MODE) {
	case 0:
		smoothing = false;
		smoothing_land = false;
		break;
	case 1:
		smoothing = true;
		smoothing_land = true;
		break;
}

/* Boat object */
var boat = { x: 0, y: 0 };

console.writeln("\t- Generating random Lake.");

/* mapgenerator.js code. */
var map = initMap();
/* Convert the map to a Lake. */
var lake = translateMapToLake(map);

/* Fish count varies based on terminal size. */
console.writeln("\t- Adding Fish.");
var fish_count = Math.round((lake_x * lake_y) / 20);
//fish_count = 10;

/* Load Rods. */
console.writeln("\t- Adding Rods.");
add_rods(); /* Add all Rods to the rods array */
var rod_equipped = rods[0];  /* Player's currently equipped rod */

var wait_time = 100;

if (SHITTY_BOX) {
    // Restrict fishes, for low-end machines.
    if (fish_count > 50) {
        fish_count = 50;
    }

    // inkey() time.
    wait_time = 2500;
}

/* fish variables */
var fishes = new Array();
var fish_caught = new Array();
var timer_fish = new Timer();
var timer_game = new Timer();
var fish_finder_on = false;
var fish_finder_locked = false;
// Deprecated?
var redraw_fish_finder = false;


var is_fatfishing = true;

/* Array to store all located fish. We only draw one per location. */
var fish_map = {};
initFish();

var best = {

    // Holds best Fish of each species.
    bass: undefined,
    eel: undefined,
    pike: undefined,
    trout: undefined,

    bassCurrent: undefined,
    eelCurrent: undefined,
    pikeCurrent: undefined,
    troutCurrent: undefined
}

// Status Bar
var bar_mode = "boat";
var old_bar_mode = undefined;
var bar_msg_persist = undefined;

/* Pop-up messages */
var popup_result = "";

// Fish finder points.
var fish_finder_points = 3;

// Player credits.
var player_credits = 0;

// Load personal scores from JSON: fatfish.QWKID.username.best_bass, etc.
if (USING_JSON) {
    console.writeln("\t- Loading data from InterBBS database:");
    try {

        if (json == undefined) {
            /* If no json object, try it again. */
            json = new JSONClient(serverAddr, serverPort);
        }

        console.write("\t\t- Loading your fish...");

        //log("JSON path is: " + system.qwk_id + "." + user.alias);
        var j1 = json.read("fatfish", system.qwk_id + "." + user.alias + ".best_bass", 1);
        if (j1 != undefined) {
            //log("JSON Bass: " + j1.toSource());
            best.bass = j1;
        }

        var j2 = json.read("fatfish", system.qwk_id + "." + user.alias + ".best_pike", 1);
        if (j2 != undefined) {
            //log("JSON Pike: " + j2.toSource());
            best.pike = j2;
        }

        var j3 = json.read("fatfish", system.qwk_id + "." + user.alias + ".best_trout", 1);
        if (j3 != undefined) {
            //log("JSON Trout: " + j3.toSource());
            best.trout = j3;
        }

        var j4 = json.read("fatfish", system.qwk_id + "." + user.alias + ".best_eel", 1);
        if (j4 != undefined) {
            //log("JSON Trout: " + j3.toSource());
            best.eel = j4;
        }

        console.writeln(" OK.");

        /* Load Player's credits from JSON. */
        console.write("\t\t- Loading your credits...");
        var c1 = json.read("fatfish", system.qwk_id + "." + user.alias + ".credits", 1);
        if (c1 != undefined) {
            player_credits = c1;
        }
        console.writeln(" OK.");

        /* Load Rods owned by player from JSON. */
        console.write("\t\t- Loading your Fishing Rods...");
        for (var ri = 1; ri < rods.length; ri++) {

            var r1 = json.read("fatfish", system.qwk_id + "." + user.alias + ".rods_owned." + ri, 1);
            if (r1 != undefined && r1 == true) {
                rods[ri].owned = true;
            }

        }
        console.writeln(" OK.");

    } catch (e) {
        console.writeln(" ERROR! (;_;)");
        console.writeln("\t\t- InterBBS functions temporarily disabled for this session. :(");

        //log("EXCEPTION: json.read(): " + e.toSource());
        log("Warning: unable to load personal scores from InterBBS database. The server may be temporarily unreachable.");

        /* Disable JSON for rest of session. */
        USING_JSON = false;

        console.pause();
    }
}

//
// Game code.
//
console.writeln("\t- Entering game engine.");

// Title screen.
console.clear();
console.printfile(PATH_FATFISH + "title.ans");
console.line_counter = 0;
console.writeln("\tv" + FF_VER);
console.crlf();
console.pause();

/* open main (parent level) frame */
frame.open();

/* put frames in the correct order */
statusFrame.top();
lakeFrame.top();
fishFrame.top();
boatFrame.top();
popupFrame.top();

/* draw some shit */
loadFish();
renderLake();
renderBoat();

if (SHITTY_BOX) {
    renderBar("Ahoy! Welcome to FatFish! Hit '?' for help.");
} else {
    renderBar();
    /* Welcome pop-up */
    popupFrame.open();
    popupFrame.gotoxy(4, lake_y - 2);
    popupFrame.putmsg(" Ahoy! Welcome to FatFish! ", HIGH | YELLOW | BG_MAGENTA);
    popupFrame.gotoxy(4, lake_y - 1);
    popupFrame.putmsg("     Hit '?' for help.     ", HIGH | YELLOW | BG_MAGENTA);
    popupFrame.draw();
    timer_game.addEvent(10000, false, event_popup_clear);
}

//frame.invalidate();

while (is_fatfishing) {
    timer_fish.cycle();
    timer_game.cycle();
    frame.cycle();

    renderPopup();

    console.aborted = false;
    c = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);
    switch (c.toLowerCase()) {
        case "c":
            // Cast.
            // Presently at boat's location.
            // TODO: Cast based on Rod's properties.
            if (!fish_finder_on) {
                redraw_cast = true;
                castAt(boat.x, boat.y);
                frame.invalidate();
            } else {
                // Can't cast while fish finder is on.
                log("Can't cast while fish finder is on.");
                renderBar("Can't cast when fish finder is on.");
            }
            break;

        case "f":
            if (fish_finder_points > 0) {
                if (fish_finder_locked) {
                    renderBar("Fish finder is already on.");
                } else {
                    if (old_bar_mode == "finder") {
                        old_bar_mode = "boat";
                    } else {
                        old_bar_mode = bar_mode;
                    }
                    bar_mode = "finder";

                    showFish();
                    timer_fish.addEvent(10000, false, event_fish_clear);

                    fish_finder_points--;
                    log(fish_finder_points + " fish finder points left.");
                    renderBar("Fish finder used: " + fish_finder_points + " points left.");

                    fish_finder_locked = true;

					// TODO: avoid full-screen functions until lock is done.
                }
            } else {
                renderBar("No fish finder points available. Woe is u.");
            }
            break;

        case "i":
            /* Display manual */
            var help_file = PATH_FATFISH + "README.txt";
            
            console.writeln(ANSI.DEFAULT);
            console.home();
            console.clear();            

            console.line_counter = 0;
            console.printfile(help_file);
            console.line_counter = 0;
            console.pause();
            console.clear();
            console.write(ANSI.DEFAULT);

            frame.invalidate();
            frame.draw();
            statusFrame.invalidate();
            statusFrame.draw();

            break;

        case "r":
            // Pick rod.

            event_reset_bar();

            var is_rodding = true;
            var cmd_rod;
            var redraw_rodding = true;
            var rod_idx = rods.indexOf(rod_equipped);
            var rod_col = ANSI.BOLD + ANSI.FG_BLACK;

            while (is_rodding) {
                timer_fish.cycle();
                timer_game.cycle();
                frame.cycle();

                if (redraw_rodding) {
                    /* Redraw rod selection statusbar */

                    if (rods[rod_idx].owned) {
                        rod_col = ANSI.BOLD + ANSI.FG_WHITE;
                    } else {
                        rod_col = ANSI.BOLD + ANSI.FG_BLACK;
                    }

                    console.gotoxy(1, lake_y + 1);
                    console.cleartoeol(HIGH | YELLOW | BG_BROWN);
                    console.write(ANSI.BOLD + ANSI.FG_YELLOW + " Left/right: " + rod_col + rods[rod_idx].name + ANSI.FG_BLACK + " (wgt:" + rods[rod_idx].weight + "kg, line:" + rods[rod_idx].max_line_distance + "m, ten:" + rods[rod_idx].max_line_tension + "kg)");
                    redraw_rodding = false;
                }

                cmd_rod = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);

                /* Parse commands */
                switch (cmd_rod.toLowerCase()) {
                    case KEY_LEFT:
                        if (rod_idx > 0) {
                            rod_idx--;
                        } else {
                            rod_idx = rods.length - 1;
                        }

                        redraw_rodding = true;
                        break;

                    case KEY_RIGHT:
                        if (rod_idx < rods.length - 1) {
                            rod_idx++;
                        } else {
                            rod_idx = 0;
                        }

                        redraw_rodding = true;
                        break;

                    case "q":
                        is_rodding = false;
                        break;

                    case "\r":
                        if (rods[rod_idx] != undefined && rods[rod_idx].owned) {
                            rod_equipped = rods[rod_idx];
                            renderBar(rods[rod_idx].name + " rod equipped.");
                        } else {
                            renderBar("Error: I don't own a " + rods[rod_idx].name + " yet.");
                        }

                        is_rodding = false;
                        break;
                }
            }

            statusFrame.invalidate();
            renderBar();

            break;

        case "q":
            is_fatfishing = false;
            console.write(ANSI.DEFAULT);
            console.clear();
            break;

        case "s":
            if (USING_JSON) {
                // Show all scores
                show_json_scores();
                frame.invalidate();
            }
            break;

        case "t":
            // Tackle shop.
            if (lake.map[boat.y][boat.x].depth >= -0.25) {
                shop();
            } else {
                renderBar("You need to be in shallower water.");
            }
            break;

        case "?":
        case "h":
            // Display help.
            console.writeln(ANSI.DEFAULT);
            console.clear();
            console.printfile(PATH_FATFISH + "help.ans");
            console.line_counter = 0;
            console.crlf();
            console.pause();
            frame.invalidate();
            renderBar("Fish finder: " + fish_finder_points + " points left.");
            popup_result = "Fish finder: " + fish_finder_points + " points left.";
            break;

        case "$":
            // Show score.
            show_fish_caught();
            frame.invalidate();
            break;

        case KEY_DOWN:
            if (lake.map[boat.y + 1] != undefined
				&& lake.map[boat.y + 1][boat.x].isWater()) {
                boat.y++;
                boatFrame.move(0, 1);
                renderBar();
                renderBoat();
            }
            break;

        case KEY_UP:
            if (boat.y - 1 >= 0 &&
                lake.map[boat.y - 1][boat.x].isWater()) {
                boat.y--;
                boatFrame.move(0, -1);
                renderBar();
                renderBoat();
            }
            break;

        case KEY_LEFT:
            if ((boat.x - 1) >= 0 && lake.map[boat.y][boat.x - 1].isWater()) {
                boat.x--;
                boatFrame.move(-1, 0);
                renderBar();
                renderBoat();
            }
            break;

        case KEY_RIGHT:
            if (lake.map[boat.y] != undefined
				&& lake.map[boat.y][boat.x + 1] != undefined
                && lake.map[boat.y][boat.x + 1].isWater()) {
                boat.x++;
                boatFrame.move(1, 0);
                renderBar();
                renderBoat();
            }
            break;

        default:
            break;
    }
}

/* map rendering functions */
function drawFish(x, y) {
    var attr = BG_BLUE;
    var fish = fish_map[x][y][0];
    if (lake.map[fish.y][fish.x].depth >= -1)
        attr = BG_CYAN;
    fishFrame.setData(x, y, fish.texture, fish.attr | attr);
}

function unDrawFish(x, y) {
    fishFrame.clearData(x, y);
}

function renderBar(msg) {
    var attr;
    switch (bar_mode) {
        case "cast":
            attr = BG_CYAN | LIGHTCYAN;
            break;
        case "duel":
            attr = BG_BLUE | LIGHTBLUE;
            break;
        case "finder":
            attr = BG_RED | LIGHTRED;
            break;
        case "shop":
            attr = BG_GREEN | LIGHTGREEN;
            break;
        default:
            attr = BG_MAGENTA | LIGHTMAGENTA;
            break;
    }

    var depth = lake.map[boat.y][boat.x].depth;
    statusFrame.clearline(attr);
    statusFrame.gotoxy(1, 1);
    statusFrame.putmsg(" " + depthToMetres(depth) + "m. ", attr);

    if (msg != undefined && msg.length > 0) {
        //log("New status bar message: " + msg);

        // Add timer.
        timer_fish.addEvent(10000, false, event_reset_bar); // Status bar reset event.

        bar_msg_persist = msg;

        statusFrame.gotoxy(30, 1);
        statusFrame.putmsg(bar_msg_persist);

        //} else if (msg !=undefined && msg.length == 0) {
        //log("Status reset received.");
    } else if (msg == undefined) {
            //log("Status undefined received.");

            // Redraw persisting status.
        statusFrame.gotoxy(30, 1);
        statusFrame.putmsg(bar_msg_persist);
    }
}

function renderDepth() {
    var attr;
    switch (bar_mode) {
        case "cast":
            attr = BG_CYAN | LIGHTCYAN;
            break;
        case "duel":
            attr = BG_BLUE | LIGHTBLUE;
            break;
        case "finder":
            attr = BG_RED | LIGHTRED;
            break;
        case "shop":
            attr = BG_GREEN | LIGHTGREEN;
            break;
        default:
            attr = BG_MAGENTA | LIGHTMAGENTA;
            break;
    }

    var depth = lake.map[boat.y][boat.x].depth;
    statusFrame.clearline(attr);
    statusFrame.gotoxy(2, 1);
    statusFrame.putmsg(depthToMetres(depth) + "m. ", attr);
}

function renderBoat() {
    // Get water depth.
    var depth = lake.map[boat.y][boat.x].depth;
    // Colour.
    var attr = BG_CYAN;
    switch (Math.floor(depth)) {
        case -4:
        case -3:
        case -2:
            attr = BG_BLUE;
            break;
        default:
            break;
    }
    boatFrame.setData(0, 0, "B", attr | YELLOW);
}

function loadFish() {
    for (var a = 0; a < fishes.length; a++) {
        var rfish = fishes[a];

        if (!fish_map[rfish.x])
            fish_map[rfish.x] = {};
        if (!fish_map[rfish.x][rfish.y])
            fish_map[rfish.x][rfish.y] = [];
        fish_map[rfish.x][rfish.y].push(rfish);
    }
}

function showFish() {
    for (var x in fish_map) {
        for (var y in fish_map[x]) {
            /* break after drawing the first fish at this location */
            for (var f = 0; f < fish_map[x][y].length; f++) {
                var fish = fish_map[x][y][f];
                /* if this fish is no longer in this location,
				remove it from this location's fish stack */
                if (fish.x != x || fish.y != y) {
                    fish_map[x][y].splice(f--, 1);
                }
            }
            /* if this location is now empty, 
			clear it */
            if (fish_map[x][y].length == 0) {
                delete fish_map[x][y];
                unDrawFish(x, y);
            }
                /* otherwise draw the topmost fish */
            else {
                drawFish(x, y);
            }
        }
    }
}

function hideFish() {
    for (var x in fish_map) {
        for (var y in fish_map[x]) {
            unDrawFish(x, y);
        }
    }
}

function renderLake() {
    for (var i = 0; i < lake.map.length; i++) {
        // Draw each line.
        for (var j = 0; j < lake.map[i].length; j++) {
            var ch;
            var attr;
            // Add each column into a string.
            ch = lake.map[i][j].texture;
            attr = BG_BLACK | LIGHTGRAY;
            lakeFrame.setData(j, i, ch, attr);
        }
        console.line_counter = 0;
    }
}

function renderPopup() {
    //log("popup_result: " + popup_result);
    if (popup_result.length > 0) {
        //log("popup_result: " + popup_result);

        popupFrame.clear();
        popupFrame.gotoxy(4, lake_y - 1);
        popupFrame.putmsg(" " + popup_result + " ", HIGH | YELLOW | BG_MAGENTA);
        popupFrame.open();

        timer_game.addEvent(7000, false, event_popup_clear);

        // Reset string.
        popup_result = "";
    }
}

/* initialization */
function initFish() {

    for (var tmp_fc = 0; tmp_fc < fish_count; tmp_fc++) {
        var tmp_f = get_random_fish();

        tmp_f.id = tmp_fc + 1;

        //log("Fish = type: " + tmp_f.type + ".  weight: " + tmp_f.weight + "kg.  length: " + tmp_f.length + "mm.");
        fishes.push(tmp_f);
    }

    //log("Total fish: " + fishes.length);

    // Fish movement timer.
    var ms = 1000;

    if (SHITTY_BOX) {
        // Restrict fish movement timer, for low-end machines.
        ms = 10000;
    }
    timer_fish.addEvent(ms, true, event_fish_move); // Fish movement event.

}

function initMap() {
    var map = new Map(lake_y, lake_x);
    map.base = -2; // -2
    map.peak = 5; // 5
    map.minRadius = 10;
    map.maxRadius = 15;
    var r_hills = 50 + parseInt(console.screen_rows * console.screen_columns / 100) + random(200);
    /* Give these hillz some flava. */
    map.hills = r_hills
    map.island = true;
    map.lake = true;
    map.border = true;
    map.randomize();
    return map;
}

function translateMapToLake(map) {
    lake = new Lake();
    lake.initialize(map);
    return lake;
}

/* Lake object */
function Lake() {
    this.map = new Array();

    this.initialize = function (map) {
        h = map.length;
        w = map[0].length;

        // Create a 2D Array to represent world.
        this.map = new Array(h);
        for (var a = 0; a < h; a++) {
            this.map[a] = new Array(w);
        }

        // Iterate through each block of the map. Insert Terrain Object.
        for (var i = 0; i < this.map.length; i++) {

            for (var j = 0; j < this.map[i].length; j++) {
                // Read value from generator.
                v1 = map[i][j];

                // Create Terrain object.
                t1 = new Terrain(v1);


                this.map[i][j] = t1;
            }   // end for()
        }   // end for()

        //
        // TODO: Create dock.
        //

        // Dock boat.
        // From N E, find the first place where water has all water neighbours.
        var found_boat = false;
        for (var b = 1; b < this.map.length; b++) {
            for (var c = 1; c < this.map[b].length; c++) {
                if (!found_boat) {
                    if (this.map[b][c].isWater() && this.map[b][c].depth <= 1.0) {
                        // This Terrain block is water.

                        // Are neighbours water?
                        if (this.map[b - 1][c - 1].isWater()) {
                            boat.x = c;
                            boat.y = b;

                            boatFrame.moveTo(c + 1, b + 1); // c,b => x,y offset is +1.
                            found_boat = true;
                        }
                    }
                }
            }
        }

    }   // end initialize()
}   // end Lake()

function Terrain(d) {
    this.depth = d;
    this.texture = " ";

    // Enhance negative depth.
    if (this.depth <= 1.0) {
        this.depth = this.depth - 1;

    }

    // Generate some random land textures.
    switch (Math.floor(this.depth)) {
        case -10:
        case -9:
        case -8:
        case -7:
        case -6:
        case -5:
        case -4:
        case -3:
        case -2:
            // Deeper  water.
            this.texture = ANSI.BG_BLUE + " ";

            if (smoothing) {
                rm2 = random(3);
                switch (rm2) {
                    // BG strong < \xB0\xB1\xB2\xDB > FG strong
                    case 0:
                        if (Math.floor(this.depth) <= -3.0) {
                            this.texture = ANSI.UNBOLD + ANSI.FG_BLACK + ANSI.BG_BLUE + "\xB0";
                        }
                        break;
                        
                    case 1:
                        if (Math.floor(this.depth) > -1.5) {
                            this.texture = ANSI.BOLD + ANSI.FG_CYAN + ANSI.BG_BLUE + "\xB0";
                        } else {
                            this.texture = ANSI.BOLD + ANSI.FG_BLUE + ANSI.BG_BLUE + "\xB0";
                        }
                        break;
                    case 2:
                        if (Math.floor(this.depth) <= -2.5) {
                            this.texture = ANSI.UNBOLD + ANSI.FG_BLACK + ANSI.BG_BLUE + "\xB0";
                        } else {
                            this.texture = ANSI.UNBOLD + ANSI.FG_CYAN + ANSI.BG_BLUE + "\xB0";
                        }
                        break;
                }
            }

            break;

        case -1:
            // Deep water.
            this.texture = ANSI.BG_CYAN + " ";

            if (smoothing) {
                rm1 = random(5);

                switch (rm1) {
                    case 0:
                        this.texture = ANSI.UNBOLD + ANSI.FG_CYAN + ANSI.BG_BLUE + "\xB2";
                        break;
                    case 1:
                    	this.texture = ANSI.UNBOLD + ANSI.FG_CYAN + ANSI.BG_BLUE + "\xB1";
                        break;
                    case 2:
                        this.texture = ANSI.UNBOLD + ANSI.FG_CYAN + ANSI.BG_BLUE + "\xB0";                        
                        break;
                    case 3:
                        if (this.depth > -0.5) {
                            this.texture = ANSI.FG_GREEN + ANSI.BG_BLUE + "\xB1";
                        } else {                            
                            this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_BLUE + "\xB0";
                        }
                        break;
                    case 4:
                        // Definitely causes issues on some BBSes.
                        this.texture = ANSI.BOLD + ANSI.FG_CYAN + ANSI.BG_BLUE + "\xB0";
                        break;
                }
            }

            break;

        case 0:
            // Water.
            this.texture = ANSI.BG_CYAN + " ";

            if (smoothing) {

                r0 = random(6);

                switch (r0) {
                    // BG strong < \xB0\xB1\xB2\xDB > FG strong
                    case 0:
                        this.texture = ANSI.BOLD + ANSI.FG_CYAN + ANSI.BG_CYAN + "\xB0";
                        break;
                    case 1:
                    	this.texture = ANSI.BOLD + ANSI.FG_CYAN + ANSI.BG_CYAN + "\xB1";
                        break;
                    case 2:
                        this.texture = ANSI.UNBOLD + ANSI.FG_BLUE + ANSI.BG_CYAN + "\xB0";
                        break;
                	case 3:
                		this.texture = ANSI.UNBOLD + ANSI.FG_BLUE + ANSI.BG_CYAN + "\xB1";
                        break;
                    case 4:
                        this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_CYAN + "\xB0";
                        break;
                    case 5:
                    	this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_CYAN + "\xB1";
                        break;
                }
            }

            break;

        case 1:
            // Sand.
        	this.texture = ANSI.BG_YELLOW + " ";
            
            if (smoothing_land) {
                r1 = random(3);

                switch (r1) {
                    case 0:
                        this.texture = ANSI.BOLD + ANSI.FG_YELLOW + ANSI.BG_YELLOW + "\xB2";
                        break;
                    case 1:
                    	this.texture = ANSI.BOLD + ANSI.FG_YELLOW + ANSI.BG_YELLOW + "\xB1";
                        break;
                    case 2:
                        this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_YELLOW + "\xB1";
                        break;
                }
            }
            
            break;

        case 2:
            // Grassland.
            this.texture = ANSI.BG_GREEN + " ";
            
            c = " ";
            
            if (smoothing_land) {
                r2 = random(3);

                switch (r2) {
                    case 0:
                        this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_GREEN + "\xB0";
                        break;
                    case 1:
                        this.texture = ANSI.BOLD + ANSI.FG_YELLOW + ANSI.BG_GREEN + "\xB0";;
                        break;
                    case 2:
                        this.texture = ANSI.UNBOLD + ANSI.FG_YELLOW + ANSI.BG_GREEN + "\xB0";
                        break;
                }
            }
            


            break;

            //default:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        default:

            // Depth > 2
            // Forest            
        	var tex = '^';
        	r3 = random(3);
        	switch (r3) {
        		case 0:
        			this.texture = ANSI.FG_YELLOW + ANSI.BG_GREEN + tex;
        			break;
        		case 1:
        			this.texture = ANSI.FG_WHITE + ANSI.BG_GREEN + tex;
        			break;
        		case 2:
        			this.texture = ANSI.FG_YELLOW + ANSI.BG_GREEN + ';';
        			break;
        	}
            
            
            

            if (smoothing_land) {
            	//c = " ";
                r3 = random(3);
                switch (r3) {
                    case 0:
                        this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_GREEN + ";";
                        break;
                    case 1:
                        this.texture = ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_GREEN + tex;
                        break;
                    case 2:
                        this.texture = ANSI.UNBOLD + ANSI.FG_YELLOW + ANSI.BG_GREEN + tex;
                        break;
                }
            } // end if(smoothing_land)
            
            break;
    }   // End switch

    this.isWater = function () {
        //log("Depth: " + this.depth + " -vs- 0: " + (this.depth <= 0));
        if (this.depth <= 0.25) {
            return true;
        } else {
            return false;
        }
    }

    this.isWaterFish = function () {
        //log("isWaterFish(): " + Math.floor(this.depth));
        if (this.depth <= 0) {
            return true;
        } else {
            return false;
        }
    }

    // TODO: Return an array of all surrounding Terrain objects from lake.map[][].
    // I.e., radius = 1, will return:
    //
    //  x-r, y-r
    //      [0] [1] [2]
    //      [3]  X  [4]
    //      [5] [6] [7]
    //              x+r, y+r
    //
    // I.e., radius = 2, will return:
    //      [0 ][1 ][2 ][3 ][4 ]
    //      [3 ][4 ][5 ][6 ][7 ]
    //      [8 ][9 ] XX [10][11]
    //      [12][13][14][15][16]
    //      [18][19][20][21][22]
    this.getSurroundings = function (radius) {
    }
}

function fishAt(x, y) {
    fishes_found = new Array();

    for (var a = 0; a < fishes.length; a++) {
        if (fishes[a].x == x && fishes[a].y == y) {
            fishes_found.push(fishes[a]);
        }
    }

    return fishes_found;
}

function event_fish_clear() {
    hideFish();
    fish_finder_on = false;
    fish_finder_locked = false;
    bar_mode = old_bar_mode;
    statusFrame.invalidate();
}

function event_fish_move() {
    if (fishes.length > 0) {
        for (var a = 0; a < fishes.length; a++) {
            if (fishes[a].is_near_lure()) {
                // Check if fish should move towards lure nearby.
                if (fishes[a].like_lure()) {
                    // If Fish likes lure, move towards it.
                    fishes[a].move_towards(boat.x, boat.y);
                } else {
                    fishes[a].move_random();
                }
            } else {
                // Otherwise, move quasi-randomly.
                if (SHITTY_BOX) {
                    var v = random(10);

                    switch (v) {
                        case 0:
                            // Reduce the load by only moving 1/random(x) Fish.
                            fishes[a].move_random();
                            break;
                    }
                } else {
                    fishes[a].move_random();
                }
            }
        }

        /* if the fish finder is enabled, update screen positions */
        if (fish_finder_on) {
            showFish();
        }
    }
}

function event_reset_bar() {
    //log("event_reset_bar()");
    renderBar("");
    bar_msg_persist = undefined;
}

// From: http://stackoverflow.com/questions/196972/convert-string-to-proper-case-with-javascript/196991#196991
function toTitleCase(str) {
    return str.replace(/\w\S*/g, function (txt) { return txt.charAt(0).toUpperCase() + txt.substr(1).toLowerCase(); });
}

function depthToMetres(d) {
    return (d * 10).toFixed(2);
}

function update_hi_scores(fish) {
    var scores_need_saving = false;

    if (fish != undefined) {
        //log("update_hi_scores(): fish length: " + fish.length);

        // Check best fish.
        switch (fish.type) {
            case FISH_TYPE_BASS:
                /* Display fish ansi */
                console.clear();
                console.printfile(PATH_FATFISH + "bass.ans", P_NOCRLF | P_NOPAUSE);
                console.line_counter = 0;
                console.gotoxy(60, 20);
                safe_pause();
                console.clear();


                // Hi score this game.
                if (best.bassCurrent == undefined || Number(fish.length) > Number(best.bassCurrent.length)) {
                    renderBar("Best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.bassCurrent = fish;
                }

                // All-time hi score.
                if (best.bass == undefined || Number(fish.length) > Number(best.bass.length)) {
                    /* Display ansi */
                    console.clear();
                    console.printfile(PATH_FATFISH + "record-personal.ans", P_NOCRLF | P_NOPAUSE);
                    console.gotoxy(44, 8);
                    console.write(ANSI.BOLD + ANSI.FG_BLACK + ANSI.BG_WHITE + "bass." + ANSI.DEFAULT);
                    console.gotoxy(1, 20);
                    safe_pause();

                    log("New record Bass!")
                    renderBar("New record Bass! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: new personal record " + toTitleCase(fish.type) + "! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.bass = fish;
                    scores_need_saving = true;
                }

                break;

            case FISH_TYPE_EEL:
                /* Display fish ansi */
                console.clear();
                console.printfile(PATH_FATFISH + "eel.ans", P_NOCRLF | P_NOPAUSE);
                console.line_counter = 0;
                console.gotoxy(60, 20);
                safe_pause();
                console.clear();

                // Hi score this game.
                if (best.eelCurrent == undefined || Number(fish.length) > Number(best.eelCurrent.length)) {
                    renderBar("Best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.eelCurrent = fish;
                }

                // All-time hi score.
                if (best.eel == undefined || Number(fish.length) > Number(best.eel.length)) {

                    /* Display ansi */
                    console.clear();
                    console.printfile(PATH_FATFISH + "record-personal.ans", P_NOCRLF | P_NOPAUSE);
                    console.gotoxy(44, 8);
                    console.write(ANSI.BOLD + ANSI.FG_BLACK + ANSI.BG_WHITE + "eel." + ANSI.DEFAULT);
                    console.gotoxy(1, 20);
                    safe_pause();

                    log("New record Eel!");
                    renderBar("New record Eel! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: new personal record " + toTitleCase(fish.type) + "! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.eel = fish;
                    scores_need_saving = true;
                }

                break;

            case FISH_TYPE_PIKE:
                // Hi score this game.
                if (best.pikeCurrent == undefined || Number(fish.length) > Number(best.pikeCurrent.length)) {
                    renderBar("Best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.pikeCurrent = fish;
                }

                // All-time hi score.
                if (best.pike == undefined || Number(fish.length) > Number(best.pike.length)) {
                    /* Display ansi */
                    console.clear();
                    console.printfile(PATH_FATFISH + "record-personal.ans", P_NOCRLF | P_NOPAUSE);
                    console.gotoxy(44, 8);
                    console.write(ANSI.BOLD + ANSI.FG_BLACK + ANSI.BG_WHITE + "pike." + ANSI.DEFAULT);
                    console.gotoxy(1, 20);
                    safe_pause();

                    log("New record Pike!")
                    renderBar("New record " + toTitleCase(fish_biting.type) + "! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: new personal record " + toTitleCase(fish.type) + "! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.pike = fish;
                    scores_need_saving = true;
                }

                break;

            case FISH_TYPE_TROUT:

                /* Display fish ansi */
                console.clear();
                console.printfile(PATH_FATFISH + "trout.ans", P_NOCRLF | P_NOPAUSE);
                console.line_counter = 0;
                console.gotoxy(60, 20);
                safe_pause();
                console.clear();

                // Hi score this game.
                if (best.troutCurrent == undefined || Number(fish.length) > Number(best.troutCurrent.length)) {
                    renderBar("Best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: best " + toTitleCase(fish.type) + " this game! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.troutCurrent = fish;
                }

                // All-time hi score.
                if (best.trout == undefined || Number(fish.length) > Number(best.trout.length)) {
                    /* Display ansi */
                    console.clear();
                    console.printfile(PATH_FATFISH + "record-personal.ans", P_NOCRLF | P_NOPAUSE);
                    console.gotoxy(44, 8);
                    console.write(ANSI.BOLD + ANSI.FG_BLACK + ANSI.BG_WHITE + "trout." + ANSI.DEFAULT);
                    console.gotoxy(1, 20);
                    safe_pause();

                    log("New record Trout!")
                    renderBar("New record " + toTitleCase(fish_biting.type) + "! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.");
                    popup_result = "You caught a fish: new personal record " + toTitleCase(fish.type) + "! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";
                    best.trout = fish;
                    scores_need_saving = true;
                }

                break;
        }

    }

    if (scores_need_saving) {
        if (USING_JSON) {
            // Save personal scores to JSON: fatfish.QWKID.username.best_bass, etc.
            try {
                if (user.alias.length > 0) {
                    // TODO: Only save the new best.FISHTYPE.
                    if (json == undefined) {
                        /* If no json object, try it again. */
                        json = new JSONClient(serverAddr, serverPort);
                    }

                    if (best.bass != undefined)
                        json.write("fatfish", system.qwk_id + "." + user.alias + ".best_bass", best.bass, 2);
                    if (best.eel != undefined)
                        json.write("fatfish", system.qwk_id + "." + user.alias + ".best_eel", best.eel, 2);
                    if (best.pike != undefined)
                        json.write("fatfish", system.qwk_id + "." + user.alias + ".best_pike", best.pike, 2);
                    if (best.trout != undefined)
                        json.write("fatfish", system.qwk_id + "." + user.alias + ".best_trout", best.trout, 2);
                }
            } catch (e) {
                //log("EXCEPTION: json.write(): " + e.Message);
                log("Warning: unable to save personal scores to the InterBBS database. The server may be temporarily unreachable.");

                /* Disable JSON for rest of session. */
                USING_JSON = false;
            }
        }

    } // end if (scores_need_saving).
}

function show_fish_caught() {
    console.aborted = false;

    console.clear();
    console.writeln(ANSI.BOLD + ANSI.FG_WHITE + "Fish caught:" + ANSI.DEFAULT);
    console.crlf();
    console.line_counter = 2;
    if (fish_caught.length == 0) {
        console.writeln("\tNone.");
        console.line_counter++;
    } else {
        for (var a = 0; a < fish_caught.length; a++) {
            var col = ANSI.FG_CYAN;
            if (fish_caught[a] == best.bassCurrent || fish_caught[a] == best.eelCurrent || fish_caught[a] == best.pikeCurrent || fish_caught[a] == best.troutCurrent) {
                col = ANSI.BOLD + ANSI.FG_CYAN;
            }

            if (fish_caught[a] == best.bass || fish_caught[a] == best.eel || fish_caught[a] == best.pike || fish_caught[a] == best.trout) {
                col = ANSI.BOLD + ANSI.FG_YELLOW;
            }

            console.writeln("\t" + (a + 1) + ". " + col + toTitleCase(fish_caught[a].type) + ANSI.DEFAULT + ", " + (fish_caught[a].length / 10).toFixed(1) + "cm, " + fish_caught[a].weight + "kg.");
            console.line_counter++;

            if (console.line_counter >= console.screen_rows - 4) {

                console.line_counter = 0;
                console.pause();
            }
        }
    }

    console.crlf(); console.crlf();
    console.line_counter++; console.line_counter++;

    // Show players all-time JSON hi scores.
    console.writeln(ANSI.BOLD + ANSI.FG_WHITE + "Your all-time best catches:" + ANSI.DEFAULT + ANSI.FG_YELLOW);
    console.line_counter++;

    if (best.bass != undefined) {
        console.writeln("\t" + ANSI.BOLD + ANSI.FG_YELLOW + toTitleCase(best.bass.type) + ANSI.DEFAULT + ", " + (best.bass.length / 10).toFixed(1) + "cm, " + best.bass.weight + "kg.");
        console.line_counter++;
    }

    if (best.eel != undefined) {
        console.writeln("\t" + ANSI.BOLD + ANSI.FG_YELLOW + toTitleCase(best.eel.type) + ANSI.DEFAULT + ", " + (best.eel.length / 10).toFixed(1) + "cm, " + best.eel.weight + "kg.");
        console.line_counter++;
    }

    if (best.pike != undefined) {
        console.writeln("\t" + ANSI.BOLD + ANSI.FG_YELLOW + toTitleCase(best.pike.type) + ANSI.DEFAULT + ", " + (best.pike.length / 10).toFixed(1) + "cm, " + best.pike.weight + "kg.");
        console.line_counter++;
    }

    if (best.trout != undefined) {
        console.writeln("\t" + ANSI.BOLD + ANSI.FG_YELLOW + toTitleCase(best.trout.type) + ANSI.DEFAULT + ", " + (best.trout.length / 10).toFixed(1) + "cm, " + best.trout.weight + "kg.");
        console.line_counter++;
    }

    console.line_counter = 0;
    console.write(ANSI.DEFAULT);
    console.crlf();
    console.crlf();

    console.pause();
}

// Display each player's score in JSON.
function show_json_scores() {
    if (USING_JSON) {
        try {
            if (json == undefined) {
                /* If no json object or scores, try it again. */
                json = new JSONClient(serverAddr, serverPort);
            }

            var scores = json.read("fatfish", "", 1);

            if (scores != undefined) {
                var top_bass_length = 0;   // InterBBS top scores.
                var top_eel_length = 0;
                var top_pike_length = 0;
                var top_trout_length = 0;

                console.clear();
                console.write(ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_GREEN + " InterBBS Scores ");
                console.cleartoeol();
                console.writeln(ANSI.DEFAULT);
                console.crlf();

                if (Object.keys(scores).length > 0) {
                    // Iterate through each BBS, showing player scores.
                    var bbses = Object.keys(scores);

                    for (var a = 0; a < bbses.length; a++) {
                        // Each BBS.
                        console.writeln(ANSI.BOLD + ANSI.FG_YELLOW + ANSI.BG_YELLOW + " " + bbses[a] + ": " + ANSI.DEFAULT);

                        var json_players = Object.keys(scores[bbses[a]]);

                        if (json_players != undefined) {

                            for (var b = 0; b < json_players.length; b++) {
                                // Each player.								
								//log("DEBUG: parsing player: " + json_players[b] + "@" + bbses[a]);
								
                                var col = ANSI.BOLD + ANSI.FG_WHITE;
                                if (json_players[b].length <= 6) {
                                    console.write(col + json_players[b] + ":" + ANSI.DEFAULT);
                                } else {
                                    console.writeln(col + json_players[b] + ":" + ANSI.DEFAULT);
                                    console.line_counter++;
                                }

                                if (scores[bbses[a]][json_players[b]].best_bass != undefined) {

                                    // Print.
                                    console.write("\t" + toTitleCase(scores[bbses[a]][json_players[b]].best_bass.type) + ANSI.DEFAULT + ", " + (scores[bbses[a]][json_players[b]].best_bass.length / 10).toFixed(1) + "cm, " + scores[bbses[a]][json_players[b]].best_bass.weight + "kg.");

                                    // Check top InterBBS score.
                                    if (scores[bbses[a]][json_players[b]].best_bass.length > top_bass_length) {
                                        // This is the new top score.
                                        top_bass_length = scores[bbses[a]][json_players[b]].best_bass.length;
                                    }
                                }

                                if (scores[bbses[a]][json_players[b]].best_eel != undefined) {
                                    console.write("\t" + toTitleCase(scores[bbses[a]][json_players[b]].best_eel.type) + ANSI.DEFAULT + ", " + (scores[bbses[a]][json_players[b]].best_eel.length / 10).toFixed(1) + "cm, " + scores[bbses[a]][json_players[b]].best_eel.weight + "kg.");

                                    // Check top InterBBS score.
                                    if (Number(scores[bbses[a]][json_players[b]].best_eel.length) > Number(top_eel_length)) {
                                        // This is the new top score.
                                        top_eel_length = scores[bbses[a]][json_players[b]].best_eel.length;
										//log("DEBUG: found new --> top eel: " + top_eel_length);
                                    }
									//log("DEBUG: player best_eel: " + scores[bbses[a]][json_players[b]].best_eel.length);
                                }

                                if (scores[bbses[a]][json_players[b]].best_pike != undefined) {
                                    console.write("\t" + toTitleCase(scores[bbses[a]][json_players[b]].best_pike.type) + ANSI.DEFAULT + ", " + (scores[bbses[a]][json_players[b]].best_pike.length / 10).toFixed(1) + "cm, " + scores[bbses[a]][json_players[b]].best_pike.weight + "kg.");

                                    // Check top InterBBS score.
                                    if (scores[bbses[a]][json_players[b]].best_pike.length > top_pike_length) {
                                        // This is the new top score.
                                        top_pike_length = scores[bbses[a]][json_players[b]].best_pike.length;
                                    }
                                }

                                if (scores[bbses[a]][json_players[b]].best_trout != undefined) {
                                    write("\t" + toTitleCase(scores[bbses[a]][json_players[b]].best_trout.type) + ANSI.DEFAULT + ", " + (scores[bbses[a]][json_players[b]].best_trout.length / 10).toFixed(1) + "cm, " + scores[bbses[a]][json_players[b]].best_trout.weight + "kg.");

                                    // Check top InterBBS score.
                                    if (scores[bbses[a]][json_players[b]].best_trout.length > top_trout_length) {
                                        // This is the new top score.
                                        top_trout_length = scores[bbses[a]][json_players[b]].best_trout.length;
                                    }
                                }

                                console.crlf();
                                console.line_counter++;
                                if (console.line_counter > (console.screen_rows - 2)) {
                                    console.pause();
                                    console.line_counter = 0;
                                }

                            } // end for Players
                        } // end if

                        console.crlf();
                    } // end for BBSes

                    // Print InterBBS top scores.
                    console.writeln(ANSI.BOLD + ANSI.FG_GREEN + ANSI.BG_GREEN + " Top scores: " + ANSI.DEFAULT);
                    console.writeln(ANSI.BOLD + "\tBass:\t" + top_bass_length / 10 + "cm.");
                    console.writeln("\tEel:\t" + top_eel_length / 10 + "cm.");
                    console.writeln("\tPike:\t" + top_pike_length / 10 + "cm.");
                    console.writeln("\tTrout:\t" + top_trout_length / 10 + "cm." + ANSI.DEFAULT);
                } else {
                    // No scores found in JSON.
                    console.writeln("No scores found.");
                } // end if

            } else {
                // scores undefined.
                console.writeln("No scores found.");
            }

            console.crlf();
            console.pause();
            console.clear();
            redraw_all = true;

        } catch (e) {
            //log("EXCEPTION: show_json_scores():json.read(): " + e.Message);
            log("Warning: unable to load scores from the InterBBS database. The server may be temporarily unreachable.");

            /* Disable JSON for rest of session. */
            USING_JSON = false;
        }
    } // end if (USING_JSON)
}

function render_status_bar(x, y, width, threshold, fgcol, thcol, bgcol) {
    // How many blocks to fill?
    var a = parseInt((x / y) * width);
    var b = 0;
    var c = 0;

    // Check thresholds.
    if ((x / y) > threshold) {
        fgcol = thcol;
    }

    //log("Filling in " + a + " out of " + width + " blocks.");
    console.write(fgcol);
    for (b = 0; b < a; b++) {
        console.write("\xDB");
    }

    console.write(bgcol);
    for (c = 0; c < (width - a) ; c++) {
        console.write(" ");
    }

    console.write(ANSI.DEFAULT);
}

function event_popup_clear() {
    popupFrame.clear();
    popupFrame.close();
}
