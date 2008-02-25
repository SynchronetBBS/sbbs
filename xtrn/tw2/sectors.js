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
		];

var sectors=new RecordFile(fname("sectors.dat"), SectorProperties);

function CheckSector()
{
	if(player.Sector==85) {
		console.crlf();
		console.attributes="W";
		console.writeln("Congratulations!  You've defeated the cabal and gained 100,000 points!");
		player.Points += 100000;
		player.Put();
		console.writeln("Unfortunately, victory is fleeting...");

		var sector=sectors.Get(85);
		sector.Fighters=3000;
		sector.FighterOwner=-1;
		sector.Put();
		var twopeng=new File(fname("twopeng.dat"));
		twopeng.open("a", true);

		twopeng.writeln("Congratulations go to "+player.Alias+
           " who invaded the Cabal empire on " + system.datestr() + 
		   " and received 100,000 points!");
		twopeng.close();
		return(false);
	}
	return(true);
}

function EnterSector()	/* 20000 */
{
	var sector=sectors.Get(player.Sector);
	var fighterteam=-1;

	console.attributes="Y";
	DisplaySector(sector.Record);
	if(sector.FighterOwner > 0) {
		var otherplayer=players.Get(sector.FighterOwner);
		if(otherplayer.TeamNumber > 0)
			fighterteam=otherplayer.TeamNumber;
	}
	while(sector.FighterOwner != player.Record && sector.Fighters > 0 && fighterteam != player.TeamNumber) {
		console.writeln("You have to destroy the fighters before you can enter this sector.");
		console.writeln("Fighters: "+player.Fighters+" / "+sector.Fighters);
		console.write("Option? (A,D,I,Q,R,?):? ");
		switch(InputFunc(['A','D','I','Q','R','?'])) {
			case 'A':
				console.writeln("<Attack>");
				var otherteam=false;

				if(sector.FighterOwner > 0) {
					var otherplayer=players.Get(sector.FighterOwner);
					if(otherplayer.TeamNumber>0)
						otherteam-true;
				}
				var killed=DoBattle(sector, otherteam);
				if(killed > 0) {
					player.Points+=k*100;
					console.writeln("You just recieved "+(k*100)+" points for that.");
				}
				if(sector.Fighters==0)
					console.writeln("You destroyed all the fighters.");
				break;
			case 'D':
				console.writeln("<Display>");
				DisplaySector(player.Sector);
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
				if(player.LastIn<1 || player.LastIn>=sectors.length)
					player.LastIn=random(sectors.length-1)+1;
				if(player.Fighters<1) {
					if(random(2)==1) {
						console.writeln("You escaped!");
						player.Sector=player.LastIn;
						player.Put();
						location.Sector=player.Sector; location.Put();
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
					player.Sector=player.LastIn;
					player.Put();
					location.Sector=player.Sector; location.Put();
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
	return(true);
}

function DisplaySector(secnum)
{
	var sector=sectors.Get(secnum);
	var i;
	var count=0;

	console.crlf();
	console.attributes="HY";
	console.writeln("Sector "+secnum);
	console.attributes="HR";
	console.write("Port   ");
	if(sector.Port > 0) {
		var port=ports.Get(sector.Port);
		console.write(port.Name+", class "+port.Class)
	}
	else
		console.write("None");
	console.crlf();
	console.attributes="HB";
	if(sector.Planet) {
		var planet=planets.Get(sector.Planet);
		console.writeln("Planet "+planet.Name);
	}
	console.attributes="HC";
	console.writeln("Other Ships");
	console.attributes="C";
	for(i=1;i<players.length;i++) {
		var otherloc=playerLocation.Get(i);

		if(otherloc.Sector==secnum) {
			var otherplayer=players.Get(i);

			if(otherplayer.UserNumber > 0 && otherplayer.Sector==secnum) {
				if(otherplayer.Record==player.Record)
					continue;
				if(otherplayer.KilledBy!=0)
					continue;

				count++;
				console.crlf();
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
			}
		}
	}
	if(count==0)
		console.write("   None");
	console.crlf();
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
			var otherplayer=players.Get(sector.FighterOwner);
			console.write(" (belong to "+otherplayer.Alias);
			if(otherplayer.TeamNumber)
				console.write("  Team ["+otherplayer.TeamNumber+"]")
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
	var univ=new Array(sectors.length);
	var done=false;
	var ret=new Array();

	function MarkWarps(univ, pos, end, hops)
	{
		var i;

		for(i=0; i<univ[pos].Warps.length; i++) {
			if(univ[pos].Warps[i]==0)
				continue;
			if(univ[univ[pos].Warps[i]]==undefined) {
				univ[univ[pos].Warps[i]]=sectors.Get(univ[pos].Warps[i]);
				univ[univ[pos].Warps[i]].hops=hops;
				if(univ[pos].Warps[i]==end)
					return(true);
			}
		}
		return(false);
	}

	/* Do the expansion */
	univ[start]=sectors.Get(start);
	univ[start].hops=hops;
	while(!done) {
		for(i=1; i<sectors.length; i++) {
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
		var lasthop=i;
		for(j=0; j<univ[i].Warps.length; j++) {
			if(univ[univ[i].Warps[j]]!=undefined && univ[univ[i].Warps[j]].hops<univ[i].hops) {
				i=univ[i].Warps[j];
				if(i!=start)
					ret.unshift(i);
				break;
			}
		}
		if(i==lasthop)
			return(null);
	}
	return(ret);
}

function InitializeSectors()
{
	uifc.pop("Writing Sectors");
	/* Write sectors.dat */
	sector=sectors.New();
	sector.Put();
	for(i=0; i<sector_map.length; i++) {
		sector=sectors.New();
		for(prop in sector_map[i])
			sector[prop]=sector_map[i][prop];
		sector.Put();
	}
}
