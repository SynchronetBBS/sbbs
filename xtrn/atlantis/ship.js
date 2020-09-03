if(js.global.Region==undefined)
	load(script_dir+'region.js');
if(js.global.Unit==undefined)
	load(script_dir+'unit.js');

function Ship()
{
	this.no=0;
	this.name='';
	this.display='';
	this.type=0;
	this.left=0;
	this.region=null;	// TODO: Added
	this.__defineGetter__("id", function() { return(this.name+' ('+this.no+')'); });
	this.__defineGetter__("cansail", function() {
		var n,u;

		n = 0;

		for(u in this.region.units)
			if(this.region.units[u].ship.no==this.no)
				n += this.region.units[u].itemweight + this.region.units[u].horseweight + (this.region.units[u].number * 10);

		return n <= shiptypes[this.type].capacity;
	});
	this.__defineGetter__("owner", function() {
		var u;

		for(u in this.region.units)
			if(this.region.units[u].ship.no==this.no)
				return(this.region.units[u]);

		return(null);
	});

	this.mayboard=function(u) {
		var u2=this.owner;

		if(u2 == undefined)
			return(true);
		return(u2.admits(u));
	};
}

function findship (no,region)
{
	var r,s;

	if(region==undefined) {
		for(r in regions)
			for(s in regions[r].ships)
				if(regions[r].ships[s].no==no)
					return(regions[r].ships[s].no);
	}
	else {
		for(s in region)
			if(region.ships[s].no==no)
				return(region.ships[s].no);
	}

	return(null);
}

