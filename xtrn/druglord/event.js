function event_cops() {

    /* Check if carrying any drugs. */
    if (get_player_drug_units() > 0) {
        /* TODO: chance of evasion. */

        /* Player carrying drugs. */
        console.writeln(ANSI.BOLD + ANSI.FG_CYAN + "While travelling, you are caught by the police! All your drugs are belong to us! You narrowly escape capture." + ANSI.DEFAULT);

        /* Clear all drugs from inventory. */
        for (var a = 0; a < druglord_state['drugs'].length; a++) {
            druglord_state['drugs'][a].inventory = 0;
        }

    } else {
        /* Player not carrying drugs. */
        console.writeln(ANSI.BOLD + ANSI.FG_CYAN + "While travelling, the police search you. They find nothing and let you go on your way. :D" + ANSI.DEFAULT);
    }
    console.crlf();
    console.pause();
    console.crlf();
}

function event_dog() {
    /* Check if carrying any drugs. */
    if (get_player_drug_units() > 0) {
        console.writeln(ANSI.BOLD + ANSI.FG_CYAN + "While travelling, a drug dog sniffs you out! They sieze all the drugs from you, but you evade capture escaping butt naked into the darkness!" + ANSI.DEFAULT);

        /* Clear all drugs from inventory. */
        for (var a = 0; a < druglord_state['drugs'].length; a++) {
            druglord_state['drugs'][a].inventory = 0;
        }

        /* Reset pocketspace. */
        druglord_state['inventory_size'] = druglord_config['MAX_INVENTORY'];

    } else {
        console.writeln(ANSI.BOLD + ANSI.FG_CYAN + "While travelling, a drug dog sniffs at your crotch. After a humiliating search, they find nothing and let you go on your way. :|" + ANSI.DEFAULT);
    }

    console.crlf();
    console.pause();
    console.crlf();
}