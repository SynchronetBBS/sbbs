/** GAME PLAY **/
function playGame(profile,game) {

	/* variables and shit */
	var menu, status, direction, queue, lines, pause, pause_reduction, 
		garbage, base_points, last_update, gameFrame, localPlayer;

	/* main shit */
	function open() {
		/* game status constants */
		status = {WAITING:-1,STARTING:0,PLAYING:1,FINISHED:2};
		/* directional constants */
		direction = {UP:0,DOWN:1,LEFT:2,RIGHT:3};
		/*	variable for holding outbound garbage quantity */
		garbage = 0;
		/* base point multiplier */
		base_points = 5;
		/*  wait between gravitational movement */
		pause = 1000;
		/* 	pause multiplier for level increase */
		pause_reduction = 0.9;
		/*	lines per level */
		lines = 10;
		
		last_update=Date.now();
		gameFrame=new Frame(undefined,undefined,undefined,undefined,undefined,frame);
		gameFrame.open();
		initBoards();
		localPlayer = game.players[profile.name];
		localPlayer.grid = getMatrix(21,10);
		client.callback = 
		queue = client.read(game_id,"metadata." + game.gameNumber + ".queue",1);
		client.subscribe(game_id,"metadata." + game.gameNumber);
		drawScore(localPlayer);
	}
	function close() {
		client.unsubscribe(game_id,"metadata." + game.gameNumber);
	}
	function cycle() {
		client.cycle();
		while(client.updates.length > 0)
			processUpdate(client.updates.shift());
		frame.cycle();
		
		if(!localPlayer || !localPlayer.active) 
 			return;
			
		var difference=Date.now()-last_update;
		if(difference<pause) 
			return;
		last_update=Date.now();

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
		}
	}
	function main()	{
		while(game.status == status.PLAYING) {
		
			cycle();
			
			var k=console.inkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER,5);
			switch(k) {
			case "\x1b":	
			case "Q":
				if(handleExit()) 
					return;
				break;
			}
			
			if(!localPlayer.active || localPlayer.currentPiece == undefined) 
				continue;
			
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
				break;
			case "2":
			case KEY_DOWN:
				if(move(direction.DOWN))
					send("MOVE");
				else {
					setPiece();
					getLines();
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
		
		console.gotoxy(localPlayer.x+1,localPlayer.y+11);
		console.pushxy();
		
		if(game.winner == localPlayer.name) {
			message("\1g\1hYOU WIN!");
		} 
		else {
			message("\1r\1hYOU LOSE!");
		}
		message("\1n\1rpress \1hQ \1n\1rto quit");
		while(1) {
			if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)=="Q") {
				return;
			}
		}
	}

	/* init shit */
	function initBoards() {
		var x=1;
		var y=3;
		var index=0;
		var width=27;
		if(game.players[profile.name]) {
			game.players[profile.name]=new GameBoard(gameFrame,profile.name,x,y);
			game.players[profile.name].open();
			index++;
		}
		for(var p in game.players) {
			if(p == profile.name) 
				continue;
			game.players[p]=new GameBoard(gameFrame,game.players[p].name,x + (index*width),y);
			game.players[p].open();
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
			data.lines=garbage;
			break;
		case "SCORE":
			data.score=localPlayer.score;
			data.lines=localPlayer.lines;
			data.level=localPlayer.level;
			break;
		case "PLAYER":
			break;
		case "GRID":
			data.grid=localPlayer.grid;
			break;
		default:
			log("Unknown tetris data not sent");
			break;
		}
		data.gameNumber=game.gameNumber;
		data.playerName=localPlayer.name;
		return data;
	}
	function send(cmd)	{
		var d=packageData(cmd);
		if(d) 
			client.write(game_id,"metadata." + game.gameNumber + ".players." + localPlayer.name,d,2);
	}
	function processUpdate(packet) {
		if(packet.oper.toUpperCase() == "WRITE") {
			var data = packet.data;
			var p = game.players[data.playerName];
			switch(data.cmd.toUpperCase()) {
			case "MOVE":
				unDrawCurrent(p);
				p.currentPiece.x=data.x;
				p.currentPiece.y=data.y;
				p.currentPiece.o=data.o;
				drawCurrent(p);
				break;
			case "CURRENT":
				unDrawCurrent(p);
				p.currentPiece=loadPiece(data.piece);
				drawCurrent(p);
				break;
			case "GARBAGE":
				//loadGarbage(data.lines,data.space);
				//drawBoard(p);
				break;
			case "SET":
				delete p.currentPiece;
				break;
			case "DEAD":
				killPlayer(p);
				break;
			case "NEXT":
				p.nextPiece=loadMini(data.piece);
				drawNext(p);
				break;
			case "HOLD":
				p.holdPiece=loadMini(data.piece);
				drawHold(p);
				break;
			case "SCORE":
				p.score=data.score;
				p.lines=data.lines;
				p.level=data.level;
				drawScore(p);
				break;
			case "GRID":
				p.grid=data.grid;
				drawBoard(p);
				break;
			default:
				log("Unknown tetris data type received");
				log("packet: " + data.toSource());
				break;
			}
		}
		else {
			//TODO: handle subscribe/unsubscribe/etc...
		}
	}

	/* game board shit */
	function getGarbageLine(space) {
		var line=new Array(10);
		for(var x=0;x<line.length;x++) {
			if(x==space) line[x]=0;
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

			if(game.garbage) {
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
		if(qty==4) qty=10;

		localPlayer.lines+=qty;
		localPlayer.score+=(localPlayer.level * qty * base_points);

		if(localPlayer.lines/10 >= localPlayer.level) {
			localPlayer.level++;
			pause=pause*pause_reduction;
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
	function display(player,msg) {
		gameFrame.gotoxy(gameFrame.x+1,gameFrame.y+10);
		gameFrame.pushxy();
		message(msg);
	}
	function message(msg) {
		gameFrame.popxy();
		gameFrame.putmsg(centerString(msg,20));
		gameFrame.popxy();
		gameFrame.down();
		gameFrame.pushxy();
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
		player.frame.gotoxy(23,4);
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
		player.frame.gotoxy(23,10);
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
		gameFrame.gotoxy(1,1);
		gameFrame.putmsg(splitPadded(
			printPadded("\1n" + player.name,19), 
			printPadded("\1h" + player.score,6,"\1n\1r0",direction.RIGHT),
			25
		));
		gameFrame.gotoxy(1,2);
		gameFrame.putmsg(splitPadded(
			"\1k\1hLevel:" + printPadded("\1h" + player.level,4,"\1n\1r0",direction.RIGHT),
			"\1k\1hLines:" + printPadded("\1h" + player.lines,4,"\1n\1r0",direction.RIGHT),
			25
		));
	}
	
	/* other shit */
	function handleExit() {
		// if(game.status == status.PLAYING) {
			// display(localPlayer,"\1c\1hQuit game? \1n\1c[\1hN\1n\1c,\1hy\1n\1c]");
			// if(console.getkey(K_NOCRLF|K_NOSPIN|K_NOECHO|K_UPPER)!="Y") {
				// gameFrame.close();
				// return false;
			// }
			// killPlayer(localPlayer.name);
			// send("DEAD");
		// }
		gameFrame.close();
		return true;
	}
	function activePlayers() {
		var count=0;
		for each(var p in game.players) {
			if(p.active) count++;
		}
		return count;
	}
	function killPlayer(playerName) {	
		if(!game.players[playerName]) {
			log(LOG_WARNING,"player does not exist: " + playerName);
			return false;
		}
		game.players[playerName].active=false;
		display(game.players[playerName],"\1n\1r[\1hGAME OVER\1n\1r]");
		
		if(activePlayers()<=1) {
			for (var p in game.players) {
				if(game.players[p].active)  {
					game.winner=p;
					break;
				}
			}
			endGame();
		}
	}
	function endGame() {
		game.status=2;
		client.write(game_id,"games." + game.gameNumber + ".status",game.status,2);
		send("UPDATE");
	}
			
	open();
	main();
	close();
}

