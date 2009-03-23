/*
	JAVASCRIPT MAZE RACE 
	For Synchronet v3.15+
	Matt Johnson(2008)
*/

load("qengine.js");
load("graphic.js");

var root;
try { barfitty.barf(barf); } catch(e) { root = e.fileName; }
root = root.replace(/[^\/\\]*$/,'');

//TODO:	FIND A BETTER WAY TO DO THIS? 
//		KILLFILE IS THE MAP FILENAME, REMOVED BY ONLY THE FIRST PERSON TO RUN THE GAME
//		ALL SUBSEQUENT USERS WHO JOIN THE GAME CANNOT DELETE THE MAP
var killfile;
var owner=false;
js.on_exit("if(owner) file_remove(killfile)");		

var MazeLog;
Maze();

function Maze() 
{
	this.oldpass=console.ctrlkey_passthru;
	this.root=root;
	this.name="maze";
	killfile=(this.root + "maze.bin");
	
	MazeLog=new Logger(this.root,this.name);

	this.startcolors=["\1g","\1m","\1c","\1r","\1w"];
	this.color="\1y";
	
	this.stream=new DataQueue(this.root,this.name,MazeLog);
	this.player=new Player(user.number,this.color,100);
	this.update=true;
	this.winner=false;
	
	//ENABLE/DISABLE DAMAGE FROM WALL IMPACT
	this.damage=true;

	//IF THIS PLAYER IS THE FIRST TO RUN THE GAME, HE "OWNS" THE MAP 
	//AND HIS INSTANCE OF THE GAME WILL BE RESPONSIBLE FOR REMOVING THE MAP FILE
	this.owner=false; 
	
	this.start=false;
	this.finish=false;
	
	this.maze=new Graphic(79,22);
	this.scores=new Object;
	
	this.SplashStart=function()
	{
		console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ_";
		bbs.sys_status|=SS_MOFF;
		bbs.sys_status|=SS_PAUSEOFF;	
		console.clear();
		//TODO: DRAW AN ANSI SPLASH WELCOME SCREEN
	}
	this.SplashExit=function()
	{
		//TODO: DRAW AN ANSI SPLASH EXIT SCREEN
		file_remove(this.root + user.number + ".usr");
		console.ctrlkey_passthru=this.oldpass;
		bbs.sys_status&=~SS_MOFF;
		bbs.sys_status&=~SS_PAUSEOFF;
		Quit();
	}
	this.Main=function()
	{
		this.GenerateMaze();	
		this.LoadMaze();
		this.FindStartFinish();
		this.players=[];
		if(this.RunMaze()) {
			this.FindWinner();
			this.winner=false;
		}
		else return;
	}
	this.GoToStartingPosition=function()
	{
		this.player.coords={x:this.start.x.valueOf(), y:this.start.y.valueOf()};
	}
	this.SaveScores=function()
	{
		var Sorted=this.SortScores();
		this.scores.score_file.open(w);
		this.scores.score_file.writeln(PrintPadded("PLAYER:",22) + "WINS:");
		this.scores.score_file.writeln("");
		for(score in Sorted) {
			pScore=this.scores.score_list[score];
			this.scores.score_file.writeln(PrintPadded(system.username(pScore.user),22) + pScore.score);
		}
	}
	this.LoadScores=function()
	{
		var scores=[];
		this.scores.score_file=new File(this.root + this.name + ".SCO");
		this.scores.score_file.open('r');
		this.scores.score_file.readln();
		this.scores.score_file.readln();
		while(!this.scores.score_file.eof)
		{
			scores.push({'user' : parseInt(this.scores.score_file.readln()), 'score' : parseInt(this.scores.score_file.readln())});
		}
		this.scores.score_list=scores;
		this.scores.score_file.close();
	}
	this.CompressScores=function()
	{
		compressed=[];
		for(score in this.scores.score_list)
		{
			compressed.push(score);
		}
		return compressed;
	}
	this.SortScores=function()
	{ 
		// The Bubble Sort method.
		var data=this.CompressScores();
		var numScores=data.length;
		for(n=0;n<numScores;n++)
		{
			for(m = 0; m < (numScores-1); m++) 
			{
				if(this.scores.score_list[data[m]].score < this.scores.score_list[data[m+1]].score) 
				{
					var holder = data[m+1];
					data[m+1] = data[m];
					data[m] = holder;
				}
			}
		}
		return data;
	}	
	this.LoadMaze=function()
	{
		this.maze.load(this.root + "maze.bin");
		Log("maze loaded");
	}
	this.Redraw=function()
	{
		this.DrawMaze();
		this.ShowPlayerInfo();
		this.player.Draw();
		for(ply in this.players) {
			this.players[ply].Draw();
		}
	}
	this.RunMaze=function()
	{
		this.DrawMaze();
		this.GoToStartingPosition();
		this.ShowPlayerInfo();
		
		while(1) 
		{
			this.Receive();
			if(this.winner==user.number) 
			{
				file_remove(this.root + "maze.bin");
				return true;
			}
			if(this.update) 
			{
				this.Send();
				this.player.Draw();
				if(this.CheckWinner(this.player.coords.x,this.player.coords.y,user.number)) this.winner=user.number;
			}

			var kk=console.inkey();
			switch(kk.toUpperCase())
			{
				case KEY_DOWN:
					if(this.CheckWalls(this.player.coords.x-1,this.player.coords.y)) 
					{
						kk=false; 
						this.ShowPlayerInfo(); 
						break;
					}
					this.player.UnDraw();
					this.player.coords.y++;
					this.update=true;
					break;
				case KEY_UP:
					if(this.CheckWalls(this.player.coords.x-1,this.player.coords.y-2)) 
					{
						kk=false; 
						this.ShowPlayerInfo(); 
						break;
					}
					this.player.UnDraw();
					this.player.coords.y--;
					this.update=true;
					break;
				case KEY_LEFT:
					if(this.CheckWalls(this.player.coords.x-2,this.player.coords.y-1)) 
					{
						kk=false; 
						this.ShowPlayerInfo(); 
						break;
					}
					this.player.UnDraw();
					this.player.coords.x--;
					this.update=true;
					break;
				case KEY_RIGHT:
					if(this.CheckWalls(this.player.coords.x,this.player.coords.y-1))
					{
						kk=false; 
						this.ShowPlayerInfo(); 
						break;
					}
					this.player.UnDraw();
					this.player.coords.x++;
					this.update=true;
					break;
				case "Q":
					return false;
				case "R":
					this.Redraw();
				default:
					break;
			}
		}
	}
	this.CheckWalls=function(posx,posy)
	{
		if(	this.maze.data[posx][posy].ch==" " 	||
			this.maze.data[posx][posy].ch=="S" 	||
			this.maze.data[posx][posy].ch=="X"	) return false;
		else
		{
			if(this.damage) 
			{
				this.player.health-=5;
				this.update=true;
			}
			if(this.player.health==0) 
			{
				this.player.UnDraw();
				this.GoToStartingPosition();
				this.player.health=100;
			}
			return true;
		}
	}
	this.DrawMaze=function()
	{
		console.clear();
		console.down();
		this.maze.draw();
		//console.gotoxy(1,24);
		console.center("\1k\1hUse Arrow Keys to Move. First player to reach '\1yX\1k' wins. - [\1cQ\1k]uit [\1cR\1k]edraw");
	}
	this.ShowPlayerInfo=function()
	{
		console.home();
		console.putmsg(this.player.color + user.alias + "\1k\1h:" + (this.player.health<=25?"\1r":this.player.color) + this.player.health + "\1n" + this.player.color);
		for(p in this.players)
		{
			var ply=this.players[p];
			console.putmsg(" " + ply.color + system.username(ply.user) + "\1k\1h:" + (ply.health<=25?"\1r":ply.color) + ply.health + "\1n" + ply.color);
		}
		console.cleartoeol();
	}
	this.RemoveOldPlayers=function()
	{
		var needs_update=false;
		for(conn in this.players) 
		{
			if(!file_exists(this.root + this.players[conn].user + ".usr")) 
			{
				needs_update=true;
				this.players[conn].UnDraw();
				this.startcolors.push(this.players[conn].color);
				delete this.players[conn];
			}
		}
		if(needs_update) this.ShowPlayerInfo();
	}
	this.Receive=function()
	{
		this.RemoveOldPlayers();
		if(this.stream.DataWaiting("move")) 
		{
			var data=this.stream.ReceiveData("move");
			for(item in data)
			{
				var user_=data[item].user;
				var coords=data[item].coords;
				var health=data[item].health;
				
				if(!this.players[user_])
				{
					this.update=true;
					var color=this.startcolors.pop();
					this.players[user_]=new Player(user_,color,health,coords);
					this.players[user_].Draw();
				}
				else
				{
					this.players[user_].UnDraw();
					this.players[user_].coords=coords;
					this.players[user_].health=health;
					this.players[user_].Draw();
				}
				if(this.CheckWinner(coords.x,coords.y,user_)) this.winner=user_;
			}
			this.ShowPlayerInfo();
		}
	}
	this.Send=function()
	{
		var data=new Object;
		data.user=user.number;
		data.coords=this.player.coords;
		data.health=this.player.health;
		this.stream.SendData(data,"move");
		this.update=false;
	}
	this.FindStartFinish=function()
	{
		this.start=false;
		this.finish=false;
		for(posx=0;posx<this.maze.data.length;posx++) 
		{
			for(posy=0;posy<this.maze.data[posx].length;posy++)
			{
				if(!this.start) if(this.maze.data[posx][posy].ch=="S") 
				{
					this.start={'x' : posx+1, 'y' : posy+1};
					this.maze.data[posx][posy].ch=" ";
				}
				if(!this.finish) if(this.maze.data[posx][posy].ch=="X") 
				{
					this.finish={'x' : posx+1, 'y' : posy+1};
					this.maze.data[posx][posy].attr=14;
				}
				if(this.start && this.finish) 
				{
					Log("start: " + this.start.x + "," + this.start.y +
						" finish: " + this.finish.x + "," + this.finish.y);
					return true;
				}
			}
		}
		Log("unable to find start & finish");
		return false;
	}
	this.FindWinner=function()
	{
		if(this.winner) {
			console.home();
			console.cleartoeol();
			console.putmsg("\1r\1h" + system.username(this.winner) + " has won!\1;\1;");
		}
	}
	this.CheckWinner=function(xx,yy,user)
	{
		if(file_exists(this.root + "winner")) return true;
		if(	xx==this.finish.x &&
			yy==this.finish.y) {
			this.winner=user;
			var winfile=new File(this.root + "winner");
			winfile.open('w');
			winfile.close();
			Log("game over: " + system.username(user) + " wins");
			return true;
		}
		else return false;
	}
	this.GenerateMaze=function()
	{
		Log("generating maze");
		if(!file_exists(this.root + "maze.bin")) 
		{
			owner=true;
			load(this.root + "mazegen.js", this.root + "maze.bin");
		}
	}
	function Log(text)
	{
		MazeLog.Log(text);
	}
	function Quit(ERR)
	{
		if(ERR)
			switch(ERR)
			{
				case 100:
					Log("Error: No root directory specified");
					break;
				default:
					Log("Error: Unknown");
					break;
			}
		exit(0);
	}
	this.SplashStart();
	this.Main();
	this.SplashExit();
}

function Player(user,color,health,coords)
{
	this.user=user;
	this.color=color;
	this.coords=coords;
	this.health=health;
	
	this.Draw=function()
	{
		console.gotoxy(this.coords.x,this.coords.y);
		console.putmsg(this.color + "\1h\xEA");
		console.home();
	}
	this.UnDraw=function()
	{
		console.gotoxy(this.coords.x,this.coords.y);
		write(" ");
	}
}