function GameSession(game,join) 
{
	var clearinput=true;
	var currentplayer;
	var shots;
	var menu;
	var name;
	var infobar=true; //TOGGLE FOR GAME INFORMATION BAR AT TOP OF CHAT WINDOW (default: false)
	
/*	
	CHAT ENGINE DEPENDENT FUNCTIONS
	
	some of these functions are redundant
	so as to make modification of the 
	method of data transfer between nodes/systems
	simpler
*/
	function InitGame()
	{
		game.LoadGame();
		game.LoadPlayers();
		name="Sea-Battle table " + game.gamenumber;
		if(join)
		{
			game.turn=gameplayers.GetFullName(user.alias);
			game.StoreGame();
			if(game.singleplayer) ComputerInit();
			JoinGame();
		}
		else currentplayer=game.FindCurrentUser();
		if(!game.started) game.StartGame();
	}
	function InitChat()
	{
		var rows=(infobar?16:19);
		var columns=35;
		var posx=45;
		var posy=(infobar?6:3);
		var input_line={'x':45,'y':23,columns:35};
		gamechat.Init(name,input_line,columns,rows,posx,posy,true);
	}
	function InitMenu()
	{
		menu=new Menu(		gamechat.input_line.x,gamechat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Sit"							, 
								"~New Game"						,
								"~Place Ships"					,
								"~Attack"						,
								"~Forfeit"						,
								"~Help"							,
								"Game ~Info"					,
								"Re~draw"						];
		menu.add(menu_items);
		RefreshMenu();
	}
	function Main()
	{
		Redraw();
		UpdateStatus();
		while(1)
		{
			Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				if(clearinput) 
				{
					gamechat.ClearInputLine();
					clearinput=false;
				} 
				switch(k)
				{
					case KEY_UP:
					case KEY_DOWN:
					case KEY_LEFT:
					case KEY_RIGHT:
						if(game.turn==currentplayer.id && !game.finished)
							Attack();
						else if(!Chat(k,gamechat) && HandleExit()) return true;
						break;
					case "/":
						if(!gamechat.buffer.length) 
						{
							RefreshMenu();
							ListCommands();
							Commands();
						}
						else if(!Chat(k,gamechat) && HandleExit()) return true;
						break;
					case "\x1b":	
						if(HandleExit()) return true;
						break;
					case "?":
						if(!gamechat.buffer.length) 
						{
							ToggleGameInfo();
						}
						else if(!Chat(k,gamechat) && HandleExit()) return true;
						break;
					default:
						if(!Chat(k,gamechat) && HandleExit()) return true;
						break;
				}
			}
		}
	}
	function UpdateStatus()
	{
		var status=["\1n\1cGAME STATUS\1h:"];
		if(game.finished)
		{
			status.push("\1n\1c Game completed");
			status.push("\1n\1c Winner\1h: " + gameplayers.GetAlias(game.winner));
			
		}
		else if(game.started)
		{
			status.push("\1n\1c Game in progress");
			if(currentplayer && game.turn==currentplayer.id) status.push("\1n\1c It's your turn");
		}
		else
		{
			if(game.players.length <2)
			{
				status.push("\1n\1c Waiting for more players");
			}
			for(p in game.players)
			{
				var player=game.players[p];
				if(!player.ready)
				{
					if(currentplayer && currentplayer.id==player.id)
					{
						status.push("\1n\1c You must place your ships");
					}
					else
					{
						status.push("\1n\1c " + player.name + " is not ready");
					}
				}
				else
				{
					if(currentplayer && currentplayer.id==player.id)					
					{
						status.push("\1n\1c You are ready");
					}
					else
					{
						status.push("\1n\1c " + player.name + " is ready");
					}
				}
			}
		}
		DisplayInfo(status);
		Alert("\1r\1h[Press any key]");
		while(console.inkey()==="");
		gamechat.Redraw();
	}
	function ClearChatWindow()
	{
		gamechat.ClearChatWindow();
	}
	function HandleExit()
	{
		if(game.finished && currentplayer)
		{
			if(currentplayer.id!=game.winner || game.singleplayer)	
			{
				if(file_exists(game.gamefile.name))
					file_remove(game.gamefile.name);
				if(file_exists(gameroot + name + ".his"))
					file_remove(gameroot + name + ".his");
			}
		}
		return true;
	}
	function RefreshMenu()
	{
		if(!currentplayer || currentplayer.ready) menu.disable(["P"]);
		else if(game.started || game.finished) menu.disable(["P"]);
		else menu.enable(["P"]);
		if(currentplayer) menu.disable(["S"]);
		else if(game.players.length==2) menu.disable(["S"]);
		else menu.enable(["S"]);
		if(!game.finished || !currentplayer) menu.disable(["N"]);
		else menu.enable(["N"]);
		if(!game.started || game.turn!=currentplayer.id || game.finished) 
		{
			menu.disable(["A"]);
			menu.disable(["F"]);
		}
		else 
		{
			menu.enable(["A"]);
			menu.enable(["F"]);
		}
	}
	function Commands()
	{	
		menu.displayHorizontal();
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		ClearAlert();
		if(k)
		{
			ClearChatWindow();
			gamechat.Redraw();
			if(menu.items[k] && menu.items[k].enabled) 
			{
				switch(k)
				{
					case "S":
						JoinGame();
						break;
					case "H":
						gamehelp.help("gameplay");
						break;
					case "D":
						Redraw();
						break;
					case "F":
						Forfeit();
						break;
					case "I":
						UpdateStatus();
						break;
					case "P":
						PlaceShips();
						break;
					case "A":
						Attack();
						break;
					case "N":
						Rematch();
						break;
					default:
						break;
				}
				ClearChatWindow();
				gamechat.Redraw();
			}
			else Log("Invalid or Disabled key pressed: " + k);
		}
	}
	function ListCommands()
	{
		var list=menu.getList();
		DisplayInfo(list);
	}
	function ToggleGameInfo()
	{
		if(infobar)
		{
			infobar=false;
			gamechat.Resize(gamechat.x,3,gamechat.columns,19);
		}
		else
		{
			infobar=true;
			gamechat.Resize(gamechat.x,6,gamechat.columns,16);
		}
		InfoBar();
	}
	function InfoBar()
	{
		console.gotoxy(gamechat.x-1,1);
		console.putmsg("\1k\1h[\1cEsc\1k\1h]\1wQuit \1k\1h[\1c/\1k\1h]\1wMenu \1k\1h[\1c?\1k\1h]\1wInfo \1k\1h[\1c" + ascii(30) + "\1k\1h,\1c" + ascii(31) +"\1k\1h]\1wScroll");
		if(infobar)
		{
			var x=gamechat.x;
			var y=3;
			var turn=false;
			if(game.started && !game.finished) turn=game.turn;
			for(player in game.players)
			{
				console.gotoxy(x,y);
				console.putmsg(PrintPadded(gameplayers.FormatPlayer(game.players[player].id,turn),27));
				console.putmsg("\1nShips\1h: \1n" + CountShips(game.players[player].id));
				y+=1;
			}
		}
	}
	function ClearAlert()
	{
		gamechat.ClearInputLine();
	}
	function DisplayInfo(array)
	{
		gamechat.DisplayInfo(array);
	}
	function Notice(msg)
	{
		gamechat.AddNotice(msg);
	}
	function Alert(msg)
	{
		gamechat.Alert(msg);
		clearinput=true;
	}
	function Cycle()
	{
		gamechat.Cycle();
		if(queue.DataWaiting("attack"))
		{
			var data=queue.ReceiveData("attack");
			for(attack in data)
			{
				var shot=data[attack];
				GetAttack(shot);
			}
			InfoBar();
		}
		if(queue.DataWaiting("endturn"))
		{
			var trash=queue.ReceiveData("endturn");
			game.NextTurn();
			if(currentplayer)
			{
				if(game.turn==currentplayer.id) Notice("\1c\1hIt's your turn");
			}
			InfoBar();
		}
		if(queue.DataWaiting("endgame"))
		{
			var data=queue.ReceiveData("endgame");
			for(var d in data)
			{
				var str1=data[d].winner;
				var str2=data[d].loser + "'s";
				if(currentplayer)
				{
					if(data[d].loser==currentplayer.id) str2="your";
				}
				for(player in game.players)
				{
					game.players[player].board.hidden=false;
					game.players[player].board.DrawBoard();
				}
				Notice("\1r\1h" + str1 + " sank all of " + str2 + " ships!");
			}
			game.finished=true;
		}
		if(queue.DataWaiting("update"))
		{
			var trash=queue.ReceiveData("update");
			game.LoadGame();
			game.LoadPlayers();
			if(!currentplayer && game.spectate && game.started)
			{
				for(player in game.players)
				{
					game.players[player].board.hidden=false;
					game.players[player].board.DrawBoard();
				}
			}
			InfoBar();
		}
		if(!game.started) 
		{
			var start=game.StartGame();
			if(start) InfoBar();
		}
	}
	function Redraw()
	{
		console.clear();
		game.Redraw();
		InfoBar();
		gamechat.Redraw();
	}
	function Send(data,ident)
	{
		queue.SendData(data,ident);
	}
	function ClearAlert()
	{
		gamechat.ClearInputLine();
	}

/*	ATTACK FUNCTIONS	*/
	function Attack()
	{
		var player=currentplayer;
		var opponent=game.FindOpponent();
		if(!shots) shots=(game.multishot?CountShips(player.id):1);
		while(shots>0)
		{
			Cycle();
			Alert("\1nChoose target [\1h?\1n] help (\1hshots\1n: \1h" + shots + "\1n)");
			var coords=ChooseTarget(opponent);
			if(!coords) return false;
			opponent.board.shots[coords.x][coords.y]=true;
			opponent.board.DrawSpace(coords.x,coords.y);
			SendAttack(coords,opponent);
			game.StoreShot(coords,opponent.id);
			var hit=CheckShot(coords,opponent);
			if(game.bonusattack) 
			{
				if(!hit) shots--;
			}
			else shots--;
			var count=CountShips(opponent.id);
			if(count===0) 
			{
				var p1=game.FindCurrentUser();
				var p2=game.FindOpponent();
				EndGame(p1,p2);
				break;
			}
			InfoBar();
		}
		shots=false;
		game.NextTurn();
		game.StoreGame();
		if(game.turn==gameplayers.GetFullName("CPU") && !game.finished) ComputerTurn();
		else game.NotifyPlayer();
		InfoBar();
		Send("endturn","endturn");
		return true;
	}
	function ChooseTarget(opponent)
	{
		var letters="abcdefghij";
		var selected;
		var board=opponent.board;
		selected={"x":0,"y":0};
		board.DrawSelected(selected.x,selected.y);
		while(1)
		{
			Cycle();
			var key=console.inkey(K_NOECHO|K_NOCRLF|K_UPPER,50);
			if(key)
			{
				Alert("\1nChoose target [\1h?\1n] help (\1hshots\1n: \1h" + shots + "\1n)");
				if(key=="\r")
				{
					if(!board.shots[selected.x][selected.y]) 
					{
						board.UnDrawSelected(selected.x,selected.y);
						return selected;
					}
					else
					{
						Alert("\1r\1hInvalid Selection");
					}
				}
				switch(key.toLowerCase())
				{
					case KEY_DOWN:
						board.UnDrawSelected(selected.x,selected.y);
						if(selected.y<9) selected.y++;
						else selected.y=0;
						break;
					case KEY_UP:
						board.UnDrawSelected(selected.x,selected.y);
						if(selected.y>0) selected.y--;
						else selected.y=9;
						break;
					case KEY_LEFT:
						board.UnDrawSelected(selected.x,selected.y);
						if(selected.x>0) selected.x--;
						else selected.x=9;
						break;
					case KEY_RIGHT:
						board.UnDrawSelected(selected.x,selected.y);
						if(selected.x<9) selected.x++;
						else selected.x=0;
						break;
					case "0":
						key=10;
					case "1":
					case "2":
					case "3":
					case "4":
					case "5":
					case "6":
					case "7":
					case "8":
					case "9":
						key-=1;
						board.UnDrawSelected(selected.x,selected.y);
						selected.x=key;
						break;
					case "a":
					case "b":
					case "c":
					case "d":
					case "e":
					case "f":
					case "g":
					case "h":
					case "i":
					case "j":
						board.UnDrawSelected(selected.x,selected.y);
						selected.y=letters.indexOf(key.toLowerCase());
						break;
					case "\x1B":
						board.UnDrawSelected(selected.x,selected.y);
						return false;
					case "?":
						gamehelp.help("choosing target");
					default:
						continue;
				}
				board.DrawSelected(selected.x,selected.y);
			}
		}
		
	}
	function SendAttack(coords,opponent)
	{
		var attack={'source':currentplayer.id,'target':opponent.id,'coords':coords};
		Send(attack,"attack");
	}
	function CheckShot(coords,opponent)
	{
		if(opponent.board.grid[coords.x][coords.y])
		{
			var segment=opponent.board.grid[coords.x][coords.y].segment;
			var shipid=opponent.board.grid[coords.x][coords.y].id;
			var ship=opponent.board.ships[shipid];
			ship.TakeHit(segment);
			if(ship.sunk)
			{
				Notice("\1r\1hYou sank " + opponent.name + "'s ship!");
			}
			else
			{
				Notice("\1r\1hYou hit " + opponent.name + "'s ship!");
			}
			return true;
		}
		else
		{
			Notice("\1c\1hYou missed!");
			return false;
		}
	}
	function GetAttack(attack)
	{
		var source=game.FindPlayer(attack.source);
		var target=game.FindPlayer(attack.target);
		var coords=attack.coords;
		var board=target.board;
		var targetname=target.name + "'s";
		if(currentplayer && target.id==currentplayer.id) targetname="your";
		if(board.grid[coords.x][coords.y])
		{
			var shipid=board.grid[coords.x][coords.y].id;
			var segment=board.grid[coords.x][coords.y].segment;
			board.ships[shipid].TakeHit(segment);
			if(board.ships[shipid].sunk)
			{
				Notice("\1r\1h" + source.name  + " sank " + targetname + " ship!");
			}
			else
			{
				Notice("\1r\1h" + source.name  + " hit " + targetname + " ship!");
			}
		}
		else
		{
			Notice("\1c\1h" + source.name  + " fired and missed!");
		}
		board.shots[coords.x][coords.y]=true;
		board.DrawSpace(coords.x,coords.y);
	}
	function CountShips(id)
	{
		var player=game.FindPlayer(id);
		var board=player.board;
		var sunk=0;
		for(ship in board.ships)
		{
			if(board.ships[ship].sunk) sunk++;
		}
		return(board.ships.length-sunk);
	}

/*	GAMEPLAY FUNCTIONS	*/
	function CheckInterference(position,ship,board)
	{
		var segments=[];
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
	function PlaceShips()
	{	
		var board=currentplayer.board;
		for(id in board.ships)
		{
			Cycle();
			var ship=board.ships[id];
			Alert("\1nPlace ships [\1h?\1n] help [\1hspace\1n] rotate");
			while(1)
			{
				var position=SelectTile(ship);
				if(position) 
				{
					board.AddShip(position,id);
					break;
				}
				//else return false;
			}
			//TODO: keep track of ships placed
		}
		currentplayer.ready=true;
		game.StoreGame();
		game.StoreBoard(currentplayer.id);
		game.StorePlayer(currentplayer.id);
		UpdateStatus();
		Send("update","update");
	}
	function SelectTile(ship,placeholder)
	{
		var selected;
		var board=currentplayer.board;
		if(placeholder) selected={"x":start.x, "y":start.y};
		else
		{
			selected={"x":0,"y":0};
		}
		board.DrawShip(selected.x,selected.y,ship);
		var interference=CheckInterference(selected,ship,board);
		var ylimit=(ship.orientation=="vertical"?ship.segments.length-1:0);
		var xlimit=(ship.orientation=="horizontal"?ship.segments.length-1:0);
		while(1)
		{
			Cycle();
			var key=console.inkey(K_NOECHO|K_NOCRLF|K_UPPER,50);
			if(key)
			{
				Alert("\1nPlace ships [\1h?\1n] help [\1hspace\1n] toggle");
				if(key=="\r")
				{
					if(!interference) return selected;
					else
					{
						Alert("\1r\1hInvalid Selection");
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
					case "?":
						gamehelp.help("placing ships");
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
				interference=CheckInterference(selected,ship,board);
			}
		}
	}
	function NewGame()
	{
		//TODO: Add the ability to change game settings (timed? rated?) when restarting/rematching
		game.NewGame();
		send("update","update");
	}
	function Forfeit()
	{
		Alert("\1c\1hForfeit this game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
		var p1=game.FindCurrentUser();
		var p2=game.FindOpponent();
		EndGame(p2,p1);
	}
	function EndGame(winner,loser)
	{
		var wdata=gameplayers.players[winner.id];
		var ldata=gameplayers.players[loser.id];
		
		if(winner.name!="CPU")
		{
			wdata.wins++;
			gameplayers.StorePlayer(winner.id);
			if(winner.id==currentplayer.id)
			{
				Notice("\1r\1hYou sank all of " + loser.name + "'s ships!");
			}
		}
		if(loser.name!="CPU")
		{
			ldata.losses++;
			gameplayers.StorePlayer(loser.id);
			game.NotifyLoser(loser.id);
		}
		game.winner=winner.id;
		game.End();
		var endgame={'winner':winner.id, 'loser':loser.id};
		Send(endgame,"endgame");
		InfoBar();
	}
	function JoinGame()
	{
		game.AddPlayer(user.alias);
		currentplayer=game.FindCurrentUser();
		Send("update","update");
		InfoBar();
	}

/* AI FUNCTIONS	*/
	function ComputerTurn()
	{
		Log("Computer player taking turn");
		var cpu=game.FindComputer();
		var opponent=currentplayer;
		var shots=(game.multishot?CountShips(cpu.id):1);
		while(shots>0)
		{
			var coords=ComputerAttack(opponent);
			opponent.board.shots[coords.x][coords.y]=true;
			SendAttack(coords,opponent);
			game.StoreShot(coords,opponent.id);
			var hit=CheckComputerAttack(coords,opponent);
			if(game.bonusattack) 
			{
				if(!hit) shots--;
			}
			else shots--;
			opponent.board.DrawSpace(coords.x,coords.y);
			var count=CountShips(opponent.id);
			if(count===0) 
			{
				var winner=cpu;
				var loser=opponent;
				EndGame(winner,loser);
				break;
			}
		}
		game.NextTurn();
		game.StoreGame();
		Send("endturn","endturn");
		return true;
	}
	function CheckComputerAttack(coords,opponent)
	{
		if(opponent.board.grid[coords.x][coords.y])
		{
			var segment=opponent.board.grid[coords.x][coords.y].segment;
			var shipid=opponent.board.grid[coords.x][coords.y].id;
			var ship=opponent.board.ships[shipid];
			game.lastcpuhit=coords;
			ship.TakeHit(segment);
			if(ship.sunk)
			{
				Notice("\1r\1hCPU sank your ship!");
			}
			else Notice("\1r\1hCPU hit your ship!");
			return true;
		}
		else
		{
			Notice("\1c\1hCPU fired and missed!");
			return false;
		}
	}
	function ComputerAttack(opponent)
	{
		var coords=new Object();
		if(game.lastcpuhit && opponent.board.grid[game.lastcpuhit.x][game.lastcpuhit.y])
		{
			var shipid=opponent.board.grid[game.lastcpuhit.x][game.lastcpuhit.y].id;
			if(!opponent.board.ships[shipid].sunk)
			{
				var start={'x':game.lastcpuhit.x,'y':game.lastcpuhit.y};
				coords=FindNearbyTarget(opponent.board,start);
				if(coords) return coords;
			}
		}
		else
		{
			for(x in opponent.board.shots)
			{
				for(y in opponent.board.shots[x])
				{
					if(opponent.board.grid[x][y])
					{
						var shipid=opponent.board.grid[x][y].id;
						if(!opponent.board.ships[shipid].sunk)
						{
							var start={'x':x,'y':y};
							coords=FindNearbyTarget(opponent.board,start);
							if(coords) return coords;
						}
					}
				}
			}
		}
		coords=FindRandomTarget(opponent.board);
		return(coords);
	}
	function FindNearbyTarget(board,coords)
	{
		var tried=[];
		var tries=4;
		while(tries>0)
		{
			var randdir=random(4);
			if(!tried[randdir])
			{
				switch(randdir)
				{
					case 0: //north
						if(coords.y>0 && !board.shots[coords.x][coords.y-1]) 
						{
							coords.y--;
							return coords;
						}
						break;
					case 1: //south
						if(coords.y<9 && !board.shots[coords.x][coords.y+1]) 
						{
							coords.y++;
							return coords;
						}
						break;
					case 2: //east
						if(coords.x<9 && !board.shots[coords.x+1][coords.y]) 
						{
							coords.x++;
							return coords;
						}
						break;
					case 3: //west
						if(coords.x>0 && !board.shots[coords.x-1][coords.y]) 
						{
							coords.x--;
							return coords;
						}
						break;
				}
				tried[randdir]=true;
				tries--;
			}
		}
		return false;
	}
	function FindRandomTarget(board)
	{
		while(1)
		{
			column=random(10);
			row=random(10);
			if(!board.shots[column][row]) break;
		}
		return({'x':column,'y':row});
	}
	function ComputerPlacement()
	{
		var cpu=game.FindComputer();
		var board=cpu.board;
		for(s in board.ships)
		{
			Log("computer placing ship: " + board.ships[s].name);
			var ship=board.ships[s];
			var randcolumn=random(10);
			var randrow=random(10);
			var randorient=random(2);
			var orientation=(randorient==1?"horizontal":"vertical");
			var position={'x':randcolumn, 'y':randrow};
			ship.orientation=orientation;
			while(1)
			{
				var xlimit=(orientation=="horizontal"?ship.segments.length:0);
				var ylimit=(orientation=="vertical"?ship.segments.length:0);
				if(position.x + xlimit <=9 && position.y + ylimit<=9)
				{
					if(!CheckInterference(position,ship,board))
					{
						break;
					}
				}
				randcolumn=random(10);
				randrow=random(10);
				randorient=random(2);
				orientation=(randorient==1?"horizontal":"vertical");
				position={'x':randcolumn, 'y':randrow};
				ship.orientation=orientation;
			}
			board.AddShip(position,s);
		}
		cpu.ready=true;
		game.StoreBoard(cpu.id);
		game.StorePlayer(cpu.id);
		Send("update","update");
	}
	function ComputerInit()
	{
		game.AddPlayer("CPU");
		ComputerPlacement();
	}
	InitGame();
	InitChat();
	InitMenu();
	Main();
}
function GameData(gamefile)
{
	this.graphic=new Graphic(43,24);
	this.graphic.load(gameroot+"blarge.bin");
	this.players=[];
//	this.gamefile;		
//	this.gamenumber;
//	this.lastupdated;
//	this.password;
//	this.singleplayer;
//	this.lastcpuhit;
//	this.winner;
//	this.turn;
//	this.started;
//	this.finished; 
//	this.multishot;
//	this.bonusattack;
//	this.spectate;
	
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
			this.players[player].board.DrawBoard();
		}
		delete this.turn;
		this.finished=true;
		this.StoreGame();
	}
	this.AddPlayer=function(name)
	{
		var start;
		var hidden;
		if(name=="CPU") 
		{
			start={'x':24,'y':3};
			hidden=true;
		}
		else 
		{
			start={'x':2,'y':3};
			hidden=false;
		}
		var id=gameplayers.GetFullName(name);
		var player=new Object();
		player.name=name;
		player.id=id;
		player.board=new GameBoard(start.x,start.y,hidden);
		this.players.push(player);
		this.StorePlayer(player.id);
	}
	this.FindComputer=function()
	{
		for(player in this.players)
		{
			if(this.players[player].name=="CPU") return this.players[player];
		}
		return false;
	}
	this.FindPlayer=function(id)
	{
		for(var player in this.players)
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
		this.gamenumber=parseInt(gNum,10);
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
		this.gamefile.iniSetValue(null,"lastcpuhit",GetBoardPosition(this.lastcpuhit));
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
		gFile.iniSetValue(null,"winner",this.winner);
		gFile.iniSetValue(null,"singleplayer",this.singleplayer);
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
		this.singleplayer=gFile.iniGetValue(null,"singleplayer");
		this.finished=gFile.iniGetValue(null,"finished");
		this.started=gFile.iniGetValue(null,"started");
		this.password=gFile.iniGetValue(null,"password");
		this.multishot=gFile.iniGetValue(null,"multishot");
		this.bonusattack=gFile.iniGetValue(null,"bonusattack");
		this.spectate=gFile.iniGetValue(null,"spectate");
		this.winner=gFile.iniGetValue(null,"winner");
		this.lastcpuhit=GetBoardPosition(gFile.iniGetValue(null,"lastcpuhit"));
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
		for(var player in this.players)
		{
			if(!this.players[player].ready) return false;
		}
		this.started=true;
		this.StoreGame();
		this.NotifyPlayer();
		return true;
	}
	this.NotifyLoser=function(id)
	{
		if(!gamechat.FindUser(id))
		{
			var loser=gameplayers.players[id].name;
			var unum=system.matchuser(loser);
			var message="\1n\1gYour fleet has been destroyed in \1c\1hSea\1n\1c-\1hBattle\1n\1g game #\1h" + this.gamenumber + "\1n\1g!\r\n\r\n";
			system.put_telegram(unum, message);
		}
	}
	this.NotifyPlayer=function()
	{
		var currentplayer=this.FindCurrentUser();
		if(!gamechat.FindUser(this.turn) && this.turn!=currentplayer.id)
		{
			var nextturn=this.FindPlayer(this.turn);
			var unum=system.matchuser(nextturn.name);
			var message="\1n\1gIt is your turn in \1c\1hSea\1n\1c-\1hBattle\1n\1g\1g game #\1h" + this.gamenumber + "\r\n\r\n";
			system.put_telegram(unum, message);
			//TODO: make this handle interbbs games if possible
		}
	}
	this.LoadBoard=function(id)
	{	
		Log("Loading grid data: " + id);
		var player=	this.FindPlayer(id);
		var board=		player.board;
		var positions=	this.gamefile.iniGetAllObjects("position",id + ".board.");
		for(pos in positions)
		{
			Log("Loading position data: " + positions[pos].position);
			var coords=				GetBoardPosition(positions[pos].position);
			if(positions[pos].shot)		board.shots[coords.x][coords.y]=positions[pos].shot;
			if(positions[pos].ship_id>=0)	
			{
				var id=positions[pos].ship_id;
				var segment=positions[pos].segment;
				var orientation=positions[pos].orientation;
				player.board.grid[coords.x][coords.y]={'id':id,'segment':segment,'orientation':orientation};
				if(segment===0)	player.board.ships[id].orientation=orientation;
				if(positions[pos].shot)
				{
					board.ships[id].TakeHit(segment);
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
	//this.segments;
	//this.sunk;
	
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
	}
	function Segment(hgraph,vgraph)
	{
		this.hgraph=hgraph;
		this.vgraph=vgraph;
		this.fg="\1n\1h";
		this.hit=false;
		this.Draw=function(orientation)
		{
			var color=(this.hit?"\1r\1h":this.fg);
			var graphic=(orientation=="horizontal"?this.hgraph:this.vgraph);
			for(character in graphic)
			{
				console.print(color + graphic[character]);
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
				console.print(" ");
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
	this.hidden=hidden;

//	this.grid;
//	this.shots;
//	this.ships;
	
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
	this.DrawSelected=function(x,y)
	{
		this.GetCoords(x,y);
		console.attributes=BLINK + BG_MAGENTA;
		this.DrawSpace(x,y);
	}
	this.UnDrawSelected=function(x,y)
	{
		this.GetCoords(x,y);
		console.attributes=ANSI_NORMAL;
		this.DrawSpace(x,y);
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
			else 
			{
				this.GetCoords(x,y);
				console.putmsg(" ",P_SAVEATR);
			}
		}
		else
		{
			if(this.shots[x][y])
			{
				this.DrawMiss(x,y);
			}
			else 
			{
				this.GetCoords(x,y);
				console.putmsg(" ",P_SAVEATR);
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
		console.putmsg("\1c\1h\xFE",P_SAVEATR);
	}
	this.DrawHit=function(x,y)
	{
		this.GetCoords(x,y);
		console.putmsg("\1r\1h\xFE",P_SAVEATR);
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
		var x=position.charAt(1);
		var y=position.charAt(0);
		x=parseInt(x==0?9:position.charAt(1)-1,10);
		y=parseInt(letters.indexOf(y.toLowerCase()),10);
		position={'x':x,'y':y};
	}
	else if(typeof position=="object")
	{
		//CONVERT XY COORDINATES TO STANDARD CHESS BOARD FORMAT
		var x=(position.x==9?0:parseInt(position.x,10)+1);
		position=(letters.charAt(position.y) + x);
	}
	return(position);
}
