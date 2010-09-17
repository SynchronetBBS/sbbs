function Tetris(dataFile)
{
	this.timer=new Timer("\1y\1h");
	this.players=[];
	this.status=-1;
	this.lastupdate=-1;
	this.garbage=true;
	this.update=true;
	this.winner=false;
	//this.dataFile;		
	//this.gameNumber;
	
	this.init=function()
	{
		if(dataFile) {
			this.dataFile=dataFile;
			this.loadGame();
		} else {
			this.setFileInfo();
		}
	}
	this.setFileInfo=function()
	{
		var gNum=1;
		if(gNum<10) gNum="0"+gNum;
		while(file_exists(root + "tetris" + gNum + ".ini")) {
			gNum++;
			if(gNum<10) gNum="0"+gNum;
		}
		var fName=root + "tetris" + gNum + ".ini";
		this.dataFile=fName;
		this.gameNumber=parseInt(gNum,10);
	}
	this.loadGame=function()
	{
		//LOAD GAME TABLE - BASIC DATA
		var file=new File(this.dataFile);
		this.lastupdate=file_date(file.name);
		if(!file_exists(file.name)) return false;
		file.open("r",true);
		
		var created=file.iniGetValue(null,"created");
		if(created > 0) {
			var current=time();
			var difference=current-created;
			this.timer.init(20-difference);
		}

		this.gameNumber=Number(file.iniGetValue(null,"gameNumber"));
		this.status=file.iniGetValue(null,"status");
		var plist=file.iniGetObject("players");
		for(var p in plist) {
			if(!this.players[p]) this.players[p]=new Player(players.getAlias(p),plist[p]);
			else this.players[p].active=plist[p];
		}
		this.update=true;
		log("loaded game: " + this.gameNumber);
		file.close();
	}
	this.redraw=function()
	{
		this.board.draw();
		this.currentPiece.draw();
	}

	this.init();
}
function Mini(fg,bg,grid)
{
	this.grid=grid;
	this.fg=fg;
	this.bg=bg;
}
function Piece(id,fg,bg,u,d,l,r)
{
	//this.attr=fg+bg;
	this.attr=fg+BG_BLACK;
	this.ch="\xDB\xDB";
	this.id=id;
	this.orientation=[u,r,d,l];

	this.x=4;
	this.y=0;
	this.o=0;
	
	for(var x=0;x<u[0].length;x++) {
		for(var y=0;y<u.length;y++) {
			var found=false;
			if(u[y][x] > 0) {
				found=true;
				break;
			}
		}
		if(found) {
			this.x-=x;
			break;
		}
	}
}
function GameBoard(name,x,y)
{
	log("creating board for: " + name);
	this.x=x;
	this.y=y;

	this.name=name;
	this.score=0;
	this.lines=0;
	this.level=1;
	this.active=true;
	this.swapped=false;
	
	this.background=new Graphic(26,22);
	this.background.load(root + "board.bin");
	this.grid=getMatrix(20,10);
	//this.currentPiece;
	//this.nextPiece;
	//this.holdPiece;
	
	this.drawBackground=function()
	{
		this.background.draw(this.x,this.y);
	}
	this.drawBoard=function() 
	{
		var startx=this.x+1;
		var starty=this.y+1;
		
		for(var y=0;y<this.grid.length;y++) {
			for(var x=0;x<this.grid[y].length;x++) {
				if(this.grid[y][x]>0) {
					getAttr(this.grid[y][x]);
					console.gotoxy(startx+(x*2), starty+y);
					console.putmsg("\xDB\xDB",P_SAVEATR);
				}
			}
		}
	}
	this.drawScore=function()
	{
		console.gotoxy(this.x,1);
		console.pushxy();
		console.putmsg(printPadded("\1y\1h" + this.name,19) + printPadded("\1h" + this.score,6,"\1n\1r0","right"));
		console.popxy();
		console.down();
		console.putmsg("\1nLEVEL \1h: " + printPadded("\1h" + this.level,4,"\1n\1r0","right") + " \1nLINES \1h: " + printPadded("\1h" + this.lines,4,"\1n\1r0","right"));
	}
	this.clearBoard=function()
	{
		clearBlock(this.x+1,this.y+1,20,20);
	}
	this.drawPiece=function()
	{
		if(!this.currentPiece) return;
		var piece=this.currentPiece;
		var startx=this.x+1;
		var starty=this.y+1;
		var grid=piece.orientation[piece.o];

		console.attributes=piece.attr;
		for(var y=0;y<grid.length;y++) {
			for(var x=0;x<grid[y].length;x++) {
				if(grid[y][x] > 0) {
					var xoffset=(piece.x+x)*2;
					var yoffset=piece.y+y;
					console.gotoxy(startx+xoffset,starty+yoffset);
					console.putmsg(piece.ch,P_SAVEATR);
				}
			}
		}
		console.home();
	}
	this.drawNext=function()
	{
		if(!this.nextPiece) return;
		console.gotoxy(this.x + 22,this.y + 3);
		var piece=loadMini(this.nextPiece);
		console.attributes=piece.fg;
		console.pushxy();
		for(var x=0;x<piece.grid.length;x++) {
			console.putmsg(piece.grid[x],P_SAVEATR);
			console.popxy();
			console.down();
			console.pushxy();
		}
	}
	this.drawHold=function()
	{
		if(!this.holdPiece) return;
		console.gotoxy(this.x + 22,this.y + 9);
		var piece=loadMini(this.holdPiece);
		console.attributes=piece.fg;
		console.pushxy();
		for(var x=0;x<piece.grid.length;x++) {
			console.putmsg(piece.grid[x],P_SAVEATR);
			console.popxy();
			console.down();
			console.pushxy();
		}
	}
	this.unDrawPiece=function()
	{
		if(!this.currentPiece) return;
		var piece=this.currentPiece;
		var startx=this.x+1;
		var starty=this.y+1;
		var grid=piece.orientation[piece.o];

		console.attributes=ANSI_NORMAL;
		
		for(var y=0;y<grid.length;y++) {
			for(var x=0;x<grid[y].length;x++) {
				if(grid[y][x] > 0) {
					var xoffset=(piece.x+x)*2;
					var yoffset=piece.y+y;
					console.gotoxy(startx+xoffset,starty+yoffset);
					console.putmsg("  ",P_SAVEATR);
				}
			}
		}
	}
}
function PlayerList()
{
	this.players=[];
	this.prefix=system.qwk_id + ".";
	
	this.storePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(root + "players.ini");
		pFile.open("r+"); 
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.close();
		sendFiles(pFile.name);
	}
	this.loadPlayers=function()
	{
		var update=false;
		var pFile=new File(root + "players.ini");
		pFile.open(file_exists(pFile.name) ? "r+":"w+",true); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name")) {
			pFile.iniSetObject(this.prefix + user.alias,new Score(user.alias));
			update=true;
		}
		var players=pFile.iniGetSections();
		for(player in players)	{
			this.players[players[player]]=pFile.iniGetObject(players[player]);
		}
		pFile.close();
		if(update) sendFiles(pFile.name);
	}
	this.getPlayer=function(id)
	{
		return this.players[id];
	}
	this.getAlias=function(id)
	{
		return id.substr(id.indexOf(".")+1);
	}
	this.getPlayerID=function(name)
	{
		return(this.prefix + name);
	}
}
function Score(name,wins,losses)
{
	this.name=name;
	this.wins=wins?wins:0;
	this.losses=losses?losses:0;
}
function Player(name,active)
{
	this.name=name;
	this.active=active;
}
function Packet(func)
{
	this.func=func;
}