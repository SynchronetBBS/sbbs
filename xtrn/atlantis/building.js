if(js.global.Region==undefined)
	load(script_dir+'region.js');
if(js.global.Unit==undefined)
	load(script_dir+'unit.js');

function Building()
{
	this.no=0;
	this.name='';
	this.display='';
	this.size=0;
	this.sizeleft=0;
	this.region=null;	// TODO: Added
	this.__defineGetter__("id", function() { return(this.name+' ('+this.no+')'); });
	this.__defineGetter__("owner", function() {
		 var u;

		for(u in this.region.units)
			if(this.region.units[u].building.no==this.no)
				return(this.region.units[u]);

		return(null);
	});

	this.mayenter=function(u) {
		var u2=this.owner;

		if(u2 == undefined)
			return(true);
		return(u2.admits(u));
	};
}

function findbuilding (no, region)
{
	var r,b;

	if(region==undefined) {
		for(r in regions)
			for(b in regions[r].buildings)
				if(regions[r].buildings[b].no==no)
					return(regions[r].buildings[b]);
	}
	else {
		for(b in region.buildings)
			if(region.buildings[b].no==no)
				return(region.buildings[b]);
	}

	return(null);
}

