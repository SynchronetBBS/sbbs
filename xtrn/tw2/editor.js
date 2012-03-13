function EditOneProperty(obj, prop, extrastr)
{
	var flt,tmpstr,i;

	console.write(prop.name+extrastr);
	switch(prop.type) {
		case 'Boolean':
			console.write(' Y/[N]? ');
			if(InputFunc(['Y','N'])=='Y')
				obj[prop.prop]=true;
			else
				obj[prop.prop]=false;
			break;
		case "Float":
			console.write('? ');
			var tmpstr=console.getstr(obj[prop.prop].toString(), 30, K_EDIT);
			flt=parseFloat(tmpstr);
			if((!isNaN(flt)) && isFinite(flt))
				obj[prop.prop]=flt;
			break;
		case "SignedInteger":
			console.write('? ');
			tmpstr=console.getstr(obj[prop.prop].toString(), 11, K_EDIT);
			i=parseInt(tmpstr);
			if((!isNaN(i)) && isFinite(i))
				obj[prop.prop]=i;
			break;
		case "Integer":
			console.write('? ');
			tmpstr=console.getstr(obj[prop.prop].toString(), 10, K_EDIT);
			flt=parseFloat(tmpstr);
			if(flt >= 0 && flt <= 4294967295)
				obj[prop.prop]=Math.floor(flt);
			break;
		case "Date":
			console.write('? ');
			obj[prop.prop]=console.gettemplate("nn/nn/nn",obj[prop.prop],K_EDIT);
			break;
		default:
			var m=prop.type.match(/^String:([0-9]+)$/);
			if(m!=null) {
				console.write('? ');
				obj[prop.prop]=console.getstr(obj[prop.prop],parseInt(m[1]),K_EDIT);
			}
			else {
				m=prop.type.match(/^Array:([0-9]+):(.*)$/);
				var arraylen=parseInt(m[1]);
				i;
				for(i=0; i<arraylen; i++) {
					console.write("\r");
					console.cleartoeol();
					var eobj=new Object();
					eobj.name=prop.name;
					eobj.prop=i;
					eobj.type=m[2];
					EditOneProperty(obj[prop.prop], eobj, extrastr+'['+i+']');
				}
			}
			break;
	}
}

function LockedEditProperties(obj, props)
{
	var i;

	while(1) {
		console.writeln("Record: "+obj.Record);
		for(i=0; i<props.length; i++) {
			console.writeln(
				format("%2d %s%.*s %s"
					,(i+1)
					,props[i].name
					,33-props[i].name.length
					,'.................................'
					,obj[props[i].prop].toString()
				)
			);
		}
		console.crlf();

		console.write("Edit which property (0 to exit): ");
		var p=InputFunc([{max:props.length}]);
		if(p > 0 && p<=props.length) {
			p--;
			EditOneProperty(obj, props[p], '');
		}
		else {
			return;
		}
	}
}

// TODO: Delete this when done...
function EditProperties(obj, props)
{
	var i;

	while(1) {
		console.writeln("Record: "+obj.Record);
		for(i=0; i<props.length; i++) {
			console.writeln(
				format("%2d %s%.*s %s"
					,(i+1)
					,props[i].name
					,33-props[i].name.length
					,'.................................'
					,obj[props[i].prop].toString()
				)
			);
		}
		console.crlf();

		console.write("Edit which property (0 to exit): ");
		var p=InputFunc([{max:props.length}]);
		if(p > 0 && p<=props.length) {
			p--;
			EditOneProperty(obj, props[p], '');
		}
		else {
			obj.Put();
			return;
		}
	}
}

function PlanetEdit()
{
	var i;
	var planetNum=0;
	console.write("Planet name or sector number: ");
	var plid=console.getstr(42);
	var planet=null;
	var planetsLen=db.read(Settings.DB,'planets.length',LOCK_READ);
	var sector=null;

	/* First, search by name */
	for(i=1; i<planetsLen; i++) {
		planet=db.read(Settings.DB,'planets.'+i,LOCK_READ);
		if(planet.Name.toUpperCase().search(plid.toUpperCase())!=-1) {
			console.write(planet.Name+" ["+planet.Sector+"] Y/[N]? ");
			if(InputFunc(['Y','N'])=='Y') {
				planetNum=i;
				break;
			}
		}
		planet=null;
	}
	/* Next, by sector */
	if(planet==null) {
		var secnum=0;
		try {
			secnum=parseInt(plid);
		}
		catch (e) {}
		var seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
		if(secnum > 0 && secnum < seclen) {
			sector=db.read(Settings.DB,'sectors.'+secnum,LOCK_READ);
			if(sector.Planet > 0 && sector.Planet < planets.length) {
				planet=db.read(Settings.DB,'planets.'+sector.Planet,LOCK_READ);
				console.write(planet.Name+" ["+planet.Sector+"] Y/[N]? ");
				if(InputFunc(['Y','N'])!='Y') {
					planet=null;
					sector=null;
				}
				else {
					planetNum=sector.Planet;
				}
			}
		}
	}
	if(planet==null) {
		console.writeln("Cannot find the planet.");
		console.write("Create new planet Y/[N]? ");
		if(InputFunc(['Y','N'])=='Y') {
			planetNum=NextAvailablePlanet();
			if(planetNum==-1)
				console.writeln("No room for additional planets.");
			else {
				planet=db.read(Settings.DB,'planets.'+planetNum,LOCK_READ);
				console.write("Sector? ");
				planet.Sector=InputFunc([{min:0,max:planets.length-1}]);
				if(planet.Sector > 0 && planet.Sector<planets.length) {
					sector=db.read(Settings.DB,'sectors.'+planet.Sector,LOCK_READ);
					sector.Planet=planet.Record;
					console.write("Planet name: ");
					planet.Name=console.getstr(41);
					planet.Created=true;
					if(planet.Name=='') {
						planet=null;
						sector=null;
					}
				}
			}
		}
	}
	if(planet==null)
		return;

	if(sector==null)
		sector=db.read(Settings.DB,'sectors.'+planet.Sector,LOCK_READ);

	var oldsector=planet.Sector;
	LockedEditProperties(planet, PlanetProperties);
	/* Planet moved... */
	if(planet.Sector != oldsector) {
		var newsector=db.read(Settings.DB,'sectors.'+planet.Sector,LOCK_READ);

		if(newsector.Planet > 0) {
			console.writeln("Sector "+newsector.Record+" already has a planet.  Planet returned to sector "+sector.Record+".");
			planet.Sector=oldsector;
		}
		else {
			sector.Planet=0;
			db.write(Settings.DB,'sectors.'+oldsector);
			newsector.Planet=planetNum;
			sector=newsector;
		}
	}
	/* Planet doesn't exist (move to sector zero */
	if(!planet.Created)
		planet.Sector=0;
	db.write(Settings.DB,'planets.'+planetNum,planet,LOCK_WRITE);
	db.write(Settings.DB,'sectors.'+planet.Sector,sector,LOCK_WRITE);
}

function PlayerEdit()
{
	var i;
	var p=null;

	console.write("Player name or sector number: ");
	var name=console.getstr(42).toUpperCase();
	for(i=1; i<players.length; i++) {
		p=players.Get(i);
		if(p.UserNumber==0)
			continue;
		if(p.KilledBy!=0)
			continue;
		if(p.Alias.toUpperCase().indexOf(name)!=-1) {
			console.write(p.Alias+" (Y/N)[Y]? ");
			if(InputFunc(['Y','N'])!='N')
				break;
		}
	}
	if(p==null) {
		console.writeln("Not found.");
		return;
	}

	var oldp=players.Get(p.Record);
	EditProperties(p, PlayerProperties);

	/* Sanity check... */
	if(p.UserNumber==0) {
		DeletePlayer(p);
	}
	if(p.Alias=='') {
		console.writeln("Empty alias not allowed.  Setting back to "+oldp.Alias);
		p.Alias=oldp.Alias;
	}
	if(p.Fighters > 9999) {
		console.writeln("Max fighters is 9999.");
		p.Fighters=9999;
	}
	if(p.Holds > 50) {
		console.writeln("Max holds is 50.");
		p.Holds=50;
	}
	p.Put();
}

function CabalEdit()
{
	console.writeln("Not implemented...");
}

function PortEdit()
{
	var i;
	console.write("Port name or sector number: ");
	var plid=console.getstr(42);
	var port=null;
	var portsLen=db.read(Settings.DB,'ports.length',LOCK_READ);
	var portNum=0;
	var sector=null;

	/* First, search by name */
	for(i=1; i<portsLen; i++) {
		port=db.read(Settings.DB,'ports.'+i,LOCK_READ);
		if(port.Name.toUpperCase().search(plid.toUpperCase())!=-1) {
			console.write(port.Name+" Y/[N]? ");
			if(InputFunc(['Y','N'])=='Y') {
				portNum=i;
				break;
			}
		}
		port=null;
	}
	/* Next, by sector */
	if(port==null) {
		var secnum=0;
		try {
			secnum=parseInt(plid);
		}
		catch (e) {}
		var seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
		if(secnum > 0 && secnum < seclen) {
			sector=db.read(Settings.DB,'sectors.'+secnum,LOCK_READ);
			var plen=db.read(Settings.DB,'ports.length',LOCK_READ);
			if(sector.Port > 0 && sector.Port < plen) {
				port=db.read(Settings.DB,'ports.'+sector.Port,LOCK_READ);
				console.write(port.Name+" ["+secnum+"] Y/[N]? ");
				if(InputFunc(['Y','N'])!='Y') {
					port=null;
					sector=null;
				}
				else {
					portNum=sector.Port;
				}
			}
		}
	}
	if(port==null)
		console.writeln("Cannot find the port.");
	if(port==null)
		return;

	if(sector==null)
		sector=db.read(Settings.DB,'sectors.'+port.Sector,LOCK_READ);

	LockedEditProperties(port, PortProperties);

	db.write(Settings.DB,'ports.'+portNum,port,LOCK_WRITE);
	sector.Put();
}

function SectorEdit()
{
	var seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
	console.write("Sector: ");
	var sec=InputFunc([{max:(seclen-1)}]);

	if(sec>0 && sec <seclen) {
		var sector=db.read(Settings.DB,'sectors.'+sec,LOCK_READ);
		EditProperties(sector, SectorProperties);
		db.write(Settings.DB,'sectors.'+sec,sector,LOCK_WRITE);
	}
}

function TeamEdit()
{
	console.write("Team: ");
	var t=InputFunc([{max:(db.read(Settings.DB,'teams.length',LOCK_READ)-1)}]);

	if(t>0 && t <teams.length) {
		var team=db.read(Settings.DB,'teams.length', LOCK_READ);
		LockedEditProperties(team, TeamProperties);
		db.write(Settings.DB,'teams.length', team, LOCK_WRITE);
	}
}

function MSGEdit()
{
	console.writeln("Not implemented...");
}

function PMSGEdit()
{
	console.writeln("Not implemented...");
}

function RMSGEdit()
{
	console.writeln("Not implemented...");
}

function Editor()
{
	while(1) {
		console.crlf();
		console.writeln("P - Player Editor");
		console.writeln("L - Planet Editor");
		console.writeln("O - Port Editor");
		console.writeln("S - Sector Editor");
		console.writeln("T - Team Editor");
		console.writeln("Q - Quit Editor");
		console.crlf();
		console.write("Editor Command: ");
		switch(InputFunc(['P','L','C','O','S','T','M','R','G','Q'])) {
			case 'P':
				PlayerEdit();
				break;
			case 'L':
				PlanetEdit();
				break;
			case 'C':
				CabalEdit();
				break;
			case 'O':
				PortEdit();
				break;
			case 'S':
				SectorEdit();
				break;
			case 'T':
				TeamEdit();
				break;
			case 'M':
				MSGEdit();
				break;
			case 'R':
				RMSGEdit();
				break;
			case 'G':
				PMSGEdit();
				break;
			case 'Q':
				return;
		}
	}
}
