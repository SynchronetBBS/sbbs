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
		if(userNumber) this.users[userNumber]=this.players.length;
		this.players.push(new Player(userNumber,vote));
	}
	this.tallyVotes=function()
	{
		var trueVotes=0;
		for(v in this.players)
		{
			if(this.players[v].user>0 && this.players[v].vote==1) 
			{
				trueVotes++;
			}
		}
		if(trueVotes==this.countHumanPlayers()) 
		{
			if(this.countPlayers()>=this.minPlayers) 
			{	
				return true;
			}
		}
		else return false;
	}
	this.countHumanPlayers=function()
	{
		var count=0;
		for(pppp in this.players)
		{
			if(this.players[pppp].user>0) count++;
		}
		return count;
	}
	this.countPlayers=function()
	{
		var count=0;
		for(pppp in this.players)
		{
			count++;
		}
		return count;
	}
	this.getVote=function(playerNumber)
	{
		if(this.players[playerNumber].vote==0) return("wait");
		else return("start");
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
	this.maxDice=maxDice;					//MAXIMUM DICE PER TERRITORY
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
		nextTurn=this.nextTurn;
		nextTurnPlayer=this.players[this.turnOrder[nextTurn]].user;
		
		if(this.status!=0)
		{
			if(this.countActiveHumans<2) 
			{
				return;
			}
			if(nextTurnPlayer>0 && nextTurnPlayer!=user.number) 
			{
				deliverMessage(nextTurnPlayer,this.gameNumber);
			}
			else
			{
				while(1)
				{
					if(nextTurn==this.maxPlayers-1) nextTurn=0;
					else nextTurn++;
					nextTurnPlayer=this.players[this.turnOrder[nextTurn]].user;
					if(nextTurnPlayer==this.players[this.turnOrder[this.nextTurn]].user) 
					{
						break;
					}
					else if(nextTurnPlayer>0) 
					{
						deliverMessage(nextTurnPlayer,this.gameNumber);
						break;
					}
				}
			}
		}
	}
	this.findDaysOld=			function()	//DETERMINE THE LAST TIME A GAME FILE WAS MODIFIED (LAST TURN TAKEN)
	{
		daysOld=(time()-this.lastModified)/daySeconds;
		return daysOld;
	}
	this.checkElimination=		function()
	{
		var numEliminated=this.eliminated.length;
		var humans=this.countActiveHumans();
		if(numEliminated==(this.maxPlayers-1) || humans==0)
		{
			this.winner=this.lastEliminator;
			if(this.lastEliminator>=0)
			{	
				this.assignPoints();
			}
			this.status=0;
			return true;
		}
		else return false;
	}
	this.countActivePlayers=	function()
	{
		var activePlayers=[];
		for(player in this.players)
		{
			if(!this.players[player].eliminated) activePlayers.push(player);
		}
		return activePlayers;
	}
	this.countActiveHumans=		function()
	{
		count=0;
		for(ply in this.players)
		{
			if(this.players[ply].user>0 && !this.players[ply].eliminated) count++;
		}
		return count;
	}
	this.assignPoints=			function()	//TODO: REWORK SCORING SYSTEM COMPLETELY
	{
		games.loadRankings();
		if(this.singlePlayer) 
		{
			scores[this.winner].wins+=1;
		}
		else 
		{
			var points=this.players.length<5?1:2;
			scores[this.winner].score=parseInt(scores[this.winner].score,10) + points;
		}
		games.storeRankings();
	}
	this.eliminatePlayer=		function(playerNumber,eliminator)
	{
		dead=this.players[playerNumber];
		dead.eliminated=true;
		this.eliminated.push(playerNumber);
		this.lastEliminator=eliminator;
		if(dead.user>0) 
		{
			games.loadRankings();
			if(this.singlePlayer)
			{
				scores[dead.user].losses+=1;
				this.status=0;
			}
			else
			{
				var pointBuffer=7-this.maxPlayers;
				var offset=(this.players.length<5?2:1);
				pts=points[pointBuffer+(this.eliminated.length-offset)];
				scores[dead.user].score=parseInt(scores[dead.user].score,10) + pts;
				if(scores[dead.user].score<minScore) scores[dead.user].score=parseInt(minScore,10);
			}
			games.storeRankings();
		}
	}
	this.setEliminated=			function()
	{										//RUNS AT STARTUP, STORING GAME ELIMINATION DATA UPON LOADING EACH GAME
		this.eliminated=[];
		for(elp in this.players)
		{
			if(this.players[elp].territories.length<=0) 
			{
				this.eliminated.push(elp);
				this.players[elp].eliminated=true;
			}
			else this.lastEliminator=this.players[elp].user;
		}
		if(this.status==0) this.winner=this.lastEliminator;
	}
	this.getNextTurn=			function()
	{
		if(this.nextTurn==this.maxPlayers-1) this.nextTurn=0;
		else this.nextTurn++;
		nextPlayer=this.turnOrder[this.nextTurn];
		while(this.players[nextPlayer].eliminated) 
		{
			if(this.nextTurn==this.maxPlayers-1) this.nextTurn=0;
			else this.nextTurn++;
			nextPlayer=this.turnOrder[this.nextTurn];
		}
	}
	this.setMap=				function()	
	{										//SET MAP SIZE
		this.playerTerr=this.minTerr+random(this.maxTerr-this.minTerr);
		this.mapSize=this.maxPlayers*(this.playerTerr);
	}								
	this.displayGrid=			function()
	{										//DISPLAYS THE LOCATION DATA FOR EACH COMPANY
		drawVerticalLine("\1w\1h");
		for(uu in this.used)
		{
			this.grid[uu].displayBorder(border_color);
		}
	}
	this.setGrid=				function()
	{
		for(uu in this.used)
		{
			this.grid[uu].setBorder(this.grid);
		}
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
		for(ply in this.players)
		{
			for(ter in this.players[ply].territories)
			{
				territory=this.players[ply].territories[ter];
				this.grid[territory].show();
			}
		}
	}	
	this.getXY=					function(place)
	{										//TAKE A GRID INDEX, AND RETURNS THE CORRESPONDING X AND Y COORDINATES FOR DISPLAY
		x=this.map_column;
		y=this.map_row;
		x+=((place%this.columns)*2);
		y+=(parseInt(index/this.columns));
		console.gotoxy(x,y);
		return(0);
	}
	this.reinforce=				function(playerNumber)
	{
		numDice=this.findConnected(playerNumber);
		placed=this.placeDice(playerNumber,numDice);
		if(this.winner<0) this.checkElimination();
		this.getNextTurn();
		if(!this.singlePlayer) 
		{
			if(this.players[playerNumber].user>0)
				this.notify();
		}
		return placed;
	}
	this.canAttack=				function(playerNumber,mapLocation)
	{										//RETURNS TRUE IF PLAYER : playerNumber CAN ATTACK FROM GRID INDEX : mapLocation
		if(mapLocation>=0)		//IF A LOCATION IS PROVIDED, RETURN AN ARRAY OF NEIGHBORING TERRITORIES THAT CAN BE ATTACKED
		{
			options=[];
			if(this.grid[mapLocation].dice>1)
			{
				dirs=this.loadDirectional(mapLocation);
				for(dir in dirs)
				{
					current=dirs[dir];
					if(this.grid[current]) 
					{
						if(this.grid[current].player!=playerNumber) options.push(current);
					}
				}
			}
			if(options.length) return options;
		}
		else				//OTHERWISE, SIMPLY DETERMINE WHETHER THE PLAYER CAN ATTACK AT ALL, AND RETURN TRUE OR FALSE
		{
			if(this.players[playerNumber].territories.length==this.players[playerNumber].totalDice) return false;
			for(terr in this.players[playerNumber].territories)
			{
				currentTerritory=this.players[playerNumber].territories[terr];
				if(this.grid[currentTerritory].dice>1)
				{
					dirs=this.loadDirectional(currentTerritory);
					for(dir in dirs)
					{
						current=dirs[dir];
						if(this.grid[current]) 
						{
							if(this.grid[current].player!=playerNumber) return true;
						}
					}
				}
			}
		}
		return false;
	}
	this.loadDirectional=		function(mapLocation)
	{
		current=this.grid[mapLocation];
		n=current.north;
		s=current.south;
		nw=current.northwest;
		ne=current.northeast;
		sw=current.southwest;
		se=current.southeast;
		dirs=[n,s,nw,ne,sw,se];
		return dirs;
	}
	this.findConnected=			function(playerNumber)
	{										//SCANS ENTIRE MAP AND RETURNS THE NUMBER EQUAL TO THE PLAYER'S LARGEST CLUSTER OF CONNECTED TILES
		largest_cluster=1;
		terr=this.players[playerNumber].territories;
		var checked=[];
		var counted=[];
		var tocheck=[];
		y=10;
		for(tttt in terr)
		{
			count=1;
			tocheck.push(terr[tttt]);
			while(tocheck.length)
			{	
				loc=tocheck.shift();
				current=this.grid[loc];
				
				if(!checked[current.location])
				{
					dirs=scanProximity(current.location);
					for(ddd in dirs)
					{
						dir=dirs[ddd];
						if(this.grid[dir] && !checked[dir]) 
						{
							if(this.grid[dir].player==playerNumber) 
							{ 
								tocheck.push(dir); 
								if(!counted[dir]) 
								{ 
									count++; 
									counted[dir]=true;
								}
							}
						}
					}
					checked[current.location]=true;
				}
			}
			if(count>largest_cluster) largest_cluster=count;
		}
		return largest_cluster;
	}
	this.displayPlayers=		function()
	{										//DISPLAY PLAYER INFO (RIGHT SIDE)
		var xxx=menuColumn;
		var yyy=menuRow;
		
		for(var ply=0;ply<this.maxPlayers;ply++)
		{
			var playerNumber=this.turnOrder[ply];
			this.countDice(playerNumber);
			var player=this.players[playerNumber];
			var name=getUserName(player,playerNumber);
			
			if(player.eliminated) 
			{
				player.bColor=blackbg; 
				player.fColor="\1n\1k\1h"; 
			}
			else if(this.status!=0 && this.hiddenNames && name!=user.alias)
			{
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
		for(pp=0;pp<this.maxPlayers;pp++)
		{
			var rand=random(this.maxPlayers);
			if(this.turnOrder[rand]>=0) pp--;
			else this.turnOrder[rand]=pp;
		}
	}
	this.generatePlayers=		function()
	{
		for(pl=0;pl<this.maxPlayers;pl++)
		{
			userNumber=this.players[pl].user;
			this.users[userNumber]=pl;
			this.players[pl].setColors(pl);
			this.players[pl].starting_territories=this.playerTerr;
		}
		this.ShufflePlayers();
	}
	this.generateMap=			function()
	{										//RANDOMLY Generate NEW LAND
		var unused=[];
		for(mi=0;mi<this.total;mi++) unused.push(mi);
		randa=random(unused.length);
		locationa=unused[randa];
		this.grid[locationa]=new Territory(locationa);
		this.used[locationa]=true;
		unused.splice(randa,1); 
		
		for(ms=1;ms<this.mapSize;ms++)
		{
			randb=random(unused.length);
			locationb=unused[randb];
			prox=scanProximity(locationb);
			if(this.landNearby(prox, this.used))
			{
	 			this.grid[locationb]=new Territory(locationb);
				this.used[locationb]=true;
				unused.splice(randb,1); 
			}
			else ms--;
		}
		var territories=[];
		for(mt in this.used)
		{
			territories.push(mt);
		}
		for(ply in this.players)
		{
			for(tt=0;tt < this.playerTerr; tt++)
			{
				rand=random(territories.length);
				location=territories[rand];
				if(this.grid[location].player>=0) tt--;
				else 
				{
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
		for(px in proximity)
		{
			location=proximity[px];
			if(this.used[location]) 
			{
				return true;
			}
		}
		return false;
	}
	this.placeDice=				function(playerNum, numDice)
	{										//RANDOMLY PLACE X NUMBER OF DICE ON PLAYER TERRITORIES
		playerNumber=parseInt(playerNum);
		this.countDice(playerNumber);
		toPlace=numDice;
		fulldice=false;
		placed=[];
		for(sd=0;sd<numDice;sd++)
		{
			rand=random(this.players[playerNumber].territories.length);
			terr=this.players[playerNumber].territories[rand];

			if(this.players[playerNumber].totalDice==(this.players[playerNumber].territories.length*this.maxDice)) 	//IF ALL OF THIS PLAYER'S TERRITORIES HAVE THE MAXIMUM
			{ 									  						//AMOUNT OF DICE  PUSH DICE INTO PLAYER'S RESERVE
				fulldice=true;
				reserveCap=(30-this.players[playerNumber].reserve);
				if(reserveCap>0)
				{
					if(reserveCap>=toPlace) this.players[playerNumber].reserve+=toPlace;
					else this.players[playerNumber].reserve+=reserveCap;
				}
				return placed;
			}
			else if(this.grid[terr].dice<this.maxDice) 
			{
				this.grid[terr].dice++;
				placed.push(terr);
				this.players[playerNumber].totalDice++;
				toPlace--;
			}
			else
			{
				if(this.players[playerNumber].territories.length==1) return placed;
				sd--;
			}
		}
		if(this.players[playerNumber].reserve>0)
		{
			if(this.players[playerNumber].totalDice==(this.players[playerNumber].territories.length*this.maxDice)) 	//IF ALL OF THIS PLAYER'S TERRITORIES HAVE THE MAXIMUM
			{ 									  						//AMOUNT OF DICE  PUSH DICE INTO PLAYER'S RESERVE
				fulldice=true;
			}
			else
			{
				while(this.players[playerNumber].reserve>0)
				{
					rand=random(this.players[playerNumber].territories.length);
					terr=this.players[playerNumber].territories[rand];
					if(this.grid[terr].dice<this.maxDice)
					{
						this.grid[terr].dice++;
						placed.push(terr);
						this.players[playerNumber].totalDice++;
						this.players[playerNumber].reserve--;
						if(this.players[playerNumber].totalDice==(this.players[playerNumber].territories.length*this.maxDice)) 	//IF ALL OF THIS PLAYER'S TERRITORIES HAVE THE MAXIMUM
						{
							return placed;
						}
					}
				}
			}
		}
		return placed;
	}
	this.countDice=				function(playerNumber)
	{										//COUNT DICE TOTALS FOR EACH PLAYER
		this.players[playerNumber].totalDice=0;
		for(td in this.players[playerNumber].territories)
		{
			terr=this.players[playerNumber].territories[td];
			this.players[playerNumber].totalDice+=this.grid[terr].dice;
		}
	}
	this.countDiceAll=			function()
	{
		for(ppp in this.players)
		{
			this.countDice(ppp);
		}
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
