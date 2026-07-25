"use strict";

var assert = require("assert");
var math = require("./knk_math.js");

function kingdom(values)
{
	var state = {
		score: 0,
		food: 0,
		civilians: 0,
		soldiers: 0,
		kastle: 0,
		kannons: 0,
		katapults: 0,
		assassins: 0,
		gold: 0,
		guards: 0
	};
	var key;
	for(key in values)
		state[key] = values[key];
	return state;
}

function useDraws(values)
{
	var index = 0;
	math.setRandom(function() {
		if(index >= values.length)
			throw new Error("RND stream exhausted");
		return values[index++];
	});
	return function() {
		assert.strictEqual(index, values.length,
			"unexpected number of RND draws");
	};
}

function losses(damage)
{
	return {
		soldiers: damage.soldiers,
		kastle: damage.kastle,
		kannons: damage.kannons,
		civilians: damage.civilians,
		katapults: damage.katapults,
		assassins: damage.assassins,
		guards: damage.guards
	};
}

(function testZeroDrawInitialState() {
	var player = kingdom({});
	var king = kingdom({});
	var done = useDraws(new Array(40).fill(0));
	var expected = kingdom({
		food: 250000,
		civilians: 15000,
		soldiers: 3750,
		kastle: 17500,
		kannons: 250,
		gold: 20000
	});

	math.initializeKingdoms(player, king);
	done();
	assert.deepStrictEqual(player, expected);
	assert.deepStrictEqual(king, expected);
})();

(function testDefectorsTransferBeforeOtherUpdate() {
	var first = kingdom({soldiers: 10});
	var second = kingdom({food: 100, gold: 100});
	var firstResult = math.updateMonth(first, second);
	var secondResult = math.updateMonth(second, first);

	assert.strictEqual(firstResult.defectors, 10);
	assert.strictEqual(secondResult.payroll, 10);
	assert.strictEqual(second.soldiers, 10);
	assert.strictEqual(second.food, 80);
})();

(function testFoodMonthClamps() {
	assert.strictEqual(math.displayedFoodMonths(
		kingdom({food: 0, civilians: 1})), 0);
	assert.strictEqual(math.displayedFoodMonths(
		kingdom({food: 100000})), 99999);
})();

(function testAssassination() {
	var attacker = kingdom({assassins: 1});
	var target = kingdom({guards: 3});
	var done = useDraws([0.9, 0.9, 0.9]);
	var result = math.assassinate(attacker, target);

	done();
	assert.strictEqual(result.success, true);
	assert.strictEqual(result.guards_killed, 3);
	assert.strictEqual(result.score, 3000);
	assert.strictEqual(attacker.score, 3000);
	assert.strictEqual(target.guards, 0);

	attacker = kingdom({assassins: 1});
	target = kingdom({guards: 3});
	done = useDraws([0.5, 0.1]);
	result = math.assassinate(attacker, target);
	done();
	assert.strictEqual(result.success, false);
	assert.strictEqual(result.assassin_killed, true);
	assert.strictEqual(attacker.score, 0);
})();

(function testMeleeStopsAtFirstLossTarget() {
	var attacker = kingdom({soldiers: 10});
	var target = kingdom({soldiers: 10});
	var done = useDraws([0, 0, 0, 0, 0.9]);
	var result = math.melee(attacker, target);

	done();
	assert.strictEqual(result.attacker_target, 1);
	assert.strictEqual(result.target_target, 1);
	assert.strictEqual(result.attacker_killed, 0);
	assert.strictEqual(result.target_killed, 1);
	assert.strictEqual(attacker.score, 1);
	assert.strictEqual(target.soldiers, 9);
})();

(function testComputerMeleeKeepsExecutableDrawOrder() {
	var attacker = kingdom({soldiers: 20});
	var target = kingdom({soldiers: 10});
	var draws = [0.2, 0.4, 0.6, 0.8];
	var done;
	var result;

	attacker.isplayer = false;
	target.isplayer = true;
	while(draws.length < 14)
		draws.push(0.9);
	done = useDraws(draws);
	result = math.melee(attacker, target);
	done();
	assert.strictEqual(result.attacker_target, 15);
	assert.strictEqual(result.target_target, 10);
	assert.strictEqual(result.attacker_killed, 0);
	assert.strictEqual(result.target_killed, 10);
})();

(function testHighDrawCastleWeaponPaths() {
	var attacker = kingdom({kannons: 10, katapults: 2});
	var target = kingdom({
		soldiers: 100,
		kastle: 20000,
		kannons: 100,
		civilians: 100,
		katapults: 4,
		assassins: 3,
		guards: 5
	});
	var cannon;
	var catapult;

	math.setRandom(function() { return 0.99; });
	cannon = math.cannonDamage(attacker, target, "castle");
	assert.deepStrictEqual(losses(cannon), {
		soldiers: 0,
		kastle: 37,
		kannons: 0,
		civilians: 0,
		katapults: 4,
		assassins: 3,
		guards: 5
	});

	math.setRandom(function() { return 0.99; });
	catapult = math.catapultDamage(attacker, target, "castle");
	assert.deepStrictEqual(losses(catapult), {
		soldiers: 0,
		kastle: 9801,
		kannons: 287,
		civilians: 0,
		katapults: 4,
		assassins: 3,
		guards: 5
	});
})();

(function testWeaponMissIsAnEarlyReturn() {
	var attacker = kingdom({kannons: 10, katapults: 2});
	var target = kingdom({soldiers: 100, kastle: 20000});
	var done = useDraws([0]);
	var damage = math.cannonDamage(attacker, target, "men");

	done();
	assert.strictEqual(damage.miss, true);
	assert.deepStrictEqual(losses(damage), losses({
		soldiers: 0, kastle: 0, kannons: 0, civilians: 0,
		katapults: 0, assassins: 0, guards: 0
	}));

	done = useDraws([0]);
	damage = math.catapultDamage(attacker, target, "castle");
	done();
	assert.strictEqual(damage.miss, true);
})();

(function testDamageClampingAndScore() {
	var attacker = kingdom({});
	var target = kingdom({soldiers: 2, kastle: 3, guards: 1});
	var actual = math.applyDamage(attacker, target, {
		soldiers: 10, kastle: 10, kannons: 0, civilians: 0,
		katapults: 0, assassins: 0, guards: 10, miss: false
	});

	assert.strictEqual(actual.soldiers, 2);
	assert.strictEqual(actual.kastle, 3);
	assert.strictEqual(actual.guards, 1);
	assert.strictEqual(attacker.score, 5);
	assert.strictEqual(target.soldiers, 0);
	assert.strictEqual(target.kastle, 0);
})();

(function testAIOrderingAndSinglePrecisionStores() {
	var done = useDraws(new Array(16).fill(0));
	var selection = math.aiPriorities(kingdom({}), kingdom({}));

	done();
	assert.strictEqual(selection.aim, "castle");
	assert.deepStrictEqual(selection.priorities.slice(0, 4).map(
		function(item) { return item.action; }), [
			"hire_guards", "buy_food", "assassinate", "hire_assassins"
		]);
	assert.deepStrictEqual(selection.priorities.slice(0, 4).map(
		function(item) { return item.score; }), [49.5, 18, 13, 13]);
})();

(function testAIQuantitiesAndGates() {
	var player = kingdom({soldiers: 100, civilians: 100});
	var king = kingdom({
		soldiers: 100,
		civilians: 500,
		gold: 10000,
		food: 10000
	});
	assert.strictEqual(math.aiQuantity("draft", player, king), 350);

	player = kingdom({soldiers: 1000, kannons: 100, katapults: 1});
	king = kingdom({
		soldiers: 100,
		civilians: 1000,
		gold: 100000,
		kastle: 1000
	});
	assert.strictEqual(math.aiQuantity("buy_food", player, king), 15600);
	assert.strictEqual(math.aiQuantity("fortify_castle", player, king), 10000);

	king.guards = 18;
	assert.strictEqual(math.aiQuantity("hire_guards", player, king), 2);
	king.assassins = 0;
	assert.strictEqual(math.aiQuantity("hire_assassins", player, king), 3);
	king.katapults = 0;
	assert.strictEqual(math.aiQuantity("buy_catapults", player, king), 4);
	king.kannons = 100;
	assert.strictEqual(math.aiQuantity("buy_cannons", player, king), 1000);

	player = kingdom({soldiers: 1000});
	king = kingdom({soldiers: 1800, civilians: 1000});
	assert.strictEqual(math.aiQuantity("retire_soldiers", player, king), 800);
})();

math.resetRandom();
console.log("knk_math_test.js: all tests passed");
