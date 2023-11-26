load('sbbsdefs.js');

/// -- constants start -- /////////////////////////////////////////////////////

const PATH_TO_MRC_STATS = "../xtrn/mrc/mrcstats.dat";
const ACTIVITY = new Array(
    /* 0: NUL */ "\x01H\x01KNUL", 
    /* 1: LOW */ "\x01H\x01YLOW", 
    /* 2: MED */ "\x01H\x01GMED", 
    /* 3: HI  */ "\x01H\x01RHI"
);
const BBSES_X = 12;
const BBSES_Y = 18;
const ROOMS_X = 23;
const ROOMS_Y = 18;
const USERS_X = 34;
const USERS_Y = 18;
const LEVEL_X = 34;
const LEVEL_Y = 17;

const STAT_COLOR = "\x01C\x01H";

/// -- constants end -- ///////////////////////////////////////////////////////

var bbses = '\x01H\x01K---';
var rooms = '\x01H\x01K---';
var users = '\x01H\x01K---';
var level = '\x01H\x01K---';

var fMs = new File(PATH_TO_MRC_STATS);
if (fMs.open("r", true)) {
    var mrcStats = fMs.readAll()[0].split(' ');
    if (mrcStats.length >= 4) {
        bbses = mrcStats[0];
        rooms = mrcStats[1];
        users = mrcStats[2];
        level = ACTIVITY[Number(mrcStats[3])];
    }
    fMs.close();
}

console.gotoxy(BBSES_X, BBSES_Y);
printf("%s%s", STAT_COLOR, bbses);
console.gotoxy(ROOMS_X, ROOMS_Y);
printf("%s%s", STAT_COLOR, rooms);
console.gotoxy(USERS_X, USERS_Y);
printf("%s%s", STAT_COLOR, users);
console.gotoxy(LEVEL_X, LEVEL_Y);
printf("%s%s", STAT_COLOR, level);

// Put cursor at the bottom of the menu when done displaying stats
console.gotoxy(2, 22);
