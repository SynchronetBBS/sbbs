function GameSession(game,join) 
{
	this.game=game;
	this.queue=gamechat.queue;
	this.clearinput=true;
	this.currentplayer;
	this.shots;
	this.menu;
	this.name;
	this.infobar=true; //TOGGLE FOR GAME INFORMATION BAR AT TOP OF CHAT WINDOW (default: false)
	
/*	
	CHAT ENGINE DEPENDENT FUNCTIONS
	
	some of these functions are redundant
	so as to make modification of the 
	method of data transfer between nodes/systems
	simpler
*/
	this.InitGame=function()
	{
		this.game.LoadGame();
		this.game.LoadPlayers();
		this.name="Sea-Battle Table " + this.game.gamenumber;
		if(join)
		{
			this.game.turn=gameplayers.GetFullName(user.alias);
			this.game.StoreGame();
			this.JoinGame();
		}
		else this.currentplayer=this.game.FindCurrentUser();
		if(!this.game.started) this.game.StartGame();
	}
	this.InitChat=function()
	{
		var rows=(this.infobar?16:19);
		var columns=35;
		var posx=45;
		var posy=(this.infobar?6:3);
		var input_line={'x':45,'y':23,columns:35};
		gamechat.Init(this.name,input_line,columns,rows,posx,posy,true);
	}
	this.InitMenu=function()
	{
		this.menu=new Menu(		gamechat.input_line.x,gamechat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Sit"							, 
								"~New Game"						,
								"~Place Ships"					,
								"~Attack"						,
								"~Help"							,
								"Re~draw"						];
		this.menu.add(menu_items);
		this.RefreshMenu();
	}
	this.Main=function()
	{
		this.Redraw();
		if(this.game.started && this.currentplayer)
		{
			if(this.game.turn==this.currentplayer.id) this.Notice("\1c\1hIt's your turn");
		}
		while(1)
		{
			this.Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				if(this.clearinput) 
				{
					gamechat.ClearInputLine();
					this.clearinput=false;
				} 
				switch(k)
				{
					case "/":
						if(!gamechat.buffer.length) 
						{
							this.RefreshMenu();
							this.ListCommands();
							this.Commands();
						}
						else if(!Chat(k,gamechat) && this.HandleExit()) return;
						break;
					case "\x1b":	
						if(this.HandleExit()) return;
						break;
					case "?":
						if(!gamechat.buffer.length) 
						{
							this.ToggleGameInfo();
						}
						else if(!Chat(k,gamechat) && this.HandleExit()) return;
						break;
					default:
						if(!Chat(k,gamechat) && this.HandleExit()) return;
						break;
				}
			}
		}
	}
	this.ClearChatWindow=function()
	{
		gamechat.ClearChatWindow();
	}
	this.HandleExit=function()
	{
		if(this.game.finished)
		{
			if(file_exists(this.game.gamefile.name))
				file_remove(this.game.gamefile.name);
		}
		else if(this.game.timed && this.game.started && this.game.movelist.length)
		{
			this.Alert("\1c\1hForfeit this game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
			this.EndGame("loss");
		}
		return true;
	}
	this.RefreshMenu=function()
	{
		if(!this.currentplayer || this.currentplayer.ready) this.menu.disable(["P"]);
		else if(this.game.started || this.game.finished) this.menu.disable(["P"]);
		else this.menu.enable(["P"]);
		if(this.currentplayer) this.menu.disable(["S"]);
		else if(this.game.players.length==2) this.menu.disable(["S"]);
		else this.menu.enable(["S"]);
		if(!this.game.finished || !this.currentplayer) this.menu.disable(["N"]);
		else this.menu.enable(["N"]);
		if(!this.game.started || this.game.turn!=this.currentplayer.id || this.game.finished) 
		{
			this.menu.disable(["A"]);
		}
		else 
		{
			this.menu.enable(["A"]);
		}
	}
	this.Commands=function()
	{	
		this.menu.displayHorizontal();
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		this.ClearAlert();
		if(k)
		{
			this.ClearChatWindow();
			gamechat.Redraw();
			if(this.menu.items[k] && this.menu.items[k].enabled) 
			{
				switch(k)
				{
					case "S":
						this.JoinGame();
						break;
					case "H":
						this.Help();
						break;
					case "D":
						this.Redraw();
						break;
					case "P":
						this.PlaceShips();
						break;
					case "A":
						this.Attack();
						break;
					case "N":
						this.Rematch();
						break;
					default:
						break;
				}
				this.ClearChatWindow();
				gamechat.Redraw();
			}
			else Log("Invalid or Disabled key pressed: " + k);
		}
	}
	this.Help=function()
	{
	}
	this.ListCommands=function()
	{
		var list=this.menu.getList();
		this.DisplayInfo(list);
	}
	this.ToggleGameInfo=function()
	{
		if(this.infobar)
		{
			this.infobar=false;
			gamechat.Resize(gamechat.x,3,gamechat.columns,19);
		}
		else
		{
			this.infobar=true;
			gamechat.Resize(gamechat.x,6,gamechat.columns,16);
		}
		this.InfoBar();
	}
	this.InfoBar=function()
	{
		console.gotoxy(gamechat.x-1,1);
		console.putmsg("\1k\1h[\1cEsc\1k\1h]\1wQuit \1k\1h[\1c/\1k\1h]\1wMenu \1k\1h[\1c?\1k\1h]\1wInfo \1k\1h[\1c" + ascii(30) + "\1k\1h,\1c" + ascii(31) +"\1k\1h]\1wScroll");
		if(this.infobar)
		{
			var x=gamechat.x;
			var y=3;
			var turn=false;
			if(this.game.started && !this.game.finished) turn=this.game.turn;
			for(player in this.game.players)
			{
				console.gotoxy(x,y);
				console.putmsg(PrintPadded(gameplayers.FormatPlayer(this.game.players[player].id,turn),27));
				console.putmsg("\1nShips\1h: \1n" + this.CountShips(this.game.players[player].id));
				y+=1;
			}
		}
	}
	this.ClearAlert=function()
	{
		gamechat.ClearInputLine();
	}
	this.DisplayInfo=function(array)
	{
		gamechat.DisplayInfo(array);
	}
	this.Notice=function(msg)
	{
		gamechat.AddNotice(msg);
	}
	this.Alert=function(msg)
	{
		gamechat.Alert(msg);
		this.clearinput=true;
	}
	this.Cycle=function()
	{
		gamechat.Cycle();
		if(this.queue.DataWaiting("attack"))
		{
			var data=this.queue.ReceiveData("attack");
			for(attack in data)
			{
				var shot=data[attack];
				this.GetAttack(shot);
			}
			this.InfoBar();
		}
		if(this.queue.DataWaiting("endturn"))
		{
			var trash=this.queue.ReceiveData("endturn");
			this.game.NextTurn();
			if(this.currentplayer)
			{
				if(this.game.turn==this.currentplayer.id) this.Notice("\1c\1hIt's your turn");
			}
			this.InfoBar();
		}
		if(this.queue.DataWaiting("endgame"))
		{
			var data=this.queue.ReceiveData("endgame");
			var str1=data.winner;
			var str2=data.loser + "'s";
			if(this.currentplayer)
			{
				if(loser==this.currentplayer.id) str2="your";
			}
			for(player in this.game.players)
			{
				this.game.players[player].board.hidden=false;
				this.game.players[player].board.DrawBoard();
			}
			this.Notice("\1r\1h" + str1 + " sank all of " + str2 + " ships!");
			this.game.finished=true;
		}
		if(this.queue.DataWaiting("update"))
		{
			var trash=this.queue.ReceiveData("update");
			this.game.LoadGame();
			this.game.LoadPlayers();
			if(!this.currentplayer && this.game.spectate && this.game.started)
			{
				for(player in this.game.players)
				{
					this.game.players[player].board.hidden=false;
					this.game.players[player].board.DrawBoard();
				}
			}
			this.InfoBar();
		}
		if(!this.game.started) 
		{
			var start=this.game.StartGame();
			if(start) this.InfoBar();
		}
	}
	this.Redraw=function()
	{
		console.clear();
		this.game.Redraw();
		this.InfoBar();
		gamechat.Redraw();
		
	}
	this.Send=function(data,ident)
	{
		this.queue.SendData(data,ident);
	}
	this.ClearAlert=function()
	{
		gamechat.ClearInputLine();
	}

	
/*	ATTACK FUNCTIONS	*/
	this.Attack=function()
	{
		var letters="abcdefghij";
		var player=this.currentplayer;
		var opponent=game.FindOpponent();
		if(!this.shots) this.shots=(this.game.multishot?this.CountShips(player.id):1);
		while(this.shots>0)
		{
			this.Cycle();
			this.Alert("\1c\1hAttack which sector? [\1hshots\1n\1c: \1h" + this.shots + "\1n\1c]\1h: ");
			var attack="";
			while(attack.length<2)
			{
				var key=console.inkey(K_NOCRLF|K_NOSPIN);
				console.putmsg(key);
				attack+=key;
			}
			var coords=GetBoardPosition(attack);
			if(coords && !opponent.board.shots[coords.x][coords.y])
			{
				opponent.board.shots[coords.x][coords.y]=true;
				opponent.board.DrawSpace(coords.x,coords.y);
				this.SendAttack(coords,opponent);
				this.game.StoreShot(coords,opponent.id);
				var hit=this.CheckShot(coords,opponent);
				if(this.game.bonusattack) 
				{
					if(!hit) this.shots--;
				}
				else this.shots--;
				var count=this.CountShips(opponent.id);
				if(count==0) this.EndGame(opponent.id);
				this.InfoBar();
			}
		}
		this.shots=false;
		this.game.NextTurn();
		this.game.StoreGame();
		this.game.NotifyPlayer();
		this.InfoBar();
		this.Send("endturn","endturn");
		return true;
	}
	this.SendAttack=function(coords,opponent)
	{
		var attack={'source':this.currentplayer.id,'target':opponent.id,'coords':coords};
		this.Send(attack,"attack");
	}
	this.CheckShot=function(coords,opponent)
	{
		if(opponent.board.grid[coords.x][coords.y])
		{
			var segment=opponent.board.grid[coords.x][coords.y].segment;
			var shipid=opponent.board.grid[coords.x][coords.y].id;
			var ship=opponent.board.ships[shipid];
			ship.TakeHit(segment);
			if(ship.sunk)
			{
				this.Notice("\1r\1hYou sank " + opponent.name + "'s ship!");
			}
			else
			{
				this.Notice("\1r\1hYou hit " + opponent.name + "'s ship!");
			}
			return true;
		}
		else
		{
			this.Notice("\1c\1hYou missed!");
			return false;
		}
	}
	this.GetAttack=function(attack)
	{
		var source=this.game.FindPlayer(attack.source);
		var target=this.game.FindPlayer(attack.target);
		var coords=attack.coords;
		var board=target.board;
		var targetname=target.name + "'s";
		if(this.currentplayer && target.id==this.currentplayer.id) targetname="your";
		if(board.grid[coords.x][coords.y])
		{
			var shipid=board.grid[coords.x][coords.y].id;
			var segment=board.grid[coords.x][coords.y].segment;
			board.ships[shipid].TakeHit(segment);
			if(board.ships[shipid].sunk)
			{
				this.Notice("\1r\1h" + source.name  + " sank " + targetname + " ship!");
			}
			else
			{
				this.Notice("\1r\1h" + source.name  + " hit " + targetname + " ship!");
			}
		}
		else
		{
			this.Notice("\1c\1h" + source.name  + " fired and missed!");
		}
		board.shots[coords.x][coords.y]=true;
		board.DrawSpace(coords.x,coords.y);
	}
	this.CountShips=function(id)
	{
		var player=this.game.FindPlayer(id);
		var board=player.board;
		var sunk=0;
		for(ship in board.ships)
		{
			if(board.ships[ship].sunk) sunk++;
		}
		return(board.ships.length-sunk);
	}
	this.SelectTile=function(ship,placeholder)
	{
		var selected;
		var board=this.currentplayer.board;
		if(placeholder) selected={"x":start.x, "y":start.y};
		else
		{
			selected={"x":0,"y":0};
		}
		board.DrawShip(selected.x,selected.y,ship);
		var interference=this.CheckInterference(selected,ship);
		var ylimit=(ship.orientation=="vertical"?ship.segments.length-1:0);
		var xlimit=(ship.orientation=="horizontal"?ship.segments.length-1:0);
		while(1)
		{
			this.Cycle();
			var key=console.inkey((K_NOECHO,K_NOCRLF,K_UPPER),50);
			if(key)
			{
				this.ClearAlert();
				if(key=="\r")
				{
					if(!interference) return selected;
					else
					{
						this.Alert("\1r\1hInvalid Selection");
					}
				}
				switch(key)
				{
					case KEY_DOWN:
					case "2":
						if(selected.y + ylimit<9)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.y++;
						}
						break;
					case KEY_UP:
					case "8":
						if(selected.y>0)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.y--;
						}
						break;
					case KEY_LEFT:
					case "4":
						if(selected.x>0)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.x--;
						}
						break;
					case KEY_RIGHT:
					case "6":
						if(selected.x + xlimit<9)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.x++;
						}
						break;
					case "7":
						if(selected.x>0 && selected.y>0)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.x--;
							selected.y--;
						}
						break;
					case "9":
						if(selected.x + xlimit<9 && selected.y>0)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.x++;
							selected.y--;
						}
						break;
					case "1":
						if(selected.x>0 && selected.y + ylimit<9)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.x--;
							selected.y++;
						}
						break;
					case "3":
						if(selected.x + xlimit<9 && selected.y + ylimit<9)
						{
							board.UnDrawShip(selected.x,selected.y,ship);
							selected.x++;
							selected.y++;
						}
						break;
					case " ":
						board.UnDrawShip(selected.x,selected.y,ship);
						ship.Toggle();
						ylimit=(ship.orientation=="vertical"?ship.segments.length-1:0);
						xlimit=(ship.orientation=="horizontal"?ship.segments.length-1:0);
						switch(ship.orientation)
						{
							case "horizontal":
								while(selected.x + xlimit>9) selected.x--;
								break;
							case "vertical":
								while(selected.y + ylimit>9) selected.y--;
								break;
						}
						break;
					case "\x1B":
						board.UnDrawShip(selected.x,selected.y,ship);
						return false;
					default:
						continue;
				}
				if(interference)
				{
					for(pos in interference)
					{
						board.DrawSpace(interference[pos].x,interference[pos].y);
					}
				}
				board.DrawShip(selected.x,selected.y,ship);
				interference=this.CheckInterference(selected,ship);
			}
		}
	}

/*	GAMEPLAY FUNCTIONS	*/
	this.CheckInterference=function(position,ship)
	{
		var segments=[];
		var board=this.currentplayer.board;
		var xinc=(ship.orientation=="vertical"?0:1);
		var yinc=(ship.orientation=="vertical"?1:0);

		for(segment=0;segment<ship.segments.length;segment++)
		{
			var posx=(segment*xinc)+position.x;
			var posy=(segment*yinc)+position.y;
			if(board.grid[posx][posy])
			{
				segments.push({'x':posx,'y':posy});
			}
		}

		if(segments.length) return segments;
		return false;
	}
	this.PlaceShips=function()
	{	
		var board=this.currentplayer.board;
		for(id in board.ships)
		{
			this.Cycle();
			var ship=board.ships[id];
			var position=this.SelectTile(ship);
			if(position) board.AddShip(position,id);
			else return false;
		}
		this.currentplayer.ready=true;
		this.game.StoreGame();
		this.game.StoreBoard(this.currentplayer.id);
		this.game.StorePlayer(this.currentplayer.id);
		this.Send("update","update");
	}
	this.NewGame=function()
	{
		//TODO: Add the ability to change game settings (timed? rated?) when restarting/rematching
		this.game.NewGame();
		this.send("update","update");
	}
	this.Forfeit=function()
	{
		this.Alert("\1c\1hAre you sure? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return;
		this.EndGame("loss");
	}
	this.EndGame=function(id)
	{
		var loser=gameplayers.players[id];
		var winner=gameplayers.players[this.currentplayer.id];
		winner.wins++;
		loser.losses++;
		gameplayers.StorePlayer(winner.id);
		gameplayers.StorePlayer(loser.id);
		this.game.End();
		var endgame={'winner':winner.id, 'loser':loser.id};
		this.Send(endgame,"endgame");
		this.InfoBar();
	}
	this.JoinGame=function()
	{
		this.game.AddPlayer();
		this.currentplayer=this.game.FindCurrentUser();
		this.Send("update","update");
		this.InfoBar();
	}

	this.InitGame();
	this.InitChat();
	this.InitMenu();
	this.Main();
}
function GameData(gamefile)
{
	this.graphic=new Graphic(43,24);
	this.graphic.load(gameroot+"blarge.bin");
	this.gamefile;		
	this.gamenumber;
	this.lastupdated;
	this.players=[];
	this.password;
	this.turn;
	this.started;
	this.finished; 
	this.multishot;
	this.bonusattack;
	this.spectate;
	
	this.Init=function()
	{
		if(gamefile)
		{
			this.gamefile=new File(gamefile);
			var fName=file_getname(gamefile);
			this.LoadGame();
		}
		else
		{
			this.SetFileInfo();
			this.NewGame();
		}
	}
	this.End=function()
	{
		for(player in this.players)
		{
			this.players[player].board.hidden=false;
		}
		delete this.turn;
		this.finished=true;
		this.StoreGame();
	}
	this.AddPlayer=function()
	{
		var id=gameplayers.GetFullName(user.alias);
		var player=new Object();
		player.name=user.alias;
		player.id=id;
		player.board=new GameBoard(2,3,false);
		this.players.push(player);
		this.StorePlayer(player.id);
	}
	this.FindPlayer=function(id)
	{
		for(player in this.players)
		{
			if(this.players[player].id==id) return this.players[player];
		}
		return false;
	}
	this.FindOpponent=function()
	{
		var current=this.FindCurrentUser();
		if(!current) return false;
		for(player in this.players)
		{
			if(this.players[player].id != current.id) return this.players[player];
		}
		return false;
	}
	this.FindCurrentUser=function()
	{
		var fullname=gameplayers.GetFullName(user.alias);
		for(player in this.players)
		{
			if(this.players[player].id==fullname) return this.players[player];
		}
		return false;
	}
	this.NextTurn=function()
	{
		for(player in this.players)
		{
			if(this.turn!=this.players[player].id) 
			{
				this.turn=this.players[player].id;
				break;
			}
		}
	}
	this.SetFileInfo=function()
	{
		var gNum=1;
		if(gNum<10) gNum="0"+gNum;
		while(file_exists(gameroot+gNum+".bom"))
		{
			gNum++;
			if(gNum<10) gNum="0"+gNum;
		}
		var fName=gameroot + gNum + ".bom";
		this.gamefile=new File(fName);
		this.gamenumber=parseInt(gNum);
	}
	this.StoreBoard=function(id)
	{
		Log("Storing board data: " + id);
		var gFile=this.gamefile;
		gFile.open("r+",true);
		var player=this.FindPlayer(id);
		var board=player.board;
		for(x in board.grid)
		{
			for(y in board.grid[x])
			{
				var position=GetBoardPosition({'x':x,'y':y});
				var section=id+".board."+position;
				if(board.grid[x][y])
				{
					var contents=board.grid[x][y];
					gFile.iniSetValue(section,"ship_id",contents.id);
					gFile.iniSetValue(section,"segment",contents.segment);
					gFile.iniSetValue(section,"orientation",contents.orientation);
				}
			}
		}
		gFile.close();
	}
	this.StorePlayer=function(id)
	{
		Log("Storing player data: " + id);
		var gFile=this.gamefile;
		gFile.open("r+",true);
		var player=this.FindPlayer(id);
		gFile.iniSetValue("players",id,player.ready);
		gFile.close();
	}
	this.StorePosition=function(id,x,y)
	{
		var gFile=this.gamefile;
		gFile.open("r+",true);
		var player=this.FindPlayer(id);
		var board=player.board;
		var position=GetBoardPosition({'x':x,'y':y});
		var section=id+".board."+position;
		if(board.grid[x][y])
		{
			var contents=board.grid[x][y];
			gFile.iniSetValue(section,"ship_id",contents.id);
			gFile.iniSetValue(section,"segment",contents.segment);
			gFile.iniSetValue(section,"orientation",contents.orientation);
		}
		gFile.close();
	}
	this.StoreShot=function(coords,id)
	{
		this.gamefile.open("r+",true);
		var position=GetBoardPosition({'x':coords.x,'y':coords.y});
		var section=id+".board."+position;
		this.gamefile.iniSetValue(section,"shot",true);
		this.gamefile.close();
	}
	this.StoreGame=function()
	{
		Log("Storing game data");
		var gFile=this.gamefile;
		gFile.open((file_exists(gFile.name) ? 'r+':'w+'),true); 		
		gFile.iniSetValue(null,"gamenumber",this.gamenumber);
		gFile.iniSetValue(null,"turn",this.turn);
		gFile.iniSetValue(null,"finished",this.finished);
		gFile.iniSetValue(null,"started",this.started);
		gFile.iniSetValue(null,"password",this.password);
		gFile.iniSetValue(null,"multishot",this.multishot);
		gFile.iniSetValue(null,"bonusattack",this.bonusattack);
		gFile.iniSetValue(null,"spectate",this.spectate);
		gFile.close();
	}
	this.LoadPlayers=function()
	{
		this.gamefile.open("r",true);
		var starts=[{'x':2,'y':3},{'x':24,'y':3}];
		for(p in this.players)
		{
			var player=this.players[p];
			if(!this.FindCurrentUser())
			{
				var start=starts.pop();
				hidden=this.spectate && this.started?false:true;
			}
			else if(player.id==gameplayers.GetFullName(user.alias))
			{
				start=starts[0];
				hidden=false;
			}
			else
			{
				start=starts[1];
				hidden=true;
			}
			if(!player.board)
			{
				player.board=new GameBoard(start.x,start.y,hidden);
			}
			if(player.ready)
			{
				this.LoadBoard(player.id);
			}
		}
		this.gamefile.close();
	}
	this.LoadPlayer=function(id,ready)
	{
		var player=this.FindPlayer(id);
		if(!player)
		{
			player=new Object();
			player.id=id;
			player.name=gameplayers.GetAlias(id);
			this.players.push(player);
		}
		player.ready=ready;
	}
	this.LoadGame=function()
	{
		//LOAD GAME TABLE - BASIC DATA
		var gFile=this.gamefile;
		this.lastupdated=file_date(gFile.name);
		if(!file_exists(gFile.name)) return false;
		gFile.open("r",true);
		this.gamenumber=parseInt(gFile.iniGetValue(null,"gamenumber"),10);
		this.turn=gFile.iniGetValue(null,"turn");
		this.finished=gFile.iniGetValue(null,"finished");
		this.started=gFile.iniGetValue(null,"started");
		this.password=gFile.iniGetValue(null,"password");
		this.multishot=gFile.iniGetValue(null,"multishot");
		this.bonusattack=gFile.iniGetValue(null,"bonusattack");
		this.spectate=gFile.iniGetValue(null,"spectate");
		var playerlist=this.gamefile.iniGetKeys("players");
		for(player in playerlist)
		{
			var ready=this.gamefile.iniGetValue("players",playerlist[player]);
			this.LoadPlayer(playerlist[player],ready);
		}
		gFile.close();
	}
	this.StartGame=function()
	{
		if(this.players.length<2) return false;
		var start=true;
		for(player in this.players)
		{
			if(!this.players[player].ready) return false;
		}
		this.started=true;
		this.StoreGame();
		this.NotifyPlayer();
		return true;
	}
	this.NotifyPlayer=function()
	{
		var currentuser=this.FindCurrentUser();
		if(!gamechat.FindUser(this.turn) && this.turn!=currentuser.id)
		{
			var player=this.FindPlayer(this.turn);
			var message="\1n\1gIt is your turn in \1c\1hSea\1n\1c-\1hBattle\1n\1g\1g game #\1h" + this.gamenumber + "\r\n\r\n";
			system.put_telegram(player.name, message);
			//TODO: make this handle interbbs games if possible
		}
	}
	this.LoadBoard=function(id)
	{
		var player=	this.FindPlayer(id);
		var board=		player.board;
		var positions=	this.gamefile.iniGetAllObjects("position",id + ".board.");
		for(pos in positions)
		{
			var coords=				GetBoardPosition(positions[pos].position);
			if(positions[pos].shot)		board.shots[coords.x][coords.y]=positions[pos].shot;
			if(positions[pos].ship_id>=0)	
			{
				var id=positions[pos].ship_id;
				var segment=positions[pos].segment;
				var orientation=positions[pos].orientation;
				player.board.grid[coords.x][coords.y]={'id':id,'segment':segment,'orientation':orientation};
				if(segment==0)	player.board.ships[id].orientation=orientation;
				if(positions[pos].shot)
				{
					board.ships[id].segments[segment].hit=positions[pos].shot;
				}
			}
		}
	}
	this.NewGame=function()
	{
		this.started=false;
		this.finished=false;
		this.turn="";
	}
	this.Redraw=function()
	{
		this.graphic.draw();
		for(player in this.players)
		{
			this.players[player].board.DrawBoard();
		}
	}
	this.Init();
}
function BattleShip(name)
{
	this.name=name;
	this.orientation="horizontal";
	this.segments;
	this.sunk;
	
	this.Toggle=function()
	{
		this.orientation=(this.orientation=="horizontal"?"vertical":"horizontal");
	}
	this.CountHits=function()
	{
		var hits=0;
		for(s in this.segments)
		{
			if(this.segments[s].hit) hits++;
		}
		if(hits==this.segments.length) this.sunk=true;
	}
	this.TakeHit=function(segment)
	{
		this.segments[segment].hit=true;
		this.CountHits();
	}
	this.DrawSegment=function(segment)
	{
		this.segments[segment].Draw(this.orientation);
	}
	this.Draw=function()
	{
		for(segment=0;segment<this.segments.length;segment++)
		{
			this.segments[segment].Draw(this.orientation);
		}
	}
	this.UnDraw=function()
	{
		for(segment=0;segment<this.segments.length;segment++)
		{
			this.segments[segment].UnDraw(this.orientation);
		}
	}
	this.Init=function()
	{
		var horizontal;
		var vertical;
		switch(this.name)
		{
			case "carrier":
				horizontal=[["\xDC","\xDB"],["\xDB","\xDC"],["\xDC","\xDC"],["\xDC","\xDC"],["\xDC"]];
				vertical=[["\xDE","\xDB"],["\xDB","\xDE"],["\xDE","\xDE"],["\xDE","\xDE"],["\xDE"]];
				break;
			case "battleship":
				horizontal=[["\xDC","\xDB"],["\xDB","\xDC"],["\xDB","\xDB"],["\xDC"]];
				vertical=[["\xDE","\xDB"],["\xDB","\xDE"],["\xDB","\xDB"],["\xDE"]];
				break;
			case "cruiser":
				horizontal=[["\xDC","\xDB"],["\xDC","\xDB"],["\xDC"]];
				vertical=[["\xDE","\xDB"],["\xDE","\xDB"],["\xDE"]];
				break;
			case "destroyer":
				horizontal=[["\xDC","\xDB"],["\xDC"]];
				vertical=[["\xDE","\xDB"],["\xDE"]];
				break;
			case "submarine":
				horizontal=[["\xDC"]];
				vertical=[["\xDE"]];
				break;
		}
		this.segments=new Array(horizontal.length);
		for(segment=0;segment<this.segments.length;segment++)
		{
			this.segments[segment]=new Segment(horizontal[segment],vertical[segment]);
		}
		delete horizontal;
		delete vertical;
	}
	function Segment(hgraph,vgraph)
	{
		this.hgraph=hgraph;
		this.vgraph=vgraph;
		this.bg=console.ansi(BG_BLACK);
		this.fg="\1n\1h";
		this.hit=false;
		this.Draw=function(orientation)
		{
			var color=(this.hit?"\1r\1h":this.fg);
			var graphic=(orientation=="horizontal"?this.hgraph:this.vgraph);
			for(character in graphic)
			{
				console.print(color + this.bg + graphic[character]);
				if(orientation=="vertical") 
				{
					console.down();
					console.left();
				}
			}
		}
		this.UnDraw=function(orientation)
		{
			var graphic=(orientation=="horizontal"?this.hgraph:this.vgraph);
			for(character in graphic)
			{
				console.print(this.bg + " ");
				if(orientation=="vertical") 
				{
					console.down();
					console.left();
				}
			}
		}
	}
	this.Init();
}
function GameBoard(x,y,hidden)
{
	this.x=x;
	this.y=y;
	this.grid;
	this.shots;
	this.ships;
	this.hidden=hidden;
	
	this.Init=function()
	{
		this.ResetBoard();
		this.LoadShips();
	}
	this.GetCoords=function(x,y)
	{
		var posx=this.x+(x*2);
		var posy=this.y+(y*2);
		console.gotoxy(posx,posy);
	}
	this.LoadShips=function()
	{
		var carrier=new BattleShip("carrier"); 
		var battleship=new BattleShip("battleship");
		var cruiser1=new BattleShip("cruiser");
		var cruiser2=new BattleShip("cruiser");
		var destroyer=new BattleShip("destroyer");
		this.ships=[carrier,
					battleship,
					cruiser1,
					cruiser2,
					destroyer];
	}
	this.ResetBoard=function()
	{
		this.grid=new Array(10);
		this.shots=new Array(10);
		for(x=0;x<this.grid.length;x++) 
		{
			this.grid[x]=new Array(10);
			this.shots[x]=new Array(10);
		}
	}
	this.DrawSpace=function(x,y)
	{
		if(this.grid[x][y])
		{
			if(!this.hidden)
			{
				var id=		this.grid[x][y].id;
				var segment=	this.grid[x][y].segment;
				var ship=		this.ships[id];
				this.GetCoords(x,y);
				ship.DrawSegment(segment);
			}
			else if(this.shots[x][y])
			{
				this.DrawHit(x,y);
			}
		}
		else
		{
			if(this.shots[x][y])
			{
				this.DrawMiss(x,y);
			}
		}
	}
	this.DrawBoard=function()
	{
		for(x=0;x<this.grid.length;x++) 
		{
			for(y=0;y<this.grid[x].length;y++)
			{
				this.DrawSpace(x,y);
			}
		}
	}
	this.DrawShip=function(x,y,ship)
	{
		this.GetCoords(x,y);
		ship.Draw();
	}
	this.DrawMiss=function(x,y)
	{
		this.GetCoords(x,y);
		console.putmsg("\1c\1h\xFE");
	}
	this.DrawHit=function(x,y)
	{
		this.GetCoords(x,y);
		console.putmsg("\1r\1h\xFE");
	}
	this.AddShip=function(position,id)
	{
		var ship=this.ships[id];
		Log("adding ship " + ship.name + " id " + id + " to " + position.x + "," + position.y);
		var xinc=(ship.orientation=="vertical"?0:1);
		var yinc=(ship.orientation=="vertical"?1:0);
		for(segment=0;segment<ship.segments.length;segment++)
		{
			var posx=(segment*xinc)+position.x;
			var posy=(segment*yinc)+position.y;
			this.grid[posx][posy]={'segment':segment,'id':id,'orientation':ship.orientation};
		}
	}
	this.UnDrawShip=function(x,y,ship)
	{
		this.GetCoords(x,y);
		ship.UnDraw();
	}
	this.Init();
}
function GetBoardPosition(position)
{
	var letters="abcdefghij";
	if(typeof position=="string")
	{
		//CONVERT BOARD POSITION IDENTIFIER TO XY COORDINATES
		var x=parseInt(position.charAt(1)==0?9:position.charAt(1)-1);
		var y=parseInt(letters.indexOf(position.charAt(0).toLowerCase()));
		var position={'x':x,'y':y};
		return(position);
	}
	else if(typeof position=="object")
	{
		//CONVERT XY COORDINATES TO STANDARD CHESS BOARD FORMAT
		var x=(position.x==9?0:parseInt(position.x)+1);
		return(letters.charAt(position.y) + x);
	}
}
