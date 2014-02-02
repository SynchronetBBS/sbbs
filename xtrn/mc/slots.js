/*
{                             Milliways Casino                               }
{                  Copyright (C) 1987 by Charles Ezzell & Matthew Warner     }
{                            All Rights Reserved                             }
{                                                                            }
{                                                                            }
*/

function slots_process_bets()
{
	var jack_pot,win;

	switch(this.win_number.length) {
		case 2:
			if(this.win_number[0] != this.win_number[1]) {
				if(this.win_number[0]==4 || this.win_number[1]==4)
					this.win_number=[4];
				else
					this.win_number=new Array();
			}
			break;
		case 3:
			if(this.win_number[0] != this.win_number[1] || this.win_number[1] != this.win_number[2]) {
				if(this.win_number[0] == this.win_number[1])
					this.win_number.splice(2,1);
				else if(this.win_number[0] == this.win_number[2])
					this.win_number.splice(1,1);
				else if(this.win_number[1] == this.win_number[2])
					this.win_number.splice(0,1);
				else if(this.win_number[0]==4 || this.win_number[1]==4 || this.win_number[2]==4)
					this.win_number=[4];
				else
					this.win_number=new Array();
			}
	}
	win=0;
	switch(this.win_number.length) {
		case 1:
			switch(this.win_number[0]) {
				case 4:
					win=this.betamount*3;
					break;
			}
			break;
		case 2:
			switch(this.win_number[0]) {
				case 1:
					win=this.betamount*26;
					break;
				case 2:
					win=this.betamount*11;
					break;
				case 4:
					win=this.betamount*6;
					break;
			}
			break;
		case 3:
			switch(this.win_number[0]) {
				case 1:
					/* JACKPOT!!! */
					win=stranger.jackpot;
					stranger.jackpot=0;
					jack_pot=true;
					console.crlf();
					ansic(4); console.print('*** ');
					ansic(8); console.print('JACKPOT');
					ansic(4); console.print(' ***\r\n');
					ansic(0);
					sysoplog(player.name+'WON THE JACKPOT!!!');
					console.crlf();
					break;
				case 2:
					win=this.betamount*26;
					break;
				case 4:
					win=this.betamount*16;
					break;
			}
			break;
	}
	if(!jack_pot)
		win=win-this.betamount;
	if(win > 0)
		console.print('You won '+format_money(win)+'\r\n');
	else	
		console.print('You lost '+format_money(win)+'\r\n');
	if(!jack_pot) {
		if(win < 0)
			stranger.jackpot += this.betamount * 0.1;
		else
			stranger.jackpot -= this.betamount * 0.05;
	}
	player.players_money += win;
	player.won_today += win;
}

function slots_spin()
{
	var wh, j_end, j, k, tempnum;

	this.win_number=new Array();
	for(wh=0; wh<3; wh++) {
		j_end=random(100);
		for(j=0; j<j_end; j++)
			k=random(17);
		tempnum=this.wheel[wh][k];
		if(tempnum==1 || tempnum==2 || tempnum==4)
			this.win_number.push(tempnum);
		console.print(this.wheel_display[this.wheel[wh][k]]);
	}
	console.attributes=7;
	console.crlf();
	console.crlf();
}

function slots_bet()
{
	var temp, temp_money;

	check_winnings();
	check_random();
	this.betamount=0;
	do {
		console.print('A voice comes out of the machine:\r\n');
		console.print(player.name+', you may bet between 10 & 5000 dollars.\r\n');
		console.print('S for stats, B for odds, Q to quit.\r\n');
		console.crlf();
		console.print('How much are you betting? ');
		temp=console.getkeys('BSQ',5000);
		switch(temp) {
			case 'B':
				this.odds();
				break;
			case 'S':
				player_stats();
				break;
			case 'Q':
				return(false);
			default:
				this.betamount=parseInt(temp);
				if(isNaN(this.betamount) || this.betamount < 10 || this.betamount > 5000)
					console.print('Illegal amount\r\n');
		}
		if(this.betamount % 10) {
			console.crlf();
			console.print('You hear a voice from the slot machine:\r\n');
			console.print(player.name+', all bets must be in multiples of 10.');
			console.print('Please redo your bet.');
			console.crlf();
		}
	} while(isNaN(this.betamount) || this.betamount < 5 || this.betamount > 5000 || this.betamount % 10);
	temp_money=player.players_money-this.betamount
	if(temp_money < 0)
		temp_money=sell_to_bruno(temp_money);
	return(true);
}

function slots_welcome()
{
	console.crlf();
	console.print('You open the door, and can not believe your eyes.  There must be\r\n');
	console.print('at least 1000 slot machines lined up in rows throughout this room.\r\n');
	console.print(' You wonder around till you find one with an empty seat, sit down,\r\n');
	console.print('and insert your card into the slot in front of you.\r\n');
	console.crlf();
	console.crlf();
}

function slots_odds()
{
	console.print('  3 Bars    : JACKPOT!     2 Bars    : 25xbet\r\n');
	console.print('  3 Sevens  : 25xbet       2 Sevens  : 10xbet\r\n');
	console.print('  3 Cherrys : 15xbet       2 Cherrys : 5xbet\r\n');
	console.print('  1 Cherry  : 2xbet\r\n');
}

function slots_play()
{
	this.welcome();
	while(1) {
		tleft();
		if(player.bruno > 0)
			buy_from_bruno();
		console.print("You have "+format_money(player.players_money)+".\r\n");
		if(this.bet()) {
			this.spin();
			this.process_bets();
		}
		else
			break;
	}
}

function Slots()
{
	this.betamount=0;
	this.win_number=new Array();
	this.wheel=[	 [1,2,7,4,5,6,7,3,7,3,1,6,3,6,3,7,5]
					,[1,2,3,7,5,6,7,5,7,3,4,5,6,7,3,2,3]
					,[1,2,3,4,5,6,7,5,7,3,4,5,6,7,5,6,3]];
	this.wheel_display=new Array(	    'ProgramBug'
							,'\1n\1k\0017   BAR    \1n'
							,'\1n\1k\0017   SEVEN  \1n'
							,'\1h\1r\0017   APPLE  \1n'
							,'\1n\1r\0017  CHERRY  \1n'
							,'\1h\1y\0017   LEMON  \1n'
							,'\1n\1y\0017  CARROT  \1n'
							,'\1h\1y\0017  BANANA  \1n');

	this.play=slots_play;
	this.welcome=slots_welcome;
	this.odds=slots_odds;
	this.bet=slots_bet;
	this.spin=slots_spin;
	this.process_bets=slots_process_bets;
}
