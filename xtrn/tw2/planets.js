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
				 prop:"OccupiedBy"
				,name:"Occupied By"
				,type:"Integer"
				,def:0
			}
		];

var planets=new RecordFile(fname("planets.dat"), PlanetProperties);

function LandOnPlanet()
{
	console.crlf();
	console.attributes="HY";
	console.writeln("Landing...");
	var sector=sectors.Get(player.Sector);
	var planet=null;
	if(sector.Planet==0) {
		/* TODO: Race condition? */
		CreatePlanet(sector);
	}
	else {
		/* 32310 */
		/* Lock the planet file and ensure we can land... */
		if(!Lock(planets.file.name, bbs.node_num, true, 5)) {
			console.writeln("The spaceport is busy.  Try again later.");
			return;
		}
		planet=planets.Get(sector.Planet);
		if(planet.OccupiedBy != 0) {
			console.writeln("The spaceport is busy.  Try again later.");
			Unlock(planets.file.name);
			return;
		}
		planet.OccupiedBy=player.Record;
		planet.Put();
		Unlock(planets.file.name);
		player.Landed=true;
		player.Put();

		Production(planet);
		PlanetReport(planet);
		console.attributes="HW";
		PlanetMenu(planet);

		if(!Lock(planets.file.name, bbs.node_num, true, 10))
			twmsg.writeln("!!! Error locking planets file!");
		planet.OccupiedBy=0;
		planet.Put();
		Unlock(planets.file.name);
		player.Landed=false;
		player.Put();
	}
}

function CreatePlanet(sector)
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
		var planet;
		for(i=1; i<planets.length; i++) {
			planet=planets.Get(i);
			if(!planet.Created)
				break;
			planet=null;
		}
		if(planet==null) {
			console.crlf();
			console.writeln("I'm sorry, but all planets are taken.");
			console.writeln("One has to be destroyed before you can buy a planet.");
			return(false);
		}

		planet.Name='';
		console.write("What do you want to name this planet? (41 chars. max)? ");
		planet.Name=console.getstr(41);
		if(planet.Name=='')
			return(false);
		for(i=0; i<Commodities.length; i++) {
			planet.Commodities[i]=1;
			planet.Production[i]=1;
		}
		player.Credits-=10000;
		player.Put();
		planet.Sector=sector.Record;
		sector.Planet=planet.Record;
		planet.LastUpdated=time();
		planet.Put();
		sector.Put();
		twmsg.writeln("  -  "+player.Alias+" made a planet: "+planet.Name);
		console.crlf();
		console.writeln("Planet created");
		return(true);
	}
}

function PlanetReport(planet)
{
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

function PlanetMenu(planet)
{
	var key;
	var freeholds=player.Holds;
	var i;
	var values=new Array('A','I','D','L','R');

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
				freeholds = PlanetTakeAll(planet, freeholds);
				break;
			case 'I':
				console.writeln("<Increase productivity>");
				PlanetIncreaseProd(planet);
				break;
			case 'D':
				if(DestroyPlanet(planet))
					return;
				break;
			case 'L':
				return;
			case 'R':
				PlanetReport(planet);
				break;
			case '?':
				console.crlf();
				console.printfile(fname("planet.asc"));
				break;
			default:
				var keynum=parseInt(key);
				if(keynum > 0 && keynum <= Commodities.length)
					freeholds=PlanetTakeCommodity(planet, keynum, freeholds);
				else
					console.writeln("Invalid command.  Enter ? for help");
		}
	}
}

function DestroyPlanet(planet)
{
	console.crlf();
	console.attributes="RI7H";
	console.writeln("*** DESTROY THE PLANET ***");
	console.crlf();
	console.attributes="Y";
	console.write("Are you sure (Y/N)[N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		planet.Created=false;
		var sector=sectors.Get(planet.Sector);
		sector.Planet=0;
		planet.Sector=0;
		planet.Put();
		sector.Put();
		twmsg.writeln("  -  " + player.Alias + " destroyed the planet in sector " + sector.Record);
		console.writeln("Planet destroyed.");
		return(true);
	}
	return(flase);
}

function PlanetTakeAll(planet, freeholds)
{
	if(freeholds < 1) {
		console.writeln("You don't have any free holds.");
		return(freeholds);
	}
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
	player.Put();
	planet.Put();
	return(freeholds);
}

function PlanetIncreaseProd(planet)
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
		if(planet.Production[keynum]>19) {
			console.writeln("It's at its maximum value.");
			return;
		}
		var max=parseInt(player.Credits/(Commodities[keynum].price*20));
		if(max<1) {
			console.writeln("You're too poor.  You only have "+player.Credits+" credits.");
			return;
		}
		if(planet.Production[keynum]+max > 19)
			max=20-planet.Production[keynum];
		console.write(Commodities[keynum].name+": Increase by how many units? ");
		var incr=console.getnum(max);
		if(incr > 0 && incr <= max) {
			player.Credits -= incr*Commodities[keynum].price*20;
			planet.Production[keynum]+=incr;
			planet.Put();
			player.Put();
		}
	}
}

function PlanetTakeCommodity(planet, commodity, freeholds)
{
	commodity--;
	var max=freeholds;
	if(max > parseInt(planet.Commodities[commodity]))
		max=parseInt(planet.Commodities[commodity]);
	console.writeln("<Take "+Commodities[commodity].name.toLowerCase()+">");
	console.write("How much [" + max + "]? ");
	var take=console.getnum(max);
	if(take > max) {
		console.writeln("They don't have that many.");
		return(freeholds);
	}
	if(take > freeholds) {
		console.writeln("You don't have enough free cargo holds.");
		return(freeholds);
	}
	planet.Commodities[commodity]-=take;
	player.Commodities[commodity]+=take;
	freeholds -= take;
	planet.Put();
	player.Put();
	return(freeholds);
}

function ResetAllPlanets()
{
	uifc.pop("Creating Planets");
	var i;
	var planet=planets.New();
	planet.Put();
	for(i=0; i<Settings.MaxPlanets; i++) {
		planet=planets.New();
		planet.Put();
	}
}
