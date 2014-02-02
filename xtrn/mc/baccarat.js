/*
{                             Milliways Casino                               }
{                  Copyright (C) 1987 by Charles Ezzell & Matthew Warner     }
{                            All Rights Reserved                             }
{                                                                            }
{                                                                            }
*/

function baccarat_init_deck()
{
	var s1;

	console.print('New Deck\r\n');
	deck=[
			 [0,0,0,0,0,0,0,0,0,0,0,0,0]
			,[0,0,0,0,0,0,0,0,0,0,0,0,0]
			,[0,0,0,0,0,0,0,0,0,0,0,0,0]
			,[0,0,0,0,0,0,0,0,0,0,0,0,0]];
}

function baccarat_bet()
{
	var bac_bet, temp_money;

	check_winnings();
	check_random();
	if(stranger_dates_kathy) {
		console.print('You look around, and seeing no one else to play with, you get up\r\n');
		console.print('and leave the room.\r\n');
		return(false);
	}
	if(player.bruno > 0)
		buy_from_bruno();
	if(stranger.strangers_money <= 100)
		strangers_gets_more_money();
	do {
		this.betamount=0;
		console.print('The stranger has '+format_money(stranger.strangers_money)+'.\r\n');
		console.print(player.name+', you have '+format_money(player.players_money)+'.\r\n');
		console.print('S for your stats.\r\n');
		console.print('Your bet (Q to quit):');
		bac_bet=console.getkeys('SQ', stranger.strangers_money);
		switch(bac_bet) {
			case 'S':
				player_stats();
				break;
			case 'Q':
				return(false);
			default:
				this.betamount=parseInt(bac_bet);
				if(isNaN(this.betamount) || this.betamount > stranger.strangers_money) {
					console.print("You can't bet that much.\r\n");
					this.betamount=0;
				}
				break;
		}
	} while(isNaN(this.betamount) || this.betamount==0);
	temp_money=player.players_money-this.betamount;
	if(temp_money < 0)
		temp_money=sell_to_bruno(temp_money);
	this.banco=false;
	if(this.betamount==stranger.strangers_money) {
		this.banco=true;
		console.crlf();
		console.print('B A N C O !\r\n');
		console.crlf();
		console.print('The stranger looks up at you and smiles.  He has been very\r\n');
		console.print('lucky today, having broken 5 other players.  You look him\r\n');
		console.print('in the eye, and smile back at him.\r\n');
		console.crlf();
		console.crlf();
		console.pause();
	}
	return(true);
}

function baccarat_new_card(forplayer)
{
	var c,d;

	do {
		c=random(4);
		d=random(13);
		deck[c][d]++;
	} while(deck[c][d] > /* Was 32! */ 9);
	if(forplayer) {
		this.player_card_name.push(card[d]+' of '+suit[c]);
		this.player_count += this.cardvalue[d];
		this.player_count %= 10;
	}
	else {
		this.banker_card_name.push(card[d]+' of '+suit[c]);
		this.banker_count += this.cardvalue[d];
		this.banker_count %= 10;
	}
	return(this.cardvalue);
}

function baccarat_ask_for_card()
{
	var i;

	console.print("You have a total of '5'\r\n");
	console.print('You have the option of drawing another card\r\n');
	console.print('Do you want to draw another card? ');
	if(console.getkeys('YN')=='Y') {			// Avoids default of no
		this.gave=this.new_card(true);
		for(i in this.player_card_name)
			console.print(this.player_card_name[i]+'\r\n');
	}
}

function baccarat_instructions()
{
	console.print('   Baccarat is a very popular game in Las Vegas.  The player\r\n');
	console.print("and banker each receive two cards from a 'shoe' containing\r\n");
	console.print('8 decks of cards.  All card combinations totaling ten are\r\n');
	console.print('not counted.  The one that ends up closer to nine wins.  The\r\n');
	console.print('stakes are high.  You can bet any amount, as long as you have\r\n');
	console.print('the funds to cover it.  A third card is given only under\r\n');
	console.print('certain conditions as you will see.\r\n');
	console.crlf();
	console.crlf();
	console.print('   Games of the baccarat and chemin de fer family originated\r\n');
	console.print('in the baccarat that became popular in the French casinos in\r\n');
	console.print("the 1830's.  In the present century they have travelled from\r\n");
	console.print('Europe to the United States, back to Europe, and to casinos\r\n');
	console.print('throughout the world.  This process has resulted in wide\r\n');
	console.print("variations in playing rules and what is called 'baccarat' in\r\n");
	console.print("one casino may more nearly resemble the 'chemin de fer' of another.\r\n");
	console.crlf();
	console.crlf();
	console.pause();
	console.print('   The computer game here is more nearly chemin de fer than it\r\n');
	console.print('is baccarat.  The rules are as follows:  Eight packs of cards\r\n');
	console.print("are shuffled together and placed in a 'shoe' from which the\r\n");
	console.print('cards can be slid out one by one.  Following this, you may make\r\n');
	console.print('your bet.  You may bet up to the total amount that the banker has.\r\n');
	console.print("This is called 'Banco', and the 'Banker' will cover\r\n");
	console.print("the bet.  If you declare 'Banco' and win, the game is over.\r\n");
	console.print("since you will have 'broken' the bank.  If you lose, well,\r\n");
	console.print("that's it for you!\r\n");
	console.crlf();
	console.crlf();
	console.print('   After the bets are placed, the banker deals two hands of\r\n');
	console.print('two cards each, dealing one card at a time.\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print('   The object of the game is to hold two or three cards which\r\n');
	console.print('count nine or as nearly nine as possible.  The values of the cards\r\n');
	console.print('are: face cards and tens=zero; aces=one each;any other card=face value.\r\n');
	console.print('Units of ten points are disregarded so that nine plus seven\r\n');
	console.print('is six and not sixteen.\r\n');
	console.crlf();
	console.crlf();
	console.print('   A player whose card is nine or eight in his first two cards\r\n');
	console.print('shows his hand immediately.  He has a natural and his hand\r\n');
	console.print('wins .. a natural nine beats a natural eight.  Naturals of the\r\n');
	console.print('same number tie and there is a new deal.\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print('   When the result is not decided by a natural, the banker\r\n');
	console.print('must give a card to his opponent on request or the opponent\r\n');
	console.print('may stand.  The opponent MUST stand on six or seven, MUST draw\r\n');
	console.print('to a zero, one, two, three, or four but has the option on five.\r\n');
	console.print('The additional card if given is face up.\r\n');
	console.crlf();
	console.crlf();
	console.print('   Then the banker decides whether to stand or take a card:\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print('IF BANKER GIVES      BANKER STANDS ON     BANKER DRAWS TO\r\n');
	console.print('Face card or ten     4,5,6,7              3,2,1,0\r\n');
	console.print('Nine                 4,5,6,7 (or 3)       2,1,0 (or 3)\r\n');
	console.print('Eight                3,4,5,6,7            2,1,0\r\n');
	console.print('Seven or six         7                    6,5,4,3,2,1,0\r\n');
	console.print('Five or four         6,7                  5,4,3,2,1,0\r\n');
	console.print('Three or two         5,6,7                4,3,2,1,0\r\n');
	console.print('Ace                  4,5,6,7              3,2,1,0\r\n');
	console.print('Opponent stands      6,7                  5,4,3,2,1,0\r\n');
	console.crlf();
	console.pause();
	console.print('   Neither player may have more than one additional card giving\r\n');
	console.print('him three cards at the most.  When each player has exercised\r\n');
	console.print('his option, the cards are shown.  If the totals are the same\r\n');
	console.print('the bets are off and may be withdrawn and new bets are placed\r\n');
	console.print('exactly as before for another deal.  If you have a\r\n');
	console.print("higher number than the banker's, then you win.\r\n");
	console.crlf();
	console.print('Good luck and have fun!\r\n');
	console.crlf();
	console.crlf();
	console.pause();
}

function baccarat_welcome()
{
	console.print('  As you enter the baccarat room, you notice how quiet things\r\n');
	console.print('seem to be.  There are only 8 tables in this room, and you see\r\n');
	console.print('huge amounts of money being lost and won.  You observe as one\r\n');
	console.print('player plays against the stranger you saw in one of the other\r\n');
	console.print("rooms.  The player declares 'BANCO', keying in his total\r\n");
	console.print('amount on his credit card.  1 minute later, you watch him leave,\r\n');
	console.print('crying.  He has just lost everything he owned to the stranger.\r\n ');
	console.print('You sit down, thinking you can do better.\r\n');
	console.crlf();
	console.crlf();
	console.pause();
}

function baccarat_play()
{
	var banker_draws, tie, i, natural, banker_natural, draw_max, x, y, new_deck;

	this.welcome();
	console.print('Do you wish instructions? ');
	if(yn())
		this.instructions();
	this.init_deck();
	while(1) {
		banker_draws=false;
		this.gave=5;
		tie=false;
		this.banker_card_name=[];
		this.player_card_name=[];

		/* Deal */
		this.player_count = 0;
		this.banker_count = 0;
		for(i=0; i<4; i++)
			this.new_card(i%2);

		if(!this.bet())
			break;

		/* Banker checks for natural */
		if(this.banker_count >= 8) {
			banker_natural=true;
			console.print('Banker (Natural)\r\n');
			for(i in this.banker_card_name)
				console.print(this.banker_card_name[i]+'\r\n');
			console.print('Total:'+this.banker_count);
			console.crlf();
			console.pause();
			console.print('The banker has: Natural '+this.banker_count+'\r\n');
			console.crlf();
			console.pause();
		}

		/* Player plays */
		console.crlf();
		console.print('Player\r\n');
		for(i in this.player_card_name)
			console.print(this.player_card_name[i]+'\r\n');
		console.print('Total:'+this.player_count);
		console.crlf();
		console.pause();
		natural=false;
		switch(this.player_count) {
			case 5:
				if(banker_natural)
					console.print('Player cannot draw because the Banker got a natural.\r\n');
				else
					this.ask_for_card();
				break;
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				if(banker_natural)
					console.print('Player cannot draw because the Banker got a natural.\r\n');
				else {
					console.print('Player must draw.\r\n');
					this.gave=this.new_card(true);
					for(i in this.player_card_name)
						console.print(this.player_card_name[i]+'\r\n');
				}
				break;
			case 8:
			case 9:
				/* Natural */
				natural=true;
				break;
			default:
				console.print('Player cannot draw.');
				if(banker_natural)
					console.print(' (Banker got a natrual)');
				console.print('\r\n');
				break;
		}
		console.print("Player's total: "+(natural?'Natural ':'')+this.player_count+"\r\n");
		console.crlf();
		console.pause();

		/* Banker plays */
		if(!banker_natural) {
			console.print('Banker\r\n');
			for(i in this.banker_card_name)
				console.print(this.banker_card_name[i]+'\r\n');
			console.print('Total:'+this.banker_count);
			console.crlf();
			console.pause();
			if(this.banker_count < 3)
				banker_draws=true;
			if(!banker_draws) {
				switch(this.gave) {
					case 10:
					case 1:
						if(this.banker_count  < 4)
							banker_draws=true;
						break;
					case 2:
					case 3:
						if(this.banker_count  < 5)
							banker_draws=true;
						break;
					case 4:
					case 5:
						if(this.banker_count  < 6)
							banker_draws=true;
						break;
					case 7:
					case 6:
						if(this.banker_count  < 7)
							banker_draws=true;
						break;
					case 8:
					case 9:
						if(this.banker_count  < 3)
							banker_draws=true;
						break;
				}
			}
			if(natural)
				banker_draws=false;
			if(banker_draws) {
				console.print('Banker must draw.\r\n');
				this.new_card(false);
				for(i in this.banker_card_name)
					console.print(this.banker_card_name[i]+'\r\n');
			}
			else if(this.banker_count >= 8) {
				banker_natural=true;
			}
			else {
				console.print('Banker cannot draw.');
				if(natural)
					console.print(' (Player got a natrual)');
				console.print('\r\n');
			}
			console.print('The banker has: '+(banker_natural?'Natural ':'')+this.banker_count+'\r\n');
			console.crlf();
			console.pause();
		}

		if(this.banker_count==this.player_count) {
			console.print("It's a tie.  The hand is played over.\r\n");
			tie=true;
			if(this.banco) {
				console.crlf();
				console.print('The stranger looks up at you, as if he dares you to try\r\n');
				console.print('again.\r\n');
				console.crlf();
				console.crlf();
			}
		}
		if(this.banker_count > this.player_count) {
			console.print('The banker wins!\r\n');
			if(this.banco) {
				console.crlf();
				console.print('The stranger extends his hand to you, but you refuse to shake\r\n');
				console.print('hands with the man that has just turned you into a pauper.\r\n');
				console.crlf();
				console.crlf();
				console.print('You get up, and leave, no looking back at the smiling face\r\n');
				console.print('of the stranger.\r\n');
				console.crlf();
				console.crlf();
			}
			stranger.strangers_money += this.betamount;
			player.players_money -= this.betamount;
			player.won_today -= this.betamount;
			console.print('You lose '+format_money(this.betamount)+', for a total of '+format_money(player.players_money)+'.\r\n');
			if(this.banco) {
				console.crlf();
				break;
			}
		}
		if(this.banker_count < this.player_count) {
			console.print('You win!\r\n');
			if(this.banco) {
				console.crlf();
				console.print('The stranger stands up, and extends his hand to you.  You are\r\n');
				console.print('still shaken by the unexpected win, but shake his hand.  He\r\n');
				console.print('tells you he has enjoyed the game, and maybe he will see you back\r\n');
				console.print('again.\r\n');
				console.crlf();
				console.crlf();
			}
			stranger.strangers_money -= this.betamount;
			player.players_money += this.betamount;
			player.won_today += this.betamount;
			console.print('You win '+format_money(this.betamount)+', for a total of '+format_money(player.players_money)+'.\r\n');
			if(this.banco) {
				console.crlf();
				break;
			}
		}
		console.crlf();
		console.print('---------- New Game ----------\r\n');
		console.crlf();
		for(x in deck) {
			for(y in deck[x]) {
				if(deck[x][y]>=8)
					new_deck=true;
			}
		}
		if(new_deck)
			this.init_deck();
	}
}

function Baccarat()
{
	this.cardvalue=[1,2,3,4,5,6,7,8,9,0,0,0,0];

	this.banco=false;
	this.banker_card_name=new Array();
	this.banker_count=0;
	this.betamount=0;
	this.gave=5;
	this.player_card_name=new Array();
	this.player_count=0;

	this.init_deck=baccarat_init_deck;
	this.bet=baccarat_bet;
	this.new_card=baccarat_new_card;
	this.ask_for_card=baccarat_ask_for_card;
	this.instructions=baccarat_instructions;
	this.welcome=baccarat_welcome;
	this.play=baccarat_play;
}
