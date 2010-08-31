function ChessGame(gamefile)
{
	this.board=new ChessBoard();
	this.players=[];
	this.movelist=[];
	//this.gamefile;		
	//this.gameNumber;
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
	
	this.init=function()
	{
		if(gamefile)
		{
			this.gamefile=gamefile;
			this.loadGameTable();
		}
		else
		{
			this.setFileInfo();
			this.newGame();
		}
		this.currentplayer=this.playerInGame();
	}
	this.move=function(move)
	{
		log("moving from " + move.from.x + "," + move.from.y + " to "+ move.to.x + "," + move.to.y);
		this.board.move(move.from,move.to);
	}
	this.unMove=function(move)
	{
		this.board.unMove(move.from,move.to);
	}
	this.playerInGame=function()
	{
		for(player in this.players)
		{
			if(this.players[player].id==chessplayers.getFullName(user.alias))
				return player;
		}
		return false;
	}
	this.setFileInfo=function()
	{
		var gNum=1;
		if(gNum<10) gNum="0"+gNum;
		while(file_exists(chessroot+gNum+".chs"))
		{
			gNum++;
			if(gNum<10) gNum="0"+gNum;
		}
		var fName=chessroot + gNum + ".chs";
		this.gamefile=fName;
		this.gameNumber=parseInt(gNum,10);
	}
	this.loadGameTable=function()
	{
		//LOAD GAME TABLE - BASIC DATA
		var gFile=new File(this.gamefile);
		this.lastupdated=file_date(gFile.name);
		if(!file_exists(gFile.name)) return false;
		gFile.open("r",true);
		
		var wp=gFile.iniGetValue(null,"white");
		var bp=gFile.iniGetValue(null,"black");
		
		this.players.white=wp?{"id":wp}:new Object();
		this.players.black=bp?{"id":bp}:new Object();
		
		this.gameNumber=parseInt(gFile.iniGetValue(null,"gameNumber"),10);
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
		
		this.currentplayer=this.playerInGame();
		gFile.close();
	}
	this.newGame=function()
	{
		this.board=new ChessBoard();
		this.board.resetBoard();
		this.players.white=new Object();
		this.players.black=new Object();
		this.started=false;
		this.finished=false;
		this.movelist=[];
		this.turn="";
	}
	this.init();
}
function ChessMove(from,to,color,check)
{
	this.from=from;
	this.to=to;
	this.color=color;
	this.check=check;
	this.format=function()
	{
		var color=(this.color=="white"?"\1w\1h":"\1k\1h");
		var prefix=(this.color=="white"?"W":"B");
		var text=color + prefix + "\1n\1c: " +color+ getChessPosition(this.from) + "\1n\1c-" + color + getChessPosition(this.to);
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
	
	this.draw=function(bg,x,y,large)
	{
		var graphic=large?this.large:this.small;
		for(offset=0;offset<graphic.length;offset++) {
			console.gotoxy(x,y+offset); console.print(this.fg + bg + graphic[offset]);
		}
	}
	this.init=function()
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
	this.init();
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
	
	this.init=function()
	{
		this.loadTiles();
		this.background.load(chessroot + "largebg.bin");
	}
	this.reverse=function()
	{
		this.side=(this.side=="white"?"black":"white");
	}
	this.drawBlinking=function(x,y,selected,color)
	{
		this.grid[x][y].drawBlinking(this.side,selected,color,this.large);
		console.attributes=ANSI_NORMAL;
		console.gotoxy(79,24);
	}
	this.drawTile=function(x,y,selected,color)
	{
		this.grid[x][y].draw(this.side,selected,color,this.large);
		console.attributes=ANSI_NORMAL;
		console.gotoxy(79,24);
	}
	this.loadTiles=function()
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
	this.findKing=function(color)
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
	this.findAllPieces=function(color)
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
	this.clearLastMove=function()
	{
		if(this.lastmove) {
			log("clearing last move");
			var from=this.lastmove.from;
			var to=this.lastmove.to;
			this.drawTile(from.x,from.y,false,false,this.large);
			this.drawTile(to.x,to.y,false,false,this.large);
		}
	}
	this.drawLastMove=function()
	{
		if(this.lastmove) {
			log("drawing last move");
			var from=this.lastmove.from;
			var to=this.lastmove.to;
			this.drawTile(from.x,from.y,true,"\1r\1h",this.large);
			this.drawTile(to.x,to.y,true,"\1r\1h",this.large);
		}
	}
	this.drawBoard=function()
	{
		if(this.large) this.background.draw();
		for(x=0;x<this.grid.length;x++) 
		{
			for(y=0;y<this.grid[x].length;y++)
			{
				this.grid[x][y].draw(this.side,false,false,this.large);
			}
		}
		this.drawLastMove();
		console.gotoxy(79,24);
		console.attributes=ANSI_NORMAL;
	}
	this.move=function(from,to)
	{
 		var from_tile=this.grid[from.x][from.y];
		var to_tile=this.grid[to.x][to.y];
		this.tempfrom=from_tile.contents;
		this.tempto=to_tile.contents;
		to_tile.addPiece(from_tile.contents);
		to_tile.has_moved=true;
		from_tile.removePiece();
	}
	this.unMove=function(from,to)
	{
		var from_tile=this.grid[from.x][from.y];
		var to_tile=this.grid[to.x][to.y];
		from_tile.contents=this.tempfrom;
		to_tile.contents=this.tempto;
		delete this.tempfrom;
		delete this.tempto;
	}
	this.getMoves=function(from)
	{
		var moves=[];
		if(this.name=="pawn") {
			moves.push(from.x,from.y-1);
			if(from.y==6) moves.push(from.x,from.y-2);
			return moves;
		}
	}
	this.loadPieces=function(color)
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
	this.resetBoard=function()
	{
		var black_set=this.loadPieces("black");
		var white_set=this.loadPieces("white");
		for(x=0;x<8;x++) //SET PIECES
		{
			this.grid[x][0].addPiece(black_set.backline[x]);
			this.grid[x][1].addPiece(black_set.pawn);
			this.grid[x][6].addPiece(white_set.pawn);
			this.grid[x][7].addPiece(white_set.backline[x]);
		}
	}
	this.init();
}
function ChessTile(color,x,y)
{
	this.x=x;
	this.y=y;
	this.color=color;
	//this.contents;
	this.bg=(this.color=="white"?console.ansi(BG_LIGHTGRAY):console.ansi(BG_CYAN));

	this.addPiece=function(piece)
	{
		this.contents=piece;
	}
	this.removePiece=function()
	{
		delete this.contents;
	}
	this.draw=function(side,selected,color,large)
	{
		var xinc=large?9:5;
		var yinc=large?5:3;
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*xinc+(large?5:1);
		var y=posy*yinc+(large?3:1);
		
		if(!this.contents) {
			var spaces=large?"         ":"     ";
			var lines=large?5:3;
			for(pos = 0;pos<lines;pos++) {
				console.gotoxy(x,y+pos); write(this.bg+spaces);
			}
		} else {
			this.contents.draw(this.bg,x,y,large);
		}
		if(selected) this.drawSelected(side,color,large);
	}
	this.drawBlinking=function(side,selected,color,large)
	{
		var xinc=large?9:5;
		var yinc=large?5:3;
		var posx=(side=="white"?this.x:7-this.x);
		var posy=(side=="white"?this.y:7-this.y);
		var x=posx*xinc+(large?5:1);
		var y=posy*yinc+(large?3:1);
		
		this.contents.draw(this.bg + console.ansi(BLINK),x,y,large);
		if(selected) this.drawSelected(side,color,large);
	}
	this.drawSelected=function(side,color,large)
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
	this.update=function(length)
	{
		this.countdown=length;
	}
	this.display=function(turn)
	{
		var color=(turn==this.player?"\1r\1h":"\1k\1h");
		var mins=parseInt(this.countdown/60,10);
		var secs=this.countdown%60;
		printf(color + "Time Remaining: %02d:%02d",mins,secs);
		this.lastshown=time();
	}
	this.countdown=function(current)
	{
		if(this.countdown<=0) return false;
		this.countdown-=(current-this.lastupdate);
		this.lastupdate=current;
		return true;
	}
}
function ChessPlayer(name)
{
	log("Creating new player: " + name);
	this.name=name;
	this.rating=1200;
	this.wins=0;
	this.losses=0;
	this.draws=0;
}
function PlayerList()
{
	this.players=[];
	this.prefix=system.qwk_id + ".";
	this.getPlayerRating=function(name)
	{
		var fullname=this.prefix + name;
		return this.players[fullname].rating;
	}
	this.storePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(chessroot + "players.ini");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"rating",player.rating);
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.iniSetValue(id,"draws",player.draws);
		pFile.close();
		sendFiles(pFile.name);
	}
	this.formatStats=function(index)
	{
		if(!index.id) return("");
		var player=this.players[index.id];
		return(	"\1n\1yR\1h" +
						printPadded(player.rating,4,"0","right") + "\1n\1yW\1h" +
						printPadded(player.wins,3,"0","right") + "\1n\1yL\1h" +
						printPadded(player.losses,3,"0","right") + "\1n\1yS\1h" +
						printPadded(player.draws,3,"0","right"));
	}
	this.formatPlayer=function(index,color,turn)
	{
		var name=(index.id?this.players[index.id].name:"Empty Seat");
		var col=(color=="white"?"\1n\1h":"\1k\1h");
		var highlight=(color==turn?"\1r\1h":col);
		return (highlight + "[" + col + name + highlight + "]");
	}
	this.loadPlayers=function()
	{
		var update=false;
		var pFile=new File(chessroot + "players.ini");
		pFile.open(file_exists(pFile.name) ? "r+":"w+",true); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name"))
		{
			pFile.iniSetObject(this.prefix + user.alias,new ChessPlayer(user.alias));
			update=true;
		}
		var players=pFile.iniGetSections();
		for(player in players)
		{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
		}
		pFile.close();
		if(update) sendFiles(pFile.name);
	}
	this.getFullName=function(name)
	{
		return(this.prefix + name);
	}
}
function Packet(func)
{
	this.func=func;
}