var startup_path='.';
try { throw new Error() } catch(e) { startup_path=e.fileName }
startup_path=startup_path.replace(/[\/\\][^\/\\]*$/,'');
startup_path=backslash(startup_path);

load(startup_path+"filename.js");
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

file_remove(fname("sectors.dat"));
file_remove(fname("ports.dat"));
file_remove(fname("players.dat"));
file_remove(fname("player-loc.dat"));
file_remove(fname("planets.dat"));
file_remove(fname("twmesg.dat"));
file_remove(fname("twpmesg.dat"));
file_remove(fname("twrmesg.dat"));
file_remove(fname("teams.dat"));
file_remove(fname("twopeng.dat"));
file_remove(fname("cabals.dat"));

load(fname("ports.js"));
load(fname("planets.js"));
load(fname("teams.js"));
load(fname("sectors.js"));
load(fname("maint.js"));
load(fname("players.js"));
load(fname("messages.js"));
load(fname("computer.js"));
load(fname("input.js"));

var twmsg=new File(fname("twmesg.dat"));
var i;

ResetAllPlayers();
ResetAllPlanets();
ResetAllMessages();
InitializeTeams();
InitializeSectors();
InitializePorts();
InitializeCabal();

uifc.pop();
uifc.bail();
