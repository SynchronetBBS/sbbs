function GameSession(game) 
{
	this.name="Chess Table " + game.gamenumber;
	this.game=game;
	this.board;
	this.queue;
	
	this.Init=function()
	{
		//LOAD GAME DATA
		this.game.LoadGameData();
		this.board=this.game.board;

		//INITIALIZE CHAT 
		var rows=22;
		var columns=37;
		var posx=42;
		var posy=1;
		var input_line={x:42,y:24,columns:37};
		this.queue=chesschat.queue;
		chesschat.Init(this.name,input_line,columns,rows,posx,posy,true);
	}
	this.Main=function()
	{
		this.Redraw();
		while(1)
		{
			this.Cycle();
			/*
			if(this.InCheck(this.king)) {
				chesschat.Alert("\1r\1hYou are in check!");
				chesschat.ClearLine();
			}
			var checkers=this.InCheck(this.king);
			if(checkers) {
				if(this.FindCheckMate(checkers)) {
					chesschat.Alert("\1r\1hCheckmate! You lose!");
					return true;
				}
			}
			*/
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO,5);
			if(k)
			switch(k)
			{
				case KEY_UP:
				case KEY_DOWN:
				case KEY_LEFT:
				case KEY_RIGHT:
					this.Move();
					break;
				case "/":
					this.ChessMenu();
					break;
				case "\x1b":	
					return;
				case "?":
					this.Help();
					break;
				default:
					Chat(chesschat,k);
					break;
			}
		}
	}
	this.ChessMenu=function()
	{
		var x=chesschat.input_line.x;
		var y=chesschat.input_line.y;
		
		var cmenu=new Menu(		""								,x,y,"\1n","\1c\1h");
		var cmenu_items=[		"~Sit "							, 
								"~Resign "						,
								"Re~match "						,
								"~Game Info "					,
								"Move ~List "					,
								"Re~draw"						];
		cmenu.add(cmenu_items);
		if(this.game.started || this.game.currentplayer) cmenu.disable(["S"]);
		if(!this.game.started || !this.game.currentplayer) cmenu.disable(["R"]);
		if(!this.game.finished || !this.game.currentplayer) cmenu.disable(["M"]);
		if(!this.game.movelist.length) cmenu.disable(["L"]);
		cmenu.displayHorizontal();

		var k=console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER);
		chesschat.ClearInputLine();
		if(cmenu.items[k] && cmenu.items[k].enabled)
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
			case "G":
				this.GameInfo();
				break;
			case "L":
				this.ListMoves();
				break;
			case "M":
				this.Rematch();
				break;
			default:
				break;
		}
		else Log("Invalid or Disabled key pressed: " + k);
		delete cmenu;
	}

/**	GAMEPLAY FUNCTIONS	*/
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
	}
	this.Rematch=function()
	{
	}
	this.Resign=function()
	{
	}
	this.JoinGame=function()
	{
		Log("Player: " + user.alias + " joining game: " + game.gamenumber);
		var color;
		if(this.game.players["white"].player)
		{
			color="black";
		}
		else color="white";
		this.game.currentplayer=color;
		this.game.players[color].player=chessplayers.current;
		this.game.started=true;
		this.game.StoreGame();
	}
	this.ListMoves=function()
	{
	}
	this.Help=function()
	{
	}
	this.GameInfo=function()
	{
	}
	this.GetRating=function(r1,r2,outcome)
	{
		//OUTCOME SHOULD BE RELATIVE TO PLAYER ONE (R1 = PLAYER ONE RATING)
		//WHERE PLAYER 1 IS THE USER RUNNING THE CURRENT INSTANCE OF THE SCRIPT
		//BASED ON "ELO" RATING SYSTEM: http://en.wikipedia.org/wiki/ELO_rating_system#Mathematical_details
		
		var K=(r1>=2400?16:32); //USE K-FACTOR OF 16 FOR PLAYERS RATED 2400 OR GREATER
		var points;
		switch(outcome.toUpperCase())
		{
			case "WIN":
				points=1;
				break;
			case "LOSE":
				points=0;
				break;
			case "DRAW":
				points=.5;
				break;
		}
		var expected=1/(1+Math.pow(10,(r2-r1)/400));
		var newrating=Math.round(r1+K*(points-expected));
		Log("Expected score: " + expected);
		Log("New Rating: " + newrating);
		
		return newrating;
	}
	this.Redraw=function()
	{
		console.clear();
		this.board.DrawBoard();
		chesschat.Redraw();
	}
	this.Move=function()
	{
		if(!this.game.started || !this.game.currentplayer || this.game.turn!=this.game.currentplayer) return false;
		var from=this.SelectTile();
		if(!from || !this.board.grid[from.x][from.y].contents) return false;
		var to=this.SelectTile(from);
		
		if(!to) return false;
		else {
			var temp_from=this.board.grid[from.x][from.y].contents;
			var temp_to=this.board.grid[to.x][to.y].contents;
			if(!this.CheckRules(from,to)) return false;
			if(this.InCheck(this.game.players[this.game.currentplayer].king)) 
			{
				chesschat.Alert("\1r\1hIllegal Move");
				this.board.grid[from.x][from.y].contents=temp_from;
				this.board.grid[to.x][to.y].contents=temp_to;
				return false;
			}
		}
		this.board.DrawMove(from,to);
		var move=new ChessMove(from,to);
		this.game.movelist.push(move);
		this.game.NextTurn();
		this.game.StoreGame();
		this.queue.SendData(move,"move");
		return true;
	}
	this.GetCastle=function(from,to)
	{
		if(to.x==1 || to.x==2) {
			this.board.grid[to.x+1][0].contents=this.board.grid[0][0].contents;
			this.board.grid[0][0].contents=false;
			//this.board.DrawTile(0,0);
			//this.board.DrawTile(to.x+1,0);
		}
		else {
			this.board.grid[to.x-1][0].contents=this.board.grid[7][0].contents;
			this.board.grid[7][0].contents=false;
			//this.board.DrawTile(7,0);
			//this.board.DrawTile(to.x-1,0);
		}
	}
	this.GetMove=function(data)
	{
		var from=data.from;
		var to=data.to;
		Log("coords received: from " + from.x + "," + from.y + " to " + to.x + "," + to.y);
		this.board.grid[to.x][to.y].contents=this.board.grid[from.x][from.y].contents;
		this.board.grid[from.x][from.y].contents=false;
		this.board.DrawMove(from,to);
		if(this.InCheck(this.game.players[this.game.currentplayer].king)) {
			chesschat.Alert("\1r\1hYou are in check!");
		}
	}
	this.SelectTile=function(start)
	{
		if(this.board.side=="white")
		{
			selected=(start?{"x":start.x, "y":start.y}:{"x":0,"y":7});
			while(1)
			{
				if(start) this.board.DrawTile(start.x,start.y,true);
				var key=console.getkey((K_NOECHO,K_NOCRLF,K_UPPER));
				if(key=="\r") return selected;
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
		}
		else
		{
			selected=(start?{"x":start.x, "y":start.y}:{"x":7,"y":0});
			while(1)
			{
				if(start) this.board.DrawTile(start.x,start.y,true);
				var key=console.getkey((K_NOECHO,K_NOCRLF,K_UPPER));
				if(key=="\r") return selected;
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
		this.board.DrawTile(selected.x,selected.y,true);
	}
	this.CheckRules=function(from,to)
	{	
		var frompiece=this.board.grid[from.x][from.y].contents;
		Log(frompiece.color + " " + frompiece.name + " moving from " + from.x + "," + from.y + " to " + to.x + "," + to.y);
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
			if(this.InCheck(to)) return false;
			else if(Math.abs(from.x-to.x)==2 && !this.InCheck(from)) 
			{
				if(from_tile.contents.has_moved) return false;
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
				if(Math.abs(from.x-to.x)>1) return false;
				if(Math.abs(from.y-to.y)>1) return false;
			}
			this.game.players[this.game.currentplayer].king=to;
		}
		else {
			if(!this.CheckMove(from,to)) return false;
			this.board.grid[to.x][to.y].contents=this.board.grid[from.x][from.y].contents;
			this.board.grid[to.x][to.y].contents.has_moved=true;
			this.board.grid[from.x][from.y].contents=false;
		}
		this.board.DrawTile(from.x,from.y);
		this.board.DrawTile(to.x,to.y);
		return true;
	}
	this.CheckMove=function(from,to)
	{
		var from_tile=this.board.grid[from.x][from.y];
		var to_tile=this.board.grid[to.x][to.y];
		
		//NECESSARY FOR DETERMINING CHECK... SINCE KING IS NOT DEFINED IN THIS RULESET
		if(to.x==from.x && to.y==from.y) return false;
		
		//PAWN RULESET
		if(from_tile.contents.name=="pawn") 
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
					if(ygap==2 && from.y!=6) return false;
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
					if(ygap==2 && from.y!=1) return false;
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
			if(from.x==to.x) {
				if(from.y<to.y) {
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) {
						if(this.board.grid[from.x][from.y+check].contents) return false;
					}
				}
				else {
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) {
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
			else if(from.x==to.x) {
				if(from.y<to.y) {
					var distance=to.y-from.y;
					for(check = 1;check<distance;check++) {
						if(this.board.grid[from.x][parseInt(from.y+check)].contents) return false;
					}
				}
				else {
					var distance=from.y-to.y;
					for(check = 1;check<distance;check++) {
						if(this.board.grid[from.x][parseInt(from.y-check)].contents) return false;
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
		return true;
	}
	this.Castle=function(from,to)
	{
		this.board.grid[to.x][to.y].contents=this.board.grid[from.x][from.y].contents;
		this.board.grid[to.x][to.y].contents.has_moved=true;
		this.board.grid[from.x][from.y].contents=false;

		var xgap=Math.abs(from.x-to.x);
		if(xgap==2) 
		{	//QUEENSIDE
			this.board.grid[to.x+1][to.y].contents=this.board.grid[to.x-2][to.y].contents;
			this.board.grid[to.x+1][to.y].contents.has_moved=true;
			this.board.grid[to.x-2][to.y].contents=false;
			this.board.DrawTile(to.x-2,to.y);
		}
		else 
		{	//KINGSIDE
			this.board.grid[to.x-1][to.y].contents=this.board.grid[to.x+1][to.y].contents;
			this.board.grid[to.x-1][to.y].contents.has_moved=true;
			this.board.grid[to.x+1][to.y].contents=false;
			this.board.DrawTile(to.x-1,to.y);
		}
		this.board.DrawTile(to.x+1,to.y);
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

	this.Init();
	this.Main();
}
function ChessGame(gamefile)
{
	this.board=new ChessBoard();
	this.players=[];
	this.movelist=[];
	this.gamefile;		
	this.gamenumber;
	this.turn;
	this.started;
	this.finished;
	this.currentplayer;
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
		Log("Storing game " + this.gamenumber);
		var gFile=this.gamefile;
		gFile.open("w+");
		//STORE GAME DATA
		var wplayer=this.players["white"].player?this.players["white"].player.name:"";
		var bplayer=this.players["black"].player?this.players["black"].player.name:"";
		
		gFile.iniSetValue(null,"gamenumber",this.gamenumber);
		gFile.iniSetValue(null,"turn",this.turn);

		if(wplayer) gFile.iniSetValue(null,"white",wplayer);
		if(bplayer) gFile.iniSetValue(null,"black",bplayer);
		if(this.started) gFile.iniSetValue(null,"started",this.started);
		if(this.finished) gFile.iniSetValue(null,"finished",this.finished);
		if(this.rated) gFile.iniSetValue(null,"rated",this.rated);
		if(this.timed) gFile.iniSetValue(null,"timed",this.timed);
		if(this.board.lastmove) 
		{
			gFile.iniSetValue("lastmove","from",this.GetPosition(this.board.lastmove.from));
			gFile.iniSetValue("lastmove","to",this.GetPosition(this.board.lastmove.to));
		}
		for(x in this.board.grid)
		{
			for(y in this.board.grid[x])
			{
				var contents=this.board.grid[x][y].contents;
				if(contents)
				{
					var position=this.GetPosition({'x':x,'y':y});
					var section="board."+position;
					gFile.iniSetValue(section,"piece",contents.name);
					gFile.iniSetValue(section,"color",contents.color);
					if(contents.has_moved) gFile.iniSetValue(section,"hasmoved",contents.has_moved);
				}
			}
			for(move in this.movelist)
			{
				gFile.iniSetValue("move." + move,"from",this.GetPosition(this.movelist[move].from));
				gFile.iniSetValue("move." + move,"to",this.GetPosition(this.movelist[move].to));
				if(this.movelist[move].check) gFile.iniSetValue("move." + move,"check",this.movelist[move].check);
			}
		}
		this.gamefile.close();
	}
	this.GetPosition=function(position)
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
	this.LoadGameTable=function()
	{
		//LOAD GAME TABLE - BASIC DATA
		var gFile=this.gamefile;
		gFile.open("r");
		
		var wp=gFile.iniGetValue(null,"white");
		var bp=gFile.iniGetValue(null,"black");
		
		if(wp==user.alias) this.currentplayer="white";
		else if(bp==user.alias) 
		{
			this.currentplayer="black";
			this.board.side="black";
		}
		
		var wpindex=chessplayers.player_index[wp];
		var bpindex=chessplayers.player_index[bp];
		
		this.players["white"]={player:chessplayers.players[wpindex]};
		this.players["black"]={player:chessplayers.players[bpindex]};
		
		this.gamenumber=parseInt(gFile.iniGetValue(null,"gamenumber"),10);
		this.turn=gFile.iniGetValue(null,"turn");
		this.started=gFile.iniGetValue(null,"started");
		this.finished=gFile.iniGetValue(null,"finished");
		this.rated=gFile.iniGetValue(null,"rated");
		this.timed=gFile.iniGetValue(null,"timed");
		
		Log("Loaded game: " + this.gamenumber);
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
			this.board.lastmove=new ChessMove(this.GetPosition(lastmove.from),this.GetPosition(lastmove.to));
		}
		
		//LOAD PIECES
		var pieces=gFile.iniGetAllObjects("position","board.");
		for(p in pieces)
		{
			var pos=this.GetPosition(pieces[p].position);
			var name=pieces[p].piece;
			var color=pieces[p].color;
			
			//TODO:  MAKE SURE TO UPDATE this.players[color].king EVERY TIME A KING MOVES
			if(name=="king") this.players[color].king=pos;
			this.board.grid[pos.x][pos.y].AddPiece(new ChessPiece(name,color));
		}
		//LOAD MOVES
		var moves=gFile.iniGetAllObjects("number","move.");
		for(move in moves)
		{
			var from=this.GetPosition(moves[move].from);
			var to=this.GetPosition(moves[move].to);
			var check=moves[move].check;
			this.movelist.push(new ChessMove(from,to,check));
		}
		gFile.close();
	}
	this.NewGame=function()
	{
		this.SetFileInfo();
		this.turn="white";
		this.players["white"]=new Object();
		this.players["black"]=new Object();
		this.board.ResetBoard();
	}
	this.Init();
}
function ChessMove(from,to,check)
{
	this.from=from;
	this.to=to;
	this.check=check;
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
	this.DrawMove=function(from,to)
	{
		this.ClearLastMove();
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