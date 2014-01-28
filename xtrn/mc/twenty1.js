/*
{                             Milliways Casino                               }
{                  Copyright (C) 1987 by Charles Ezzell & Matthew Warner     }
{                            All Rights Reserved                             }
{                                                                            }
{                                                                            }
*/

function twenty1_welcome()
{
	console.print('Welcome to the Blackjack Room.\r\n');
	console.crlf();
	console.print('There are 3 empty seats around the room.\r\n');
	console.crlf();
	console.print('You notice that each of them are at different tables,\r\n');
	console.print('each one having different betting limits.\r\n');
	console.crlf();
	console.crlf();
	console.print('Table 1 :     $10 - $500\r\n');
	console.print('Table 2 :    $100 - $1,000\r\n');
	console.print('Table 3 :    $500 - $10,000\r\n');
	console.crlf();
	console.print('Which table do you sit at? (1-3) ');
	switch(console.getkeys("123")) {
		case '1':
			this.min=10;
			this.max=500;
			this.min_name="$10";
			this.max_name="$500";
			break;
		case '2':
			this.min=100;
			this.max=1000;
			this.min_name="$100";
			this.max_name="$1,000";
			break;
		case '3':
			this.min=500;
			this.max=10000;
			this.min_name="$500";
			this.max_name="$10000";
			break;
	}
}

function twenty1_shuffle()
{
	var left=new Array(52);
	var i,rnd;

	for(i=0; i<52; i++)
		left[i]=i;
	this.card=[];
	while(left.length) {
		rnd=random(left.length);
		this.card.push(left[rnd]);
		left.splice(rnd,1);
	}
	console.print("\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08            \x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08");
	console.crlf();
	console.crlf();
}

function twenty1_showcard(hand, showit)
{
	var i,j,x,y,c,s,cv;

	for(i in hand) {
		x=hand[i];
		c=x % 13;
		s=parseInt(x/13);
		if(showit || i != 1)
			cv=card[c]+' of '+suit[s];
		else
			cv='HIDDEN CARD';
		console.print(cv+'\r\n');
	}
}

function twenty1_dealcard()
{
	if(this.card.length < 17 && this.new_hand)
		this.shuffle();
	return(this.card.shift());
}

function twenty1_handvalue(hand)
{
	var i,y,x,ace;

	x=ace=0;
	for(i in hand) {
		y=(hand[i] % 13) + 1;
		if(y>10)
			y=10;
		if(y==1) {
			y=11;
			ace++;
		}
		x+=y;
	}
	while(x>21 && ace>0) {
		x-=10;
		ace--;
	}
	if(x>21)
		x=0;
	if(hand.length==2 && x==21)
		x=22;
	return(x);
}

function twenty1_stranger_bets()
{
	var temp_max;

	this.strangers_bet=0;

	if(stranger.strangers_money < this.min)
		strangers_gets_more_money();
	temp_max=this.max/2;
	do {
		this.strangers_bet=random(temp_max);
	} while (this.strangers_bet < this.min || (this.strangers_bet % 100) != 0);
	if(random(10)+1 > 5)
		this.strangers_bet += temp_max;
	console.print('The stranger bets '+format_money(this.strangers_bet)+'.\r\n');
	console.crlf();
	console.pause();
}

function twenty1_get_bet()
{
	var temp;

	this.player_bet=0;
	check_winnings();
	check_random();
	do {
		console.print('The dealer tells you to place your bet.\r\n');
		console.print('('+this.min_name+'-'+this.max_name+')\r\n');
		console.print('(Q to quit)\r\n');
		console.print('(S for stats)\r\n');
		console.print('How much are you betting? ');
		temp=console.getkeys('SQ', this.max);
		switch(temp) {
			case 'S':
				player_stats();
				break;
			case 'Q':
				return(false);
			default:
				this.player_bet=parseInt(temp);
				break;
		}
		if(this.player_bet % 10) {
			console.print('The dealer looks down at you and says:\r\n');
			console.print(user.name+', all bets must be in multiples of 10.\r\n');
			console.print('Please redo your bet.\r\n');
			console.crlf();
			console.crlf();
		}
	} while(isNaN(this.player_bet) || this.player_bet < this.min || this.player_bet > this.max || this.player_bet % 10);

	temp=player.players_money-this.player_bet;
	if(temp < 0)
		temp=sell_to_bruno(temp);
	console.crlf();
	if(!stranger_dates_kathy)
		this.stranger_bets();
	return(true);
}

function twenty1_deal_first_cards()
{
	this.new_hand=true;
	this.strangers_hand=[];
	this.player_hand=[];
	this.dealer_hand=[];
	if(!stranger_dates_kathy)
		this.strangers_hand.push(this.dealcard());
	this.player_hand.push(this.dealcard());
	this.dealer_hand.push(this.dealcard());
	if(!stranger_dates_kathy)
		this.strangers_hand.push(this.dealcard());
	this.player_hand.push(this.dealcard());
	this.dealer_hand.push(this.dealcard());
	this.new_hand=false;
}

function twenty1_stranger_hand()
{
	var choice, strangers_done, test_num;

	strangers_done=false;
	this.strangers_payoff=1;
	choice=' ';
	do {
		console.crlf();
		console.print('Strangers Hand:\r\n');
		this.showcard(this.strangers_hand, true);
		this.scardval=this.handvalue(this.strangers_hand);
		if(this.scardval > 0 && this.scardval < 22)
			console.print('The stranger has: '+this.scardval+'\r\n');
		if(this.scardval==0) {
			console.print('The stranger busted!\r\n');
			this.strangers_hand_done=true;
			this.strangers_busted=true;
			strangers_done=true;
			console.pause();
		}
		if(choice=='D')
			strangers_done=true;
		if(!strangers_done) {
			test_num=random(100)+1;
			if(this.scardval > 9 && this.scardval < 12 && this.strangers_hand.length==2 && test_num > 20)
				choice='D';
			if(choice != 'D') {
				if(this.scardval >= 1 && this.scardval <= 11)
					choice='H';
				else if(this.scardval <= 14) {
					if(test_num < 50)
						choice='H';
					else
						choice='S';
				}
				else if(this.scardval <= 16) {
					if(test_num < 25)
						choice='H';
					else
						choice='S';
				}
				else if(this.scardval <= 21)
					choice='S';
			}
			switch(choice) {
				case 'H':
					console.print('The stranger takes another card.\r\n');
					this.strangers_hand.push(this.dealcard());
					strangers_done=false;
					break;
				case 'S':
					console.print('The stranger stands with '+this.scardval+'.\r\n');
					strangers_done=true;
					break;
				case 'D':
					console.print('The stranger doubles his bet.\r\n');
					this.strangers_hand.push(this.dealcard());
					strangers_done=true;
					this.strangers_payoff=2;
					break;
			}
		}
		console.pause();
	} while(!strangers_done);
	console.crlf();
}

function twenty1_players_hand()
{
	var test_num, player_double, cha, player_done;

	player_double=false;
	cha=' ';
	this.dcardval=this.handvalue(this.dealer_hand);
	this.pcardval=this.handvalue(this.player_hand);
	if(!stranger_dates_kathy)
		this.scardval=this.handvalue(this.strangers_hand);
	else {
		this.scardval=99;
		this.strangers_hand_done=true;
	}
	console.print('Dealers Hand\r\n');
	this.showcard(this.dealer_hand, false);
	console.crlf();
	if(this.scardval==22 || this.pcardval==22 || (((this.dealer_hand[0]-1)%13)==0)) {
		if(this.scardval==22) {
			console.print('Strangers Hand\r\n');
			this.showcard(this.strangers_hand, true);
			console.crlf();
			console.print('The stranger has BLACKJACK!\r\n');
			this.strangers_hand_done=true;
			this.strangers_blackjack=true;
			this.strangers_payoff=1.5;
		}
		if(!this.strangers_hand_done) {
			console.print('Strangers Hand\r\n');
			this.showcard(this.strangers_hand);
			console.crlf();
		}
		console.pause();
		console.crlf();
		if(this.pcardval==22) {
			console.print('Your Hand\r\n');
			this.showcard(this.player_hand,true);
			console.crlf();
			console.print('You have BLACKJACK!\r\n');
			this.player_payoff=1.5;
			this.player_hand_done=true;
			this.player_blackjack=true;
			console.pause();
		}
		if((!this.player_hand_done) || (!this.strangers_hand_done)) {
			if(((this.dealer_hand[0]-1) % 13)==0) {
				if(!this.player_hand_done) {
					console.print('Your Hand');
					this.showcard(this.player_hand, true);
					console.crlf();
					console.print('You have '+this.pcardval+'\r\n');
					console.pause();
				}
				console.crlf();
				console.print('The dealer may have blackjack.\r\n');
				test_num=random(10)+1;
				if(!this.strangers_hand_done) {
					if(test_num >= 8) {
						console.print('The stranger takes insurance.\r\n');
						this.strangers_insurance=true;
					}
					else
						console.print('The stranger does not take insurance.\r\n');
				}
				console.crlf();
				if(!this.player_hand_done) {
					console.print('Do you wish insurance? ');
					if(console.getkeys('YN')=='Y')
						this.player_insurance=true;
				}
				if(this.dcardval==22) {
					console.crlf();
					console.print('The dealer has blackjack!\r\n');
					this.player_hand_done=true;
					this.strangers_hand_done=true;
					this.dealer_blackjack=true;
				}
				else
					console.print('The dealer does not have blackjack.\r\n');
			}
		} 
	}
	if((!this.strangers_hand_done) && this.scardval != 99)
		this.stranger_hand();
	if(!this.player_hand_done) {
		this.player_payoff=1;
		player_done=false;
		do {
			console.crlf();
			console.print('your hand:\r\n');
			this.showcard(this.player_hand, true);
			this.pcardval=this.handvalue(this.player_hand);
			if(this.pcardval > 0 && this.pcardval < 22)
				console.print('You have: '+this.pcardval+'\r\n');
			if(this.pcardval == 0) {
				console.print('You busted!\r\n');
				player_done=true;
				this.player_busted=true;
			}
			if(cha=='D')
				player_done=true;
			if(!player_done) {
				if(this.pcardval > 9 && this.pcardval < 12 && this.player_hand.length == 2)
					player_double=true;
				else
					player_double=false;
				console.print('H)it S)tand ');
				if(player_double)
					console.print('D)ouble ');
				console.print('[H/S');
				if(player_double)
					console.print('/D');
				console.print('] : ');
				switch((char=console.getkeys('HS'+(player_double?'D':'')))) {
					case 'H':
						console.print('you take another card.\r\n');
						this.player_hand.push(this.dealcard());
						player_done=false;
						break;
					case 'S':
						console.print('You stand pat with '+this.pcardval+'\r\n');
						player_done=true;
						break;
					case 'D':
						console.print('You double your bet.\r\n');
						this.player_hand.push(this.dealcard());
						player_done=false;
						this.player_payoff=2;
				}
			}
		} while(!player_done);
	}
}

function twenty1_dealers_hand()
{
	var dealer_done=false;
	if((this.pcardval != 22 || this.scardval != 22) && (this.pcardval != 0 || this.scardval != 0) && ((!this.player_hand_done) || (!this.strangers_hand_done))) {
		do {
			console.crlf();
			this.showcard(this.dealer_hand, true);
			this.dcardval=this.handvalue(this.dealer_hand);
			if(this.dcardval==0) {
				console.print('Dealer goes *** BUST ***\r\n');
				dealer_done=true;
			}
			else {
				console.print('Dealer has '+(this.dcardval==22?'BLACKJACK':this.dcardval)+'\r\n');
				if(this.dcardval < 17) {
					console.print('dealer takes a hit.\r\n');
					this.dealer_hand.push(this.dealcard());
					dealer_done=false;
				}
				else {
					console.print('dealer stands pat.\r\n');
					dealer_done=true;
				}
			}
			console.pause();
		} while(!dealer_done);
	}
	console.crlf();
}

function twenty1_process_hands()
{
	var player_won=strangers_won=player_push=strangers_push=false;
	var player_amount,strangers_amount;

	/* look forwinning hands */
	if(this.pcardval > this.dcardval || this.player_blackjack)
		player_won=true;
	if(this.scardval != 99) {
		if(this.scardval > this.dcardval || this.strangers_blackjack)
			strangers_won=true;
	}

	/* PROCESS STRANGERS HAND */
	if(!stranger_dates_kathy) {
		if(strangers_won) {
			console.print('The stranger wins his hand!\r\n');
			if(this.strangers_insurance) {
				console.print('However, he losses his insurance money.\r\n');
				this.strangers_payoff -= 0.5;
			}
		}
		else {
			if(this.scardval == this.dcardval && (!this.strangers_blackjack) && (!this.strangers_busted))
				strangers_push=true;
			if(strangers_push) {
				console.print('The stranger and dealers hands push.\r\n');
				if(this.strangers_insurance) {
					console.print('However, he losses his insurance money.\r\n');
					this.strangers_payoff -= 0.5;
				}
				else
					this.strangers_payoff=0;
			}
			else {
				console.print('The stranger loses.\r\n');
				this.strangers_payoff = 0-this.strangers_payoff;
				if(this.dealer_blackjack && this.strangers_insurance) {
					console.print("The stranger's insurnace protects him from blackjack,\r\n");
					console.print('but he loses his insuracne money.\r\n');
					this.strangers_payoff += 0.5;
				}
				else if((!this.dealer_blackjack) && this.strangers_insurance) {
					console.print('The stranger loses both his bet and insurance.\r\n');
					this.strangers_payoff -= 0.5;
				}
			}
		}
	}

	/* PROCESS PLAYER'S HAND */
	if(player_won) {
		console.print('You win your hand!\r\n');
		if(this.player_insurance) {
			console.print('However, you lose your insurance money.\r\n');
			this.player_payoff -= 0.5;
		}
	}
	else {
		if(this.pcardval == this.dcardval && (!this.player_blackjack) && (!this.player_busted))
			player_push=true;
		if(player_push) {
			console.print('Your Hands push. No win or loss.\r\n');
			if(this.player_insurance) {
				console.print('However, you lose your insurance money.\r\n');
				this.player_payoff -= 0.5;
			}
			else
				this.player_payoff=0;
		}
		else {
			console.print('You lose.\r\n');
			this.player_payoff = 0-this.player_payoff;
			if(this.dealer_blackjack && this.player_insurance) {
				console.print('Insurance protects you from blackjack.\r\n');
				console.print('(You lose your insurance money though.)\r\n');
				this.player_payoff += 0.5;
			}
			else if((!this.dealer_blackjack) && this.player_insurance) {
				console.print('You lose your bet and insurance\r\n');
				this.player_payoff -= 0.5;
			}
		}
	}

	player_amount = this.player_bet * this.player_payoff;
	player.players_money += player_amount;
	player.won_today += player_amount;
	if(!stranger_dates_kathy) {
		strangers_amount = this.strangers_bet * this.strangers_payoff;
		stranger.strangers_money += strangers_amount;
		if(strangers_amount != 0) {
			console.print(format_money(Math.abs(strangers_amount)));
			if(strangers_amount > 0)
				console.print(' added to');
			else
				console.print(' subtracted from');
			console.print(' the strangers bankroll.\r\n');
		}
	}
	if(player_amount != 0) {
		console.print(format_money(Math.abs(player_amount)));
		if(player_amount > 0)
			console.print(' added to');
		else
			console.print(' subtracted from');
		console.print(' your bankroll.\r\n');
		console.crlf();
	}
	if(player_amount > 0)
		stranger.jackpot -= player_amount *0.05;
	else
		stranger.jackpot += player_amount *0.1;
	console.pause();
}

function twenty1_play()
{
	this.welcome();
	console.print('You take your seat');
	if(!stranger_dates_kathy) {
		console.print(', and notice\r\n');
		console.print('the stranger sitting to your right\r\n');
		console.print('surrounded by his girls.\r\n');
	}
	else
		console.print('.\r\n');
	while(1) {
		tleft();
		this.player_payoff=this.strangers_payoff=1;
		this.player_insurance=this.strangers_insurance=false;
		this.player_hand_done=this.strangers_hand_done=false;
		this.player_blackjack=this.strangers_blackjack=this.dealer_blackjack=false;
		this.player_busted=this.strangers_busted=this.dealer_busted=false;
		if(player.bruno)
			buy_from_bruno();
		console.crlf();
		if(!stranger_dates_kathy)
			console.print('The stranger has '+format_money(stranger.strangers_money)+'.\r\n');
		console.print('You have '+format_money(player.players_money)+'.\r\n');
		if(!this.get_bet())
			break;
		this.deal_first_cards();
		this.players_hand();
		this.dealers_hand();
		this.process_hands();
	}
}

function Twenty1()
{
	/* Properties */
	this.card=new Array();
	this.dcardval=0;
	this.dealer_hand=new Array();
	this.max=0;
	this.max_name="$0";
	this.min=0;
	this.min_name="$0";
	this.new_hand=false;
	this.pcardval=0;
	this.player_bet=0;
	this.strangers_bet=0;

	this.player_payoff=this.strangers_payoff=1;
	this.player_insurance=this.strangers_insurance=false;
	this.player_hand_done=this.strangers_hand_done=false;
	this.player_busted=this.strangers_busted=this.dealer_busted=false;
	this.player_blackjack=this.strangers_blackjack=this.dealer_blackjack=false;

	/* Methods */
	this.welcome=twenty1_welcome;
	this.shuffle=twenty1_shuffle;
	this.showcard=twenty1_showcard;
	this.dealcard=twenty1_dealcard;
	this.handvalue=twenty1_handvalue;
	this.stranger_bets=twenty1_stranger_bets;
	this.get_bet=twenty1_get_bet;
	this.deal_first_cards=twenty1_deal_first_cards;
	this.stranger_hand=twenty1_stranger_hand;
	this.players_hand=twenty1_players_hand;
	this.dealers_hand=twenty1_dealers_hand;
	this.process_hands=twenty1_process_hands;
	this.play=twenty1_play;
}
