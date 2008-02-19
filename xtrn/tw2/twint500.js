load("../xtrn/tw2/filename.js");
load(fname("gamesettings.js"));
load(fname("sector_map.js"));
load(fname("ports_map.js"));
load("recordfile.js");

var Settings=new GameSettings();

function ConfigureSettings()
{
	var i;
	var last=0;

	for(;;) {
		var list=new Array();
		var str;

		for(i=0; i<GameSettingProperties.length; i++)
			list.push(format("%-35s %s",GameSettingProperties[i].name,Settings[GameSettingProperties[i].prop]));

		i=uifc.list(WIN_MID|WIN_ORG|WIN_ACT|WIN_ESC, 0, 0, 0, last, last, "Configuration", list);
		if(i==-1) {
			if(uifc.changes) {
				var q=uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, 0, 0, "Save Changes?", ["Yes", "No"]);
				if(q!=-1) {
					if(q==0)
						Settings.save();
					break;
				}
			}
			else
				break;
		}
		else {
			last=i;
			switch(GameSettingProperties[i].type) {
				case "String":
					str=uifc.input(WIN_MID, GameSettingProperties[i].name, Settings[GameSettingProperties[i].prop], 0, K_EDIT);
					if(str!=undefined)
						Settings[GameSettingProperties[i].prop]=str;
					break;
				case "Date":
					str=uifc.input(WIN_MID, GameSettingProperties[i].name, Settings[GameSettingProperties[i].prop], 8, K_EDIT);
					if(str!=undefined)
						Settings[GameSettingProperties[i].prop]=str;
					break;
				case "Integer":
					str=uifc.input(WIN_MID, GameSettingProperties[i].name, Settings[GameSettingProperties[i].prop].toString(), 0, K_NUMBER|K_EDIT);
					if(str!=undefined)
						Settings[GameSettingProperties[i].prop]=parseInt(str);
					break;
				case "Boolean":
					Settings[GameSettingProperties[i].prop]=!Settings[GameSettingProperties[i].prop];
					break;
			}
		}
	}
}

if(uifc == undefined) {
	writeln("UIFC interface not available!");
	exit(1);
}

uifc.init("TradeWars 2 Initialization");
ConfigureSettings();

if(uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, 0, 0, "Reset Game?", ["No", "Yes"])!=1)
	exit(1);

load(fname("structs.js"));

file_remove(fname("sectors.dat"));
file_remove(fname("ports.dat"));
file_remove(fname("players.dat"));
file_remove(fname("planets.dat"));
file_remove(fname("twmesg.dat"));
file_remove(fname("twpmesg.dat"));
file_remove(fname("twrmesg.dat"));
file_remove(fname("teams.dat"));
file_remove(fname("twopeng.dat"));
file_remove(fname("cabals.dat"));
var sectors=new RecordFile(fname("sectors.dat"), SectorProperties);
var ports=new RecordFile(fname("ports.dat"), PortProperties);
var players=new RecordFile(fname("players.dat"), PlayerProperties);
var teams=new RecordFile(fname("teams.dat"), TeamProperties);
var planets=new RecordFile(fname("planets.dat"), PlanetProperties);
var twmsg=new File(fname("twmesg.dat"));
var twpmsg=new RecordFile(fname("twpmesg.dat"), PlayerMessageProperties);
var twrmsg=new RecordFile(fname("twrmesg.dat"), RadioMessageProperties);
var cabals=new RecordFile(fname("cabal.dat"), CabalProperties);
var i;

uifc.pop("Creating Players");
player=players.New();
player.UserNumber=0;
player.Put();
for(i=0; i<Settings.MaxPlayers; i++) {
	player=players.New();
	player.UserNumber=0;
	player.Put();
}

uifc.pop("Creating Planets");
planet=planets.New();
planet.Put();
for(i=0; i<Settings.MaxPlanets; i++) {
	planet=planets.New();
	planet.Put();
}

uifc.pop("SysOp Messages");
twmsg.open("w");
twmsg.writeln(system.timestr()+" TW 500 initialized");
twmsg.close();

uifc.pop("Player Messages");
for(i=0; i<100; i++) {
	msg=twpmsg.New();
	msg.Put();
}

uifc.pop("Radio Messages");
for(i=0; i<2; i++) {
	msg=twrmsg.New();
	twrmsg.Read=true;
	msg.Put();
}

uifc.pop("Teams");
var team=teams.New();
team.Put();

uifc.pop("Placing Ports");
/* Place ports */
for(i=0; i<ports_init.length; i++) {
	sector_map[ports_init[i].Position-1].Port=i+1;
	ports_init[i].LastUpdate=system.datestr(time()-15*86400);
}

uifc.pop("Initializing the Cabal");
sector_map[85-1].Fighters=3000;
sector_map[85-1].FightersOwner="The Cabal";
for(i=1; i<=10; i++) {
	var grp=cabals.New();
	if(i==1) {
		grp.Size=3000;
		grp.Sector=85;
		grp.Goal=85;
	}
	grp.Put();
}

uifc.pop("Writing Sectors");
/* Write sectors.dat */
sector=sectors.New();
sector.Put();
for(i=0; i<sector_map.length; i++) {
	sector=sectors.New();
	for(prop in sector_map[i])
		sector[prop]=sector_map[i][prop];
	sector.Put();
}

uifc.pop("Writing Ports");
/* Write ports.ini */
port=ports.New();
port.Put();
for(i=0; i<ports_init.length; i++) {
	port=ports.New();
	for(prop in ports_init[i])
		port[prop]=ports_init[i][prop];
	port.Production=[ports_init[i].OreProduction, ports_init[i].OrgProduction, ports_init[i].EquProduction];
	port.PriceVariance=[ports_init[i].OreDeduction,ports_init[i].OrgDeduction,ports_init[i].EquDeduction];
	port.Put();
}
uifc.pop();
uifc.bail();
