/* Prices, per KG. */
var price_bass = 5.49;
var price_eel = 24.99;
var price_pike = 3.30;
var price_trout = 6.05;

/* KGs of each species the player has unsold */
var kg_bass = parseFloat(0);
var kg_eel = parseFloat(0);
var kg_pike = parseFloat(0);
var kg_trout = parseFloat(0);

var price_fish_finder = 27.50;

var redraw_shop = true;

function shop() {
    var is_shopping = true;
    redraw_shop = true;

    /* Preserve old status mode */
    old_bar_mode = bar_mode;
    bar_mode = "shop";

    /* Layer and open frame */
    shopFrame.top();
    shopFrame.open();

    /* Main shop loop */
    while (is_shopping) {
        frame.cycle();
        timer_fish.cycle(); // The Fish keep moving.

        if (redraw_shop) {

            shopFrame.gotoxy(1, 1);
            shopFrame.putmsg(" Fonzo's Tackle and Bait Shoppe", HIGH | GREEN | BG_GREEN);
            shopFrame.cleartoeol(BG_GREEN);
            shopFrame.gotoxy(1, 3);
            shopFrame.putmsg(" B  Buy  ", DARKGRAY | BG_LIGHTGRAY);
            shopFrame.gotoxy(1, 4);
            shopFrame.putmsg(" S  Sell ", DARKGRAY | BG_LIGHTGRAY);
            shopFrame.gotoxy(1, 5);
            shopFrame.putmsg(" Q  Quit ", RED | BG_LIGHTGRAY);

            shopFrame.gotoxy(1, shopFrame.height);
            shopFrame.putmsg(" Credits: $" + parseFloat(player_credits).toFixed(2), BLACK | BG_LIGHTGRAY);
            
            renderBar("Fonzo's Tackle and Bait Shoppe");

            redraw_shop = false;
        }

        var c = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);
        console.write(ANSI.DEFAULT);

        switch (c.toLowerCase()) {
            case "b":
                // Buy
                buy();
                break;

            case "s":
                // Sell

                var unsold = sell_fish();

                var is_selling = true;

                while (is_selling) {
                    
                    var y = 1;
                    var attr = BLUE | BG_LIGHTGRAY
                    shopFrame.clear(BG_LIGHTGRAY);

                    shopFrame.gotoxy(1, y);                    
                    shopFrame.cleartoeol(BG_GREEN);
                    shopFrame.putmsg(" Sell all unsold fish? ", HIGH | GREEN | BG_GREEN);                    
                    y++;
                    y++;

                    shopFrame.gotoxy(1, y);
                    y++;
                    shopFrame.putmsg("      Bass: " + parseFloat(kg_bass).toFixed(2) + " kg @ $" + price_bass.toFixed(2) + "/kg", attr);

                    shopFrame.gotoxy(1, y);
                    shopFrame.putmsg("       Eel: " + parseFloat(kg_eel).toFixed(2) + " kg @ $" + price_eel.toFixed(2) + "/kg", attr);
                    y++;

                    shopFrame.gotoxy(1, y);
                    y++;
                    shopFrame.putmsg("      Pike: " + parseFloat(kg_pike).toFixed(2) + " kg @ $" + price_pike.toFixed(2) + "/kg", attr);

                    shopFrame.gotoxy(1, y);
                    y++;
                    shopFrame.putmsg("     Trout: " + parseFloat(kg_trout).toFixed(2) + " kg @ $" + price_trout.toFixed(2) + "/kg", attr);

                    shopFrame.gotoxy(1, y);
                    var earnings = parseFloat(price_bass.toFixed(2) * kg_bass + price_eel.toFixed(2) * kg_eel + price_pike.toFixed(2) * kg_pike + price_trout.toFixed(2) * kg_trout).toFixed(2);
                    shopFrame.putmsg("  Earnings: $" + earnings, HIGH | BLUE | BG_LIGHTGRAY);
                    y++;
                    y++;
                    shopFrame.gotoxy(1, y);
                    y++;
                    shopFrame.putmsg(" Y  Yes, sell.    ", DARKGRAY | BG_LIGHTGRAY);

                    shopFrame.gotoxy(1, y);
                    y++;
                    shopFrame.putmsg(" Q  Quit to shop. ", RED | BG_LIGHTGRAY);

                    shopFrame.gotoxy(1, shopFrame.height);
                    shopFrame.putmsg(" Credits: $" + parseFloat(player_credits).toFixed(2), BLACK | BG_LIGHTGRAY);

                    shopFrame.draw();

                    var c_sell = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);

                    switch (c_sell.toLowerCase()) {
                        
                        case "y":
                            /* Sell all unsold fish. */

                            /* Ensure we're using floats */
                            player_credits = parseFloat(player_credits);
                            earnings = parseFloat(earnings);

                            /*log("SELLING:");
                            log("credits: " + player_credits.toSource());
                            log("earnings: " + earnings.toSource());*/

                            // Add to credits.
                            player_credits = parseFloat(player_credits + earnings).toFixed(2);

                            // Update json.
                            json_write_credits();

                            // Mark Fish as sold.
                            //log("TODO: marking " + unsold.length + " fish as sold... New player_credits: " + player_credits.toSource());
                            for (var a = 0; a < unsold.length; a++) {
                                unsold[a].sold = true;
                            }


                            //break;
                        case "q":
                            is_selling = false;
                            shopFrame.clear(BG_LIGHTGRAY);

                            break;
                    }
                }
                redraw_shop = true;
                break;
            case "q":
                // Quit
                is_shopping = false;
                shopFrame.clear(BG_LIGHTGRAY);
                shopFrame.close();
                break;
        }
    }

    bar_mode = old_bar_mode;
    shopFrame.close();
    event_reset_bar();
}

function sell_fish() {
    var unsold = new Array();
    kg_bass = parseFloat(0);
    kg_eel = parseFloat(0);
    kg_pike = parseFloat(0);
    kg_trout = parseFloat(0);

    for (var a = 0; a < fish_caught.length; a++) {
        if (fish_caught[a].sold) {
            //log("Fish" + fish_caught[a].id + " has already been sold.");
        } else {
            //log("Fish" + fish_caught[a].id + " has not been sold.");            

            switch (fish_caught[a].type) {
                case FISH_TYPE_BASS:
                    kg_bass += fish_length_to_weight(fish_caught[a]);
                    break;

                case FISH_TYPE_EEL:
                    kg_eel += fish_length_to_weight(fish_caught[a]);
                    break;

                case FISH_TYPE_PIKE:
                    kg_pike += fish_length_to_weight(fish_caught[a]);
                    break;

                case FISH_TYPE_TROUT:
                    kg_trout += fish_length_to_weight(fish_caught[a]);
                    break;
            }

            //log("RUNNING Unsold fish total weight: Bass: " + kg_bass + ", Pike: " + kg_pike + ", Trout: " + kg_trout + ".");

            // Add to unsold array.
            unsold.push(fish_caught[a]);
        }
    }    

    //log("Unsold fish total weight: Bass: " + kg_bass + ", Pike: " + kg_pike + ", Trout: " + kg_trout + ".");

    return unsold;
}

function buy() {
    // Buy

    var is_buying = true;

    while (is_buying) {
        var attr = DARKGRAY | BG_LIGHTGRAY
        var cattr = WHITE | BG_LIGHTGRAY;
        var y = 1;

        shopFrame.clear(BG_LIGHTGRAY);

        shopFrame.gotoxy(1, y);
        shopFrame.cleartoeol(BG_GREEN);
        shopFrame.putmsg(" Buy ", HIGH | GREEN | BG_GREEN);
        y++;
        y++;

        shopFrame.gotoxy(1, y);
        shopFrame.putmsg(" F  Fish finder ($" + parseFloat(price_fish_finder).toFixed(2) + ")", cattr);
        y++;
        shopFrame.gotoxy(1, y);
        shopFrame.putmsg("    (Only usable in this game)", cattr);
        y++;
        y++;

        /* Display available Rods for sale */
        for (var idx = 1; idx < rods.length ; idx++) {
            shopFrame.gotoxy(1, y);
            if (rods[idx].owned) {
                shopFrame.putmsg(" " + idx + "  " + rods[idx].name + " ($" + rods[idx].price.toFixed(2) + ")", attr);
            } else {
                shopFrame.putmsg(" " + idx + "  " + rods[idx].name + " ($" + rods[idx].price.toFixed(2) + ")", cattr);
            }
            y++;
        }
        y++;

        shopFrame.gotoxy(1, y);
        shopFrame.putmsg(" Q  Quit to shop. ", RED | BG_LIGHTGRAY);
        y++;

        shopFrame.gotoxy(1, shopFrame.height);
        shopFrame.putmsg(" Credits: $" + parseFloat(player_credits).toFixed(2), BLACK | BG_LIGHTGRAY);

        shopFrame.draw();

        var c_buy = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);

        switch (c_buy.toLowerCase()) {
            case "f":
                // Check if have credits.

                if (player_credits >= price_fish_finder) {
                    // Take credits.
                    player_credits -= price_fish_finder;

                    // Add fish finder.
                    fish_finder_points++;

                    // Update JSON.
                    json_write_credits();

                    renderBar("PURCHASED: You now have " + fish_finder_points + " fish finder points.");
                } else {
                    renderBar("Insufficient credits.");
                }

                break;

            case "1":
            case "2":
            case "3":
                if (player_credits >= rods[c_buy].price) {
                    if (rods[c_buy].owned) {
                        renderBar("You already own this rod.");
                    } else {
                        // Take credits.
                        player_credits -= rods[c_buy].price;

                        // Add Rod to owned.
                        rods[c_buy].owned = true;

                        // Update JSON.
                        json_write_credits();
                        json_write_rod(c_buy);

                        renderBar("PURCHASED: " + rods[c_buy].name + ".");
                    }
                } else {
                    renderBar("Insufficient credits.");
                }

                break;

            case "q":
                is_buying = false;
                redraw_shop = true;
                shopFrame.clear(BG_LIGHTGRAY);
                break;
        }
    }
}

function json_write_credits() {
    // Update json.
    try {
        if (player_credits != undefined)
            json.write("fatfish", system.qwk_id + "." + user.alias + ".credits", player_credits, 2);
    } catch (e) {
        log("EXCEPTION: json.write(player_credits): " + e.toSource());
    }
}

function json_write_rod(idx) {
    try {
        if (rods[idx] != undefined)
            json.write("fatfish", system.qwk_id + "." + user.alias + ".rods_owned." + idx, rods[idx].owned, 2);
    } catch (e) {
        log("EXCEPTION: json_write_rod(): " + e.toSource());
    }
}