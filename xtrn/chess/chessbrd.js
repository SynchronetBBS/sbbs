function GameSession(game) 
{
	this.name="Chess Table " + game.gamenumber;
	this.game=game;
	this.board;
	this.queue=chesschat.queue;
	this.menu;
	this.infobar=true; //TOGGLE FOR GAME INFORMATION BAR AT TOP OF CHAT WINDOW (default: true)
	
/*	
	CHAT ENGINE DEPENDENT FUNCTIONS
	
	some of these functions are redundant
	so as to make modification of the 
	method of data transfer between nodes/systems
	simpler
*/
	this.InitGame=function()
	{
		this.game.LoadGameTable();
		this.game.LoadGameData();
		this.board=this.game.board;
		
		currentplayer=this.PlayerInGame();
		this.board.side=(currentplayer?currentplayer:"white");
		this.board.currentplayer=(currentplayer?currentplayer:false);
	}
	this.InitChat=function()
	{
		var rows=(this.infobar?17:20);
		var columns=38;
		var posx=42;
		var posy=(this.infobar?6:3);
		var input_line={'x':42,'y':24,columns:38};
		chesschat.Init(this.name,input_line,columns,rows,posx,posy,true);
	}
	this.InitMenu=function()
	{
		this.menu=new Menu(		chesschat.input_line.x,chesschat.input_line.y,"\1n","\1c\1h");
		var menu_items=[		"~Sit"							, 
								"~Resign"						,
								"~Offer Draw"					,
								"~New Game"						,
								"~Move"							,
								"Re~verse"						,
								"~Help"							,
								"Move ~List"					,
								"Re~draw"						];
		this.menu.add(menu_items);
		this.RefreshMenu();
	}
	this.Main=function()
	{
		this.Redraw();
		while(1)
		{
			this.Cycle();
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			{
				switch(k)
				{
					case "/":
						if(!chesschat.buffer.length) 
						{
							this.RefreshMenu();
							this.ListCommands();
							this.ChessMenu();
						}
						else if(!Chat(k,chesschat)) return;
						break;
					case "\x1b":	
						return;
					case "?":
						if(!chesschat.buffer.length) 
						{
							this.ToggleGameInfo();
						}
						else if(!Chat(k,chesschat)) return;
						break;
					default:
						if(!Chat(k,chesschat)) return;
						break;
				}
			}
		}
	}
	this.ClearChatWindow=function()
	{
		chesschat.ClearChatWindow();
	}
	this.RefreshMenu=function()
	{
		if(this.game.started || this.PlayerInGame()) this.menu.disable(["S"]);
		else this.menu.enable(["S"]);
		if(!this.game.finished || !this.PlayerInGame()) this.menu.disable(["N"]);
		else this.menu.enable(["N"]);
		if(!this.game.started || this.game.turn!=this.PlayerInGame() || this.game.finished) 
		{
			this.menu.disable(["M"]);
			this.menu.disable(["R"]);
			this.menu.disable(["O"]);
		}
		else 
		{
			this.menu.enable(["M"]);
			this.menu.enable(["R"]);
			this.menu.enable(["O"]);
		}
		if(!this.game.movelist.length) this.menu.disable(["L"]);
		else this.menu.enable(["L"]);
	}
	this.ChessMenu=function()
	{	
		this.menu.displayHorizontal();
		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		this.ClearAlert();
		if(k)
		{
			this.ClearChatWindow();
			chesschat.Redraw();
			if(this.menu.items[k] && this.menu.items[k].enabled) 
			{
				switch(k)
				{
					case "R":
						this.Resign();
						break;
					case "S":
						this.JoinGame();
						break;
					case "H":
						this.Help();
						break;
					case "D":
						this.Redraw();
						break;
					case "O":
						this.OfferDraw();
						break;
					case "M":
						this.board.ClearLastMove();
						this.Move();
						break;
					case "V":
						this.Reverse();
						this.Redraw();
						break;
					case "L":
						this.ListMoves();
						break;
					case "N":
						this.Rematch();
						break;
					default:
						break;
				}
				this.ClearChatWindow();
				chesschat.Redraw();
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
			chesschat.Resize(chesschat.x,3,chesschat.columns,20);
		}
		else
		{
			this.infobar=true;
			chesschat.Resize(chesschat.x,6,chesschat.columns,17);
		}
		this.InfoBar();
	}
	this.InfoBar=function()
	{
		console.gotoxy(chesschat.x-1,1);
		console.putmsg("\1k\1h[\1cEsc\1k\1h]\1wQuit \1k\1h[\1c/\1k\1h]\1wMenu \1k\1h[\1c?\1k\1h]\1wToggle \1k\1h[\1c" + ascii(30) + "\1k\1h,\1c" + ascii(31) +"\1k\1h]\1wScroll");
		if(this.infobar)
		{
			var turn=false;
			if(this.game.started && !this.game.finished) turn=this.game.turn;
			chessplayers.FormatPlayer(this.game.players["white"],chesschat.x-1,3,"white",turn);
			chessplayers.FormatStats(this.game.players["white"],chesschat.x+20,3);
			chessplayers.FormatPlayer(this.game.players["black"],chesschat.x-1,4,"black",turn);
			chessplayers.FormatStats(this.game.players["black"],chesschat.x+20,4);
		}
	}
	this.ClearAlert=function()
	{
		chesschat.ClearInputLine();
	}
	this.DisplayInfo=function(array)
	{
		chesschat.DisplayInfo(array);
	}
	this.Alert=function(msg)
	{
		chesschat.Alert(msg);
	}
	this.Cycle=function()
	{
		chesschat.Cycle();
		if(this.queue.DataWaiting("move"))
		{
			var data=this.queue.ReceiveData("move");
			for(move in data)
			{
				this.GetMove(data[move]);
			}
			this.game.NextTurn();
		}
		if(this.queue.DataWaiting("castle"))
		{
			var data=this.queue.ReceiveData("castle");
			for(move in data)
			{
				this.GetMove(data[move]);
			}
		}
		if(this.queue.DataWaiting("update"))
		{
			var trash=this.queue.ReceiveData("update");
			this.game.LoadGameTable();
			this.game.LoadGameData();
		}
	}
	this.Redraw=function()
	{
		console.clear();
		this.board.DrawBoard();
		this.InfoBar();
		chesschat.Redraw();
	}
	this.Send=function(data,name)
	{
		this.queue.SendData(data,name);
	}
	this.Reverse=function()
	{
		this.board.Reverse();
	}
	
/*	GAMEPLAY FUNCTIONS	*/
	this.PlayerInGame=function()
	{
		return this.game.PlayerInGame();
	}
	this.OfferDraw=function()
	{
		//TODO: !!!
	}
	this.Rematch=function()
	{
		//TODO: add rematch feature
	}
	this.Resign=function()
	{
		this.Alert("\1c\1hAre you sure? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
		if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") return;
		this.EndGame("loss");
	}
	this.EndGame=function(outcome)
	{
		var cp=this.game.currentplayer;
		
		var side1=(cp=="white"?"white":"black");
		var side2=(cp=="white"?"black":"white");
		
		var id1=this.game.players[side1].id;
		var id2=this.game.players[side2].id;
		
		var p1=chessplayers.players[id1];
		var p2=chessplayers.players[id2];
		
		var r1=p1.rating;
		var r2=p2.rating;
		
		switch(outcome)
		{
			case "win":
				p1.rating=this.GetRating(p1.rating,p2.rating,"win");
				p1.wins+=1;
				p2.rating=this.GetRating(p2.rating,p1.rating,"loss");
				p2.losses+=1;
				break;
			case "loss":
				p1.rating=this.GetRating(p1.rating,p2.rating,"loss");
				p1.losses+=1;
				p2.rating=this.GetRating(p2.rating,p1.rating,"win");
				p2.wins+=1;
				break;
			case "draw":
				p1.rating=this.GetRating(p1.rating,p2.rating,"draw");
				p1.draws+=1;
				p2.rating=this.GetRating(p2.rating,p1.rating,"draw");
				p2.draws+=1;
				break;
		}
		chessplayers.StorePlayer(id1);
		chessplayers.StorePlayer(id2);
		
		this.game.finished=true;
		file_remove(this.game.gamefile.name);
	}
	this.JoinGame=function()
	{
		if(this.game.players["white"].id) 
		{
			this.game.Start("black");
		}
		else if(this.game.players["black"].id) 
		{
			this.game.Start("white");
		}
		else 
		{
			chesschat.Alert("\1nChoose a side [\1hW\1n,\1hB\1n]: ");
			var color=console.getkeys("\x1bWB");
			switch(color)
			{
				case "W":
					this.game.players["white"].id=user.alias  + chessplayers.suffix;
					break;
				case "B":
					this.game.players["black"].id=user.alias  + chessplayers.suffix;
					break;
				default:
					return;
			}
		}
		this.Send("update","update");
		this.game.StoreGame();
		this.Redraw();
	}
	this.ListMoves=function()
	{
		var list=["\1c\1hMOVE HISTORY"];
		for(m=0;m<this.game.movelist.length;m++)
		{	
			var text=this.game.movelist[m].Format();
			m++;
			if(this.game.movelist[m])
			{
				text+=("  " + this.game.movelist[m].Format());
			}
			list.push(text);
		}
		this.DisplayInfo(list);
		this.Alert("\1r\1hPress a key to continue");
		console.getkey();
	}
	this.GameInfo=function()
	{
	}
	this.GetRating=function(r1,r2,outcome)
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
				points=.5;
				break;
		}
		var expected=1/(1+Math.pow(10,(r2-r1)/400));
		var newrating=Math.round(r1+K*(points-expected));
		return newrating;
	}
	this.Move=function()
	{
		var from;
		var to;
		var placeholder;
		var unselect;
		
		while(1)
		{
			unselect=false;
			while(1)
			{
				from=this.SelectTile(false,from);
				if(from==false) return false;
				else if(	this.board.grid[from.x][from.y].contents && 
							this.board.grid[from.x][from.y].contents.color == this.game.turn) 
					break;
				else 
				{
					this.Alert("\1r\1hInvalid Selection");
				}
			}
			placeholder=from;
			while(1)
			{
				to=this.SelectTile(from,placeholder);
				if(to==false) return false;
				else if(from.x==to.x && from.y==to.y) 
				{
					unselect=true;
					break;
				}
				else if(!this.board.grid[to.x][to.y].contents || 
						this.board.grid[to.x][to.y].contents.color != this.game.turn)
				{
					if(!this.CheckMove(from,to)) 
					{
						placeholder=to;
						this.Alert("\1r\1hIllegal Move");
					}
					else break;
				}
				else 
				{
					placeholder=to;
					this.Alert("\1r\1hInvalid Selection");
				}
			}
			if(!unselect) break;
		}
		var temp_from=this.board.grid[from.x][from.y].contents;
		var temp_to=this.board.grid[to.x][to.y].contents;
		
		if(this.InCheck(this.game.players[this.game.currentplayer].king)) 
		{
			this.board.grid[from.x][from.y].contents=temp_from;
			this.board.grid[to.x][to.y].contents=temp_to;
			return false;
		}
		var move=new ChessMove(from,to,temp_from.color);
		this.game.Move(move);
		this.game.NextTurn();
		this.Send(move,"move");
		this.game.StoreGame();
		return true;
	}
	this.GetMove=function(data)
	{
		var from=data.from;
		var to=data.to;
		Log("coords received: from " + from.x + "," + from.y + " to " + to.x + "," + to.y);
		this.board.Move(data);
		if(this.game.currentplayer)
		{
			if(this.InCheck(this.game.players[this.game.currentplayer].king)) {
				this.Alert("\1r\1hYou are in check!");
			}
			/*
			var checkers=this.InCheck(this.king);
			if(checkers) {
				if(this.FindCheckMate(checkers)) {
					this.Alert("\1r\1hCheckmate! You lose!");
					return true;
				}
			}
			*/
		}
	}
	this.SelectTile=function(start,placeholder)
	{
		if(placeholder) selected={"x":placeholder.x, "y":placeholder.y};
		else if(start) selected={"x":start.x, "y":start.y};
		else
		{
			if(this.board.side=="white") selected={"x":0,"y":7};
			else selected={"x":7,"y":0};
		}
		this.board.DrawTile(selected.x,selected.y,true);
		while(1)
		{
			this.Cycle();
			var key=console.inkey((K_NOECHO,K_NOCRLF,K_UPPER),50);
			if(key=="\r") return selected;
			if(this.board.side=="white" && key)
			{
				this.ClearAlert();
				if(start) this.board.DrawTile(start.x,start.y,true);
				switch(key)
				{
					case KEY_DOWN:
					case "2":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.y==7) selected.y=0;
						else selected.y++;
						break;
					case KEY_UP:
					case "8":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.y==0) selected.y=7;
						else selected.y--;
						break;
					case KEY_LEFT:
					case "4":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==0) selected.x=7;
						else selected.x--;
						break;
					case KEY_RIGHT:
					case "6":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==7) selected.x=0;
						else selected.x++;
						break;
					case "7":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==0) selected.x=7;
						else selected.x--;
						if(selected.y==0) selected.y=7;
						else selected.y--;
						break;
					case "9":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==7) selected.x=0;
						else selected.x++;
						if(selected.y==0) selected.y=7;
						else selected.y--;
						break;
					case "1":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==0) selected.x=7;
						else selected.x--;
						if(selected.y==7) selected.y=0;
						else selected.y++;
						break;
					case "3":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==7) selected.x=0;
						else selected.x++;
						if(selected.y==7) selected.y=0;
						else selected.y++;
						break;
					case "\x1B":
						if(start) this.board.DrawTile(start.x,start.y);
						this.board.DrawTile(selected.x,selected.y);
						return false;
					default:
						continue;
				}
				this.board.DrawTile(selected.x,selected.y,true);
			}
			else
			{
				this.ClearAlert();
				if(start) this.board.DrawTile(start.x,start.y,true);
				switch(key)
				{
					case KEY_UP:
					case "2":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.y==7) selected.y=0;
						else selected.y++;
						break;
					case KEY_DOWN:
					case "8":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.y==0) selected.y=7;
						else selected.y--;
						break;
					case KEY_RIGHT:
					case "4":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==0) selected.x=7;
						else selected.x--;
						break;
					case KEY_LEFT:
					case "6":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==7) selected.x=0;
						else selected.x++;
						break;
					case "3":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==0) selected.x=7;
						else selected.x--;
						if(selected.y==0) selected.y=7;
						else selected.y--;
						break;
					case "1":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==7) selected.x=0;
						else selected.x++;
						if(selected.y==0) selected.y=7;
						else selected.y--;
						break;
					case "9":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==0) selected.x=7;
						else selected.x--;
						if(selected.y==7) selected.y=0;
						else selected.y++;
						break;
					case "7":
						this.board.DrawTile(selected.x,selected.y);
						if(selected.x==7) selected.x=0;
						else selected.x++;
						if(selected.y==7) selected.y=0;
						else selected.y++;
						break;
					case "\x1B":
						if(start) this.board.DrawTile(start.x,start.y);
						this.board.DrawTile(selected.x,selected.y);
						return false;
					default:
						continue;
				}
				this.board.DrawTile(selected.x,selected.y,true);
			}
		}
	}
	this.CheckMove=function(from,to)
	{
		var from_tile=this.board.grid[from.x][from.y];
		var to_tile=this.board.grid[to.x][to.y];
		if(to.x==from.x && to.y==from.y) 
		{
			Log("Invalid Move: Target Position same as Source Position")
			return false;
		}
		if(to_tile.contents && to_tile.contents.color==from_tile.contents.color) {
			Log("Invalid Move: Target Color same as Source Color");
			return false
		}
		
		//KING RULESET
		if(from_tile.contents.name=="king") 
		{
			if(this.InCheck(to)) 
			{
				Log("Invalid Move: Would cause check");
				return false;
			}
			else if(Math.abs(from.x-to.x)==2) 
			{
				if(this.InCheck(from))
				{
					Log("Invalid Move: King is in check");
					return false;
				}
				if(from_tile.contents.has_moved) 
				{
					Log("Invalid Move: King has already moved and cannot castle");
					return false;
				}
				if(from.x > to.x) {
					var x=from.x-1;
					while(x>0) {
						var spot_check={"x":x,"y":from.y};
						if(this.board.grid[x][from.y].contents) return false;
						if(this.InCheck(spot_check)) return false;
						x--;
					}
				}
				else {
					var x=from.x+1;
					while(x<7) {
						var spot_check={"x":x,"y":from.y};
						if(this.board.grid[x][from.y].contents) return false;
						if(this.InCheck(spot_check)) return false;
						x++;
					}
				}
				this.Castle(from,to);
			}
			else {
				if(Math.abs(from.x-to.x)>1 || Math.abs(to.x-to.y)>1) 
				{
					Log("Invalid Move: King can only move one space unless castling");
					return false;
				}
			}
			this.game.players[this.game.currentplayer].king=to;
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
						if(from.y!=6) return false;
						if(this.board.grid[from.x][from.y-1].contents) return false;
					}
					if(to.y==0) this.board.grid[from.x][from.y].contents=new ChessPiece("queen",this.board.grid[from.x][from.y].contents.color);
					if(xgap==ygap && !to_tile.contents)
					{
						//TODO: ADD RULES FOR EN PASSANT
						if(to.y==2) return false; //SHOULD BE TRUE FOR EN PASSANT
						else return false;
					}
					break;
				case "black":
					if(from.y>to.y) return false; //CANNOT MOVE BACKWARDS
					if(ygap==2)
					{
						if(from.y!=1) return false;
						if(this.board.grid[from.x][from.y+1].contents) return false;
					}
					if(to.y==7) this.board.grid[from.x][from.y].contents=new ChessPiece("queen",this.board.grid[from.x][from.y].contents.color);
					if(xgap==ygap && !to_tile.contents)
					{
						//TODO: ADD RULES FOR EN PASSANT
						if(to.y==5) return false; //SHOULD BE TRUE FOR EN PASSANT
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
						if(this.board.grid[from.x][from.y+check].contents) return false;
					}
				}
				else 
				{
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) 
					{
						if(this.board.grid[from.x][from.y-check].contents) return false;
					}
				}
			}
			else if(from.y==to.y) {
				if(from.x<to.x) {
					var distance=to.x-from.x;
					for(check = 1;check<distance;check++) {
						if(this.board.grid[from.x+check][from.y].contents) return false;
					}
				}
				else {
					var distance=from.x-to.x;
					for(check = 1;check<distance;check++) {
						if(this.board.grid[from.x-check][from.y].contents) return false;
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
					if(this.board.grid[checkx][checky].contents) return false;
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
					if(this.board.grid[checkx][checky].contents) return false;
				}
			}
			else if(from.x==to.x) 
			{
				if(from.y<to.y) 
				{
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) 
					{
						if(this.board.grid[from.x][parseInt(from.y)+check].contents) return false;
					}
				}
				else 
				{
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) 
					{
						if(this.board.grid[from.x][from.y-check].contents) return false;
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
						if(this.board.grid[parseInt(from.x)+check][from.y].contents) return false;
					}
				}
				else 
				{
					var distance=from.x-to.x;
					for(check = 1;check<distance;check++) 
					{
						if(this.board.grid[parseInt(from.x)-check][from.y].contents) return false;
					}
				}
			}
			else return false;
		}

		return true;
	}
	this.Castle=function(from,to)
	{
		var rfrom;
		var rto;
		if(from.x-to.x==2) 
		{	//QUEENSIDE
			var rfrom={'x':to.x-2,'y':to.y};
			var rto={'x':to.x+1,'y':to.y};
			
		}
		else 
		{	//KINGSIDE
			var rfrom={'x':to.x+1,'y':to.y};
			var rto={'x':to.x-1,'y':to.y};
		}
		var move=new ChessMove(rfrom,rto);
		this.board.Move(move);
		this.Send(move,"castle");
	}
	this.FindCheckMate=function(checkers)
	{
		var checkmate=false;
		if(this.KingCanMove) return false;
		for(checker in checkers) {
			var spot=checker[checkers];
			var attacker=this.board.grid[spot.x][spot.y];
			if(attacker.name=="knight") {
				if(!this.CanMoveTo(spot)) checkmate=true;
			}
		}
		return checkmate;
	}
	this.CanMoveTo=function(position)
	{
		var take_pieces=[];
		Log("checking if player can take piece that causes check");
		for(column in this.board.grid) { 
			for(row in this.board.grid[column]) {
				if(this.board.grid[column][row].contents)
				{
					if(this.board.grid[column][row].contents.color==this.color) {
						Log(	"checking piece: " + 	this.board.grid[column][row].contents.color + " " +  
																this.board.grid[column][row].contents.name +
											" on " + column + "," + row);
						var from={"x":column, "y":row};
						if(this.CheckMove(from,position)) {
							take_pieces.push(from);
						}
					}
				}
			}
		}
		if(take_pieces.length) return take_pieces;
		else {
			return false;
		}
	}
	this.KingCanMove=function()
	{
		var position=this.king;
		
		var north=		{"x":position.x,	"y":position.y-1};
		var northeast=	{"x":position.x+1,	"y":position.y-1};
		var northwest=	{"x":position.x-1,	"y":position.y-1};
		var south=		{"x":position.x,	"y":position.y+1};
		var southeast=	{"x":position.x+1,	"y":position.y+1};
		var southwest=	{"x":position.x-1,	"y":position.y+1};
		var east=		{"x":position.x+1,	"y":position.y};
		var west=		{"x":position.x-1,	"y":position.y};
		
		var directions=[north,northeast,northwest,south,southeast,southwest,east,west];
		
		for(dir in directions) {
			if(	directions[dir].x>=0 && 
				directions[dir].x<=7 &&
				directions[dir].y>=0 &&
				directions[dir].y<=7 ) 
			{
				if(!this.CheckRules(position,directions[dir])) {
					Log("king can move");
					return true;
				}
			}
		}
		Log("king cannot move");
		return false;
	}
	this.InCheck=function(position)
	{
		var check_pieces=[];
		Log("Scanning for check");
		for(column in this.board.grid) { 
			for(row in this.board.grid[column]) {
				if(this.board.grid[column][row].contents)
				{
					if(this.board.grid[column][row].contents.name=="king");
					else if(this.board.grid[column][row].contents.color!=this.game.currentplayer) {
						//CHECK EVERY SQUARE ON THE BOARD FOR OPPONENT"S PIECES, TO SEE IF THEY HAVE THIS PLAYER"S KING IN CHECK
						//THERE HAS TO BE A BETTER WAY, BECAUSE THIS SUCKS
						var from={"x":column, "y":row};
						if(this.CheckMove(from,position)) {
							Log("Player is in check");
							this.game.players[this.game.currentplayer].in_check=true;
							check_pieces.push(from);
						}
					}
				}
			}
		}
		if(check_pieces.length) return check_pieces;
		else {
			this.game.players[this.game.currentplayer].in_check=false;
			return false;
		}
	}

	this.InitGame();
	this.InitChat();
	this.InitMenu();
	this.Main();
}
function ChessGame(gamefile)
{
	this.board=new ChessBoard();
	this.players=[];
	this.movelist=[];
	this.gamefile;		
	this.gamenumber;
	this.currentplayer;
	this.turn;
	this.started;
	this.finished;
	this.rated;
	this.timed;
	
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
			this.NewGame();
		}
		this.currentplayer=this.PlayerInGame();
	}
	this.Move=function(move)
	{
		this.board.Move(move);
		this.movelist.push(move);
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
		this.gamenumber=gNum;
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
		if(this.board.lastmove) 
		{
			gFile.iniSetValue("lastmove","from",GetChessPosition(this.board.lastmove.from));
			gFile.iniSetValue("lastmove","to",GetChessPosition(this.board.lastmove.to));
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
		gFile.open("r");
		
		var wp=gFile.iniGetValue(null,"white");
		var bp=gFile.iniGetValue(null,"black");
		
		this.players["white"]=wp?{"id":wp}:new Object();
		this.players["black"]=bp?{"id":bp}:new Object();
		
		this.gamenumber=parseInt(gFile.iniGetValue(null,"gamenumber"),10);
		this.turn=gFile.iniGetValue(null,"turn");
		this.started=gFile.iniGetValue(null,"started");
		this.finished=gFile.iniGetValue(null,"finished");
		this.rated=gFile.iniGetValue(null,"rated");
		this.timed=gFile.iniGetValue(null,"timed");
		
		gFile.close();
	}
	this.LoadGameData=function()
	{
		var gFile=this.gamefile;
		gFile.open("r");
		Log("Reading data from file: " + gFile.name);
		
		var lastmove=gFile.iniGetObject("lastmove");
		if(lastmove.from)
		{
			this.board.lastmove=new ChessMove(GetChessPosition(lastmove.from),GetChessPosition(lastmove.to));
		}
		
		//LOAD PIECES
		var pieces=gFile.iniGetAllObjects("position","board.");
		for(p in pieces)
		{
			var pos=GetChessPosition(pieces[p].position);
			var name=pieces[p].piece;
			var color=pieces[p].color;
			
			//TODO:  BE SURE TO UPDATE this.players[color].king EVERY TIME A KING MOVES
			if(name=="king") this.players[color].king=pos;
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
	this.NewGame=function()
	{
		this.SetFileInfo();
		this.players["white"]=new Object();
		this.players["black"]=new Object();
		this.board.ResetBoard();
	}
	this.Start=function(current)
	{
		Log("Starting Game: " + this.gamenumber);
		this.players[current].id=user.alias + chessplayers.suffix;
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
	this.fg;
	this.has_moved;
	this.graphic;
	
	this.Draw=function(bg,x,y)
	{
		for(offset=0;offset<3;offset++) {
			console.gotoxy(x,y+offset); console.print(this.fg + bg + this.graphic[offset]);
		}
	}
	this.Init=function()
	{
		this.fg=(this.color=="white"?"\1n\1h":"\1n\1k");
		var base=" \xDF\xDF\xDF ";
		switch(this.name)
		{
			case "pawn":
				this.graphic=["  \xF9  ","  \xDB  ",base];
				break;
			case "rook":
				this.graphic=[" \xDC \xDC "," \xDE\xDB\xDD ",base];
				break;
			case "knight":
				this.graphic=["  \xDC  "," \xDF\xDB\xDD ",base];
				break;
			case "bishop":
				this.graphic=["  \x06  ","  \xDB  ",base];
				break;
			case "queen":
				this.graphic=[" <Q> ","  \xDB  ",base];
				break;
			case "king":
				this.graphic=[" \xF3\xF1\xF2 ","  \xDB  ",base];
				break;
			default:
				this.graphic=["","\1r\1h ERR ",""];
				break;
		}
	}
	this.Init();
}
function ChessBoard()
{
	this.lastmove=false;
	this.gridpositions;
	this.grid;
	this.side="white";
	this.Init=function()
	{
		this.LoadTiles();
	}
	this.Reverse=function()
	{
		this.side=(this.side=="white"?"black":"white");
	}
	this.DrawTile=function(x,y,selected)
	{
		this.grid[x][y].Draw(this.side,selected);
		console.gotoxy(79,24);
		write(console.ansi(ANSI_NORMAL));
	}
	this.LoadTiles=function()
	{
		this.grid=new Array(8);
		for(x=0;x<this.grid.length;x++) 
		{
			this.grid[x]=new Array(8);
			for(y=0;y<this.grid[x].length;y++) 
			{
				if((x+y)%2==0) 	var color="white"; //SET TILE COLOR
				else 				var color="black";
				this.grid[x][y]=new ChessTile(color,x,y);
			}
		}
	}
	this.ClearLastMove=function()
	{
		if(this.lastmove) {
			Log("Clearing last move");
			var from=this.lastmove.from;
			var to=this.lastmove.to;
			this.DrawTile(from.x,from.y);
			this.DrawTile(to.x,to.y);
		}
	}
	this.DrawLastMove=function()
	{
		if(this.lastmove) {
			Log("Drawing last move");
			var from=this.lastmove.from;
			var to=this.lastmove.to;
			this.DrawTile(from.x,from.y,true);
			this.DrawTile(to.x,to.y,true);
		}
	}
	this.DrawBoard=function()
	{
		for(x=0;x<this.grid.length;x++) 
		{
			for(y=0;y<this.grid[x].length;y++)
			{
				this.DrawTile(x,y);
			}
		}
		this.DrawLastMove();
	}
	this.Move=function(move)
	{
		var from=move.from;
		var to=move.to;
		this.ClearLastMove();
		var from_tile=this.grid[from.x][from.y];
		var to_tile=this.grid[to.x][to.y];
		Log(from_tile.contents.color + " " + from_tile.contents.name + " moved from " + from.x + "," + from.y + " to " + to.x + "," + to.y);
		to_tile.AddPiece(from_tile.contents);
		from_tile.RemovePiece();
		to_tile.contents.has_moved=true;
		this.DrawTile(from.x,from.y,true);
		this.DrawTile(to.x,to.y,true);
		this.lastmove=new ChessMove(from,to);
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
	this.contents;
	this.bg=(this.color=="white"?console.ansi(BG_LIGHTGRAY):console.ansi(BG_CYAN));

	this.AddPiece=function(piece)
	{
		this.contents=piece;
	}
	this.RemovePiece=function()
	{
		delete this.contents;
	}
	this.Draw=function(side,selected)
	{
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*5+1;
		var y=posy*3+1;
		
		if(!this.contents) 
		{
			for(pos = 0;pos<3;pos++) {
				console.gotoxy(x,y+pos); write(this.bg+"     ");
			}
		}
		else 
		{
			this.contents.Draw(this.bg,x,y);
		}
		if(selected==true) this.DrawSelected(side);
	}
	this.DrawSelected=function(side)
	{
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*5+1;
		var y=posy*3+1;
		
		console.gotoxy(x,y);
		console.putmsg("\1r\1h" + this.bg + "\xDA");
		console.gotoxy(x+4,y);
		console.putmsg("\1r\1h" + this.bg + "\xBF");
		console.gotoxy(x,y+2);
		console.putmsg("\1r\1h" + this.bg + "\xC0");
		console.gotoxy(x+4,y+2);
		console.putmsg("\1r\1h" + this.bg + "\xD9");
		console.gotoxy(79,1);
	}
}
function GetChessPosition(position)
{
	var letters="abcdefgh";
	if(typeof position=="string")
	{
		//CONVERT BOARD POSITION IDENTIFIER TO XY COORDINATES
		var position={x:letters.indexOf(position.charAt(0)),y:8-(parseInt(position.charAt(1)))};
		return(position);
	}
	else if(typeof position=="object")
	{
		//CONVERT XY COORDINATES TO STANDARD CHESS BOARD FORMAT
		return(letters.charAt(position.x)+(8-position.y));
	}
}
