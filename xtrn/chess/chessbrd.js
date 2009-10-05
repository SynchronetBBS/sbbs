function GameSession(game) 
{
	var name;
	var board;
	var menu;
	var currentplayer;
	var infobar=true; //TOGGLE FOR GAME INFORMATION BAR AT TOP OF CHAT WINDOW (default: true)
	
/*	
	CHAT ENGINE DEPENDENT FUNCTIONS
	
	some of these functions are redundant
	so as to make modification of the 
	method of data transfer between nodes/systems
	simpler
*/
	function InitGame()
	{
		game.LoadGameTable();
		game.LoadGameData();
		board=game.board;
		name="Chess Table " + game.gamenumber;
		
		currentplayer=game.currentplayer;
		board.side=(currentplayer?currentplayer:"white");
	}
	function InitChat()
	{
		var rows=(infobar?15:20);
		var columns=38;
		var posx=42;
		var posy=(infobar?8:3);
		var input_line={'x':42,'y':24,columns:38};
		chesschat.Init(name,input_line,columns,rows,posx,posy,true);
	}
	function InitMenu()
	{
		menu=new Menu(		chesschat.input_line.x,chesschat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Sit"							, 
								"~Resign"						,
								"~Offer Draw"					,
								"~New Game"						,
								"~Move"							,
								"Re~verse"						,
								"Toggle Board E~xpansion"		,
								"~Help"							,
								"Move ~List"					,
								"Re~draw"						];
		menu.add(menu_items);
		RefreshMenu();
	}
	function Main()
	{
		Redraw();
		ScanCheck();
		if(game.turn==game.currentplayer)
		{
			Notice("\1g\1hIt's your turn.");
			Notice("\1n\1gUse arrow keys or number pad to move around, and [\1henter\1n\1g] to select.");
			Notice("\1n\1gPress [\1hescape\1n\1g] to cancel.");
		}
		while(1)
		{
			Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(game.turn==game.currentplayer)
			{
				switch(k)
				{
					case "1":
					case "2":
					case "3":
					case "4":
					case "6":
					case "7":
					case "8":
					case "9":
						if(chesschat.buffer.length) 
						{
							if(!Chat(k,chesschat) && HandleExit()) return;						
						}
					case KEY_UP:
					case KEY_DOWN:
					case KEY_LEFT:
					case KEY_RIGHT:
						TryMove();
						board.DrawLastMove();
						break;
					default:
						break;
				}
			}
			if(k)
			{
				switch(k)
				{
					case "/":
						if(!chesschat.buffer.length) 
						{
							RefreshMenu();
							ListCommands();
							ChessMenu();
						}
						else if(!Chat(k,chesschat) && HandleExit()) return;
						break;
					case "\x1b":	
						if(HandleExit()) return;
						break;
					case "?":
						if(!chesschat.buffer.length) 
						{
							ToggleGameInfo();
						}
						else if(!Chat(k,chesschat) && HandleExit()) return;
						break;
					default:
						if(!Chat(k,chesschat) && HandleExit()) return;
						break;
				}
			}
		}
	}
	function ClearChatWindow()
	{
		chesschat.ClearChatWindow();
	}
	function HandleExit()
	{
		if(game.finished)
		{
			if(file_exists(game.gamefile.name))
				file_remove(game.gamefile.name);
			if(file_exists(chessroot + name + ".his"))
				file_remove(chessroot + name + ".his");
		}
		else if(game.timed && game.started && game.movelist.length)
		{
			Alert("\1c\1hForfeit this game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
			EndGame("loss");
		}
		return true;
	}
	function RefreshMenu()
	{
		if(game.started || game.finished || currentplayer) menu.disable(["S"]);
		else if(game.password && game.password != user.alias) menu.disable(["S"]);
		else if(game.minrating && game.minrating > chessplayers.GetPlayerRating(user.alias)) menu.disable(["S"]);
		else menu.enable(["S"]);
		if(!game.finished || !currentplayer) menu.disable(["N"]);
		else menu.enable(["N"]);
		if(!game.started || game.turn!=currentplayer || game.finished) 
		{
			menu.disable(["M"]);
			menu.disable(["R"]);
		}
		else 
		{
			menu.enable(["M"]);
			menu.enable(["R"]);
		}
		if(game.draw || game.turn != currentplayer) menu.disable(["O"]);
		else
		{
			menu.enable(["O"]);
		}
		if(!game.movelist.length) menu.disable(["L"]);
		else menu.enable(["L"]);
	}
	function ChessMenu()
	{	
		menu.displayHorizontal();
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		ClearAlert();
		if(k)
		{
			ClearChatWindow();
			chesschat.Redraw();
			if(menu.items[k] && menu.items[k].enabled) 
			{
				switch(k)
				{
					case "R":
						Resign();
						break;
					case "S":
						JoinGame();
						break;
					case "H":
						Help();
						break;
					case "D":
						Redraw();
						break;
					case "O":
						OfferDraw();
						break;
					case "M":
						TryMove();
						board.DrawLastMove();
						break;
					case "V":
						Reverse();
						Redraw();
						break;
					case "L":
						ListMoves();
						break;
					case "X":
						ToggleBoardSize();
						break;
					case "N":
						NewGame();
						break;
					default:
						break;
				}
				ClearChatWindow();
				chesschat.Redraw();
			}
			else Log("Invalid or Disabled key pressed: " + k);
		}
	}
	function ToggleBoardSize()
	{
		board.large=board.large?false:true;
		if(board.large)
		{
			console.screen_rows=50;
			var rows=3;
			var columns=77;
			var posx=2;
			var posy=45;
			var input_line={'x':2,'y':50,columns:77};
			chesschat.Init(name,input_line,columns,rows,posx,posy,false);
			//reinitialize chat beneath large board
		}
		else
		{
			console.screen_rows=24;
			InitChat();
			//reinitialize chat beside small board
		}
		InitMenu();
		Redraw();
	}
	function Help()
	{
	}
	function ListCommands()
	{
		if(board.large) return;
		var list=menu.getList();
		DisplayInfo(list);
	}
	function ToggleGameInfo()
	{
		if(infobar)
		{
			infobar=false;
			chesschat.Resize(chesschat.x,3,chesschat.columns,20);
		}
		else
		{
			infobar=true;
			chesschat.Resize(chesschat.x,8,chesschat.columns,15);
		}
		InfoBar();
	}
	function InfoBar()
	{
		if(board.large) return;
		console.gotoxy(chesschat.x,1);
		console.putmsg("\1k\1h[\1cEsc\1k\1h]\1wQuit \1k\1h[\1c/\1k\1h]\1wMenu \1k\1h[\1c?\1k\1h]\1wToggle \1k\1h[\1c" + ascii(30) + "\1k\1h,\1c" + ascii(31) +"\1k\1h]\1wScroll");
		if(infobar)
		{
			var x=chesschat.x;
			var y=3;
			var turn=false;
			if(game.started && !game.finished) turn=game.turn;
			
			for(player in game.players)
			{
				console.gotoxy(x,y);
				console.putmsg(PrintPadded(chessplayers.FormatPlayer(game.players[player],player,turn),21));
				console.putmsg(chessplayers.FormatStats(game.players[player]));
				if(game.timed && game.started) 
				{
					console.gotoxy(x,y+1);
					game.players[player].timer.Display(game.turn);
				}
				y+=2;
			}
		}
	}
	function ShowTimers()
	{
		if(!infobar) return;
		var x=chesschat.x;
		var y=4;
		for(player in game.players)
		{
			var timer=game.players[player].timer;
			var current=time();
			if(current!=timer.lastshown && game.turn==player)
			{
				console.gotoxy(x,y);
				game.players[player].timer.Display(game.turn);
			}
			y+=2;
		}
	}
	function ClearAlert()
	{
		if(!chesschat.buffer.length) chesschat.ClearInputLine();
	}
	function Notice(txt)
	{
		chesschat.AddNotice(txt);
	}
	function DisplayInfo(array)
	{
		chesschat.DisplayInfo(array);
	}
	function Alert(msg)
	{
		chesschat.Alert(msg);
	}
	function Cycle()
	{
		chesschat.Cycle();
		if(queue.DataWaiting("move"))
		{
			var data=queue.ReceiveData("move");
			for(move in data)
			{
				var m=new ChessMove(data[move].from,data[move].to,data[move].color);
				GetMove(m);
				game.movelist.push(m);
				ScanCheck();
			}
			game.NextTurn();
			InfoBar();
			if(game.turn==game.currentplayer)
			{
				Notice("\1g\1hIt's your turn.");
			}
			else
			{
				Notice("\1g\1hIt is " + game.GetAlias(game.turn) + "'s turn.");
			}
		}
		if(queue.DataWaiting("draw"))
		{
			Log("received draw offer");
			var trash=queue.ReceiveData("draw");
			if(Draw())
			{
				Send("\1g\1hDraw offer accepted.","alert");
			}
			else
			{
				game.StoreGame();
				Send("\1g\1hDraw offer refused.","alert");
			}
			delete game.draw;
			ClearAlert();
		}
		else if(game.draw)
		{
			if(game.draw!=currentplayer)
			{
				Log("received draw offer from: " + game.draw);
				if(Draw())
				{
					Send("\1g\1hDraw offer accepted.","alert");
				}
				else
				{
					Send("\1g\1hDraw offer refused.","alert");
					game.StoreGame();
				}
			}
			delete game.draw;
			ClearAlert();
		}
		if(queue.DataWaiting("alert"))
		{
			Log("received alert");
			var alert=queue.ReceiveData("alert");
			DisplayInfo(alert);
			Alert("\1r\1h[Press any key]");
			while(console.inkey==="");
			ClearChatWindow();
			chesschat.Redraw();
		}
		if(queue.DataWaiting("castle"))
		{
			var data=queue.ReceiveData("castle");
			for(move in data)
			{
				GetMove(data[move]);
			}
		}
		if(queue.DataWaiting("updatetile"))
		{ 
			var data=queue.ReceiveData("updatetile");
			for(tile in data)
			{
				var newpiece=false;
				var update=board.grid[data[tile].x][data[tile].y];
				if(data[tile].contents) newpiece=new ChessPiece(data[tile].contents.name,data[tile].contents.color);
				update.contents=newpiece;
				update.Draw(board.side,true);
			}
		}
		if(queue.DataWaiting("update"))
		{
			Log("updating game data");
			var trash=queue.ReceiveData("update");
			game.LoadGameTable();
			game.LoadGameData();
			chessplayers.LoadPlayers();
			InfoBar();
			if(game.timed) ShowTimers();
		}
		if(queue.DataWaiting("timer"))
		{
			var data=queue.ReceiveData("timer");
			for(item in data)
			{
				var timer=data[item];
				var player=timer.player;
				var countdown=timer.countdown;
				game.players[player].timer.Update(countdown);
			}
			InfoBar();
		}
		if(game.timed && game.started && !game.finished) 
		{
			for(player in game.players)
			{
				if(game.turn==player && game.movelist.length)
				{
					var timer=game.players[player].timer;
					var current=time();
					if(timer.lastupdate===false) game.players[player].timer.lastupdate=current;
					if(timer.lastupdate!=current) 
					{
						var counted=timer.Countdown(current);
						if(!counted && player==currentplayer) EndGame("loss");
					}
				}
			}
			ShowTimers();
		}
	}
	function Redraw()
	{
		console.clear();
		board.DrawBoard();
		InfoBar();
		chesschat.Redraw();
	}
	function Send(data,ident)
	{
		queue.SendData(data,ident);
	}
	function Reverse()
	{
		board.Reverse();
	}

	
/*	MOVEMENT FUNCTIONS	*/
	function TryMove()
	{
		board.ClearLastMove();
		var moved=false;
		var from;
		var to;
		var placeholder;
		var reselect;
		while(1)
		{
			reselect=false;
			while(1)
			{
				from=SelectTile(false,from);
				if(from===false) return false;
				else if(	board.grid[from.x][from.y].contents && 
							board.grid[from.x][from.y].contents.color == game.turn) 
					break;
				else 
				{
					Alert("\1r\1hInvalid Selection");
				}
			}
			placeholder=from;
			while(1)
			{
				to=SelectTile(from,placeholder);
				if(to===false) return false;
				else if(from.x==to.x && from.y==to.y) 
				{
					reselect=true;
					break;
				}
				else if(!board.grid[to.x][to.y].contents || 
						board.grid[to.x][to.y].contents.color != game.turn)
				{
					if(CheckMove(from,to)) 
					{
						moved=true;
						break; 
					}
					else 
					{
						placeholder=to;
						Alert("\1r\1hIllegal Move");
					}
				}
				else 
				{
					placeholder=to;
					Alert("\1r\1hInvalid Selection");
				}
			}
			if(!reselect) break;
		}
		board.DrawTile(from.x,from.y,moved);
		board.DrawTile(to.x,to.y,moved);
		return true;
	}
	function CheckMove(from,to)
	{ 
		var movetype=CheckRules(from,to);
		if(!movetype) return false; //illegal move
		
		var from_tile=board.grid[from.x][from.y];
		var to_tile=board.grid[to.x][to.y];
		var move=new ChessMove(from,to,from_tile.contents.color);

		if(movetype=="en passant")
		{
			if(!EnPassant(from_tile,to_tile,move)) return false;
		}
		else if(movetype=="pawn promotion")
		{
			if(!PawnPromotion(to_tile,move)) return false;
		}
		else if(movetype=="castle")
		{
			if(!Castle(from_tile,to_tile)) return false;
		}
		else
		{
			game.Move(move);
			if(InCheck(board.FindKing(currentplayer),currentplayer)) 
			{
				Log("Illegal Move: King would be in check");
				game.UnMove(move);
				return false;
			}
			SendMove(move);
		}
		InfoBar();
		return true;
	}
	function SendMove(move)
	{
		game.NextTurn();
		Notice("\1g\1hIt is " + game.GetAlias(game.turn) + "'s turn.");
		game.NotifyPlayer();
		board.lastmove=move;
		game.movelist.push(move);
		game.StoreGame();
		Send(move,"move");
		if(game.timed) 
		{
			Send(game.players[currentplayer].timer,"timer");
		}
	}
	function GetMove(move)
	{
		game.Move(move);
		board.ClearLastMove();
		board.lastmove=move;
		board.DrawTile(move.from.x,move.from.y,true,"\1r\1h");
		board.DrawTile(move.to.x,move.to.y,true,"\1r\1h");
	}
	function ScanCheck()
	{
		if(currentplayer)
		{
			var checkers=InCheck(board.FindKing(currentplayer),currentplayer);
			if(checkers) 
			{
				if(FindCheckMate(checkers,currentplayer)) 
				{
					EndGame("loss");
					Notice("\1r\1hCheckmate! You lose!");
				}
				else Notice("\1r\1hYou are in check!");
			}
		}
	}
	function SelectTile(start,placeholder)
	{
		if(start) 
		{
			var sel=false;
			if(!placeholder) 
			{
				selected={"x":start.x, "y":start.y};
				sel=true;
			}
			board.DrawBlinking(start.x,start.y,sel,"\1n\1b");
		}
		if(placeholder) 
		{
			selected={"x":placeholder.x, "y":placeholder.y};
			board.DrawTile(selected.x,selected.y,true,"\1n\1b");
		}
		else
		{
			if(board.side=="white") selected={"x":0,"y":7};
			else selected={"x":7,"y":0};
			board.DrawTile(selected.x,selected.y,true,"\1n\1b");
		}
		while(1)
		{
			Cycle();
			var key=console.inkey(K_NOECHO|K_NOCRLF,50);
			if(key=="\r") 
			{
				if(chesschat.buffer.length) Chat(key,chesschat);
				else return selected;
			}
			if(key)
			{
				if(board.side=="white")
				{
					ClearAlert();
					switch(key)
					{
						case KEY_DOWN:
						case "2":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case KEY_UP:
						case "8":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case KEY_LEFT:
						case "4":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							break;
						case KEY_RIGHT:
						case "6":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							break;
						case "7":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "9":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "1":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "3":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "\x1B":
							if(start) board.DrawTile(start.x,start.y);
							board.DrawTile(selected.x,selected.y);
							return false;
						default:
							if(!Chat(key,chesschat)) 
							{
								if(start) board.DrawTile(start.x,start.y);
								board.DrawTile(selected.x,selected.y);
								return false;
							}
							continue;
					}
					if(start && start.x==selected.x && start.y==selected.y) 
							board.DrawBlinking(start.x,start.y,true);
					else
						board.DrawTile(selected.x,selected.y,true);
				}
				else
				{
					ClearAlert();
					if(start) board.DrawBlinking(start.x,start.y);
					switch(key)
					{
						case KEY_UP:
						case "2":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case KEY_DOWN:
						case "8":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case KEY_RIGHT:
						case "4":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							break;
						case KEY_LEFT:
						case "6":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							break;
						case "3":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "1":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y===0) selected.y=7;
							else selected.y--;
							break;
						case "9":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x===0) selected.x=7;
							else selected.x--;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "7":
							if(start && start.x==selected.x && start.y==selected.y) 
									board.DrawBlinking(start.x,start.y);
							else
								board.DrawTile(selected.x,selected.y);
							if(selected.x==7) selected.x=0;
							else selected.x++;
							if(selected.y==7) selected.y=0;
							else selected.y++;
							break;
						case "\x1B":
							if(start) board.DrawTile(start.x,start.y);
							board.DrawTile(selected.x,selected.y);
							return false;
						default:
							if(!Chat(key,chesschat)) 
							{
								if(start) board.DrawTile(start.x,start.y);
								board.DrawTile(selected.x,selected.y);
								return false;
							}
							continue;
					}
					if(start && start.x==selected.x && start.y==selected.y) 
							board.DrawBlinking(start.x,start.y,true);
					else
						board.DrawTile(selected.x,selected.y,true);
				}
			}
		}
	}
	function CheckRules(from,to)
	{ 
		var from_tile=board.grid[from.x][from.y];
		var to_tile=board.grid[to.x][to.y];
		
		from.y=parseInt(from.y);
		from.x=parseInt(from.x);
		to.y=parseInt(to.y);
		to.x=parseInt(to.x);
		
		if(to_tile.contents && to_tile.contents.color==from_tile.contents.color) 
		{
			Log("Invalid Move: Target Color same as Source Color");
			return false;
		}
		//KING RULESET
		if(from_tile.contents.name=="king") 
		{
			if(Math.abs(from.x-to.x)==2) 
			{
				if(InCheck(from,from_tile.contents.color))
				{
					Log("Invalid Move: King is in check");
					return false;
				}
				if(from_tile.contents.has_moved) 
				{
					Log("Invalid Move: King has already moved and cannot castle");
					return false;
				}
				if(from.x > to.x) 
				{
					var x=from.x-1;
					while(x>0) 
					{
						var spot_check={"x":x,"y":from.y};
						if(board.grid[x][from.y].contents) return false;
						if(InCheck(spot_check,from_tile.contents.color)) return false;
						x--;
					}
				}
				else 
				{
					var x=from.x+1;
					while(x<7) 
					{
						var spot_check={"x":x,"y":from.y};
						if(board.grid[x][from.y].contents) return false;
						if(InCheck(spot_check,from_tile.contents.color)) return false;
						x++;
					}
				}
				return "castle";
			}
			else 
			{
				if(Math.abs(from.x-to.x)>1 || Math.abs(from.y-to.y)>1) 
				{
					Log("Invalid Move: King can only move one space unless castling");
					return false;
				}
			}
		}
		//PAWN RULESET
		else if(from_tile.contents.name=="pawn") 
		{
			var xgap=Math.abs(from.x-to.x);
			var ygap=Math.abs(from.y-to.y);

			if(ygap>2 || ygap<1) return false; 
			if(xgap>1) return false; 
			if(from.x==to.x && to_tile.contents) return false; //CANNOT TAKE PIECES ON SAME VERTICAL LINE
			switch(from_tile.contents.color)
			{
				case "white":
					if(from.y<to.y) return false; //CANNOT MOVE BACKWARDS
					if(ygap==2)
					{
						if(from.y!=6) 
							return false;
						if(board.grid[from.x][from.y-1].contents) 
							return false;
					}
					if(to.y===0) return "pawn promotion";
					if(xgap==ygap && !to_tile.contents)
					{
						//EN PASSANT
						if(to.y==2) 
						{
							var lastmove=board.lastmove;
							if(lastmove.to.x!=to.x) 
							{ 
								return false; 
							}
							var lasttile=board.grid[lastmove.to.x][lastmove.to.y];
							if(Math.abs(lastmove.from.y-lastmove.to.y)!=2)
							{ 
								return false; 
							}
							if(lasttile.contents.name!="pawn")
							{ 
								return false; 
							}
							return "en passant";
						}
						else return false;
					}
					break;
				case "black":
					if(from.y>to.y) return false; //CANNOT MOVE BACKWARDS
					if(ygap==2)
					{
						if(from.y!=1) return false;
						if(board.grid[from.x][from.y+1].contents) return false;
					}
					if(to.y==7) return "pawn promotion";
					if(xgap==ygap && !to_tile.contents)
					{
						//EN PASSANT
						if(to.y==5)
						{
							var lastmove=board.lastmove;
							if(lastmove.to.x!=to.x) 
							{ 
								return false; 
							}
							var lasttile=board.grid[lastmove.to.x][lastmove.to.y];
							if(Math.abs(lastmove.from.y-lastmove.to.y)!=2)
							{ 
								return false; 
							}
							if(lasttile.contents.name!="pawn")
							{ 
								return false; 
							}
							return "en passant";
						}
						else return false;
					}
					break;
			}
		}
		//KNIGHT RULESET
		else if(from_tile.contents.name=="knight") 
		{
			if(Math.abs(from.x-to.x)==2 && Math.abs(from.y-to.y)==1);
			else if(Math.abs(from.y-to.y)==2 && Math.abs(from.x-to.x)==1);
			else return false;
		}
		//ROOK RULESET
		else if(from_tile.contents.name=="rook") 
		{
			if(from.x==to.x) 
			{
				if(from.y<to.y) 
				{
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[from.x][from.y+check].contents) return false;
					}
				}
				else 
				{
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[from.x][from.y-check].contents) return false;
					}
				}
			}
			else if(from.y==to.y) 
			{
				if(from.x<to.x) 
				{
					var distance=to.x-from.x;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[from.x+check][from.y].contents) return false;
					}
				}
				else 
				{
					var distance=from.x-to.x;
					for(check = 1;check<distance;check++)
					{
						if(board.grid[from.x-check][from.y].contents) return false;
					}
				}
			}
			else return false;
		}
		//BISHOP RULESET
		else if(from_tile.contents.name=="bishop") 
		{
			diffx=from.x-to.x;
			diffy=from.y-to.y;
			if(Math.abs(diffx)==Math.abs(diffy)){
				var distance=Math.abs(diffx);
				var checkx=from.x;
				var checky=from.y;
				for(check=1;check<distance;check++) {
					if(diffx<0) checkx++;
					else checkx--;
					if(diffy<0) checky++;
					else checky--;
					Log("checking space: " + checkx + "," + checky);
					if(board.grid[checkx][checky].contents) return false;
				}
			}
			else return false;
		}
		//QUEEN RULESET
		else if(from_tile.contents.name=="queen") 
		{
			diffx=from.x-to.x;
			diffy=from.y-to.y;
			if(Math.abs(diffx)==Math.abs(diffy))
			{
				var distance=Math.abs(diffx);
				var checkx=from.x;
				var checky=from.y;
				for(check=1;check<distance;check++) 
				{
					if(diffx<0) checkx++;
					else checkx--;
					if(diffy<0) checky++;
					else checky--;
					if(board.grid[checkx][checky].contents) return false;
				}
			}
			else if(from.x==to.x) 
			{
				if(from.y<to.y) 
				{
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[from.x][parseInt(from.y,10)+check].contents) return false;
					}
				}
				else 
				{
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[from.x][from.y-check].contents) return false;
					}
				}
			}
			else if(from.y==to.y) 
			{
				if(from.x<to.x) 
				{
					var distance=to.x-from.x;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[parseInt(from.x,10)+check][from.y].contents) return false;
					}
				}
				else 
				{
					var distance=from.x-to.x;
					for(check = 1;check<distance;check++) 
					{
						if(board.grid[parseInt(from.x)-check][from.y].contents) return false;
					}
				}
			}
			else return false;
		}

		return true;
	}
	function EnPassant(from,to,move)
	{
		Log("trying en passant");
		var row=(to.y<from.y?to.y+1:to.y-1);
		var cleartile=board.grid[to.x][row];
		var temp=cleartile.contents;
		delete cleartile.contents;
		game.Move(move);
		if(InCheck(board.FindKing(currentplayer),currentplayer)) 
		{
			Log("restoring original piece");
			game.UnMove(move);
			cleartile.contents=temp;
			return false;
		}
		board.DrawTile(cleartile.x,cleartile.y);
		Send(cleartile,"updatetile");
		SendMove(move);
		return true;
	}
	function Castle(from,to)
	{
		Log("trying castle");
		var rfrom;
		var rto;
		if(from.x-to.x==2) 
		{	//QUEENSIDE
			rfrom={'x':to.x-2,'y':to.y};
			rto={'x':to.x+1,'y':to.y};	
		}
		else 
		{	//KINGSIDE
			rfrom={'x':to.x+1,'y':to.y};
			rto={'x':to.x-1,'y':to.y};
		}
		var castle=new ChessMove(rfrom,rto);
		var move=new ChessMove(from,to);
		game.Move(castle);
		game.Move(move);
		Send(castle,"castle");
		SendMove(move);
		board.DrawTile(rfrom.x,rfrom.y,false);
		board.DrawTile(rto.x,rto.y,false);
		return true;
	}
	function PawnPromotion(to_tile,move)
	{
		game.Move(move);
		if(InCheck(board.FindKing(currentplayer),currentplayer)) 
		{
			game.UnMove(move);
			return false;
		}
		SendMove(move);
		to_tile.contents=new ChessPiece("queen",to_tile.contents.color);
		Send(to_tile,"updatetile");
	}
	function FindCheckMate(checkers,player)
	{
		var position=board.FindKing(player);
		if(KingCanMove(position,player)) return false;
		Log("king cannot move");
		for(checker in checkers) 
		{
			var spot=checkers[checker];
			Log("checking if " + player + " can take or block " +  spot.contents.color + " " + spot.contents.name);
			if(spot.contents.name=="knight" || spot.contents.name=="pawn") 
			{
				var canmove=CanMoveTo(spot,player);
				//if king cannot move and no pieces can take either the attacking knight or pawn - checkmate!
				if(!canmove) return true;
				//otherwise attempt each move, then scan for check - if check still exists after all moves are attempted - checkmate!
				for(piece in canmove)
				{
					var move=new ChessMove(canmove[piece],spot);
					game.Move(move);
					if(InCheck(position,player)) 
					{
						game.UnMove(move);
					}
					//if player is no longer in check after move is attempted - game continues
					else 
					{
						game.UnMove(move);
						return false;
					}						
				}
			}
			else 
			{
				path=FindPath(spot,position);
				for(p in path)
				{
					var pos=path[p];
					var canmove=CanMoveTo(pos,player);
					if(canmove)
					{
						for(piece in canmove)
						{
							Log("attempting block move: " + canmove[piece].contents.name);
							var move=new ChessMove(canmove[piece],pos);
							game.Move(move);
							if(InCheck(position,player)) 
							{
								game.UnMove(move);
							}
							else 
							{
								game.UnMove(move);
								return false;
							}						
						}
					}
				}
			}
		}
		return true;
	}
	function FindPath(from,to)
	{
		var path=[];
		var incrementx=0;
		var incrementy=0;
		if(from.x!=to.x)
		{
			incrementx=(from.x>to.x?-1:1);
		}
		if(from.y!=to.y)
		{
			incrementy=(from.y>to.y?-1:1);
		}
		var x=from.x;
		var y=from.y;
		while(x!=to.x || y!=to.y)
		{
			Log("adding target x" + x + "y" + y);
			path.push({"x":x,"y":y});
			x+=incrementx;
			y+=incrementy;
		}
		return path;
	}
	function CanMoveTo(position,player)
	{
		var pieces=board.FindAllPieces(player);
		var canmove=[];
		for(p in pieces)
		{
			var piece=pieces[p];
			var gridposition=board.grid[piece.x][piece.y];
			if(gridposition.contents.name=="king")
			{
				Log("skipping king, moves already checked");
			}
			else if(CheckRules(piece,position))
			{
				Log("piece can cancel check: " + gridposition.contents.color + " " + gridposition.contents.name);
				canmove.push(piece);
			}
		}
		if(canmove.length) return canmove;
		return false;
	}
	function KingCanMove(king,player)
	{
		Log("checking if king can move from x: " + king.x + " y: " + king.y);
		var north=		(king.y>0?					{"x":king.x,	"y":king.y-1}:false);
		var northeast=	(king.y>0 && king.x<7?		{"x":king.x+1,	"y":king.y-1}:false);
		var northwest=	(king.y>0 && king.x>0?		{"x":king.x-1,	"y":king.y-1}:false);
		var south=		(king.y<7?					{"x":king.x,	"y":king.y+1}:false);
		var southeast=	(king.y<7 && king.x<7?		{"x":king.x+1,	"y":king.y+1}:false);
		var southwest=	(king.y<7 && king.x>0?		{"x":king.x-1,	"y":king.y+1}:false);
		var east=		(king.x<7?					{"x":king.x+1,	"y":king.y}:false);
		var west=		(king.x>0?					{"x":king.x-1,	"y":king.y}:false);
		
		var directions=[north,northeast,northwest,south,southeast,southwest,east,west];
		for(dir in directions) 
		{
			var direction=directions[dir];
			if(direction && CheckRules(king,directions[dir])) 
			{
				var move=new ChessMove(king,direction,player);
				game.Move(move);
				if(InCheck(direction,player)) 
				{
					Log("taking piece would cause check");
					game.UnMove(move);
				}
				else
				{
					Log("king can move to x: " + direction.x + " y: " + direction.y);
					game.UnMove(move);
					return true;
				}
			}
		}
		return false;
	}
	function InCheck(position,player)
	{
		var check_pieces=[];
		for(column in board.grid) 
		{ 
			for(var row in board.grid[column]) 
			{
				if(board.grid[column][row].contents)
				{
					if(board.grid[column][row].contents.name=="king");
					else if(board.grid[column][row].contents.color!=player) 
					{
						var from={"x":column, "y":row};
						if(CheckRules(from,position)) 
						{
							Log(board.grid[column][row].color + " " + board.grid[column][row].contents.name + " at " + column + "," + row + " can take king");
							check_pieces.push(board.grid[column][row]);
						}
					}
				}
			}
		}
		if(check_pieces.length) return check_pieces;
		else return false;
	}
	function Draw()
	{
		Alert("\1c\1hOpponent offered a draw. Accept? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
		EndGame("draw");
		return true;
	}
/*	GAMEPLAY FUNCTIONS	*/
	function PlayerInGame()
	{
		return game.PlayerInGame();
	}
	function OfferDraw()
	{
		Alert("\1c\1hOffer your opponent a draw? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return false;
		game.draw=currentplayer;
		game.StoreGame();
		Send("draw","draw");
	}
	function NewGame()
	{
		//TODO: Add the ability to change game settings (timed? rated?) when restarting/rematching
		game.NewGame();
		send("update","update");
	}
	function Resign()
	{
		Alert("\1c\1hAre you sure? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return;
		EndGame("loss");
	}
	function EndGame(outcome)
	{
		Log("Ending game - " + currentplayer + ": " + outcome);
		if(game.movelist.length) 
		{
			var cp=currentplayer;
			
			var side1=(cp=="white"?"white":"black");
			var side2=(cp=="white"?"black":"white");
			
			var id1=game.players[side1].id;
			var id2=game.players[side2].id;
			
			var p1=chessplayers.players[id1];
			var p2=chessplayers.players[id2];
			
			var r1=p1.rating;
			var r2=p2.rating;
			
			switch(outcome)
			{
				case "win":
					p1.wins+=1;
					p2.losses+=1;
					break;
				case "loss":
					p1.losses+=1;
					p2.wins+=1;
					break;
				case "draw":
					p1.draws+=1;
					p2.draws+=1;
					break;
			}
			if(game.rated)
				switch(outcome)
				{
					case "win":
						p1.rating=GetRating(p1.rating,p2.rating,"win");
						p2.rating=GetRating(p2.rating,p1.rating,"loss");
						break;
					case "loss":
						p1.rating=GetRating(p1.rating,p2.rating,"loss");
						p2.rating=GetRating(p2.rating,p1.rating,"win");
						break;
					case "draw":
						p1.rating=GetRating(p1.rating,p2.rating,"draw");
						p2.rating=GetRating(p2.rating,p1.rating,"draw");
						break;
				}
			
			chessplayers.StorePlayer(id1);
			chessplayers.StorePlayer(id2);
		}
		game.End();
		InfoBar();
		Send("update","update");
	}
	function ShowNewRatings(p1,p2)
	{
		var info=[];
		info.push("\1c\1hGame Completed");
		info.push("\1c\1h" + p1.name + "'s new rating: " + p1.rating);
		info.push("\1c\1h" + p2.name + "'s new rating: " + p2.rating);
		DisplayInfo(info);
		Send(info,"alert");
		Alert("\1r\1h[Press any key]");
		while(console.inkey()==="");
	}
	function JoinGame()
	{
		if(game.players.white.id) 
		{
			game.AddPlayer("black",user.alias);
			game.Start("black");
		}
		else if(game.players.black.id) 
		{
			game.AddPlayer("white",user.alias);
			game.Start("white");
		}
		else 
		{
			chesschat.Alert("\1nChoose a side [\1hW\1n,\1hB\1n]: ");
			var color=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
			switch(color)
			{
				case "W":
					game.AddPlayer("white",user.alias);
					break;
				case "B":
					game.AddPlayer("black",user.alias);
					break;
				default:
					return;
			}
			game.StoreGame();
		}
		currentplayer=game.currentplayer;
		Send("update","update");
		Redraw();
	}
	function ListMoves()
	{
		var list=["\1c\1hMOVE HISTORY"];
		for(m=0;m<game.movelist.length;m++)
		{	
			var move=game.movelist[m];
			var text=game.movelist[m].Format();
			m++;
			if(game.movelist[m])
			{
				text+=("  " + game.movelist[m].Format());
			}
			list.push(text);
		}
		DisplayInfo(list);
		Alert("\1r\1hPress a key to continue");
		console.getkey();
	}
	function GetRating(r1,r2,outcome)
	{
		//OUTCOME SHOULD BE RELATIVE TO PLAYER 1 (R1 = PLAYER ONE RATING)
		//WHERE PLAYER 1 IS THE USER RUNNING THE CURRENT INSTANCE OF THE SCRIPT
		//BASED ON "ELO" RATING SYSTEM: http://en.wikipedia.org/wiki/ELO_rating_system#Mathematical_details
		
		
		var K=(r1>=2400?16:32); //USE K-FACTOR OF 16 FOR PLAYERS RATED 2400 OR GREATER
		var points;
		switch(outcome)
		{
			case "win":
				points=1;
				break;
			case "loss":
				points=0;
				break;
			case "draw":
				points=0.5;
				break;
		}
		var expected=1/(1+Math.pow(10,(r2-r1)/400));
		var newrating=Math.round(r1+K*(points-expected));
		return newrating;
	}

	InitGame();
	InitChat();
	InitMenu();
	Main();
}
function ChessGame(gamefile)
{
	this.board=new ChessBoard();
	this.players=[];
	this.movelist=[];
	//this.gamefile;		
	//this.gamenumber;
	//this.lastupdated;
	//this.currentplayer;
	//this.password;
	//this.turn;
	//this.started;
	//this.finished;  
	//this.rated;
	//this.timed;
	//this.minrating;
	//this.draw;
	
	this.Init=function()
	{
		if(gamefile)
		{
			this.gamefile=new File(gamefile);
			var fName=file_getname(gamefile);
			this.LoadGameTable();
		}
		else
		{
			this.SetFileInfo();
			this.NewGame();
		}
		this.currentplayer=this.PlayerInGame();
	}
	this.End=function()
	{
		delete this.turn;
		this.started=false;
		this.finished=true;
		this.StoreGame();
	}
	this.Move=function(move)
	{
		Log("moving from " + move.from.x + "," + move.from.y + " to "+ move.to.x + "," + move.to.y);
		this.board.Move(move.from,move.to);
	}
	this.UnMove=function(move)
	{
		this.board.UnMove(move.from,move.to);
	}
	this.AddPlayer=function(color,player)
	{
		var fullname=chessplayers.GetFullName(player);
		this.players[color].id=fullname;
		this.currentplayer=color;
		this.board.side=color;
	}
	this.PlayerInGame=function()
	{
		for(player in this.players)
		{
			if(this.players[player].id==chessplayers.GetFullName(user.alias))
				return player;
		}
		return false;
	}
	this.GetAlias=function(color)
	{
		var playerfullname=this.players[color].id;
		if(playerfullname)
		{
			return(chessplayers.players[playerfullname].name);
		}
		return "[Empty Seat]";
	}
	this.NextTurn=function()
	{
		this.turn=(this.turn=="white"?"black":"white");
	}
	this.SetFileInfo=function()
	{
		var gNum=1;
		if(gNum<10) gNum="0"+gNum;
		while(file_exists(chessroot+gNum+".chs"))
		{
			gNum++;
			if(gNum<10) gNum="0"+gNum;
		}
		var fName=chessroot + gNum + ".chs";
		this.gamefile=new File(fName);
		this.gamenumber=parseInt(gNum,10);
	}
	this.StoreGame=function()
	{
		//STORE GAME DATA
		Log("Storing game " + this.gamenumber);
		var gFile=this.gamefile;
		gFile.open("w+");
		var wplayer;
		var bplayer;
		for(player in this.players)
		{
			gFile.iniSetValue(null,player,this.players[player].id);
		}
		gFile.iniSetValue(null,"gamenumber",this.gamenumber);
		gFile.iniSetValue(null,"turn",this.turn);

		if(this.started) gFile.iniSetValue(null,"started",this.started);
		if(this.finished) gFile.iniSetValue(null,"finished",this.finished);
		if(this.rated) gFile.iniSetValue(null,"rated",this.rated);
		if(this.timed) gFile.iniSetValue(null,"timed",this.timed);
		if(this.password) gFile.iniSetValue(null,"password",this.password);
		if(this.minrating) gFile.iniSetValue(null,"minrating",this.minrating);
		if(this.board.lastmove) 
		{
			gFile.iniSetValue("lastmove","from",GetChessPosition(this.board.lastmove.from));
			gFile.iniSetValue("lastmove","to",GetChessPosition(this.board.lastmove.to));
		}
		if(this.draw)
		{
			gFile.iniSetValue(null,"draw",this.draw);
		}
		for(x in this.board.grid)
		{
			for(y in this.board.grid[x])
			{
				var contents=this.board.grid[x][y].contents;
				if(contents)
				{
					var position=GetChessPosition({'x':x,'y':y});
					var section="board."+position;
					gFile.iniSetValue(section,"piece",contents.name);
					gFile.iniSetValue(section,"color",contents.color);
					if(contents.has_moved) gFile.iniSetValue(section,"hasmoved",contents.has_moved);
				}
			}
		}
		for(move in this.movelist)
		{
			gFile.iniSetValue("move." + move,"from",GetChessPosition(this.movelist[move].from));
			gFile.iniSetValue("move." + move,"to",GetChessPosition(this.movelist[move].to));
			gFile.iniSetValue("move." + move,"color",this.movelist[move].color);
			if(this.movelist[move].check) gFile.iniSetValue("move." + move,"check",this.movelist[move].check);
		}
		this.gamefile.close();
	}
	this.LoadGameTable=function()
	{
		//LOAD GAME TABLE - BASIC DATA
		var gFile=this.gamefile;
		this.lastupdated=file_date(gFile.name);
		if(!file_exists(gFile.name)) return false;
		gFile.open("r");
		
		var wp=gFile.iniGetValue(null,"white");
		var bp=gFile.iniGetValue(null,"black");
		
		this.players.white=wp?{"id":wp}:new Object();
		this.players.black=bp?{"id":bp}:new Object();
		
		this.gamenumber=parseInt(gFile.iniGetValue(null,"gamenumber"),10);
		this.turn=gFile.iniGetValue(null,"turn");
		this.started=gFile.iniGetValue(null,"started");
		this.finished=gFile.iniGetValue(null,"finished");
		this.rated=gFile.iniGetValue(null,"rated");
		this.timed=gFile.iniGetValue(null,"timed");
		this.password=gFile.iniGetValue(null,"password");
		this.minrating=gFile.iniGetValue(null,"minrating");
		
		if(this.timed)
		{
			this.players.white.timer=new ChessTimer(this.timed,"white");
			this.players.black.timer=new ChessTimer(this.timed,"black");
		}
		
		this.currentplayer=this.PlayerInGame();
		gFile.close();
	}
	this.LoadGameData=function()
	{
		var gFile=this.gamefile;
		if(!file_exists(gFile.name)) return false;
		gFile.open("r");
		Log("Reading data from file: " + gFile.name);
		
		this.movelist=[];
		var lastmove=gFile.iniGetObject("lastmove");
		if(lastmove.from)
		{
			this.board.lastmove=new ChessMove(GetChessPosition(lastmove.from),GetChessPosition(lastmove.to));
		}
		var draw=gFile.iniGetValue(null,"draw");
		if(draw)
		{
			this.draw=draw;
		}
		
		//LOAD PIECES
		var pieces=gFile.iniGetAllObjects("position","board.");
		for(p in pieces)
		{
			var pos=GetChessPosition(pieces[p].position);
			var name=pieces[p].piece;
			var color=pieces[p].color;
			this.board.grid[pos.x][pos.y].AddPiece(new ChessPiece(name,color));
		}
		//LOAD MOVES
		var moves=gFile.iniGetAllObjects("number","move.");
		for(move in moves)
		{
			var from=GetChessPosition(moves[move].from);
			var to=GetChessPosition(moves[move].to);
			var color=moves[move].color;
			var check=moves[move].check;
			this.movelist.push(new ChessMove(from,to,color,check));
		}
		gFile.close();
	}
	this.NotifyPlayer=function()
	{
		var nextturn=this.players[this.turn];
		if(!chesschat.FindUser(nextturn.id))
		{
			var uname=chessplayers.players[nextturn.id].name;
			var unum=system.matchuser(uname);
			var message="\1n\1yIt is your turn in \1hChess\1n\1y game #\1h" + this.gamenumber + "\r\n\r\n";
			system.put_telegram(unum, message);
			//TODO: make this handle interbbs games if possible
		}
	}
	this.NewGame=function()
	{
		this.board=new ChessBoard();
		this.board.ResetBoard();
		this.players.white=new Object();
		this.players.black=new Object();
		this.started=false;
		this.finished=false;
		this.movelist=[];
		this.turn="";
	}
	this.Start=function(current)
	{
		Log("Starting Game: " + this.gamenumber);
		this.currentplayer=current;
		this.started=true;
		this.turn="white";
		this.board.side=current;
		this.StoreGame();
	}
	this.Init();
}
function ChessMove(from,to,color,check)
{
	this.from=from;
	this.to=to;
	this.color=color;
	this.check=check;
	this.Format=function()
	{
		var color=(this.color=="white"?"\1w\1h":"\1k\1h");
		var prefix=(this.color=="white"?"W":"B");
		var text=color + prefix + "\1n\1c: " +color+ GetChessPosition(this.from) + "\1n\1c-" + color + GetChessPosition(this.to);
		return text;
	}
}
function ChessPiece(name,color)
{
	this.name=name;
	this.color=color;
	//this.fg;
	//this.has_moved;
	//this.small;
	//this.large;
	
	this.Draw=function(bg,x,y,large)
	{
		var graphic=large?this.large:this.small;
		for(offset=0;offset<graphic.length;offset++) {
			console.gotoxy(x,y+offset); console.print(this.fg + bg + graphic[offset]);
		}
	}
	this.Init=function()
	{
		this.fg=(this.color=="white"?"\1w\1h":"\1n\1k");
		var base=" \xDF\xDF\xDF ";
		switch(this.name)
		{
			case "pawn":
				this.small=["  \xF9  ","  \xDB  ",base];
				this.large=["         ","         ","    \xFE    ","    \xDB    ","   \xDF\xDF\xDF   "];
				break;
			case "rook":
				this.small=[" \xDC \xDC "," \xDE\xDB\xDD ",base];
				this.large=["         ","  \xDB\xDC\xDB\xDC\xDB  ","  \xDE\xDD\xDB\xDB\xDD  ","  \xDE\xDB\xDB\xDE\xDD  ","  \xDF\xDF\xDF\xDF\xDF  "];
				break;
			case "knight":
				this.small=["  \xDC  "," \xDF\xDB\xDD ",base];
				//this.large=["   \xDC\xDC    ","  \xDF\xDF\xDB\xDD   ","   \xDE\xDB\xDD   ","   \xDC\xDB\xDC   ","  \xDF\xDF\xDF\xDF\xDF  "];
				this.large=["    \xDC    ","   \xDF\xDB\xDD   ","   \xDE\xDB\xDD   ","   \xDC\xDB\xDC   ","  \xDF\xDF\xDF\xDF\xDF  "];
				break;
			case "bishop":
				this.small=["  \x06  ","  \xDB  ",base];
				this.large=["    \xDC    ","   \xDE\xDE\xDD   ","    \xDB    ","   \xDE\xDB\xDD   ","   \xDF\xDF\xDF   "];
				break;
			case "queen":
				this.small=[" <Q> ","  \xDB  ",base];
				this.large=["   \xF2\xF3   ","   \xDE\xDB\xDD   ","    \xDB    ","   \xDE\xDB\xDD   ","   \xDF\xDF\xDF   "];
				break;
			case "king":
				this.small=[" \xF3\xF1\xF2 ","  \xDB  ",base];
				this.large=["    \xC5    ","   \xDE\xDB\xDD   ","    \xDB    ","   \xDE\xDB\xDD   ","   \xDF\xDF\xDF   "];
				break;
			default:
				this.small=["\1r\1hERR"];
				this.large=["\1r\1hERR"];
				break;
		}
	}
	this.Init();
}
function ChessBoard()
{
	this.lastmove=false;
	//this.tempfrom;
	//this.tempto;
	this.gridpositions;
	//this.grid;
	this.side="white";
	this.large=false;
	this.background=new Graphic(80,50);
	
	this.Init=function()
	{
		this.LoadTiles();
		this.background.load(chessroot + "largebg.bin");
	}
	this.Reverse=function()
	{
		this.side=(this.side=="white"?"black":"white");
	}
	this.DrawBlinking=function(x,y,selected,color)
	{
		this.grid[x][y].DrawBlinking(this.side,selected,color,this.large);
		console.attributes=ANSI_NORMAL;
		console.gotoxy(79,24);
	}
	this.DrawTile=function(x,y,selected,color)
	{
		this.grid[x][y].Draw(this.side,selected,color,this.large);
		console.attributes=ANSI_NORMAL;
		console.gotoxy(79,24);
	}
	this.LoadTiles=function()
	{
		this.grid=new Array(8);
		for(x=0;x<this.grid.length;x++) 
		{
			this.grid[x]=new Array(8);
			for(y=0;y<this.grid[x].length;y++) 
			{
				if((x+y)%2===0) 	var color="white"; //SET TILE COLOR
				else 				var color="black";
				this.grid[x][y]=new ChessTile(color,x,y);
			}
		}
	}
	this.FindKing=function(color)
	{
		for(column in this.grid)
		{
			for(row in this.grid[column])
			{
				var position=this.grid[column][row];
				if(position.contents && position.contents.name=="king" && position.contents.color==color)
				{
					return position;
				}
			}
		}
	}
	this.FindAllPieces=function(color)
	{
		var pieces=[];
		for(column in this.grid)
		{
			for(row in this.grid[column])
			{
				var position=this.grid[column][row];
				if(position.contents && position.contents.color==color)
				{
					pieces.push(position);
				}
			}
		}
		return(pieces);
	}
	this.ClearLastMove=function()
	{
		if(this.lastmove) {
			Log("Clearing last move");
			var from=this.lastmove.from;
			var to=this.lastmove.to;
			this.DrawTile(from.x,from.y,false,false,this.large);
			this.DrawTile(to.x,to.y,false,false,this.large);
		}
	}
	this.DrawLastMove=function()
	{
		if(this.lastmove) {
			Log("Drawing last move");
			var from=this.lastmove.from;
			var to=this.lastmove.to;
			this.DrawTile(from.x,from.y,true,"\1r\1h",this.large);
			this.DrawTile(to.x,to.y,true,"\1r\1h",this.large);
		}
	}
	this.DrawBoard=function()
	{
		if(this.large) this.background.draw();
		for(x=0;x<this.grid.length;x++) 
		{
			for(y=0;y<this.grid[x].length;y++)
			{
				this.grid[x][y].Draw(this.side,false,false,this.large);
			}
		}
		this.DrawLastMove();
		console.gotoxy(79,24);
		console.attributes=ANSI_NORMAL;
	}
	this.Move=function(from,to)
	{
 		var from_tile=this.grid[from.x][from.y];
		var to_tile=this.grid[to.x][to.y];
		this.tempfrom=from_tile.contents;
		this.tempto=to_tile.contents;
		to_tile.AddPiece(from_tile.contents);
		to_tile.has_moved=true;
		from_tile.RemovePiece();
	}
	this.UnMove=function(from,to)
	{
		var from_tile=this.grid[from.x][from.y];
		var to_tile=this.grid[to.x][to.y];
		from_tile.contents=this.tempfrom;
		to_tile.contents=this.tempto;
		delete this.tempfrom;
		delete this.tempto;
	}
	this.GetMoves=function(from)
	{
		var moves=[];
		if(this.name=="pawn") {
			moves.push(from.x,from.y-1);
			if(from.y==6) moves.push(from.x,from.y-2);
			return moves;
		}
	}
	this.LoadPieces=function(color)
	{
		var pawn=new ChessPiece("pawn",color);
		var rook=new ChessPiece("rook",color);
		var knight=new ChessPiece("knight",color);
		var bishop=new ChessPiece("bishop",color);
		var queen=new ChessPiece("queen",color);
		var king=new ChessPiece("king",color);
		var order=[	rook,
					knight,
					bishop,
					queen,
					king,
					bishop,
					knight,
					rook];
		return({'backline':order,'pawn':pawn});
	}
	this.ResetBoard=function()
	{
		var black_set=this.LoadPieces("black");
		var white_set=this.LoadPieces("white");
		for(x=0;x<8;x++) //SET PIECES
		{
			this.grid[x][0].AddPiece(black_set.backline[x]);
			this.grid[x][1].AddPiece(black_set.pawn);
			this.grid[x][6].AddPiece(white_set.pawn);
			this.grid[x][7].AddPiece(white_set.backline[x]);
		}
	}
	this.Init();
}
function ChessTile(color,x,y)
{
	this.x=x;
	this.y=y;
	this.color=color;
	//this.contents;
	this.bg=(this.color=="white"?console.ansi(BG_LIGHTGRAY):console.ansi(BG_CYAN));

	this.AddPiece=function(piece)
	{
		this.contents=piece;
	}
	this.RemovePiece=function()
	{
		delete this.contents;
	}
	this.Draw=function(side,selected,color,large)
	{
		var xinc=large?9:5;
		var yinc=large?5:3;
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*xinc+(large?5:1);
		var y=posy*yinc+(large?3:1);
		
		if(!this.contents) 
		{
			var spaces=large?"         ":"     ";
			var lines=large?5:3;
			for(pos = 0;pos<lines;pos++) {
				console.gotoxy(x,y+pos); write(this.bg+spaces);
			}
		}
		else 
		{
			this.contents.Draw(this.bg,x,y,large);
		}
		if(selected) this.DrawSelected(side,color,large);
	}
	this.DrawBlinking=function(side,selected,color,large)
	{
		var xinc=large?9:5;
		var yinc=large?5:3;
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*xinc+(large?5:1);
		var y=posy*yinc+(large?3:1);
		
		this.contents.Draw(this.bg + console.ansi(BLINK),x,y,large);
		if(selected) this.DrawSelected(side,color,large);
	}
	this.DrawSelected=function(side,color,large)
	{
		color=color?color:"\1n\1b";
		var xinc=large?9:5;
		var yinc=large?5:3;
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*xinc+(large?5:1);
		var y=posy*yinc+(large?3:1);
		
		console.gotoxy(x,y);
		console.putmsg(color + this.bg + "\xDA");
		console.gotoxy(x+(xinc-1),y);
		console.putmsg(color + this.bg + "\xBF");
		console.gotoxy(x,y+(yinc-1));
		console.putmsg(color + this.bg + "\xC0");
		console.gotoxy(x+(xinc-1),y+(yinc-1));
		console.putmsg(color + this.bg + "\xD9");
		console.gotoxy(79,1);
	}
}
function ChessTimer(length,player,turn)
{
	this.player=player;
	this.countdown=length*60;
	this.lastupdate=false;
	this.lastshown=false;
	this.Update=function(length)
	{
		this.countdown=length;
	}
	this.Display=function(turn)
	{
		var color=(turn==this.player?"\1r\1h":"\1k\1h");
		var mins=parseInt(this.countdown/60,10);
		var secs=this.countdown%60;
		printf(color + "Time Remaining: %02d:%02d",mins,secs);
		this.lastshown=time();
	}
	this.Countdown=function(current)
	{
		if(this.countdown<=0) return false;
		this.countdown-=(current-this.lastupdate);
		this.lastupdate=current;
		return true;
	}
}
function GetChessPosition(position)
{
	var letters="abcdefgh";
	if(typeof position=="string")
	{
		//CONVERT BOARD POSITION IDENTIFIER TO XY COORDINATES
		var pos={x:letters.indexOf(position.charAt(0)),y:8-(parseInt(position.charAt(1),10))};
		return(pos);
	}
	else if(typeof position=="object")
	{
		//CONVERT XY COORDINATES TO STANDARD CHESS BOARD FORMAT
		return(letters.charAt(position.x)+(8-position.y));
	}
}
