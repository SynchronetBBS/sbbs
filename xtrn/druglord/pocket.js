function buy_pockets() {
    var pockets = (random(3) + 1) * 10;
    var price = 1000;
    console.writeln("*Pssst* Hey--do you wanna buy some pocketspace?");

    var max_pockets = Math.min(pockets, Math.floor(druglord_state['cash'] / price));

    console.write("Buy pocketspace? (" + max_pockets + " max): ");

    var buy_amt = console.getnum(max_pockets);

    /*if (buy_amt === 0) {
        buy_amt = max_pockets;
    }*/

    if (buy_amt != undefined && buy_amt > 0) {

        var buy_cfm = console.noyes("Buy " + buy_amt + " pocketspace for œ" + nice_number(buy_amt * price));

        if (buy_cfm == false) {
            /* Increase pocket size. */
            console.crlf();
            console.writeln("You fit your clothes with more pockets.");
            console.crlf();
            druglord_state['inventory_size'] += buy_amt;
            druglord_state['cash'] -= buy_amt * price
        }
    }
    
}