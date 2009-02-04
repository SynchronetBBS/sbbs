var turn=0;
var game_dir=script_dir;

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
if(js.global.Unit==undefined)
	load(script_dir+'unit.js');

function makeblock (x1,y1)
{
	var i,n,x,y,r,r2;
	var newblock=new Array(BLOCKSIZE);
	
	function transmute (from,to,n,count)
	{
		var i,x,y;

		do
		{
			i = 0;

			do
			{

				x = random(BLOCKSIZE);
				y = random(BLOCKSIZE);
				i += count;
			}
			while (i <= 10 &&
					 !(newblock[x][y] == from &&
						((x != 0 && newblock[x - 1][y] == to) ||
						 (x != BLOCKSIZE - 1 && newblock[x + 1][y] == to) ||
						 (y != 0 && newblock[x][y - 1] == to) ||
						 (y != BLOCKSIZE - 1 && newblock[x][y + 1] == to))))
						 ;

			if (i > 10)
				break;

			newblock[x][y] = to;
		}
		while (--n);
	}

	function seed(to,n)
	{
		var x,y;

		do
		{
			x = random(BLOCKSIZE);
			y = random(BLOCKSIZE);
		}
		while (newblock[x][y] != T_PLAIN);

		newblock[x][y] = to;
		transmute (T_PLAIN,to,n,1);
	}

	function blockcoord (x)
	{
		return parseInt((x / (BLOCKSIZE + BLOCKBORDER*2)) * (BLOCKSIZE + BLOCKBORDER*2));
	}


	if (x1 < 0)
		while (x1 != blockcoord (x1))
			x1--;

	if (y1 < 0)
		while (y1 != blockcoord (y1))
			y1--;

	x1 = blockcoord (x1);
	y1 = blockcoord (y1);

	for(i=0; i<newblock.length; i++) {
		newblock[i]=new Array(BLOCKSIZE);
		for(n=0; n<newblock[i].length; n++) {
			newblock[i][n]=T_OCEAN;
		}
	}

	newblock[parseInt(BLOCKSIZE / 2)][parseInt(BLOCKSIZE / 2)] = T_PLAIN;
	transmute (T_OCEAN,T_PLAIN,31,0);
	seed (T_MOUNTAIN,1);
	seed (T_MOUNTAIN,1);
	seed (T_FOREST,1);
	seed (T_FOREST,1);
	seed (T_SWAMP,1);
	seed (T_SWAMP,1);

	for (x = 0; x != BLOCKSIZE + BLOCKBORDER*2; x++)
		for (y = 0; y != BLOCKSIZE + BLOCKBORDER*2; y++)
		{
			r=new Region();

			r.x = x1 + x;
			r.y = y1 + y;

			if (x >= BLOCKBORDER && x < BLOCKBORDER + BLOCKSIZE &&
				 y >= BLOCKBORDER && y < BLOCKBORDER + BLOCKSIZE)
			{
				r.terrain = newblock[x - BLOCKBORDER][y - BLOCKBORDER];

				if (r.terrain != T_OCEAN)
				{
					n = 0;
					for (r2 in regions)
						if (regions[r2].name[0]!='')
							n++;

					i = random(regionnames.length);
					if (n < regionnames.length)
						while(regionnameinuse(regionnames[i]))
							i = random(regionnames.length);

					r.name=regionnames[i];
					r.peasants = terrains[r.terrain].maxfoodoutput / 50;
				}
			}

			regions.push(r);
		}

	connectregions ();
}

function writegame ()
{
	var tmp,tmp2,tmp3,i,f,r,u,b,s,prop,prop2,prop3,arr;
	var gfile=new File(game_dir+"data/"+turn);

	gfile.open("w");

	arr=[];
	/* Write factions */
	for(f in factions) {
		tmp=new Object();
		for(prop in factions[f]) {
			switch(prop) {
				/* Keep unchanged */
				case 'no':
				case 'name':
				case 'password':
				case 'addr':
				case 'lastorders':
				case 'showdata':
					tmp[prop]=factions[f][prop];
					break;
				/* Convert */
				case 'allies':
					/* Convert to just an array of numbers */
					for(i in factions[f][prop])
						tmp.allies[i]=factions[f][prop][i].no;
					break;
			}
		}
		arr.push(tmp);
	}
	gfile.writeln("factions = "+arr.toSource()+';');

	arr=[];	
	/* Write regions */
	for(r in regions) {
		tmp=new Object();
		for(prop in regions[r]) {
			switch(prop) {
				case 'x':
				case 'y':
				case 'name':
				case 'terrain':
				case 'peasants':
				case 'money':
					tmp[prop]=regions[r][prop];
					break;
				case 'buildings':
					tmp.buildings=new Array();
					for(b in regions[r].buildings) {
						tmp2=new Object();
						for(prop2 in regions[r].buildings[b]) {
							switch(prop2) {
								case 'no':
								case 'name':
								case 'display':
								case 'size':
									tmp2[prop2]=regions[r].buildings[b][prop2];
									break;
							}
						}
						tmp.buildings[b]=tmp2;
					}
					break;
				case 'ships':
					tmp.ships=new Array();
					for(s in regions[r].ships) {
						tmp2=new Object();
						for(prop2 in regions[r].ships[s]) {
							switch(prop2) {
								case 'no':
								case 'name':
								case 'display':
								case 'type':
								case 'left':
									tmp2[prop2]=regions[r].ships[s][prop2];
									break;
							}
						}
						tmp.ships[s]=tmp2;
					}
					break;
				case 'units':
					tmp.units=new Array();
					for(u in regions[r].units) {
						tmp2=new Object();
						for(prop2 in regions[r].units[u]) {
							switch(prop2) {
								case 'no':
								case 'name':
								case 'display':
								case 'number':
								case 'money':
								case 'owner':
								case 'behind':
								case 'guard':
								case 'combatspell':
									tmp2[prop2]=regions[r].units[u][prop2];
									break;
								case 'lastorder':
									tmp2[prop2]={};
									for(prop3 in regions[r].units[u].lastorder) {
										switch(prop3) {
											case 'unit':
												break;
											default:
												tmp2[prop2][prop3]=regions[r].units[u].lastorder[prop3];
										}
									}
									break;
								case 'faction':
									tmp2.faction=regions[r].units[u].faction.no;
									break;
								case 'building':
									if(tmp2.building == null)
										tmp2.building=0;
									else
										tmp2.building=regions[r].units[u].building.no;
									break;
								case 'ship':
									if(tmp2.ship == null)
										tmp2.ship=0;
									else
										tmp2.ship=regions[r].units[u].ship.no;
									break;
							}
						}
						tmp.units[u]=tmp2;
					}
					break;
			}
		}
		arr.push(tmp);
	}
	gfile.writeln('regions = '+arr.toSource()+";");
	gfile.close();
}

function readgame ()
{
	factions=[];
	regions=[];
	load(game_dir+"data/"+turn);

	var f,a,i,r,u,b,s,n,prop,prop2;

	/* Fixup factions */
	for(f in factions) {
		for(a in factions[f].allies)
			factions[f].allies[a]=findfaction(factions[f].allies[a]);
		n=new Faction();
		for(prop in factions[f])
			n[prop]=factions[f][prop];
		factions[f]=n;
	}

	/* Read regions */
	for(r in regions) {
		n=new Region();
		for(prop in regions[r])
			n[prop]=regions[r][prop];
		regions[r]=n;

		for(b in regions[r].buildings) {
			n=new Building();
			for(prop in regions[r].buildings[b])
				n[prop]=regions[r].buildings[b][prop];
			regions[r].buildings[b]=n;
			regions[r].buildings[b].region=regions[r];
		}
		for(s in regions[r].ships) {
			n=new Ship();
			for(prop in regions[r].ships[s])
				n[prop]=regions[r].ships[s][prop];
			regions[r].ships[s]=n;
			regions[r].ships[s].region=regions[r];
		}
		for(u in regions[r].units) {
			n=new Unit();
			for(prop in regions[r].units[u])
				n[prop]=regions[r].units[u][prop];
			n.region=regions[r];
			n.faction=findfaction(regions[r].units[u].faction);
			n.building=findbuilding(regions[r].units[u].building);
			n.ship=findship(regions[r].units[u].ship);
			n.lastorder.unit=regions[r].units[u];

			regions[r].units[u]=n;

			if(regions[r].units[u].faction.seendata == undefined)
				regions[r].units[u].faction.seendata=new Array(MAXSPELLS);


			/* Initialize faction seendata values */
			for(i=0; i<MAXSPELLS; i++) {
				if(regions[r].units[u].spells[i])
					regions[r].units[u].faction.seendata[i]=true;
				else
					regions[r].units[u].faction.seendata[i]=false;
			}
			regions[r].units[u].faction.alive=true;
		}
	}

	connectregions ();
}

function gamedate()
{
	var buf;
	var monthnames=
	[
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"October",
		"November",
		"December",
	];

	if (turn == 0)
		return("In the Beginning");
	return(monthnames[(turn-1)%12]+", Year "+(parseInt((turn-1)/12)+1));
}

function gamemap()
{
	var x,y,minx,miny,maxx,maxy;
	var r,buf;
	var ret='';

	minx = Number.MAX_VALUE;
	maxx = Number.MIN_VALUE;
	miny = Number.MAX_VALUE;
	maxy = Number.MIN_VALUE;

	for (r in regions)
	{
		minx = Math.min (minx,regions[r].x);
		maxx = Math.max (maxx,regions[r].x);
		miny = Math.min (miny,regions[r].y);
		maxy = Math.max (maxy,regions[r].y);
	}

	for (y = miny; y <= maxy; y++)
	{
		buf=format("%*s",maxy-miny,'');

		for (r in regions)
			if (regions[r].y == y) {
				buf=buf.substr(0, (regions[r].x-minx))+(".+MFS".charAt(regions[r].terrain))+buf.substr((regions[r].x-minx)+1);
			}

		for (x = 0; x<buf.length; x++)
		{
			ret += ' ';
			ret += buf.charAt(x);
		}

		ret += '\n';
	}
	return(ret);
}

function gamesummary()
{
	var inhabitedregions;
	var peasants;
	var peasantmoney;
	var nunits;
	var playerpop;
	var playermoney;
	var f,r,u;
	var sfile;
	var ret='';

	inhabitedregions = 0;
	peasants = 0;
	peasantmoney = 0;

	nunits = 0;
	playerpop = 0;
	playermoney = 0;

	for (r in regions)
		if (regions[r].peasants || regions[r].units)
		{
			inhabitedregions++;
			peasants += regions[r].peasants;
			peasantmoney += regions[r].money;

			for (u in regions[r].units)
			{
				nunits++;
				playerpop += regions[r].units[u].number;
				playermoney += regions[r].units[u].money;

				regions[r].units[u].faction.nunits++;
				regions[r].units[u].faction.number += regions[r].units[u].number;
				regions[r].units[u].faction.money += regions[r].units[u].money;
			}
		}

	ret += format("Summary file for Atlantis, %s\n\n",gamedate());

	ret += format("Regions:            %d\n",regions.length);
	ret += format("Inhabited Regions:  %d\n\n",inhabitedregions);

	ret += format("Factions:           %d\n",factions.length);
	ret += format("Units:              %d\n\n",nunits);

	ret += format("Player Population:  %d\n",playerpop);
	ret += format("Peasants:           %d\n",peasants);
	ret += format("Total Population:   %d\n\n",playerpop + peasants);

	ret += format("Player Wealth:      $%d\n",playermoney);
	ret += format("Peasant Wealth:     $%d\n",peasantmoney);
	ret += format("Total Wealth:       $%d\n\n",playermoney + peasantmoney);

	ret += gamemap();

	if (factions.length > 0)
		ret += '\n';

	for (f in factions)
		ret += format("%s, units: %d, number: %d, $%d, address: %s\n",factions[f].id,
					factions[f].nunits,factions[f].number,factions[f].money,factions[f].addr);

	return(ret);
}

function getturn()
{
	var lastt=-1;
	var d=directory(game_dir+'data/*');
	var path;
	var tmp;

	if(d.length > 0) {
		for(path in d) {
			tmp=d[path].replace(/^.*[\\\/]/,'');
			if(parseInt(tmp) > lastt)
				lastt=parseInt(tmp);
		}
	}
	return(lastt);
}

function addplayers()
{
	var r,f,u,i,region;
	var newplayer;
	var nf=directory(game_dir+'newplayers/*');
	var minx,miny,maxx,maxy,x,y,done;

	eachplayer:
	for(i in nf) {
		newplayer=new Object();
		load(newplayer, nf[i]);
		if(newplayer.addr==undefined)
			continue;
		for(f in factions)
			if(factions[f].addr==newplayer.addr)
				continue eachplayer;

		if(newplayer.name != undefined)
			if(!factionnameisunique(newplayer.name))
				newplayer.name=undefined;

		/* Find an empty plains region... */
		region=undefined;
		do {
			for(r in regions) {
				if(regions[r].terrain!=T_PLAIN)
					continue;
				if(regions[r].units.length != 0)
					continue;
				if(regions[r].buildings.length != 0)
					continue;
				if(regions[r].ships.length != 0)
					continue;
				region=regions[r];
				break;
			}
			if(region==undefined) {
				/* Make a new continent! */

				/* Search for holes in the existing area first... */
				minx = Number.MAX_VALUE;
				maxx = Number.MIN_VALUE;
				miny = Number.MAX_VALUE;
				maxy = Number.MIN_VALUE;

				for (r in regions)
				{
					minx = Math.min (minx,regions[r].x);
					maxx = Math.max (maxx,regions[r].x);
					miny = Math.min (miny,regions[r].y);
					maxy = Math.max (maxy,regions[r].y);
				}

				for_loop:
				for(x=minx; x<=maxy; x+=BLOCKSIZE+(BLOCKBORDER*2)) {
					for(y=miny; y<=maxy; y+=BLOCKSIZE+(BLOCKBORDER*2)) {
						if(findregion(x,y)==null) {
							makeblock(x,y);
							done=true;
							break for_loop;
						}
					}
				}

				/* If there are no holes, find the longest direction */
				/* And add to middle in direction opposite longest */
				/* If there is no longest direction, add to middle right */
				if(!done) {
					if(maxx-minx < maxy-miny)
						makeblock(maxx+1, parseInt((maxy+miny)/2));
					else
						makeblock(parseInt((maxx+minx)/2), maxy+1);
				}
			}
		} while(region==undefined);
		f=new Faction();
		f.lastorders=turn;
		f.alive=true;
		do {
			f.no++;
			f.name="Faction "+f.no;
		} while(findfaction(f.no)!=null);
		if(newplayer.name != undefined)
			f.name=newplayer.name;
		if(newplayer.password != undefined)
			f.password=newplayer.password;
		if(newplayer.addr != undefined)
			f.addr=newplayer.addr;
		factions.push(f);
		u=new Unit(region);
		u.number=1;
		u.money=STARTMONEY;
		u.faction=f;
		u.isnew=true;
		u.region=region;
		file_remove(nf[i]);
	}
}

function initgame(dir)
{
	game_dir=backslash(script_dir+dir);
	var d=directory(game_dir+'data/*');
	var path;

	if(d.length > 0) {
		turn=getturn();
		readgame();
		writegame();
	}
	else {
		log("No data files found, creating game...");
		mkdir (game_dir+"data");
		mkdir (game_dir+"newplayers");
		makeblock (0,0);
	}
}

