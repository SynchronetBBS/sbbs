load("sbbsdefs.js");				// K_*
load("uifcdefs.js");				// WIN_*
load("../xtrn/tw2/filename.js");

var GameSettingProperties=[
			 {
				 prop:"EditorPassword"
				,name:"Editor Password"
				,type:"String"
				,def:""
			}
			,{
				 prop:"Start"
				,name:"Starting Date"
				,type:"Date"
				,def:system.datestr()
			}
			,{
				 prop:"StartingFighters"
				,name:"Starting Fighters"
				,type:"Integer"
				,def:20
			}
			,{
				 prop:"StartingCredits"
				,name:"Starting Credits"
				,type:"Integer"
				,def:200
			}
			,{
				 prop:"StartingHolds"
				,name:"Starting Holds"
				,type:"Integer"
				,def:20
			}
			,{
				 prop:"DaysInactivity"
				,name:"Days of inactivity before deletion"
				,type:"Integer"
				,def:21
			}
			,{
				 prop:"MaintLastRan"
				,name:"Maintenance Last Ran"
				,type:"Date"
				,def:system.datestr(time()-86400)
			}
			,{
				 prop:"TurnsPerDay"
				,name:"Turns Per Day"
				,type:"Integer"
				,def:30
			}
			,{
				 prop:"CabalRegeneration"
				,name:"Cabal Regeneration (ftrs/day)"
				,type:"Integer"
				,def:200
			}
			,{
				 prop:"MessagesToSysop"
				,name:"Messages to SysOp"
				,type:"Boolean"
				,def:true
			}
			,{
				 prop:"MaxPlayers"
				,name:"Maximum Players"
				,type:"Integer"
				,def:70
			}
			,{
				 prop:"MaxPlanets"
				,name:"Maximum Planets"
				,type:"Integer"
				,def:50
			}
			,{
				 prop:"TopTenFile"
				,name:"Top Ten File Name"
				,type:"String"
				,def:"twtopten.txt"
			}
			,{
				 prop:"AllowAliases"
				,name:"Allow Aliases"
				,type:"Boolean"
				,def:true
			}
			,{
				 prop:"MaxTime"
				,name:"Minutes Allowed Per Session"
				,type:"Integer"
				,def:30
			}
		];

function GameSettings()
{
	var f=new File(fname("game.ini"));
	var i;

	if(file_exists(f.name)) {
		f.open("r+");
		for(i=0; i<GameSettingProperties.length; i++) {
			this[GameSettingProperties[i].prop]=f.iniGetValue(null, GameSettingProperties[i].prop, GameSettingProperties[i].def);
		}
		f.close();
	}
	else {
		for(i=0; i<GameSettingProperties.length; i++) {
			this[GameSettingProperties[i].prop]=GameSettingProperties[i].def;
		}
	}
	this.save=GameSettings_Save;
}

function GameSettings_Save()
{
	var f=new File(fname("game.ini"));

	f.open(file_exists(f.name) ? 'r+':'w+');
	for(i=0; i<GameSettingProperties.length; i++) {
		if(GameSettingProperties[i].type=="Date" || this[GameSettingProperties[i].prop]!=GameSettingProperties[i].def)
			f.iniSetValue(null, GameSettingProperties[i].prop, this[GameSettingProperties[i].prop]);
		else
			f.iniRemoveKey(null, GameSettingProperties[i].prop);
	}
	f.close();
}
