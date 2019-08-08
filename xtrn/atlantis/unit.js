if(js.global.terrains==undefined)
	load(script_dir+'gamedefs.js');
if(js.global.Region==undefined)
	load(script_dir+'region.js');
if(js.global.Faction==undefined)
	load(script_dir+'faction.js');
if(js.global.Building==undefined)
	load(script_dir+'building.js');
if(js.global.Ship==undefined)
	load(script_dir+'ship.js');
if(js.global.Order==undefined)
	load(script_dir+'order.js');

function Unit(region)
{
	var i,n,v,r,u;

	this.no=0;
	this.name='';
	this.display='';
	this.number=0;
	this.money=0;
	this.faction=null;
	this.building=null;
	this.ship=null;
	this.region=null;		// TODO: Added
	this.owner=false;
	this.behind=false;
	this.guard=false;
	this.thisorder=null;
	this.lastorder=new Order();
	this.lastorder.unit=this;
	this.lastorder.command=K_WORK;
	this.combatspell=-1;
	this.skills=new Array(MAXSKILLS);
	for(i in this.skills)
		this.skills[i]=0;
	this.items=new Array(MAXITEMS);
	for(i in this.items)
		this.items[i]=0;
	this.spells=new Array(MAXSPELLS);
	for(i in this.spells)
		this.spells[i]=0;
	this.orders=new Array();
	this.alias=0;
	this.dead=0;
	this.leaning=0;
	this.n=0;
	this.litems=new Array();
	this.side=0;
	this.isnew=false;
	this.__defineGetter__("id", function() { return(this.name+' ('+this.no+')'); });
	this.__defineGetter__("itemweight", function() {
		var i,n;

		n = 0;

		for (i = 0; i < MAXITEMS; i++) {
			switch (i) {
				case I_STONE:
					n += this.items[i] * 50;
					break;

				case I_HORSE:
					break;

				default:
					n += this.items[i];
			}
		}

		return n;
	});
	this.__defineGetter__("horseweight", function () {
		var i,n;

		n = 0;

		for (i = 0; i < MAXITEMS; i++)
			switch (i)
			{
				case I_HORSE:
					n += this.items[i] * 50;
					break;
			}

		return n;
	});
	this.__defineGetter__("canmove", function() {
		return this.itemweight - this.horseweight - (this.number * 5) <= 0;
	});
	this.__defineGetter__("canride", function() {
		return this.itemweight - this.horseweight + (this.number * 10) <= 0;
	});
	this.__defineGetter__("armedmen", function() {
		var n=0;

		if (this.effskill (SK_SWORD))
			n += this.items[I_SWORD];
		if (this.effskill (SK_CROSSBOW))
			n += this.items[I_CROSSBOW];
		if (this.effskill (SK_LONGBOW))
			n += this.items[I_LONGBOW];
		return Math.min(n,this.number);
	});

	this.effskill=function(skill) {
		var n,j,result;

		n = 0;
		if (this.number)
			n = this.skills[skill] / this.number;

		j = 30;
		result = 0;

		while (j <= n)
		{
			n -= j;
			j += 30;
			result++;
		}

		return result;
	};
	this.cancast=function(i) {
		i=parseInt(i);
		if(i<0 || i>=MAXSPELLS)
			return 0;
		if (this.spells[i])
			return this.number;

		if (!this.effskill(SK_MAGIC))
			return 0;

		switch (i)
		{
			case SP_BLACK_WIND:
				return this.items[I_AMULET_OF_DARKNESS];

			case SP_FIREBALL:
				return this.items[I_STAFF_OF_FIRE];

			case SP_HAND_OF_DEATH:
				return this.items[I_AMULET_OF_DEATH];

			case SP_LIGHTNING_BOLT:
				return this.items[I_STAFF_OF_LIGHTNING];

			case SP_TELEPORT:
				return Math.min (this.number,this.items[I_WAND_OF_TELEPORTATION]);
		}

		return 0;
	};
	this.isallied=function(u2) {
		var rf;

		if (u2==null)
			return this.guard;

		if (u2.faction.no == this.faction.no)
			return true;

		for(rf in this.faction.allies)
			if(this.faction.allies[rf].no==u2.faction.no)
				return(true);

		return(false);
	};

	this.accepts=function(u2) {
		var rf;

		if(this.isallied(u2))
			return(true);

		for(rf in this.faction.accept)
			if(this.faction.accept[rf].no==u2.faction.no)
				return(true);

		return false;
	};

	this.admits=function(u2) {
		var rf;

		if(this.isallied(u2))
			return(true);

		for(rf in this.faction.admit)
			if(this.faction.admit[rf].no==u2.faction.no)
				return(true);

		return false;
	};
	this.leave=function()
	{
		var u2,b,sh;

		if (this.building!=null)
		{
			b = u.building;
			u.building = null;

			if (u.owner)
			{
				u.owner = false;

				for (u2 in this.region.units)
					if (region.units[u2].faction.no == this.faction.no && region.units[u2].building.no == b.no)
					{
						region.units[u2].owner = true;
						return;
					}

				for (u2 in this.region.units)
					if (region.units[u2].building.no == b.no)
					{
						region.units[u2].owner = true;
						return;
					}
			}
		}

		if (this.ship!=null)
		{
			sh = this.ship;
			this.ship = null;

			if (this.owner)
			{
				this.owner = false;

				for (u2 in this.region.units)
					if (region.units[u2].faction.no == this.faction.no && region.units[u2].ship.no == sh.no)
					{
						region.units[u2].owner = true;
						return;
					}

				for (u2 in this.region.units)
					if (region.units[u2].ship.no == sh.no)
					{
						region.units[u2].owner = true;
						return;
					}
			}
		}
	};

	this.getnewunit=function(arg)
	{
		var u2;
		var n=parseInt(arg);

		if (isNaN(n) || n == 0)
			return null;

		for (u2 in this.region.units)
			if (this.region.units[u2].faction.no == this.faction.no && this.region.units[u2].alias == n)
				return this.region.units[u2];

		return null;
	};

	this.getunit=function (args)
	{
		var u2;
		var tmp;

		tmp=args.shift();
		if(tmp == "new")
			return(this.getnewunit(args.shift()));
		if(this.region.terrain != T_OCEAN && tmp=="peasants")
			return {getunitpeasants:true};
		if(tmp==0)
			return {getunit0:true};
		for(u2 in this.region.units) {
			if(this.region.units[u2].no==tmp && this.cansee(this.region.units[u2])
					&& !this.region.units[u2].isnew)
				return(this.region.units[u2]);
		}
		return null;
	};

	/* Find an unused unit number... */
	if(region != undefined) {
		for(n=0;; n+=1000) {
			v=new Array();
			for(r in regions)
				for(u in regions[r].units)
					if(regions[r].units[u].no >= n && regions[r].units[u].no < n + 1000)
						v[regions[r].units[u].no]=true;
			for(i=0; i<1000; i++) {
				if(n+i==0)			// Prevent unit zero
					continue;
				if(v[i]==undefined) {
					this.no=n+i;
					this.name="Unit "+this.no;
					region.units.push(this);
					return;
				}
			}
		}
	}
}

function findunit(no,region)
{
	var r,u;

	if(region==undefined) {
		for(r in regions)
			for(u in regions[r].units)
				if(regions[r].units[u].no==no)
					return(regions[r].units[u]);
	}
	else {
		for(u in region.units)
			if(region.units[u].no==no)
				return(region.units[u]);
	}

	return(null);
}

function removeempty()
{
	var i,f,r,sh,sh2,u,u2,u3;

	for (r in regions)
	{
		for (u=0; u < regions[r].units.length; u++)
		{
			if (!regions[r].units[u].number)
			{
				regions[r].units[u].leave();

				for (u3 in regions[r].units)
					if (regions[r].units[u3].faction.no == regions[r].units[u].faction.no)
					{
						regions[r].units[u3].money += regions[r].units[u].money;
						regions[r].units[u].money = 0;
						for (i = 0; i != MAXITEMS; i++)
							regions[r].units[u3].items[i] += regions[r].units[u].items[i];
						break;
					}

				if (regions[r].terrain != T_OCEAN)
					regions[r].money += regions[r].units[u].money;

				regions[r].units.splice(u,1);
				u--;
			}
		}

		if (regions[r].terrain == T_OCEAN)
			for (sh=0; sh < regions[r].ships.length; sh++)
			{
				f=false;
				for (u in regions[r].units)
					if (regions[r].units[u].ship.no == sh.no) {
						f=true;
						break;
					}

				if (f) {
					regions[r].ships.splice(sh,1);
					sh--;
				}
			}
	}
}
