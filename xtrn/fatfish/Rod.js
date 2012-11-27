
function Rod() {
    this.weight = -1;
    this.max_line_distance = -1;
    this.max_line_tension = -1;
    this.name = "";
    this.owned = false;
    this.price = 0;
}

/*
 * Fishing Rods
 */

/* Default rod */
var ROD_DEFAULT = new Rod();
ROD_DEFAULT.name = "Fisher Prize FirstRod";
ROD_DEFAULT.weight = 5;
ROD_DEFAULT.max_line_distance = 25;
ROD_DEFAULT.max_line_tension = 35;
ROD_DEFAULT.owned = true;

/* Light rod */
var ROD_LIGHT = new Rod();
ROD_LIGHT.name = "FeatherRod Mark II";
ROD_LIGHT.weight = 3;
ROD_LIGHT.max_line_distance = 20;
ROD_LIGHT.max_line_tension = 30;
ROD_LIGHT.price = 350;

/* Medium rod */
var ROD_MEDIUM = new Rod();
ROD_MEDIUM.name = "Uncle Generico's EasyRod";
ROD_MEDIUM.weight = 7;
ROD_MEDIUM.max_line_distance = 40;
ROD_MEDIUM.max_line_tension = 50;
ROD_MEDIUM.price = 500;

/* Heavy rod */
var ROD_HEAVY = new Rod();
ROD_HEAVY.name = "Fishmaster Heavy";
ROD_HEAVY.weight = 10;
ROD_HEAVY.max_line_distance = 50;
ROD_HEAVY.max_line_tension = 60;
ROD_HEAVY.price = 1000;

/* Array of all available Rods in the game */
var rods = new Array();

function add_rods() {
    rods.push(ROD_DEFAULT);     // rods[0] should always be the default rod.
    rods.push(ROD_LIGHT);
    rods.push(ROD_MEDIUM);
    rods.push(ROD_HEAVY);
}