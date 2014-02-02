/*
{                             Milliways Casino                               }
{                  Copyright (C) 1987 by Charles Ezzell & Matthew Warner     }
{                            All Rights Reserved                             }
{                                                                            }
{                                                                            }
*/

function roulette_play_again()
{
	if(player.bruno > 0)
		buy_from_bruno();
	console.print('TOTALS'+'\r\n');
	if(!stranger_dates_kathy)
		console.print('The stranger has :'+format_money(stranger.strangers_money)+' dollars'+'\r\n');
	console.print('You have         :'+format_money(player.players_money)+' dollars'+'\r\n');
	check_winnings();
	console.crlf();
}

function roulette_swon(str)
{
	console.print('The stranger won '+format_money(this.strangers_bet) + ' dollars on his bet '+str+'\r\n');
	stranger.strangers_money += this.strangers_bet;
}

function roulette_slost(str)
{
	console.print('The stranger lost '+format_money(this.strangers_bet)+' dollars on his bet '+str+'\r\n');
	stranger.strangers_money -= this.strangers_bet;
}

function roulette_process_strangers_bets()
{
	switch(this.strangers_bettype) {
			case 43:
				if(this.win_number < 19 && this.win_number > 0)
					this.swon('(1-18 bet)');
				else
					this.slost('(1-18)');
				break;
			case 44:
				if(this.win_number < 37 && this.win_number > 18)
					this.swon('(19-36 bet)');
				else
					this.slost('(19-36)');
				break;
			case 45:
				if(this.win_number==0 || this.win_number==37)
					this.slost('(even)');
				else {
					if(this.win_number %2 == 0)
						this.swon('(even)');
					else
						this.slost('(even)');
				}
				break;
			case 46:
				if(this.win_number==0 || this.win_number==37)
					this.slost('(odd)');
				else {
					if(this.win_number % 2)
						this.swon('(odd)');
					else
						this.slost('(odd)');
					break;
				}
			case 47:
				if(this.namebets[this.win_number].red)
					this.swon('(red)');
				else
					this.slost('(red)');
				break;
			case 48:
				if(this.namebets[this.win_number].black)
					this.swon('(black)');
				else
					this.slost('(black)');
				break;
	}
}

function roulette_won(odds, amount, betnum, str)
{
	console.print('You won '+(amount*odds)+' dollars on bet #' + betnum + ' ' + str+'\r\n');
	player.players_money += amount*odds;
	player.won_today += amount*odds;
	stranger.jackpot -= ((amount*odds)*0.05);
}

function roulette_lost(amount,betnum,str)
{
	console.print('You lost '+amount+' dollars on bet #' + betnum + ' ' +str+'\r\n');
	player.players_money -= amount;
	stranger.jackpot += (amount * 0.10);
}

function roulette_process_players_bets()
{
	var betnum=0;
	var bettype;
	var betamount;

	while(this.betamount.length) {
		betamount=this.betamount.shift();
		bettype=this.bettype.shift();
		betnum++;
		switch(bettype) {
			case 37:
				if(this.win_number <= 12 && this.win_number > 0)
					this.won(2,betamount,betnum,'(1-12 bet)');
				else
					this.lost(betamount,betnum,'(1-12 bet)');
				break;
			case 38:
				if(this.win_number > 12 && this.win_number < 25)
					this.won(2,betamount,betnum,'(13-24 bet)');
				else
					this.lost(betamount,betnum,'(13-24)');
				break;
			case 39:
				if(this.win_number > 24 && this.win_number < 37)
					this.won(2,betamount,betnum,'(25-36 bet)');
				else
					this.lost(betamount,betnum,'(25-36 bet)');
				break;
			case 40:
				if(this.namebets[this.win_number].column==1)
					this.won(2,betamount,betnum,'(1st column)');
				else
					this.lost(betamount,betnum,'(1st column)');
				break;
			case 41:
				if(this.namebets[this.win_number].column==2)
					this.won(2,betamount,betnum,'(2nd column)');
				else
					this.lost(betamount,betnum,'(2nd column)');
				break;
			case 42:
				if(this.namebets[this.win_number].column==3)
					this.won(2,betamount,betnum,'(3rd column)');
				else
					this.lost(betamount,betnum,'(3rd column)');
				break;
			case 43:
				if(this.win_number < 19 && this.win_number > 0)
					this.won(1,betamount,betnum,'(1-18 bet)');
				else
					this.lost(betamount,betnum,'(1-18)');
				break;
			case 44:
				if(this.win_number < 37 && this.win_number > 18)
					this.won(1,betamount,betnum,'(19-36 bet)');
				else
					this.lost(betamount,betnum,'(19-36)');
				break;
			case 45:
				if(this.win_number == 0 && this.win_number == 37)
					this.lost(betamount,betnum,'(even)');
				else {
					if(this.win_number % 2 == 0)
						this.won(1,betamount,betnum,'(even)');
					else
						this.lost(betamount,betnum,'(even)');
				}
				break;
			case 46:
				if(this.win_number == 0 && this.win_number == 37)
					this.lost(betamount,betnum,'(odd)');
				else {
					if(this.win_number % 2)
						this.won(1,betamount,betnum,'(odd)');
					else
						this.lost(betamount,betnum,'(odd)');
				}
				break;
			case 47:
				if(this.namebets[this.win_number].red)
					this.won(1,betamount,betnum,'(red)');
				else
					this.lost(betamount,betnum,'(red)');
				break;
			case 48:
				if(this.namebets[this.win_number].black)
					this.won(1,betamount,betnum,'(black)');
				else
					this.lost(betamount,betnum,'(black)');
				break;
			case 49:
				if(this.win_number==0)
					this.won(35,betamount,betnum,'(0)');
				else
					this.lost(betamount,betnum,'(0)');
				break;
			case 50:
				if(this.win_number==37)
					this.won(35,betamount,betnum,'(00)');
				else
					this.lost(betamount,betnum,'(0)');
				break;
			default:
				if(bettype==this.win_number)
					this.won(35,betamount,betnum,'Lucky dude, you guessed the #!');
				else
					this.lost(betamount,betnum,'(Gonna have to guess better than that!)');
		}
	}
}

function roulette_spin()
{
	var i,red,black,def,back_four,rnd;

	red="\1h\1w\x011";
	black="\1h\1w\x010";
	green="\1h\1w\x012";
	def="\1n";
	//back_four="\b\b\b\b";
	back_four="\r";
	console.print('Spinning the wheel'+'\r\n');
	rnd=random(20)+10;
	for(i=0; i<rnd; i++) {
		this.win_number=random(this.namebets.length);
		if(this.namebets[this.win_number].red)
			console.print(red+' '+this.namebets[this.win_number].name+' ');
		else if(this.namebets[this.win_number].black)
			console.print(black+' '+this.namebets[this.win_number].name+' ');
		else
			console.print(green+' '+this.namebets[this.win_number].name+' ');
		console.print(back_four);
	}
	console.print(def);
	console.crlf();
	console.crlf();
	console.print('The winning number is :');
	if(this.win_number==0 || this.win_number == 37)
		console.print(this.namebets[this.win_number].name+'\r\n');
	else if(this.namebets[this.win_number].red)
		console.print(this.namebets[this.win_number].name + ' Red'+'\r\n');
	else 
		console.print(this.namebets[this.win_number].name + ' Black'+'\r\n');
	console.crlf();
	tleft();
}

function roulette_strangers_bets()
{
	var i, test, rnd;
	var strangers_type_of_bet;

	console.crlf();
	console.print('The stranger now makes his bet:'+'\r\n');
	strangers_temp_money=stranger.strangers_money;
	do {
		this.strangers_bet=random(4500)+51;
	} while(this.strangers_bet % 50);
	strangers_temp_money -= this.strangers_bet;
	if(strangers_temp_money < 0)
		strangers_temp_money=strangers_gets_more_money(strangers_temp_money);
	do {
		rnd=random(50+50);
		for(i=0; i<rnd; i++)
			strangers_type_of_bet=random(100);
	} while(strangers_type_of_bet < 43 || strangers_type_of_bet > 48);
	this.strangers_bettype=strangers_type_of_bet;
	switch(strangers_type_of_bet) {
		case 43:
			console.print('The stranger places '+format_money(this.strangers_bet)+' on a 1-18 bet.'+'\r\n');
			break;
		case 44:
			console.print('The stranger places '+format_money(this.strangers_bet)+' on a 19-36 bet.'+'\r\n');
			break;
		case 45:
			console.print('The stranger places '+format_money(this.strangers_bet)+' on an even number bet.'+'\r\n');
			break;
		case 46:
			console.print('The stranger places '+format_money(this.strangers_bet)+' on an odd number bet.'+'\r\n');
			break;
		case 47:
			console.print('The stranger places '+format_money(this.strangers_bet)+' on a red number.'+'\r\n');
			break;
		case 48:
			console.print('The stranger places '+format_money(this.strangers_bet)+' on a black number.'+'\r\n');
			break;
	}
	console.crlf();
	console.pause();
}

function roulette_betting_odds()
{
	console.print('35:1 bets are:'+'\r\n');
	console.print('  the numbers (1-36), (49)-0, and (50)-00'+'\r\n');
	console.crlf();
	console.print('2:1 bets are:'+'\r\n');
	console.print(' (37)  1-12   (40) 1st column'+'\r\n');
	console.print(' (38) 13-24   (41) 2nd column'+'\r\n');
	console.print(' (39) 25-36   (42) 3rd column'+'\r\n');
	console.crlf();
	console.print('Even money bets are:'+'\r\n');
	console.print(' (43)  1-18   (46) odd'+'\r\n');
	console.print(' (44) 19-36   (47) red'+'\r\n');
	console.print(' (45) even    (48) black'+'\r\n');
	console.crlf();
	console.print('Minimum bet is $10, maximum is $5000'+'\r\n');
	console.print('You may place up to 5 different bets'+'\r\n');
	console.crlf();
}

function roulette_bets()
{
	var money;
	var type_of_bet;
	var player_betted_on=new Array(51);
	var num_bets;
	var ch,c,test,i,error,good,check;
	var bet_num;
	var temp_money;

	money=0;
	type_of_bet=0;
	for(i=0; i<50; i++)
		player_betted_on[i]=false;
	check_random();
	do {
		num_bets=0;
		console.print('B for betting odds chart'+'\r\n');
		console.print('S for player stats'+'\r\n');
		console.print('Q to quit'+'\r\n');
		console.print('How many bets are you placing? ');
		switch((ch=console.getkeys("BSQ5"))) {
			case 'B':
				this.betting_odds();
				break;
			case 'S':
				player_stats();
				break;
			case 'Q':
				return(false);
				break;
			default:
				num_bets=parseInt(ch);
				break;
		}
	} while(num_bets <1 || num_bets > 5 || isNaN(num_bets));

	temp_money=player.players_money;
	for(bet_num=0; bet_num < num_bets; bet_num++) {
		do {
			console.print("'B' for odds and types of bets."+'\r\n');
			console.print("'S' for player_stats."+'\r\n');
			console.print('Bet #'+(bet_num+1)+'\r\n')
			console.print('You have '+format_money(temp_money)+' dollars.'+'\r\n');
			console.print('How much are you betting? ');
			switch((ch=console.getkeys("BS", 5000))) {
				default:
					money=parseInt(ch);
					if(money < 10 || money > 5000 || isNaN(money))
						console.print('Illegal amount'+'\r\n');
					if(money % 10) {
						console.crlf();
						console.print('The coupier looks down at you and says:'+'\r\n');
						console.print(user.name+', all bets must be in multiples of 10.'+'\r\n');
						console.print('Please redo your bet.'+'\r\n');
						console.crlf();
						console.crlf();
					}
					break;
				case 'B':
					this.betting_odds();
					break;
				case 'S':
					player_stats();
					break;
			}
		} while(isNaN(money) || money < 10 || money > 5000 || money % 10);
		this.betamount.push(money);
		temp_money -= money;
		if(temp_money < 0)
			temp_money=sell_to_bruno(temp_money);
		do {
			good=false;
			console.print('What are you betting on (1-50)? ');
			switch((ch=console.getkeys('BS',50))) {
				case 'B':
					this.betting_odds();
					break;
				case 'S':
					player_stats();
					break;
				default:
					type_of_bet=parseInt(ch);
					if(isNaN(type_of_bet) || type_of_bet < 1 || type_of_bet > 50)
						console.print('Illegal bet'+'\r\n');
					else if(player_betted_on[type_of_bet])
						console.print("You've already bet on that one dummy!"+'\r\n');
					else
						good=true;
			}
		} while(!good);
		this.bettype.push(type_of_bet);
		player_betted_on[type_of_bet]=true;
	}
	return(true);
}

function roulette_instructions()
{
	console.crlf();
	console.crlf();
	console.crlf();
	console.print('Bets are made on the following'+'\r\n');
	console.print('35:1 bets are:'+'\r\n');
	console.print('  the numbers (1-36), (49)-0, and (50)-00'+'\r\n');
	console.crlf();
	console.print('2:1 bets are:'+'\r\n');
	console.print(' (37)  1-12   (40) 1st column'+'\r\n');
	console.print(' (38) 13-24   (41) 2nd column'+'\r\n');
	console.print(' (39) 25-36   (42) 3rd column'+'\r\n');
	console.crlf();
	console.print('Even money bets are:'+'\r\n');
	console.print(' (43)  1-18   (46) odd'+'\r\n');
	console.print(' (44) 19-36   (47) red'+'\r\n');
	console.print(' (45) even    (48) black'+'\r\n');
	console.crlf();
	console.print('Minimum bet is $10, maximum is $5000'+'\r\n');
	console.print('You may place up to 5 different bets'+'\r\n');
	console.crlf();
	console.pause();
	console.print('First column numbers are:'+'\r\n');
	console.print('1,4,7,10,13,16,19,22,25,28,31,34'+'\r\n');
	console.crlf();
	console.print('Second Column numbers are:'+'\r\n');
	console.print('2,5,8,11,14,17,20,23,26,29,32,35'+'\r\n');
	console.crlf();
	console.print('Third Column numbers are:'+'\r\n');
	console.print('3,6,9,12,15,18,21,24,27,30,33,36'+'\r\n');
	console.crlf();
	console.print('1-12, 13-24, 25-36, 1-18, 19-36, even, odd, red, black'+'\r\n');
	console.print('should be self explanatory.'+'\r\n');
	console.crlf();
	console.pause();
	console.print('You may place up to 5 different bets at one time.'+'\r\n');
	console.print('You will be asked first for how many bets you wish to make,'+'\r\n');
	console.print('then, for each different bet, you enter how much you are betting'+'\r\n');
	console.print('and for the number of the bet.  Bets are numbered 1-50.'+'\r\n');
	console.print('The numbers 1-36, 49 & 50 signify a bet on 1-36, 0, & 00, all of'+'\r\n');
	console.print('which pay off at 35:1.  37-42 are 2:1 bets, and 43-48 are even money'+'\r\n');
	console.print('bets.'+'\r\n');
	console.crlf();
	console.pause();
}

function roulette_welcome()
{
	console.crlf();
	console.crlf();
  	console.print ('  Smoke fills the air as you look across a vast room.  There'+'\r\n');
	console.print ('must be at least 100 Roulette tables in front of you.  People'+'\r\n');
	console.print ('of all different sorts fill this room, all different colors,'+'\r\n');
	console.print ('and creeds.'+'\r\n');
	console.crlf();
	console.crlf();
	console.print ('The fifth roulette table has only 1 other person there, so you'+'\r\n');
	console.print ('approach it.  Sitting there, in the middle of a mob of blonde'+'\r\n');
	console.print ('showgirls, you see a rather slim man, wearing a dark grey,'+'\r\n');
	console.print ('pin-stripped suit.  He seems to be in the possession of a large'+'\r\n');
	console.print ('quantity of money, as well as a one foot cigar.  He is wearing a'+'\r\n');
	console.print ('pair of dark glasses, and has a deep scar down the right side of'+'\r\n');
	console.print ('his face.  As you approach the table, he glares at you, and'+'\r\n');
	console.print ('says something that makes that makes the girls giggle.'+'\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print ('  You sit down on a small, wooden stool at the table, placing'+'\r\n');
	console.print ('your chips in front of you.  The man looks at your pile,'+'\r\n');
	console.print ('takes a swig of his bourbon (straight) and in a very deep voice'+'\r\n');
	console.print ('says, "Girls, I think this could get interesting."'+'\r\n');
	console.crlf();
	console.crlf();
	console.pause();
}

function roulette_play()
{
	this.welcome();
	console.print('Do you wish instructions? ');
	if(yn())
		roulette_instructions()
	while(1) {
		tleft();
		if(this.bets()) {
			if(!stranger_dates_kathy)
				this.sbets();
			this.spin();
			this.process_bets();
			if(!stranger_dates_kathy)
				this.process_sbets();
			this.play_again();
		}
		else
			break;
	}
}

function Roulette()
{
	var i;

	function NameBets(num) {
		this.column=num%3;
		if(this.column==0)
			this.column=3;
		if(num==0)
			this.column=0;
		if(num==37)
			this.column=0;
		switch(num) {
			case 0:
				this.name=' 0';
				break;
			case 37:
				this.name='00';
				break;
			default:
				this.name=format("%2d", num);
				break;
		}
		this.red=false;
		this.black=false;
		switch(num) {
			case 1:
			case 3:
			case 5:
			case 7:
			case 9:
			case 12:
			case 14:
			case 16:
			case 18:
			case 19:
			case 21:
			case 23:
			case 25:
			case 27:
			case 30:
			case 32:
			case 34:
			case 36:
				this.red=true;
				break;
			default:
				if(num != 0 && num != 37)
					this.black=true;
				break;
		}
	}

	this.namebets=new Array(38);
	for(i=0; i<38; i++)
		this.namebets[i]=new NameBets(i);
	this.strangers_bet=0;
	this.betamount=new Array();
	this.bettype=new Array();
	this.win_number=0;
	this.strangers_bettype=0;
	this.strangers_bet=0;

	this.play=roulette_play;
	this.welcome=roulette_welcome;
	this.instructions=roulette_instructions;
	this.bets=roulette_bets;
	this.betting_odds=roulette_betting_odds;
	this.sbets=roulette_strangers_bets;
	this.spin=roulette_spin;
	this.process_bets=roulette_process_players_bets;
	this.won=roulette_won;
	this.lost=roulette_lost;
	this.process_sbets=roulette_process_strangers_bets;
	this.swon=roulette_swon;
	this.slost=roulette_slost;
	this.play_again=roulette_play_again;
}
