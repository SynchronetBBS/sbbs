#!/synchronet/src/src/sbbs3/gcc.freebsd.exe.stephen.hurd.local/jsexec

var startup_path='.';
try { throw barfitty.barf(barf) } catch(e) { startup_path=e.fileName }
startup_path=startup_path.replace(/[\/\\][^\/\\]*$/,'');
startup_path=backslash(startup_path);

load(startup_path+"filename.js");
load("json-client.js");
var LOCK_WRITE=2;
var LOCK_READ=1;
load(fname("gamesettings.js"));
load(fname("sector_map.js"));
load(fname("ports_map.js"));
var Settings;
try {
	Settings=new GameSettings();
}
catch (e) {
	print(e);
}
var db;

function ConfigureSettings()
{
	var i;
	var last=0;
	var connected=false;
	var old_help;

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
						try {
							db=new JSONClient(Settings.Server,Settings.Port);
							db.connect();
							connected=true;
						}
						catch (e) {
							old_help=uifc.help_text;
							uifc.help_text=e;
							uifc.msg("WARNING: Unable to connect to server (F1 for details)");
							uifc.help_text=old_help;
							connected=false;
						}
						Settings.save();
					break;
				}
			}
			else {
				try {
					db=new JSONClient(Settings.Server,Settings.Port);
					db.connect();
					connected=true;
				}
				catch (e) {
					old_help=uifc.help_text;
					uifc.help_text=e;
					uifc.msg("WARNING: Unable to connect to server (F1 for details)");
					uifc.help_text=old_help;
					connected=false;
				}
				break;
			}
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

if(js.global.db != undefined) {
	if(uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, 0, 0, "Reset Game?", ["No", "Yes"])!=1)
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

var i;

ResetAllPlayers();
ResetAllPlanets();
ResetAllMessages();
InitializeTeams();
InitializeSectors();
InitializePorts();
InitializeCabal();
db.write(Settings.DB,'twopeng',[],LOCK_WRITE);

uifc.pop();
uifc.bail();
