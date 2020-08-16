// $Id: twint500.js,v 1.13 2020/04/17 05:05:00 rswindell Exp $
// #!/synchronet/src/src/sbbs3/gcc.freebsd.exe.stephen.hurd.local/jsexec

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

var reset = false;
if(this.uifc == undefined) {

	while(!js.terminated && (!this.console || !console.aborted)) {
		print("Configuration:");
		print();
		for(var i=0; i< GameSettingProperties.length; i++)
			print(format("%2d: %-35s = %s", i + 1, GameSettingProperties[i].name, Settings[GameSettingProperties[i].prop]));
		print();
		var which = prompt("Which");
		which = parseInt(which, 10);
		if(isNaN(which))
			break;
		if(which < 1 || which > GameSettingProperties.length)
			continue;
		which--;
		if(GameSettingProperties[which].type == "Boolean") {
			if(Settings[GameSettingProperties[which].prop])
				Settings[GameSettingProperties[which].prop] = confirm(GameSettingProperties[which].name);
			else
				Settings[GameSettingProperties[which].prop] = !deny(GameSettingProperties[which].name);
			continue;
		}
		var val = prompt(GameSettingProperties[which].name);
		switch(GameSettingProperties[which].type) {
			case "String":
				Settings[GameSettingProperties[which].prop] = val;
				break;
			case "Date":
				Settings[GameSettingProperties[which].prop] = val;
				break;
			case "Integer":
				Settings[GameSettingProperties[which].prop] = parseInt(val);
				break;
		}
	}
	if(js.terminated || (this.console && console.aborted))
		exit(0);
	
	reset = confirm("Reset Game");
	try {
		db=new JSONClient(Settings.Server,Settings.Port);
		db.connect();
		Settings.save();
	}
	catch (e) {
		alert("WARNING: Unable to connect to server: " + e);
	}
} else {
	uifc.init("TradeWars 2 Initialization", /* ciolibmode: */argv[0]);
	ConfigureSettings();

	if(js.global.db != undefined) {
		if(uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, 0, 0, "Reset Game?", ["No", "Yes"]) == 1)
			reset = true;
	}
}

if(!reset)
	exit(0);

print("Resetting game");
load(fname("ports.js"));
load(fname("planets.js"));
load(fname("teams.js"));
load(fname("sectors.js"));
load(fname("maint.js"));
load(fname("players.js"));
load(fname("messages.js"));
load(fname("computer.js"));
load(fname("input.js"));

ResetAllPlayers();
ResetAllPlanets();
ResetAllMessages();
InitializeTeams();
InitializeSectors();
InitializePorts();
InitializeCabal();
db.write(Settings.DB,'twopeng',[],LOCK_WRITE);
if(this.uifc) {
	uifc.pop();
	uifc.bail();
}
