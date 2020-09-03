var regions=new Array();

if(js.global.terrains==undefined)
	load(script_dir+'gamedefs.js');
if(js.global.makeblock==undefined)
	load(script_dir+'gamedata.js');
if(js.global.Building==undefined)
	load(script_dir+'building.js');
if(js.global.Ship==undefined)
	load(script_dir+'ship.js');
if(js.global.Unit==undefined)
	load(script_dir+'unit.js');
if(js.global.Faction==undefined)
	load(script_dir+'faction.js');

function Region()
{
	this.x=0;
	this.y=0;
	this.name='';
	this.connect=new Array(4);
	this.terrain=0;
	this.peasants=0;
	this.money=0;
	this.buildings=new Array();
	this.ships=new Array();
	this.units=new Array();
	this.immigrants=0;
	this.__defineGetter__("id", function() { return(this.name+' ('+this.x+','+this.y+')'); });

	this.__defineGetter__("iscoast", function()
	{
		var i;

		if(this.terrain == T_OCEAN)
			return(false);

		for(i in this.connect)
			if(this.connect[i]!=null && this.connect[i].terrain==T_OCEAN)
				return(true);
		return(false);
	});

	this.movewhere=function(keyword) {
		var r2;

		switch (keyword)
		{
			case K_NORTH:
				if (this.connect[0]==null)
					makeblock (this.x,this.y - 1);
				return(this.connect[0]);

			case K_SOUTH:
				if (this.connect[1]==null)
					makeblock (this.x,this.y + 1);
				return(this.connect[1]);

			case K_EAST:
				if (this.connect[2]==null)
					makeblock (this.x + 1,this.y);
				return(this.connect[2]);

			case K_WEST:
				if (this.connect[3]==null)
					makeblock (this.x - 1,this.y);
				return(this.connect[3]);
		}

		return null;
	};
	this.reportevent=function(event) {
		var f,u;
		
		for(f in factions) {
			for(u in this.units) {
				if(this.units[u].faction.no==factions[f].no) {
					factions[f].events.push(event);
					break;
				}
			}
		}
	};
}

function findregion (x,y)
{
	var r;

	for(r in regions)
		if(regions[r].x==x && regions[r].y==y)
			return(regions[r]);

	return(null);
}

function connecttothis(r, x, y, from, to)
{
	var r2=findregion(x,y);

	if(r2 != null) {
		r.connect[from]=r2;
		r2.connect[to]=r;
	}
}

function connectregions()
{
	var r;

	for (r in regions)
	{
		if (regions[r].connect[0]==undefined)
			connecttothis (regions[r],regions[r].x,regions[r].y - 1,0,1);
		if (regions[r].connect[1]==undefined)
			connecttothis (regions[r],regions[r].x,regions[r].y + 1,1,0);
		if (regions[r].connect[2]==undefined)
			connecttothis (regions[r],regions[r].x + 1,regions[r].y,2,3);
		if (regions[r].connect[3]==undefined)
			connecttothis (regions[r],regions[r].x - 1,regions[r].y,3,2);
	}
}

function regionnameinuse (s)
{
	var r;

	for(r in regions)
		if(regions[r].name==s)
			return(true);
	return(false);
}
