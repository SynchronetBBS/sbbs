var DefaultCabal = {
	Sector:0,
	Size:0,
	Goal:0
};

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

function RunMaint()
{
	Settings.MaintLastRan = strftime("%Y:%m:%d");
	Settings.save();
	console.attributes="HY";
	console.writeln("Running maintence program...");
	console.crlf();
	console.crlf();
	console.writeln("Trade Wars maintence program");
	console.writeln("  by Chris Sherrick (PTL)");
	console.crlf();
	console.writeln("This program is run once per day.");
	db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:user.alias+": Maintence program ran"},LOCK_WRITE);

	DeleteInactive();

	console.crlf();

	MoveCabal();
}

function DeleteInactive()
{
	console.writeln("Deleting inactive players");
	var oldest_allowed=strftime("%Y:%m:%d",time()-Settings.DaysInactivity*86400);
	var allplayers=db.read(Settings.DB,'players',LOCK_READ);
	for(i=1; i<allplayers.length; i++) {
		if(allplayers[i].QWKID==system.qwk_id && allplayers[i].UserNumber > 0) {
			if((!file_exists(system.data_dir+format("user/%04d.tw2",allplayers[i].UserNumber))) || (allplayers[i].LastOnDay < oldest_allowed && allplayers[i].KilledBy != 0)) {
				DeletePlayer(players.Get(i));
			}
		}
	}
}

function MoveCabalGroup(cabal, cabalRecord, target)
{
	var i;
	var cabalsLen=db.read(Settings.DB,'cabals.length');

	if(cabal.Sector==0 || cabal.Size < 1)
		return;

	db.lock(Settings.DB,'sectors.'+cabal.Sector,LOCK_WRITE);
	var sector=db.read(Settings.DB,'sectors.'+cabal.Sector);

	sector.Fighters -= cabal.Size;
	if(sector.Fighters < 1) {
		sector.Fighters=0;
		sector.FighterOwner=0;
	}
	db.write(Settings.DB,'sectors.'+cabal.Sector,sector);
	db.unlock(Settings.DB,'sectors.'+cabal.Sector);

	/* Set new sector */
	cabal.Sector=target;
	db.lock(Settings.DB,'sectors.'+cabal.Sector,LOCK_WRITE);
	sector=db.read(Settings.DB,'sectors.'+cabal.Sector);

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
		i;
		var msg={};
		msg.Read=false;
		msg.To=sector.FighterOwner;
		msg.From=-1;
		msg.Destroyed=killed;
		db.push(Settings.DB,'updates',msg,LOCK_WRITE);
		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"      Group "+cabalRecord+" --> Sector "+target+" ("+ownername+"): lost "+killed+", dstrd "+lost+" ("+(sector.Fighters==0?"Player":"Cabal")+" ftrs dstrd)"},LOCK_WRITE);
		if(cabal.Size==0) {
			cabal=eval(DefaultCabal.toSource());
			return;
		}
		else {
			sector.FighterOwner=-1;
			sector.Fighters=cabal.Size;
		}
	}
	db.write(Settings.DB,'sectors.'+cabal.Sector,sector);
	db.unlock(Settings.DB,'sectors.'+cabal.Sector);

	CabalAttack(cabal, cabalRecord);
	if(cabal.Sector==0 || cabal.Size < 1)
		return;

	/* Merge into another group */
	i;
	for(i=1; i<cabalsLen; i++) {
		if(i==cabalRecord)
			continue;
		var othercabal=db.read(Settings.DB,'cabals.'+i);
		/* Merge Groups */
		if(othercabal.Sector==cabal.Sector) {
			if(i < cabalRecord) {
				othercabal.Size += cabal.Size;
				cabal=eval(DefaultCabal.toSource());
			}
			else {
				cabal.Size += othercabal.Size;
				othercabal=eval(DefaultCabal.toSource());
			}
			db.write(Settings.DB,'cabals.'+cabalRecord, cabal);
			db.write(Settings.DB,'cabals.'+i, othercabal);
		}
	}
}

function MoveCabal()
{
	var total=0;
	var i;
	var cabal;
	var group2;
	var wgroup,agroup,hgroup;
	var path,next;
	db.lock(Settings.DB,'cabals',LOCK_WRITE);
	var cabalsLen=db.read(Settings.DB,'cabals.length');

	console.writeln("Moving the Cabal.");
	db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"    Cabal report:"},LOCK_WRITE);
	/* Validate Groups and count the total */
	for(i=1; i<cabalsLen; i++) {
		cabal=db.read(Settings.DB,'cabals.'+i);
		var sector=db.read(Settings.DB,'sectors.'+cabal.Sector,LOCK_READ);
		cabal.Size=sector.Fighters;
		if(sector.FighterOwner != -1 || sector.Fighters==0) {
			cabal.Sector=0;
			cabal.Size=0;
			cabal.Goal=0;
		}
		db.write(Settings.DB,'cabals.'+i, cabal);
		total+=cabal.Size;
	}

	/* Move group 2 into sector 85 (merge into group 1) */
	cabal=db.read(Settings.DB,'cabals.2');
	MoveCabalGroup(cabal, 2, 85);
	db.write(Settings.DB,'cabals.2',cabal);

	/* Note, this seems to have a max limit of 2000 for regeneration */
	var regen=Settings.CabalRegeneration;
	if(total+regen > 2000)
		regen=2000-total;
	if(regen<1)
		regen=0;

	cabal=db.read(Settings.DB,'cabals.1');
	cabal.Size += regen;

	/* Split off group 2 */
	group2=db.read(Settings.DB,'cabals.2');
	if(cabal.Size > 1000) {
		group2.Size=cabal.Size-1000;
		group2.Sector=85;
		MoveCabalGroup(group2, 2, 83);
		db.write(Settings.DB,'cabals.2',cabal2);
	}

	var seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
	/* Create wandering groups */
	for(i=3; i<6; i++) {
		wgroup=db.read(Settings.DB,'cabals.'+i);
		if(wgroup.Size < 1 || wgroup.Sector < 1 || wgroup.Sector >= seclen) {
			if(group2.size >= 600) {
				wgroup.Size=100;
				group2.size-=100;
				wgroup.Sector=83;
				wgroup.Goal=0;
				while(wgroup.Goal < 8)
					wgroup.Goal=random(seclen-1)+1;
			}
			else {
				wgroup=eval(DefaultCabal.toSource());
			}
			db.write(Settings.DB,'cabals.'+i, wgroup);
		}
	}

	/* Create attack groups */
	for(i=6; i<9; i++) {
		agroup=db.read(Settings.DB,'cabals.'+i);
		if(agroup.Size < 1 || agroup.Sector < 1 || agroup.Sector >= seclen) {
			if(group2.size >= 550) {
				agroup.Size=50;
				group2.size-=50;
				agroup.Sector=83;
				agroup.Goal=0;
				while(agroup.Goal < 8)
					agroup.Goal=random(seclen-1)+1;
			}
			else {
				agroup=eval(DefaultCabal.toSource());
			}
			db.write(Settings.DB,'cabals.'+i, agroup);
		}
	}
	
	/* Create hunter group */
	hgroup=db.read(Settings.DB,'cabals.9');
	if(hgroup.Size < 1 || hgroup.Sector < 1 || hgroup.Sector >= seclen) {
		if(group2.size >= 500) {
			hgroup.Size=group2.Size-500;
			group2.size-=hgroup.Size;
			hgroup.Sector=83;
			hgroup.Goal=0;
		}
		else {
			hgroup=eval(DefaultCabal.toSource());
		}
		db.write(Settings.DB,'cabals.9', hgroup);
	}
	db.write(Settings.DB,'cabals.2', group2);

	/* Move wandering groups */
	for(i=3; i<6; i++) {
		wgroup=db.read(Settings.DB,'cabals.'+i);
		if(wgroup.Size < 1 || wgroup.Sector == 0)
			continue;
		/* Choose new target */
		if(wgroup.Sector == wgroup.Goal) {
			wgroup.Goal=0;
			while(wgroup.Goal < 8)
				wgroup.Goal=random(seclen-1)+1;
		}
		if(wgroup.Size < 50 || wgroup.Size > 100)
			wgroup.Goal=85;
		path=ShortestPath(wgroup.Sector, wgroup.Goal);
		while(path[0] < 8)
			path.shift();
		MoveCabalGroup(wgroup,i,path[0]);
		db.write(Settings.DB,'cabals.'+i,wgroup);
	}

	/* Move Attack Groups */
	for(i=6; i<9; i++) {
		agroup=db.read(Settings.DB,'cabals.'+i);
		if(agroup.Size < 1 || agroup.Sector == 0)
			continue;
		/* Choose new target */
		if(agroup.Sector == agroup.Goal) {
			agroup.Goal=0;
			while(agroup.Goal < 8)
				agroup.Goal=random(seclen);
		}
		if(agroup.Size < 20 || agroup.Size > 50)
			agroup.Goal=85;
		path=ShortestPath(agroup.Sector, agroup.Goal);
		while(path.length > 0 && agroup.Size > 0) {
			next=path.shift();
			if(next < 8)
				continue;
			MoveCabalGroup(agroup,i,next);
			db.write(Settings.DB,'cabals.'+i,agroup);
		}
	}

	/* Move hunter groups... */
	for(i=9; i<10; i++) {
		hgroup=db.read(Settings.DB,'cabals.'+i);
		if(hgroup.Size < 1 || hgroup.Sector == 0)
			continue;
		/* Choose target */
		var ranks=RankPlayers();
		if(ranks.length < 0) {
			var player=players.Get(ranks[0].Record);
			hgroup.Goal=player.Sector;
			path=ShortestPath(hgroup.Sector, hgroup.Goal);
			while(path.length > 0 && hgroup.Size > 0) {
				next=path.shift();
				MoveCabalGroup(hgroup,i,next);
				db.write(Settings.DB,'cabals.'+i,hgroup);
			}
		}
	}
	db.unlock(Settings.DB,'cabals');
}

function CabalAttack(cabal, cabalRecord)
{
	/* Cabal groups lower than 6 (wandering and defence) do not attack players */
	if(cabalRecord < 6)
		return;

	/* Look for another player in the sector (attacking by rank) */
	var i;
	var attackwith=0;

	var ranks=RankPlayers();
	db.lock(Settings.DB,'sectors.'+cabal.Sector,LOCK_WRITE);
	var sector=db.read(Settings.DB,'sectors.'+cabal.Sector);
	if(sector.FighterOwner != -1) {	/* Huh? */
		db.unlock(Settings.DB,'sectors.'+cabal.Sector);
		return;
	}

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
		else if(cabalRecord<9)	/* Attack Group */
			attackwith=20;
		else if(cabalRecord==9) { /* Top player hunter */
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
		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"      Group "+cabalRecord+" --> "+otherplayer.Alias+": lost "+killed+ ", dstrd "+killed+" ("+(cabal.Size==0?"Cabal":"Player")+" dstrd)"},LOCK_WRITE);
		if(cabal.Size==0)
			cabaleval(DefaultCabal.toSource());
		else {				/* Player destroyed by the cabal! */
			otherplayer.KilledBy=-1;
			otherplayer.Sector=0;
		}
		otherplayer.Put();
		db.write(Settings.DB,'cabals.'+cabalRecord, cabal);
	}
	sector.Fighters += cabal.Size;
	if(cabal.Size > 0)
		sector.FighterOwner=-1;
	db.write(Settings.DB,'sectors.'+cabal.Sector,sector);
	db.unlock(Settings.DB,'sectors.'+cabal.Sector);
}

function InitializeCabal()
{
	var sector=db.read(Settings.DB,'sectors.85',LOCK_READ);

	if(this.uifc) uifc.pop("Initializing the Cabal");
	sector_map[85].Fighters=3000;
	sector_map[85].FightersOwner=-1;
	sector.Fighters=3000;
	sector.FightersOwner=-1;
	db.write(Settings.DB,'sectors.85',sector,LOCK_WRITE);
	db.lock(Settings.DB,'cabals',LOCK_WRITE);
	db.write(Settings.DB,'cabals',[]);
	db.push(Settings.DB,'cabals',DefaultCabal);
	for(i=1; i<10; i++) {
		var grp=eval(DefaultCabal.toSource());
		if(i==1) {
			grp.Size=3000;
			grp.Sector=85;
			grp.Goal=85;
		}
		db.write(Settings.DB,'cabals.'+i,grp);
	}
	db.unlock(Settings.DB,'cabals');
}
