function	NewGame(minp,maxp,n)
{
	this.status=-1;
	this.minPlayers=minp;
	this.maxPlayers=maxp;
	this.gameNumber=n;
	this.players=[];
	this.users=[];
	this.fixedPlayers=false;
	this.singlePlayer=false;
	this.hiddenNames=false;
	this.lastModified=0;
	this.fileName="";
	
	this.addPlayer=function(userNumber,vote)
	{
		if(userNumber) 
			this.users[userNumber]=this.players.length;
		this.players.push(new Player(userNumber,vote));
	}
	this.tallyVotes=function()
	{
		var trueVotes=0;
		for(var v in this.players) {
			if(this.players[v].user>0 && this.players[v].vote==1) {
				trueVotes++;
			}
		}
		if(trueVotes==this.countHumanPlayers()) {
			if(this.countPlayers()>=this.minPlayers) 
				return true;
		}
		else 
			return false;
	}
	this.countHumanPlayers=function()
	{
		var count=0;
		for(var pppp in this.players) {
			if(this.players[pppp].user>0) 
				count++;
		}
		return count;
	}
	this.countPlayers=function()
	{
		var count=0;
		for(var pppp in this.players) {
			count++;
		}
		return count;
	}
	this.getVote=function(playerNumber)
	{
		if(this.players[playerNumber].vote==0) 
			return("wait");
		else 
			return("start");
	}
	this.removePlayer=function(playerNumber)
	{
		this.players.splice(playerNumber,1);
		this.users.splice(user.number,1);
	}
}
function 	Map(c,r,p,gn) 				
{								
									//OBJECT VARIABLES
	this.grid=[]; 						//ROWS * COLUMNS IN LENGTH, TRACKS GRID OCCUPANCY
	this.status=1;						//GAME STATUS INDICATOR
	this.takingTurn=false;
	this.singlePlayer=false;
	this.hiddenNames=false;
	this.eliminated=[];
	this.lastEliminator=-1;
	this.lastMapPosition=-1;
	this.winner=-1;
	this.lastModified=-1;
	this.fileName="";
	this.gameNumber=gn;
	
	this.rows=r;						//MAP ROWS 
	this.columns=c;						//MAP COLUMNS
	this.maxDice=settings.maxDice;		//MAXIMUM DICE PER TERRITORY
	this.maxPlayers=p;					//NUMBER OF PLAYERS IN GAME
	this.nextTurn=0;

	this.playerTerr=-1;					//initIALIZE TERRITORIES PER PLAYER (SET DURING init BY this.setMap())
	this.used=[];						//initIALIZE USED MAP SECTORS (FOR GENERATION OF MAP)
	this.turnOrder=[];					//initIALIZE PLAYER TURN ORDER
	this.players=[];					//initIALIZE PLAYER DATA
	this.users=[];						//ATTACH SYSTEM USER NUMBER TO GAME PLAYER NUMBER
	
	this.total=this.columns*this.rows;	//TOTAL NUMBER OF POSSIBLE GRID LOCATIONS
	this.mapSize=-1;					//initIALIZE MAP SIZE (NOT THE SAME AS TOTAL)
	this.minTerr=6;						//MINIMUM FOR RANDOM TERRITORY GENERATION PER PLAYER
	this.maxTerr=11;					//MAXIMUM FOR RANDOM TERRITORY GENERATION PER PLAYER
	
	//NOTIFY NEXT PLAYER OF TURN 
	this.notify=				function()	//NOTIFY NEXT PLAYER OF TURN
	{
		var nextTurn=this.nextTurn;
		var nextTurnPlayer=this.players[this.turnOrder[nextTurn]].user;
		
		if(this.status!=0 && !this.singlePlayer) {
			if(this.countActiveHumans<2) 
				return;
			if(nextTurnPlayer>0 && nextTurnPlayer!=user.number) 
				deliverMessage(nextTurnPlayer,this.gameNumber);
			else {
				while(1) {
					if(nextTurn==this.maxPlayers-1) 
						nextTurn=0;
					else 
						nextTurn++;
					nextTurnPlayer=this.players[this.turnOrder[nextTurn]].user;
					if(nextTurnPlayer==this.players[this.turnOrder[this.nextTurn]].user) 
						break;
					else if(nextTurnPlayer>0) {
						deliverMessage(nextTurnPlayer,this.gameNumber);
						break;
					}
				}
			}
		}
	}
	this.findDaysOld=			function()	//DETERMINE THE LAST TIME A GAME FILE WAS MODIFIED (LAST TURN TAKEN)
	{
		return((time()-this.lastModified)/daySeconds);
	}
	this.checkElimination=		function()
	{
		var numEliminated=this.eliminated.length;
		var humans=this.countActiveHumans();
		if(numEliminated==(this.maxPlayers-1) || humans==0)	{
			this.winner=this.lastEliminator;
			if(this.lastEliminator>0)
				this.assignPoints();
			this.status=0;
			if(this.singlePlayer) 
				file_remove(this.fileName);
			return true;
		}
		else return false;
	}
	this.countActivePlayers=	function()
	{
		var activePlayers=[];
		for(var player in this.players) {
			if(!this.players[player].eliminated) 
				activePlayers.push(player);
		}
		return activePlayers;
	}
	this.countActiveHumans=		function()
	{
		var count=0;
		for(var ply in this.players) {
			if(this.players[ply].user>0 && !this.players[ply].eliminated) 
				count++;
		}
		return count;
	}
	this.assignPoints=			function()	
	{
		games.loadRankings();
		scores[this.winner].wins+=1;
		if(this.singlePlayer) 
			scores[this.winner].score+=settings.winPointsSolo;
		else 
			scores[this.winner].score+=settings.winPointsMulti;
		games.storeRankings();
	}
	this.eliminatePlayer=		function(playerNum1,playerNum2)
	{
		var dead=this.players[playerNum1];
		var killer=this.players[playerNum2];
		
		dead.eliminated=true;
		this.eliminated.push(playerNum1);
		this.lastEliminator=killer.user;
		var updaterankings=false;
		
		if(killer.user>0) {
			games.loadRankings();
			if(!scores[killer.user]) {
				scores[killer.user] = {
					'score':0,
					'kills':0,
					'wins':0,
					'losses':0
				}
			}
			if(this.singlePlayer) 
				scores[killer.user].score+=settings.killPointsSolo;
			else 
				scores[killer.user].score+=settings.killPointsMulti;
			scores[killer.user].kills+=1;
			updaterankings=true;
		}
		if(dead.user>0) {
			if(!updaterankings)	{
				games.loadRankings();
				updaterankings=true;
			}
			if(!scores[dead.user]) {
				scores[dead.user] = {
					'score':0,
					'kills':0,
					'wins':0,
					'losses':0
				}
			}
			scores[dead.user].losses+=1;
			if(this.singlePlayer) {
				this.status=0;
				file_remove(this.fileName);
				scores[dead.user].score+=settings.deathPointsSolo;
			}
			else {
				var kname= (killer.user<1?this.players[playerNum2].AI.name:system.username(killer.user));
				deliverKillMessage(kname,dead.user,this.gameNumber);
				scores[dead.user].score+=settings.deathPointsMulti;
				if(scores[dead.user].score<settings.minScore) 
					scores[dead.user].score=settings.minScore;
			}
		}
		if(updaterankings) 
			games.storeRankings();
		if(this.checkElimination()) 
			games.storeGame(this.gameNumber); 
	}
	this.setEliminated=			function()
	{										//RUNS AT STARTUP, STORING GAME ELIMINATION DATA UPON LOADING EACH GAME
		this.eliminated=[];
		for(var elp in this.players) {
			if(this.players[elp].territories.length<=0) {
				this.eliminated.push(elp);
				this.players[elp].eliminated=true;
			}
			else 
				this.lastEliminator=this.players[elp].user;
		}
		if(this.status==0) 
			this.winner=this.lastEliminator;
	}
	this.getNextTurn=			function()
	{
		if(this.nextTurn==this.maxPlayers-1) 
			this.nextTurn=0;
		else 
			this.nextTurn++;
		var nextPlayer=this.turnOrder[this.nextTurn];
		while(this.players[nextPlayer].eliminated) {
			if(this.nextTurn==this.maxPlayers-1) 
				this.nextTurn=0;
			else 
				this.nextTurn++;
			nextPlayer=this.turnOrder[this.nextTurn];
		}
	}
	this.setMap=				function()	
	{										//SET MAP SIZE
		var rand=random(this.maxTerr-this.minTerr);
		while(rand < 0 || rand > (this.maxTerr-this.minTerr))
			rand=random(this.maxTerr-this.minTerr);
		this.playerTerr=this.minTerr+rand;
		this.mapSize=this.maxPlayers*(this.playerTerr);
	}								
	this.displayGrid=			function()
	{										//DISPLAYS THE LOCATION DATA FOR EACH COMPANY
		for(var uu in this.used)
			this.grid[uu].displayBorder(border_color);
	}
	this.setGrid=				function()
	{
		for(var uu in this.used)
			this.grid[uu].setBorder(this.grid);
	}
	this.redraw=				function()
	{
		this.displayPlayers();
		this.displayGrid();
		this.displayGame();
		wipeCursor("left");
	}
	this.displayGame=			function()
	{										//DISPLAY EACH PLAYER'S TERRITORIES
		for(var ply in this.players) {
			for(var ter in this.players[ply].territories) {
				var territory=this.players[ply].territories[ter];
				this.grid[territory].show();
			}
		}
	}	
	this.getXY=					function(place)
	{										//TAKE A GRID INDEX, AND RETURNS THE CORRESPONDING X AND Y COORDINATES FOR DISPLAY
		var x=this.map_column;
		var y=this.map_row;
		x+=((place%this.columns)*2);
		y+=(parseInt(index/this.columns));
		console.gotoxy(x,y);
		return;
	}
	this.reinforce=				function(playerNumber)
	{
		var numDice=this.findConnected(playerNumber);
		var placed=this.placeDice(playerNumber,numDice);
		if(this.winner<0) 
			this.checkElimination();
		this.getNextTurn();
		if(this.players[playerNumber].user>0)
			this.notify();
		return placed;
	}
	this.canAttack=				function(playerNumber,mapLocation)
	{										//RETURNS TRUE IF PLAYER : playerNumber CAN ATTACK FROM GRID INDEX : mapLocation
		if(mapLocation>=0) {
			if(this.grid[mapLocation].player!=playerNumber) 
				return false;
			var options=[];
			if(this.grid[mapLocation].dice>1) {
				var dirs=this.loadDirectional(mapLocation);
				for(dir in dirs) {
					var current=dirs[dir];
					if(this.grid[current]) {
						if(this.grid[current].player!=playerNumber) 
							options.push(current);
					}
				}
			}
			if(options.length) 
				return options;
			else 
				return false;
		}
		else {			//OTHERWISE, SIMPLY DETERMINE WHETHER THE PLAYER CAN ATTACK AT ALL, AND RETURN TRUE OR FALSE
			if(this.players[playerNumber].territories.length==this.players[playerNumber].totalDice) 
				return false;
			for(terr in this.players[playerNumber].territories) {
				var currentTerritory=this.players[playerNumber].territories[terr];
				if(this.grid[currentTerritory].dice>1) {
					var dirs=this.loadDirectional(currentTerritory);
					for(dir in dirs) {
						var current=dirs[dir];
						if(this.grid[current]) {
							if(this.grid[current].player!=playerNumber) 
								return true;
						}
					}
				}
			}
		}
		return false;
	}
	this.loadDirectional=		function(mapLocation)
	{
		var current=this.grid[mapLocation];
		var n=current.north;
		var s=current.south;
		var nw=current.northwest;
		var ne=current.northeast;
		var sw=current.southwest;
		var se=current.southeast;
		return [n,s,nw,ne,sw,se];;
	}
	this.findConnected=			function(playerNumber)
	{										//SCANS ENTIRE MAP AND RETURNS THE NUMBER EQUAL TO THE PLAYER'S LARGEST CLUSTER OF CONNECTED TILES
		var largest_cluster=1;
		var terr=this.players[playerNumber].territories;
		var checked=[];
		var counted=[];
		var tocheck=[];
		for(tttt in terr) {
			var count=1;
			tocheck.push(terr[tttt]);
			while(tocheck.length) {	
				var loc=tocheck.shift();
				var current=this.grid[loc];
				
				if(!checked[current.location]) {
					var dirs=scanProximity(current.location);
					for(ddd in dirs) {
						var dir=dirs[ddd];
						if(this.grid[dir] && !checked[dir]) {
							if(this.grid[dir].player==playerNumber) { 
								tocheck.push(dir); 
								if(!counted[dir]) { 
									count++; 
									counted[dir]=true;
								}
							}
						}
					}
					checked[current.location]=true;
				}
			}
			if(count>largest_cluster) 
				largest_cluster=count;
		}
		return largest_cluster;
	}
	this.displayPlayers=		function()
	{										//DISPLAY PLAYER INFO (RIGHT SIDE)
		drawVerticalLine("\1w\1h");
		var xxx=menuColumn;
		var yyy=menuRow;
		
		for(var ply=0;ply<this.maxPlayers;ply++) {
			var playerNumber=this.turnOrder[ply];
			this.countDice(playerNumber);
			var player=this.players[playerNumber];
			var name=getUserName(player,playerNumber);
			
			if(player.eliminated) {
				player.bColor=blackbg; 
				player.fColor="\1n\1k\1h"; 
			}
			else if(this.status!=0 && this.hiddenNames && name!=user.alias)	{
				name="Player " + (playerNumber+1);
			}
			
			console.gotoxy(xxx,yyy); yyy++;
			console.putmsg(printPadded(player.bColor + player.fColor + " " + name + ":",36," ", "left"));
			console.gotoxy(xxx,yyy); yyy++;
			console.putmsg(player.fColor + player.bColor+ " LAND: " + printPadded(player.fColor + player.territories.length,3," ", "right"));
			console.putmsg(player.fColor + player.bColor+ " DICE: " + printPadded(player.fColor + player.totalDice,3," ", "right"));
			console.putmsg(player.fColor + player.bColor+ " RSRV: " + printPadded(player.fColor + player.reserve,3," ", "right") + " ");
		}
		console.print(blackbg);
	}
	this.ShufflePlayers=		function()
	{										//Generate TURN ORDER (this.turnOrder[])
		for(var pp=0;pp<this.maxPlayers;pp++) {
			var rand=random(this.maxPlayers);
			if(this.turnOrder[rand]>=0) 
				pp--;
			else 
				this.turnOrder[rand]=pp;
		}
	}
	this.generatePlayers=		function()
	{
		for(var pl=0;pl<this.maxPlayers;pl++) {
			var userNumber=this.players[pl].user;
			this.users[userNumber]=pl;
			this.players[pl].setColors(pl);
			this.players[pl].starting_territories=this.playerTerr;
		}
		this.ShufflePlayers();
	}
	this.generateMap=			function()
	{										//RANDOMLY Generate NEW LAND
		var unused=[];
		for(mi=0;mi<this.total;mi++) 
			unused.push(mi);
		var randa=random(unused.length);
		while(!unused[randa])
			randa=random(unused.length);
		var locationa=unused[randa];
		this.grid[locationa]=new Territory(locationa);
		this.used[locationa]=true;
		unused.splice(randa,1); 
		
		for(var ms=1;ms<this.mapSize;ms++) {
			var randb=random(unused.length);
			while(!unused[randb])
				randb=random(unused.length);
			var locationb=unused[randb];
			var prox=scanProximity(locationb);
			if(this.landNearby(prox, this.used)) {
	 			this.grid[locationb]=new Territory(locationb);
				this.used[locationb]=true;
				unused.splice(randb,1); 
			}
			else 
				ms--;
		}
		var territories=[];
		for(var mt in this.used)
			territories.push(mt);
		for(var ply in this.players) {
			for(tt=0;tt < this.playerTerr; tt++) {
				var rand=random(territories.length);
				while(!territories[rand])
					rand=random(territories.length);
				var location=territories[rand];
				if(this.grid[location].player>=0) 
					tt--;
				else {
					this.grid[location].assign(ply,this.players[ply]);
					this.players[ply].territories.push(location);
				}
			}
			this.placeDice(ply,this.playerTerr);
		}
		this.countDiceAll();
	}	
	this.landNearby=			function(proximity)
	{										//CHECK IMMEDIATE AREA FOR LAND
		for(var px in proximity) {
			var location=proximity[px];
			if(this.used[location]) 
				return true;
		}
		return false;
	}
	this.placeDice=				function(playerNum, numDice)
	{										//RANDOMLY PLACE X NUMBER OF DICE ON PLAYER TERRITORIES
		var playerNumber=Number(playerNum);
		this.countDice(playerNumber);
		var toPlace=numDice;
		var fulldice=false;
		var placed=[];
		for(var sd=0;sd<numDice;sd++) {
			var rand=random(this.players[playerNumber].territories.length);
			while(!this.players[playerNumber].territories[rand])
				rand=random(this.players[playerNumber].territories.length);
			var terr=this.players[playerNumber].territories[rand];
			if(this.players[playerNumber].totalDice==(this.players[playerNumber].territories.length*this.maxDice)) {
				fulldice=true;
				var reserveCap=(30-this.players[playerNumber].reserve);
				if(reserveCap>0) {
					if(reserveCap>=toPlace) 
						this.players[playerNumber].reserve+=toPlace;
					else 
						this.players[playerNumber].reserve+=reserveCap;
				}
				return placed;
			}
			else if(this.grid[terr].dice<this.maxDice) {
				this.grid[terr].dice++;
				placed.push(terr);
				this.players[playerNumber].totalDice++;
				toPlace--;
			}
			else {
				if(this.players[playerNumber].territories.length==1) 
					return placed;
				sd--;
			}
		}
		if(this.players[playerNumber].reserve>0) {
			if(this.players[playerNumber].totalDice==(this.players[playerNumber].territories.length*this.maxDice)) 	
				fulldice=true;
			else {
				while(this.players[playerNumber].reserve>0)	{
					var rand=random(this.players[playerNumber].territories.length);
					while(!this.players[playerNumber].territories[rand])
						rand=random(this.players[playerNumber].territories.length);
					var terr=this.players[playerNumber].territories[rand];
					if(this.grid[terr].dice<this.maxDice) {
						this.grid[terr].dice++;
						placed.push(terr);
						this.players[playerNumber].totalDice++;
						this.players[playerNumber].reserve--;
						if(this.players[playerNumber].totalDice==(this.players[playerNumber].territories.length*this.maxDice))
							return placed;
					}
				}
			}
		}
		return placed;
	}
	this.countDice=				function(playerNumber)
	{										//COUNT DICE TOTALS FOR EACH PLAYER
		this.players[playerNumber].totalDice=0;
		for(var td in this.players[playerNumber].territories) {
			var terr=this.players[playerNumber].territories[td];
			this.players[playerNumber].totalDice+=this.grid[terr].dice;
		}
	}
	this.countDiceAll=			function()
	{
		for(var ppp in this.players)
			this.countDice(ppp);
	}
	this.init=					function()
	{										//initIALIZE GAME
		this.setMap();
		this.generatePlayers();
		this.generateMap(this.columns, this.rows);
		this.setGrid();
	}
									//END METHODS 
}
