var cast_ansi = "float-wait-orig.ans";
var redraw_cast = true;
var redraw_duel = true;
var line_biting = false;
var fish_biting = undefined;    // Store which Fish is biting, if any.
var is_casting = false;
var line_distance = 0;
var line_tension = 0;

/* TODO: Get values from currently equipped Rod. */
var max_line_distance = 50;    // Metres
var max_line_tension = 25;      // Break strength, kilos.

var timer_cast = new Timer();

// Cast at lake.map[y][x] with equipped Rod.
function castAt(x, y) {
    log("Casting at position: " + x + "," + y);

    // Reset ANSI for good measure.
    cast_ansi = "float-wait-orig.ans";

    // Display cast.
    is_casting = true;
    redraw_bar = true;
    line_biting = true;
    fish_biting = undefined;
    line_distance = 0;
    line_tension = 0;

    max_line_distance = rod_equipped.max_line_distance;    // Metres
    max_line_tension = rod_equipped.max_line_tension;      // Break strength, kilos.

    // Release the lock on biting line X seconds after starting cast.
    var r_lock = random(3000) + 3000;
    //log("Locking line for " + r_lock + "s.");
    timer_cast.addEvent(r_lock, false, line_release_bite);

    fish_biting = undefined;

    var old_bar_mode = bar_mode;
    bar_mode = "cast";

    line_lock();
    timer_cast.addEvent(1000, true, event_cast_fish_move); // Fish movement event.

    // Clear any popups.
    event_popup_clear();

    // Draw initial status bar.
    renderBar("Line cast: press '?' for help.");

    while (is_casting) {

        // Cycle the casting timer.
        timer_cast.cycle();

        // Cycle frames.
        frame.cycle();

        event_cast(x, y);

        if (redraw_cast) {
            //log("DEBUG: redrawing cast");

            console.write(ANSI.DEFAULT);
            console.clear();
            console.gotoxy(1, 1);
            console.printfile(PATH_FATFISH + cast_ansi, P_NOCRLF | P_NOPAUSE);
            console.write(ANSI.DEFAULT);

            console.gotoxy(console.screen_columns, console.screen_rows);
            console.line_counter = 0;

            statusFrame.invalidate();

            redraw_cast = false;
        }

        //
        // Commands during casting mode.
        //

        c = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);
        console.write(ANSI.DEFAULT);
        switch (c.toLowerCase()) {
            case "c":
                is_casting = false
                break;

            case "?":
            case "h":
                // Display help.

                console.clear();

                console.printfile(PATH_FATFISH + "help-cast.ans");

                console.line_counter = 0;
                console.crlf();
                console.pause();

                redraw_cast = true;
                break;

            case " ":
                if (line_biting && fish_biting != undefined) {
                    var fish_snagged = false;
                    var duelling = true;

                    old_bar_mode = bar_mode;
                    bar_mode = "duel";

                    log(user.alias + " starting duel with Fish" + fish_biting.id + ": " + fish_biting.type + ".  weight: " + fish_biting.weight + "kg.  length: " + fish_biting.length + "mm.");

                    line_distance = parseFloat(((1 + Math.abs(fish_biting.depth)) * (10 + random(15))).toFixed(2));

                    // line_distance sanity check.
                    if (line_distance > (max_line_distance - 10)) {
                        // Ensure line_distance isn't longer than maximum possible - 10.
                        // This gives the player at least 10m of line to start the duel with at worst.
                        line_distance = max_line_distance - 10;
                    }

                    // Add duel timer.
                    var timer_duel = new Timer();
                    timer_duel.addEvent(250, true, event_duel); // Cast event.   

                    console.clear();

                    console.printfile(PATH_FATFISH + "duel.ans");

                    /* Display equipped rod */
                    console.gotoxy(5, 2);
                    console.write(ANSI.BOLD + ANSI.FG_WHITE + rod_equipped.name + ANSI.DEFAULT);

                    console.gotoxy(5, 5);
                    console.write(ANSI.BOLD + ANSI.FG_YELLOW + "Distance:" + ANSI.DEFAULT);

                    console.gotoxy(5, 8);
                    console.write(ANSI.BOLD + ANSI.FG_CYAN + "Tension:" + ANSI.DEFAULT);

                    var col_distance = ANSI.DEFAULT;
                    var col_tension = ANSI.DEFAULT;

                    var reel_amount = 0;

                    while (duelling) {

                        timer_duel.cycle();

                        if (redraw_duel) {

                            if (line_distance > (max_line_distance * 0.9)) {
                                col_distance = ANSI.FG_RED;
                            } else {
                                col_distance = ANSI.DEFAULT;
                            }

                            if (line_tension > (max_line_tension * 0.9)) {
                                col_tension = ANSI.FG_RED;
                            } else {
                                col_tension = ANSI.DEFAULT;
                            }

                            console.gotoxy(15, 5);
                            console.write(col_distance + line_distance.toFixed(2) + "/" + rod_equipped.max_line_distance + "m" + ANSI.DEFAULT);
                            console.cleartoeol();

                            console.gotoxy(15, 6);
                            render_status_bar(line_distance, max_line_distance, console.screen_columns - 20, 0.7, ANSI.FG_YELLOW, ANSI.BOLD + ANSI.FG_RED, ANSI.BG_GREEN);

                            console.gotoxy(15, 8);
                            console.write(col_tension + line_tension.toFixed(2) + "/" + rod_equipped.max_line_tension + "kg" + ANSI.DEFAULT);
                            console.cleartoeol();

                            console.gotoxy(15, 9);
                            render_status_bar(line_tension, max_line_tension, console.screen_columns - 20, 0.7, ANSI.FG_CYAN, ANSI.BOLD + ANSI.FG_RED, ANSI.BG_GREEN);

                            redraw_duel = false;
                        }

                        var c2 = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);
                        console.write(ANSI.DEFAULT);

                        reel_amount = 0.05 * rod_equipped.weight;

                        switch (c2.toLowerCase()) {

                            case KEY_DOWN:
                                if (line_distance > 0) {
                                    line_distance -= reel_amount + random(rod_equipped.weight/10);
                                }

                                line_tension += reel_amount + (rod_equipped.weight/5);
                                redraw_duel = true;

                                break;

                            case KEY_UP:
                                if (line_distance < 1000) {
                                    line_distance += (reel_amount / (rod_equipped.weight));
                                    if (line_tension > 0) {
                                        if (rod_equipped.weight < 10) {
                                            line_tension -= rod_equipped.weight;
                                        } else {
                                            line_tension -= 10;
                                        }
                                    }
                                    redraw_duel = true;
                                }
                                break;


                        } // end switch (c2)

                        if (line_tension < 0) {
                            line_tension = 0;
                        }

                        if (line_distance <= 0) {
                            // Player's won!
                            log("Fish" + fish_biting.id + ": bested! Properties: " + fish_biting.type + ".  weight: " + fish_biting.weight + "kg.  length: " + fish_biting.length + "mm.");
                            fish_snagged = true;
                            duelling = false;
                            renderBar("You caught a fish! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length/10 + "cm.");

                            popup_result = "You caught a fish!";
                        } else if (line_distance > max_line_distance) {
                                // Player's lost!
                            log("Fish" + fish_biting.id + ": got away! Properties: " + fish_biting.type + ".  weight: " + fish_biting.weight + "kg.  length: " + fish_biting.length + "mm.");

                            //renderBar("Fish got away!");
                            popup_result = "Fish got away! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length / 10 + "cm.";

                            fish_snagged = false;
                            duelling = false;
                            //statusFrame.draw();
                        } else if (line_tension > max_line_tension) {
                                // Player's lost: line snapped.
                            log("Fish" + fish_biting.id + ": snapped line & got away! Properties: " + fish_biting.type + ".  weight: " + fish_biting.weight + "kg.  length: " + fish_biting.length + "mm.");

                            // renderBar("Fish snapped your line!");
                            popup_result = "Line broke: too much tension. Oh, snap! " + toTitleCase(fish_biting.type) + ": " + fish_biting.weight + "kg, " + fish_biting.length/10 + "cm.";

                            fish_snagged = false;
                            duelling = false;
                        }

                        console.aborted = false;
                    } // end while(duelling)

                    console.aborted = false;

                    //log("---- End Duel...");

                    bar_mode = old_bar_mode;
                    
                    
                    if (fish_snagged) {
                        //log("Cast: Fish SNAGGED!");
                        //log("Fish caught = type: " + fish_biting.type + ".  weight: " + fish_biting.weight + "kg.  length: " + fish_biting.length + "mm.");

                        // Output win.

                        // Let go of the line.
                        line_biting = false;
                        if (fish_biting != undefined) {
                            // Remove fish from lake.
                            var i = fishes.indexOf(fish_biting);
                            if (i != -1 && fishes[i] != undefined) {
                                //log("Removing Fish from Lake. Index: " + i);

                                fishes.splice(i, 1);
                            }

                            // Add fish to fish_caught.
                            fish_caught.push(fish_biting);

                            // Update hi scores.
                            update_hi_scores(fish_biting);

                            fish_biting.is_biting = false;
                            fish_biting = undefined;

                            //log("Fish removed from lake. Lake now has " + fishes.length + " fish. You have caught " + fish_caught.length + " fish.");
                        }
                    } // end if snagged

                } else {
                    log("Cast: failed snagging fish.");

                    popup_result = "You failed to snag a fish.";

                    // Output fail.
                    /*
                    console.clear();
                    console.printfile(PATH_FATFISH + "fail1.ans");
                    console.gotoxy(1, console.screen_rows);
                    console.write(ANSI.BOLD + ANSI.FG_RED + ANSI.BG_RED + " You failed to catch anything.");
                    console.cleartoeol();
                    console.write(ANSI.DEFAULT);
                    //console.home();
                    console.gotoxy(1, console.screen_rows - 1);
                    console.getkey();
                    //console.pause();*/

                }

                is_casting = false;

                break;

        } // end switch

        // RESET CAST SETTINGS.

    }   // end while(is_casting)

    // Reset the ANSI.
    cast_ansi = "float-wait-orig.ans";

    // Let go of the line.
    line_biting = false;
    if (fish_biting != undefined) {
        fish_biting.is_biting = false;
        fish_biting = undefined;
    }

    console.write(ANSI.DEFAULT);

    bar_mode = "boat";
    renderBar();
} // end function castAt().

// Cast event.
function event_cast(x, y) {
    if (is_casting) {
        // Check for fish here.
        var f_here = fishAt(x, y);
        //log("There are " + f_here.length + " fish here.");

        // If there are fish here, bob the float.
        if (cast_ansi == "float-wait-orig.ans" && f_here.length > 0 && !line_biting) {

            // TODO: Test each Fish here to see if they bite. Only one fish can bite!
            for (var a = 0; a < f_here.length; a++) {
                if (line_biting) {
                    // If something's already biting the line, do nothing.
                    return;
                } else {
                    // Nothing's biting.
                    // Check if each Fish is interested.
                    line_biting = f_here[a].will_bite();
                    if (line_biting) {
                        fish_biting = f_here[a];
                        cast_ansi = "float-wait-orig-nudge.ans";
                        redraw_cast = true;
                        //log("Fish biting = type: " + fish_biting.type + ".  weight: " + fish_biting.weight + "kg.  length: " + fish_biting.length + "mm.");
                    }
                }
            }
        } else if ((cast_ansi == "float-wait-orig-nudge.ans" && f_here.length == 0) && !line_biting) {
                // Reset the ANSI.
            cast_ansi = "float-wait-orig.ans";

            redraw_cast = true;
        }
        
    } // end if is_casting.
}


function event_cast_fish_move() {
    timer_fish.cycle();
}

function line_release_bite() {
    line_biting = false;
    // Reset ANSI.
    cast_ansi = "float-wait-orig.ans";
    //log("line_release_bite(): line released.");
}

function line_lock() {
    //log("line_biting? " + line_biting + ". Set to true.");
    line_biting = true;
    // Reset ANSI.
    cast_ansi = "float-wait-orig.ans";
}

function event_duel() {
    //log("event_duel: " + line_distance + "m, " + fish_biting.toSource());
    var vary_direction = true;
    var directions = 3;

    var flinch = random(100) / 100;

    // The larger the fish, the more they will flinch.
    flinch += random(fish_biting.length / rod_equipped.weight) / 1000;  // By length.
    flinch += fish_biting.weight / 10;  // By weight.

    // Last breath: when distance is smaller, fight harder.
    if (line_distance < (max_line_distance * 0.1)) {
        vary_direction = false;
        //directions = 5;
        flinch = flinch * (1 + (random(50) / 100));
        //} else if (line_distance < (max_line_distance * 0.2) || line_distance < 10) {
    } else if (line_distance < (max_line_distance * 0.2)) {
            //directions = 8;
        directions = 4;
    }

    if (vary_direction) {
        // Varying directions.
        var r1 = random(directions); // 0 - move towards, else move away.

        switch (r1) {
            case 0:
                flinch = -flinch;
                break;
        }
    }

    line_distance += flinch;
    line_tension += flinch;

    // Reduce some tension.
    /*if (line_tension > 0) {
        line_tension -= 0.3;
    }*/

    if (line_tension < 0) {
        line_tension = 0;
    }

    redraw_duel = true;
}