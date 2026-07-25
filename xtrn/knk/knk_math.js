/*
 * Arithmetic recovered from Kannons & Katapults 4.4.
 *
 * Keep this file independent of the door UI so its results can be checked
 * outside Synchronet.  State objects use the names from knk.js.
 */
var KNKMath = (function() {
	var rng = function() {
		return random(0x1000000) / 0x1000000;
	};
	var actions = [
		"buy_food",
		"attack",
		"hire_guards",
		"draft",
		"fortify_castle",
		"assassinate",
		"hire_assassins",
		"buy_catapults",
		"buy_cannons",
		"retire_soldiers",
		"fire_cannons",
		"fire_catapults"
	];

	function single(value)
	{
		if(typeof Float32Array !== "undefined") {
			var value32 = new Float32Array(1);
			value32[0] = value;
			return value32[0];
		}
		return value;
	}

	var F_00001 = single(0.0001);
	var F_001 = single(0.01);
	var F_005 = single(0.05);
	var F_01 = single(0.1);
	var F_015 = single(0.15);
	var F_02 = single(0.2);
	var F_025 = single(0.25);
	var F_05 = single(0.5);
	var F_066 = single(0.66);
	var F_075 = single(0.75);
	var F_08 = single(0.8);
	var F_085 = single(0.85);
	var F_09 = single(0.9);
	var F_095 = single(0.95);

	function rnd()
	{
		var value = single(rng());
		if(value < 0 || value >= 1)
			throw new Error("RND value must be in [0, 1)");
		return value;
	}

	function setRandom(func)
	{
		rng = func;
	}

	function resetRandom()
	{
		rng = function() {
			return random(0x1000000) / 0x1000000;
		};
	}

	/* QuickBASIC INT, including its behaviour for negative values. */
	function qbInt(value)
	{
		return Math.floor(value);
	}

	function difference(scale, base)
	{
		return qbInt(base + rnd() * scale - rnd() * scale);
	}

	function initializeKingdoms(player, king)
	{
		var sides;
		var i;
		var side;

		player.food = difference(50000, 250000);
		king.food = difference(50000, 250000);
		player.civilians = difference(2500, 15000);
		king.civilians = difference(2500, 15000);

		sides = [player, king];
		for(i = 0; i < sides.length; i++) {
			side = sides[i];
			side.soldiers = qbInt(side.civilians / 4
				+ rnd() * side.civilians / 8
				- rnd() * side.civilians / 8);
		}

		player.kastle = difference(5000, 17500);
		king.kastle = difference(5000, 17500);
		player.kannons = difference(50, 250);
		king.kannons = difference(50, 250);
		player.katapults = qbInt(2 * rnd());
		king.katapults = qbInt(2 * rnd());
		player.assassins = qbInt(3 * rnd() + 3 * rnd());
		king.assassins = qbInt(3 * rnd() + 3 * rnd());

		for(i = 0; i < sides.length; i++)
			sides[i].gold = qbInt(20000 + rnd() - 2500 * rnd());

		player.guards = 0;
		king.guards = 0;
		for(i = 0; i < 5; i++)
			player.guards += 6 * rnd();
		for(i = 0; i < 5; i++)
			king.guards += 6 * rnd();
		player.guards = qbInt(player.guards);
		king.guards = qbInt(king.guards);
	}

	function displayedFoodMonths(side)
	{
		var value = side.food
			/ (2 * side.soldiers + side.civilians + F_00001);
		if(value < F_01)
			return 0;
		if(value > 9999)
			return 99999;
		return value;
	}

	function updateMonth(side, other)
	{
		var oldGold = side.gold;
		var result = {};
		var armyNeed;
		var shortage;

		result.immigrants = qbInt(side.kastle / 100);
		side.civilians += result.immigrants;

		result.taxes = Math.max(0, side.civilians);
		side.gold += result.taxes;

		result.payroll = Math.min(side.gold, side.soldiers);
		result.defectors = Math.min(side.soldiers,
			Math.max(0, side.soldiers - side.gold));
		side.gold -= result.payroll;
		side.soldiers -= result.defectors;
		other.soldiers += result.defectors;

		armyNeed = 2 * side.soldiers;
		result.army_food = Math.min(side.food, armyNeed);
		shortage = Math.max(0, armyNeed - side.food);
		result.soldiers_starved = Math.min(side.soldiers,
			qbInt(shortage / 2));
		side.food -= result.army_food;
		side.soldiers -= result.soldiers_starved;

		result.civilian_food = Math.min(side.food, side.civilians);
		result.civilians_starved = Math.min(side.civilians,
			Math.max(0, side.civilians - side.food));
		side.food -= result.civilian_food;
		side.civilians -= result.civilians_starved;
		result.treasury_change = side.gold - oldGold;
		return result;
	}

	function lossTarget(size)
	{
		if(size <= 0)
			return 0;
		return qbInt(rnd() * size / 2)
			+ qbInt(rnd() * size / 2) + 1;
	}

	function melee(attacker, target)
	{
		var playerAttacks = attacker.isplayer !== false;
		var player = playerAttacks ? attacker : target;
		var king = playerAttacks ? target : attacker;
		var playerTarget = lossTarget(player.soldiers);
		var kingTarget = lossTarget(king.soldiers);
		var playerKilled = 0;
		var kingKilled = 0;
		var attackerKilled;
		var targetKilled;

		/* The executable always generates the human target first. */
		if(playerTarget > 3 * kingTarget)
			kingTarget = king.soldiers;
		if(kingTarget > 3 * playerTarget)
			playerTarget = player.soldiers;

		while(kingKilled < kingTarget && playerKilled < playerTarget) {
			if(playerAttacks) {
				if(rnd() > F_066)
					kingKilled++;
				else
					playerKilled++;
			}
			else if(rnd() > F_066)
				playerKilled++;
			else
				kingKilled++;
		}

		player.soldiers -= playerKilled;
		king.soldiers -= kingKilled;
		player.score += kingKilled;
		king.score += playerKilled;
		attackerKilled = playerAttacks ? playerKilled : kingKilled;
		targetKilled = playerAttacks ? kingKilled : playerKilled;
		return {
			attacker_target: playerAttacks ? playerTarget : kingTarget,
			target_target: playerAttacks ? kingTarget : playerTarget,
			attacker_killed: attackerKilled,
			target_killed: targetKilled
		};
	}

	function assassinate(attacker, target)
	{
		var originalGuards = target.guards;
		var killed = 0;
		var passed = 0;
		var events = [];
		var i;

		attacker.assassins--;
		for(i = 0; i < originalGuards; i++) {
			if(rnd() > F_08) {
				killed++;
				target.guards--;
				events.push("killed");
				continue;
			}
			if(rnd() < F_02) {
				events.push("assassin_killed");
				return {
					success: false,
					guards_killed: killed,
					guards_passed: passed,
					assassin_killed: true,
					score: 0,
					events: events
				};
			}
			passed++;
			events.push("passed");
		}

		attacker.score += 1000 * originalGuards;
		return {
			success: true,
			guards_killed: killed,
			guards_passed: passed,
			assassin_killed: false,
			score: 1000 * originalGuards,
			events: events
		};
	}

	function Damage()
	{
		return {
			soldiers: 0,
			kastle: 0,
			kannons: 0,
			civilians: 0,
			katapults: 0,
			assassins: 0,
			guards: 0,
			miss: false
		};
	}

	function cannonDamage(attacker, target, aim)
	{
		var result = Damage();
		var men;
		var castle;
		var count;
		var first;
		var second;

		if(rnd() < F_01) {
			result.miss = true;
			return result;
		}
		men = aim === "men" ? 2 : 1;
		castle = aim === "castle" ? 2 : 1;
		count = attacker.kannons;

		if(rnd() > F_005)
			result.soldiers = men * Math.abs(qbInt(
				rnd() * count - rnd() * count));
		if(rnd() > F_075)
			result.soldiers = qbInt(rnd() * result.soldiers * 2);

		if(rnd() > F_015)
			result.kannons = qbInt(rnd() * count / 10);
		if(rnd() > F_09)
			result.kannons = qbInt(rnd() * result.kannons * 10);

		first = rnd() > F_085;
		second = rnd() > F_09;
		if(first || (castle !== 1 && second))
			result.katapults = qbInt(rnd() * target.katapults + 0.5);

		first = rnd() > F_02;
		second = rnd() > F_01;
		if(first || (castle !== 1 && second))
			result.kastle = qbInt(rnd() * count * castle);
		if(rnd() > F_08 && castle !== 1)
			result.kastle = qbInt(rnd() * result.kastle * 2);

		if(rnd() > F_025 && castle !== 1)
			result.civilians = castle * Math.abs(qbInt(
				rnd() * count - rnd() * count));
		if(rnd() > F_09 && castle !== 1)
			result.civilians = qbInt(rnd() * result.civilians * 5);
		if(rnd() > F_095 && castle !== 1)
			result.assassins = qbInt(rnd() * target.assassins + 0.5);
		if(rnd() > F_085 && castle !== 1)
			result.guards = qbInt(rnd() * target.guards + 0.5);
		return result;
	}

	function catapultDamage(attacker, target, aim)
	{
		var result = Damage();
		var men;
		var castle;
		var count;
		var width;
		var first;
		var second;

		if(rnd() < F_025) {
			result.miss = true;
			return result;
		}
		men = aim === "men" ? 2 : 1;
		castle = aim === "castle" ? 2 : 1;
		count = attacker.katapults;

		if(rnd() > F_005) {
			width = count * 250;
			result.soldiers = men * Math.abs(qbInt(
				rnd() * width - rnd() * width));
		}
		if(rnd() > F_09)
			result.soldiers = qbInt(rnd() * result.soldiers * 4);

		if(rnd() > F_01)
			result.kannons = qbInt(rnd() * count * 15);
		if(rnd() > F_075)
			result.kannons = qbInt(rnd() * result.kannons * 10);

		first = rnd() > F_09;
		second = rnd() > F_085;
		if(first || (castle !== 1 && second))
			result.katapults = qbInt(rnd() * target.katapults + 0.5);

		first = rnd() > F_015;
		second = rnd() > F_01;
		if(first || (castle !== 1 && second))
			result.kastle = qbInt(rnd() * count * 500 * castle);
		if(rnd() > F_075 && castle !== 1)
			result.kastle = qbInt(rnd() * result.kastle * 5);

		if(rnd() > F_05 && castle !== 1) {
			width = count * 500;
			result.civilians = castle * Math.abs(qbInt(
				rnd() * width - rnd() * width));
		}
		if(rnd() > F_09 && castle !== 1)
			result.civilians = qbInt(rnd() * result.civilians * 5);
		if(rnd() > F_095 && castle !== 1)
			result.assassins = qbInt(rnd() * target.assassins + 0.5);
		if(rnd() > F_085 && castle !== 1)
			result.guards = qbInt(rnd() * target.guards + 0.5);
		return result;
	}

	function applyDamage(attacker, target, damage)
	{
		var fields = [
			"soldiers", "kastle", "kannons", "civilians",
			"katapults", "assassins", "guards"
		];
		var actual = Damage();
		var i;
		var field;

		actual.miss = damage.miss;
		for(i = 0; i < fields.length; i++) {
			field = fields[i];
			actual[field] = Math.min(target[field], damage[field]);
			target[field] -= actual[field];
		}
		attacker.score += actual.soldiers + actual.kastle;
		return actual;
	}

	function aiPriorities(player, king)
	{
		var w = [];
		var i;
		var r;
		var kingFoodNeed;
		var cannonRatio;
		var catapultRatio;
		var menAim;
		var order;

		function put(index, value)
		{
			w[index] = single(value);
		}
		function add(index, value)
		{
			put(index, w[index] + value);
		}
		function mul(index, value)
		{
			put(index, w[index] * value);
		}

		for(i = 0; i < actions.length; i++)
			w[i] = single(rnd());

		r = king.food / (2 * king.soldiers + king.civilians + F_001);
		add(0, 4 - r);
		if(r < 2)
			mul(0, 1.5);
		if(r < 0)
			mul(0, 2);
		if(r < 0.75)
			mul(0, 3);
		if(r > 2)
			put(0, -Math.abs(w[0]));

		r = king.soldiers / (player.soldiers + 0.01);
		add(1, r);
		if(r > 2.5)
			mul(1, 2.5);
		if(r > 3.5)
			mul(1, 5);
		if(king.soldiers < 2 * player.soldiers)
			put(1, -w[1]);

		add(2, 15 - king.guards);
		if(king.guards < 5)
			put(2, rnd() + 99);
		kingFoodNeed = 2 * king.soldiers + king.civilians;
		if(player.assassins === 0 && kingFoodNeed < king.food / 3)
			put(2, rnd());
		if(player.assassins < 1)
			put(2, w[2] / 2);
		if(player.kastle < 1500 * king.katapults
				&& king.guards < 15 && player.assassins > 0)
			put(2, rnd() + 40);
		if(player.soldiers < king.kannons
				&& king.guards < 15 && player.assassins > 0)
			put(2, rnd() + 40);

		add(3, player.soldiers / (king.soldiers + 0.01));
		add(3, 2 * player.kannons / (king.soldiers + 0.01));
		add(3, 150 * player.katapults / (king.soldiers + 0.01));
		if(3 * (player.soldiers + player.civilians)
				> king.soldiers + king.civilians)
			mul(3, 4);
		if(player.food < 2 * king.soldiers + player.civilians)
			mul(3, 3);

		add(4, 1500 * player.katapults / (king.kastle + 0.01));
		add(4, (2 * player.kannons + 0.01) / (king.kastle + 0.01));
		if(king.kastle < 3000 * player.katapults
				|| king.kastle < 4 * player.kannons)
			mul(4, 2);
		if(king.kastle < 1500 * player.katapults
				|| king.kastle < 2 * player.kannons)
			mul(4, 2);

		add(5, 13 - player.guards);
		if(player.kannons > king.soldiers
				&& king.civilians < 3 * player.kannons)
			add(5, 4 * player.kannons / (king.soldiers + 0.01));
		add(5, 4 * player.kannons / (king.kastle + 0.01));
		if(king.soldiers < 500 * player.katapults
				&& king.civilians < 1500 * player.katapults)
			add(5, 500 * player.katapults
				/ (king.soldiers + 0.01));
		add(5, 3000 * player.katapults / (king.kastle + 0.01));
		if(player.soldiers > 1.5
				* (king.civilians + king.soldiers))
			add(5, 3 * player.soldiers
				/ (king.soldiers + 0.01));

		if(king.assassins === 0 && w[5] > 2)
			put(6, rnd() + w[5]);

		add(7, qbInt(king.gold / 25000));
		add(8, king.gold / 20000);
		cannonRatio = (king.gold / 100 + king.kannons) / 650;
		catapultRatio = qbInt(king.gold / 25000) + king.katapults;
		if(cannonRatio < catapultRatio)
			put(8, rnd());
		else
			put(7, rnd());

		add(9, 3 * king.soldiers / (king.civilians + 0.01));
		if(king.soldiers > king.gold)
			add(9, king.soldiers
				? (king.gold + king.civilians) / king.soldiers : 0);
		if(king.soldiers > 2 * (king.gold + king.civilians))
			mul(9, 2);
		if(player.soldiers < king.soldiers / 2)
			mul(9, 2);
		if(player.soldiers > 2 * player.food
				&& king.soldiers > (king.gold + king.civilians) / 5)
			mul(9, 2);

		menAim = (player.kastle > 8 * player.soldiers
				&& king.kannons < 250 * king.katapults)
			|| (player.kastle > 5 * player.soldiers
				&& king.kannons >= 250 * king.katapults);

		add(10, king.kannons / 400);
		if(menAim) {
			if(king.kannons > player.soldiers / 2)
				mul(10, 1.5);
			for(i = 1; i <= 3; i++) {
				if(player.soldiers < i * king.kannons)
					mul(10, 1.25);
			}
		}
		else {
			if(player.kastle < 2 * king.kannons)
				mul(10, 1.5);
			for(i = 3; i <= 4; i++) {
				if(player.kastle < i * king.kannons)
					mul(10, 1.25);
			}
		}
		if(player.kannons > 3 * king.soldiers
				&& king.civilians < 3000)
			mul(10, 1.25);
		if(500 * player.katapults > 4 * king.soldiers
				&& king.civilians < 3000)
			mul(10, 1.25);
		if(player.kannons > 2 * king.kastle && king.gold < 15000)
			mul(10, 1.25);
		if(king.kastle < 1000 * player.katapults
				&& king.gold < 15000)
			mul(10, 1.25);

		add(11, 1.5 * king.katapults);
		if(menAim) {
			if(player.soldiers < 250 * king.katapults)
				mul(11, 1.5);
			if(player.soldiers < 500 * king.katapults)
				mul(11, 1.25);
			if(player.soldiers < 1000 * king.katapults)
				mul(11, 1.25);
		}
		else {
			if(player.kastle < 1000 * king.katapults)
				mul(11, 1.5);
			if(player.kastle < 2000 * king.katapults)
				mul(11, 1.25);
			if(player.kastle < 3000 * king.katapults)
				mul(11, 1.25);
		}
		if(player.kannons > 3 * king.soldiers
				&& king.civilians < 3000)
			mul(11, 1.25);
		if(500 * player.katapults > 4 * king.soldiers
				&& king.civilians < 3000)
			mul(11, 1.25);
		if(player.kannons > 2 * king.kastle && king.gold < 5000)
			mul(11, 1.25);
		if(1000 * player.katapults > 2 * king.kastle
				&& king.gold < 5000)
			mul(11, 1.25);

		if(king.kannons > 500 * king.katapults)
			put(11, rnd());
		else
			put(10, rnd());

		order = [];
		for(i = 0; i < actions.length; i++)
			order.push({action: actions[i], score: w[i], index: i});
		order.sort(function(a, b) {
			if(a.score === b.score)
				return a.index - b.index;
			return b.score - a.score;
		});
		return {aim: menAim ? "men" : "castle", priorities: order};
	}

	function aiFoodQuantity(player, king)
	{
		var quantity = (2 * king.soldiers + king.civilians) * 13;
		if(quantity / 5 > king.gold)
			quantity = king.gold * 5;
		quantity = qbInt(quantity / 5) * 5;
		if(king.soldiers > 3.5 * player.soldiers)
			return 0;
		if(king.food / (2 * king.soldiers + king.civilians + F_001) > 3)
			return 0;
		return quantity > 0 ? quantity : 0;
	}

	function aiGuardQuantity(player, king)
	{
		var quantity;
		if(king.civilians > king.soldiers)
			quantity = qbInt(king.gold / 1000);
		else
			quantity = qbInt((king.gold - king.soldiers) / 1000);
		quantity = Math.min(quantity, 20 - king.guards);
		return quantity >= 1 ? quantity : 0;
	}

	function aiDraftQuantity(player, king)
	{
		var quantity;
		var step;
		var emergency = (2 * player.soldiers + player.civilians
				> player.food
				&& king.soldiers + king.civilians
				> 3 * player.soldiers)
			|| king.soldiers + king.civilians
				> 3 * (player.soldiers + player.civilians);

		if(emergency) {
			quantity = king.civilians;
			if(king.soldiers + king.civilians
					> 4.5 * player.soldiers)
				quantity = qbInt(4.5 * player.soldiers
					- king.soldiers);
			if(quantity > 0) {
				if(king.soldiers + quantity > king.gold / 2)
					return 0;
				if(2 * (king.soldiers + quantity)
						+ (king.civilians - quantity)
						> king.food / 2)
					return 0;
				return Math.min(quantity, king.civilians);
			}
		}

		if(500 * player.katapults > 2 * player.kannons)
			quantity = 400 * player.katapults - king.soldiers;
		else
			quantity = qbInt(3 * player.kannons) - king.soldiers;
		step = qbInt(king.civilians / 20) + 1;
		while(king.soldiers + quantity < player.soldiers
				&& king.civilians - quantity
					> king.soldiers + quantity)
			quantity += step;
		if(king.soldiers + quantity > king.civilians - quantity)
			quantity = qbInt(king.civilians / 1.7) - king.soldiers;
		if(king.soldiers + quantity > king.gold + king.civilians)
			quantity = king.gold + king.civilians;
		if(king.soldiers + quantity
				> king.food / 2 - king.civilians)
			quantity = qbInt(king.food / 2 - king.civilians);
		if(king.soldiers + quantity < player.kannons / 5)
			return 0;
		if(king.soldiers + quantity < player.katapults * 125)
			return 0;
		quantity = Math.min(quantity, king.civilians);
		if(quantity <= qbInt(king.soldiers / 1.5))
			return 0;
		return quantity;
	}

	function aiFortifyQuantity(player, king)
	{
		var quantity;
		if(king.civilians > king.soldiers)
			quantity = qbInt(king.gold / 10);
		else
			quantity = qbInt((king.gold - king.soldiers) / 10);
		if(quantity < 750 * player.katapults)
			return 0;
		if(quantity < 2 * player.kannons)
			return 0;
		return quantity > qbInt(king.kastle / 2) ? quantity : 0;
	}

	function aiAssassinQuantity(player, king)
	{
		var quantity;
		if(king.assassins > 0)
			return 0;
		quantity = qbInt((king.gold - king.soldiers) / 7500);
		if(quantity > 2)
			quantity = 3;
		return quantity > 0 ? quantity : 0;
	}

	function aiCatapultQuantity(player, king)
	{
		var quantity;
		if(king.civilians > king.soldiers)
			quantity = qbInt(king.gold / 25000);
		else
			quantity = qbInt((king.gold - king.soldiers) / 25000);
		if(king.katapults / 2 > quantity && quantity < 2)
			return 0;
		return quantity > 0 ? quantity : 0;
	}

	function aiCannonQuantity(player, king)
	{
		var quantity;
		if(king.civilians > king.soldiers)
			quantity = qbInt(king.gold / 100);
		else
			quantity = qbInt((king.gold - king.soldiers) / 100);
		if(quantity < 200 && king.kannons > 100)
			return 0;
		if(quantity <= qbInt(king.kannons / 1.5))
			return 0;
		return quantity > 100 ? quantity : 0;
	}

	function aiRetireQuantity(player, king)
	{
		var quantity = king.soldiers - player.soldiers;
		var remaining = player.soldiers;
		if(remaining < 4 * player.kannons)
			return 0;
		if(remaining < 750 * player.katapults)
			return 0;
		if(remaining < player.soldiers / 2
				&& king.soldiers < king.civilians)
			return 0;
		if(king.civilians > 1.5 * king.soldiers)
			return 0;
		if(king.soldiers > 2.5 * player.soldiers)
			return 0;
		return quantity > qbInt(king.soldiers / 3) ? quantity : 0;
	}

	function aiQuantity(action, player, king)
	{
		switch(action) {
			case "buy_food":
				return aiFoodQuantity(player, king);
			case "hire_guards":
				return aiGuardQuantity(player, king);
			case "draft":
				return aiDraftQuantity(player, king);
			case "fortify_castle":
				return aiFortifyQuantity(player, king);
			case "hire_assassins":
				return aiAssassinQuantity(player, king);
			case "buy_catapults":
				return aiCatapultQuantity(player, king);
			case "buy_cannons":
				return aiCannonQuantity(player, king);
			case "retire_soldiers":
				return aiRetireQuantity(player, king);
		}
		return 0;
	}

	return {
		setRandom: setRandom,
		resetRandom: resetRandom,
		qbInt: qbInt,
		initializeKingdoms: initializeKingdoms,
		displayedFoodMonths: displayedFoodMonths,
		updateMonth: updateMonth,
		melee: melee,
		assassinate: assassinate,
		cannonDamage: cannonDamage,
		catapultDamage: catapultDamage,
		applyDamage: applyDamage,
		aiPriorities: aiPriorities,
		aiQuantity: aiQuantity
	};
})();

if(typeof module !== "undefined" && module.exports)
	module.exports = KNKMath;
