var game_number=argv[0];
var game_dir=argv[1];
load(game_dir+"maps.js");
var map=game_data.list[game_number];

var sortFunctions={random:randomSort, wild:wildAndCrazyAISort, killMost:killMostDiceAISort, paranoia:paranoiaAISort, randomAI:randomAISort, groupParanoid:groupAndParanoidAISort};
var checkFunctions={random:randomAICheck, paranoid:paranoidAICheck, wild:wildAndCrazyAICheck, ultraParanoid:ultraParanoidAICheck};
var qtyFunctions={random:randomAttackQuantity, full:fullAttackQuantity, single:singleAttackQuantity};

/* Callbacks for sorting the targets array */
function randomSort()
{
	return(random(2)*2-1);
}
function slowAndSteadyAISort(a, b)
{
	var adiff=0;
	var bdiff=0;

	adiff=a.base.dice - a.target.dice;
	bdiff=b.base.dice - b.target.dice;
	return(adiff-bdiff);
}
function wildAndCrazyAISort(a, b)
{
	var adiff=0;
	var bdiff=0;

	adiff=a.base.dice - a.target.dice;
	bdiff=b.base.dice - b.target.dice;
	return(bdiff-adiff);
}
function killMostDiceAISort(a, b)
{
	return(b.base.dice - a.target.dice);
}
function paranoiaAISort(a,b)
{
	var ascore=0;
	var bscore=0;

	ascore = a.base.dice - a.target.dice;
	ascore *= a.target.dice;
	bscore = b.base.dice - b.target.dice;
	bscore *= b.target.dice;
	return(bscore-ascore);
}
function randomAISort(a,b)
{
	var sortfuncs=new Array(randomSort, slowAndSteadyAISort, wildAndCrazyAISort, killMostDiceAISort, paranoiaAISort);

	return(sortfuncs[random(sortfuncs.length)](a,b));
}
function groupAndParanoidAISort(a,b)
{
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

/* Callbacks for deciding if a given attack should go into the targets array */
function randomAICheck(map, base, target)
{
	var computer=map.players[base.owner];
	var rand=random(100);
	
	if(rand>10 && base.dice>target.dice)
		return(true);
	var computer_tiles=getPlayerTiles(map,base.owner);
	if(base.dice==target.dice) {
		if(rand>50 || base.dice==settings.max_dice) {
			if(computer_tiles.length>map.tiles.length/6 || computer.reserve>=settings.max_reserve*.66)
				return(true);
			if(countConnected(map,computer_tiles,base.owner)+computer.reserve>=settings.max_dice)
				return(true);
		}
	}
	if(rand>90 && base.dice==target.dice-1) {
		if(computer_tiles.length>map.tiles.length/6)
			return(true);
	}
	return(false);
}
function paranoidAICheck(map, base, target)
{
	var computer=map.players[base.owner];
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
function wildAndCrazyAICheck(map, base, target)
{
	var computer=map.players[base.owner];
	var rand=random(100);
	
	if(base.dice>target.dice)
		return(true);
	var computer_tiles=getPlayerTiles(map,base.owner);
	if(base.dice==target.dice) {
		if(computer_tiles.length>map.tiles.length/6 || computer.reserve>=settings.max_reserve*.66)
			return(true);
		else {
			if(countConnected(map,computer_tiles,base.owner)+computer.reserve>=settings.max_dice)
				return(true);
		}
	}
	if(rand>50 && base.dice==target.dice-1) {
		if(computer_tiles.length>map.tiles.length/6)
			return(true);
	}
	return(false);
}
function ultraParanoidAICheck(map, base, target)
{
	var computer=map.players[base.owner];
	var rand=random(100);
	
	var computer_tiles=getPlayerTiles(map,base.owner);
	/* If we don't have our "fair share" of territories, use paranoid attack */
	if(computer_tiles.length < map.tiles.length/map.players.length) {
		return(paranoidAICheck(map, base, target));
	}

	/* If reserves + expected new dice - used reserves is still greater than seven, use the merely paranoid attack */
	if(computer.reserve + computer_tiles.length - (computer.AI.moves*settings.max_dice) > settings.max_dice-1) {
		return(paranoidAICheck(map, base, target));
	}

	/* Always try to attack on the first turn */
	if(computer.AI.turns==0) {
		return(paranoidAICheck(map, base, target));
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
function randomAttackQuantity(tlen)
{
	if(tlen <= 2)
		return(tlen); 
	return(random(tlen-2)+2);
}
function fullAttackQuantity(tlen)
{
	return(tlen);
}
function singleAttackQuantity(tlen)
{
	if(tlen > 0)
		return(1);
	return(0);
}

/* Attack functions */
function main()
{
	while(map.players[map.turn].AI && map.in_progress) {
		takeTurn();
		if(countActivePlayers(map)==2 && countTiles(map,map.turn)<map.tiles.length*.4) {
			forfeit();
		} else {
			reinforce();
			updateStatus(map);
			nextTurn(map);
		}
		game_data.saveData(map);
	}
}
function attack()
{
	var computer=map.players[map.turn];
	var territories=getPlayerTiles(map,map.turn);
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
				if(checkFunctions[computer.AI.check](map,base,target))
					basetargets.push({map:map,target:target,base:base});
			}
			/* If we found acceptable attacks, sort them and choose the best one */
			if(basetargets.length > 0) {
				basetargets.sort(sortFunctions[computer.AI.sort]);
				attacks.push(basetargets.shift());
			}
		}
	}
	/* Randomize the targets array */
	attacks.sort(randomSort);
	attackQuantity=qtyFunctions[computer.AI.qty](attacks.length);
	if(attackQuantity<1) return false;
	
	attacks.sort(sortFunctions[computer.AI.sort]);
	for(var a=0;a<attackQuantity;a++)
	{
		var attack=attacks.shift();
		var attacking=attack.base;
		var defending=attack.target;
		var attacker=map.players[attacking.owner].name;
		var defender=map.players[defending.owner].name;
		
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
		var data=new Packet("battle");
		data.a=a;
		data.d=d;
		stream.send(data);
		
		if(a.total>d.total) {
			if(countTiles(map,defending.owner)==1) {
				players.scoreKill(map.players[attacking.owner].name);
				players.scoreLoss(map.players[defending.owner].name);
				map.players[defending.owner].active=false;
			}
			defending.assign(attacking.owner,attacking.dice-1);
		} 
		attacking.dice=1;
		
		game_data.saveActivity(map,attacker + " attacked " + defender + ": " + attacking.dice + " vs " + defending.dice);
		game_data.saveActivity(map,attacker + ": " + a.total + " " + defender + ": " + d.total);
		game_data.saveTile(map,attacking);
		game_data.saveTile(map,defending);
		computer.AI.moves++;
	}
	computer.AI.turns++;
	return true;
}
function takeTurn()
{
	while(map.in_progress) {
		if(!attack()) break;
		updateStatus(map);
		mswait(500);
	}
}
function forfeit()
{
	map.in_progress=false;
	map.players[map.turn].active=false;
	var data=new Packet("activity");
	data.activity=("\1n\1y" + map.players[map.turn].name + " forfeits");
	stream.send(data);
	players.scoreLoss(map.players[map.turn].name);
}
function reinforce()
{
	var player_tiles=getPlayerTiles(map,map.turn);
	var reinforcements=countConnected(map,player_tiles,map.turn);
	var placed=placeReinforcements(map,player_tiles,reinforcements);
	var total=0;
	for(var p in placed) {
		total+=placed[p];
	}
	var reserved=placeReserves(map,reinforcements-total);
	var data=new Packet("activity");
	if(total>0) {
		data.activity=("\1n\1y" + map.players[map.turn].name + " placed " + total + " dice");
		stream.send(data);
	}
	if(reserved>0) {
		data.activity=("\1n\1y" + map.players[map.turn].name + " reserved " + reserved + " dice");
		stream.send(data);
	}
}

main();