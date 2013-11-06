load("json-client.js");

var gameNumber=argv[0];
var root=argv[1];
var client,game,map;

initClient();
load(root+"diceobj.js");
load(root+"dicefunc.js");

var sortFunctions={
	all:allAISort, 
	random:randomAISort, 
	killLeast:killLeastDiceAISort, 
	killMost:killMostDiceAISort, 
	paranoid:paranoidAISort, 
	stupid:stupidAISort,
	groupParanoid:groupAndParanoidAISort,
	cluster:clusterAISort,
	killFirst:killFirstPlaceAISort,
	killLast:killLastPlaceAISort
};
var checkFunctions={
	all:allAICheck, 
	random:randomAICheck,
	paranoid:paranoidAICheck, 
	wild:wildAndCrazyAICheck, 
	ultraParanoid:ultraParanoidAICheck, 
	weakest:weakestAICheck,
	cluster:clusterAICheck,
	connect:connectAICheck
};
var qtyFunctions={
	random:randomAttackQuantity, 
	full:fullAttackQuantity, 
	single:singleAttackQuantity
};

/* Callbacks for sorting the targets array */
function allAISort(a,b) {
	var funcs = [];
	for(var sf in sortFunctions) 
		funcs.push(sortFunctions[sf]);
	return(funcs[random(funcs.length)](a,b));
}
function randomAISort(a,b) {
	return(random(2)*2-1);
}
function stupidAISort(a, b) {
	var adiff=0;
	var bdiff=0;

	adiff=a.base.dice - a.target.dice;
	bdiff=b.base.dice - b.target.dice;
	return(adiff-bdiff);
}
function killMostDiceAISort(a, b) {
	return(b.target.dice - a.target.dice);
}
function killLeastDiceAISort(a,b) {
	return(a.target.dice - b.target.dice);
}
function paranoidAISort(a,b) {
	var ascore=0;
	var bscore=0;

	ascore = a.base.dice - a.target.dice;
	ascore *= a.target.dice;
	bscore = b.base.dice - b.target.dice;
	bscore *= b.target.dice;
	return(bscore-ascore);
}
function groupAndParanoidAISort(a,b) {
	var aopts=0;
	var bopts=0;

	function countem(map, location, p) {
		var ret=0;
		var neighbors=getNeighboringTiles(location,map);
		for(var n=0;n<neighbors.length;n++) {
			var neighbor=neighbors[n];
			if(neighbor.owner!=p)
				ret++;
		}
		return(ret);
	}

	var aopts=countem(a.map, a.target, a.base.owner);
	var bopts=countem(b.map, b.target, b.base.owner);

	if(aopts==bopts)
		return(paranoidAISort(a,b));
	return(aopts-bopts);
}
function clusterAISort(a,b) {

	/* TODO */
	var tiles = getPlayerTiles(map,a.base.owner);
	var clusters = getAllClusters(map,tiles,a.base.owner);
	var acluster, bcluster;
	
	/* find the cluster each attack base is in */
	for(var c in clusters) {
		var clust = clusters[c];
		for(var t in clust) {
			if(t == a.base.id) 
				acluster = countMembers(clust);
			else if(t == b.base.id) 
				bcluster = countMembers(clust);
			if(bcluster && acluster)
				break;
		}
		if(bcluster && acluster)
			break;
	}
	
	/* in case of equal cluster, most dice wins */
	if(bcluster == acluster) 
		return paranoidAISort(a,b);
	
	/* compare size of each, bigger cluster wins */
	return acluster-bcluster;
}
function killFirstPlaceAISort(a,b) {
	/* determine which player has the most tiles */
	var firstCount;
	var firstPlayer;
	for(var p=0;p<game.players.length;p++) {
		var numTiles = countTiles(map,p);
		if(!firstPlayer || numTiles > firstCount) {
			firstCount = numTiles;
			firstPlayer = p;
		}
		/* if two players have the same amount of tiles, 
		choose the player with the most dice */
		else if(numTiles == firstCount) {
			var acount = countDice(map,firstPlayer);
			var bcount = countDice(map,p);
			if(bcount > acount) {
				firstPlayer = p;
			}
		}
	}
	/* if both tiles have the same owner, dont sort */
	if(a.owner == b.owner) 
		return (b.dice - a.dice);
	if(a.owner == firstPlayer)
		return -1;
	return 1;
}
function killLastPlaceAISort(a,b) {
	/* determine which player has the least tiles */
	var lastCount;
	var lastPlayer;
	for(var p=0;p<game.players.length;p++) {
		var numTiles = countTiles(map,p);
		if(!lastPlayer || numTiles < lastCount) {
			lastCount = numTiles;
			lastPlayer = p;
		}
		/* if two players have the same amount of tiles, 
		choose the player with the least dice */
		else if(numTiles == lastCount) {
			var acount = countDice(map,lastPlayer);
			var bcount = countDice(map,p);
			if(bcount < acount) {
				lastPlayer = p;
			}
		}
	}
	/* if both tiles have the same owner, take the easy win */
	if(a.owner == b.owner) 
		return (b.dice - a.dice);
	if(a.owner == lastPlayer)
		return -1;
	return 1;
}

/* Callbacks for deciding if a given attack should go into the targets array */
function allAICheck(game, map, base, target) {
	var funcs = [];
	for(var cf in checkFunctions) 
		funcs.push(checkFunctions[cf]);
	return(funcs[random(funcs.length)](game, map, base, target));
}
function clusterAICheck(game, map, base, target) {
	var computer_tiles=getPlayerTiles(map,base.owner);
	var clusters = getAllClusters(map,computer_tiles,base.owner);
	/* if the base tile is part of the player's largest cluster, use it to attack */
	for(var t in clusters[0]) {
		if(t == base.id) {
			/* If we have an advantage, add to targets array */
			if(base.dice>target.dice || base.dice == settings.max_dice)
				return(true);
		}
	}
	return false;
}
function weakestAICheck(game, map, base, target) {
	var target_tiles = countTiles(map, target.owner);
	var neighbors = getNeighboringTiles(base,map);
	
	for(var n=0;n<neighbors.length;n++) {
		var neighbor=neighbors[n];
		if(neighbors[n].id==target.id)
			continue;
		if(neighbor.owner!=base.owner) {
			var neighbor_tiles=countTiles(map, neighbor.owner);
			if(neighbor_tiles<target_tiles) 
				return(false);
		}
	}
	if(base.dice>target.dice || base.dice==settings.max_dice)
		return(true);
	return(false);
}
function randomAICheck(game, map, base, target) {
	var computer=game.players[base.owner];
	var rand=random(100);
	
	if(rand>10 && base.dice>target.dice)
		return(true);
	var computer_tiles=getPlayerTiles(map,base.owner);
	if(base.dice==target.dice) {
		if(rand>50 || base.dice==settings.max_dice) {
			if(computer_tiles.length>map.tiles.length/6 || computer.reserve>=settings.max_reserve*.66)
				return(true);
			if(getLargestCluster(map,computer_tiles,base.owner)+computer.reserve>=settings.max_dice)
				return(true);
		}
	}
	if(rand>90 && base.dice==target.dice-1) {
		if(computer_tiles.length>map.tiles.length/6)
			return(true);
	}
	return(false);
}
function paranoidAICheck(game, map, base, target) {
	/* If we have an advantage, add to targets array */
	if(base.dice>target.dice || base.dice==settings.max_dice)
		return(true);
	return(false);
}
function wildAndCrazyAICheck(game, map, base, target) {
	var computer=game.players[base.owner];
	var rand=random(100);
	
	if(base.dice>target.dice)
		return(true);
	var computer_tiles=getPlayerTiles(map,base.owner);
	if(base.dice==target.dice) {
		if(computer_tiles.length>map.tiles.length/6 || computer.reserve>=settings.max_reserve*.66)
			return(true);
		else {
			if(getLargestCluster(map,computer_tiles,base.owner)+computer.reserve>=settings.max_dice)
				return(true);
		}
	}
	if(rand>50 && base.dice==target.dice-1) {
		if(computer_tiles.length>map.tiles.length/6)
			return(true);
	}
	return(false);
}
function ultraParanoidAICheck(game, map, base, target) {
	var computer=game.players[base.owner];

	/* First, check that we have a dice advantage */
	if(base.dice < target.dice)
		return(false);

	var computer_tiles=getPlayerTiles(map,base.owner);
	/* If we don't have our "fair share" of territories, use paranoid attack */
	if(computer_tiles.length < map.tiles.length/game.players.length) {
		return(paranoidAICheck(game, map, base, target));
	}

	/* If reserves + expected new dice - used reserves is still greater than seven, use the merely paranoid attack */
	if(computer.reserve + computer_tiles.length - (computer.AI.moves*settings.max_dice) > settings.max_dice-1) {
		return(paranoidAICheck(game, map, base, target));
	}

	/* Always try to attack on the first turn */
	if(computer.AI.turns==0) {
		return(paranoidAICheck(game, map, base, target));
	}

	/* First, check if we would leave ourselves open.  If so,
	 * do not take the risk */
	var neighbors=getNeighboringTiles(base,map);
	for(var n=0;n<neighbors.length;n++) {
		var neighbor=neighbors[n];
		if(neighbors[n].id==target.id)
			continue;
		if(neighbor.owner!=base.owner && neighbor.dice>2)
			return(false);
	}

	/* Finally, check that we will still be at least equal to all neighbors after the capture */
	neighbors=getNeighboringTiles(target,map);
	for(var n=0;n<neighbors.length;n++) {
		var neighbor=neighbors[n];
		if(neighbor.id==base.id)
			continue;
		if(neighbor.owner!=base.owner && neighbor.dice >= base.dice) {
			return(false);
		}
	}
	return(true);
}
function connectAICheck(game, map, base, target) {
	var computer=game.players[base.owner];
	var baseCluster;
	
	/* First, check that we have a reasonable chance of winning */
	if(base.dice < target.dice)
		return(false);

	var computer_tiles=getPlayerTiles(map,base.owner);
	var clusters = getAllClusters(map,computer_tiles,base.owner);
	
	/* if we have only one cluster, use a different check method */
	if(clusters.length == 1) 
		return paranoidAICheck(game,map,base,target);
	
	/* find and store the cluster we are attacking from */
	for(var c=0;c<clusters.length;c++) {
		for(var t in clusters[c]) {
			if(t == base.id) {
				baseCluster = clusters[c];
				break;
			}
		}
		if(baseCluster)
			break;
	}

	/* check to see if the target tile is neighbored by any of our tiles,
	to potential bridge two isolated clusters */
	neighbors=getNeighboringTiles(target,map);
	for(var n=0;n<neighbors.length;n++) {
		var neighbor=neighbors[n];
		/* we found a neighbor that we own */
		if(neighbor.owner==base.owner) {
			/* skip the tile we are scanning from */
			if(neighbor.id==base.id)
				continue;
			var noGood = false;
			/* scan the base cluster to see if the neighbor
			tile is part of the same group */
			for(var t in baseCluster) {
				if(t == neighbor.id) {
					noGood = true;
					break;
				}
			}
			/* if we've found a neighbor of the target tile
			that is ours and not part of the cluster we're
			attacking from, it's a link! */
			if(!noGood) 
				return true;
		}
	}
	
	/* if we cant connect two islands right now, do something else */
	return paranoidAICheck(game,map,base,target);
}

/* Callbacks for selecting the number of targets to use */
function randomAttackQuantity(numTargets) {
	if(numTargets < 2)
		return(numTargets); 
	return(random(numTargets)+1);
}
function fullAttackQuantity(numTargets) {
	return(numTargets);
}
function singleAttackQuantity(numTargets) {
	if(numTargets > 0)
		return(1);
	return(0);
}

/* Attack functions */
function open() {
	
	map = client.read(game_id,"maps." + gameNumber,1);
	game = client.read(game_id,"games." + gameNumber,1);
	
	if(!game)
		exit();
	if(!game.players[game.turn].active) {
		game.turn = nextTurn(game);
		data.saveTurn(game);
	}
	log(LOG_DEBUG,"Dicewarz II AI thread loaded");
}
function initClient() {
	if(!file_exists(root + "server.ini"))
		throw("server initialization file missing");
		
	var server_file = new File(root + "server.ini");
	server_file.open('r',true);
	var serverAddr=server_file.iniGetValue(null,"host");
	var serverPort=server_file.iniGetValue(null,"port");
	server_file.close();
	client = new JSONClient(serverAddr,serverPort);
}
function main() {
	while(game.players[game.turn].AI && game.status==status.PLAYING) {

		var computer=game.players[game.turn];
		computer.AI.turns = 0;
		computer.AI.moves = 0;
		
		var attacks = attack();
		if(countActivePlayers(game)==2 && countTiles(map,game.turn)<(map.tiles.length*.3)) 
			forfeit(computer);
		else 
			reinforce(computer);
		
		updateStatus(game, map);
		nextTurn(game);
		
		mswait(1000);
	}
}
function cycle() {

	/* in theory.. we should have sole control
	over everything happening in this particular game
	until we get to an end game condition or the next
	player is human (at which point control returns to the service) */
	client.cycle();
	while(client.updates.length > 0)
		processUpdate(client.updates.shift());
}
function processUpdate(update) {
	/* this entire function may very well be useless */
	var playerName = undefined;
	var gameNumber = undefined;
	
	switch(update.oper.toUpperCase()) {
	case "SUBSCRIBE":
	case "UNSUBSCRIBE":
		/* we will likely proceed with our AI turns regardless 
		of anyone else's subscription status, so that turns take
		place whether a player drops carrier or not */
		break;
	case "WRITE":
		/* somebody changed somethin */
		var p=update.location.split(".");
		var obj=data;
		while(p.length > 1) {
			var child=p.shift();
			obj=obj[child];
			switch(child.toUpperCase()) {
			case "PLAYERS":
				playerName = p[0];
				break;
			case "GAMES":
			case "MAPS":
				gameNumber = p[0];
				break;
			}
		}
		
		var child = p.shift();
		if(update.data == undefined)
			delete obj[child];
		else
			obj[child] = update.data;
			
		/* we may receive notice to end the game early
		due to pleyer forfeits */
		if(child.toUpperCase() == "STATUS")
			updateStatus(update,gameNumber);
		break;
	}
}
function close() {
	log(LOG_DEBUG,"Dicewarz II AI thread complete");
}
function attack() {
	var computer=game.players[game.turn];
	var attackCount=0;
	var attacks, territories;
	
	do {
		/* Randomize the targets array */
		territories=getPlayerTiles(map,game.turn);
		attacks=getAttackOptions(territories, computer);
		kills=getKillOptions(territories, computer);
		
		/* always attempt all kill options */
		while(kills.length > 0) {
			var attack=kills.shift();
			if(attack.base.owner == attack.target.owner)
				continue;
			doAttack(attack.base,attack.target);
			computer.AI.moves++;
			attackCount++;
			mswait(1000);
		}
		
		attackQuantity=qtyFunctions[computer.AI.qty](attacks.length);
		if(attackQuantity<1) 
			break;
		
		attacks.sort(sortFunctions[computer.AI.sort]);
		for(var n=0;n<attackQuantity;n++) {
			var attack=attacks.shift();
			if(attack.base.owner == attack.target.owner)
				continue;
			doAttack(attack.base,attack.target);
			computer.AI.moves++;
			attackCount++;
			mswait(1000);
		} 
	} while(attacks.length > 0);
	
	computer.AI.turns++;
	return attackCount;
}
function doAttack(attacking,defending) {
	var attacker=game.players[attacking.owner];
	var defender=game.players[defending.owner];
	
	var a=new Roll(attacking.owner);
	for(var r=0;r<attacking.dice;r++) {
		var roll=random(6)+1;
		a.roll(roll);
	}
	
	var d=new Roll(defending.owner);
	for(var r=0;r<defending.dice;r++) {
		var roll=random(6)+1;
		d.roll(roll);
	}
	
	attackMessage(attacker,defender,a,d);
	if(a.total>d.total) {
		if(countTiles(map,defending.owner)==1 && defender.active) {
			data.scoreKiller(attacker);
			data.scoreLoser(defender);
			killMessage(attacker,defender);
		}
		data.assignTile(map,defending,attacking.owner,attacking.dice-1);
		updateStatus(game,map);
	}
	
	data.assignTile(map,attacking,attacking.owner,1);
}
function attackMessage(attacker,defender,a,d) {
	data.saveActivity(game,"\1n\1m" + 
		attacker.name + 
		" (\1h" + a.total + "\1n\1m) vs (\1h" + d.total + "\1n\1m) " + 
		defender.name);
}
function killMessage(attacker,defender) {
	data.saveActivity(game,"\1r\1h" + attacker.name + 
		" eliminated " + defender.name + "!");
}
function getKillOptions(territories, player) {
	var attacks=[];
	/* For each owned territory */
	for(var t=0;t<territories.length;t++) {
		var base=territories[t];
		/* If we have enough to attack */
		var attackOptions=canAttack(map,base);
		if(attackOptions.length>0) {
			var basetargets=[];
			for(var o=0;o<attackOptions.length;o++) {
				var target=attackOptions[o];
				var numTiles=countTiles(map,target.owner);
				
				/* kill option! */
				if(numTiles == 1 && checkFunctions['paranoid'](game,map,base,target)) {
					attacks.push({map:map,target:target,base:base});
				}
			}
		}
	}
	return attacks;
}
function getAttackOptions(territories, player) {
	var attacks=[];
	/* For each owned territory */
	for(var t=0;t<territories.length;t++) {
		var base=territories[t];
		/* If we have enough to attack */
		var attackOptions=canAttack(map,base);
		if(attackOptions.length>0) {
			var basetargets=[];
			for(var o=0;o<attackOptions.length;o++) {
				var target=attackOptions[o];
				/* Check if this is an acceptable attack */
				if(checkFunctions[player.AI.check](game,map,base,target))
					basetargets.push({map:map,target:target,base:base});
			}
			/* If we found acceptable attacks, sort them and choose the best one */
			if(basetargets.length > 0) {
				basetargets.sort(sortFunctions[player.AI.sort]);
				attacks.push(basetargets.shift());
			}
		}
	}
	return attacks;
}
function forfeit(computer) {
	computer.active=false;
	data.savePlayer(game,computer);
	data.saveActivity(game.gameNumber,"\1n\1r" + computer.name + " forfeits.");
	data.scoreForfeit(computer);
	updateStatus(game, map);
}
function reinforce(computer) {
	var update=getReinforcements(game,map,game.turn);
	if(update.placed>0) {
		var msg="\1n\1y" + computer.name + " placed " + update.placed + " dice";
		data.saveActivity(game,msg);
	}
	if(update.reserved>0) {
		var msg="\1n\1y" + computer.name + " reserved " + update.reserved + " dice";
		data.saveActivity(game,msg);
	}
}

open();
main();
close();