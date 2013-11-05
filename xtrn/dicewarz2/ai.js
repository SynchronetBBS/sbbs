load("json-client.js");

var gameNumber=argv[0];
var root=argv[1];
var client,game,map;

initClient();
load(root+"diceobj.js");
load(root+"dicefunc.js");

var sortFunctions={
	random:randomSort, 
	wild:wildAndCrazyAISort, 
	killMost:killMostDiceAISort, 
	paranoia:paranoiaAISort, 
	randomAI:randomAISort, 
	groupParanoid:groupAndParanoidAISort,
	cluster:clusterAISort
};
var checkFunctions={
	random:randomAICheck, 
	paranoid:paranoidAICheck, 
	wild:wildAndCrazyAICheck, 
	ultraParanoid:ultraParanoidAICheck, 
	weakest:weakestAICheck,
	cluster:clusterAICheck
};
var qtyFunctions={
	random:randomAttackQuantity, 
	full:fullAttackQuantity, 
	single:singleAttackQuantity
};

/* Callbacks for sorting the targets array */
function randomSort() {
	return(random(2)*2-1);
}
function slowAndSteadyAISort(a, b) {
	var adiff=0;
	var bdiff=0;

	adiff=a.base.dice - a.target.dice;
	bdiff=b.base.dice - b.target.dice;
	return(adiff-bdiff);
}
function wildAndCrazyAISort(a, b) {
	var adiff=0;
	var bdiff=0;

	adiff=a.base.dice - a.target.dice;
	bdiff=b.base.dice - b.target.dice;
	return(bdiff-adiff);
}
function killMostDiceAISort(a, b) {
	return(b.base.dice - a.target.dice);
}
function paranoiaAISort(a,b) {
	var ascore=0;
	var bscore=0;

	ascore = a.base.dice - a.target.dice;
	ascore *= a.target.dice;
	bscore = b.base.dice - b.target.dice;
	bscore *= b.target.dice;
	return(bscore-ascore);
}
function randomAISort(a,b) {
	var sortfuncs=new Array(randomSort, slowAndSteadyAISort, wildAndCrazyAISort, killMostDiceAISort, paranoiaAISort, clusterAISort);
	return(sortfuncs[random(sortfuncs.length)](a,b));
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
		return(paranoiaAISort(a,b));
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
	
	/* compare size of each, bigger cluster wins */
	if(bcluster == acluster)
		return paranoiaAISort(a,b);
	return acluster-bcluster;
}

/* Callbacks for deciding if a given attack should go into the targets array */
function clusterAICheck(game, map, base, target) {
	var computer_tiles=getPlayerTiles(map,base.owner);
	var clusters = getAllClusters(map,computer_tiles,base.owner);
	/* if the base tile is part of the player's largest cluster, use it to attack */
	for(var t in clusters[0]) {
		if(t == base.id) {
			return true;
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
			if(neighbor_tiles<target_tiles) return(false);
		}
	}
	return(true);
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
	var computer=game.players[base.owner];
	var rand=random(100);

	/* If we have an advantage, add to targets array */
	if(base.dice>target.dice)
		return(true);
	/* If we are equal, only add to targets if we are maxDice */
	if(base.dice==target.dice && base.dice==settings.max_dice) {
		return(true);
	}
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
	var rand=random(100);
	
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
		if(neighbor.owner!=base.owner && neighbor.dice>=2)
			return(false);
	}

	/* Next, check that we have a dice advantage */
	if(base.dice <= target.dice)
		return(false);

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

/* Callbacks for selecting the number of targets to use */
function randomAttackQuantity(tlen) {
	if(tlen <= 2)
		return(tlen); 
	return(random(tlen-2)+2);
}
function fullAttackQuantity(tlen) {
	return(tlen);
}
function singleAttackQuantity(tlen) {
	if(tlen > 0)
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
	var attacks;
	
	do {
		/* Randomize the targets array */
		attacks=getAttackOptions();
		attackQuantity=qtyFunctions[computer.AI.qty](attacks.length);
		if(attackQuantity<1) 
			break;
		
		attacks.sort(sortFunctions[computer.AI.sort]);
		for(var n=0;n<attackQuantity;n++) {
			var attack=attacks.shift();
			var attacking=attack.base;
			var defending=attack.target;
			
			if(attacking.owner == defending.owner)
				continue;
				
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
			computer.AI.moves++;
			attackCount++;
			mswait(1000);
			
		} 
	} while(attacks.length > 0);
	
	computer.AI.turns++;
	return attackCount;
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
function getAttackOptions() {
	var territories=getPlayerTiles(map,game.turn);
	var computer=game.players[game.turn];
	var attacks=[];
	
	/* For each owned territory */
	for(var t=0;t<territories.length;t++) {
		var base=territories[t];
		/* If we have enough to attack */
		var attack_options=canAttack(map,base);
		if(attack_options.length>0) {
			var basetargets=[];
			/* Randomize the order to check in */
			attack_options.sort(randomSort);
			for(var o=0;o<attack_options.length;o++) {
				var target=attack_options[o];
				/* Check if this is an acceptable attack */
				if(checkFunctions[computer.AI.check](game,map,base,target))
					basetargets.push({map:map,target:target,base:base});
			}
			/* If we found acceptable attacks, sort them and choose the best one */
			if(basetargets.length > 0) {
				basetargets.sort(sortFunctions[computer.AI.sort]);
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