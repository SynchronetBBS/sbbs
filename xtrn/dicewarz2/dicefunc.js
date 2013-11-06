load("funclib.js");

var game_id=	"dicewarz2";
var settings=	client.read(game_id,"settings",1);
var ai=			client.read(game_id,"ai",1);
var data=		new Data();
var dice=		loadDice();

/* game data functions */
function connected(tile1,tile2,map) {
	for(var c=0;c<tile1.coords.length;c++) {
		var valid_directions=getNeighboringSectors(tile1.coords[c],map.grid);
		for(var d in valid_directions) {
			var dir=valid_directions[d];
			if(map.grid[dir.x][dir.y]==tile2.id) 
				return true;
		}
	}
	return false;
}
function getBorders(map,tile) {
	var borders=[];
	for(var c in tile.coords) {
		var coord=tile.coords[c];
		var neighbors=getNeighboringSectors(coord,map.grid);
		for(var n in neighbors) {
			var neighbor=neighbors[n];
			if(map.grid[neighbor.x][neighbor.y]!=tile.id) {
				if(!borders[neighbor.x]) 
					borders[neighbor.x]=[];
				borders[neighbor.x][neighbor.y]=true;
			}
		}
	}
	return borders;
}
function getNeighboringTiles(tile,map) {
	var found=[];
	var neighbors=[];
	for(var c=0;c<tile.coords.length;c++) {
		var valid_directions=getNeighboringSectors(tile.coords[c],map.grid);
		for(var d in valid_directions) {
			var dir=valid_directions[d];
			var t=map.grid[dir.x][dir.y];
			if(t>=0 && t!=tile.id && !found[t]) {
				found[t]=true;
				neighbors.push(map.tiles[t]);
			}
		}
	}
	return neighbors;
}
function getNeighboringSectors(relative_to_position,on_grid) {
	var xlimit=on_grid.length;
	var ylimit=on_grid[0].length;
	var offset=relative_to_position.x%2;
	var neighbors=[];
	var dir_names=["northwest","north","northeast","southwest","south","southeast"];
	var dir_x=[-1,0,1,-1,0,1];
	var dir_y=[-1+offset,-1,-1+offset,0+offset,1,0+offset];
	for(var i=0;i<dir_x.length;i++) {
		var x=parseInt(relative_to_position.x,10)+dir_x[i];
		var y=parseInt(relative_to_position.y,10)+dir_y[i];
		if(x>=xlimit || y>=ylimit || x<0 || y<0) 
			continue;
		neighbors[dir_names[i]]=new Coords(x,y);
	}
	return neighbors;
}
function getRandomDirection(from_position,on_grid) {
	var valid_directions=getNeighboringSectors(from_position,on_grid);
	var available=[];
	for(var d in valid_directions) {
		available.push(d);
	}
	while(available.length) {
		var rand=random(available.length);
		var random_dir=valid_directions[available[rand]];
		available.splice(rand,1);
		if(on_grid[random_dir.x][random_dir.y]>=0) 
			continue;
		return random_dir;
	}
	return false;
}
function landNearby(grid,x,y) {
	var valid_directions=getNeighboringSectors(new Coords(x,y),grid);
	var land=0;
	for(var d in valid_directions) {
		var coords=valid_directions[d];
		if(grid[coords.x][coords.y]>=0) 
			land++;
	}
	return land;
}
function getTile(grid,coords) {
	if(coords && grid[coords.x][coords.y]>=0) 
		return grid[coords.x][coords.y];
	else 
		return -1;
}
function canAttack(map,base) {
	var valid_targets=[];
	if(base.dice>1) {
		var neighbors=getNeighboringTiles(base,map);
		for(var n=0;n<neighbors.length;n++) {
			var tile=neighbors[n];
			if(tile.owner !== base.owner) {
				valid_targets.push(tile);
			}
		}
	}
	return valid_targets;
}
function getLargestCluster(map,tiles,playerNum) {	
	return countMembers(getAllClusters(map,tiles,playerNum)[0]);
}
function getAllClusters(map,tiles,playerNum) {
	/* keep track of which tiles have been searched for links */
	var counted={};
	/* store individual clusters in an array */
	var clusters=[];
	/* scan all player tiles, looking for connections to other tiles 
	belonging to the same player */
	for(var t=0;t<tiles.length;t++) {
		/* mark each tile as scanned */
		if(!counted[tiles[t].id]) {
			var connections={};
			connections[tiles[t].id]=true;
			/* trace all connected tiles from this point */
			var connections=trace(map,getBorders(map,tiles[t]),connections,playerNum);
			/* update overall scanned list with trace data */
			for(var c in connections) 
				counted[c]=true;
			clusters.push(connections);
		}
	}
	/* sort clusters from largest to smallest */
	clusters.sort(function(a,b) {
		return countMembers(b) - countMembers(a);
	});
	return clusters;
}
function trace(map,grid,connections,match) {
	for(var x in grid) {
		for(var y in grid[x]) {
			if(map.grid[x][y]>=0) {
				var tile=map.tiles[map.grid[x][y]];
				if(tile.owner==match && !connections[tile.id]) {
					connections[tile.id]=true;
					connections = trace(map,getBorders(map,tile),connections,match);
				}
			}
		}
	}
	return connections;
}

/* map initialization functions */
function addComputers(game) {
	var num=settings.num_players-game.players.length;
	if(num == 0)
		return;
	for(var a in ai) 
		ai[a].name = a;
	var aiList = compressList(ai);
	while(num>0) {
		var n=random(aiList.length);
		var a=aiList[n];
		var player=new Player(a.name,"AI",true);
		player.AI=new AI(a.sort,a.check,a.quantity);
		game.players.push(player);
		aiList.splice(n,1);
		num--;
	}
}
function addPlayer(game,name,sys_name,vote) {
	game.players.push(new Player(name,sys_name,vote));
}
function shufflePlayers(game) {
	game.players=shuffle(game.players);
}
function dispersePlayers(game,map) {
	var tiles_per_player=map.tiles.length/game.players.length;
	var placeholder=[];
	
	for(var p=0;p<game.players.length;p++) {
		var t=tiles_per_player;
		var occupied=[];
		
		while(t>0) {
			var rand_t=random(map.tiles.length);
			if(placeholder[rand_t]) continue;
			
			map.tiles[rand_t].owner = p;
			map.tiles[rand_t].dice = 1;
			placeholder[rand_t]=true;
			occupied.push(rand_t);
			t--;
		}

		var d=tiles_per_player;
		if(p==game.players.length-1) 
			d+=2;
		else if(p>=(game.players.length-1)/2) 
			d++;
		while(d>0)	{
			var rand_t=occupied[random(occupied.length)];
			if(map.tiles[rand_t].dice==settings.max_dice) 
				continue;
			map.tiles[rand_t].dice++;
			d--;
		}
	}
}
function generateMap(game) {		
	var map = new Map(game.gameNumber);
	
	var total=map.width*map.height;
	var min_size=settings.min_tile_size;
	var max_size=settings.max_tile_size;
	
	map.num_players=settings.num_players;
	var num_tiles=parseInt(total/(max_size-1),10);
	num_tiles-=num_tiles%game.players.length;
	
	for(var t=0;t<num_tiles;t++) {
		map.tiles[t]=new Tile(t);
		var size=random(max_size-min_size)+min_size;
		
		while(1) {
			x=random(map.width-1)+1;
			y=random(map.height-1)+1;
			
			if(map.grid[x][y]>=0) 
				continue;
			if(t==0) 
				break;
			
			var land_nearby=landNearby(map.grid,x,y);
			if(land_nearby>0 && land_nearby<6) 
				break;
		}
		
		var sector=new Coords(x,y);
		map.grid[sector.x][sector.y]=t;
		map.tiles[t].coords.push(new Coords(x,y));
		
		for(var s=1;s<size;s++)	{
			sector=getRandomDirection(sector,map.grid);
			if(!sector) 
				break;
			map.grid[sector.x][sector.y]=t;
			map.tiles[t].coords.push(sector);
		}
		
		var home=random(map.tiles[t].coords.length);
		map.tiles[t].home=map.tiles[t].coords[home];
	}
	
	return map;
}
function createGrid(w,h) {
	var grid=new Array(w);
	for(var x=0;x<grid.length;x++) 
		grid[x]=new Array(h);
	return grid;
}

/* map data functions */
function updateStatus(game,map) {
	/* count the tiles of all active players,
	if tiles == 0, player is dead */
	for(var p=0;p<game.players.length;p++) {
		if(game.players[p].active) {
			if(countTiles(map,p)==0) {
				//log(LOG_DEBUG,"inactive player: " + game.players[p].name);
				game.players[p].active=false;
				data.savePlayer(game,p);
			}
		}
	}
	/* for a single player game, find the non-AI player and check whether active. 
	if not, game over */
	if(game.single_player && game.status==status.PLAYING) {
		for(var p=0;p<game.players.length;p++) {
			var player=game.players[p];
			if(!player.AI && !player.active) {
				game.status=status.FINISHED;
				game.winner=getWinner(game,map);

				data.saveStatus(game);
				data.saveWinner(game);
				data.scoreWinner(game);
				return;
			}
		}
	}
	/* for all games, if there is only one active player left,
	that player is the winner */
	if(countActivePlayers(game)==1) {
		game.status=status.FINISHED;
		game.winner=getWinner(game,map);

		data.saveStatus(game);
		data.saveWinner(game);
		data.scoreWinner(game);
	}
}
function getWinner(game,map) {
	var winner=0;
	var most_tiles=0;
	var most_dice=0;
	
	for(var p=0;p<game.players.length;p++){
		if(game.players[p].active) {
			var num_tiles=countTiles(map,p);
			var num_dice=countDice(map,p);
			if(num_tiles>most_tiles) {
				most_tiles=num_tiles;
				most_dice=num_dice;
				winner=p;
			} else if(num_tiles==most_tiles) {
				if(num_dice>most_dice) {
					most_dice=num_dice;
					winner=p;
				}
			}
		}
	}
	return winner;
}
function getReinforcements(game,map,playerNumber) {
	var tiles=getPlayerTiles(map,playerNumber);
	var reinforcements=getLargestCluster(map,tiles,playerNumber);
	var player=game.players[playerNumber];
	var updated={};
	
	while(reinforcements>0 && tiles.length>0) {
		var rand=random(tiles.length);
		var tile=tiles[rand];
		if(tile.dice<settings.max_dice) {
			tile.dice++;
			reinforcements--;
			if(!updated[tile.id]) 
				updated[tile.id]=0;
			updated[tile.id]++;
		} 
		else {
			tiles.splice(rand,1);
		}
	}
	
	while(player.reserve>0 && tiles.length>0) {
		var rand=random(tiles.length);
		var tile=tiles[rand];
		if(tile.dice<settings.max_dice) {
			tile.dice++;
			player.reserve--;
			if(!updated[tile.id]) 
				updated[tile.id]=0;
			updated[tile.id]++;
		} else {
			tiles.splice(rand,1);
		}
	}
	
	/* put remaining reinforcements in reserve */
	if(reinforcements > 0 && player.reserve < settings.max_reserve) {
		player.reserve+=reinforcements;
		if(player.reserve > settings.max_reserve) {
			reinforcements -= (player.reserve-settings.max_reserve);
			player.reserve = settings.max_reserve;
		}
		data.savePlayer(game,playerNumber);
	}
	
	var totalPlaced=0;
	for(var t in updated) {
		data.saveTile(map,map.tiles[t]);
		totalPlaced+=updated[t];
	}
	
	return {
		tiles:updated,
		placed:totalPlaced,
		reserved:reinforcements
	};
}
function nextTurn(game) {
	/* if we reached the end of the player list reset to beginning */
	if(game.turn==game.players.length-1) {
		game.turn=0;
		game.round++;
		data.saveRound(game);
	} 
	else {
		game.turn++;
	}
	
	/* skip inactive players, recursively */
	if(!game.players[game.turn].active) {
		nextTurn(game);
	}
	else {
		data.saveTurn(game);
	}
}

/* player data functions */
function countActivePlayers(game) {
	var count=0;
	for(var p=0;p<game.players.length;p++) {
		if(game.players[p].active) count++;
	}
	return count;
}
function getPlayerTiles(map,playerNumber) {
	var tiles=[];
	for(var t in map.tiles) {
		if(map.tiles[t].owner==playerNumber)
			tiles.push(map.tiles[t]);
	}
	return tiles;
}
function findPlayer(game,name) {
	for(var p in game.players) {
		if(game.players[p].name==name) 
			return p;
	}
	return -1;
}
function countDice(map,playerNumber) {
	var dice=0;
	var tiles=getPlayerTiles(map,playerNumber);
	for(var t=0;t<tiles.length;t++) 
		dice+=tiles[t].dice;
	return dice;
}
function countTiles(map,playerNumber) {
	return getPlayerTiles(map,playerNumber).length;
}

/* dice rolling */
function loadDice() {								
	var dice=[];
	for(d=1;d<=6;d++)
	{
		dice[d]=new Die(d);
	}
	return dice;
}



