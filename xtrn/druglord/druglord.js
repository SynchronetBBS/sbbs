/* DrugLord
 *  DrugLord is a game similar to Dope Wars and its derivatives for Synchronet.
 *
 * WEBSITE:
 *  http://art.poorcoding.com/druglord
 *
 * COPYRIGHT:
 *  No part of DrugLord may be used modified without consent from the author.
 *
 * AUTHOR:
 *  Art, at Fatcats BBS, dot com.
 */
load("sbbsdefs.js");

/* Path to DrugLord. */
var PATH_DRUGLORD = js.exec_dir;

load(PATH_DRUGLORD + "ANSI.js");
load(PATH_DRUGLORD + "atm.js");
load(PATH_DRUGLORD + "Drug.js");
load(PATH_DRUGLORD + "event.js");
load(PATH_DRUGLORD + "Location.js");
load(PATH_DRUGLORD + "pocket.js");

var druglord_config = {
    /* Player config */
    STARTING_CASH: 2000,
    STARTING_DEBT: 50000,
    GAME_DAYS: 30,
    INTEREST_RATE: 1.05,
    JSON_PORT: 10088,
    JSON_SERVER: "romulusbbs.com",
    MAX_INVENTORY: 100,			/* Maximum number of units player can hold. */
    MAX_LOAN: 75000,			/* Maximum amount bank will load. */
    USE_JSON: true,				/* Use JSON interbbs functions? false to disable. */
	USE_GLOBAL_SERVER: false,	/* Use author's recommended global InterBBS server? */
}

var druglord_state = {
    bank: 0,
    cash: druglord_config['STARTING_CASH'],
    debt: druglord_config['STARTING_DEBT'],
    have_todays_newspaper: false,
    is_playing: true,
    day: 0,
    drugs: new Array(),
    inventory_size: druglord_config['MAX_INVENTORY'],
    last_day_checked: 0,
    locations: new Array(),
    score: 0,
}

var VER_DRUGLORD = "0.4b";

console.clear();
console.write(ANSI.DEFAULT);

/* Initialize locations. */
locations_init();

/* Set player to a location. */
druglord_state['location'] = druglord_state['locations'][0];

/* Initialize drugs. */
drugs_init();

/* JSON initialize code. */
if (druglord_config['USE_JSON']) {
    load("json-client.js");
	try {

		/* Note: to override JSON server settings, please use the standard server.ini file. */		
		if (!druglord_config['USE_GLOBAL_SERVER']) {
			var server_file = new File(file_cfgname(PATH_DRUGLORD, "server.ini"));
			server_file.open('r', true);
			druglord_config['JSON_SERVER'] = server_file.iniGetValue(null, "host", "localhost");
			druglord_config['JSON_PORT'] = server_file.iniGetValue(null, "port", 10088);
			server_file.close();			
		}

		log("DrugLord connecting to InterBBS server: " + druglord_config['JSON_SERVER'] + ":" + druglord_config['JSON_PORT'] + "... ");
		console.writeln("Connecting to InterBBS server: " + druglord_config['JSON_SERVER'] + ":" + druglord_config['JSON_PORT'] + "... ");

		/* JSON client object. */
		var json = new JSONClient(druglord_config['JSON_SERVER'], druglord_config['JSON_PORT']);
	} catch (e) {
		/* Exception thrown connecting. Disable JSON this session to avoid heartbreak. */
		druglord_config['USE_JSON'] = false;

		console.writeln("Error connecting to InterBBS server: " + e.message + ". InterBBS mode disabled for this session.");
		console.pause();
	}
}

druglord_state['day'] = 0;

/* Introductory movie. */
intro();

/* Main prompt. */
while (druglord_state['is_playing']) {
    /* Ensure a nice day. Fixes X.0 day bug behavior. */
    druglord_state['day'] = Number(Number(druglord_state['day']).toFixed(1));

    if (druglord_state['last_day_checked'] < Math.floor(druglord_state['day'])) {
        /* Daily events */

        druglord_state['last_day_checked']++;

        randomize_prices();

        calculate_interest();

        /* Reset have_todays_newspaper */
        druglord_state['have_todays_newspaper'] = false;
    }

    /* Recalculate score. */
    calculate_score();

    /* Display location information. */
    console.crlf();
    console.writeln(ANSI.BG_RED + "  " + ANSI.BG_BLUE + ANSI.BOLD + ANSI.FG_WHITE + ' ' + druglord_state['location'].name + ' ' + ANSI.BG_RED + "  " + ANSI.DEFAULT);
    console.crlf();

    if (druglord_state['location'].news.length > 0) {
        console.writeln(ANSI.BOLD + ANSI.FG_YELLOW + druglord_state['location'].news + ANSI.DEFAULT);
        console.crlf();
    }

    /* Display drugs. */
    for (var a = 0; a < druglord_state['drugs'].length; a++) {
        /* TODO: Highlight good/bad deals. */
        console.write("  " + (a + 1) + '. (' + ANSI.BOLD + druglord_state['drugs'][a].inventory + ANSI.DEFAULT);

        if (a == 0)
            console.write(' owned');

        console.write(')\t\t' + ANSI.BOLD + druglord_state['drugs'][a].name + ANSI.DEFAULT);
        console.write("\t\tœ" + nice_number(druglord_state['location'].price[druglord_state['drugs'][a].name.toLowerCase()]));

        if (a == 0)
            console.write("/unit");

        console.crlf();
    }
    console.crlf();

    console.writeln(ANSI.BOLD + " b Buy drugs.\t\tt Travel.\t\tn Daily news.");
    console.writeln(" s Sell drugs.\t\t$ Use ATM.\t\tq Quit.");
    if (druglord_config['USE_JSON']) {
        console.writeln("\t\t\t\t\t\ti InterBBS scoreboard.");
    }
    console.write(ANSI.DEFAULT)
    console.crlf();

    /* Display cash. */
    console.write(ANSI.FG_GREEN + " Cash: œ" + ANSI.BOLD + nice_number(druglord_state['cash']) + ANSI.DEFAULT);
    /* Display debt. */
    if (druglord_state['debt'] > 0) {
        console.write("\t" + ANSI.FG_RED + " Debt: œ" + ANSI.BOLD + nice_number(druglord_state['debt']) + ANSI.DEFAULT);
    }
    /* Display pockets. */
    console.writeln("\t" + ANSI.FG_CYAN + " Pockets: " + ANSI.BOLD + (druglord_state['inventory_size'] - get_player_drug_units()) + " spaces free." + ANSI.DEFAULT);
    console.crlf();

    /* Display date and prompt. */
    console.write(ANSI.BOLD + ANSI.FG_MAGENTA + 'Day ' + druglord_state['day'].toFixed(1) + ' of ' + druglord_config['GAME_DAYS'] + ', score: ' + nice_number(druglord_state['score']) + ' > ' + ANSI.DEFAULT);

    var dl_main_cmd = console.getkey().toLowerCase();

    if (dl_main_cmd) {
        console.writeln(dl_main_cmd);

        switch (dl_main_cmd) {
            case '$':
                /* ATM */
                atm();
                break;

            case 'b':
                /* Buy. */
                buy_drugs();
                break;

            case 'i':
                /* InterBBS Scores. */
                json_show_scores();
                break;

            case 'n':
                /* Daily news */
                daily_news();
                break;

            case 's':
                /* Sell. */
                sell_drugs();
                break;

            case 'q':
                var q_cmd = console.noyes("Quit game -- progress will be lost");

                if (q_cmd == false) {
                    if (druglord_config['USE_JSON']) {
                        var q_submit = console.yesno("Submit score: " + nice_number(druglord_state['score']));

                        if (q_submit) {
                            json_send_score();
                        }
                    }
                    druglord_state['is_playing'] = false;
                }
                break;

            case 't':
                /* Travel to another location. */
                travel_locations();
                break;

            case '?':
                console.printfile(PATH_DRUGLORD + "druglord.ans");
                console.crlf();
                console.writeln("This is DrugLord v" + VER_DRUGLORD + ".");
                console.crlf();
                console.pause();
                break;

            default:
                console.crlf();
                console.crlf();
                break;

        } // End switch(dl_main_cmd).

        /* Recalculate score. */
        calculate_score();

        /* Check if game over. */
        check_state();

    } // End if (dl_main_cmd).
}

/* Display scores. */
if (console.yesno("Display InterBBS scores") == true) {
    json_show_scores();
}

/* Quit DrugLord. */
console.line_counter = 0;
console.printfile(PATH_DRUGLORD + "winners.ans", P_NOPAUSE);
console.write(ANSI.DEFAULT + " ");
console.getkey();

function calculate_interest() {
    /* Daily interest rate. */
    druglord_state['debt'] = Math.round(druglord_state['debt'] * druglord_config['INTEREST_RATE']);
}

/* Calculate and update score. */
function calculate_score() {
    /* Score is what you average profit per day is:
     * Total profit / Total days elapsed.
     */
    druglord_state['score'] = Math.max(0, (druglord_state['cash'] - druglord_state['debt']) / (druglord_config['GAME_DAYS'])).toFixed(0);
}

function check_state() {
    if (druglord_state['day'] > (druglord_config['GAME_DAYS'] + 1)) {
        druglord_state['is_playing'] = false;

        console.crlf();
        console.crlf();
        console.crlf();
        console.writeln("  " + ANSI.BOLD + ANSI.FG_YELLOW + ANSI.BG_MAGENTA + " TIME'S UP! " + ANSI.DEFAULT);
        console.crlf();
        console.writeln(ANSI.BOLD + "\tFinal balance:\tœ" + nice_number((druglord_state['cash'] - druglord_state['debt'])) + "." + ANSI.DEFAULT);
        console.crlf();
        console.crlf();
        console.writeln(ANSI.BOLD + "\tFinal score:\tœ" + nice_number(druglord_state['score']) + "." + ANSI.DEFAULT);

        if (druglord_state['score'] >= 1000000) {
            console.writeln(ANSI.BOLD + ANSI.FG_YELLOW + "\t\tYou've joined the Millionaire's Club--congratulations!" + ANSI.DEFAULT);
        }

        console.crlf();
        console.crlf();
        console.pause();

        if (druglord_config['USE_JSON']) {
            /* Submit score to JSON server. */
            json_send_score();
        }
    }
}

function daily_news() {
    if (druglord_state['have_todays_newspaper'] == false) {
        var newspaper_price = 1000 * Math.floor(druglord_state['day']);

        console.crlf();
        console.writeln(ANSI.FG_RED + "You do not have today's edition of The Daily DrugLord Insider." + ANSI.DEFAULT);
        var buy_paper = console.noyes("Purchase today's edition for œ" + nice_number(newspaper_price));
        if (buy_paper == false) {
            /* Buy today's newspaper edition. */

            /* Sanity checks and debit amount from player. */
            if (druglord_state['cash'] >= newspaper_price) {
                console.writeln(ANSI.BOLD + ANSI.FG_WHITE + "You purchase today's edition." + ANSI.DEFAULT);
                druglord_state['cash'] -= newspaper_price;
                druglord_state['have_todays_newspaper'] = true;
            } else {
                console.writeln(ANSI.BOLD + ANSI.FG_RED + "You don't have enough money." + ANSI.DEFAULT);
                return;
            }
        } else {
            return;
        }
    }

    var have_news = false;
    console.crlf();
    console.writeln(ANSI.BG_WHITE + ANSI.FG_BLACK + "  The Daily DrugLord Insider  " + ANSI.DEFAULT)
    console.crlf();

    for (var a = 0; a < druglord_state['locations'].length; a++) {
        if (druglord_state['locations'][a].news.length > 0) {
            console.writeln("  " + ANSI.BOLD + druglord_state['locations'][a].name + ": " + ANSI.DEFAULT);
            console.writeln("    \"" + druglord_state['locations'][a].news + "\"");
            console.crlf();
            have_news = true;
        }
    }

    if (have_news) {
        console.pause();
    } else {
        console.writeln("  No news today. Remember to read our paper tomorrow!");
        console.pause();
    }
}

function intro() {
    /* Black out. */
    console.clear();
    console.crlf();

    console.writeln(ANSI.BOLD + "  You awake, bloodied and battered, lying in a pool of filth... " + ANSI.DEFAULT);
    mswait(1500);
    console.crlf();
    console.writeln("  Struggling to your feet, you recall the events leading to this disaster:");
    mswait(500);
    console.crlf();
    console.writeln(ANSI.BOLD + ANSI.FG_BLACK + "<memory>" + ANSI.DEFAULT);
    console.writeln(ANSI.FG_CYAN + "  You:\t\t... Another month, that's all I need to pay you back--please!");
    mswait(500);
    console.crlf();
    console.writeln(ANSI.FG_RED + "  Bossman:\tAgain? This ain't a charity--you've got one month.");
    console.writeln(ANSI.FG_RED + "\t\tAnd, to show that we're serious:");
    mswait(1000);
    console.writeln(ANSI.BOLD + ANSI.FG_RED + "\t\t... TEACH THAT FOOL A LESSON, BOYZ!" + ANSI.DEFAULT);
    mswait(1000);
    console.crlf();
    console.writeln(ANSI.BOLD + "  The gang deals out a vicious beating, you succumb in a heap of fear and rage." + ANSI.DEFAULT);
    mswait(500);
    console.writeln(ANSI.BOLD + "  As your surroundings fade, the last thing you hear is:" + ANSI.DEFAULT);
    console.crlf();
    console.writeln(ANSI.FG_RED + "  Bossman:\tConsider this month's deposit paid: you have one month, bitch." + ANSI.DEFAULT);
    mswait(1000);
    console.writeln(ANSI.BOLD + ANSI.FG_RED + "\t\tThanks for doing business with London Third National--we value\r\n\t\tyour custom!" + ANSI.DEFAULT);
    console.crlf();
    console.writeln(ANSI.BOLD + "  You black out." + ANSI.DEFAULT);
    console.writeln(ANSI.BOLD + ANSI.FG_BLACK + "</memory>" + ANSI.DEFAULT);

    console.pause();
    console.write(ANSI.DEFAULT);

    /* Fade in. */
    console.crlf();
    console.writeln(ANSI.BOLD + ANSI.FG_BLACK + "<instructions>" + ANSI.DEFAULT);
    console.writeln(ANSI.DEFAULT + ANSI.FG_MAGENTA + "  You have 30 days to repay the debt--with daily interest." + ANSI.DEFAULT);
    console.crlf();
    console.writeln(ANSI.DEFAULT + ANSI.FG_MAGENTA + "  Buy low, sell high, and take advantage of price fluctuations (purchase the daily newspapers for the inside scoop). Repay your debt at the bank's ATM machine as soon as possible. Increase your pocketspace to hold more items." + ANSI.DEFAULT);
    console.crlf();
    console.writeln(ANSI.DEFAULT + ANSI.FG_MAGENTA + "  Profit as much as you can before the end of the 30th day." + ANSI.DEFAULT);
    console.writeln(ANSI.BOLD + ANSI.FG_BLACK + "</instructions>" + ANSI.DEFAULT);
    console.crlf();
    console.writeln(ANSI.BOLD + "  Composing your thoroughly-tenderized self," + ANSI.DEFAULT);
    console.writeln(ANSI.BOLD + "  you come to a dreadful realization..." + ANSI.DEFAULT);
    console.crlf();
    console.pause();
    //console.clear();

    console.line_counter = 0;
    console.printfile(PATH_DRUGLORD + "druglord.ans", P_NOPAUSE);
    console.pause();
}

/* Get updated list of scores from JSON server. */
function json_get_scores() {
    /* TODO: JSON initialize code. */
    if (druglord_config['USE_JSON']) {
        /* Renew JSON object if required. */
        if (json == undefined) {
            /* JSON client object. */
            json = new JSONClient(druglord_config['JSON_SERVER'], druglord_config['JSON_PORT']);
        }

        var scores = json.read("druglord", "", 1);

        if (scores != undefined) {
            return scores;
        }
    } else {
        /* USE_JSON is false. */
        console.writeln(ANSI.FG_RED + "InterBBS is not configured--yell at your sysop. :|" + ANSI.DEFAULT);
        console.pause();
    }

    return null;
}

/* Submit score to JSON server. */
function json_send_score() {
    try {
        if (druglord_config['USE_JSON']) {

            /* TODO: Update JSON scores. */

            /* Renew JSON object if required. */
            if (json == undefined) {
                /* JSON client object. */
                json = new JSONClient(druglord_config['JSON_SERVER'], druglord_config['JSON_PORT']);
            }


            /* Check it beats current JSON score. */
            var s1 = json.read("druglord", system.qwk_id + "." + user.alias + ".score", 1);
            if (s1 == undefined)
                s1 = 0;

            if (Number(druglord_state['score']) > Number(s1)) {
                /* Send score to server. */
                console.writeln("New personal record! Sending score to server...");
                try {
                    if (user.alias && (user.alias.length > 0)) {
                        json.write("druglord", system.qwk_id + "." + user.alias + ".score", druglord_state['score'], 2);
                    } else {
                        console.writeln("Invalid user/alias. Not saved.");
                    }
                } catch (e_json) {
                    console.writeln("There was an error sending score to server. Darkest condolences.");
                    log("json_send_score() Exception: " + e_json.message);
                }
            } else {
                console.writeln("That does not beat your previous best score. Not saved.");
            }

        } else {
            /* USE_JSON is false. */
            console.writeln(ANSI.FG_RED + "InterBBS is not configured--yell at your sysop. :|" + ANSI.DEFAULT);
            console.pause();
        }

    } catch (e) {
        log("json_send_score() Exception: " + e.message);
        //console.writeln("Error submitting score to server.");
        console.pause();
    }
}

/* Display InterBBS scores. */
function json_show_scores() {
    var scores = json_get_scores();

    if (scores != null && scores != undefined) {
        //console.clear();
        console.printfile(PATH_DRUGLORD + "highscore.ans");
        console.line_counter = 20;

        var bbses = Object.keys(scores);

        for (var b = 0; b < bbses.length; b++) {
            /* For every BBS... */
            console.writeln(ANSI.BOLD + ANSI.FG_CYAN + " " + bbses[b] + ANSI.DEFAULT);

            var players = Object.keys(scores[bbses[b]]);

            for (var c = 0; c < players.length; c++) {
                if (players[c] && (players[c].length > 0)) {
                    /* For every player (in every BBS)... */

                    /* Paginator. */
                    if (console.line_counter >= console.screen_rows - 3) {
                        //log("Paginating.");
                        console.pause();
                        console.line_counter = 0;
                    }

                    var col = ANSI.DEFAULT;
                    var score = scores[bbses[b]][players[c]].score;
                    if (score >= 1000000)
                        col = ANSI.BOLD + ANSI.FG_YELLOW;
                    console.writeln(col + "  " + players[c] + ANSI.DEFAULT + ":\t\t\tœ" + nice_number(score));
                }
            }
            console.crlf();

        }

        console.pause();
    }
}

function nice_number(number) {
    /* http://stackoverflow.com/questions/5731193/how-to-format-numbers-using-javascript */
    number = Number(number).toFixed(0) + '';
    x = number.split('.');
    x1 = x[0];
    x2 = x.length > 1 ? '.' + x[1] : '';
    var rgx = /(\d+)(\d{3})/;
    while (rgx.test(x1)) {
        x1 = x1.replace(rgx, '$1' + ',' + '$2');
    }
    return x1 + x2;
}