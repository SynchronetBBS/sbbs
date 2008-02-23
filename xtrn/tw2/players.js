var PlayerProperties=[
			{
				 prop:"UserNumber"
				,name:"User Number"
				,type:"Integer"
				,def:(user == undefined?0:user.number)
			}
			,{
				 prop:"Points"
				,name:"Pointes"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Alias"
				,name:"Alias"
				,type:"String:42"
				,def:(user == undefined?"":user.alias)
			}
			,{
				 prop:"LastOnDay"
				,name:"Date Last On"
				,type:"Date"
				,def:system.datestr()
			}
			,{
				 prop:"KilledBy"
				,name:"Killed By"
				,type:"SignedInteger"
				,def:0
			}
			,{
				 prop:"TurnsLeft"
				,name:"Turns Remaining"
				,type:"Integer"
				,def:Settings.TurnsPerDay
			}
			,{
				 prop:"Sector"
				,name:"Sector"
				,type:"Integer"
				,def:1
			}
			,{
				 prop:"Fighters"
				,name:"Fighters"
				,type:"Integer"
				,def:Settings.StartingFighters
			}
			,{
				 prop:"Holds"
				,name:"Holds"
				,type:"Integer"
				,def:Settings.StartingHolds
			}
			,{
				 prop:"Commodities"
				,name:"Commodities"
				,type:"Array:3:Integer"
				,def:[0,0,0]
			}
			,{
				 prop:"Credits"
				,name:"Credits"
				,type:"Integer"
				,def:Settings.StartingCredits
			}
			,{
				 prop:"TeamNumber"
				,name:"Team Number"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"LastIn"
				,name:"Sector Last In"
				,type:"Integer"
				,def:1
			}
			,{
				 prop:"Online"
				,name:"Online"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"Ported"
				,name:"Ported"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"Landed"
				,name:"Landed"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"TimeToday"
				,name:"Time used today (secs)"
				,type:"Integer"
				,def:0
			}
		];

var PlayerLocation = [
			{
				 prop:"Sector"
				,name:"Sector"
				,type:"Integer"
				,def:1
			}
		];

var players=new RecordFile(fname("players.dat"), PlayerProperties);
var playerLocation=new RecordFile(fname("player-loc.dat"), PlayerLocation);

function AttackPlayer()
{
	var count=0;
	var i;

	if(player.Fighters < 1) {
		console.writeln("You don't have any fighters.");
		return(false);
	}
	for(i=1;i<players.length; i++) {
		var otherloc=playerLocation.Get(i);
		if(otherloc.Sector==player.Sector) {
			var otherplayer=players.Get(i);
			if(otherplayer.Sector==player.Sector
					&& otherplayer.Record!=player.Record
					&& otherplayer.KilledBy!=0
					&& otherplayer.UserNumber!=0
					&& !otherplayer.Online) {
				count++;
				console.write("Attack "+otherplayer.Alias+" (Y/N)[Y]? ");
				if(InputFunc(['Y','N'])!='N') {
					console.writeln("<Yes>");
					break;
				}
			}
		}
		otherplayer=null;
	}
	if(otherplayer==null) {
		if(count)
			console.writeln("There are no other ships in here");
		else
			console.writeln("There are no ships here to attack.");
		return(false);
	}

	DoBattle(otherplayer, otherplayer.TeamNumber > 0);

	if(otherplayer.Fighters > 0)
		return(true);

	KilledBy(otherplayer, player, true);
	/* 15600 player salvages ship from otherplayer */
	var salvaged=parseInt(otherplayer.Holds/4)+1;
	if(player.Holds + salvaged > 50)
		salvaged=50-player.Holds;
	if(salvaged < 1) {
		console.writeln("You destroyed the ship, but you can't salvage anything from it");
		return(true);
	}

	var j;

	console.writeln("You destroyed the ship and salvaged these cargo holds:");
	var holds=new Array(Commodities.length+1);
	for(i=0; i<holds.length+1; i++)
		holds[i]=0;
	for(i=0; i<otherplayer.Holds; i++) {
		var limit=0;
		var r=random(otherplayer.Holds)+1;
		for(j=0; j<Commodities.length; j++) {
			limit += otherplayer.Commodities[j];
			if(r<limit) {
				otherplayer.Commodities[j]--;
				holds[j]++;
				r=0;
				break;
			}
		}
		if(r==0)
			holds[Commodities.length]++;
	}
	if(holds[Commodities.length]>0) {
		console.writeln("  "+holds[Commodities.length]+" empty.");
		player.Holds+=holds[Commodities.length];
	}
	for(j=0; j<Commodities.length; j++) {
		if(holds[j]>0) {
			console.writeln("  "+holds[j]+" with "+Commodities[j].name.toLowerCase()+".");
			player.Holds+=holds[j];
			player.Commodities[j]+=holds[j];
		}
	}
}

function PlayerMove()
{
	if(player.TurnsLeft < 1) {
		console.writeln("I'm sorry but you don't have any turns left.");
		return(false);
	}
	console.attributes="HW";
	console.write("Warps lead to: ");

	var sector=sectors.Get(player.Sector);
	var i;
	var avail=new Array();
	for(i=0; i<sector.Warps.length; i++) {
		if(sector.Warps[i]>0)
			avail.push(sector.Warps[i].toString());
	}
	console.writeln(avail.join(", "));
	console.write("To which sector? ");
	var to=InputFunc(sectors.length-1);
	if(to=='')
		return(false);
	return(MoveTo(parseInt(to)));
}

function PlayerGamble()
{
	console.attributes="HW";
	console.writeln("You have "+player.Credits+" credits.");
	console.writeln("How much do you want to gamble at double or nothing (50-50 odds)");
	console.write("[0]? ");
	var gamble=console.getnum(player.Credits > 99999?99999:player.Credits);
	if(gamble > 0 && gamble <=player.Credits) {
		console.write("Flipping the coin...");
		mswait(250);
		if(random(2)==1) {
			console.crlf();
			player.Credits-=gamble;
			console.writeln("Sorry, you lost.");
		}
		else {
			console.crlf();
			player.Credits+=gamble;
			console.writeln("You won!");
		}
		player.Put();
		console.writeln("You now have "+player.Credits+" credits.");
	}
}

function PlayerInfo(num)
{
	var p=players.Get(num);

	console.attributes="HW";
	console.write("   Pilot's Name: "+p.Alias);
	if(p.PlayerNumber>0)
		console.write("  Team ["+p.TeamNumber+"]");
	console.crlf();
	console.writeln("       Fighters: "+p.Fighters);
	console.attributes="HG";
	console.writeln("Sector Location: "+p.Sector);
	console.writeln("     Turns left: "+p.TurnsLeft);
	console.writeln("    Cargo Holds: "+p.Holds);
	console.attributes="HR";
	var i;
	for(i=0; i<Commodities.length; i++)
		console.writeln("     # with "+Commodities[i].abbr+": "+p.Commodities[i]+" ");
	console.attributes="HM";
	console.writeln("        Credits: "+p.Credits);
	console.writeln("    Door points: "+p.Points);
}

function KilledBy(killed, killer, notify)	/* 15300 */
{
	var i;

	killed.KilledBy=killer.Record;
	killed.Put();
	var killedLocation=playerLocation.Get(killed.Record);
	killedLocation.Sector=killed.Sector; killedLocation.Put();

	/* Destroy all deployed fighters */
	for(i=1; i<sectors.length; i++) {
		var sector=sectors.Get(i);
		if(sector.FighterOwner==player.Record) {
			sector.Fighters=0;
			sector.FighterOwner=0;
			sector.Put();
		}
	}

	if(killed.TeamNumber > 0) {
		var team=teams.Get(killed.TeamNumber);
		var i;
		for(i=0; i<team.Members.length; i++) {
			if(team.Members[i]==killed.Record) {
				team.Members[i]=0;
				team.Size--;
			}
		}
	}

	if(notify)
		twmsg.writeln(" - "+killer.Alias+"  killed "+killed.Alias);
}

function RankPlayers()
{
	var i;
	var rank=new Array();
	var fighters=new Array();

	for(i=1; i<sectors.length; i++) {
		var sector=sectors.Get(i);
		if(sector.Fighters > 0 && sector.FighterOwner > 0) {
			if(fighters[sector.FighterOwner]==undefined)
				fighters[sector.FighterOwner]=0;
			fighters[sector.FighterOwner]+=sector.Fighters;
		}
	}
	for(i=1; i<players.length; i++) {
		var p=players.Get(i);
		if(p.UserNumber==0)
			continue;
		if(p.KilledBy!=0)
			continue;
		var robj=new Object();
		robj.Record=p.Record;
		robj.Score=p.Fighters*100 + p.Holds*500 + p.Credits;
		for(i=0; i<Commodities.length; i++)
			robj.Score += Commodities[i].price*p.Commodities[i];
		if(fighters[i]!=undefined)
			robj.Score += fighters[i]*100;
		rank.push(robj);
	}

	function sortfunc(a,b) {
		return(a.Score-b.Score);
	}

	rank.sort(sortfunc);
	return(rank);
}

function TWRank()
{
	var rf=new File(fname(Settings.TopTenFile));
	rf.open("w");
	rf.writeln();
	rf.writeln("  T R A D E W A R S   I I - 500T   S C O R E B O A R D  ");
	rf.writeln();
	rf.writeln("Last updated at: "+system.timestr());
	rf.writeln();
	rf.writeln("Player Rankings");
	rf.writeln("Rank     Value      Team   Player");
	rf.writeln("==== ============= ====== ================");
	var ranked=RankPlayers();
	var i;
	var trank=new Object();

	for(i=0; i<ranked.length; i++) {
		var p=players.Get(ranked[i].Record);
		if(i<10)
			rf.writeln(format("%4d %13d %6s %s",(i+1),ranked[i].Score,p.TeamNumber==0?"":p.TeamNumber.toString(),p.Alias));
		if(p.TeamNumber.toString() != 0) {
			if(trank[p.TeamNumber.toString()]==undefined)
				trank[p.TeamNumber.toString()]=0;
			trank[p.TeamNumber.toString()]+=ranked[i].Score;
		}
	}
	var tsort=new Array();
	for(tr in trank) {
		var ts=new Object();
		ts.Record=tr;
		ts.Score=trank[tr];
		tsort.push(tr);
	}
	if(tsort.length > 0) {
		function sortfunc(a,b) {
			return(a.Score-b.Score);
		}

		tsort.sort(sortfunc);
		rf.writeln();
		rf.writeln("Team Rankings");
		rf.writeln("Rank     Value      Team");
		rf.writeln("==== ============= ======");
		for(i=0; i<tsort.length; i++) {
			if(i>=10)
				break;
			rf.writeln(format("%4d %13d %6d",(i+1),tsort[i].Score,tsort[i].Record));
		}
	}
	rf.close();
}

function DoBattle(opp, otherteam)
{
	if(player.Fighters<1) {
		console.writeln("You don't have any fighters!");
		return(0);
	}
	else {
		console.write("How many fighters do you wish to use? ");
		var use=console.getnum(player.Fighters);

		if(use > 0 && use <= player.Fighters) {
			var lost=0;
			var killed=0;

			player.Fighters -= use;
			while(use && opp.Fighters) {
				if(player.TeamNumber > 0 && otherteam) {
					if((random(10)+1) > 6) {
						opp.Fighters--;
						killed++;
					}
					else {
						use--;
						lost++;
					}
				}
				else if(otherteam && player.TeamNumber==0) {
					if((random(10)+1) > 6) {
						use--;
						lost++;
					}
					else {
						opp.Fighters--;
						killed++;
					}
				}
				else {
					if(random(2)==0) {
						use--;
						lost++;
					}
					else {
						opp.Fighters--;
						killed++;
					}
				}
			}
			player.Fighters += use;
			opp.Put();
			player.Put();

			console.writeln("You lost "+lost+" fighter(s), "+player.Fighters+" remain.");
			if(opp.Fighters > 0)
				console.write("You destroyed "+killed+" enemy fighters, "+opp.Fighters+" remain.");
			else
				console.write("You destroyed all of the enemy fighters.");
			return(killed);
		}
	}
	return(0);
}

function MatchPlayer(name)
{
	var i;

	name=name.toUpperCase();
	for(i=1; i<players.length; i++) {
		var p=players.Get(i);
		if(p.UserNumber==0)
			continue;
		if(p.KilledBy!=0)
			continue;
		if(p.Alias.toUpperCase().indexOf(name)!=-1) {
			console.write(p.Alias+" (Y/N)[Y]? ");
			if(InputFunc(['Y','N'])!='N')
				return(p);
		}
	}
	console.writeln("Not found.");
	return(null);
}

function DeletePlayer(player)
{
	/* Delete player */
	player.ReInit();
	player.UserNumber=0;
	player.Alias="<Deleted>";
	player.Sector=0;
	player.Put();
	/* Move to sector zero */
	var loc=playerLocation.Get(player.Record);
	loc.Sector=0;
	loc.Put();
	/* Set fighter owner to "Deleted Player" */
	var i;
	for(i=1; i<sectors.length; i++) {
		var sector=sectors.Get(i);
		if(sector.FighterOwner==player.Record) {
			sector.FighterOwner=-98;
			sector.Put();
		}
	}
	/* Set messages TO the deleted player as read and FROM as from deleted */
	for(i=0; i<twpmsg.length; i++) {
		var msg=twpmsg.Get(i);

		if(msg.To==player.Record && !msg.Read) {
			msg.Read=true;
			msg.Put();
		}
		else if(msg.From==player.Record) {
			msg.From=-98;
			msg.Put();
		}
	}
	/* Set radio messages TO the deleted player as read and FROM as from deleted */
	for(i=0; i<twrmsg.length; i++) {
		var msg=twrmsg.Get(i);

		if(msg.To==player.Record && !msg.Read) {
			msg.Read=true;
			msg.Put();
		}
		else if(msg.From==player.Record) {
			msg.From=-98;
			msg.Put();
		}
	}
	/* Set killed bys to Deleted Player */
	for(i=1; i<players.length; i++) {
		var otherplayer=players.Get(i);

		if(otherplayer.KilledBy==player.Record) {
			otherplayer.KilledBy=-98;
			otherplayer.Put();
		}
	}
	twmsg.writeln("  - "+player.Alias+" deleted from game");
}

function MoveTo(to)
{
	var sector=sectors.Get(player.Sector);

	if(to > 0) {
		for(i=0; i<sector.Warps.length; i++) {
			if(sector.Warps[i]==to) {
				player.TurnsLeft--;
				player.LastIn=player.Sector;
				player.Sector=to;
				player.Put();
				location.Sector=player.Sector; location.Put();
				if(player.TurnsLeft==10 || player.TurnsLeft < 6) {
					console.writeln("You have " + player.TurnsLeft + " turns left.");
				}
				return(true);
			}
		}
		console.writeln("You can't get there from here.");
	}
	return(false);
}

function LoadPlayer()
{
	var firstnew;
	for(i=1; i<players.length; i++) {
		player=players.Get(i);
		if(player.UserNumber == user.number && (!file_exists(system.data_dir+format("user/%04d.tw2",player.UserNumber))))
			DeletePlayer(player);
		if(player.UserNumber==0 && firstnew==undefined)
			firstnew=player;
		if(player.UserNumber == user.number)
			break;
	}
	if(player==undefined || player.UserNumber!=user.number) {
		console.attributes="G";
		console.writeln("I can't find your record, so I am assuming you are a new player.");
		console.attributes="M";
		console.writeln("Entering a new player...");
		player=firstnew;
		if(player==undefined) {
			console.writeln("I'm sorry but the game is full.");
			console.writeln("Please leave a message for the Sysop so");
			console.writeln("he can save a space for you when one opens up.");
			twmsg.writeln(system.timestr()+": New player not allowed - game full.");
			return(false);
		}
		console.crlf();
		console.writeln("Notice: If you don't play this game for "+Settings.DaysInactivity+" days, you will");
		console.writeln("be deleted to make room for someone else.");
		console.crlf();
		console.writeln("Your ship is being initialized.");

		/* Lock the player file and mark us online... */
		if(!Lock(players.file.name, bbs.node_num, true, 10)) {
			console.writeln("!!!Unable to lock player data!");
			twmsg.writeln("!!!Unable to lock player data!");
			return(false);
		}
		player.ReInit();
		player.Online=true;
		player.Put();
		Unlock(players.file.name);

		twmsg.writeln(system.timestr()+" "+user.alias+": New Player logged on");
		Instructions();
	}
	else {
		/* Lock the player file and mark us online... */
		if(!Lock(players.file.name, bbs.node_num, true, 10)) {
			console.writeln("!!!Unable to lock player data!");
			twmsg.writeln("!!!Unable to lock player data!");
			return(false);
		}
		player.Online=true;
		player.Put();
		Unlock(players.file.name);

		console.crlf();		/* TODO: BASIC magic... N$ appears empty tw2.bas: 242 */
		twmsg.writeln(system.timestr()+" "+user.alias+": Logged on");
		if(today < system.datestr(player.LastOnDay)) {
			console.writeln("I'm sorry, but you won't be allowed on for another " + parseInt((system.datestr(player.LastOnDay)-today)/86400) + " day(s)");
			return(false);
		}
		if(today==system.datestr(player.LastOnDay)) {
			if(player.KilledBy != -99) {
				console.writeln("You have been on today.");
				if(player.TurnsLeft<1) {
					console.writeln("You don't have any turns left today. You will be allowed to play tomorrow.");
					return(false);
				}
				if(player.KilledBy==player.Record) {
					console.writeln("You killed yourself today. You will be allowed to play tomorrow.");
					return(false);
				}
				ReadPMsg();
			}
		}
		else {
			player.TurnsLeft=Settings.TurnsPerDay;
			player.TimeToday=0;
		}
		player.LastOnDay=system.datestr();
		if(player.KilledBy != 0) {
			switch(player.KilledBy) {
				case player.Record:
					console.attributes="R";
					console.writeln("You managed to kill yourself on your last time on.");
					break;
				case -1:
					console.attributes="R";
					console.writeln("You have been killed by the Cabal!");
					break;
				case -98:
					console.writeln("You have been killed by a deleted player.");
					break;
				default:
					var otherplayer=players.Get(player.KilledBy);

					console.writeln(otherplayer.Alias+" destroyed your ship!");
					break;
			}
			player.ReInit();
		}
	}
	file_touch(system.data_dir+format("user/%04d.tw2",player.UserNumber))

	ReadRadio();

	if(player.Sector < 1 || player.Sector >= sectors.length) {
		console.writeln("You are being moved to sector 1");
		player.Sector=1;
		player.Put();
	}
	location=playerLocation.Get(player.Record);
	location.Sector=player.Sector;
	location.Put();

	if(player.Credits > 25000) {
		console.crlf();
		console.writeln("Tax time! You are being taxed 5000 credits to help support the resistance");
		console.writeln("against the evil Cabal.");
		player.Credits -= 5000;
		player.Put();
	}
	return(true);
}

function Dropfighters()
{
	if(player.Sector < 8) {
		console.writeln("You can't leave fighters in the Union (sectors 1-7)");
		return(false);
	}
	var sector=sectors.Get(player.Sector);
	if(sector.Fighters > 0 && sector.FighterOwner != player.Record) {
		console.writeln("There are already fighters in this sector!");
		return(false);
	}
	console.writeln("You have " + (player.Fighters+sector.Fighters) + " fighters available.");
	console.write("How many fighters do you want defending this sector? ");
	var newf=player.Fighters+sector.Fighters;
	if(newf > 9999)
		newf=9999;
	var newf=console.getnum(newf);
	if(newf >= 0 && newf <=player.Fighters+sector.Fighters) {
		if((player.Fighters+sector.Fighters)-newf > 9999) {
			console.writeln("Too many ships in your fleet!  You are limited to 9999");
			return(false);
		}
		player.Fighters=(player.Fighters+sector.Fighters)-newf;
		sector.Fighters=newf;
		if(sector.Fighters > 0)
			sector.FighterOwner=player.Record;
		sector.Put();
		player.Put();
		console.writeln("Done.  You have " + player.Fighters + " fighter(s) in your fleet.");
		return(true);
	}
	return(false);
}

function ResetAllPlayers()
{
	uifc.pop("Creating Players");
	var player=players.New();
	var loc=playerLocation.New();
	var i;
	player.UserNumber=0;
	player.Put();
	loc.Put();
	for(i=0; i<Settings.MaxPlayers; i++) {
		player=players.New();
		player.UserNumber=0;
		player.Sector=0;
		player.Put();
		loc=playerLocation.New();
		loc.Sector=0;
		loc.Put();
	}
}
