function atm() {
    console.write(ANSI.DEFAULT);
    console.crlf();
    console.crlf();
    /* TODO: Random ATM bank names. */
    console.write(ANSI.BOLD + ANSI.FG_GREEN)
    console.writeln("\t*****************************");
    console.writeln("\t* LONDON THIRD NATIONAL ATM *");
    console.writeln("\t*****************************");
    console.write(ANSI.DEFAULT)
    console.crlf();
    console.writeln(" 1. Repay debt.\t\tOutstanding:\tœ" + nice_number(druglord_state['debt']) + ".");
    console.writeln(ANSI.FG_WHITE + " 2. Deposit funds.\tBalance:\tœ" + nice_number(druglord_state['bank']) + ".");
    console.writeln(ANSI.FG_WHITE + " 3. Withdraw funds.");
    console.writeln(ANSI.FG_WHITE + " 4. Borrow money.");
    console.crlf();
    console.write(ANSI.BOLD + ANSI.FG_WHITE + "\tChoice: ");
    var atm_cmd = console.getnum(4);

    if (atm_cmd != undefined) {
        switch (atm_cmd) {
            case 1:
                /* Repay debt. */
                var max_repayment = Math.min(druglord_state['debt'], druglord_state['cash']);

                console.write(ANSI.BOLD + ANSI.FG_WHITE + "\tRepay how much? (œ" + nice_number(max_repayment) + " max):");
                var atm_repay = console.getnum(max_repayment);

                /*if (atm_repay === 0) {
                    atm_repay = max_repayment;
                }*/

                var atm_confirm = console.yesno("Repay œ" + nice_number(atm_repay) + " of your debt");

                if (atm_confirm) {
                    console.writeln(" Repaying: œ" + nice_number(atm_repay));

                    druglord_state['debt'] -= atm_repay;
                    druglord_state['cash'] -= atm_repay;
                }

                console.pause();

                break;

            case 2:
                /* Deposit funds into account. */
                console.write(ANSI.BOLD + ANSI.FG_WHITE + "\tDeposit how much? (œ" + nice_number(druglord_state['cash']) + " max):");
                var atm_deposit = console.getnum(druglord_state['cash']);

                /*if (atm_deposit === 0) {
                    atm_deposit = druglord_state['cash'];
                }*/

                var atm_confirm = console.yesno("Deposit œ" + nice_number(atm_deposit) + " into your account");

                if (atm_confirm) {
                    console.writeln(" Deposited: œ" + nice_number(atm_deposit));

                    druglord_state['bank'] += atm_deposit;
                    druglord_state['cash'] -= atm_deposit;
                }

                console.pause();

                break;

            case 3:
                /* Withdraw funds. */
                console.write(ANSI.BOLD + ANSI.FG_WHITE + "\tWithdraw how much? (œ" + nice_number(druglord_state['bank']) + " max):");
                var atm_withdraw = console.getnum(druglord_state['bank']);

                if (atm_withdraw === 0) {
                    atm_withdraw = druglord_state['bank'];
                }

                var atm_confirm = console.yesno("Withdraw œ" + nice_number(atm_withdraw) + " from your account");
                if (atm_confirm) {
                    console.writeln(" Withdrew: œ" + nice_number(atm_withdraw));

                    druglord_state['bank'] -= atm_withdraw;
                    druglord_state['cash'] += atm_withdraw;

                }

                console.pause();

                break;

            case 4:
                /* Borrow money. */
                var borrow_max = Math.max(0, druglord_config['MAX_LOAN'] - druglord_state['debt']);

                console.write(ANSI.BOLD + ANSI.FG_WHITE + "\tBorrow how much? (œ" + nice_number(borrow_max) + " max):");
                var atm_borrow = console.getnum(borrow_max);

                /*if (atm_borrow === 0) {
                    atm_borrow = borrow_max;
                }*/

                var atm_brw_conf = console.noyes("Borrow œ" + nice_number(atm_borrow));
                if (atm_brw_conf == false) {
                    console.writeln(" Borrowed: œ" + nice_number(atm_borrow));

                    druglord_state['debt'] += atm_borrow;
                    druglord_state['cash'] += atm_borrow;

                    console.writeln(" Total debt: œ" + nice_number(druglord_state['debt']));

                }

                console.pause();

                break;
        }
    }

    console.write(ANSI.DEFAULT);
    console.crlf();
    console.crlf();
}