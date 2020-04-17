var DefaultPlanet = {
	Name:'',
	Created:false,
	Commodities:[0,0,0],
	Production:[0,0,0],
	LastUpdate:0,
	Sector:0,
	OccupiedCount:0
}

var PlanetProperties = [
			{
				 prop:"Name"
				,name:"Name"
				,type:"String:42"
				,def:""
			}
			,{
				 prop:"Created"
				,name:"Planet has been created"
				,type:"Boolean"
				,def:false
			}
			,{
				 prop:"Commodities"
				,name:"Commodities"
				,type:"Array:3:Float"
				,def:[0,0,0]
			}
			,{
				 prop:"Production"
				,name:"Production (units/day)"
				,type:"Array:3:SignedInteger"
				,def:[0,0,0]
			}
			,{
				 prop:"LastUpdate"
				,name:"Planet last produced"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"Sector"
				,name:"Location of planet"
				,type:"Integer"
				,def:0
			}
			,{
				 prop:"OccupiedCount"
				,name:"Players on planet"
				,type:"Integer"
				,def:0
			}
		];

function LandOnPlanet()
{
	console.crlf();
	console.attributes="HY";
	console.writeln("Landing...");
	var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	var planet=null;

	if(sector.Planet==0) {
		/* TODO: Race condition? */
		CreatePlanet(player.Sector);
	}
	else {
		/* 32310 */
		/* Lock the planet file and ensure we can land... */
		db.lock(Settings.DB,'planets.'+sector.Planet,LOCK_WRITE);
		planet=db.read(Settings.DB,'planets.'+sector.Planet);
		planet.OccupiedCount++;
		player.Landed=true;
		player.Put();

		LockedProduction(planet);
		db.write(Settings.DB,'planets.'+sector.Planet,planet);
		db.unlock(Settings.DB,'planets.'+sector.Planet);
		if(user.settings&USER_ANSI)
			ANSIPlanet(sector.Planet);
		PlanetReport(sector.Planet);
		console.attributes="HW";
		PlanetMenu(sector.Planet);

		db.lock(Settings.DB,'planets.'+sector.Planet,LOCK_WRITE);
		planet=db.read(Settings.DB,'planets.'+sector.Planet);
		planet.OccupiedCount--;
		db.write(Settings.DB,'planets.'+sector.Planet,planet);
		db.unlock(Settings.DB,'planets.'+sector.Planet);
		player.Landed=false;
		player.Put();
	}
}

// TODO: Sector/Planet locking dance needed here so we end up with just a single locked planet
function NextAvailablePlanet()
{
	var i;
	var planet;
	var planetLen;

	db.lock(Settings.DB,'planets',LOCK_WRITE);
	planetLen=db.read(Settings.DB,'planets.length');

	for(i=1; i<planetLen; i++) {
		planet=db.read(Settings.DB,'planets.'+i);
		if(!planet.Created)
			break;
		planet=null;
	}

	if(planet != null) {
		planet.Created=true;
		planet.Sector=0;
		db.write(Settings.DB,'planets.'+i,planet);
		db.unlock(Settings.DB,'planets');
		db.lock(Settings.DB,'planets.'+i,LOCK_WRITE);
	}
	else {
		db.unlock(Settings.DB,'planets');
		return -1;
	}
	return(i);
}

// TODO: Sector/Planet locking dance needed here
function CreatePlanet(sectorNum)
{
	console.writeln("There isn't a planet in this sector.");
	console.writeln("Planets cost 10,000 credits.");
	if(player.Credits < 10000) {
		console.writeln("You're too poor to buy one.");
		return(false);
	}
	console.writeln("You have " + player.Credits + " credits.");
	console.write("Do you wish to buy a planet(Y/N) [N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		var i;
		var planetNum=NextAvailablePlanet();
		if(planetNum==-1) {
			console.crlf();
			console.writeln("I'm sorry, but all planets are taken.");
			console.writeln("One has to be destroyed before you can buy a planet.");
			return(false);
		}

		planet=db.read(Settings.DB,'planets.'+planetNum);
		db.unlock(Settings.DB,'planets.'+planetNum);
		planet.Name='';
		console.write("What do you want to name this planet? (41 chars. max)? ");
		planet.Name=console.getstr(41);
		db.lock(Settings.DB,'planets.'+planetNum,LOCK_WRITE);
		db.lock(Settings.DB,'sectors.'+sectorNum,LOCK_WRITE);
		var sector=db.read(Settings.DB,'sectors.'+sectorNum);
		if(sector.Planet != 0) {
			console.crlf();
			console.writeln("While you were deciding on a name, someone else has created a planet in this");
			console.writeln("sector (saving you 10,000 credits!).");
			planet.Name='';
		}
		if(planet.Name=='') {
			planet.Created=false;
			db.write(Settings.DB,'planets.'+planetNum, planet);
			db.unlock(Settings.DB,'planets.'+planetNum);
			db.unlock(Settings.DB,'sectors.'+sectorNum);
			return(false);
		}
		for(i=0; i<Commodities.length; i++) {
			planet.Commodities[i]=1;
			planet.Production[i]=1;
		}
		player.Credits-=10000;
		player.Put();
		planet.Sector=sector.Record;
		sector.Planet=planetNum;
		planet.LastUpdated=time();
		planet.Created=true;
		db.write(Settings.DB,'planets.'+planetNum, planet);
		db.write(Settings.DB,'sectors.'+sectorNum, sector);
		db.unlock(Settings.DB,'planets.'+planetNum);
		db.unlock(Settings.DB,'sectors.'+sectorNum);
		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"  -  "+player.Alias+" made a planet: "+planet.Name},LOCK_WRITE);
		console.crlf();
		console.writeln("Planet created");
		return(true);
	}
}

function PlanetReport(planetNum)
{
	var planet=db.read(Settings.DB,'planets.'+planetNum,LOCK_READ);
	console.crlf();
	console.attributes="HC";
	console.writeln("Planet: "+planet.Name);
	console.writeln(" Item      Prod.  Amount  in holds");
	console.attributes="HK";
	console.writeln(" ====      =====  ======  ========");
	var i;
	var freeholds=player.Holds;
	for(i=0; i<Commodities.length; i++) {
		console.attributes=['HR','HG','HY','HB','HM','HC','HW'][i];
		console.writeln(format("%s   %2d  %6d %8d"
				,Commodities[i].disp
				,planet.Production[i]
				,parseInt(planet.Commodities[i])
				,player.Commodities[i]));
		freeholds-=player.Commodities[i];
	}
	console.attributes="HG";
	console.writeln("You have "+freeholds+" free cargo holds.");
}

function ANSIPlanet(planetNum)
{
	var planet=db.read(Settings.DB, 'planets.'+planetNum,LOCK_READ);

	console.printfile(fname("landed.ans"));
	console.gotoxy(3,1);
	console.attributes="HM";
	console.writeln(planet.Name+ascii(204));
	if(planet.OccupiedCount > 1) {
		console.printfile(fname("landship.ans"));
	}
	console.gotoxy(1,9);
	console.attributes="N";
}

function PlanetMenu(planetNum)
{
	var key;
	var freeholds=player.Holds;
	var i;
	var values=new Array('A','I','D','L','R','?');

	for(i=0; i<Commodities.length; i++) {
		freeholds-=player.Commodities[i];
		values.push((i+1).toString());
	}

	while(true) {
		console.crlf();
		console.write("Planet command (?=help)[A]? ");
		key=InputFunc(values);
		switch(key) {
			case '':
			case 'A':
				console.writeln("<Take all>");
				freeholds = PlanetTakeAll(planetNum, freeholds);
				break;
			case 'I':
				console.writeln("<Increase productivity>");
				PlanetIncreaseProd(planetNum);
				break;
			case 'D':
				if(DestroyPlanet(planetNum))
					return;
				break;
			case 'L':
				return;
			case 'R':
				PlanetReport(planetNum);
				break;
			case '?':
				if(user.settings&USER_ANSI)
					ANSIPlanet(planetNum);
				else {
					console.crlf();
					console.printfile(fname("planet.asc"));
				}
				break;
			default:
				var keynum=parseInt(key);
				if(keynum > 0 && keynum <= Commodities.length)
					freeholds=PlanetTakeCommodity(planetNum, keynum, freeholds);
				else
					console.writeln("Invalid command.  Enter ? for help");
		}
	}
}

function DestroyPlanet(planetNum)
{
	var secnum;
	
	console.crlf();
	console.attributes="RIH";
	console.writeln("*** DESTROY THE PLANET ***");
	console.crlf();
	console.attributes="Y";
	console.write("Are you sure (Y/N)[N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		db.lock(Settings.DB,'planets.'+planetNum,LOCK_WRITE);
		var planet=db.read(Settings.DB,"planets."+planetNum);
		secnum=planet.Sector;
		db.lock(Settings.DB,'sectors.'+secnum,LOCK_WRITE);
		if(planet.OccupiedCount > 1) {
			console.writeln("Another player prevents destroying the planet.");
			db.unlock(Settings.DB,'planets.'+planetNum);
			db.unlock(Settings.DB,'sectors.'+secnum);
			return(false);
		}
		var sector=db.read(Settings.DB,'sectors.'+secnum);
		if(sector.Planet==planetNum)
			sector.Planet=0;
		planet.Created=false;
		planet.Sector=0;
		db.write(Settings.DB,'sectors.'+secnum,sector);
		db.unlock(Settings.DB,'sectors.'+secnum);
		db.write(Settings.DB,'planets.'+planetNum,planet);
		db.unlock(Settings.DB,'planets.'+planetNum);
		db.push(Settings.DB,'log',{Date:strftime("%a %b %d %H:%M:%S %Z"),Message:"  -  " + player.Alias + " destroyed the planet in sector " + secnum},LOCK_WRITE);
		console.writeln("Planet destroyed.");
		return(true);
	}
	return(false);
}

function PlanetTakeAll(planetNum, freeholds)
{
	var planet;

	if(freeholds < 1) {
		console.writeln("You don't have any free holds.");
		return(freeholds);
	}
	/*
	 * Re-read the planet struct 
	 */
	db.lock(Settings.DB,'planets.'+planetNum,LOCK_WRITE);
	planet=db.read(Settings.DB,'planets.'+planetNum);
	for(i=Commodities.length-1; i>=0; i--) {
		var take=parseInt(planet.Commodities[i]);
		if(take > freeholds)
			take=freeholds;
		if(take) {
			console.writeln("You took " + take + " holds of "+Commodities[i].name.toLowerCase());
			player.Commodities[i]+=take;
			planet.Commodities[i]-=take;
			freeholds-=take;
		}
		if(freeholds < 1) {
			console.writeln("Your cargo holds are full.");
			break;
		}
	}
	db.write(Settings.DB,'planets.'+planetNum,planet);
	db.unlock(Settings.DB,'planets.'+planetNum);
	player.Put();
	return(freeholds);
}

function PlanetIncreaseProd(planetNum)
{
	console.crlf();
	console.writeln("You have "+player.Credits+" credits.");
	var opts="";
	var values=new Array();
	for(i=0; i<Commodities.length; i++) {
		console.writeln((i+1)+" - "+Commodities[i].name+" costs "+(Commodities[i].price*20));
		if(i)
			opts += ',';
		opts += (i+1).toString();
		values.push((i+1).toString());
	}
	console.write("Which one do you want to increase ("+opts+")? ");
	var keynum=parseInt(InputFunc(values));
	if(keynum > 0 && keynum <= Commodities.length) {
		keynum--;
		db.lock(Settings.DB,'planets.'+planetNum,LOCK_WRITE);
		planet=db.read(Settings.DB,'planets.'+planetNum);
		if(planet.Production[keynum]>19) {
			console.writeln("It's at its maximum value.");
			db.unlock(Settings.DB,'planets.'+planetNum);
			return;
		}
		var max=parseInt(player.Credits/(Commodities[keynum].price*20));
		if(max<1) {
			console.writeln("You're too poor.  You only have "+player.Credits+" credits.");
			db.unlock(Settings.DB,'planets.'+planetNum);
			return;
		}
		if(planet.Production[keynum]+max > 19)
			max=20-planet.Production[keynum];
		db.unlock(Settings.DB,'planets.'+planetNum);
		console.write(Commodities[keynum].name+": Increase by how many units? ");
		var incr=InputFunc([{min:0,max:max}]);
		if(incr > 0 && incr <= max) {
			db.lock(Settings.DB,'planets.'+planetNum,LOCK_WRITE);
			planet=db.read(Settings.DB,'planets.'+planetNum);
			if(planet.Production[keynum]+incr > 20)
				incr=20-planet.Production[keynum];
			player.Credits -= incr*Commodities[keynum].price*20;
			planet.Production[keynum]+=incr;
			db.write(Settings.DB,'planets.'+planetNum,planet);
			db.unlock(Settings.DB,'planets.'+planetNum);
			player.Put();
			console.writeln("Production of "+Commodities[keynum].name+" increased by "+incr+" for "+incr*Commodities[keynum].price*20+" credits.");
		}
	}
}

function PlanetTakeCommodity(planetNum, commodity, freeholds)
{
	var planet;
	var max;

	commodity--;
	max=freeholds;
	if(max > parseInt(planet.Commodities[commodity]))
		max=parseInt(planet.Commodities[commodity]);
	console.writeln("<Take "+Commodities[commodity].name.toLowerCase()+">");
	console.write("How much [" + max + "]? ");
	var take=InputFunc([{min:0,max:max}]);
	/*
	 * Re-read the planet struct 
	 */
	db.lock(Settings.DB,'planets.'+planetNum,LOCK_WRITE);
	planet=db.read(Settings.DB,'planets.'+planetNum);
	max=freeholds;
	if(max > parseInt(planet.Commodities[commodity]))
		max=parseInt(planet.Commodities[commodity]);
	if(take > max) {
		console.writeln("They don't have that many.");
		db.unlock(Settings.DB,'planets.'+planetNum);
		return(freeholds);
	}
	if(take > freeholds) {
		console.writeln("You don't have enough free cargo holds.");
		db.unlock(Settings.DB,'planets.'+planetNum);
		return(freeholds);
	}
	planet.Commodities[commodity]-=take;
	db.write(Settings.DB,'planets.'+planetNum,planet);
	db.unlock(Settings.DB,'planets.'+planetNum);
	player.Commodities[commodity]+=take;
	freeholds -= take;
	player.Put();
	return(freeholds);
}

function ResetAllPlanets()
{
	var i;

	if(this.uifc) uifc.pop("Creating Planets");
	db.lock(Settings.DB,'planets',LOCK_WRITE);
	db.write(Settings.DB,'planets',[]);
	db.push(Settings.DB,'planets',DefaultPlanet);
	for(i=0; i<Settings.MaxPlanets; i++) {
		db.push(Settings.DB,'planets',DefaultPlanet);
	}
	db.unlock(Settings.DB,'planets');
}
