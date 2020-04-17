var DefaultSector = {
	Warps:[],
	Port:0,
	Planet:0,
	Fighters:0,
	FighterOwner:0,
	Ships:[]
};

var SectorProperties = [
			{
				 prop:"Warps"
				,name:"Warps"
				,type:"Array:6:Integer"
				,def:[0,0,0,0,0,0]
			}
			,{
				 prop:"Port"
				,name:"Port"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Planet"
				,name:"Planet"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Fighters"
				,name:"Fighters"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"FighterOwner"
				,name:"Fighters Owner"
				,type:"SignedInteger"
				,def:0
			}
			,{
				 prop:"Ships"
				,name:"Ships in Sector"
				,type:"Array:0:Integer"	// Ok, now we've broken it.
				,def:[]
			}
		];

function CheckSector()
{
	if(player.Sector==85) {
		console.crlf();
		console.attributes="W";
		console.writeln("Congratulations!  You've defeated the cabal and gained 100,000 points!");
		player.Points += 100000;
		player.Put();
		console.writeln("Unfortunately, victory is fleeting...");

		db.lock(Settings.DB,'sectors.85',LOCK_WRITE);
		var sector=db.read(Settings.DB,'sectors.85');
		sector.Fighters=3000;
		sector.FighterOwner=-1;
		sector=db.write(Settings.DB,'sectors.85',sector);
		db.unlock(Settings.DB,'sectors.85');
		db.push(Settings.DB,'twopeng',{Date:strftime("%Y:%m:%d"),Message:"Congratulations go to "+player.Alias+
           " who invaded the Cabal empire on " + strftime("%b %d, %Y") + 
		   " and received 100,000 points!"},LOCK_WRITE);
		return(false);
	}
	return(true);
}

function EnterSector()	/* 20000 */
{
	var lastsector;
	var fighterteam=-1;
	var otherplayer;
	var i;

	console.attributes="Y";
	sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	if(sector.FighterOwner > 0) {
		otherplayer=players.Get(sector.FighterOwner);
		if(otherplayer.TeamNumber > 0)
			fighterteam=otherplayer.TeamNumber;
	}
	while(sector.FighterOwner != player.Record && sector.Fighters > 0 && fighterteam != player.TeamNumber) {
		DisplaySector(sector,player.Sector,false,'enter.ans');
		console.writeln("You have to destroy the fighters before you can enter this sector.");
		console.writeln("Fighters: "+player.Fighters+" / "+sector.Fighters);
		console.write("Option? (A,D,I,Q,R,?):? ");
		switch(InputFunc(['A','D','I','Q','R','?'])) {
			case 'A':
				console.writeln("<Attack>");
				var otherteam=false;

				if(sector.FighterOwner > 0) {
					otherplayer=players.Get(sector.FighterOwner);
					if(otherplayer.TeamNumber>0)
						otherteam=true;
				}
				var killed=JSON_DoBattle('sectors.'+player.Sector, otherteam);
				if(killed > 0) {
					player.Points+=killed*100;
					console.writeln("You just recieved "+(killed*100)+" points for that.");
				}
				sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
				if(sector.Fighters==0)
					console.writeln("You destroyed all the fighters.");
				break;
			case 'D':
				console.writeln("<Display>");
				sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
				DisplaySector(sector,player.Sector,false,'enter.ans');
				break;
			case 'I':
				console.writeln("<Info>");
				PlayerInfo(player.Record);
				break;
			case 'Q':
				console.write("Are you sure (Y/N)? ");
				if(InputFunc(['Y','N'])=='Y') {
					exit_tw2=true;
					return(false);
				}
				break;
			case 'R':
				console.writeln("<Retreat>");
				var sectorsLen=db.read(Settings.DB,'sectors.length',LOCK_READ);
				while(player.LastIn<1 || player.LastIn>=sectorsLen || player.LastIn==player.Sector) {
					player.LastIn=random(sectorsLen-1)+1;
				}
				var oldsector=player.Sector;
				if(player.Fighters<1) {
					if(random(2)==1) {
						console.writeln("You escaped!");
						db.lock(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
						db.lock(Settings.DB,'sectors.'+player.LastIn,LOCK_WRITE);
						sector=db.read(Settings.DB,'sectors.'+player.Sector);
						lastsector=db.read(Settings.DB,'sectors.'+player.LastIn);
						lastsector.Ships.push(player.Record);
						for(i=0; i<sector.Ships.length; i++) {
							if(sector.Ships[i]==player.Record) {
								sector.Ships.splice(i,1);
								i--;
							}
						}
						db.write(Settings.DB,'sectors.'+player.Sector,sector);
						db.write(Settings.DB,'sectors.'+player.LastIn,lastsector);
						player.Sector=player.LastIn;
						player.Put();
						db.unlock(Settings.DB,'sectors.'+oldsector);
						db.unlock(Settings.DB,'sectors.'+player.LastIn);
						return(false);
					}
					console.attributes="R";
					console.writeln("You didn't escape!");
					console.writeln("Your ship has been destroyed!");
					console.crlf();
					console.attributes="R";
					console.writeln("You will start over tomorrow with a new ship.");
					KilledBy(player, player, true);
					return(false);
				}
				else {
					player.Fighters--;
					console.writeln("You have "+player.Fighters+" fighter(s) left.");
					db.lock(Settings.DB,'sectors.'+player.Sector,LOCK_WRITE);
					db.lock(Settings.DB,'sectors.'+player.LastIn,LOCK_WRITE);
					sector=db.read(Settings.DB,'sectors.'+player.Sector);
					lastsector=db.read(Settings.DB,'sectors.'+player.LastIn);
					lastsector.Ships.push(player.Record);
					for(i=0; i<sector.Ships.length; i++) {
						if(sector.Ships[i]==player.Record) {
							sector.Ships.splice(i,1);
							i--;
						}
					}
					db.write(Settings.DB,'sectors.'+player.Sector,sector);
					db.write(Settings.DB,'sectors.'+player.LastIn,lastsector);
					player.Sector=player.LastIn;
					player.Put();
					db.unlock(Settings.DB,'sectors.'+oldsector);
					db.unlock(Settings.DB,'sectors.'+player.LastIn);
					return(false);
				}
				break;
			case '':
				console.writeln("?=<Help>");
				break;
			case '?':
				console.writeln("<Help>");
				console.crlf();
				console.printfile(fname("entersector.asc"));
				break;
			default:
				console.crlf();
				console.writeln("Invalid command.  ? = Help");
		}
	}
	DisplaySector(sector,player.Sector,false,'main.ans');
	return(true);
}

function DisplaySector(sector, secnum, helponly, mainfname)
{
	var i;
	var count=0;
	var otherships=new Array();
	var otherplayer;
	var reads=[];

	for(i=0;i<sector.Ships.length;i++) {
		if(sector.Ships[i]==player.Record)
			continue;
		reads.push([Settings.DB,'players.'+sector.Ships[i],LOCK_READ,'players.'+sector.Ships[i]]);
	}

	if(user.settings&USER_ANSI) {
		console.printfile(fname(mainfname));
		if(sector.Port > 0)
			console.printfile(fname("port.ans"));
		if(sector.Planet > 0)
			console.printfile(fname("planet.ans"));
		if(sector.Fighters > 0 && sector.FighterOwner != 0)
			console.printfile(fname("fighters.ans"));
		if(reads.length > 0)
			console.printfile(fname("ship.ans"));
		console.printfile(fname("sector.ans"));
		console.attributes="HM";
		console.writeln(secnum);
	}
	if(helponly)
		return;

	if(sector.Port > 0)
		reads.push([Settings.DB,'ports.'+sector.Port,LOCK_READ,'port']);
	if(sector.Planet > 0)
		reads.push([Settings.DB,'planets.'+sector.Planet,LOCK_READ,'planet']);

	sd=db.readmulti(reads);

	console.crlf();
	console.attributes="HY";
	console.writeln("Sector "+secnum);
	console.attributes="HR";
	console.write("Port   ");
	if(sector.Port > 0) {
		console.write(sd.port.Name+", class "+sd.port.Class);
	}
	else
		console.write("None");
	console.crlf();
	console.attributes="HB";
	if(sector.Planet) {
		console.writeln("Planet "+sd.planet.Name);
	}
	console.attributes="HC";
	console.writeln("Other Ships");
	console.attributes="C";
	for(i in sd) {
		if(i.substr(0,8)=='players.') {
			otherships.push(new Player(sd[i],i.substr(8)));
		}
	}
	for(i in otherships) {
		otherplayer=otherships[i];
		console.write("   "+otherplayer.Alias);
		if(otherplayer.TeamNumber>0)
			console.write("  Team ["+otherplayer.TeamNumber+"]");
		console.write(" with "+otherplayer.Fighters+" fighters");
		if(otherplayer.Landed)
			console.write(" (on planet)");
		else if(otherplayer.Ported)
			console.write(" (docked)");
		else if(otherplayer.Online)
			console.write(" (online)");
		console.crlf();
	}
	if(otherships.length==0)
		console.writeln("   None");
	console.attributes="HG";
	console.writeln("Fighters defending sector");
	console.attributes="G";
	if(sector.Fighters==0)
		console.writeln("   None");
	else {
		console.write("   "+sector.Fighters);
		if(sector.FighterOwner<0)
			console.writeln(" (Cabal)");
		else if(sector.FighterOwner==player.Record)
			console.writeln(" (yours)");
		else {
			otherplayer=players.Get(sector.FighterOwner);
			console.write(" (belong to "+otherplayer.Alias);
			if(otherplayer.TeamNumber)
				console.write("  Team ["+otherplayer.TeamNumber+"]");
			console.writeln(")");
		}
	}
	console.attributes="HW";
	console.write("Warps lead to   ");
	count=0;
	for(i=0; i<sector.Warps.length; i++) {
		if(sector.Warps[i] != 0) {
			if(count)
				console.write(", ");
			console.write(sector.Warps[i]);
			count++;
		}
	}
	console.attributes="W";
	console.crlf();
}

function ShortestPath(start, end)
{
	var i,j;
	var hops=0;
	var seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
	var univ=new Array(seclen);
	var done=false;
	var ret=new Array();

	function MarkWarps(univ, pos, end, hops)
	{
		var i;

		for(i=0; i<univ[pos].Warps.length; i++) {
			if(univ[pos].Warps[i]==0)
				continue;
			if(univ[univ[pos].Warps[i]]==undefined) {
				univ[univ[pos].Warps[i]]=db.read(Settings.DB,'sectors.'+univ[pos].Warps[i],LOCK_READ);
				univ[univ[pos].Warps[i]].hops=hops;
				univ[univ[pos].Warps[i]].from=pos;
				if(univ[pos].Warps[i]==end)
					return(true);
			}
		}
		return(false);
	}

	/* Do the expansion */
	univ[start]=db.read(Settings.DB,'sectors.'+start,LOCK_READ);
	univ[start].hops=hops;
	while(!done) {
		for(i=1; i<seclen; i++) {
			if(univ[i]!=undefined && univ[i].hops==hops) {
				if(MarkWarps(univ, i, end, hops+1)) {
					done=true;
					break;
				}
			}
		}
		hops++;
	}
	ret.push(end);
	for(i=end;i!=start;) {
		i=univ[i].from;
		if(i!=start)
			ret.unshift(i);
	}
	return(ret);
}

function InitializeSectors()
{
	var i,prop;

	if(this.uifc) uifc.pop("Writing Sectors");
	/* Write sectors.dat */
	db.lock(Settings.DB,'sectors',LOCK_WRITE);
	db.write(Settings.DB,'sectors',[]);
	db.push(Settings.DB,'sectors',{Excuse:"I hate zero-based arrays, so I'm just stuffing this crap in here"});
	for(i=0; i<sector_map.length; i++) {
		var sector=eval(DefaultSector.toSource());
		for(prop in sector_map[i])
			sector[prop]=sector_map[i][prop];
		db.push(Settings.DB,'sectors',sector);
	}
	db.unlock(Settings.DB,'sectors');
}
