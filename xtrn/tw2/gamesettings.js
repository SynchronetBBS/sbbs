load("sbbsdefs.js");				// K_*
load("uifcdefs.js");				// WIN_*
load("../xtrn/tw2/filename.js");

var GameSettingProperties=[
			 {	// This index (0) is hard-coded.
				 prop:"Server"
				,name:"Server hostname"
				,type:"String"
				,def:"localhost"
			}
			,{	// This index (1) is hard-coded.
				 prop:"Port"
				,name:"Server port"
				,type:"Integer"
				,def:"10088"
			}
			,{	// This index (2) is hard-coded.
				 prop:"DB"
				,name:"Server scope"
				,type:"String"
				,def:"tw2"
			}
			,{
				 prop:"EditorPassword"
				,name:"Editor Password"
				,type:"String"
				,def:""
			}
			,{
				 prop:"Start"
				,name:"Starting Date"
				,type:"Date"
				,def:strftime("%Y:%m:%d")
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
				,def:strftime("%Y:%m:%d",time()-86400)
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

function GameSettings_Save()
{
	var f=new File(fname("game.ini"));
	var s={};

	f.open(file_exists(f.name) ? 'r+':'w+');
	if(this.Server!=GameSettingProperties[0].def)
		f.iniSetValue(null, 'Server', this.Server);
	else
		f.iniRemoveKey(null, 'Server');
	if(this.Port!=GameSettingProperties[1].def)
		f.iniSetValue(null, 'Port', this.Server);
	else
		f.iniRemoveKey(null, 'Port');
	if(this.Port!=GameSettingProperties[2].def)
		f.iniSetValue(null, 'DB', this.DB);
	else
		f.iniRemoveKey(null, 'DB');

	for(var i=3; i<GameSettingProperties.length; i++) {
		s[GameSettingProperties[i].prop]=this[GameSettingProperties[i].prop];
	}
	db.write(Settings.DB,'settings',s,LOCK_WRITE);
}

function GameSettings()
{
	var f=new File(fname("game.ini"));
	var i;

	this.save=GameSettings_Save;
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
	try {
		db=new JSONClient(this.Server,this.Port);
		var s=db.read(this.DB,'settings',LOCK_READ);
		for(i in s) {
			this[i]=s[i];
		}
	}
	catch(e) {
		db=undefined;
	}
}
