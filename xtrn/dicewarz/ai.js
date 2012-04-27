 /* Callbacks for sorting the targets array */
function	RandomSort()
{
	return(random(2)*2-1);
}
function	SlowAndSteadyAISort(a, b)
{
	var adiff=0;
	var bdiff=0;

	adiff=a.base_grid.dice - a.target_grid.dice;
	bdiff=b.base_grid.dice - b.target_grid.dice;
	return(adiff-bdiff);
}
function	WildAndCrazyAISort(a, b)
{
	var adiff=0;
	var bdiff=0;

	adiff=a.base_grid.dice - a.target_grid.dice;
	bdiff=b.base_grid.dice - b.target_grid.dice;
	return(bdiff-adiff);
}
function	KillMostDiceAISort(a, b)
{
	return(b.target_grid.dice - a.target_grid.dice);
}
function	ParanoiaAISort(a,b)
{
	var ascore=0;
	var bscore=0;

	ascore = a.base_grid.dice - a.target_grid.dice;
	ascore *= a.target_grid.dice;
	bscore = b.base_grid.dice - b.target_grid.dice;
	bscore *= b.target_grid.dice;
	return(bscore-ascore);
}
function	RandomAISort(a,b)
{
	var sortfuncs=new Array(RandomSort, SlowAndSteadyAISort, WildAndCrazyAISort, KillMostDiceAISort, ParanoiaAISort);

	return(sortfuncs[random(sortfuncs.length)](a,b));
}
function	GroupAndParanoidAISort(a,b)
{
	var aopts=0;
	var bopts=0;


	function countem(map, location, player) {
		var ret=0;

		var dirs=map.loadDirectional(location);
		for(var dir in dirs) {
			var current=dirs[dir];
			if(map.grid[current]) {
				if(map.grid[current].player!=player)
					ret++;
			}
		}
		return(ret);
	}

	var aopts=countem(games.gameData[a.gameNumber], a.target, a.base_grid.player);
	var bopts=countem(games.gameData[b.gameNumber], b.target, b.base_grid.player);

	if(aopts==bopts)
		return(ParanoiaAISort(a,b));
	return(aopts-bopts);
}
function	RandomAIForfeit()
{
	var f=new Array(AIForfeitValues.Coward,AIForfeitValues.Normal,AIForfeitValues.Alamo);

	return(f[random(f.length)]);
}

/* Callbacks for deciding if a given attack should go into the targets array */
function	RandomAICheck(gameNumber, playerNumber, base, target)
{
	var g=games.gameData[gameNumber];
	var computerPlayer=g.players[playerNumber];

	var rand=random(100);
	if(rand>10 && g.grid[base].dice>g.grid[target].dice)
		return(true);
	if(g.grid[base].dice==g.grid[target].dice) {
		if(rand>50 || g.grid[target].dice==g.maxDice) {
			if(computerPlayer.territories.length>g.grid.length/6 || computerPlayer.reserve>=20)
				return(true);
			else {
				if(g.findConnected(playerNumber)+computerPlayer.reserve>=8)
					return(true);
			}
		}
	}
	if(rand>90 && g.grid[base].dice==(g.grid[target].dice-1))
	{
		if(computerPlayer.territories.length>g.grid.length/6)
			return(true);
	}
	return(false);
}
function	ParanoidAICheck(gameNumber, playerNumber, base, target)
{
	var g=games.gameData[gameNumber];
	var computerPlayer=g.players[playerNumber];

	var rand=random(100);
	/* If we have an advantage, add to targets array */
	if(g.grid[base].dice>g.grid[target].dice)
		return(true);
	/* If we are equal, only add to targets if we are maxDice */
	if(g.grid[base].dice==g.grid[target].dice) {
		if(g.grid[target].dice==g.maxDice)
			return(true);
	}
	return(false);
}
function	WildAndCrazyAICheck(gameNumber, playerNumber, base, target)
{
	var g=games.gameData[gameNumber];
	var computerPlayer=g.players[playerNumber];

	var rand=random(100);
	if(g.grid[base].dice>g.grid[target].dice)
		return(true);
	if(g.grid[base].dice==g.grid[target].dice) {
		if(computerPlayer.territories.length>g.grid.length/6 || computerPlayer.reserve>=20)
			return(true);
		else {
			if(g.findConnected(playerNumber)+computerPlayer.reserve>=8)
				return(true);
		}
	}
	if(rand>50 && g.grid[base].dice==(g.grid[target].dice-1)) {
		if(computerPlayer.territories.length>g.grid.length/6)
			return(true);
	}
	return(false);
}
function	UltraParanoidAICheck(gameNumber, playerNumber, base, target)
{
	var g=games.gameData[gameNumber];
	var computerPlayer=g.players[playerNumber];

	/* If we don't have our "fair share" of territories, use paranoid attack */
	if(computerPlayer.territories.length <= g.playerTerr) {
		return(ParanoidAICheck(gameNumber, playerNumber, base, target));
	}

	/* If reserves + expected new dice - used reserves is still greater than seven, use the merely paranoid attack */
	if(computerPlayer.reserve + computerPlayer.territories.length - (computerPlayer.AI.moves*8) > 7) {
		return(ParanoidAICheck(gameNumber, playerNumber, base, target));
	}

	/* Always try to attack on the first turn */
	if(computerPlayer.AI.turns==0) {
		return(ParanoidAICheck(gameNumber, playerNumber, base, target));
	}

	/* First, check if we would leave ourselves open.  If so,
	 * do not take the risk */
	var dirs=g.loadDirectional(base);
	for(dir in dirs) {
		current=dirs[dir];
		if(current==target)
			continue;
		if(g.grid[current]) {
			if(g.grid[current].player!=playerNumber && g.grid[current].dice > 2)
				return(false);
		}
	}

	/* Next, check that we have a dice advantage */
	if(g.grid[base].dice <= g.grid[target].dice)
		return(false);

	/* Finally, check that we will still be at least equal after the capture */
	var dirs=g.loadDirectional(target);
	var troublecount=0;
	for(dir in dirs) {
		var current=dirs[dir];
		if(current==base)
			continue;
		if(g.grid[current]) {
			if(g.grid[current].player!=playerNumber && g.grid[current].dice >= g.grid[base].dice) {
				troublecount += g.grid[current].dice - g.grid[base].dice
				if(troublecount >= 3)
					return(false);
			}
		}
	}
	return(true);
}

/* Callbacks for selecting the number of targets to use */
function	RandomAttackQuantity(tlen)
{
	if(tlen <= 2)
		return(tlen); 
	return(random(tlen-2)+2);
}
function	FullAttackQuantity(tlen)
{
	return(tlen);
}
function	SingleAttackQuantity(tlen)
{
	if(tlen > 0)
		return(1);
	return(0);
}

var AISortFunctions={Random:RandomSort, Wild:WildAndCrazyAISort, KillMost:KillMostDiceAISort, Paranoia:ParanoiaAISort, RandomAI:RandomAISort, GroupParanoid:GroupAndParanoidAISort};
var AICheckFunctions={Random:RandomAICheck, Paranoid:ParanoidAICheck, Wild:WildAndCrazyAICheck, UltraParanoid:UltraParanoidAICheck};
var AIQtyFunctions={Random:RandomAttackQuantity, Full:FullAttackQuantity, Single:SingleAttackQuantity};
var AIForfeitValues={Coward:.45,Normal:.25,Alamo:0,Random:RandomAIForfeit};