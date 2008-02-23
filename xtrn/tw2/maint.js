var CabalProperties = [
			{
				 prop:"Sector"
				,name:"Sector"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Size"
				,name:"Size"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Goal"
				,name:"Goal"
				,type:"Integer"
				,def:0
			}
		];

var cabals=new RecordFile(fname("cabal.dat"), CabalProperties);

function RunMaint()
{
	Settings.MaintLastRan = system.datestr();
	Settings.save();
	console.attributes="HY";
	console.writeln("Running maintence program...");
	console.crlf();
	console.crlf();
	console.writeln("Trade Wars maintence program");
	console.writeln("  by Chris Sherrick (PTL)");
	console.crlf();
	console.writeln("This program is run once per day.");
	twmsg.writeln(system.timestr()+": "+user.alias+": Maintence program ran");

	DeleteInactive();

	console.crlf();

	MoveCabal();
}

function DeleteInactive()
{
	console.writeln("Deleting inactive players");
	var oldest_allowed=today-Settings.DaysInactivity*86400;
	for(i=1; i<players.length; i++) {
		var p=players.Get(i);
		if(p.UserNumber > 0) {
			if((!file_exists(system.data_dir+format("user/%04d.tw2",p.UserNumber))) || (system.datestr(p.LastOnDay) < oldest_allowed && p.KilledBy != 0)) {
				DeletePlayer(p)
			}
		}
	}
}

function MoveCabalGroup(cabal, target)
{
	if(cabal.Sector==0 || cabal.Size < 1)
		return;
	var sector=sectors.Get(cabal.Sector);

	sector.Fighters -= cabal.Size;
	if(sector.Fighters < 1) {
		sector.Fighters=0;
		sector.FighterOwner=0;
	}
	sector.Put();

	/* Set new sector */
	cabal.Sector=target;
	sector=sectors.Get(cabal.Sector);

	/* Attack dropped fighters */
	if(sector.Fighters > 0 && sector.FighterOwner != -1) {
		var killed=0;
		var lost=0;
		var ownername="Deleted Player";
		if(sector.FighterOwner > 0) {
			var fowner=players.Get(sector.FighterOwner);
			ownername=fowner.Alias;
		}
		while(cabal.Size > 0 && sector.Fighters > 0) {
			if(random(2)==1) {
				cabal.Size--;
				lost++;
			}
			else {
				sector.Fighters--;
				killed++;
			}
		}
		/* Place into twpmsg.dat */
		var i;
		var msg=null;
		for(i=0; i<twpmsg.length; i++) {
			msg=twpmsg.Get(i);
			if(msg.Read)
				break;
			msg=null;
		}
		if(msg==null)
			msg=twpmsg.New();
		msg.Read=false;
		msg.To=sector.FighterOwner;
		msg.From=-1;
		msg.Destroyed=killed;
		msg.Put();
		twmsg.writeln("      Group "+cabal.Record+" --> Sector "+target+" ("+ownername+"): lost "+killed+", dstrd "+lost+" ("+(sector.Fighters==0?"Player":"Cabal")+" ftrs dstrd)");
		if(cabal.Size==0) {
			cabal.ReInit();
			cabal.Put();
			return;
		}
		else {
			sector.FighterOwner=-1;
			sector.Fighters=cabal.Size;
		}
		sector.Put();
	}

	CabalAttack(cabal);
	if(cabal.Sector==0 || cabal.Size < 1)
		return;

	/* Merge into another group */
	var i;
	for(i=1; i<cabals.length; i++) {
		if(i==cabal.Record)
			continue;
		var othercabal=cabals.Get(i);
		/* Merge Groups */
		if(othercabal.Sector==cabal.Sector) {
			if(othercabal.Record < cabal.Record) {
				othercabal.Size += cabal.Size;
				cabal.ReInit();
			}
			else {
				cabal.Size += othercabal.Size;
				othercabal.ReInit();
			}
			cabal.Put();
			othercabal.Put();
		}
	}
}

function MoveCabal()
{
	var total=0;
	var i;

	console.writeln("Moving the Cabal.");
	twmsg.writeln("    Cabal report:");
	/* Validate Groups and count the total */
	for(i=1; i<cabals.length; i++) {
		var cabal=cabals.Get(i);
		var sector=sectors.Get(cabal.Sector);
		cabal.Size=sector.Fighters;
		if(sector.FighterOwner != -1 || sector.Fighters==0) {
			cabal.Sector=0;
			cabal.Size=0;
			cabal.Goal=0;
		}
		cabal.Put();
		total+=cabal.Size;
	}

	/* Move group 2 into sector 85 (merge into group 1) */
	var cabal=cabals.Get(2);
	MoveCabalGroup(cabal, 85);

	/* Note, this seems to have a max limit of 2000 for regeneration */
	var regen=Settings.CabalRegeneration;
	if(total+regen > 2000)
		regen=2000-total;
	if(regen<1)
		regen=0;

	cabal=cabals.Get(1);
	cabal.Size += regen;

	/* Split off group 2 */
	group2=cabals.Get(2);
	if(cabal.Size > 1000) {
		group2.Size=cabal.Size-1000;
		group2.Sector=85;
		MoveCabalGroup(group2, 83);
	}

	/* Create wandering groups */
	for(i=3; i<6; i++) {
		var wgroup=cabals.Get(i);
		if(wgroup.Size < 1 || wgroup.Sector < 1 || wgroup.Sector >= sectors.length) {
			if(group2.size >= 600) {
				wgroup.Size=100;
				group2.size-=100;
				wgroup.Sector=83;
				wgroup.Goal=0;
				while(wgroup.Goal < 8)
					wgroup.Goal=random(sectors.length);
			}
			else {
				wgroup.ReInit();
			}
			wgroup.Put();
		}
	}

	/* Create attack groups */
	for(i=6; i<9; i++) {
		var agroup=cabals.Get(i);
		if(agroup.Size < 1 || agroup.Sector < 1 || agroup.Sector >= sectors.length) {
			if(group2.size >= 550) {
				agroup.Size=50;
				group2.size-=50;
				agroup.Sector=83;
				agroup.Goal=0;
				while(agroup.Goal < 8)
					agroup.Goal=random(sectors.length);
			}
			else {
				agroup.ReInit();
			}
			agroup.Put();
		}
	}
	
	/* Create hunter group */
	var hgroup=cabals.Get(9);
	if(hgroup.Size < 1 || hgroup.Sector < 1 || hgroup.Sector >= sectors.length) {
		if(group2.size >= 500) {
			hgroup.Size=group2.Size-500;
			group2.size-=hgroup.Size;
			hgroup.Sector=83;
			hgroup.Goal=0;
		}
		else {
			hgroup.ReInit();
		}
		hgroup.Put();
	}
	group2.Put();

	/* Move wandering groups */
	for(i=3; i<6; i++) {
		var wgroup=cabals.Get(i);
		if(wgroup.Size < 1 || wgroup.Sector == 0)
			continue;
		/* Choose new target */
		if(wgroup.Sector == wgroup.Goal) {
			wgroup.Goal=0;
			while(wgroup.Goal < 8)
				wgroup.Goal=random(sectors.length);
		}
		if(wgroup.Size < 50 || wgroup.Size > 100)
			wgroup.Goal=85;
		var path=ShortestPath(wgroup.Sector, wgroup.Goal);
		while(path[0] < 8)
			path.shift();
		MoveCabalGroup(wgroup,path[0]);
	}

	/* Move Attack Groups */
	for(i=6; i<9; i++) {
		var agroup=cabals.Get(i);
		if(agroup.Size < 1 || agroup.Sector == 0)
			continue;
		/* Choose new target */
		if(agroup.Sector == agroup.Goal) {
			agroup.Goal=0;
			while(agroup.Goal < 8)
				agroup.Goal=random(sectors.length);
		}
		if(agroup.Size < 20 || agroup.Size > 50)
			agroup.Goal=85;
		var path=ShortestPath(agroup.Sector, agroup.Goal);
		var next;
		while(path.length > 0 && agroup.Size > 0) {
			var next=path.shift();
			if(next < 8)
				continue;
			MoveCabalGroup(agroup,next);
		}
	}

	/* Move hunter groups... */
	for(i=9; i<10; i++) {
		var hgroup=cabals.Get(i);
		if(hgroup.Size < 1 || hgroup.Sector == 0)
			continue;
		/* Choose target */
		var ranks=RankPlayers();
		if(ranks.length < 0) {
			var player=players.Get(ranks[0].Record);
			hgroup.Goal=player.Sector;
			var path=ShortestPath(hgroup.Sector, hgroup.Goal);
			var next;
			while(path.length > 0 && hgroup.Size > 0) {
				var next=path.shift();
				MoveCabalGroup(agroup,next);
			}
		}
	}
}

function CabalAttack(cabal)
{
	/* Cabal groups lower than 6 (wandering and defence) do not attack players */
	if(cabal.Record < 6)
		return;

	/* Look for another player in the sector (attacking by rank) */
	var i;
	var attackwith=0;

	var ranks=RankPlayers();
	var sector=sectors.Get(cabal.Sector);
	if(sector.FighterOwner != -1)	/* Huh? */
		return;

	sector.Fighters -= cabal.Size;
	if(sector.Fighters < 0)
		sector.Fighters=0;

	for(i=0; i<ranks.length; i++) {
		var otherplayer=players.Get(ranks[i].Record);

		if(otherplayer.Sector != cabal.Sector)
			continue;
		if(otherplayer.UserNumber==0)
			continue;
		if(otherplayer.KilledBy != 0)
			continue;
		else if(cabal.Record<9)	/* Attack Group */
			attackwith=20;
		else if(cabal.Record==9) { /* Top player hunter */
			if(i==0)
				attackwith=cabal.Size;
			else
				attackwith=0;
		}

		if(attackwith==0)
			continue;
		if(attackwith > cabal.Size)
			attackwith = cabal.Size;

		cabal.Size -= attackwith;
		/* Attack the player */
		var killed=0;
		var lost=0;
		while(otherplayer.Fighters > 0 && attackwith > 0) {
			if(random(2)==1) {
				attackwith--;
				lost++;
			}
			else {
				otherplayer.Fighters--;
				killed++;
			}
		}
		cabal.Size += attackwith;
		twmsg.writeln("      Group "+cabal.Record+" --> "+otherplayer.Alias+": lost "+killed+ ", dstrd "+killed+" ("+(cabal.Size==0?"Cabal":"Player")+" dstrd)");
		if(cabal.Size==0)
			cabal.ReInit();
		else {				/* Player destroyed by the cabal! */
			otherplayer.KilledBy=-1;
			var loc=playerLocation.Get(otherplayer.Record);
			loc.Sector=0;
			loc.Put();
		}
		otherplayer.Put();
		cabal.Put();
	}
	sector.Fighters += cabal.Size;
	if(cabal.Size > 0)
		sector.FighterOwner=-1;
	sector.Put();
}

function InitializeCabal()
{
	var sector=sectors.Get(85);

	uifc.pop("Initializing the Cabal");
	sector_map[85].Fighters=3000;
	sector_map[85].FightersOwner=-1;
	for(i=1; i<=10; i++) {
		var grp=cabals.New();
		if(i==1) {
			grp.Size=3000;
			grp.Sector=85;
			grp.Goal=85;
		}
		grp.Put();
	}
}
