function GameData(gamefile)
{
	this.graphic=new Graphic(43,24);
	this.graphic.load(root+"blarge.bin");
	this.players=[];
//	this.gamefile;		
//	this.gameNumber;
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
	
	this.init=function()
	{
		if(gamefile) {
			this.gamefile=new File(gamefile);
			var fName=file_getname(gamefile);
			this.loadGame();
		} else	{
			this.setFileInfo();
			this.newGame();
		}
	}
	this.end=function()
	{
		for(player in this.players)
		{
			this.players[player].board.hidden=false;
			this.players[player].board.drawBoard();
		}
		delete this.turn;
		this.finished=true;
		this.storeGame();
	}
	this.addPlayer=function(name)
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
		var id=players.getFullName(name);
		var player=new Object();
		player.name=name;
		player.id=id;
		player.board=new GameBoard(start.x,start.y,hidden);
		this.players.push(player);
		this.storePlayer(player.id);
	}
	this.findComputer=function()
	{
		for(player in this.players)
		{
			if(this.players[player].name=="CPU") return this.players[player];
		}
		return false;
	}
	this.findPlayer=function(id)
	{
		for(var player in this.players)
		{
			if(this.players[player].id==id) return this.players[player];
		}
		return false;
	}
	this.findOpponent=function()
	{
		var current=this.findCurrentUser();
		if(!current) return false;
		for(player in this.players)
		{
			if(this.players[player].id != current.id) return this.players[player];
		}
		return false;
	}
	this.findCurrentUser=function()
	{
		var fullname=players.getFullName(user.alias);
		for(player in this.players)
		{
			if(this.players[player].id==fullname) return this.players[player];
		}
		return false;
	}
	this.nextTurn=function()
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
	this.setFileInfo=function()
	{
		var gNum=1;
		if(gNum<10) gNum="0"+gNum;
		while(file_exists(root+gNum+".bom"))
		{
			gNum++;
			if(gNum<10) gNum="0"+gNum;
		}
		var fName=root + gNum + ".bom";
		this.gamefile=new File(fName);
		this.gameNumber=parseInt(gNum,10);
	}
	this.storeBoard=function(id)
	{
		var gFile=this.gamefile;
		gFile.open("r+",true);
		var player=this.findPlayer(id);
		var board=player.board;
		for(x in board.grid)
		{
			for(y in board.grid[x])
			{
				var position=getBoardPosition({'x':x,'y':y});
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
		sendFiles(gFile.name);
	}
	this.storePlayer=function(id)
	{
		var gFile=this.gamefile;
		gFile.open("r+",true);
		var player=this.findPlayer(id);
		gFile.iniSetValue("players",id,player.ready);
		gFile.close();
		sendFiles(gFile.name);
	}
	this.storePosition=function(id,x,y)
	{
		var gFile=this.gamefile;
		gFile.open("r+",true);
		var player=this.findPlayer(id);
		var board=player.board;
		var position=getBoardPosition({'x':x,'y':y});
		var section=id+".board."+position;
		if(board.grid[x][y])
		{
			var contents=board.grid[x][y];
			gFile.iniSetValue(section,"ship_id",contents.id);
			gFile.iniSetValue(section,"segment",contents.segment);
			gFile.iniSetValue(section,"orientation",contents.orientation);
		}
		gFile.close();
		sendFiles(this.gamefile.name);
	}
	this.storeShot=function(coords,id)
	{
		this.gamefile.open("r+",true);
		var position=getBoardPosition({'x':coords.x,'y':coords.y});
		var section=id+".board."+position;
		this.gamefile.iniSetValue(section,"shot",true);
		this.gamefile.iniSetValue(null,"lastcpuhit",getBoardPosition(this.lastcpuhit));
		this.gamefile.close();
		sendFiles(this.gamefile.name);
	}
	this.storeGame=function()
	{
		var gFile=this.gamefile;
		gFile.open((file_exists(gFile.name) ? 'r+':'w+'),true); 		
		gFile.iniSetValue(null,"gameNumber",this.gameNumber);
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
		sendFiles(gFile.name);
	}
	this.loadPlayers=function()
	{
		this.gamefile.open("r",true);
		var starts=[{'x':2,'y':3},{'x':24,'y':3}];
		for(p in this.players)
		{
			var player=this.players[p];
			if(!this.findCurrentUser())	{
				var start=starts.pop();
				hidden=this.spectate && this.started?false:true;
			} else if(player.id==players.getFullName(user.alias))	{
				start=starts[0];
				hidden=false;
			} else	{
				start=starts[1];
				hidden=true;
			}
			if(!player.board) {
				player.board=new GameBoard(start.x,start.y,hidden);
			}
			if(player.ready) {
				this.loadBoard(player.id);
			}
		}
		this.gamefile.close();
	}
	this.loadPlayer=function(id,ready)
	{
		var player=this.findPlayer(id);
		if(!player)	{
			player=new Object();
			player.id=id;
			player.name=players.getAlias(id);
			this.players.push(player);
		}
		player.ready=ready;
	}
	this.loadGame=function()
	{
		//LOAD GAME TABLE - BASIC DATA
		var gFile=this.gamefile;
		this.lastupdated=file_date(gFile.name);
		if(!file_exists(gFile.name)) return false;
		gFile.open("r",true);
		this.gameNumber=parseInt(gFile.iniGetValue(null,"gameNumber"),10);
		this.turn=gFile.iniGetValue(null,"turn");
		this.singleplayer=gFile.iniGetValue(null,"singleplayer");
		this.finished=gFile.iniGetValue(null,"finished");
		this.started=gFile.iniGetValue(null,"started");
		this.password=gFile.iniGetValue(null,"password");
		this.multishot=gFile.iniGetValue(null,"multishot");
		this.bonusattack=gFile.iniGetValue(null,"bonusattack");
		this.spectate=gFile.iniGetValue(null,"spectate");
		this.winner=gFile.iniGetValue(null,"winner");
		this.lastcpuhit=getBoardPosition(gFile.iniGetValue(null,"lastcpuhit"));
		var playerlist=this.gamefile.iniGetKeys("players");
		for(var p in playerlist)	{
			var player=playerlist[p]; 
			log("loading player: " + player);
			if(!players.players[player]) players.loadPlayer(player);
			var ready=this.gamefile.iniGetValue("players",player);
			this.loadPlayer(player,ready);
		}
		gFile.close();
	}
	this.startGame=function()
	{
		if(this.players.length<2) return false;
		for(var player in this.players) {
			if(!this.players[player].ready) return false;
		}
		this.started=true;
		this.storeGame();
		this.notifyPlayer();
		return true;
	}
	this.notifyLoser=function(id)
	{
		if(!chat.findUser(id))
		{
			var loser=players.players[id].name;
			var unum=system.matchuser(loser);
			var message="\1n\1gYour fleet has been destroyed in \1c\1hSea\1n\1c-\1hBattle\1n\1g game #\1h" + this.gameNumber + "\1n\1g!\r\n\r\n";
			system.put_telegram(unum, message);
		}
	}
	this.notifyPlayer=function()
	{
		var currentplayer=this.findCurrentUser();
		if(!chat.findUser(this.turn) && this.turn!=currentplayer.id)
		{
			var nextturn=this.findPlayer(this.turn);
			var unum=system.matchuser(nextturn.name);
			var message="\1n\1gIt is your turn in \1c\1hSea\1n\1c-\1hBattle\1n\1g\1g game #\1h" + this.gameNumber + "\r\n\r\n";
			system.put_telegram(unum, message);
			//TODO: make this handle interbbs games if possible
		}
	}
	this.loadBoard=function(id)
	{	
		var player=	this.findPlayer(id);
		var board=		player.board;
		var positions=	this.gamefile.iniGetAllObjects("position",id + ".board.");
		for(pos in positions) {
			var coords=				getBoardPosition(positions[pos].position);
			if(positions[pos].shot)		board.shots[coords.x][coords.y]=positions[pos].shot;
			if(positions[pos].ship_id>=0) {
				var id=positions[pos].ship_id;
				var segment=positions[pos].segment;
				var orientation=positions[pos].orientation;
				player.board.grid[coords.x][coords.y]={'id':id,'segment':segment,'orientation':orientation};
				if(segment===0)	player.board.ships[id].orientation=orientation;
				if(positions[pos].shot)	{
					board.ships[id].takeHit(segment);
				}
			}
		}
	}
	this.newGame=function()
	{
		this.started=false;
		this.finished=false;
		this.turn="";
	}
	this.redraw=function()
	{
		this.graphic.draw();
		for(player in this.players)
		{
			this.players[player].board.drawBoard();
		}
	}
	this.init();
}
function BattleShip(name)
{
	this.name=name;
	this.orientation="horizontal";
	//this.segments;
	//this.sunk;
	
	this.toggle=function()
	{
		this.orientation=(this.orientation=="horizontal"?"vertical":"horizontal");
	}
	this.countHits=function()
	{
		var hits=0;
		for(s in this.segments)
		{
			if(this.segments[s].hit) hits++;
		}
		if(hits==this.segments.length) this.sunk=true;
	}
	this.takeHit=function(segment)
	{
		this.segments[segment].hit=true;
		this.countHits();
	}
	this.drawSegment=function(segment)
	{
		this.segments[segment].draw(this.orientation);
	}
	this.draw=function()
	{
		for(segment=0;segment<this.segments.length;segment++)
		{
			this.segments[segment].draw(this.orientation);
		}
	}
	this.unDraw=function()
	{
		for(segment=0;segment<this.segments.length;segment++)
		{
			this.segments[segment].unDraw(this.orientation);
		}
	}
	this.init=function()
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
	this.init();
}
function Segment(hgraph,vgraph)
{
	this.hgraph=hgraph;
	this.vgraph=vgraph;
	this.fg="\1n\1h";
	this.hit=false;
	this.draw=function(orientation)
	{
		var color=(this.hit?"\1r\1h":this.fg);
		var graphic=(orientation=="horizontal"?this.hgraph:this.vgraph);
		for(character in graphic) {
			console.print(color + graphic[character]);
			if(orientation=="vertical") {
				console.down();
				console.left();
			}
		}
	}
	this.unDraw=function(orientation)
	{
		var graphic=(orientation=="horizontal"?this.hgraph:this.vgraph);
		for(character in graphic) {
			console.print(" ");
			if(orientation=="vertical") {
				console.down();
				console.left();
			}
		}
	}
}
function GameBoard(x,y,hidden)
{
	this.x=x;
	this.y=y;
	this.hidden=hidden;

//	this.grid;
//	this.shots;
//	this.ships;
	
	this.init=function()
	{
		this.resetBoard();
		this.loadShips();
	}
	this.getCoords=function(x,y)
	{
		var posx=this.x+(x*2);
		var posy=this.y+(y*2);
		console.gotoxy(posx,posy);
	}
	this.loadShips=function()
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
	this.drawSelected=function(x,y)
	{
		this.getCoords(x,y);
		console.attributes=BLINK + BG_MAGENTA;
		this.drawSpace(x,y);
	}
	this.unDrawSelected=function(x,y)
	{
		this.getCoords(x,y);
		console.attributes=ANSI_NORMAL;
		this.drawSpace(x,y);
	}
	this.resetBoard=function()
	{
		this.grid=new Array(10);
		this.shots=new Array(10);
		for(x=0;x<this.grid.length;x++) 
		{
			this.grid[x]=new Array(10);
			this.shots[x]=new Array(10);
		}
	}
	this.drawSpace=function(x,y)
	{
		if(this.grid[x][y])
		{
			if(!this.hidden)
			{
				var id=		this.grid[x][y].id;
				var segment=	this.grid[x][y].segment;
				var ship=		this.ships[id];
				this.getCoords(x,y);
				ship.drawSegment(segment);
			}
			else if(this.shots[x][y])
			{
				this.drawHit(x,y);
			}
			else 
			{
				this.getCoords(x,y);
				console.putmsg(" ",P_SAVEATR);
			}
		}
		else
		{
			if(this.shots[x][y])
			{
				this.drawMiss(x,y);
			}
			else 
			{
				this.getCoords(x,y);
				console.putmsg(" ",P_SAVEATR);
			}
		}
	}
	this.drawBoard=function()
	{
		for(x=0;x<this.grid.length;x++) 
		{
			for(y=0;y<this.grid[x].length;y++)
			{
				this.drawSpace(x,y);
			}
		}
	}
	this.drawShip=function(x,y,ship)
	{
		this.getCoords(x,y);
		ship.draw();
	}
	this.drawMiss=function(x,y)
	{
		this.getCoords(x,y);
		console.putmsg("\1c\1h\xFE",P_SAVEATR);
	}
	this.drawHit=function(x,y)
	{
		this.getCoords(x,y);
		console.putmsg("\1r\1h\xFE",P_SAVEATR);
	}
	this.addShip=function(position,id)
	{
		var ship=this.ships[id];
		var xinc=(ship.orientation=="vertical"?0:1);
		var yinc=(ship.orientation=="vertical"?1:0);
		for(segment=0;segment<ship.segments.length;segment++)
		{
			var posx=(segment*xinc)+position.x;
			var posy=(segment*yinc)+position.y;
			this.grid[posx][posy]={'segment':segment,'id':id,'orientation':ship.orientation};
		}
	}
	this.unDrawShip=function(x,y,ship)
	{
		this.getCoords(x,y);
		ship.unDraw();
	}
	this.init();
}
function PlayerList()
{
	this.prefix=system.qwk_id + "."; //BBS prefix FOR INTERBBS PLAY?? WE'LL SEE
	this.players=[];
	//this.current;
	
	this.storePlayer=function(id)
	{
		var player=this.players[id];
		var pFile=new File(root + "players.dat");
		pFile.open("r+",true); 
		pFile.iniSetValue(id,"wins",player.wins);
		pFile.iniSetValue(id,"losses",player.losses);
		pFile.close();
	}
	this.formatStats=function(player)
	{
		if(this.getAlias(player)=="CPU") return "";
		var player=this.players[player];
		return(			"\1n\1cW\1h" +
						printPadded(player.wins,3,"0","right") + "\1n\1cL\1h" +
						printPadded(player.losses,3,"0","right"));
	}
	this.formatPlayer=function(player,turn)
	{
		var name=this.getAlias(player);
		var highlight=(player==turn?"\1r\1h":"\1n\1h");
		return (highlight + "[" + "\1n" + name + highlight + "]");
	}
	this.loadPlayers=function()
	{
		var update=false;
		var pFile=new File(root + "players.ini");
		pFile.open((file_exists(pFile.name) ? "r+":"w+"),true); 
		
		if(!pFile.iniGetValue(this.prefix + user.alias,"name")) {
			update=true;
			pFile.iniSetObject(this.prefix + user.alias,new Player(user.alias));
		}
		var players=pFile.iniGetSections();
		for(var player in players) {
			this.players[players[player]]=pFile.iniGetObject(players[player]);
		}
		pFile.close();
		if(update) {
			sendFiles("players.ini");
		}
	}
	this.loadPlayer=function(player_id)
	{
		var pFile=new File(root + "players.ini");
		if(!pFile.open("r",true)) return false; 
		log("loading player data: " + player_id);
		this.players[player_id]=pFile.iniGetObject(player_id);
		debug(this.players[player_id]);
	}
	this.getAlias=function(fullname)
	{
		return(fullname.substr(fullname.indexOf(".")+1));
	}
	this.getFullName=function(alias)
	{
		return(this.prefix + alias);
	}
	this.loadPlayers();
}
function Player(name)
{
	this.name=name;
	this.wins=0;
	this.losses=0;
}
function Attack(attacker,defender,coords)
{
	this.attacker=attacker;
	this.defender=defender;
	this.coords=coords;
}
function Packet(func)
{
	this.func=func;
}