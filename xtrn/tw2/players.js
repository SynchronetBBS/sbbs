var PlayerProperties=[
			{
				 prop:"UserNumber"
				,name:"User Number"
				,type:"Integer"
				,def:(user == undefined?0:user.number)
			}
			,{
				 prop:"QWKID"
				,name:"BBS QWK ID"
				,type:"String:8"
				,def:system.qwk_id
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
				,def:strftime("%Y:%m:%d")
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

var players = {
	GetLocked:function GetLocked(playerNum, lock) {
		if(playerNum==undefined)
			return undefined;
		return new Player(db.read(Settings.DB,'players.'+playerNum, lock), playerNum);
	},
	Get:function GetLocked(playerNum) {
		return this.GetLocked(playerNum, LOCK_READ);
	},
	get length() {
		return(db.read(Settings.DB,'players.length',LOCK_READ));
	}
}

function Player(player,playerNum)
{
	if(playerNum==undefined)
		return undefined;
	this.Record=playerNum;
	this.PutLocked=function(lock) {
		var p={};
		var i;

		for(i in PlayerProperties) {
			p[PlayerProperties[i]['prop']]=this[PlayerProperties[i]['prop']];
		}
		db.write(Settings.DB,'players.'+this.Record,p,lock);
	}
	this.Put=function () {
		this.PutLocked(LOCK_WRITE);
	}
	this.ReInit=function() {
		for(i in PlayerProperties) {
			this[PlayerProperties[i]['prop']]=PlayerProperties[i]['def'];
		}
	}
	for(i in PlayerProperties) {
		this[PlayerProperties[i]['prop']]=player[PlayerProperties[i]['prop']];
	}
}

function AttackPlayer()
{
	var count=0;
	var i;

	if(player.Fighters < 1) {
		console.writeln("You don't have any fighters.");
		return(false);
	}
	var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	for(i=0;i<sector.Ships.length; i++) {
		var otherplayer=players.Get(sector.Ships[i]);
		if(otherplayer.Sector==player.Sector
				&& otherplayer.Record!=player.Record
				&& otherplayer.KilledBy==0
				&& otherplayer.UserNumber!=0
				&& !otherplayer.Online) {
			count++;
			console.write("Attack "+otherplayer.Alias+" (Y/N)[Y]? ");
			if(InputFunc(['Y','N'])!='N') {
				console.writeln("<Yes>");
				break;
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
	for(i=0; i<holds.length; i++)
		holds[i]=0;
	for(i=0; i<salvaged; i++) {
		var limit=0;
		var r=random(otherplayer.Holds)+1;
		for(j=0; j<Commodities.length; j++) {
			limit += otherplayer.Commodities[j];
			if(r<limit) {
				otherplayer.Commodities[j]--;
				holds[j]++;
				r=-1;
				break;
			}
		}
		if(r!=-1)
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

	var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	var i;
	var avail=new Array();
	for(i=0; i<sector.Warps.length; i++) {
		if(sector.Warps[i]>0)
			avail.push(sector.Warps[i].toString());
	}
	console.writeln(avail.join(", "));
	console.write("To which sector? ");
	var to=InputFunc(avail);
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
	var gamble=InputFunc([{min:0,max:(player.Credits > 99999?99999:player.Credits)}]);
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
	if(p.TeamNumber>0)
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
	var i,j;

	killed.KilledBy=killer.Record;
	killed.Put();
	db.lock(Settings.DB,'sectors',LOCK_WRITE);
	var sectors=db.read(Settings.DB,'sectors');
	/* Destroy all deployed fighters */
	for(i=1; i<sectors.length; i++) {
		if(i==killed.Sector) {
			for(j=0; j<sectors[i].Ships.length; j++) {
				if(sectors[i].Ships[j]==killed.Record) {
					sectors[i].Ships.splice(j,1);
					j--;
				}
			}
		}
		if(sectors[i].FighterOwner==killed.Record) {
			sectors[i].Fighters=0;
			sectors[i].FighterOwner=0;
		}
	}
	db.write(Settings.DB,'sectors',sectors);
	db.unlock(Settings.DB,'sectors');

	if(killed.TeamNumber > 0) {
		var ktn=killed.TeamNumber;
		db.lock(Settings.DB,'teams.'+ktn,LOCK_WRITE);
		var team=db.read(Settings.DB,'teams.'+ktn);
		var i;
		for(i=0; i<team.Members.length; i++) {
			if(team.Members[i]==killed.Record) {
				team.Members.splice(i,1);
				i--;
			}
		}
		killed.TeamNumber=0;
		killed.Put();
		db.write(Settings.DB,'teams.'+ktn);
		db.unlock(Settings.DB,'teams.'+ktn);
	}

	if(notify)
		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:" - "+killer.Alias+"  killed "+killed.Alias},LOCK_WRITE);
}

function RankPlayers()
{
	var i;
	var rank=new Array();
	var fighters=new Array();
	var universe=db.read(Settings.DB,'sectors',LOCK_READ);
	var allplayers=db.read(Settings.DB,'players',LOCK_READ);

	for(i=1; i<universe.length; i++) {
		if(universe[i].Fighters > 0 && universe[i].FighterOwner > 0) {
			if(fighters[universe[i].FighterOwner]==undefined)
				fighters[universe[i].FighterOwner]=0;
			fighters[universe[i].FighterOwner]+=universe[i].Fighters;
		}
	}
	for(i=1; i<allplayers.length; i++) {
		if(allplayers[i].UserNumber==0)
			continue;
		if(allplayers[i].KilledBy!=0)
			continue;
		var robj=new Object();
		robj.Record=i;
		robj.Score=allplayers[i].Fighters*100 + allplayers[i].Holds*500 + allplayers[i].Credits;

		var j;
		for(j=0; j<Commodities.length; j++)
			robj.Score += Commodities[j].price*allplayers[i].Commodities[j];
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
	var rstr='';
	rstr += '\r\n';
	rstr += "  T R A D E W A R S   I I - 500T   S C O R E B O A R D  \r\n";
	rstr += '\r\n';
	rstr += "Last updated at: "+system.timestr()+"\r\n";
	rstr += '\r\n';
	rstr += "Player Rankings\r\n";
	rstr += "Rank     Value      Team   Player\r\n";
	rstr += "==== ============= ====== ================\r\n";
	var ranked=RankPlayers();
	var i;
	var trank=new Object();

	for(i=0; i<ranked.length; i++) {
		if(ranked[i].Record == undefined)
			continue;
		var p=players.Get(ranked[i].Record);
		if(i<10)
			rstr += format("%4d %13d %6s %s\r\n",(i+1),ranked[i].Score,p.TeamNumber==0?"":p.TeamNumber.toString(),p.Alias);
		if(p.TeamNumber.toString() != 0) {
			if(trank[p.TeamNumber.toString()]==undefined)
				trank[p.TeamNumber.toString()]=0;
			trank[p.TeamNumber.toString()]+=ranked[i].Score;
		}
	}
	var tsort=new Array();
	var tr;
	for(tr in trank) {
		var ts=new Object();
		ts.Record=tr;
		ts.Score=trank[tr];
		tsort.push(tr);
	}
	function sortfunc(a,b) {
		return(a.Score-b.Score);
	}
	if(tsort.length > 0) {

		tsort.sort(sortfunc);
		rstr += "\r\n";
		rstr += "Team Rankings\r\n";
		rstr += "Rank     Value      Team\r\n";
		rstr += "==== ============= ======\r\n";
		for(i=0; i<tsort.length; i++) {
			if(i>=10)
				break;
			rstr += format("%4d %13d %6d\r\n",(i+1),tsort[i].Score,tsort[i].Record);
		}
	}
	db.write(Settings.DB,'ranking',rstr,LOCK_WRITE);
}

function JSON_DoBattle(oppPath, otherteam)
{
	if(player.Fighters<1) {
		console.writeln("You don't have any fighters!");
		return(0);
	}
	else {
		console.write("How many fighters do you wish to use? ");
		var use=InputFunc([{min:0,max:player.Fighters}]);

		if(use > 0 && use <= player.Fighters) {
			var lost=0;
			var killed=0;

			db.lock(Settings.DB,oppPath,LOCK_WRITE);
			var opp=db.read(Settings.DB,oppPath);
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
			db.write(Settings.DB,oppPath,opp);
			db.unlock(Settings.DB,oppPath);
			player.Put();

			console.writeln("You lost "+lost+" fighter(s), "+player.Fighters+" remain.");
			if(opp.Fighters > 0)
				console.writeln("You destroyed "+killed+" enemy fighters, "+opp.Fighters+" remain.");
			else
				console.writeln("You destroyed all of the enemy fighters.");
			return(killed);
		}
	}
	return(0);
}

// TODO: Obsolete
function DoBattle(opp, otherteam)
{
	if(player.Fighters<1) {
		console.writeln("You don't have any fighters!");
		return(0);
	}
	else {
		console.write("How many fighters do you wish to use? ");
		var use=InputFunc([{min:0,max:player.Fighters}]);

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
				console.writeln("You destroyed "+killed+" enemy fighters, "+opp.Fighters+" remain.");
			else
				console.writeln("You destroyed all of the enemy fighters.");
			return(killed);
		}
	}
	return(0);
}

function MatchPlayer(name)
{
	var i;
	var allplayers=db.read(Settings.DB,'players',LOCK_READ);

	name=name.toUpperCase();
	for(i=1; i<allplayers.length; i++) {
		if(allplayers[i].UserNumber==0)
			continue;
		if(allplayers[i].KilledBy!=0)
			continue;
		if(allplayers[i].Alias.toUpperCase().indexOf(name)!=-1) {
			console.write(allplayers[i].Alias+" (Y/N)[Y]? ");
			if(InputFunc(['Y','N'])!='N')
				return(players.Get(i));
		}
	}
	console.writeln("Not found.");
	return(null);
}

function DeletePlayer(player)
{
	var msg;
	var i;
	var sector;

	/* Delete player */
	player.ReInit();
	player.UserNumber=0;
	player.Alias="<Deleted>";
	db.lock(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
	sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
	for(i=0; i<sector.Ships.length; i++) {
		if(sector.Ships[i]==player.Record) {
			sector.Ships.splice(i,1);
			i--;
		}
	}
	db.write(Settings.DB,'sectors.'+player.Sector,sector);
	player.Sector=0;
	player.Put();
	db.unlock(Settings.DB,'sectors.'+player.Sector);
	/* Set fighter owner to "Deleted Player" */
	var i;
	db.lock(Settings.DB,'sectors',LOCK_WRITE);
	var sectors=db.read(Settings.DB,'sectors');
	for(i=1; i<sectors.length; i++) {
		if(sectors[i].FighterOwner==player.Record) {
			sectors[i].FighterOwner=-98;
		}
	}
	db.unlock(Settings.DB,'sectors');
	/* Set messages TO the deleted player as read and FROM as from deleted */
	db.lock(Settings.DB,'updates',LOCK_WRITE);
	var updates=db.read(Settings.DB,'updates');
	for(i=0; i<updates.length; i++) {
		if(updates[i].To==player.Record && !msg.Read) {
			updates.splice(i,1);
			i--;
		}
		else if(updates[i].From==player.Record) {
			updates[i].From=-98;
		}
	}
	db.write(Settings.DB,'updates',updates);
	db.unlock(Settings.DB,'updates');
	/* Set radio messages TO the deleted player as read and FROM as from deleted */
	db.lock(Settings.DB,'radio',LOCK_WRITE);
	var radio=db.read(Settings.DB,'radio');
	for(i=0; i<radio.length; i++) {
		if(radio[i].To==player.Record && !msg.Read) {
			radio.splice(i,1);
			i--;
		}
		else if(radio[i].From==player.Record) {
			radio[i].From=-98;
		}
	}
	db.write(Settings.DB,'radio',radio);
	db.unlock(Settings.DB,'radio');
	/* Set killed bys to Deleted Player */
	db.lock(Settings.DB,'players',LOCK_WRITE);
	var allplayers=db.read(Settings.DB,'players');
	for(i=1; i<allplayers.length; i++) {
		if(allplayers[i].KilledBy==player.Record) {
			allplayers[i].KilledBy=-98;
		}
	}
	db.write(Settings.DB,'players');
	db.unlock(Settings.DB,'players');
	db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"  - "+player.Alias+" deleted from game"},LOCK_WRITE);
}

function MoveTo(to)
{
	var sector,from=player.Sector;

	if(player.TurnsLeft < 1) {
		console.writeln("I'm sorry but you don't have any turns left.");
		return(false);
	}
	if(to > 0) {
		db.lock(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
		db.lock(Settings.DB,'sectors.'+to,LOCK_WRITE);
		sector=db.readmulti([
			[Settings.DB,'sectors.'+player.Sector,undefined,'from'],
			[Settings.DB,'sectors.'+to,undefined,'to']
		]);
		for(i=0; i<sector.from.Warps.length; i++) {
			if(sector.from.Warps[i]==to) {
				sector.to.Ships.push(player.Record);
				for(i=0; i<sector.from.Ships.length; i++) {
					if(sector.from.Ships[i]==player.Record) {
						sector.from.Ships.splice(i,1);
						i--;
					}
				}
				db.write(Settings.DB,'sectors.'+player.Sector,sector.from);
				db.write(Settings.DB,'sectors.'+to,sector.to);
				player.TurnsLeft--;
				player.LastIn=player.Sector;
				player.Sector=to;
				player.Put();
				db.unlock(Settings.DB,'sectors.'+player.Sector);
				db.unlock(Settings.DB,'sectors.'+player.LastIn);
				return(true);
			}
		}
		db.unlock(Settings.DB,'sectors.'+player.Sector);
		db.unlock(Settings.DB,'sectors.'+to);
		console.writeln("You can't get there from here.");
	}
	return(false);
}

function LoadPlayer()
{
	var firstnew;
	var allplayers;
	var playerNum=0;

	for(var done=false; !done;) {
		db.lock(Settings.DB,'players',LOCK_WRITE);
		allplayers=db.read(Settings.DB,'players');
		for(i=1; i<allplayers.length; i++) {
			player=allplayers[i];
			if(player.QWKID==system.qwk_id && player.UserNumber == user.number && (!file_exists(system.data_dir+format("user/%04d.tw2",player.UserNumber)))) {
				db.unlock(Settings.DB,'players');
				DeletePlayer(player);
				break;
			}
			if(player.UserNumber==0 && firstnew==undefined)
				firstnew=i;
			if(player.QWKID==system.qwk_id && player.UserNumber == user.number) {
				done=true;
				playerNum=i;
				break;
			}
		}
		done=true;
	}
	if(player==undefined || player.UserNumber!=user.number) {
		player=players.GetLocked(firstnew);
		if(player != undefined) {
			player.UserNumber=-1;
			player.Online=true;
			player.PutLocked();
		}
		db.unlock(Settings.DB,'players');
		db.lock(Settings.DB,'players.'+player.Record,LOCK_WRITE);
		console.attributes="G";
		console.writeln("I can't find your record, so I am assuming you are a new player.");
		console.attributes="M";
		console.writeln("Entering a new player...");
		if(player==undefined) {
			console.writeln("I'm sorry but the game is full.");
			console.writeln("Please leave a message for the Sysop so");
			console.writeln("he can save a space for you when one opens up.");
			db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:": New player not allowed - game full."},LOCK_WRITE);
			db.unlock(Settings.DB,'players.'+player.Record);
			return(false);
		}
		console.crlf();
		console.writeln("Notice: If you don't play this game for "+Settings.DaysInactivity+" days, you will");
		console.writeln("be deleted to make room for someone else.");
		console.crlf();
		console.writeln("Your ship is being initialized.");
		player.ReInit();
		player.Online=true;
		player.PutLocked();
		db.unlock(Settings.DB,'players.'+player.Record);

		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:user.alias+": New Player logged on"},LOCK_WRITE);
		Instructions();
	}
	else {
		player=players.GetLocked(playerNum);
		player.Online=true;
		player.PutLocked();
		db.unlock(Settings.DB,'players');

		console.crlf();		/* TODO: BASIC magic... N$ appears empty tw2.bas: 242 */
		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:user.alias+": Logged on"},LOCK_WRITE);
		if(strftime("%Y:%m:%d") < player.LastOnDay) {
			console.writeln("I'm sorry, but you won't be allowed on until " + player.LastOnDay);
			return(false);
		}
		if(strftime("%Y:%m:%d")==player.LastOnDay) {
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
		player.LastOnDay=strftime("%Y:%m:%d");
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
	file_touch(system.data_dir+format("user/%04d.tw2",player.UserNumber));

	ReadRadio();

	if(player.Sector < 1 || player.Sector >= db.read(Settings.DB,'sectors.length',LOCK_READ)) {
		console.writeln("You are being moved to sector 1");
		player.Sector=1;
		db.lock(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
		sector=db.read(Settings.DB,'sectors.'+player.Sector);
		sector.Ships.push(player.Record);
		db.write(Settings.DB,'sectors.'+player.Sector,sector);
		player.Put();
		db.unlock(Settings.DB,'sectors.'+player.Sector);
		db.unlock(Settings.DB,'sectors.'+player.LastIn);
		player.Put();
	}

	if(player.Credits > 25000) {
		console.crlf();
		console.writeln("Tax time! You are being taxed 5000 credits to help support the resistance");
		console.writeln("against the evil Cabal.");
		player.Credits -= 5000;
		player.Put();
	}
	return(true);
}

function DropFighters()
{
	if(player.Sector < 8) {
		console.writeln("You can't leave fighters in the Union (sectors 1-7)");
		return(false);
	}
	var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	if(sector.Fighters > 0 && sector.FighterOwner != player.Record) {
		console.writeln("There are already fighters in this sector!");
		return(false);
	}
	console.writeln("You have " + (player.Fighters+sector.Fighters) + " fighters available.");
	console.write("How many fighters do you want defending this sector? ");

	var newf=player.Fighters+sector.Fighters;
	if(newf > 9999)
		newf=9999;
	newf=InputFunc([{min:0,max:newf}]);
	db.lock(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
	sector=db.read(Settings.DB,'sectors.'+player.Sector);
	if(sector.Fighters > 0 && sector.FighterOwner != player.Record) {
		console.writeln("There are already fighters in this sector!");
		db.unlock(Settings.DB,'sectors.'+player.Sector);
		return(false);
	}
	if(newf >= 0 && newf <=player.Fighters+sector.Fighters) {
		if((player.Fighters+sector.Fighters)-newf > 9999) {
			console.writeln("Too many ships in your fleet!  You are limited to 9999");
			db.unlock(Settings.DB,'sectors.'+player.Sector);
			return(false);
		}
		player.Fighters=(player.Fighters+sector.Fighters)-newf;
		sector.Fighters=newf;
		if(sector.Fighters > 0)
			sector.FighterOwner=player.Record;
		db.write(Settings.DB,'sectors.'+player.Sector,sector);
		db.unlock(Settings.DB,'sectors.'+player.Sector);
		player.Put();
		console.writeln("Done.  You have " + player.Fighters + " fighter(s) in your fleet.");
		return(true);
	}
	db.unlock(Settings.DB,'sectors.'+player.Sector);
	return(false);
}

function ResetAllPlayers()
{
	if(this.uifc) uifc.pop("Creating Players");
	var player={};
	var i;

	db.lock(Settings.DB,'players',LOCK_WRITE);
	db.write(Settings.DB,'players',[]);
	for(i in PlayerProperties) {
		player[PlayerProperties[i]['prop']]=PlayerProperties[i]['def'];
	}
	player.UserNumber=0;
	player.Sector=0;
	db.push(Settings.DB,'players',{Excuse:"I hate zero-based arrays, so I'm just stuffing this crap in here"});
	for(i=0; i<Settings.MaxPlayers; i++) {
		db.push(Settings.DB,'players',player);
	}
	db.unlock(Settings.DB,'players');
}
