function Drug() {
    this.name = "Drug";
    this.inventory = 0;
    this.price = 0;
}

function drugs_init() {
    var drugs = new Array();

    var cocaine = new Drug();
    cocaine.name = "Cocaine";
    drugs.push(cocaine);

    var hash = new Drug();
    hash.name = "Hash";
    drugs.push(hash);

    var heroin = new Drug();
    heroin.name = "Heroin";
    drugs.push(heroin);

    var lsd = new Drug();
    lsd.name = "LSD";
    drugs.push(lsd);

    var mdma = new Drug();
    mdma.name = "MDMA";
    drugs.push(mdma);

    var pcp = new Drug();
    pcp.name = "PCP";
    drugs.push(pcp);

    var peyote = new Drug();
    peyote.name = "Peyote";
    drugs.push(peyote);

    var shrooms = new Drug();
    shrooms.name = "Shrooms";
    drugs.push(shrooms);

    var speed = new Drug();
    speed.name = "Speed";
    drugs.push(speed);

    var weed = new Drug();
    weed.name = "Weed";
    drugs.push(weed);

    druglord_state['drugs'] = new Object(drugs);
}

function buy_drugs() {
    console.write("Buy what? ");

    var idx_buy = console.getnum(druglord_state['drugs'].length) - 1;

    if (idx_buy != NaN && druglord_state['drugs'][idx_buy] != undefined) {
        var drug = druglord_state['drugs'][idx_buy];
        var price = druglord_state['location'].price[drug.name.toLowerCase()];

        if ((druglord_state['cash'] / price) >= 1) {
            var max_units = Math.min(Math.floor(druglord_state['cash'] / price), (druglord_state['inventory_size'] - get_player_drug_units()));
            console.write("Buy how many units of " + drug.name + " for œ" + nice_number(price) + "/unit? (CR: max - " + max_units + "): ");

            var buy_amt = console.getnum(max_units);
            // Returns 0 if CR entered.

            /* Default to buy maximum possible. */
            if (buy_amt === 0) {
                buy_amt = max_units;
            }
            

            if (buy_amt != NaN && buy_amt >= 1) {

                if ((get_player_drug_units() + buy_amt) <= druglord_state['inventory_size']) {

                    var cost = buy_amt * price;
                    var buy_confirm = console.yesno(buy_amt + " units will cost œ" + nice_number(cost));

                    if (buy_confirm) {
                        console.writeln("You pay for the transaction.");

                        var player_drug = get_player_drug(drug.name);
                        player_drug.inventory += buy_amt;

                        druglord_state['cash'] -= cost;
                    }

                } else {
                    console.writeln("You can't hold that much.");
                }

            }
        } else {
            console.writeln("Nae. You can't afford that.");
        }
    }
}

function get_player_drug(name) {
    for (var a = 0; a < druglord_state['drugs'].length; a++) {
        if (name.toLowerCase() == druglord_state['drugs'][a].name.toLowerCase()) {
            //log("Returning: " + druglord_state['drugs'][a].name);
            return druglord_state['drugs'][a];
        }
    }
}

/* Return number of units used in inventory. */
function get_player_drug_units() {
    var count = 0;

    for (var a = 0; a < druglord_state['drugs'].length; a++) {
        count += druglord_state['drugs'][a].inventory;
    }

    return count;
}

function sell_drugs() {
    console.write("Sell what? ");

    var idx_sell = console.getnum(druglord_state['drugs'].length) - 1;

    if (idx_sell != NaN && druglord_state['drugs'][idx_sell] != undefined) {
        var drug = druglord_state['drugs'][idx_sell];
        var price = druglord_state['location'].price[drug.name.toLowerCase()];
        
        if (drug.inventory >= 1) {
            console.write("Sell how many units of " + drug.name + " for œ" + nice_number(price) + "/unit? (CR: max - " + drug.inventory + "): ");

            var sell_amt = console.getnum(drug.inventory);

            if (sell_amt === 0) {
                sell_amt = drug.inventory;
            }

            /* Default to sell all. */

            if (sell_amt != NaN & sell_amt >= 1) {
                var cost = sell_amt * price;
                var sell_confirm = console.yesno("Sell " + sell_amt + " units for œ" + nice_number(cost));

                if (sell_confirm) {
                    console.writeln("You get paid for the transaction.");

                    var player_drug = get_player_drug(drug.name);
                    player_drug.inventory -= sell_amt;

                    druglord_state['cash'] += cost;
                }
            }
        }
    }
}