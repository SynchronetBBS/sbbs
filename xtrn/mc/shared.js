var messagefile=new File(game_dir+'casino.msg');
var hangup=false;
const win_money=1000000;
var suit=['Spades', 'Hearts', 'Diamonds', 'Clubs'];
var card=['Ace','Two','Three','Four','Five','Six','Seven','Eight','Nine','Ten','Jack','Queen','King'];

var PlayerProperties = [
	{
		 prop:"UserNumber"
		,name:"User Number"
		,type:"Integer"
		,def:(user.number)
	}
	,{
		 prop:"name"
		,name:"Name"
		,type:"String:42"
		,def:(user == undefined?"":user.alias)
	}
	,{
		 prop:"last_on"
		,name:"Last On"
		,type:"String:8"
		,def:system.datestr()
	}
	,{
		 prop:"music"
		,name:"Music"
		,type:"Boolean"
		,def:false
	}
	,{
		 prop:"used_condom"
		,name:"Used Condom"
		,type:"Boolean"
		,def:false
	}
	,{
		 prop:"ansi_ok"
		,name:"ANSI Ok"
		,type:"Boolean"
		,def:((user.settings & USER_ANSI)==USER_ANSI)
	}
	,{
		 prop:"male"
		,name:"Male"
		,type:"Boolean"
		,def:(user.sex=='M')
	}
	,{
		 prop:"won_today"
		,name:"Won Today"
		,type:"SignedInteger"
		,def:0
	}
	,{
		 prop:"players_money"
		,name:"Players Money"
		,type:"SignedInteger"
		,def:1000
	}
	,{
		 prop:"times_on"
		,name:"Times On"
		,type:"Integer"
		,def:1
	}
	,{
		 prop:"aids"
		,name:"Aids"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"played_baccarat"
		,name:"Played Baccarat"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"played_twenty1"
		,name:"Played Twenty-one"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"played_slots"
		,name:"Played Slots"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"played_roulette"
		,name:"Played Roulette"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"turned_down_date"
		,name:"Turned Down Date"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"hooker"
		,name:"Hooker"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"screwed"
		,name:"Screwed"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"bruno"
		,name:"Bruno"
		,type:"Integer"
		,def:0
	}
	,{
		 prop:"condom"
		,name:"Condom"
		,type:"Integer"
		,def:0
	}
];

var players=new RecordFile(game_dir+'players.dat', PlayerProperties);

var GameProperties = [
	 {
		 prop:"old_date"
		,name:"Old Date"
		,type:"String:8"
		,def:(system.datestr())
	}
	,{
		 prop:"dating_kathy"
		,name:"Dating Kathy"
		,type:"Boolean"
		,def:false
	}
	,{
		 prop:"jackpot"
		,name:"Jackpot"
		,type:"Float"
		,def:10000
	}
	,{
		 prop:"strangers_money"
		,name:"Stranger's Money"
		,type:"Integer"
		,def:20000
	}
];
var game=new RecordFile(game_dir+'game.dat', GameProperties);

function tleft()
{
	bbs.get_time_left();
}
function sysoplog() {}

function ansic(x) {
	if(x==1 || x==0)
		x=0;
	else {
		if(x==2)
			x=7;
		else
			x-=2;
	}
	
	switch(x) {
		case 0:
			console.attributes=7;
			break;
		case 1:
			console.attributes=LIGHTCYAN;
			break;
		case 2:
			console.attributes=YELLOW;
			break;
		case 3:
			console.attributes=MAGENTA;
			break;
		case 4:
			console.attributes=WHITE|BG_BLUE;
			break;
		case 5:
			console.attributes=GREEN;
			break;
		case 6:
			console.attributes=LIGHTRED|BLINK;
			break;
		case 7:
			console.attributes=LIGHTBLUE;
			break;
		case 8:
			console.attributes=BLUE;
			break;
		case 9:
			console.attributes=CYAN;
			break;
		default:
			console.attributes=7;
			break;
	}
}

function checkhangup() 
{
	// TODO: More stuff to check?
	if(js.terminated)
		hangup=true;
	if(!bbs.online)
		hangup=true;
}

function leave()
{
	var i;

	if(!won) {
		if(player.players_money <= 0)
			player.players_money=1000;
		if(player.bruno >= 7)
			player_died();
		if(player.last_on != system.datestr())
			player.last_on=system.datestr()
	}
	else {
		player_died();
		player.won_today=0;
	}
	player.Put();
	stranger.Put();
	exit(0);
}

function setcol(c)
{
	ansic(c);
}

function tlef()
{
	var tl=bbs.get_time_left();
	return(parseInt(tl/60)+":"+format("%02d",tl%80));
}

function check_winnings()
{
	tleft();
	if(player.won_today == -1000000) {
		console.print("You died today!  You'll have to\r\n");
		console.printf('try again tomorrow.');
		leave();
	}
	if(player.won_today >= 100000) {
		console.print('Bruno walks up to you and says:\r\n');
		console.crlf();
		console.print('You have won '+format_money(player.won_today)+' today.\r\n');
		console.print('Unfortunately, that is over the limit, so try\r\n');
		console.print('again tomorrow.\r\n');
		console.crlf();
		leave();
	}
	if(player.won_today <= -1000000) {
		console.print('Bruno walks up to you and informs you that\r\n');
		console.print('you have lost '+format_money(player.won_today)+' today, and that you might\r\n');
		console.print('do better if you try again tomorrow.\r\n');
		console.crlf();
		leave();
	}
}

function format_money(s)
{
	var minus=false;

	if(s<0) {
		minus=true;
		s=0-s;
	}
	if(s < 1000) {
		/* Zero commas */
		s=format("$%ld",s);
	}
	else if(s < 1000000) {
		/* One comma */
		s=format("$%ld,%03ld",(s/1000),(s%1000));
	}
	else if(s < 1000000000) {
		/* Two commas */
		s=format("$%ld,%03ld,%03ld",(s/1000000),((s/1000)%1000),(s%1000));
	}
	else if(s < 1000000000000) {
		/* Three commas */
		s=format("$%ld,%03ld,%03ld,%03ld",(s/1000000000),((s/1000000)%1000),((s/1000)%1000),(s%1000));
	}
	if(minus)
		s='-'+s;
	return(s);
}

function real_dough(pl)
{
	var ret=pl.players_money;

	switch(pl.bruno) {
		case 6:
			ret-=200000;
		case 5:
			ret-=100000;
		case 4:
			ret-=25000;
		case 3:
			ret-=8000;
		case 2:
			ret-=1000;
		case 1:
			ret-=100;
	}
	return(ret);
}

function player_stats()
{
	var str;	/* temp */

	console.crlf();
	console.crlf();
	console.print('Your Name '+player.name+'\r\n');
	if(player.last_on != system.datestr)
		console.print('Last time here was '+player.last_on+'.'+'\r\n');
	console.print('You have played the game '+player.times_on+(player.times_on > 1?' times.':' time')+'\r\n');
	console.print('The slots jackpot now stands at '+format_money(stranger.jackpot)+'.'+'\r\n');
	console.print('You have '+format_money(player.players_money)+' on hand at this time.'+'\r\n');
	console.print('Your actual cash is '+format_money(real_dough(player))+'.'+'\r\n');
	if(!stranger.dating_kathy)
		console.print('The stranger has '+format_money(stranger.strangers_money)+'\r\n');
	else
		console.print('The stranger is out fooling around.'+'\r\n');
	if(player.hooker > 0) {
		console.print("You've dated Kathy "+player.hooker+' '+(player.hooker > 1?' \times.':'time.')+'\r\n');
		if(player.screwed > 0)
			console.print("You've made love to Kathy "+player.screwed+' '+(player.screwed > 1?' \times.':'time.')+'\r\n');
	}
	if(player.turned_down_date > 0) {
		if(player.hooker==0) {
			console.crlf();
			ansic(8);
			console.print("You have not had a date yet!  Are you 'funny'?"+'\r\n');
			console.crlf();
			ansic(0);
		}
		else {
			console.print("You've turned down "+player.turned_down_date+' '+(player.turned_down_date > 1?'dates':'date')+' with Kathy.'+'\r\n');
		}
	}
	if(player.won_today > 0)
		console.print('Today, you have won '+player.won_today+'.'+'\r\n');
	if(player.won_today < 0)
		console.print('Today, you have lost '+player.won_today+'.'+'\r\n');
	if(player.won_today == 0)
		console.print('You have not won or lost today.'+'\r\n');
	if(player.bruno > 0)
		print('You have seen Bruno '+player.bruno+' times.'+'\r\n');
	tleft();
	console.print('Time Left: '+tlef()+'\r\n');
	console.crlf();
	console.crlf();
	console.pause();
}

function sell_to_bruno(temp_money)
{
	console.print("You don't have the funds, so a large man named Bruno"+'\r\n');
	console.print('comes over, and'+'\r\n');
	do {
		switch(++player.bruno) {
			case 1:
				console.print('you sell your plane ticket for $100!'+'\r\n');
				player.players_money += 100;
				temp_money += 100;
				break;
			case 2:
				console.print('you sell your solid gold watch for $1,000!'+'\r\n');
				player.players_money += 1000;
				temp_money += 1000;
				break;
			case 3:
				console.print('you sell Bruno your new limo for $8,000!'+'\r\n');
				player.players_money += 8000;
				temp_money += 8000;
				break;
			case 4:
				console.print('you sell Bruno your new home for $25,000!'+'\r\n');
				player.players_money += 25000;
				temp_money += 25000;
				break;
			case 5:
				console.print('you sell Bruno your business for $100,000!'+'\r\n');
				player.players_money += 100000;
				temp_money += 100000;
				break;
			case 6:
				console.print('you sell your virgin nieces'+'\r\n');
				console.print('(ages 12 & 13) into slavery to an arab prince'+'\r\n');
				console.print('for $200,000!'+'\r\n');
				player.players_money += 200000;
				temp_money += 200000;
				break;
			case 7:
				console.crlf();
				console.crlf();
				mswait(1000);
				console.print('The crowd around the table has become mighty quite.'+'\r\n');
				console.print('You realize, too late, that you have made a serious'+'\r\n');
				console.print('mistake, and have nothing left to offer.  Bruno, and'+'\r\n');
				console.print('two of his friends grab you, and take you for a ride'+'\r\n');
				console.print('in the Nevada desert.  After an hour of driving, they'+'\r\n');
				console.print('drag you out of the car, and proceed to break your arms'+'\r\n');
				console.print('and legs.  Then they get back into the car, leaving you'+'\r\n');
				console.print('laying there, to die.'+'\r\n');
				sysoplog('\r\n***** Bruno killed '+player.name+' *****');
				messagefile.open('a');
				messagefile.writeln('Bruno killed '+player.name+' on '+system.datestr()+'.\r\n');
				messagefile.close();
				mswait(5000);
				leave();
		}
		console.crlf();
		console.crlf();
		console.pause();
		if(temp_money < 0 && player.bruno != 7)
			console.print("You still don't have the funds so "+'\r\n');
		else
			console.crlf();
	} while(temp_money <= 0);
	return(temp_money);
}

function strangers_gets_more_money(strangers_temp_money)
{
	var i,x,temp;

	for(x=1; x <= 10; x++) {
		do {
			i=random(10000)+20000;
		} while(i%100);
	}
	console.print("The stranger's cash seems to have run out."+'\r\n');
	console.print(' He whispers something into the ear of one of the girls sitting'+'\r\n');
	console.print('around him, and she leaves, returning in a few minutes with an'+'\r\n');
	console.print('envelope.  The stranger puts down his cigar, and takes the envelope'+'\r\n');
	console.print('from the girl.  He then removes a large wad of money, hands it to'+'\r\n');
	console.print('another one of his girls, who rushes off.  Seconds later, you are'+'\r\n');
	console.print('amazed as he is credited with another '+temp+'\r\n');
	stranger.strangers_money += i;
	strangers_temp_money += i;
	console.crlf();
	console.pause();
	return(strangers_temp_money);
}

function yn()
{
	var ch;

	while(1) {
		switch(console.getkey(K_NOECHO).toUpperCase()) {
			case 'Y':
				console.writeln('Yes');
				return(true);
			case 'N':
			case '\r':
			case '\n':
				console.writeln('No');
				return(false);
		}
	}
}

function buy_from_bruno()
{
	var i,home,business,women,temp,nope;

	nope=false;
	home=50000;
	business=300000;
	women=500000;

	do {
		if(player.players_money > women && player.bruno == 6)
			i=6;
		else if(player.players_money > business && player.bruno == 5)
			i=5;
		else if(player.players_money > home && player.bruno == 4)
			i=4;
		else if(player.players_money > 25000 && player.bruno == 3)
			i=3;
		else if(player.players_money > 7000 && player.bruno == 2)
			i=2;
		else if(player.players_money > 250 && player.bruno == 1)
			i=1;
		else
			return;
		if(i) {
			console.print('You have '+format_money(player.players_money)+' dollars.'+'\r\n');
			console.crlf();
			switch(i) {
				case 1:
					console.print('You can now buy your airplane ticket back'+'\r\n');
					console.write('from Bruno for $150.  Do you wish to do so? ');
					if(yn()) {
						console.print('Bruno returns your plane ticket, relieving you of'+'\r\n');
						console.print('$150 of your hard earned money.'+'\r\n');
						player.bruno -= 1;
						player.players_money -= 150;
						console.crlf();
						console.pause();
					}
					else
						nope=true;
					break;
				case 2:
					console.print('You may now buy your watch back from Bruno for $1,250.'+'\r\n');
					console.write('Do you wish to do so?' );
					if(yn()) {
						console.print('Bruno smiles as he returns your watch from his arm,'+'\r\n');
						console.print('while taking your money'+'\r\n');
						player.bruno -= 1;
						player.players_money -= 1250;
						console.crlf();
						console.crlf();
						console.pause();
					}
					else
						nope=true;
					break;
				case 3:
					console.print('Bruno offers to return the limo for $12,500.'+'\r\n');
					console.write('Do you take him up on his offer? ');
					if(yn()) {
						console.print('Bruno returns the limo to you, and shakes'+'\r\n');
						console.print('your hand at your smart move.'+'\r\n');
						player.bruno -= 1;
						player.players_money -= 12500;
						console.crlf();
						console.crlf();
						console.pause();
					}
					else
						nope=true;
					break;
				case 4:
					console.print('Bruno offers to sell you your home back for $35,000.'+'\r\n');
					console.write('Do you want to buy your house back? ');
					if(yn()) {
						console.print('Bruno returns the deed to you.'+'\r\n');
						player.players_money -= 35000.;
						player.bruno -= 1;
						console.crlf();
						console.crlf();
						console.pause();
					}
					else
						nope=true;
					break;
				case 5:
					console.print('Bruno offers to sell your business back to you for $150,000.'+'\r\n');
					console.write('Do you want to buy your business back? ');
					if(yn()) {
						console.print('Bruno returns control of the business back to you,'+'\r\n');
						console.print('shaking your hand at a smart move.'+'\r\n');
						player.players_money -= 150000.;
						player.bruno -= 1;
						console.crlf();
						console.crlf();
						console.pause();
					}
					else {
						nope=true;
						console.print('Bruno shakes his head at that stupid move of yours.'+'\r\n');
						console.crlf();
					}
					break;
				case 6:
					console.print('Bruno walks up to you and says the arab prince has not left'+'\r\n');
					console.print('and might be willing to return your nieces back to you (for a small handling fee, of course).'+'\r\n');
					console.print("It seems he's got his eye out for the chorus line, and needs"+'\r\n');
					console.print('a little extra cash.'+'\r\n');
					console.crlf();
					console.crlf();
					console.write('Do you let Bruno retrieve your nieces from the Prince? ');
					if(yn()) {
						console.crlf();
						console.print('Bruno returns and says the arab will return them for $250,000.'+'\r\n');
						console.print('You hand Bruno the money, and seconds later, they are returned.'+'\r\n');
						console.print('to their hotel room, not even knowing they had been sold.'+'\r\n');
						console.crlf();
						console.print('(Bruno had fed them mickeys before taking them to the'+'\r\n');
						console.print('arab, and luckily for you, the drug had not worn off yet.'+'\r\n');
						console.crlf();
						player.bruno -= 1;
						player.players_money -= 250000.;
						console.crlf();
						console.crlf();
						console.pause();
					}
					else {
						nope=true;
						console.print('Bruno walks away, sadly shaking his head, thinking about'+'\r\n');
						console.print('how stupid you are being.'+'\r\n');
						console.crlf();
					}
					break;
			}
		}
	} while(!nope);
}

function strange_things(i)
{
	switch(i) {
		case 1:
			if(roul) {
				console.print('You throw the coupier a 10 dollar chip in the hopes'+'\r\n');
				console.print('that it will improve your luck.'+'\r\n');
				player.players_money -= 10;
				console.crlf();
				console.crlf();
				console.pause();
			}
			break;
		case 2:
			console.print('Feeling thirsty, you order a drink from one of the many'+'\r\n');
			console.print('cocktail waitresses wandering buy.  After about 30 seconds'+'\r\n');
			console.print('you are amazed when she returns with your drink.  You'+'\r\n');
			console.print('give her a 10 dollar chip in appreciation (not to mention'+'\r\n');
			console.print('her good looks).'+'\r\n');
			player.players_money -= 10;
			console.crlf();
			console.crlf();
			console.pause();
			break;
		case 3:
			console.print('You notice something strange happening over in the corner.'+'\r\n');
			console.print('A golden creature seems to be approaching you, limping along'+'\r\n');
			console.print('the corridor in what seems to be a fast motion.'+'\r\n');
			console.crlf();
			console.crlf();
			console.print("What's this?  He's a robot!  He is mumbling under his"+'\r\n');
			console.print("breath, and as you strain to hear what he's saying, you"+'\r\n');
			console.print('catch a few phrases.  He seems to be complaining, mumbling'+'\r\n');
			console.print('something about Earthmen, a Heart of Gold, stupid mice'+'\r\n');
			console.print('and the number 42.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.print('A 2-headed man runs up to him, and they stand there not'+'\r\n');
			console.print('15 feet from you.  They talk, then you see the robot'+'\r\n');
			console.print('nod his head in agreement, and they disappear before your'+'\r\n');
			console.print('eyes.'+'\r\n');
			console.crlf();
			console.crlf();
			console.print('You pick up your drink, and down it in one large swallow, not'+'\r\n');
			console.print('believing anything you have just seen.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			break;
		case 4:
			console.print('As you set your drink on the table, you hear a deep rumble'+'\r\n');
			console.print('from outside the large, one piece mirror that covers the southern wall'+'\r\n');
			console.print('of the casino.  All of a sudden, with a large smash, a rather'+'\r\n');
			console.print('large sports car comes rushing through the mirror, quickly'+'\r\n');
			console.print('followed by 4 rather large guys on motorcycles.  The car lands'+'\r\n');
			console.print('in the middle of the casino.  Without notice, a slim, young'+'\r\n');
			console.print('looking man jumps out, pulls out a gun and manages to kill'+'\r\n');
			console.print('3 of the 4 cyclists.  By this time.  you are on the floor,'+'\r\n');
			console.print('hiding under the table with about 10 other people.  The'+'\r\n');
			console.print('young man and the cyclist get into a huge fight, rolling'+'\r\n');
			console.print('off tables, throwing each other into the slot machines, knocking'+'\r\n');
			console.print('over other patrons.  Kinda like what you would see in an action adventure'+'\r\n');
			console.print('film.  The young man, who has astounded you with his quick'+'\r\n');
			console.print('right hooks, slams the cyclist into the wall, finally knocking'+'\r\n');
			console.print('him out.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.print('The young man gets up, walks over to you, and asks'+'\r\n');
			console.print('what you are drinking.  You tell him a 7&7, and he tells you'+'\r\n');
			console.print("that is a bad choice, then proceeds to order 2 Martini's,"+'\r\n');
			console.print('shaken, not stirred.  You ask him his name, and he responds'+'\r\n');
			console.print("with \"Bond, James Bond.\".  The martini's arrive, he hands you one"+'\r\n');
			console.print('and proceeds to drink his right on down.  With that over, he'+'\r\n');
			console.print('then picks up a nearby'+'\r\n');
			console.print('girl, gets into his car, and exits the same way he entered,'+'\r\n');
			console.print('making another loud crash as he tears down another wall'+'\r\n');
			console.crlf();
			console.crlf();
			console.print('  Almost immidately, the usher-boys have started'+'\r\n');
			console.print('cleaning things up, and within minutes, things are back'+'\r\n');
			console.print('to normal...'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.print('You pick up the martini left on the table, and swallow it in one'+'\r\n');
			console.print('large gulp, not quite believing anything that has just happened.'+'\r\n');
			console.crlf();
			console.crlf();
			break;
		case 5:
			console.print('You stagger a minute and notice that your drinks seems'+'\r\n');
			console.print('a bit stronger.  Just then, you hear a small, quite voice'+'\r\n');
			console.print('behind you say, "Excuse me sir.".  You turn around and'+'\r\n');
			console.print('see a young lady with ruby slippers and carrying a picnic'+'\r\n');
			console.print('basket.  She asks, "Could you tell me in which direction'+'\r\n');
			console.print("Kansas is?  I've gotten lost in a storm and just"+'\r\n');
			console.print("can't seem to find my way back home.\"  You think to"+'\r\n');
			console.print("yourself that you've seen this girl someplace before,"+'\r\n');
			console.print("but you just can't seem to place her,"+'\r\n');
			console.print("and before you know it, you've pointed her in the"+'\r\n');
			console.print('direction of the teleporter.  She says, "Thank you sir.'+'\r\n');
			console.print(' Come on Toto, Aunt-Em is going to be worried about us."'+'\r\n');
			console.print(' And with that, you see a little dog pop'+'\r\n');
			console.print('out of the basket just long enough to bark, before'+'\r\n');
			console.print('being teleported back to Kansas.  You take another drink,'+'\r\n');
			console.print('and pass the whole thing off as a trick done'+'\r\n');
			console.print('by the magacian on stage.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			break;
		case 6:
			console.print("One of the many waitresses comes over and asks you if you'd"+'\r\n');
			console.print('like a drink.  You are slightly intimidated by her beautiful'+'\r\n');
			console.print('body, and start to stutter.  The girls standing around the'+'\r\n');
			console.print('stranger giggle at your nervousness.  You manage to get out'+'\r\n');
			console.print('a yes, and she leaves for a few minutes and returns with your'+'\r\n');
			console.print('drink.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			break;
	}
}

function more(i)
{
	var t,r,temp;

	switch(i) {
		case 1:
			console.print('You stare at the stranger and how quickly he can drink'+'\r\n');
			console.print('his drinks!  In the reflection of the bourbon, you'+'\r\n');
			console.print('notice something flashing off to the corner of the casino.  You'+'\r\n');
			console.print('glance over in that direction and see 3 people'+'\r\n');
			console.print('materializing.  You notice two figures that'+'\r\n');
			console.print('human, but the third looks like a cross'+'\r\n');
			console.print('between a man and the devil!'+'\r\n');
			console.crlf();
			console.crlf();
			console.print('The one in the middle approaches you and asks, "Excuse'+'\r\n');
			console.print('me Sir, but could you direct me to George and Gracey?"'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.print('You look in puzzlement at the three men and the one in'+'\r\n');
			console.print('the middle responds again.'+'\r\n');
			console.crlf();
			console.print('"The two humpback whales..  You know.." he says.'+'\r\n');
			console.print('The one that looks like the devil says, "Admiral, I'+'\r\n');
			console.print("don't think he knows.  Maybe we should find them"+'\r\n');
			console.print('ourselves."'+'\r\n');
			console.crlf();
			console.crlf();
			console.print('And with that, they both exit out the doors to the far right...'+'\r\n');
			console.crlf();
			console.crlf();
			pausescr;
			break;
		case 2:
			console.print('You hear a crash and see a large creature rushing'+'\r\n');
			console.print('through the doors!  He looks like a troll, but you'+'\r\n');
			console.print("wouldn't know one if you saw one.  He lets out a roar,"+'\r\n');
			console.print("and runs into the men's bathroom."+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.print('Not more than 2 seconds pass before you see a '+'\r\n');
			console.print('large man run through the same broken door.  He is'+'\r\n');
			console.print('carrying a lantern and a rather antique looking sword.'+'\r\n');
			console.print('He approaches you.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			ansic(4);
			console.print('>ASK MAN ABOUT TROLL'+'\r\n');
			ansic(4);
			console.print(">\"Ah yes...  He's in the bathroom.\""+'\r\n');
			ansic(4);
			console.print('>SAY THANK YOU.'+'\r\n');
			mswait(2000);
			console.crlf();
			ansic(4);
			console.beep(3);
			console.print('Your score is 420 in 4200 moves'+'\r\n');
			console.beep(3);
			ansic(0);
			console.crlf();
			console.pause();
			console.print('and with that over with, he runs into the bathroom...'+'\r\n');
			console.print('You return to the game..'+'\r\n');
			break;
		case 3:
			console.print('All of the sudden your concentration is broken as you'+'\r\n');
			console.print('hear the shrill of alarms and buzzers.  You notice that'+'\r\n');
			console.print('a man at one of the other tables is yelling.  Bruno goes over'+'\r\n');
			console.print('to that man and picks him up off his feet as if he was'+'\r\n');
			console.print('nothing.  The man is screaming and dragging his'+'\r\n');
			console.print('feet.  As Bruno walks by you, the man throws a little'+'\r\n');
			console.print('black box at you.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.print('Bruno carries the man to an airlock, where the bartender'+'\r\n');
			console.print('is standing.  The bartender pushes a button, and Bruno'+'\r\n');
			console.print('shoves the man into it.  Then, the bartender pushes another'+'\r\n');
			console.print('button, and you hear the whooshing sound of air rushing out. '+'\r\n');
			console.print('Seconds later, you notice the man float by one of the windows'+'\r\n');
   			console.print('just before his body explodes.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
			console.write('Do you pick up the box? ');
			if(yn()) {
				console.print("You examine the box, and notice it's a calculator for determining "+'\r\n');
				console.print('the winning combinations for slot machines and roulette wheels!'+'\r\n');
				console.write('Do you keep the box? ');
				if(yn()) {
					console.print('Bruno comes back over, looking around, and notices the box'+'\r\n');
					console.print('in your hand.  The next thing you know, you are standing'+'\r\n');
					console.print('in the airlock.  You look around, trying to find a way to'+'\r\n');
					console.print('escape, then......'+'\r\n');
					console.crlf();
					player.bruno=100;
					sysoplog('\r\n***** Bruno killed '+player.name+' *****');
					messagefile.open('a');
					messagefile.writeln('Bruno killed '+player.name+' on '+system.datestr()+' for cheating.\r\n');
					messagefile.close();
					mswait(5000);
					leave();
				}
				else {
					console.print('You call Bruno over, and hand him the box.  He thanks you,'+'\r\n');
					console.print("and in return hands you the man's Milliways Credit"+'\r\n');
					console.print('Card.  You take the card, and hand yours and it to one'+'\r\n');
					console.print('of the hostesses, who returns in a few minutes with your'+'\r\n');
					console.print('card.'+'\r\n');
					console.crlf();
					for(t=1; t<=30; t++)
						r=random(100)*100;
					console.print('You are '+format_money(r)+' richer for being honest!'+'\r\n');
					player.players_money += r;
					console.crlf();
					console.pause();
				}
			}
	}
}

function player_died()
{
	player.used_condom=false;
	player.players_money=1000;
	player.last_on=system.datestr;
	player.times_on=0;
	player.hooker=0;
	player.screwed=0;
	player.condom=0;
	player.bruno=0;
	player.aids=0;
	player.played_slots=0;
	player.played_roulette=0;
	player.played_twenty1=0;
	player.played_baccarat=0;
	player.turned_down_date=0;
	player.won_today=-1000000;
}

function died()
{
	consle.print('OH NO!  You have collapsed inside the Casino and have been rushed to the');
	consle.print('hospital.  After several days, a doctor comes in with the bad news');
	consle.print('of all the tests they have run on you.  You have AIDS, and they want');
	consle.print('to know of all the people you have had sexual contact with in the past');
	consle.print('several years.  Your mind is so messed up from the news, you can');
	consle.print('only think of Kathy.  You give them her name, and learn she has died');
	consle.print('herself, only 2 days ago.');
	consle.crlf();
	consle.print('Well, better luck in your next life '+player.name+'!');
	messagefile.open('a');
	messagefile.writeln(player.name + ' caught the deadly disease and died on '+system.datestr()+'.');
	messagefile.close();
	player_died();
	leave();
}

function stranger_dates()
{
	var h,i;

	if(!stranger_dates_kathy) {
		h=random(100);
		if(h>75) {
			console.crlf();
			if(player.hooker==0) {
				console.print('  A beautiful blonde comes up to the stranger and asks him'+'\r\n');
				console.print('if he would like to go out with her.  '+'\r\n');
			}
			else
				console.print('  You see Kathy approach the stranger and ask him for a date.  '+'\r\n');
			if(h>90) {
				console.print('The stranger looks her over, gets up and tells you that'+'\r\n');
				console.print("maybe he'll see you later.  He then takes her arm, and they"+'\r\n');
				console.print('leave.'+'\r\n');
				console.crlf();
				stranger.dating_kathy=true;
				stranger_dates_kathy=true;
				console.crlf();
				console.pause();
			}
			else if(h>85) {
				console.print('The stranger looks at her, and says something to the showgirls'+'\r\n');
				console.print('sitting around him.  Two of them get up, grab her, and throw her'+'\r\n');
				console.print('out of the casino.'+'\r\n');
				console.crlf();
				console.crlf();
				console.pause();
				console.print('  You get up out of your chair, and leave the casino and find'+'\r\n');
				if(player.bruno==0)
					console.print('the young lady'+'\r\n');
				else
					console.print('Kathy'+'\r\n');
				console.print('standing outside the casino, crying.  You ask her if there is'+'\r\n');
				console.print('anything you can do to help her.  She looks at you, noticing'+'\r\n');
				console.print('what appears to be concern in your eyes, and asks you if you'+'\r\n');
				console.print('would like to keep her company tonight.'+'\r\n');
				console.crlf();
				console.crlf();
				console.write('Do you go with her? ');
				if(yn())
					date_kathy();
				else {
					console.crlf();
					player.turned_down_date++;
					console.print("  You tell her that you can't leave right now, but you"+'\r\n');
					console.print('would be glad to get her a taxi to take her back home.'+'\r\n');
					console.print('She accepts, saying that she would like to see you again'+'\r\n');
					console.print('sometime soon.  You agree to this, and hail a taxi for her.'+'\r\n');
					console.print(' She gets into the taxi, and leaves.  '+'\r\n');
					if(player.condom==0) {
						console.print('As the taxi drives off'+'\r\n');
						console.print('you notice a small package laying on the sidewalk.'+'\r\n');
						console.crlf();
						console.crlf();
						console.write('Do you pick it up? ');
						if(yn()) {
							console.crlf();
							player.condom++;
							console.print('It is a condom.  Your mind begins to wonder about'+'\r\n');
							console.print('what kind of person'+'\r\n');
							if(player.bruno==0)
								print('the young lady');
							else
								print('Kathy');
							console.print('might be.'+'\r\n');
							console.crlf();
							console.crlf();
							console.pause();
						}
					}
				}
			}
			else {
				console.print("The stranger tells her to get lost, he doesn't have time to mess"+'\r\n');
				console.print('with her at the moment.'+'\r\n');
				console.crlf();
			}
		}
	}
	else {
		stranger.dating_kathy=false;
		stranger_dates_kathy=false;
		console.print('The stranger returns back to the casino, taking his seat near you.'+'\r\n');
		i=random(10)*100;
		stranger.strangers_money -= i;
	}
}

function date_kathy()
{
	var make_love;
	var i,y;
	var temp;

	i=random(10)*100;
	if(player.hooker==0) {
		player.hooker++;
		console.print('  The young lady introduces herself as Kathy.  '+'\r\n');
		console.print('The two of you go out and spend several hours together enjoying'+'\r\n');
		console.print('the nightlife.  After a while, you and Kathy end up in her suite.  '+'\r\n');
		console.print('She tells you to mix a couple of drinks while she goes and gets'+'\r\n');
		console.print('into something a little more comfortable.  You head towards the'+'\r\n');
		console.print('bar and and mix 2 drinks, then sit down on the luscious couch.'+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
		console.print('  She comes back out of the bedroom, wearing a long black robe'+'\r\n');
		console.print('that covers her from her neck down to her ankles.  She sits down'+'\r\n');
		console.print('next to you, takes her drink, and finishes it off in one swallow.  '+'\r\n');
		console.print('She turns to you, leans closer, and begins to nibble on your earlobe.'+'\r\n');
		console.crlf();
		console.print('"'+user.name+'", she whispers into your ear, "Would you like to make'+'\r\n');
		console.print('love to me?"'+'\r\n');
		console.crlf();
		console.crlf();
	}
	else {
		player.hooker++;
		console.print('  You and Kathy again go out, spending several hours'+'\r\n');
		console.print('enjoying the nightlife of Los Vegas, and once again,'+'\r\n');
		console.print('end up back in her apartment.'+'\r\n');
		console.crlf();
		console.crlf();
		console.print('  Again, she has you mix a couple of drinks, and she heads'+'\r\n');
		console.print('towards the bedroom to change her clothes.  After a few'+'\r\n');
		console.print('minutes, she comes back out, and asks'+'\r\n');
		console.print('if you would like to make love to her.'+'\r\n');
		console.crlf();
		console.crlf();
	}
	console.write('Do you make love to Kathy? ');
	make_love=yn();
/*{********** BEGIN MAKE LOVE **************}*/
	if(make_love) {
		if(player.condom) {
			console.write('Do you use the condom you found? ');
			if(yn()) {
				player.condom=0;
				player.used_condom=true;
			}
			else {
				player.aids++;
			}
		}
		if(player.screwed==0) {
			console.print('  You are stunned, although deep in you mind, you knew this was going'+'\r\n');
			console.print('happen sooner or later.  You turn to look at her and her lips approach yours...'+'\r\n');
			console.crlf();
			console.crlf();
			mswait(5000);
			for(y=1; y<=10; y++)
				console.crlf();
			ansic(8);
			console.print("FOOLED YOU DIDN'T I!"+'\r\n');
			ansic(0);
			console.crlf();
			console.print("This is a family program, so leaving out the details, let's just say"+'\r\n');
			console.print('that you leave the suite 12 hours later, with a smile on your face and'+'\r\n');
			console.print('1000 dollars poorer.  You return to the table to continue playing.'+'\r\n');
			console.crlf();
			console.crlf();
			console.crlf();
			console.pause();
		}
		else {
			console.print('  Once again you and Kathy spend several lovely hours together, and you return'+'\r\n');
			console.print('to the Casino, $1000 poorer, but happy with your experience with her.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
		}
		player.players_money-=1000;
		player.screwed++;
	}
	else {
/*{********* BEGIN NO MAKE LOVE ***********}*/
		if(player.screwed==0) {
			console.print('  You politely refuse, saying that you believe that 2 people should'+'\r\n');
			console.print('not do such things unless they are deeply in love and are married.'+'\r\n');
			console.print('Kathy looks at you in surprise, but realizes that you mean what you'+'\r\n');
			console.print('say, and a wishful look comes across her face.  She kisses you'+'\r\n');
			console.print('on the cheek, and says that she hopes you and her will see each'+'\r\n');
			console.print('other again.'+'\r\n');
			console.crlf();
			console.crlf();
			console.pause();
		}
		else {
			console.print('  You tell Kathy that you just are not in the mood right now,'+'\r\n');
			console.print('but maybe later on.  You give her a quick kiss, a $100 bill, then return to'+'\r\n');
			console.print('the Casino to continue playing.'+'\r\n');
			player.players_money -= 100;
			console.crlf();
			console.crlf();
			console.pause();
		}
	}
	if(player.screwed==0 && player.hooker >= 10 && player.players_money > win_money && player.condom > 0) {
		console.crlf();
		console.pause();
		console.print("  You notice tears running out of Kathy's eyes, and she turns her"+'\r\n');
		console.print('to try and hide them from you.  You hold her in your arms, and ask'+'\r\n');
		console.print('her what is wrong.  She says she has a confession to make, that she'+'\r\n');
		console.print("hasn't been truely honest with you.  She explains that she is a high"+'\r\n');
		console.print('class hooker, getting over $1000 a night, and yet, because of you, she'+'\r\n');
		console.print("doesn't quite know what is going on with her life, and has fallen"+'\r\n');
		console.print('madly in love with you.'+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
		console.print('  You turn your face towards hers, look her in the eyes, and tell her'+'\r\n');
		console.print('that you have known she was a hooker for quite a while, and that'+'\r\n');
		console.print('you had fallen in love with her from the moment you first laid eyes on'+'\r\n');
		console.print('her, but were afraid to say anything.  She asks you how you knew, and'+'\r\n');
		console.print('you show her the condom you had found.'+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
		console.print('  Kathy begins crying all over again, and you hold her tightly'+'\r\n');
		console.print('in your arms.  After a short time, the closeness of her body'+'\r\n');
		console.print('to yours starts to make you feel very funny.  You look her in the'+'\r\n');
		console.print('eyes, and begin to kiss her.  She responds, drawing her body'+'\r\n');
		console.print('even closer to yours.'+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
		console.print('The next morning, you wake up and look over at Kathy laying there'+'\r\n');
		console.print('next to you.  She wakes up, smiles, and kisses you shyly.  You'+'\r\n');
		console.print('are feeling strange, and ask her, stutteringly, if she will'+'\r\n');
		console.print('marry you.  To your surprise, she accepts, and the two of you'+'\r\n');
		console.print('run out, and within 3 weeks are married.'+'\r\n');
		console.crlf();
		console.crlf();
		console.print('  With the money that you have won here at the casino, you and her'+'\r\n');
		console.print('open up a business in the mountains of North Carolina, where you'+'\r\n');
		console.print('and Kathy live happily ever after.'+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
		won=true;
	}
	if(!won) {
		console.print('Your date with the young lady costs you $'+i+' for dinner, shows, and drinks.'+'\r\n');
		console.crlf();
		player.players_money -= i;
	}
	if(won) {
		console.print('CONGRATULATIONS '+player.name+'\r\n');
		console.print('YOU HAVE WON THE GAME!'+'\r\n');
		sysoplog('\r\n **** '+player.name+' WON AT CASINO ****\r\n');
		console.print("If I was you, I'd brag about it."+'\r\n');
		console.print("It's not designed to be won easily!"+'\r\n');
		messagefile.open('a');
		messagefile.writeln(player.name+' won on '+system.datestr()+'.');
		messagefile.close();
		leave();
	}
}

function refuse_date()
{
	if(player.hooker==0) {
		console.print('You politely refuse the invitation, saying that you'+'\r\n');
		console.print("really don't care to see the nightlife right now."+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
	}
	else {
		console.print('You politely turn down the invitation, telling Kathy that'+'\r\n');
		console.print('you are feeling lucky, and want to keep trying to win,'+'\r\n');
		console.print('but maybe you and her can get together later.'+'\r\n');
		console.crlf();
		console.crlf();
		console.pause();
	}
	player.turned_down_date++;
}

function hooker()
{
	var temp,temp_num,y,i,dated,condom;

	if(player.hooker==0) {
		console.print('  A beautiful blonde comes up to you and sits down beside you.'+'\r\n');
		console.print('She mentions that she has been watching you play, and would like'+'\r\n');
		console.print('to get to know you better.  She asks you if you would like to go'+'\r\n');
		console.print('out and see some of the nightlife around town.'+'\r\n');
		console.crlf();
		console.crlf();
		console.write('Do you go with the young lady? ');
		if(yn())
			date_kathy();
		else
			refuse_date();
	}
	else if(player.hooker > 0) {
		console.print('Kathy comes up to you and asks if you would like to go out again.'+'\r\n');
		console.crlf();
		console.crlf();
    	console.write('Do you go with her? ');
		if(yn())
			date_kathy();
		else
			refuse_date();
	}
}

function check_random()
{
	var hooker_money=100000.;
	var i;

	tleft();
	i=random(500)+1;
	if(i>=200 && i<=300)
	i=200;
	if(i>=350 && i<=400)
	i=350;
	if(i>=450 && i<=500)
	i=450;

	switch(i) {
		case 50:
			if(roul)
				strange_things(1);
			break;
		case 60:
			strange_things(2);
			break;
		case 70:
			strange_things(3);
			break;
		case 80:
			strange_things(4);
			break;
		case 90:
			strange_things(5);
			break;
		case 100:
			strange_things(6);
			break;
		case 110:
			if(roul) {
				console.print('You notice the showgirls talking amongst themselves, and you'+'\r\n');
				console.print('hear them talking about the number '+(random(38)+1)+'\r\n');
				console.crlf()
				console.crlf()
			}
			break;
		case 120:
			more(1);
			break;
		case 130:
			more(2);
			break;
		case 140:
			more(3);
			break;
		case 200:
			if(player.aids > 0) {
				switch(player.aids) {
					case 1:
					case 2:
						console.print('You are feeling weak.'+'\r\n');
						break;
					case 3:
					case 4:
						console.print('You are feeling really bad!'+'\r\n');
						break;
					case 5:
						died();
				}
			}
			break;
		case 350:
			if(player.bruno==0 && player.players_money > player.hooker*hooker_money && (!stranger_dates_kathy))
				hooker();
			break;
		case 450:
			if(player.players_money > player.hooker*hooker_money)
				stranger_dates();
	}
}
