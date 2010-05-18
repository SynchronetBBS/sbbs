/* 	IRC Bot Module - Server Commands
	You would place all of your module functions in this file.	*/
	
this.Server_command=function(srv,cmdline,onick,ouh) 
{
	var cmd=IRC_parsecommand(cmdline);
	switch (cmd[0]) {
		case "JOIN":
			if (onick == srv.curnick) break;
			
			// Someone else joining? Let's send them a private welcome message!
			srv.o(onick,"Welcome to Poker!");
			srv.o(onick,"This is a module for IRCBot - by Cyan");
			break;
		case "PRIVMSG":
			if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
				var chan = srv.channel[cmd[1].toUpperCase()];
				if (!chan)
					break;
				if (!chan.is_joined)
					break;
				if(srv.users[onick.toUpperCase()]) {
					/* 	You can do special command processing here, if you like.
						This is currently set up to parse public room messages for
						things like trivia answers, or other responses that
						are inconvenient for users to submit with a command prefix */
				}
			}
			break;
		default:
			break;
	}
}

//////////////////// Non-object Functions ////////////////////
function poker_deal_hole_cards(target,srv) {
	var poker_game=poker_games[target];
	poker_game.round = 1;
	for (p in poker_game.users) {
		poker_game.users[p].cards[0] = poker_game.deck.deal();
		poker_game.users[p].cards[1] = poker_game.deck.deal();
		srv.o(p,"Your hole cards: " 
			+ poker_game.users[p].cards[0].color + "[ "
			+ poker_game.users[p].cards[0].char + " ] "
			+ poker_game.users[p].cards[1].color + "[ "
			+ poker_game.users[p].cards[1].char + " ]"
		,"NOTICE");
	}
}

function poker_next_turn(target,srv) {
	var poker=poker_games[target];
	poker.turn++;
	if(poker.turn==poker.users_map.length) poker.turn=0;
	
	if(poker.deal_next) {
		poker_load_pot(target,srv);
		switch(++poker.round) {
			case 1:
				poker_deal_flop(target,srv);
				break;
			case 2:
				poker_deal_turn(target,srv);
				break;
			case 3:
				poker_deal_river(target,srv);
				break;
		}
		poker.deal_next=false;
	} else {
		var turn_user=poker.users[poker.users_map[poker.turn]];
		if(poker.current_bet==turn_user.bet) poker.deal_next=true;
	}
	poker_prompt_player(target,srv);
}

function poker_deal_flop(target,srv) { 
	var poker_game=poker_games[target];
	poker_game.round = 2;
	poker_game.community_cards[0] = poker_game.deck.deal();
	poker_game.community_cards[1] = poker_game.deck.deal();
	poker_game.community_cards[2] = poker_game.deck.deal();
	srv.writeout("PRIVMSG " + target + " :The Flop: " +
	poker_show_card(poker_game.community_cards[0]) +
	poker_show_card(poker_game.community_cards[1]) +
	poker_show_card(poker_game.community_cards[2]));
}

function poker_deal_turn(target,srv) {
	var poker_game=poker_games[target];
	poker_game.round = 3;
	poker_game.community_cards[3] = poker_game.deck.deal();
	srv.writeout("PRIVMSG " + target + " :The Turn: " + 
	poker_show_card(poker_game.community_cards[0]) +
	poker_show_card(poker_game.community_cards[1]) +
	poker_show_card(poker_game.community_cards[2]) +
	poker_show_card(poker_game.community_cards[3]));
}

function poker_deal_river(target,srv) {
	var poker_game=poker_games[target];
	poker_game.round = 4;
	poker_game.community_cards[4] = poker_game.deck.deal();
	srv.writeout("PRIVMSG " + target + " :The River: " + 
	poker_show_card(poker_game.community_cards[0]) +
	poker_show_card(poker_game.community_cards[1]) +
	poker_show_card(poker_game.community_cards[2]) +
	poker_show_card(poker_game.community_cards[3]) +
	poker_show_card(poker_game.community_cards[4]));
}

function poker_show_card(card) {
	return(card.color + "[ " + card.char + " ]");
}

function poker_load_pot(target,srv) {
	var poker=poker_games[target];
	for(var p in poker.users) {
		poker.pot+=poker.users[p].bet;
		poker.users[p].bet=0;
	}
	srv.o(target,"Current pot: $" + poker.pot);
	return;
}

function poker_prompt_player(target,srv) {
	var poker=poker_games[target];
	var turn=poker.users_map[poker.turn];
	srv.writeout("NOTICE " + turn + " :" + "It is your turn. You may CHECK,  CALL, BET, RAISE or FOLD");
	srv.writeout("NOTICE " + turn + " :" + "Minimum bet: $" + poker.current_bet);
}

function poker_verify_game_status(target,srv,onick) {
	var poker=poker_games[target];
	if (!poker) {
		srv.o(target, "No poker game in progress. Type '" + get_cmd_prefix() + "DEAL' to "
			+ "start a new one.");
		return false;
	}
	if(!poker.users[onick.toUpperCase()]) {
		srv.o(onick, "You're not even in the hand!");
		return false;
	}
	var turn_player=poker.users_map[poker.turn];
	if (turn_player != onick.toUpperCase()) {
		srv.o(target, "Acting out of turn?");
		return false;
	}
	return true;
}

function poker_init_hand(target) {
	poker_games[target].deck.shuffle();
	for(var u in poker_games[target].users) {
		poker_games[target].users_map.push(u);
	}
}

function load_scores()
{
	var s_file=new File(poker_dir + "scores.ini");
	if(s_file.open(file_exists(s_file.name)?"r+":"w+")) {
		writeln("reading scores from file: " + s_file.name);
		var players=s_file.iniGetKeys();
		for(var p in players) {
			writeln("loading player score: " + players[p]);
			poker_scores[players[p]]=s_file.iniGetValue(null,players[p]);
		}
		s_file.close();
	}
}

