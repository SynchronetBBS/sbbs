var startup_path='.';
try { throw barfitty.barf(barf) } catch(e) { startup_path=e.fileName }
startup_path=startup_path.replace(/[\/\\][^\/\\]*$/,'');
startup_path=backslash(startup_path);

var on_at=time();
var i;

var Commodities=[
	{
		 name:"Ore"
		,abbr:"Ore"
		,disp:"Ore......."
		,price:10
	}
	,{
		 name:"Organics"
		,abbr:"Org"
		,disp:"Organics.."
		,price:20
	}
	,{
		 name:"Equipment"
		,abbr:"Equ"
		,disp:"Equipment."
		,price:35
	}
];
var Settings;
var player=null;
var sector=null;
var initialized=null;
var exit_tw2=false;

load("json-client.js");
load(startup_path+"filename.js");
load(fname("gamesettings.js"));
var db;
var LOCK_WRITE=2;
var LOCK_READ=1;

Settings=new GameSettings();
if(db==undefined) {
	alert("ERROR: Configuation invalid");
	exit(1);
}

load(fname("ports.js"));
load(fname("planets.js"));
load(fname("teams.js"));
load(fname("sectors.js"));
load(fname("maint.js"));
load(fname("players.js"));
load(fname("messages.js"));
load(fname("computer.js"));
load(fname("input.js"));
load(fname("editor.js"));

function Menu(sector)
{
	var refresh;
	/* 22000 */
	while(1) {
		refresh=true;
		console.crlf();
		if(player.TurnsLeft==10 || player.TurnsLeft < 6) {
			console.attributes="HM";
			console.writeln("You have " + player.TurnsLeft + " turns left.");
		}
		console.attributes="HC";
		console.print("Command (?=Help)? ");
		var valid=new Array('A','C','D','E','F','G','I','L','M','P','Q','T','Z','?');
		var i;
		for(i=0; i<sector.Warps.length; i++) {
			if(sector.Warps[i]>0)
				valid.push(sector.Warps[i].toString());
		}
		var inp=InputFunc(valid);
		switch(inp) {
			case '':
				console.writeln("? = Help");
				break;
			case 'A':
				/* 25000 */
				console.writeln("<Attack>");
				AttackPlayer();
				break;
			case 'C':
				/* 33640 */
				console.writeln("<Computer>");
				ComputerMenu();
				sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
				DisplaySector(sector,player.Sector,false,'main.ans');
				refresh=false;
				break;
			case 'D':
				console.writeln("<Display>");
				sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
				DisplaySector(sector,player.Sector,false,'main.ans');
				refresh=false;
				continue;
			case 'E':
				if(user.level < 90)
					break;
				console.writeln("<TW Editor>");
				console.print("Do you wish to use the editor? Y/N [N] ");
				if(InputFunc(['Y','N'])=='Y') {
					console.writeln("Running Tradewars ][ Editor...");
					Editor();
				}
				break;
			case 'F':
				/* 24000 */
				console.writeln("<Drop/Take Fighters>");
				DropFighters();
				break;
			case 'G':
				/* 27500 */
				console.writeln("<Gamble>");
				PlayerGamble();
				break;
			case 'I':
				console.writeln("<Info>");
				PlayerInfo(player.Record);
				break;
			case 'L':
				/* 31000 */
				console.writeln("<Land/Create planet>");
				LandOnPlanet();
				break;
			case 'M':
				/* 23000 */
				console.writeln("<Move>");
				if(PlayerMove())
					return;
				break;
			case 'P':
				console.writeln("<Port>");
				DockAtPort();
				break;
			case 'T':
				/* 32799 */
				console.attributes="HW";
				console.writeln("<Team menu>");
				TeamMenu();
				sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
				DisplaySector(sector,player.Sector,false,'main.ans');
				refresh=false;
				break;
			case '?':
				console.attributes="C";
				console.writeln("<Help>");
				console.crlf();
				if(user.settings&USER_ANSI) {
					sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
					DisplaySector(sector,player.Sector,true,'main.ans');
					refresh=false;
				}
				else
					console.printfile(fname("main.asc"));
				break;

			case 'Z':
				console.writeln("<Instructions>");
				Instructions();
				break;
			case 'Q':
				console.attributes="W";
				console.writeln("<Quit>");
				console.attributes="W";
				console.print("Are you sure (Y/N)? ");
				if(InputFunc(['Y','N'])=='Y') {
					exit_tw2=true;
					return;
				}
				break;
			default:
				if(inp.search(/^[0-9]*$/)!=-1) {
					if(MoveTo(parseInt(inp)))
						return;
				}
				break;
		}
		if(refresh)
			sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
	}
}

function do_exit()
{
	if(player != undefined) {
		if(db.status(Settings.DB,'players').lock!=undefined)
			db.unlock(Settings.DB,'players');
		if(db.status(Settings.DB,'players.'+player.Record).lock!=undefined)
			db.unlock(Settings.DB,'players.'+player.Record);
		player.Online=false;
		if(player.Ported || player.Landed) {
			if(db.status(Settings.DB,'sectors').lock!=undefined)
				db.unlock(Settings.DB,'sectors');
			if(db.status(Settings.DB,'sectors.'+player.Sector).lock!=undefined)
				db.unlock(Settings.DB,'sectors.'+player.Sector);
			var sector=db.read(Settings.DB,'sectors.'+player.Sector,LOCK_READ);
			if(player.Ported) {
				console.writeln("Leaving the port...");
				player.Ported=false;
				if(db.status(Settings.DB,'ports').lock!=undefined);
					db.unlock(Settings.DB,'ports');
				if(db.status(Settings.DB,'ports.'+sector.Port).lock!=undefined);
					db.unlock(Settings.DB,'ports.'+sector.Port);
				db.lock(Settings.DB,'ports.'+sector.Port,LOCK_WRITE);
				port=db.read(Settings.DB,'ports.'+sector.Port);
				port.OccupiedBy=0;
				db.write(Settings.DB,'ports.'+sector.Port,port);
				db.unlock(Settings.DB,'ports.'+sector.Port);
			}
			if(player.Landed) {
				console.writeln("Launching from planet...");
				player.Landed=false;
				if(db.status(Settings.DB,'planets').lock!=undefined);
					db.unlock(Settings.DB,'planets');
				if(db.status(Settings.DB,'planets.'+sector.Planet).lock!=undefined);
					db.unlock(Settings.DB,'planets.'+sector.Planet);
				db.lock(Settings.DB,'planets.'+sector.Planet,LOCK_WRITE);
				var planet=db.read(Settings.DB,'planets.'+sector.Planet);
				planet.OccupiedCount--;
				db.write(Settings.DB,'planets.'+sector.Planet,planet);
				db.unlock(Settings.DB,'planets.'+sector.Planet);
			}
		}
		player.TimeUsed += time()-on_at;
		if(player.Put != undefined)
			player.Put();
	}
	console.writeln("Returning to Door monitor...");
	if(initialized != undefined) {
		TWRank();
	}
}

function Instructions()
{
	console.print("Do you want instructions (Y/N) [N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		console.crlf();
		console.printfile(fname("twinstr.doc"), P_CPM_EOF);
	}
}

// NOTE: Caller needs to save now...
function LockedProduction(place)
{
	var newupd=time();
	var diffdays=(newupd-place.LastUpdate)/86400;

	if(diffdays>10)
		diffdays=10;
	for(i=0; i<Commodities.length; i++) {
		if(diffdays > 0) {
			place.Commodities[i] += place.Production[i]*diffdays;
			if(place.Commodities[i] > place.Production[i]*10)
				place.Commodities[i] = place.Production[i]*10;
		}
	}
	place.LastUpdate=newupd;
}

function Production(place)
{
	var newupd=time();
	var diffdays=(newupd-place.LastUpdate)/86400;

	if(diffdays>10)
		diffdays=10;
	for(i=0; i<Commodities.length; i++) {
		if(diffdays > 0) {
			place.Commodities[i] += place.Production[i]*diffdays;
			if(place.Commodities[i] > place.Production[i]*10)
				place.Commodities[i] = place.Production[i]*10;
		}
	}
	place.LastUpdate=newupd;
	place.Put();
}

function ShowOpeng()
{
	var len=db.read(Settings.DB,'twopeng.length',LOCK_READ);
	var i;
	var msg;

	// TODO: Only show "new" stuff from here...
	for(i=0; i<len; i++) {
		msg=db.read(Settings.DB,'twopeng.'+i,LOCK_READ);
		console.writeln(msg.Message);
		console.crlf();
	}
	return len;
}

function main()
{
	var today=strftime("%Y:%m:%d");

try {
	js.on_exit("do_exit()");

	console.attributes="C";
	console.crlf();
	console.crlf();
	console.center("Trade Wars (v.ii)");
	console.center("By Chris Sherrick (PTL)");
	console.center("Copyright 1986 Chris Sherrick");
	console.crlf();
	console.center(system.name);
	console.center("Sysop  "+system.operator);
	console.crlf();
	console.crlf();
	initialized = ShowOpeng();
	
	/* Make sure game has been 'big-banged' */
	if(initialized == undefined) {
		console.attributes="R";
		console.writeln("The game has not been initialized.");
		console.writeln("Please notify the SysOp.");
		exit(0);
	}
	
	/* Run maintenance */
	if(Settings.MaintLastRan < today) {
		RunMaint();
	}
	console.attributes="W";
	console.writeln("Initializing...");
	console.writeln("Searching my records for your name.");
	if(!LoadPlayer())
		exit(0);

	console.pause();
	while(player.KilledBy==0 && exit_tw2==false) {
		if(EnterSector()) {
			if(CheckSector())
				Menu(sector);
		}
	}
}
catch (e) { log(e); log(e.toSource()); throw(e); }
}

main();
