/*
	-----------------------------------------------------------
	Tank Battle! - Javascript Module for Synchronet 3.14+
	By Matt Johnson - MCMLXXIX - (2008) 
	-----------------------------------------------------------
	The Broken Bubble (MDJ.ATH.CX)
	-----------------------------------------------------------
	Home of this and many other original javascript games
	for Synchronet BBS systems.
	-----------------------------------------------------------
*/

load("graphic.js");
load("sbbsdefs.js");
load("sockdefs.js");

var root;
try { barfitty.barf(barf); } catch(e) { root = e.fileName; } //Deuce is a freak
root = root.replace(/[^\/\\]*$/,'');

load(root+"logging.js");
var game=new TankBattle(root);
var game_log;

function TankBattle(root_dir) 
{
	this.oldpass=console.ctrlkey_passthru;
	this.root=root_dir;
	this.name="tanks";
	this.queueRoot=this.name + "_";
	game_log=new Logger(this.root,this.name);

	this.game=new TankMap();
	
	this.Main=function()
	{
		this.SplashStart();
		this.LoadMaps();
		var map_num=this.LoadPlayers();
		map_num=this.LoadMap(map_num);
		this.game.DrawMap();
		this.LoadPlayer(map_num);
		this.Battle();
		this.SplashExit();
	}
	this.LoadPlayer=function(map_num)
	{
		var playerNumber=this.CountPlayers();
		var start={'x':this.game.tankStart[playerNumber].x,'y':this.game.tankStart[playerNumber].y};
		var color=this.game.tankColors[playerNumber];
		var pFilename=this.root + user.number + ".usr";
		var qName=this.queueRoot+user.number;
		this.game.player=new Player(pFilename,user.number,qName);
		this.game.player.WritePlayerFile(map_num);
		this.game.player.LoadTank(color,start);
		if(this.game.players.length) this.SendData();
	}
	this.SplashStart=function()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status |= SS_PAUSEOFF;		
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
	}
	this.SplashExit=function()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		console.ctrlkey_passthru=this.oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
	}
	this.CountPlayers=function()
	{
		return this.game.players.length;
	}
	this.SendData=function()
	{ 
		var data={'update':this.game.player.update,'init':this.game.player.init};
		for(playerData in this.game.players)
		{
			var player=this.game.players[playerData];
			game_log.Log("Sending data to player: " + system.username(player.user));
			var dQueue=player.queue;
			dQueue.write(data);
		}
		this.game.player.update=false;
	}
	this.ReceiveData=function()
	{
		var pQueue=this.game.player.queue;
		if(pQueue.data_waiting)
		{
			while(pQueue.data_waiting) 
			{
				this.LoadPlayers();
				var data=pQueue.read();
				var pNum=this.FindPlayer(data.init.user);
				var pData=this.game.players[pNum];
				
				game_log.Log("Reading data from player: " + system.username(data.init.user));
                if(pData)
                {
                    if(pData.Exists() && !pData.tank)
                    {
                        game_log.Log("Creating tank data for player: " + system.username(data.init.user));

                        var pColor=data.init.color;
                        var pStart=data.init.start;
                        var pHeading=data.init.heading;
                        var pTurret=data.init.turret;
                        var pCoords=data.init.coords;
                        pData.LoadTank(pColor,pStart,pHeading,pTurret,pCoords);
                        this.ShowScores();
                    }
                    if(pData.Exists() && data.update)
                    {
                        if(data.update.turn) pData.tank.Turn(data.update.turn);
                        if(data.update.move_tank) pData.tank.MoveTank(data.update.move_tank);
                        if(data.update.move_turret) pData.tank.TurnTurret(data.update.move_turret);
                        if(data.update.coords) pData.tank.coords = data.update.coords;
                        if(data.update.shot_fired)
                        {
                            if(this.game.FireShot(pData));
                                this.ShowScores();
                        }
                    }
                }
            }
			return true;
		}
		return false;
	}
	this.FindPlayer=function(userNumber)
	{
		for(player in this.game.players)
		{
			if(this.game.players[player].user==userNumber) return player;
		}
		return false;
	}
	this.LoadPlayers=function()
	{
		var map=false;
		var playerList=directory(this.root+"*.usr");
		for(item in playerList)
		{
			var pFilename=file_getname(playerList[item]);
			var player=parseInt(pFilename.substring(0,pFilename.indexOf(".")));
			if(player!=user.number)
			{
				var pNum=this.FindPlayer(player);
				if(!pNum)
				{
					var pFile=new File(playerList[item]);
					pFile.open('r');
					map=parseInt(pFile.readln());
					pFile.close();
					game_log.Log("loading player " + system.username(player) + " pnum: " + pNum);
					var qName=this.queueRoot+player;
					this.game.players.push(new Player(pFile.name,player,qName));
				}
			}
		}
		for(ply in this.game.players)
		{
			if(this.game.players[ply])
			{
				if(file_exists(this.game.players[ply].playerFile.name));
				else 
				{	
					game_log.Log("removing absent player: " + ply);
					if(this.game.players[ply].tank) this.game.players[ply].tank.Undraw();
					delete this.game.players[ply].queue;
					this.game.players.splice(ply,1);
				}
			}
		}
		return map;
	}
	this.Battle=function()
	{
		game_log.Log("Executing gameplay function");
        var update=true;
		var lastShot=false;

        while(1)
		{
			var shot_fired=false;
			var move_direction=false;
			var turn_direction=false;
			var turret_direction=false;

			if(this.ReceiveData()) this.SendData();
			
			var direction=console.inkey(K_NOECHO|K_NOSPIN);
			switch(direction.toUpperCase())
			{
				case "Q":
					file_remove(this.game.player.playerFile.name);
					return;
				case " ":
					if(lastShot)
					{
						if( (time()-lastShot)>=2) shot_fired=true;
 					}
					else shot_fired=true;
					break;
				case KEY_UP: 
					move_direction="forward";
					break;
				case KEY_DOWN: 
					move_direction="backward";
					break;
				case KEY_LEFT:
					turn_direction="left";
					break;
				case KEY_RIGHT:
					turn_direction="right";
					break;
				case "4":
				case "D":
					turret_direction="left";
					break;
				case "6":
				case "F":
					turret_direction="right";
					break;
			}
			
			if(turret_direction) this.game.player.tank.TurnTurret(turret_direction);
			if(turn_direction) this.game.player.tank.Turn(turn_direction);
			if(move_direction)
			{
				if(this.game.CheckWalls(move_direction))
				{
					//game_log.Log("cannot move - heading: " + this.game.tank.compass[this.game.tank.heading]);
					move_direction=false;
				}
				else this.game.player.tank.MoveTank(move_direction);
			}
			
			if(update || move_direction || turn_direction || shot_fired || turret_direction)
			{
				game_log.Log("sending data updates");
				this.game.player.update={
					'move_tank':move_direction,
					'move_turret':turret_direction,
					'turn':turn_direction,
					'shot_fired':shot_fired
				};
                this.game.player.Init();
                this.SendData();
                update=false;
				mswait(5);
            }
			if(shot_fired) 
			{
				lastShot=time();
				if(this.game.FireShot(this.game.player));
					this.ShowScores();
			}
		}
	}
	this.ShowScores=function()
	{
		console.home();
		console.cleartoeol();
		console.putmsg(this.game.player.tank.color + user.alias + ": " + this.game.player.score);
		for(player in this.game.players)
		{
			var playerData=this.game.players[player];
			console.putmsg("  " + playerData.tank.color + system.username(playerData.user) + ": " + playerData.score);
		}
	}
	this.LoadMaps=function()
	{
		this.game.maps=directory(this.root + "*.map");
		for(map in this.game.maps)
		{
			game_log.Log("map loaded: " + this.game.maps[map]);
		}
	}
	this.LoadMap=function(map)
	{
		var map_num=map;
		if(!map_num) 
		{
			console.putmsg("\1n\1c Battle Maps:\r\n\r\n");
			for(map=1;map<=this.game.maps.length;map++)
			{
				var map_name=file_getname(this.game.maps[map-1]);
				map_name=map_name.slice(0,map_name.indexOf("."));
				console.putmsg("\1n\1c [\1c\1h" + map + "\1n\1c] " + map_name + "\r\n");
			}
			map_num=console.getkeys("Q",this.game.maps.length);
		}
		if(map_num=="Q") exit(0);
		else if(map_num>0 && map_num<=this.game.maps.length)
		{
			this.game.LoadMap(map_num-1);
		}
		return map_num;
	}
	this.Main();
}
function TankMap()
{
	this.maps=[];
	this.players=[];
	this.player;
	this.map;
	this.tankStart=[];
	this.tankColors=["\1w","\1c","\1y","\1r","\1m","\1g"];
	
	this.DrawMap=function()
	{
		this.map.draw();
	}
	this.LoadMap=function(map_num)
	{
		var index=this.maps[map_num];
		if(index) 
		{
			var map_filename=index;
			var map_size=file_size(map_filename);
			map_size/=2;	// Divide by two for attr/char pairs
			map_size/=80;	// Divide by 80 cols.  Size should now be height (assuming int)
			this.map=new Graphic(80,map_size);
			this.map.load(map_filename);
			
			for(x in this.map.data)
			{
				for(y in this.map.data[x])
				{
					if(this.map.data[x][y].ch=="X") 
					{
						game_log.Log("found map start pos x: " + x + " y: " + y);
						this.map.data[x][y].ch=" ";
						this.tankStart.push({'x':parseInt(x)+1,'y':parseInt(y)+1});
					}
				}
			}
			return true;
		}
		else return false;
	}
	this.CheckWalls=function(direction)
	{
		//TO COMPENSATE  FOR THE DIFFERENCE BETWEEN 
		//TRUE SCREEN POSITION AND ARRAY INDICES
		var xoffset=-1;
		var yoffset=-1;
		
		var posx=this.player.tank.coords.x;
		var posy=this.player.tank.coords.y;
		var spots=[];
		var heading=this.player.tank.compass[this.player.tank.heading];		
		game_log.Log("tank position x: " +posx+ " y: " + posy + " h: " + heading);
		
		if(direction=="forward")		{
			//VERTICAL OFFSET
			if(heading==0) yoffset-=3;
			if(heading==180) yoffset+=3;
			
			//HORIZONTAL OFFSET
			if(heading==90) xoffset+=3;
			if(heading==270) xoffset-=3;
		}
		if(direction=="backward")
		{
			//VERTICAL OFFSET
			if(heading==0) yoffset+=3;
			if(heading==180) yoffset-=3;
			
			//HORIZONTAL OFFSET
			if(heading==90) xoffset-=3;
			if(heading==270) xoffset+=3;
		}
		switch(heading)
		{
			case 0:
			case 180:
				spots.push({'x':posx-3,'y':posy+yoffset});
				spots.push({'x':posx-2,'y':posy+yoffset});
				spots.push({'x':posx-1,'y':posy+yoffset});
				spots.push({'x':posx,'y':posy+yoffset});
				spots.push({'x':posx+1,'y':posy+yoffset});
				break;
			case 90:
			case 270:
				spots.push({'x':posx+xoffset,'y':posy-3});
				spots.push({'x':posx+xoffset,'y':posy-2});
				spots.push({'x':posx+xoffset,'y':posy-1});
				spots.push({'x':posx+xoffset,'y':posy});				
				spots.push({'x':posx+xoffset,'y':posy+1});
				break;
			default:
				game_log.Log("error scanning heading"); //THIS SHOULD NEVER OCCUR
		}
		for(spot=0;spot<spots.length;spot++)
		{
			var spotX=spots[spot].x;
			var spotY=spots[spot].y;
			if(this.FindSpace(spotX,spotY)) return true;
		}
		return false;
	}
	this.FireShot=function(tPlayer)
	{
		var player=tPlayer;
		var tank=tPlayer.tank;
		var x=tank.coords.x;
		var y=tank.coords.y;
		var xWalk=0;
		var yWalk=0;
		
		var turret=tank.turretCompass[tank.turret];
		
		switch(turret)
		{
			case 0:
				yWalk=-1;
				break;
			case 45:
				yWalk=-1;
				xWalk=1;
				break
			case 90:
				xWalk=1;
				break;
			case 135:
				xWalk=1;
				yWalk=1;
				break;
			case 180:
				yWalk=1;
				break;
			case 225:
				xWalk=-1;
				yWalk=1;
				break;
			case 270:
				xWalk=-1;
				break;
			case 315:
				xWalk=-1;
				yWalk=-1;
				break;
		}
		
		var xOffset=2*xWalk;
		var yOffset=2*yWalk;
		
		for(distance=0;distance<30;distance++)
		{
			var xCheck=x+xOffset+xWalk;
			var yCheck=y+yOffset+yWalk;
			var wallPiece=this.FindWall(xCheck,yCheck);
			
			if(wallPiece==" ")
			{
				if(this.CheckTankHit(xCheck,yCheck,player)) return true;
			}
			else
			{
				if(	turret==0 	|| 
					turret==90	||
					turret==180 ||
					turret==270	)
				{
						yWalk*=-1;
						xWalk*=-1;
				}
				else if(wallPiece=="\xC5") 
				{
					xWalk*=-1;
					yWalk*=-1;
				}
				else if(wallPiece=="\xC4")
				{
					yWalk*=-1;
				}
				else if(wallPiece=="\xB3")
				{
					xWalk*=-1;
				}
				else
				{
					switch(turret)
					{
						case 45:
							if(wallPiece=="\xC1" ||
								wallPiece=="\xD9") 
								yWalk*=-1;
							else if(wallPiece=="\xC2" || 
									wallPiece=="\xB4" ||
									wallPiece=="\xBF")
							{
								xWalk*=-1;
								yWalk*=-1;
							}
							else if(wallPiece=="\xC3" ||
									wallPiece=="\xDA" ||
									wallPiece=="\xC0")
								xWalk*=-1;
							break;
						case 135:
							if(wallPiece=="\xC2" || 
								wallPiece=="\xDA") 
								yWalk*=-1;
							else if(wallPiece=="\xC1" || 
									wallPiece=="\xB4" ||
									wallPiece=="\xD9")
							{
								xWalk*=-1;
								yWalk*=-1;
							}
							else if(wallPiece=="\xC3" ||
									wallPiece=="\xC0" ||
									wallPiece=="\xBF")
								xWalk*=-1;
							break;
						case 225:
							if(wallPiece=="\xC2" || 
								wallPiece=="\xDA") 
								yWalk*=-1;
							else if(wallPiece=="\xC1" || 
									wallPiece=="\xC3" ||
									wallPiece=="\xC0")
							{
								xWalk*=-1;
								yWalk*=-1;
							}
							else if(wallPiece=="\xB4" ||
									wallPiece=="\xD9" ||
									wallPiece=="\xBF")
								xWalk*=-1;
							break;
						case 315:
							if(wallPiece=="\xC1" || 
								wallPiece=="\xC0)") 
								yWalk*=-1;
							else if(wallPiece=="\xDA" || 
									wallPiece=="\xC3" ||
									wallPiece=="\xC2")
							{
								xWalk*=-1;
								yWalk*=-1;
							}
							else if(wallPiece=="\xB4" ||
									wallPiece=="\xD9" ||
									wallPiece=="\xBF")
								xWalk*=-1;
							break;
					}
				}
			}
			xOffset+=xWalk;
			yOffset+=yWalk;
			
			var spotX=x+xOffset;
			var spotY=y+yOffset;

			console.gotoxy(spotX,spotY);
			console.putmsg("\1h" + player.tank.color + "\xF9");
			mswait(15);
			console.gotoxy(spotX,spotY);
			console.putmsg(" ");
		}
		return false; //USE FOR TANK IMPACT CHECKING
	}
	this.CheckTankHit=function(spotX,spotY,tPlayer)
	{
		//COMPARE EACH POSITION ALONG THE PATH OF THE TANK SHOT
		//WITH EACH POSITION OCCUPIED BY EACH PLAYER'S TANK
		//UNTIL A HIT OCCURS, OR THE BULLET PATH STOPS
	
		var players=[];
		
		//ADD CURRENT PLAYER'S TANK TO THE LIST TEMPORARILY TO MINIMIZE PROCESSING TIME
		players.push(this.player); 
		for(player in this.players)
		{
			if(this.players[player].tank) players.push(this.players[player]);
		}
		for(a=0;a<5;a++)
		{
			for(player in players)
			{
				var pTank=players[player].tank;
				var x=pTank.coords.x-2;
				var y=pTank.coords.y-2;
				
				if(spotX==x+a)
				{
					for(b=0;b<5;b++)
					{
						if(spotY==y+b)
						{
							if(tPlayer.user==players[player].user) players[player].score--;
							else 
							{
								players[player].tank.ResetPosition();
								tPlayer.tank.ResetPosition();
								tPlayer.score++;
							}
							return true;
						}
					}
				}
			}
		}
		return false;
	}
	this.FindWall=function(spotX,spotY)
	{
		return this.map.data[spotX-1][spotY-1].ch;
	}
	this.FindSpace=function(spotX,spotY)
	{
		if(this.map.data[spotX][spotY].ch==" ") return false;
		else return true;
	}
}
function Tank(color,start,heading,turret,coords)
{
	this.color=color;
    this.start=start;

	if(heading) this.heading=heading;
    else this.heading=0;
	if(turret) this.turret=turret;
    else this.turret=0;
    if(coords) this.coords=eval(coords.toSource());
    else this.coords=eval(this.start.toSource());
    this.compass=[0,90,180,270];
	this.turretCompass=[0,45,90,135,180,225,270,315];

    game_log.Log("created new tank: color: " + color + " coords: " + this.coords.x + "," + this.coords.y);

	this.MoveTank=function(direction)
	{	
		this.Undraw();
		switch(direction)
		{
			case "forward":
				if(this.compass[this.heading]==0) this.coords.y--;
				if(this.compass[this.heading]==180) this.coords.y++;
				if(this.compass[this.heading]==90) this.coords.x++;
				if(this.compass[this.heading]==270) this.coords.x--;
				break;
			case "backward":
				if(this.compass[this.heading]==0) this.coords.y++;
				if(this.compass[this.heading]==180) this.coords.y--;
				if(this.compass[this.heading]==90) this.coords.x--;
				if(this.compass[this.heading]==270) this.coords.x++;
				break;
		}
		this.DrawTank();
		this.DrawTurret();
		console.gotoxy(1,1);
	}
	this.Turn=function(direction)
	{
		this.Undraw();
		if(direction=="left") 
		{
			if(this.turret==0) this.turret=this.turretCompass.length-2;
			else if(this.turret==1) this.turret=this.turretCompass.length-1;
			else this.turret-=2;
			if(this.heading==0) this.heading=this.compass.length-1;
			else this.heading--;		
		}
		if(direction=="right") 
		{
			if(this.turret==this.turretCompass.length-2) this.turret=0;
			else if(this.turret==this.turretCompass.length-1) this.turret=1;
			else this.turret+=2;
			if(this.heading==this.compass.length-1) this.heading=0;
			else this.heading++;		
		}
		this.DrawTank();
		this.DrawTurret();
		console.gotoxy(1,1);
	}
	this.TurnTurret=function(direction)
	{
		this.Undraw();
		if(direction=="left") 
		{
			if(this.turret==0) this.turret=this.turretCompass.length-1;
			else this.turret--;
		}
		if(direction=="right") 
		{
			if(this.turret==this.turretCompass.length-1) this.turret=0;
			else this.turret++;
		}
		this.DrawTank();
		this.DrawTurret();
		console.gotoxy(1,1);
	}
	this.Undraw=function()
	{
		var x=this.coords.x;
		var y=this.coords.y;
		switch(this.compass[this.heading])
		{
			case 0:
			case 180:
				console.gotoxy(x-2,y-2); 	print("     ");
				console.gotoxy(x-2,y-1); 	print("     ");
				console.gotoxy(x-2,y);   	print("     ");
				console.gotoxy(x-2,y+1); 	print("     ");
				console.gotoxy(x-2,y+2); 	print("     ");
				break;
			case 90:
			case 270:
				console.gotoxy(x-2,y-2); 	print("     ");
				console.gotoxy(x-2,y-1); 	print("     ");
				console.gotoxy(x-2,y);   	print("     ");
				console.gotoxy(x-2,y+1); 	print("     ");
				console.gotoxy(x-2,y+2); 	print("     ");
				break;
		}
	}
	this.DrawTurret=function()
	{
		var x=this.coords.x;
		var y=this.coords.y;
		console.gotoxy(x,y);
		switch(this.turretCompass[this.turret])
		{
			case 0:
				console.gotoxy(x,y-1);
				console.putmsg("\1h" + this.color + "\xB3");
				break;
			case 180:
				console.gotoxy(x,y+1);
				console.putmsg("\1h" + this.color + "\xB3");
				break;
			case 90:
				console.gotoxy(x+1,y);
				console.putmsg("\1h" + this.color + "\xC4");
				break;
			case 270:
				console.gotoxy(x-1,y);
				console.putmsg("\1h" + this.color + "\xC4");
				break;
			case 45:
				console.gotoxy(x+1,y-1);
				console.putmsg("\1h" + this.color + "/");
				break;
			case 135:
				console.gotoxy(x+1,y+1);
				console.putmsg("\1h" + this.color + "\\");
				break;
			case 225:
				console.gotoxy(x-1,y+1);
				console.putmsg("\1h" + this.color + "/");
				break;
			case 315:
				console.gotoxy(x-1,y-1);
				console.putmsg("\1h" + this.color + "\\");
				break;
		}
	}
	this.DrawTank=function()
	{
		var x=this.coords.x;
		var y=this.coords.y;
		console.gotoxy(x,y);
		switch(this.compass[this.heading])
		{
			case 0:
			case 180:
				console.gotoxy(x-2,y-2); 
				console.putmsg("\1k\1h\xFE" + this.color + "\xDA\xC4\xBF" + "\1k\1h\xFE");
				console.gotoxy(x-2,y-1); 
				console.putmsg("\1k\1h\xFE" + this.color + "\xB3 \xB3" + "\1k\1h\xFE");
				console.gotoxy(x-2,y); 
				console.putmsg("\1k\1h\xFE" + this.color + "\xB3O\xB3" + "\1k\1h\xFE");
				console.gotoxy(x-2,y+1); 
				console.putmsg("\1k\1h\xFE" + this.color + "\xB3 \xB3" + "\1k\1h\xFE");
				console.gotoxy(x-2,y+2); 
				console.putmsg("\1k\1h\xFE" + this.color + "\xC0\xC4\xD9" + "\1k\1h\xFE");
				break;
			case 90:
			case 270:
				console.gotoxy(x-2,y-2); 
				console.putmsg("\1k\1h\xFE\xFE\xFE\xFE\xFE");
				console.gotoxy(x-2,y-1); 
				console.putmsg("\1h" + this.color + "\xDA\xC4\xC4\xC4\xBF");
				console.gotoxy(x-2,y); 
				console.putmsg("\1h" + this.color + "\xB3 O \xB3");
				console.gotoxy(x-2,y+1); 
				console.putmsg("\1h" + this.color + "\xC0\xC4\xC4\xC4\xD9");
				console.gotoxy(x-2,y+2); 
				console.putmsg("\1k\1h\xFE\xFE\xFE\xFE\xFE");
				break;
		}
		this.DrawTurret();
	}
	this.ResetPosition=function()
	{
		this.Undraw();
		this.coords=eval(this.start.toSource());
		game_log.Log("resetting tank position x: " + this.coords.x + " y: " + this.coords.y);
		this.DrawTank();
	}
	this.DrawTank();
}
function Player(playerFileName,userNumber,queueName)
{
	game_log.Log("pfile: " + playerFileName + " unum: " + userNumber);
	this.playerFile=new File(playerFileName);

	this.user=userNumber;
	this.tank=false;
	this.score=0;
	this.update=false;
	this.init=false;
	this.queue=new Queue(queueName);
	this.LoadTank=function(color,start,heading,turret,coords)
	{
		this.tank=new Tank(color,start,heading,turret,coords);
        this.Init();
    }
    this.Init=function()
    {
        this.init={ 'user':this.user,
                    'start':this.tank.start,
                    'color':this.tank.color,
                    'heading':this.tank.heading,
                    'turret':this.tank.turret,
                    'coords':this.tank.coords };
    }
    this.Exists=function()
	{
		if(file_exists(this.playerFile.name)) return true;
		else return false;
	}
	this.WritePlayerFile=function(map_num)
	{
		game_log.Log("storing player file: " + this.playerFile.name);
		this.playerFile.open('w+');
		this.playerFile.writeln(map_num);
		this.playerFile.close();
	}
}
