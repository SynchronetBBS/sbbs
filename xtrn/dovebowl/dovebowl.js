/*
 * Allows sports picks over DOVE-Net
 */

/*
 * INI file is the first argument.
 * This allows multiple leagues per invocation
 * or one league per invocation.
 */

load("lightbar.js");

//var sports=new MsgBase('DOVE-SPO');
var sports=new MsgBase('ENTERTAI');
if(!sports.open())
	return_error("Cannot open message base!",1);

var path;
try { barfitty.barf(barf); } catch(e) { path = e.fileName; }
path = path.replace(/[^\/\\]*$/,'');

var inifile=new File(path + "dovebowl.ini");
var tmpfile=new File(path+format("%04d.tmp",user.number));

var leaguefile;
if(!inifile.open("r+", true))
	return_error("Cannot open "+inifile.name,1);

var leagues=inifile.iniGetSections();
if(leagues.length == 0)
	return_error("No leagues configured",1);

var leagueslb=new Lightbar();

for(i=0; i<leagues.length; i++) {
	leagueslb.add(leagues[i],leagues[i]);
}
leagueslb.xpos=inifile.iniGetValue(null,"LeaguesXPos",1);
leagueslb.ypos=inifile.iniGetValue(null,"LeaguesYPos",2);

var ownerlb=new Lightbar();
ownerlb.add("|Create new set of games","create");
ownerlb.add("Enter |Results","results");
ownerlb.add("|Play","play");
ownerlb.add("|Quit","quit");
ownerlb.xpos=inifile.iniGetValue(null,"OwnerXPos",1);
ownerlb.ypos=inifile.iniGetValue(null,"OwnerYPos",2);

while(1) {
	var league;
	var done_league=false;

	/* Select the league */
	if(leagues.length == 1)
		league=leagues[0];
	else {
		console.clear();
		console.printfile(path+inifile.iniGetValue(null,"LeaguesImage","leagues.asc"));
		league=leagueslb.getval();
	}

	leaguefile=new File(path+league.replace(/[\[\]\/\\\=\+\<\>\:\;\"\,\*\|\^]/g,''));
	if(!leaguefile.open(file_exists(leaguefile.name)?"r+":"w+",true,4096))
		return_error("Cannot open "+leaguefile.name,1);
	inifile.iniSetValue(league,"MessagePointer",read_messages(league, inifile.iniGetValue(league, "MessagePointer", 0)));

	while(!done_league) {
		/* Read the league owner ID */
		owner=inifile.iniGetValue(league, "Owner");
		if(owner.toUpperCase() == (user.alias+"@"+system.qwk_id).toUpperCase()) {
			console.clear();
			console.printfile(path+inifile.iniGetValue(null,"OwnerImage","owner.asc"));

			/* Owners get a special menu */
			switch(ownerlb.getval()) {
				case 'create':
					create_games(league);
					continue;
				case 'results':
					enter_results(league);
					continue;
				case 'play':
					play(league);
					break;
				case 'quit':
					done_league=true;
					break;
			}
		}
		else {
			/* Players menu */
			play(league);
			done_league=true;
		}
	}

	leaguefile.close();
	return_error("Program incomplete!", 1);
}

function play(league)
{
	var sets=leaguefile.iniGetSections();
	var i;
	var games;
	var gameslb=new Lightbar();
	gameslb.current=-1;

	console.clear();
	console.printfile(path+leaguefile.iniGetValue(null,"GamesImage","gamelist.asc"));
	console.gotoxy(inifile.iniGetValue(null,"CreateXPos",1),inifile.iniGetValue(null,"CreateYPos",2));

	for(set in sets) {
		var first=new Date(leaguefile.iniGetValue(sets[set], "FirstDate", 0));
		if(first > (new Date())) {
			if(user_voted(sets[set]))
				gameslb.add(sets[set], sets[set], undefined, '[', ']');
			else {
				gameslb.add(sets[set], sets[set]);
				if(gameslb.current==-1)
					gameslb.current=gameslb.items.length-1;
			}
		}
	}
	if(gameslb.current==-1) {
		writeln("No games in this league yet... bug "+inifile.iniGetValue(league, "Owner")+"about that.");
		writeln("Press any key to continue");
		console.getkey();
		return;
	}
	var set=gameslb.getval();
	make_picks(league, set, false);
}

function onevote(away, home, vote, results, xpos, ypos)
{
	var votelb=new Lightbar;

	votelb.xpos=xpos;
	votelb.ypos=ypos;
	votelb.direction=1;
	votelb.hblanks=1;
	votelb.current=vote;
	votelb.add(away, 0, 25, vote==0?'[':' ', vote==0?']':' ');
	votelb.add(home, 1, 25, vote==1?'[':' ', vote==1?']':' ');
	votelb.add(results?'Tie':'Abstain', 2, results?3:7, vote==2?'[':' ', vote==2?']':' ');
	return(votelb.getval());
}

function make_picks(league, set, results)
{
	var games;
	var game=0;
	var gamedata=new Array();
	var PickXPos=parseInt(inifile.iniGetValue(null,"PickXPos",1));
	var PickYPos=parseInt(inifile.iniGetValue(null,"PickYPos",2));
	var maxawaylen=0;
	var maxhomelen=0;

	games=leaguefile.iniGetValue(set, "Games", 0);
	for(i=0; i<games; i++) {
		gamedata[i]=new Object;
		gamedata[i].vote=2;
		gamedata[i].away=leaguefile.iniGetValue(set, "Game"+(i+1)+"Away", '');
		if(gamedata[i].away.length > maxawaylen)
			maxawaylen = gamedata[i].away.length;
		gamedata[i].home=leaguefile.iniGetValue(set, "Game"+(i+1)+"Home", '');
		if(gamedata[i].home.length > maxhomelen)
			maxhomelen = gamedata[i].home.length;
		var votes=leaguefile.iniGetValue(set, "Game"+(i+1)+"AwayVotes", '');
		if(vote_check(votes))
			gamedata[i].vote=0;
		votes=leaguefile.iniGetValue(set, "Game"+(i+1)+"HomeVotes", '');
		if(vote_check(votes))
			gamedata[i].vote=1;
		gamedata[i].orig_vote=gamedata[i].vote;
	}

	var current=-1;
	do {
		var gameslb=new Lightbar;
		gameslb.current=current;
		gameslb.xpos=PickXPos;
		gameslb.ypos=PickYPos;
		console.clear();
		console.printfile(path+leaguefile.iniGetValue(null,"PicksImage","picklist.asc"));
		console.gotoxy(PickXPos,PickYPos);

		for(i=0; i<games; i++) {
			var c=gamedata[i].vote;
			var home=gamedata[i].home;
			var away=gamedata[i].away;
			var gamestr=format("%s%-25.25s%s %s%-25.25s%s %s%s%s", c==0?'[':' ', away, c==0?']':' ', c==1?'[':' ', home, c==1?']':' ', c==2?'[':' ', results?"Tie":"Abstain", c==2?']':' ');
			gameslb.add(gamestr, i.toString());
		}
		gameslb.add("--------------------------- --------------------------- -----"+(results?'':'----'),-1);
		if((game=gameslb.getval()) != -1) {
			gamedata[game].vote=onevote(gamedata[game].away, gamedata[game].home, gamedata[game].vote, results, PickXPos, PickYPos+parseInt(game));
			set_vote(user.alias+'@'+system.qwk_id, set, game, vote);
		}
		current=gameslb.current;
	} while(game != -1);

	/* Post picks */
	var subject=league.replace(/[\[\]\/\\\=\+\<\>\:\;\"\,\*\|\^]/g,'') + " " + set + " picks";
	if(!tmpfile.open("w",false))
		return_error("Cannot open "+tmpfile.name);
	tmpfile.writeln("=== "+set+" Picks ===");
	tmpfile.writeln("");
	var newvotes=0;

	for(i=0; i<games; i++) {
		var vote="Abstain";
		if(gamedata[i].vote != gamedata[i].orig_vote)
			newvotes++;
		switch(gamedata[i].vote) {
		case 0:	/* Away */
			vote=gamedata[i].away;
			break;
		case 1:	/* Home */
			vote=gamedata[i].home;
			break;
		}

		tmpfile.writeln(format("%*s at %-*s [%s]", maxawaylen, gamedata[i].away, maxhomelen, gamedata[i].home, vote));
	}
	tmpfile.close();
	if(newvotes > 0)
		send_message(user.alias, "DoveBowl", subject);
	inifile.iniSetValue(league,"MessagePointer",read_messages(league, inifile.iniGetValue(league, "MessagePointer", 0)));
}

function vote_check(votestr)
{
	var votes=votestr.split('|');
	for(vote in votes) {
		if(votes[vote]==user.alias+'@'+system.qwk_id) {
			return(true);
		}
	}

	return(false);
}

function user_voted(set)
{
	var games;
	var i;

	games=leaguefile.iniGetValue(set, "Games", 0);
	for(i=0; i<games; i++) {
		var votestr=leaguefile.iniGetValue(set, "Game"+(i+1)+"HomeVotes","");
		if(vote_check(votestr))
			return(true);
		votestr=leaguefile.iniGetValue(set, "Game"+(i+1)+"AwayVotes","");
		if(vote_check(votestr))
			return(true);
	}

	return false;
}

function remove_vote(votestr, alias)
{
	var votes=votestr.split('|');
	for(vote in votes) {
		if(votes[vote]==alias) {
			votes.splice(vote, 1);
			return(votes.join("|"));
		}
	}
	return(votes.join("|"));
}

function add_vote(votestr, alias)
{
	var votes=votestr.split('|');
	for(vote in votes) {
		if(votes[vote]==alias) {
			return(votestr);
		}
	}
	votes.push(alias);
	return(votes.join("|"));
}

function set_vote(alias, set, game, vote)
{
	var votestr=leaguefile.iniGetValue(set, "Game"+(parseInt(game)+1)+"AwayVotes","");

	switch(vote) {
	case '0':		/* Away */
		votestr=add_vote(votestr, alias);
		break;
	case '1':		/* Home */
	default:
		votestr=remove_vote(votestr, alias);
		break;
	}
	leaguefile.iniSetValue(set, "Game"+(parseInt(game)+1)+"HomeVotes",votestr);

	votestr=leaguefile.iniGetValue(set, "Game"+(parseInt(game)+1)+"HomeVotes","");
	switch(vote) {
	default:
	case '0':		/* Away */
		votestr=remove_vote(votestr, alias);
		break;
	case '1':		/* Home */
		votestr=add_vote(votestr, alias);
		break;
	}
	leaguefile.iniSetValue(set, "Game"+(parseInt(game)+1)+"AwayVotes",votestr);
}

function enter_results(league)
{
	var games;
	var game=0;
	var gamedata=new Array();
	var ResultXPos=parseInt(inifile.iniGetValue(null,"ResultXPos",1));
	var ResultYPos=parseInt(inifile.iniGetValue(null,"ResultYPos",2));
	var maxawaylen=0;
	var maxhomelen=0;

	games=leaguefile.iniGetValue(set, "Games", 0);
	for(i=0; i<games; i++) {
		gamedata[i]=new Object;
		gamedata[i].vote=2;
		gamedata[i].away=leaguefile.iniGetValue(set, "Game"+(i+1)+"Away", '');
		if(gamedata[i].away.length > maxawaylen)
			maxawaylen = gamedata[i].away.length;
		gamedata[i].home=leaguefile.iniGetValue(set, "Game"+(i+1)+"Home", '');
		if(gamedata[i].home.length > maxhomelen)
			maxhomelen = gamedata[i].home.length;
		var votes=leaguefile.iniGetValue(set, "Game"+(i+1)+"AwayVotes", '');
		if(vote_check(votes))
			gamedata[i].vote=0;
		votes=leaguefile.iniGetValue(set, "Game"+(i+1)+"HomeVotes", '');
		if(vote_check(votes))
			gamedata[i].vote=1;
		gamedata[i].orig_vote=gamedata[i].vote;
	}

	var current=-1;
	do {
		var gameslb=new Lightbar;
		gameslb.current=current;
		gameslb.xpos=ResultXPos;
		gameslb.ypos=ResultYPos;
		console.clear();
		console.printfile(path+leaguefile.iniGetValue(null,"ResultsImage","resultlist.asc"));
		console.gotoxy(ResultXPos,ResultYPos);

		for(i=0; i<games; i++) {
			var c=gamedata[i].vote;
			var home=gamedata[i].home;
			var away=gamedata[i].away;
			var gamestr=format("%s%-25.25s%s %s%-25.25s%s %s%s%s", c==0?'[':' ', away, c==0?']':' ', c==1?'[':' ', home, c==1?']':' ', c==2?'[':' ', results?"Tie":"Abstain", c==2?']':' ');
			gameslb.add(gamestr, i.toString());
		}
		gameslb.add("--------------------------- --------------------------- -----"+(results?'':'----'),-1);
		if((game=gameslb.getval()) != -1) {
			gamedata[game].vote=onevote(gamedata[game].away, gamedata[game].home, gamedata[game].vote, results, PickXPos, PickYPos+parseInt(game));
			set_vote(user.alias+'@'+system.qwk_id, set, game, vote);
		}
		current=gameslb.current;
	} while(game != -1);

	/* Post picks */
	var subject=league.replace(/[\[\]\/\\\=\+\<\>\:\;\"\,\*\|\^]/g,'') + " " + set + " results";
	if(!tmpfile.open("w",false))
		return_error("Cannot open "+tmpfile.name);
	tmpfile.writeln("=== "+set+" Results ===");
	tmpfile.writeln("");
	var newvotes=0;

	for(i=0; i<games; i++) {
		var vote="Not Played";
		if(gamedata[i].vote != gamedata[i].orig_vote)
			newvotes++;
		switch(gamedata[i].vote) {
		case 0:	/* Away */
			vote=gamedata[i].away;
			break;
		case 1:	/* Home */
			vote=gamedata[i].home;
			break;
		}

		tmpfile.writeln(format("%*s at %-*s [%s]", maxawaylen, gamedata[i].away, maxhomelen, gamedata[i].home, vote));
	}
	tmpfile.close();
	if(newvotes > 0)
		send_message(user.alias, "DoveBowl", subject);
	inifile.iniSetValue(league,"MessagePointer",read_messages(league, inifile.iniGetValue(league, "MessagePointer", 0)));
}

function create_games(league)
{
	var identifier="";
	var subject;
	var games=new Array();
	var maxawaylen=0;
	var game=0;
	var lastdate=new Date(1971,0,1);

	console.clear();
	console.printfile(path+inifile.iniGetValue(null,"CreateImage","create.asc"));
	console.gotoxy(inifile.iniGetValue(null,"CreateXPos",1),inifile.iniGetValue(null,"CreateYPos",2));
	console.write('Identifier (ie: "Week 1"): ');
	identifier = console.getstr(20);
	subject=league.replace(/[\[\]\/\\\=\+\<\>\:\;\"\,\*\|\^]/g,'') + " " + identifier + " games";
	if(!tmpfile.open("w",false))
		return_error("Cannot open "+tmpfile.name);
	tmpfile.writeln("Comments for "+identifier+":");
	tmpfile.close();
	console.editfile(tmpfile.name);
	tmpfile.open("a");
	tmpfile.writeln("");
	tmpfile.writeln("=== "+identifier+" Schedule ===");

	console.crlf();
	while(1) {
		var date;
		var curpos;
		var home;
		var away;
		var m;
		var d;
		var y;

		game++;
		console.write(format("Game: %2d\r\n",game));
		console.write("Date: ");
		curpos=console.getxy();
		while(1) {
			console.gotoxy(curpos.x, curpos.y);
			date=console.gettemplate("NN/NN/NN");
			if(date=='')
				break;
			var mdy=date.split('/');
			m=parseInt(mdy[0],10)-1;
			d=parseInt(mdy[1],10);
			y=parseInt(mdy[2],10)+2000;
			date=new Date(y,m,d);
			/* Validate date */
			if(date.getFullYear() != y || date.getMonth() != m || date.getDate() != d) {
				console.gotoxy(curpos.x+10, curpos.y);
				console.write("Invalid Date!");
				console.beep();
				continue;
			}
			break;
		}
		if(date=='') {
			game--;
			break;
		}
		console.gotoxy(16, curpos.y);
		console.write("Away: ");
		away=console.getstr(25);
		console.gotoxy(49, curpos.y);
		console.write("Home: ");
		home=console.getstr(25);
		games[game-1]=new Object;
		games[game-1].date=date;
		games[game-1].away=away;
		if(away.length > maxawaylen)
			maxawaylen = away.length+1;
		games[game-1].home=home;
		console.write("\r\n");
	}

	for(i=0; i<game; i++) {
		if(games[i].date.valueOf() != lastdate.valueOf()) {
			tmpfile.writeln('');
			tmpfile.writeln(format_date(games[i].date));
			lastdate=new Date(games[i].date.getTime());
		}
		tmpfile.writeln(format("%*s at %s", maxawaylen, games[i].away, games[i].home));
	}
	tmpfile.close();
	send_message(user.alias, "DoveBowl", subject);
	inifile.iniSetValue(league,"MessagePointer",read_messages(league, inifile.iniGetValue(league, "MessagePointer", 0)));
	file_remove(tmpfile.name);
}

function return_error(str, num)
{
	writeln(str);
	writeln("Press any key to continue");
	console.getkey();
	exit(num);
}

function format_date(dateval)
{
	var dow=new Array("Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday");
	var months=new Array("January","February","March","April","May","June","July","August","September","October","November","December");
	var ret='';

	d=dateval.getDate();
	y=dateval.getFullYear();

	ret=dow[dateval.getDay()];
	ret += ", "+months[dateval.getMonth()];
	ret += " "+d
/*	if(d>10 && d < 20)
		ret += "th";
	else switch(d%10) {
		case 1:
			ret += 'st';
			break;
		case 2:
			ret += 'nd';
			break;
		case 3:
			ret += 'rd';
			break;
		default:
			ret += 'th';
	} */
//	if(y!=(new Date).getFullYear())
		ret += ', '+y;
	return(ret);
}

function send_message(from_alias, to_alias, subject)
{
	var hdr=new Object();
	hdr.subject=subject;
	hdr.to=to_alias;
	hdr.from=from_alias;
	
	if(!tmpfile.open("r",false))
		return_error("Cannot open "+tmpfile.name);
	var body=tmpfile.readAll().join("\r\n");
	if(!sports.save_msg(hdr, body))
		return_error("Cannot send message");
	tmpfile.close();
}

function read_messages(league, ptr)
{
	var last_msg;
	var curr_msg;
	var hdr;
	var subject_prefix=league.replace(/[\[\]\/\\\=\+\<\>\:\;\"\,\*\|\^]/g,'')+' ';

	last_msg=sports.last_msg;
	for(curr_msg=ptr+1; curr_msg <= last_msg; curr_msg++) {
		hdr = sports.get_msg_header(curr_msg);
		if(hdr.to == 'DoveBowl') {
			if(hdr.subject.substr(0, subject_prefix.length) == subject_prefix) {
				if(hdr.subject.substr(-6)==' games') {
					/* Import new set of games */
					/* Verify this came from the league owner */
					owner=inifile.iniGetValue(league, "Owner");
					if(owner==hdr.from+"@"+((hdr.from_net_type==undefined || hdr.from_net_type==0)?system.qwk_id:((hdr.from_net_addr+'').replace(/^.*\//,'')))) {
						/* Create new set of games in the league file */
						var set=hdr.subject.substr(subject_prefix.length, hdr.subject.length - subject_prefix.length - 6);
						var games=0;
						var body=sports.get_msg_body(curr_msg);
						var body_lines=body.split(/\r?\n/);
						var started=false;
						var got_date=false;
						var first_date;
						var this_date;

						for(var line in body_lines) {
							if(body_lines[line]=="=== "+set+" Schedule ===")
								started=true;
							else if(started) {
								if(body_lines[line].length > 0) {
									if(body_lines[line].substr(0, 1) != ' ') {
										/* This is a new date */
										this_date=new Date(body_lines[line]);
										if(this_date.toString() != "Invalid Date") {
											if(got_date) {
												if(this_date < first_date)
													first_date = this_date;
											}
											else {
												first_date = this_date;
												got_date=true;
											}
										}
									}
									else {
										/* This is a game */
										if(got_date) {
											var teams=body_lines[line].replace(/^\s+/, '').split(/ at /, 2);
											leaguefile.iniSetValue(set, "Game"+(games+1)+"Away", teams[0]);
											leaguefile.iniSetValue(set, "Game"+(games+1)+"Home", teams[1]);
											leaguefile.iniSetValue(set, "Game"+(games+1)+"Date", this_date.toString());
											games++;
										}
									}
								}
							}
						}
						if(games > 0) {
							leaguefile.iniSetValue(set, "FirstDate", first_date.toString());
							leaguefile.iniSetValue(set, "Games", games);
						}
					}
				}
				else if(hdr.subject.substr(-6)==' picks') {
					/* Set of Votes */
					var set=hdr.subject.substr(subject_prefix.length, hdr.subject.length - subject_prefix.length - 6);
					var body=sports.get_msg_body(curr_msg);
					var body_lines=body.split(/\r?\n/);
					var started=false;
					var alias=hdr.from+"@"+((hdr.from_net_type==undefined || hdr.from_net_type==0)?system.qwk_id:((hdr.from_net_addr+'').replace(/^.*\//,'')));

					/* Read game data for this set */
					var games;
					var game=0;
					var gamedata=new Array();

					games=leaguefile.iniGetValue(set, "Games", 0);
					for(game=0; game<games; game++) {
						gamedata[game]=new Object;
						gamedata[game].away=leaguefile.iniGetValue(set, "Game"+(game+1)+"Away", '');
						gamedata[game].home=leaguefile.iniGetValue(set, "Game"+(game+1)+"Home", '');
					}

					/* Verify this was imported BEFORE the first game date */
					if(new Date(leaguefile.iniGetValue(set, "FirstDate", 0)) < new Date(hdr.when_imported_time*1000))
						continue;

					for(var line in body_lines) {
						if(body_lines[line]=="=== "+set+" Picks ===")
							started=true;
						else if(started) {
							if(body_lines[line].length > 0) {
								var m=body_lines[line].match(/^\s*(.*?) at (.*?)\s+\[(.*)\]$/);
								if(m != null) {
									/* Find the correct game */
									for(game=0; game<games; game++) {
										if(m[1]==gamedata[game].away && m[2]==gamedata[game].home)
											break;
									}
									if(game==games)
										continue;

									if(m[1]==m[3]) {
										set_vote(alias, set, game, '0');
									}
									else if(m[2]==m[3]) {
										set_vote(alias, set, game, '1');
									}
									else {
										set_vote(alias, set, game, '2');
									}
								}
							}
						}
					}
				}
				else if(hdr.subject.substr(-8==' results')) {
					/* Set of Votes */
					var set=hdr.subject.substr(subject_prefix.length, hdr.subject.length - subject_prefix.length - 8);
					var body=sports.get_msg_body(curr_msg);
					var body_lines=body.split(/\r?\n/);
					var started=false;
					var alias=hdr.from+"@"+((hdr.from_net_type==undefined || hdr.from_net_type==0)?system.qwk_id:((hdr.from_net_addr+'').replace(/^.*\//,'')));

					/* Read game data for this set */
					var games;
					var game=0;
					var gamedata=new Array();

					games=leaguefile.iniGetValue(set, "Games", 0);
					for(game=0; game<games; game++) {
						gamedata[game]=new Object;
						gamedata[game].away=leaguefile.iniGetValue(set, "Game"+(game+1)+"Away", '');
						gamedata[game].home=leaguefile.iniGetValue(set, "Game"+(game+1)+"Home", '');
					}

					/* Verify this was imported AFTER the first game date */
					if(new Date(leaguefile.iniGetValue(set, "FirstDate", 0)) > new Date(hdr.when_imported_time*1000))
						continue;

					for(var line in body_lines) {
						if(body_lines[line]=="=== "+set+" Results ===")
							started=true;
						else if(started) {
							if(body_lines[line].length > 0) {
								var m=body_lines[line].match(/^\s*(.*?) at (.*?)\s+\[(.*)\]$/);
								if(m != null) {
									/* Find the correct game */
									for(game=0; game<games; game++) {
										if(m[1]==gamedata[game].away && m[2]==gamedata[game].home)
											break;
									}
									if(game==games)
										continue;

									if(m[1]==m[3] || m[2]==m[3]) {
										leaguefile.iniSetValue(set, "Game"+(game+1)+"Winner", m[3]);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return(curr_msg-1);
}
