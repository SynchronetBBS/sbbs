var factions=new Array();

if(js.global.terrains==undefined)
	load(script_dir+'gamedefs.js');
if(js.global.Region==undefined)
	load(script_dir+'region.js');
if(js.global.Building==undefined)
	load(script_dir+'building.js');
if(js.global.Ship==undefined)
	load(script_dir+'ship.js');
if(js.global.Unit==undefined)
	load(script_dir+'unit.js');
if(js.global.Event==undefined)
	load(script_dir+'event.js');

function Faction()
{
	this.no=0;
	this.name='';
	this.addr='';
	this.password='';
	this.lastorders=0;
	this.seendata=new Array();
	this.showdata=new Array();
	this.accept=new Array();
	this.admit=new Array();
	this.allies=new Array();
	this.mistakes=new Array();
	this.messages=new Array();
	this.battles=new Array();
	this.events=new Array();
	this.alive=false;
	this.attacking=false;
	this.dh=false;
	this.nunits=0;
	this.number=0;
	this.money=0;
	this.__defineGetter__("id", function() { return(this.name+' ('+this.no+')'); });
	this.__defineGetter__("magicians", function() {
		var n,r,u;

		n = 0;

		for(r in regions)
			for (u in regions[r].units)
				if (regions[r].units[u].skills[SK_MAGIC] && regions[r].units[u].faction.no == this.no)
					n += regions[r].units[u].number;

		return n;
	});

	this.ispresent=function(region) {
		var unit;

		for (unit in region.units)
			if(region.units[unit].faction.no==this.no)
				return(true);

		return(false);
	};
	this.cansee=function(unit) {
		var cansee=0;
		var stealth;
		var observation;
		var u;

		/* You can always see your own units in detail */
		if(unit.faction.no==this.no)
			return(2);

		/* You can always see units on guard, in a building, or on a ship */
		if (unit.guard || unit.building || unit.ship)
			cansee = 1;

		/* Stealthiness */
		stealth = unit.effskill(SK_STEALTH);

		for(u in unit.region.units) {
			/* Only your own units help see things. */
			if(unit.region.units[u].faction.no != this.no)
				continue;

			/* If the unit is invisible and your unit doesn't have true seeing, your unit doesn't help */
			if(unit.items[I_RING_OF_INVISIBILITY]
					&& unit.items[I_RING_OF_INVISIBILITY] == unit.number
					&& !unit.region.units[u].items[I_AMULET_OF_TRUE_SEEING])
				continue;

			observation=unit.region.units[u].effskill(SK_OBSERVATION);

			/* If you're better at observation than they are at hiding, you can see their detail */
			if(observation > stealth)
				return(2);
			/* If you're just as good, you can see them, but not the detail */
			if(observation >= stealth)
				cansee = 1;
		}

		return(cansee);
	};
	this.destroy=function()
	{
		var u,r;

		for (r in regions)
			for (u in regions[r].units)
				if (regions[r].units[u].faction.no == this.no)
				{
					if (regions[r].terrain != T_OCEAN)
						regions[r].peasants += regions[r].units[u].number;

					regions[r].units[u].number = 0;
				}

		this.alive = false;
	};
}

function findfaction(no)
{
	var f;

	no=parseInt(no);
	for(f in factions)
		if(factions[f].no==no)
			return(factions[f]);

	return null;
}

function removenullfactions()
{
	var f,f2,f3;
	var rf,rf2;

	for (f=0; f<factions.length; f++)
	{
		if (!factions[f].alive)
		{
			printf ("Removing %s.\n",factions[f].name);

			for (f3 in factions)
				for (rf=0; rf < factions[f3].allies.length; rf++)
				{
					if (factions[f3].allies[rf].no == factions[f].no) {
						factions[f3].allies.splice(rf,1);
						rf--;
					}
				}

			factions.splice(f,1);
			f--;
		}
	}
}

function factionnameisunique(name)
{
	var f;

	for(f in factions)
		if(factions[f].name==name)
			return(false);

	return true;
}

function factionbyaddr(addr)
{
	var f;

	for(f in factions)
		if(factions[f].addr==addr)
			return(factions[f]);

	return null;
}
