Bot_Commands["DEAL"] = new Bot_Command(0,false,false);
Bot_Commands["DEAL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if (!poker_games[target]) {
		srv.o(target, onick + " just started a new poker hand.  'DEAL' to join.");
		poker_games[target] = new Poker_Game();
		poker_games[target].users[onick]=new Poker_Player();
	} else	if (poker_games[target].users[onick]) {
		if(poker_games[target].users[onick].active) 
			srv.o(target, onick + ", you're already in the hand. Relax, don't do it.");
		else
			srv.o(target, onick + ", you already folded. Wait until the next hand.");
	} else {
		poker_games[target].users[onick]=new Poker_Player();
		srv.o(target, onick + " has been dealt in for this hand.");
	}
	return;
}

Bot_Commands["GO"] = new Bot_Command(0,false,false);
Bot_Commands["GO"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if (!poker_games[target]) {
		srv.o(target, "No poker game to 'GO' with. Type '" + get_cmd_prefix() + "DEAL' to "
			+ "start a new one.");
		return;
	} else if(poker_games[target].round>3) {
		srv.o(target, "Starting new poker hand.");
		poker_reset_game(target,srv);
	}else if(poker_games[target].round>=0) {
		srv.o(target, "This hand has already started.");
		return;
	}
	if(true_array_len(poker_games[target].users)==1) {
		srv.o(target, "At least two players are necessary to start the game.");
		return;
	}
	poker_games[target].round = 0;
	poker_init_hand(target,srv);
	poker_deal_hole_cards(target,srv);
	poker_prompt_player(target,srv);
	return;
}

Bot_Commands["FOLD"] = new Bot_Command(0,false,false);
Bot_Commands["FOLD"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_verify_user_status(target,srv,onick)) return;
	poker_fold_player(target,srv,onick);
	return;
}

Bot_Commands["CHECK"] = new Bot_Command(0,false,false);
Bot_Commands["CHECK"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_verify_user_status(target,srv,onick)) return;
	var poker=poker_games[target];
	var current_player=poker.users[onick];
	if(poker.current_bet>current_player.bet) {
		srv.o(onick,"You have not met the current bet: $" + poker.current_bet,"NOTICE");
		srv.o(onick,"Your current bet: $" + current_player.bet,"NOTICE");
		return;
	}
	srv.o(target,onick + " checks.");
	poker_next_turn(target,srv);
	return;
}

Bot_Commands["BET"] = new Bot_Command(0,false,false);
Bot_Commands["BET"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_verify_user_status(target,srv,onick)) return;
	if(!cmd[1]) {
		srv.o(onick,"You must specify an amount to bet!","NOTICE");
		return;
	}
	if(isNaN(cmd[1])) {
		srv.o(onick,"That's not money!","NOTICE");
		return;
	}
	var bet=Number(cmd[1]);
	if(bet<0) {
		srv.o(onick,"You must bet a positive number!","NOTICE");
		return;
	}
	var poker=poker_games[target];
	var current_player=poker.users[onick];
	var difference=poker.current_bet-current_player.bet;
	if(bet>current_player.money) {
		srv.o(onick,"You don't have that much money! Balance: $" + current_player.money,"NOTICE");
		return;
	}
	if(difference==0 && bet<poker.min_bet) {
		srv.o(onick,"Your bet must meet the minimum! Minimum bet: $" + poker.min_bet,"NOTICE");
		return;
	}
	srv.o(target,onick + " bets $" + bet);
	current_player.money-=bet;
	current_player.bet+=bet;
	poker.current_bet+=bet-difference;
	srv.o(onick,"Balance: $" + current_player.money,"NOTICE");
	poker_next_turn(target,srv);
	return;
}

Bot_Commands["CALL"] = new Bot_Command(0,false,false);
Bot_Commands["CALL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_verify_user_status(target,srv,onick)) return;
	var poker=poker_games[target];
	var current_player=poker.users[onick];
	
	if(poker.current_bet==current_player.bet) {
		srv.o(target,onick + " checks.");
	} else if(poker.current_bet>current_player.money+current_player.bet) {
		srv.o(onick,"You don't have enough to call!","NOTICE");
		srv.o(onick,"Balance: $" + current_player.money,"NOTICE");
		return;
	} else {
		var difference=poker.current_bet-current_player.bet;
		srv.o(target,onick + " calls the bet: $" + poker.current_bet);
		current_player.money-=difference;
		current_player.bet+=difference;
		srv.o(onick,"Balance: $" + current_player.money,"NOTICE");
	}
	poker_next_turn(target,srv);
	return;
}

Bot_Commands["RAISE"] = new Bot_Command(0,false,false);
Bot_Commands["RAISE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_verify_user_status(target,srv,onick)) return;
	var bet=Number(cmd[1]);
	if(!bet) {
		srv.o(onick,"You must specify an amount to raise!","NOTICE");
		return;
	}
	if(isNaN(bet)) {
		srv.o(onick,"That's not money!","NOTICE");
		return;
	}
	if(bet<0) {
		srv.o(onick,"You must bet a positive number!","NOTICE");
		return;
	}
	var poker=poker_games[target];
	var difference=poker.current_bet-poker.users[onick].bet;
	if(difference+bet>poker.users[onick].money) {
		srv.o(onick,"You don't have that much money! Balance: $" + poker.users[onick].money,"NOTICE");
		return;
	}
	poker.users[onick].money-=bet;
	poker.users[onick].bet+=difference;
	poker.users[onick].bet+=bet;
	poker.current_bet+=bet;
	srv.o(target,onick + " raises the bet to $" + (poker.current_bet));
	srv.o(onick,"Balance: $" + poker.users[onick].money,"NOTICE");
	poker_next_turn(target,srv);
	return;
}

Bot_Commands["STATUS"] = new Bot_Command(0,false,false);
Bot_Commands["STATUS"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_games[target]) {
		srv.o(target,"No poker game in progress. 'DEAL' to start one.");
		return;
	}
	poker_game_status(target,srv);
	return;
}

Bot_Commands["LIST"] = new Bot_Command(0,false,false);
Bot_Commands["LIST"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if(!poker_games[target]) {
		srv.o(target,"There is no active game.");
		return;
	}
	var list="";
	for(u in poker_games[target].users) {
		list+=" "+u;
	}
	srv.o(target,"Poker players:" + list);
	return;
}

Bot_Commands["SHOW"] = new Bot_Command(0,false,false);
Bot_Commands["SHOW"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!poker_games[target]) {
		srv.o(target,"There is no active game.");
		return;
	}
	if(!poker_games[target].users[onick]) {
		srv.o(target, onick + ", you aren't playing this game.");
		return;
	}
	if(poker_games[target].round<0) {
		srv.o(target, onick + ", the game hasn't started yet.");
		return;
	}
	srv.o(target, onick + " shows: "
		+ poker_show_card(poker_games[target].users[onick].cards[0])
		+ poker_show_card(poker_games[target].users[onick].cards[1])
	);
}

Bot_Commands["HOLE"] = new Bot_Command(0,false,false);
Bot_Commands["HOLE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!poker_games[target]) {
		srv.o(target,"There is no active game.");
		return;
	}
	if(!poker_games[target].users[onick]) {
		srv.o(target, onick + ", you aren't playing this game.");
		return;
	}
	if(poker_games[target].round<0) {
		srv.o(target, onick + ", the game hasn't started yet.");
		return;
	}
	srv.o(onick, "Your hole cards: "
		+ poker_show_card(poker_games[target].users[onick].cards[0])
		+ poker_show_card(poker_games[target].users[onick].cards[1])
	,"NOTICE");
}

Bot_Commands["INVITE"] = new Bot_Command(0,1,false);
Bot_Commands["INVITE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(srv.users[cmd[0].toUpperCase()]) {
		srv.o(target,cmd[0] + " is already here!");
		return;
	}
	srv.o(cmd[0],onick + " invited you to play poker in " + target
		+ "! /J " + target + " to join.");
	return;
}

Bot_Commands["BALANCE"] = new Bot_Command(0,false,false);
Bot_Commands["BALANCE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!poker_games[target]) {
		srv.o(target,"There is no active game.");
		return;
	}
	if(!poker_games[target].users[onick]) {
		srv.o(target, onick + ", I have no record for you.");
		return;
	}
	srv.o(onick, "Your balance: $" + poker_games[target].users[onick].money,"NOTICE");
	return;
}

Bot_Commands["BUYIN"] = new Bot_Command(50,false,false);
Bot_Commands["BUYIN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	if (!poker_games[target]) {
		srv.o(target,"There is no active game.");
		return;
	}
	if(!poker_games[target].users[onick]) {
		srv.o(target, onick + ", I have no user record for you.");
		return;
	}
	if(poker_games[target].users[onick].money>10) {
		srv.o(target, onick + ", you already have money in the bank.");
		return;
	}
	poker_games[target].users[onick].money=100;
	srv.o(onick, "Your balance: $" + poker_games[target].users[onick].money,"NOTICE");
	return;
}

