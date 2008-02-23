var startup_path='.';
try { throw new Error() } catch(e) { startup_path=e.fileName }
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

load("recordfile.js");
load("lockfile.js");
load(startup_path+"filename.js");
load(fname("gamesettings.js"));
var Settings=new GameSettings();

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

var twopeng=new File(fname("twopeng.dat"));
twopeng.open("a", true);

var today=system.datestr(system.datestr());

js.on_exit("do_exit()");
js.auto_terminate=false;
/* Run maintenance */
if(Settings.MaintLastRan != system.datestr()) {
	RunMaint();
}

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
console.printfile(fname("twopeng.asc"));
if(file_exists(fname("twopeng.dat")))
	console.printfile(fname("twopeng.dat"));
console.crlf();
console.attributes="W";
console.writeln("Initializing...");
console.writeln("Searching my records for your name.");
var player=null;
if(!LoadPlayer())
	exit(0);

var exit_tw2=false;

while(player.KilledBy==0 && exit_tw2==false) {
	if(EnterSector()) {
		if(CheckSector())
			Menu();
	}
}

function Menu()
{
	/* 22000 */
	while(1) {
		console.crlf();
		console.attributes="HC";
		console.write("Command (?=Help)? ");
		var valid=new Array('A','C','D','E','F','G','I','L','M','P','Q','T','Z','?');
		var sector=sectors.Get(player.Sector);
		var i;
		for(i=0; i<sector.Warps.length; i++) {
			if(sector.Warps[i]>0)
				valid.push(sector.Warps[i].toString());
		}
		var inp=InputFunc(valid)
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
				break;
			case 'D':
				console.writeln("<Display>");
				DisplaySector(player.Sector);
				break;
			case 'E':
				/* TODO: 22800 */
				if(user.level < 90)
					break;
				console.writeln("<TW Editor>");
				console.write("Do you wish to use the editor? Y/N [N] ");
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
				break;
			case '?':
				console.attributes="C";
				console.writeln("<Help>");
				console.crlf();
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
				console.write("Are you sure (Y/N)? ");
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
	}
}

function do_exit()
{
	player.Online=false;
	if(player.Ported || player.Landed) {
		var sector=sectors.Get(player.Sector);
		if(player.Ported) {
			console.writeln("Leaving the port...");
			player.Ported=false;
			var port=ports.Get(sector.Port);
			port.OccupiedBy=0;
			port.Put();
		}
		if(player.Landed) {
			console.writeln("Launching from planet...");
			player.Landed=false;
			var planet=planets.Get(sector.Planet);
			planet.OccupiedBy=0;
			planet.Put();
		}
	}
	player.TimeUsed += time()-on_at;
	player.Put();
	console.writeln("Returning to Door monitor...");
	TWRank();
}

function Instructions()
{
	console.write("Do you want instructions (Y/N) [N]? ");
	if(InputFunc(['Y','N'])=='Y') {
		console.crlf();
		console.printfile(fname("twinstr.doc"));
	}
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
