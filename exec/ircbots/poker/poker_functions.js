//////////////////// Non-object Functions ////////////////////
function poker_deal_hole_cards(target,srv) {
	var poker_game=poker_games[target];
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
	var current_turn=poker.turn;
	poker.users[poker.users_map[current_turn]].has_bet=true;
	poker.turn=get_next_player(poker,poker.turn);
	var next_turn_user=poker.users[poker.users_map[poker.turn]];
	
	if(poker_count_active_players(poker)==1) {
		poker.round=3;
		poker.deal_next=true;
	} else	if(poker.current_bet==next_turn_user.bet && next_turn_user.has_bet) {
		poker.deal_next=true;
	}
	if(poker.deal_next) {
		poker_load_pot(poker);
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
			default:
				poker_compare_hands(target,srv);
				break;
		}
		poker.current_bet=0;
		poker.deal_next=false;
		if(poker.round<4) {
			poker.turn=get_next_player(poker,poker.dealer);
			poker_game_status(target,srv);
		}
	}
	if(poker.round<4) {
		poker_prompt_player(target,srv);
	}
}

function poker_count_active_players(poker_game) {
	var count=0;
	for(var u in poker_game.users) {
		if(poker_game.users[u].active) count++;
	}
	return count;
}

function poker_verify_game_status(poker_game) {
	var active_users=false;
	for(var u in srv.users) {
		if(poker.users[u] && poker.users[u].active) {
			if(time()-srv.users[u].last_spoke<activity_timeout){
				active_users=true;
				break;
			} else {
				delete poker.users[u];
				srv.o(u,"You have been idle too long. "
					+ "Type 'DEAL' here to resume playing poker.", "NOTICE");
			}
		}
	}
	if(!active_users) return;
}

function poker_fold_player(target,srv,onick) {
	poker_games[target].pot+=poker_games[target].users[onick].bet;
	poker_games[target].users[onick].active=false;
	srv.o(target, onick + " folded their hand.");
	poker_next_turn(target,srv);
}

function get_next_player(poker_game,turn) {
	turn++;
	if(turn==poker_game.users_map.length) turn=0;
	if(!poker_game.users[poker_game.users_map[turn]].active) {
		turn=get_next_player(poker_game,turn);
	}
	return turn;
}

function poker_compare_hands(target,srv) {
	var poker=poker_games[target];
	var winning_hand=false;
	var winning_player=false;
	for(var p in poker.users) {
		var player=poker.users[p];
		if(player.active) {
			var hand=poker.community_cards.concat(player.cards)
			var ranked_hand=Rank(hand);
			if(!winning_hand) {
				winning_hand=ranked_hand;
				winning_player=p;
			} else if(ranked_hand.rank>winning_hand.rank) {
				winning_hand=ranked_hand;
				winning_player=p;
			} else if(ranked_hand.rank==winning_hand.rank) {
				for(v=0;v<ranked_hand.high.length;v++) {
					if(ranked_hand.high[v]>winning_hand.high[v]) {
						winning_hand=ranked_hand;
						winning_player=p;
						break;
					}
				}
			}
		}
	}
	srv.o(target,winning_player + " won this hand with " + winning_hand.str + "!");
	srv.o(winning_player,"Winnings: $" + poker.pot);
	poker.users[winning_player].money+=poker.pot;
	poker.winner=winning_player;

}

function poker_deal_flop(target,srv) { 
	var poker_game=poker_games[target];
	burn_card(poker_game.deck);
	poker_game.community_cards[0] = poker_game.deck.deal();
	poker_game.community_cards[1] = poker_game.deck.deal();
	poker_game.community_cards[2] = poker_game.deck.deal();
}

function burn_card(deck) {
	deck.ptr++;
}

function poker_deal_turn(target,srv) {
	var poker_game=poker_games[target];
	burn_card(poker_game.deck);
	poker_game.community_cards[3] = poker_game.deck.deal();
}

function poker_deal_river(target,srv) {
	var poker_game=poker_games[target];
	burn_card(poker_game.deck);
	poker_game.community_cards[4] = poker_game.deck.deal();
}

function poker_show_card(card) {
	return(card.color + "[ " + card.char + " ] ");
}

function poker_load_pot(poker_game) {
	for(var p in poker_game.users) {
		poker_game.pot+=poker_game.users[p].bet;
		poker_game.users[p].bet=0;
		poker_game.users[p].has_bet=false;
	}
	return;
}

function poker_prompt_player(target,srv) {
	var poker=poker_games[target];
	var turn=poker.users_map[poker.turn];
	var min_bet=poker.current_bet-poker.users[turn].bet;
	if(min_bet<0) min_bet=0;
	srv.o(turn,"It is your turn. Minimum bet to call: $" + min_bet,"NOTICE");
}

function poker_verify_user_status(target,srv,onick) {
	var poker=poker_games[target];
	if (!poker) {
		srv.o(target, "No poker game in progress. Type '" + get_cmd_prefix()
			+ "DEAL' to start a new one.")
		return false;
	} 
	if(poker.round<0) {
		srv.o(target, onick + ", the game hasn't started yet.");
		return false;
	}
	if(!poker.users[onick] || !poker.users[onick].active) {
		srv.o(onick, "You're not even in the hand!");
		return false;
	}
	var turn_player=poker.users_map[poker.turn];
	if (turn_player != onick) {
		srv.o(target, "Acting out of turn?");
		return false;
	}
	return true;
}

function poker_init_hand(target,srv) {
	var poker=poker_games[target];
	poker.deck.shuffle();
	for(var u in poker.users) {
		poker.users_map.push(u);
	}
	var small_blind=get_next_player(poker,poker.dealer);
	var large_blind=get_next_player(poker,small_blind);
	
	srv.o(target,poker.users_map[poker.dealer] + " is the dealer for this hand.");
	poker.users[poker.users_map[small_blind]].bet+=poker.sm_blind;
	poker.users[poker.users_map[small_blind]].money-=poker.sm_blind;
	srv.o(target,"Small blind: " + poker.users_map[small_blind]  + " $" + poker.sm_blind);
	poker.users[poker.users_map[large_blind]].bet+=poker.lg_blind;
	poker.users[poker.users_map[large_blind]].money-=poker.lg_blind;
	srv.o(target,"Large blind: " + poker.users_map[large_blind]  + " $" + poker.lg_blind);
	poker.turn=get_next_player(poker,large_blind);
}

function load_scores() {
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

function poker_game_status(target,srv) {
	var poker=poker_games[target];
	var state="";
	var cards="";
		
	for(var c in poker.community_cards) {
		if(poker.community_cards[c]) cards+=poker_show_card(poker.community_cards[c]);
	}
	switch(poker.round) {
		case -1:
			state="Status: waiting for players";
			break;
		case 0:
			state="Status: First round betting";
			break;
		case 1:
			state="The Flop: " + cards;
			break;
		case 2:
			state="The Turn: " + cards;
			break;
		case 3:
			state="The River: " + cards;
			break;
		case 4:
			state="Hand complete. Winner: " + poker.winner;
			break;
		default:
			break;
	}
	if(poker.round>=0 && poker.round<4) {
		srv.o(target,"Current pot: $" + poker.pot);
		srv.o(target,"Current bet: $" + poker.current_bet);
		srv.o(target,"Current turn: " + poker.users_map[poker.turn]);
	}
	srv.o(target,state);
}

function poker_reset_game(target,srv) {
	var poker=poker_games[target];
	for(var u in poker.users) {
		poker.users[u].cards=[];
		poker.users[u].bet=0;
		poker.users[u].active=true;
		poker.users[u].has_bet=false;
	}
	poker.dealer=get_next_player(poker,poker.dealer);
	poker.round=-1;
	poker.current_bet=poker.lg_blind;
	poker.community_cards=[];
	poker.pot=0;
}



