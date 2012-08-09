/** GAME PLAY **/
function playGame(profile,game) {

	/* variables and shit */
	var menu, status, direction, queue, last_update, 
		gameFrame, localPlayer, pause, players, readyMsg;

	/* main shit */
	function open() {
		/* game status constants */
		status = {WAITING:-1,STARTING:0,SYNCING:1,PLAYING:2,FINISHED:3};
		/* directional constants */
		direction = {UP:0,DOWN:1,LEFT:2,RIGHT:3};
		last_update=Date.now();
		pause = settings.pause;
		queue = client.read(game_id,"metadata." + game.gameNumber + ".queue",1);
		gameFrame=new Frame(undefined,undefined,undefined,undefined,undefined,frame);
		gameFrame.open();
		initPlayers();
		drawScore(localPlayer);
		client.subscribe(game_id,"metadata." + game.gameNumber);
	}
	function close() {
		if(localPlayer != undefined) {
			if(countMembers(players) > 1) {
				if(game.winner == localPlayer.name) 
					scoreWin();
				else
					scoreLoss();
			}
			saveScore();
		}
		client.unsubscribe(game_id,"metadata." + game.gameNumber);
		data.loadGames();
	}
	function cycle() {
		client.cycle();
		while(client.updates.length > 0)
			processUpdate(client.updates.shift());
		frame.cycle();
		
		var difference=Date.now()-last_update;
		if(difference<pause) 
			return;
		last_update=Date.now();

		if(!localPlayer || !localPlayer.active || game.status !== status.PLAYING) 
 			return;
			
		//TODO: this might not be necessary anymore
		/* if the player needs a new piece, give 'em one! */
		if(localPlayer.currentPiece == undefined) {
			getNewPiece();
			return;
		}
		
		/* if the piece stack has reached the top of the screen, kill */
		if(checkInterference()) {
			killPlayer(localPlayer.name);
			send("DEAD");
			return;
		} 
		
		/* normal timed piece drop */
		if(move(direction.DOWN)) {
			send("MOVE");
		} 
		/* check for any clearable lines */
		else {
			setPiece();
			getLines();
			getNewPiece();
		}
	}
	function main()	{
		while(!js.terminated) {
			cycle();
			
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			switch(k) {
			case "\x1b":	
			case "Q":
				if(handleExit()) 
					return;
				break;
			}
			
			if(game.status == status.SYNCING) {
			
				if(!readyMsg)
					readyMsg = showMessage("Waiting for players");
			}
			
			else if(game.status == status.PLAYING) {
			
				if(readyMsg) {
					readyMsg.close();
					delete readyMsg;
				}
				
				if(!localPlayer.active || localPlayer.currentPiece == undefined) 
					continue;
				
				/* if the piece stack has reached the top of the screen, kill */
				if(checkInterference()) {
					killPlayer(localPlayer.name);
					send("DEAD");
					continue;
				} 
				
				switch(k) {
				case " ":
				case "0":
					if(!localPlayer.swapped) {
						localPlayer.swapped=true;
						swapPiece();
					}
					break;
				case "8":
				case KEY_UP:
					fullDrop();
					send("MOVE");
					setPiece();
					getLines();
					getNewPiece();
					break;
				case "2":
				case KEY_DOWN:
					if(move(direction.DOWN))
						send("MOVE");
					else {
						setPiece();
						getLines();
						getNewPiece();
					}
					break;
				case "4":
				case KEY_LEFT:
					if(move(direction.LEFT)) 
						send("MOVE");
					break;
				case "6":
				case KEY_RIGHT:
					if(move(direction.RIGHT)) 
						send("MOVE");
					break;
				case "5":
					if(rotate(direction.RIGHT)) 
						send("MOVE");
					break;
				case "7":
				case "A":
					if(rotate(direction.LEFT)) 
						send("MOVE");
					break;
				case "9":
				case "D":
					if(rotate(direction.RIGHT)) 
						send("MOVE");
					break;
				default:
					break;
				}
			}
			
			else if(game.status == status.FINISHED) {
				var msg = ["Game Over"];
				if(game.winner == localPlayer.name)
					msg.push("\1g\1hYou win!");
				else 
					msg.push("\1r\1hYou lose!");
				msg.push("\1n\1yPress [\1hQ\1n\1y] to exit");
				showMessage(msg);
				while(console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Q");
				break;
			}
		}
		
		/* shutdown gameplay and return to lobby */
		gameFrame.close();
		return;
	}

	/* init shit */
	function initPlayers() {
		players = {};
		var x=1;
		var y=1;
		var index=0;
		var width=27;
		if(game.players[profile.name]) {
			players[profile.name]=new GameBoard(gameFrame,profile.name,x,y);
			players[profile.name].open();
			index++;
			localPlayer = players[profile.name];
			localPlayer.grid = getMatrix(21,10);
			localPlayer.ready = true;
		}
		for(var p in game.players) {
			if(p == profile.name) 
				continue;
			players[p]=new GameBoard(gameFrame,game.players[p].name,x + (index*width),y);
			players[p].open();
			index++;
		}
	}
	function getMatrix(h,w)	{
		var grid=new Array(h);
		for(var y=0;y<grid.length;y++) {
			grid[y]=getNewLine(w);
		}
		return grid;
	}

	/* data shit */
	function packageData(cmd) {
		var data={cmd:cmd};
		switch(cmd.toUpperCase()) {
		case "MOVE":
			data.x=localPlayer.currentPiece.x;
			data.y=localPlayer.currentPiece.y;
			data.o=localPlayer.currentPiece.o;
			break;
		case "SET":
			break;
		case "CURRENT":
			data.piece=localPlayer.currentPiece.id;
			break;
		case "NEXT":
			data.piece=localPlayer.nextPiece.id;
			break;
		case "HOLD":
			data.piece=localPlayer.holdPiece.id;
			break;
		case "DEAD":
			break;
		case "GARBAGE":
			data.space=random(10);
			data.lines=garbage;
			break;
		case "SCORE":
			data.score=localPlayer.score;
			data.lines=localPlayer.lines;
			data.level=localPlayer.level;
			break;
		case "PLAYER":
			break;
		case "READY":
			break;
		case "GRID":
			data.grid=localPlayer.grid;
			break;
		default:
			log(LOG_WARNING,"Unknown tetris data not sent: " + cmd);
			break;
		}
		data.gameNumber=game.gameNumber;
		data.playerName=localPlayer.name;
		return data;
	}
	function send(cmd)	{
		var d=packageData(cmd);
		if(d) {
			client.write(game_id,"metadata." + game.gameNumber + ".players." + 
				localPlayer.name,d,2);
		}
	}
	function processUpdate(update) {
		if(update.oper == "WRITE" && update.data) {
			var d = update.data;
			var p = players[d.playerName];
			switch(d.cmd) {
			case "MOVE":
				unDrawCurrent(p);
				p.currentPiece.x=d.x;
				p.currentPiece.y=d.y;
				p.currentPiece.o=d.o;
				drawCurrent(p);
				break;
			case "CURRENT":
				unDrawCurrent(p);
				p.currentPiece=loadPiece(d.piece);
				drawCurrent(p);
				break;
			case "GARBAGE":
				loadGarbage(d.lines,d.space);
				drawBoard(localPlayer);
				drawCurrent(p);
				send("GRID");
				break;
			case "SET":
				delete p.currentPiece;
				break;
			case "DEAD":
				killPlayer(d.playerName);
				break;
			case "NEXT":
				p.nextPiece=loadMini(d.piece);
				drawNext(p);
				break;
			case "HOLD":
				p.holdPiece=loadMini(d.piece);
				drawHold(p);
				break;
			case "SCORE":
				p.score=d.score;
				p.lines=d.lines;
				p.level=d.level;
				drawScore(p);
				break;
			case "GRID":
				p.grid=d.grid;
				drawBoard(p);
				break;
			default:
				var playerName = undefined;
				var gameNumber = undefined;
				var p=update.location.split(".");
				
				while(p.length > 1) {
					var child=p.shift();
					switch(child.toUpperCase()) {
					case "PLAYERS":
						playerName = p[0];
						break;
					case "GAMES":
					case "METADATA":
						gameNumber = p[0];
						break;
					}
				}
				if(gameNumber && gameNumber == game.gameNumber) {
					switch(p.pop().toUpperCase()) { 
					case "STATUS":
						game.status = update.data;
						break;
					case "WINNER":
						game.winner = update.data;
						break;
					}
				}
				break;
			}
		}
		else if(update.oper == "SUBSCRIBE") {
			/* player rejoining? */
		}
		else if(update.oper == "UNSUBSCRIBE") {
			/* player leaving? */
		}
	}

	/* game board shit */
	function loadGarbage(lines,space) {
		for(var l=0;l<lines;l++) {
			/* pull the top line off the stack */
			var topLine = localPlayer.grid.shift();
			
			/* if the current falling piece is interfering
			push the top line back on the stack and set the piece
			in the stack and remove the top line again 
			(seems kludgy) */
			if(checkInterference(localPlayer)) {
				localPlayer.grid.unshift(topLine);
				setPiece();
				topLine = localPlayer.grid.shift();
			}
			
			/* add a line of garbage at the bottom */
			localPlayer.grid.push(getGarbageLine(space));
			
			/* if the top line we removed contains pieces,
			the player is now dead */
			for each(var i in topLine) {
				if(i > 0) {
					killPlayer(localPlayer.name);
					send("DEAD");
					return;
				}
			}
		}
	}
	function getGarbageLine(space) {
		var line=new Array(10);
		for(var x=0;x<line.length;x++) {
			if(x==space) 
				line[x]=0;
			else {
				line[x]=random(7)+1;
			}
		}
		return line;
	}
	function getNewLine(w) {
		var line=new Array(w);
		for(var x=0;x<line.length;x++) {
			line[x]=0;
		}
		return line;
	}
	function getLines()	{
		var lines=[];
		for(var y=0;y<localPlayer.grid.length;y++) {
			var line=true;
			for(var x=0;x<localPlayer.grid[y].length;x++) {
				if(localPlayer.grid[y][x] == 0) {
					line=false;
					break;
				}
			}
			if(line) {
				localPlayer.grid.splice(y,1);
				localPlayer.grid.unshift(getNewLine(localPlayer.grid[0].length));
				lines.push(y);
			}
		}
		if(lines.length>0) {
			drawBoard(localPlayer);
			send("GRID");
			scoreLines(lines.length);

			if(game.garbage && activePlayers() > 1) {
				if(lines.length==4) 
					garbage=4;
				else if(lines.length==3) 
					garbage=2;
				else if(lines.length==2) 
					garbage=1;
				else 
					garbage=0;
				if(garbage>0) 
					send("GARBAGE");
			} 
		}
	}
	function scoreLines(qty) {
		/*	a "TETRIS" */
		if(qty==4) 
			qty=8;

		localPlayer.lines+=qty;
		localPlayer.score+=(localPlayer.level * qty * settings.base_points);

		if(localPlayer.lines/settings.lines >= localPlayer.level) {
			localPlayer.level++;
			pause*=settings.pause_reduction;
		}
		
		drawScore(localPlayer);
		send("SCORE");
	}
	function checkInterference() {
		var piece=localPlayer.currentPiece;
		if(piece == undefined)
			return;
		var piece_matrix=piece.direction[piece.o];
		
		var yoffset=piece.y;
		var xoffset=piece.x;
		
		for(var y=0;y<piece_matrix.length;y++) {
			for(var x=0;x<piece_matrix[y].length;x++) {
				var posy=yoffset+y;
				var posx=xoffset+x;
				if(piece_matrix[y][x] > 0) {
					if(!localPlayer.grid[posy] || localPlayer.grid[posy][posx] == undefined) {
						return true;
					}
					if(localPlayer.grid[posy][posx] > 0) {
						return true;
					}
				}
			}
		}
		return false;
	}

	/* piece assignment shit */
	function loadPiece(index) {
		var up;
		var down;
		var left;
		var right;
		var fg;
		var bg;
		
		switch(index) {
		case 1: // BAR 1 x 4 CYAN
			up=[
				[0,1,0,0],
				[0,1,0,0],
				[0,1,0,0],
				[0,1,0,0]
			];
			right=[
				[0,0,0,0],
				[1,1,1,1],
				[0,0,0,0],
				[0,0,0,0]
			];
			down=up;
			left=right;
			fg=CYAN;
			bg=BG_CYAN;
			break;
		case 2: // BLOCK 2 x 2 LIGHTGRAY
			up=[
				[2,2],
				[2,2]
			];
			down=up;
			right=up;
			left=up;
			fg=LIGHTGRAY;
			bg=BG_LIGHTGRAY;
			break;
		case 3: // "T"	2 x 3 BROWN
			up=[
				[0,3,0],
				[3,3,3],
				[0,0,0]
			];
			right=[
				[0,3,0],
				[0,3,3],
				[0,3,0]
			];
			down=[
				[0,0,0],
				[3,3,3],
				[0,3,0]
			];
			left=[
				[0,3,0],
				[3,3,0],
				[0,3,0]
			];
			bg=BG_BROWN;
			fg=BROWN;
			break;
		case 4: // RH OFFSET 2 x 3 GREEN
			up=[
				[4,0,0],
				[4,4,0],
				[0,4,0]
			];
			right=[
				[0,4,4],
				[4,4,0],
				[0,0,0]
			];
			down=up;
			left=right;
			fg=GREEN;
			bg=BG_GREEN;
			break;
		case 5: // LH OFFSET 2 x 3 RED
			up=[
				[0,0,5],
				[0,5,5],
				[0,5,0]
			];
			right=[
				[5,5,0],
				[0,5,5],
				[0,0,0]
			];
			down=up;
			left=right;
			fg=RED;
			bg=BG_RED;
			break;
		case 6: // RH "L" 2 x 3 MAGENTA
			up=[
				[0,6,0],
				[0,6,0],
				[0,6,6]
			];
			right=[
				[0,0,0],
				[6,6,6],
				[6,0,0]
			];
			down=[
				[6,6,0],
				[0,6,0],
				[0,6,0]
			];
			left=[
				[0,0,6],
				[6,6,6],
				[0,0,0]
			];
			fg=MAGENTA;
			bg=BG_MAGENTA;
			break;
		case 7: // LH "L" 2 x 3 BLUE
			up=[
				[0,7,0],
				[0,7,0],
				[7,7,0]
			];
			right=[
				[7,0,0],
				[7,7,7],
				[0,0,0]
			];
			down=[
				[0,7,7],
				[0,7,0],
				[0,7,0]
			];
			left=[
				[0,0,0],
				[7,7,7],
				[0,0,7]
			];
			fg=BLUE;
			bg=BG_BLUE;
			break;
		}
		
		return new Piece(index,fg,bg,up,down,right,left);
	}
	function loadMini(index) {
		var grid;
		var fg;
		var bg;
		
		switch(index) {
		case 1: // BAR 1 x 4 CYAN
			grid=[
				" \xDB ",
				" \xDB ",
				" \xDB "
			];
			fg=CYAN;
			bg=BG_CYAN;
			break;
		case 2: // BLOCK 2 x 2 LIGHTGRAY
			grid=[
				" \xDB\xDB",
				" \xDB\xDB",
				"   "
			];
			fg=LIGHTGRAY;
			bg=BG_LIGHTGRAY;
			break;
		case 3: // "T"	2 x 3 BROWN
			grid=[
				" \xDB ",
				"\xDB\xDB\xDB",
				"   "
			];
			bg=BG_BROWN;
			fg=BROWN;
			break;
		case 4: // RH OFFSET 2 x 3 GREEN
			grid=[
				" \xDB\xDB",
				"\xDB\xDB ",
				"   "
			];
			fg=GREEN;
			bg=BG_GREEN;
			break;
		case 5: // LH OFFSET 2 x 3 RED
			grid=[
				"\xDB\xDB ",
				" \xDB\xDB",
				"   "
			];
			fg=RED;
			bg=BG_RED;
			break;
		case 6: // RH "L" 2 x 3 MAGENTA
			grid=[
				" \xDB ",
				" \xDB ",
				" \xDB\xDB"
			];
			fg=MAGENTA;
			bg=BG_MAGENTA;
			break;
		case 7: // LH "L" 2 x 3 BLUE
			grid=[
				" \xDB ",
				" \xDB ",
				"\xDB\xDB "
			];
			fg=BLUE;
			bg=BG_BLUE;
			break;
		}
		
		return new Mini(index,fg,bg,grid);
	}
	function setPiece()	{
		var piece=localPlayer.currentPiece;
		var piece_matrix=piece.direction[localPlayer.currentPiece.o];
		
		var yoffset=piece.y;
		var xoffset=piece.x;
		
		for(var y=0;y<piece_matrix.length;y++) {
			for(var x=0;x<piece_matrix[y].length;x++) {
				var posy=yoffset+y;
				var posx=xoffset+x;
				if(piece_matrix[y][x] > 0) {
					localPlayer.grid[posy][posx]=piece_matrix[y][x];
				}
			}
		}
		delete localPlayer.currentPiece;
		send("SET");
	}
	function swapPiece() {
		unDrawCurrent(localPlayer);
		if(localPlayer.holdPiece) {
			var mini=localPlayer.currentPiece.id;
			localPlayer.currentPiece=loadPiece(localPlayer.holdPiece.id);
			localPlayer.holdPiece=loadMini(mini);
			
			drawCurrent(localPlayer);
			drawNext(localPlayer);
			drawHold(localPlayer);
			
			send("HOLD");
			send("CURRENT");
			send("NEXT");
		} else {
			localPlayer.holdPiece=loadMini(localPlayer.currentPiece.id);
			send("HOLD");
			getNewPiece();
			drawHold(localPlayer);
		}
	}
	function getNewPiece() {
		if(localPlayer.nextPiece == undefined) 
			localPlayer.nextPiece=loadMini(queue.pop());
		localPlayer.currentPiece=loadPiece(localPlayer.nextPiece.id);
		localPlayer.nextPiece=loadMini(queue.pop());
		
		drawCurrent(localPlayer);
		drawNext(localPlayer);
		localPlayer.swapped=false;
		last_update=Date.now();		
		
		send("CURRENT");
		send("NEXT");
	}

	/* movement shit */
	function fullDrop()	{	
		while(1) {
			if(!move(direction.DOWN)) 
				break;
		}
	}
	function move(dir) {
		var piece=localPlayer.currentPiece;
		var old_x=piece.x;
		var old_y=piece.y;
		
		var new_x=old_x;
		var new_y=old_y;
		
		switch(dir) {
		case direction.DOWN:
			new_y=piece.y+1;
			break;
		case direction.LEFT:
			new_x=piece.x-1;
			break;
		case direction.RIGHT:
			new_x=piece.x+1;
			break;
		}
		
		piece.x=new_x;
		piece.y=new_y;

		if(checkInterference()) {
			piece.x=old_x;
			piece.y=old_y;
			return false;
		} 
		
		piece.x=old_x;
		piece.y=old_y;
		unDrawCurrent(localPlayer);
		piece.x=new_x;
		piece.y=new_y;
		drawCurrent(localPlayer);
		return true;
	}
	function rotate(dir) {
		var piece=localPlayer.currentPiece;
		var old_o=piece.o;
		var new_o=old_o;
		var old_x=piece.x;
		var new_x=old_x;
		
		switch(dir)	{
		case direction.RIGHT:
			new_o=piece.o-1;
			if(new_o<0) 
				new_o=piece.direction.length-1;
			break;
		case direction.LEFT:
			new_o=piece.o+1;
			if(new_o==piece.direction.length) 
				new_o=0;
			break;
		}
		var grid=piece.direction[piece.o];
		var overlap=(piece.x + grid[0].length)-10;
		if(overlap>0) 
			new_x=piece.x-overlap;
		
		piece.o=new_o;
		piece.x=new_x
	
		if(checkInterference()) {
			piece.x=old_x;
			piece.o=old_o;
			return false;
		} 
		
		piece.x=old_x;
		piece.o=old_o;
		unDrawCurrent(localPlayer);
		piece.x=new_x;
		piece.o=new_o;
		drawCurrent(localPlayer);
		return true;
	}

	/* display shit */
	function getAttr(index)	{
		switch(Number(index)) {
		case 1:
			return CYAN;
		case 2:
			return LIGHTGRAY;
		case 3:
			return BROWN;
		case 4:
			return GREEN;
		case 5:
			return RED;
		case 6:
			return MAGENTA;
		case 7:
			return BLUE;
		}
	}
	function getMsgFrame(msg) {
		var w = 0;
		var h = 0;
		if(msg instanceof Array) {
			for each(var l in msg) {
				var length = console.strlen(l);
				if(length > w)
					w = length;
			}
			h = msg.length;
		}
		else {
			w = console.strlen(msg);
			h = 1;
		}
		w += 2;
		h += 2;
		var x = Math.floor((localPlayer.frame.width - w)/2);
		var y = 10;
		var f = new Frame(x,y,w,h,BG_BLUE+YELLOW,localPlayer.frame);
		f.gotoxy(2,2);
		return f;
	}
	function showMessage(msg) {
		var f = getMsgFrame(msg);
		f.open();
		
		if(msg instanceof Array) {
			for each(var l in msg) {
				f.center(l + "\r\n");
			}
		}
		else {
			f.center(msg);
		}
		
		f.draw();
		return f;
	}
	function drawCurrent(player) {
		var piece = player.currentPiece;
		if(piece == undefined) 
			return;
		var grid=piece.direction[piece.o];
		for(var y=0;y<grid.length;y++) {
			for(var x=0;x<grid[y].length;x++) {
				if(grid[y][x] > 0) {
					var xoffset=(piece.x+x)*2+1;
					var yoffset=piece.y+y+1;
					player.stack.gotoxy(xoffset,yoffset);
					player.stack.putmsg(piece.ch,piece.attr);
				}
			}
		}
	}
	function drawNext(player) {
		var piece = player.nextPiece;
		if(piece == undefined) 
			return;
		player.frame.gotoxy(23,6);
		player.frame.pushxy();
		for(var x=0;x<piece.grid.length;x++) {
			player.frame.putmsg(piece.grid[x],piece.fg);
			player.frame.popxy();
			player.frame.down();
			player.frame.pushxy();
		}
	}
	function drawHold(player) {
		var piece = player.holdPiece;
		if(piece == undefined) 
			return;
		player.frame.gotoxy(23,12);
		player.frame.pushxy();
		for(var x=0;x<piece.grid.length;x++) {
			player.frame.putmsg(piece.grid[x],piece.fg);
			player.frame.popxy();
			player.frame.down();
			player.frame.pushxy();
		}
	}
	function unDrawCurrent(player) {
		var piece = player.currentPiece;
		if(piece == undefined) 
			return;
		var grid=piece.direction[piece.o];

		for(var y=0;y<grid.length;y++) {
			for(var x=0;x<grid[y].length;x++) {
				if(grid[y][x] > 0) {
					var xoffset=(piece.x+x)*2+1;
					var yoffset=piece.y+y+1;
					player.stack.gotoxy(xoffset,yoffset);
					player.stack.putmsg("  ");
				}
			}
		}
	}
	function drawBoard(player) {
		for(var y=0;y<player.grid.length;y++) {
			for(var x=0;x<player.grid[y].length;x++) {
			
				var xpos = (x*2);
				var ypos = y;
				var ch = " ";
				var attr = 0;
				
				if(player.grid[y][x]>0) {
					ch = "\xDB";
					attr = getAttr(player.grid[y][x]);
				}
				
				player.stack.setData(xpos,ypos,ch,attr);
				player.stack.setData(xpos+1,ypos,ch,attr);
			}
		}
	}
	function drawScore(player) {
		player.frame.gotoxy(1,1);
		player.frame.putmsg(splitPadded(
			printPadded("\1n" + player.name,20), 
			printPadded("\1h" + player.score,6,"\1n\1r0","right"),
			25
		));
		player.frame.gotoxy(1,2);
		player.frame.putmsg(splitPadded(
			"\1k\1hLevel:" + printPadded("\1h" + player.level,4,"\1n\1r0","right"),
			"\1k\1hLines:" + printPadded("\1h" + player.lines,4,"\1n\1r0","right"),
			26
		));
	}
	
	/* other shit */
	function onlinePlayers() {
		var online = client.who(game_id,"metadata." + game.gameNumber);
		log(online.toSource());
		return online;
	}
	function handleExit() {
		if(game.status == status.PLAYING) {
			var f = showMessage("\1c\1hQuit game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") {
				f.close();
				return false;
			}
			f.close();
			killPlayer(localPlayer.name);
			send("DEAD");
		}
		gameFrame.close();
		return true;
	}
	function activePlayers() {
		var count=0;
		for each(var p in players) {
			if(p.active) 
				count++;
		}
		return count;
	}
	function killPlayer(playerName) {	
		players[playerName].active=false;
		findWinner(); 
	}
	function findWinner() {
		if(activePlayers() <= 1) {
			for(var p in players) {
				if(players[p].active == true)  {
					game.winner=p;
					client.write(game_id,"games." + game.gameNumber + ".winner",game.winner,2);
					break;
				}
			}
			endGame();
		}
	}
	function scoreWin() {
		profile.wins++;
	}
	function scoreLoss() {
		profile.losses++;
	}
	function saveScore() {
		if(localPlayer.score > profile.score)
			profile.score = localPlayer.score;
		if(localPlayer.lines > profile.lines) 
			profile.lines = localPlayer.lines;
		if(localPlayer.level > profile.level) 
			profile.level = localPlayer.level;
		client.write(game_id,"profiles." + localPlayer.name,profile,2);
	}
	function endGame() {
		game.status=status.FINISHED;
		client.write(game_id,"games." + game.gameNumber + ".status",game.status,2);
	}
			
	open();
	main();
	close();
}

