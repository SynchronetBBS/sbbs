function Location() {
    /* Location name. */
    this.name = "Location";

    /* x,y global co-ordinates. */
    this.x = 0;
    this.y = 0;

    this.drugs = undefined;

    this.news = "";

    this.price = {
    }
}

function locations_init() {
    var loc_walthamstow = new Location();
    loc_walthamstow.name = "Walthamstow Central";
    druglord_state['locations'].push(loc_walthamstow);

    var loc_camden = new Location();
    loc_camden.name = "Camden Town";
    druglord_state['locations'].push(loc_camden);

    var loc_kgx = new Location();
    loc_kgx.name = "King's Cross";
    druglord_state['locations'].push(loc_kgx);

    var loc_pad = new Location();
    loc_pad.name = "Paddington";
    druglord_state['locations'].push(loc_pad);

    var loc_soh = new Location();
    loc_soh.name = "Soho / Chinatown";
    druglord_state['locations'].push(loc_soh);

    var loc_brixton = new Location();
    loc_brixton.name = "Brixton";
    druglord_state['locations'].push(loc_brixton);

    var loc_pkm = new Location();
    loc_pkm.name = "Peckham";
    druglord_state['locations'].push(loc_pkm);

    randomize_prices();
}

/* Randomize prices for all locations. */
function randomize_prices() {
    for (var a = 0; a < druglord_state['locations'].length; a++) {
        druglord_state['locations'][a].price = {
            cocaine: 10000 + random(15000),
            hash: 250 + random(700),
            heroin: 5000 + random(10000),
            lsd: 800 + random(1000),
            mdma:1000 + random(2500),
            pcp:700 + random(1500),
            peyote:300 + random(500),
            shrooms:200 + random(500),
            speed: 100 + random(150),
            weed: 140 + random(250),
            
        }
        druglord_state['locations'][a].news = "";

        /* TODO: Random events. */
        var r = random(4);

        if (r == 0) {
            var r_drug = random(druglord_state['drugs'].length);

            var drug = druglord_state['drugs'][r_drug];

            if (drug != undefined) {

                //log(druglord_state['drugs'].toSource());

                var reason = random(5);

                var reason_str;

                switch (reason) {
                    case 0:
                        reason_str = "Drug convention drives prices for " + drug.name + " sky high!";
                        break;
                    case 1:
                        reason_str = "Huge " + drug.name + " bust: addicts buy at ridiculous prices!";
                        break;
                    case 2:
                        reason_str = "Busted " + drug.name + " dealer: shortage makes prices outrageous!";
                        break;
                    case 3:
                        reason_str = drug.name + " drought: addicts will buy at any price!";
                        break;
                    default:
                        reason_str = "Border control seizes " + drug.name + ": addicts are going insane!";
                        break;
                }

                druglord_state['locations'][a].news = reason_str;

                druglord_state['locations'][a].price[drug.name.toLowerCase()] *= 2 + random(3);
            }
        }
    }
}

function travel_locations() {
    console.writeln(ANSI.BOLD + "Where do you want to go?" + ANSI.DEFAULT);
    for (var a = 0; a < druglord_state['locations'].length; a++) {
        console.writeln("  " + Number(a+1) + ". " + druglord_state['locations'][a].name + ".");
    }

    console.write("Travel to: ");

    var idx_travel = console.getnum(druglord_state['locations'].length) - 1;

    if (druglord_state['location'] == druglord_state['locations'][idx_travel]) {
        console.crlf();
        console.writeln(ANSI.FG_RED + "You're already here." + ANSI.DEFAULT);
        console.crlf();
    } else if (druglord_state['locations'][idx_travel] != undefined) {
        console.crlf();
        console.writeln(ANSI.FG_BLACK + ANSI.BOLD + "Travelling to " + druglord_state['locations'][idx_travel].name + "..." + ANSI.DEFAULT);
        console.crlf();
        druglord_state['location'] = druglord_state['locations'][idx_travel];

        /* Add time each travel. */
        druglord_state['day'] += 0.3;

        var event_triggered = false;

        /* Chance to buy pocket. */
        var r_pocket = random(10);
        if (r_pocket == 0) {
            buy_pockets();
            event_triggered = true;
        }

        /* Generate random events. */
        if (event_triggered == false) {
            var r_event = random(100);
            switch (r_event) {
                case 0:
                    event_cops();
                    event_triggered = true;
                    break;
                case 1:
                    event_dog();
                    event_triggered = true;
                    break;
            }
        }
    }
}