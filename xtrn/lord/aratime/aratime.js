var Arag_Defs = [
	{
		prop:'day',
		type:'SignedInteger',
		def:-1
	},
	{
		prop:'played',
		type:'Array:150:Boolean',
		def:eval('var aret = []; while(aret.length < 150) aret.push(false); aret;')
	}
];

var astate;
var as;

function menu()
{
	var ch;
	var done = false;
	var tempstr;
	var tempnum;
	var cashbet;
	var realnum;
	var endtime = 0;
	var times = 0;

	function get_guess() {
		var guess;
		var result;
		var now;

		lw('  `2Guess?`% ');
		guess = parseInt(getstr(0, 0, 10, 0, 7, '', {integer:true}), 10);
		sln('');
		now = (new Date()).valueOf();
		if (endtime != 0 && now > endtime)
			done = true;
		times += 1;
		if (times > 15)
			done = true;
		if (!done) {
			if (guess > realnum)
				result = 'Too High';
			else if ( guess < realnum)
				result = 'Too Low';
		}
		else if (guess === realnum)
			done = false;
		if (!done && guess !== realnum) {
			lln('  `0'+result);
			lln('  `2You have `$'+pretty_int(endtime === 0 ? 20 : ((endtime - now)/1000))+' `2seconds left!');
			sln('');
		}
		if (done)
			return -1;
		return guess;
	}

	sclrscr();
	sln('');
	sln('');
	lln('  `2You take a hesitant step toward the opening of `0Aragorn`2\'s Cave.');
	lln('  You\'ve heard of his games making some very rich indeed, and you');
	lln('  desire that wealth.  But others have been broken - destined to be');
	lln('  level five warriors forever...');
	sln('');
	lw('  `2Shall you enter? [`5N`2]:`% ');
	ch = getkey().toUpperCase();
	if (ch !== 'Y')
		ch = 'N';
	sln(ch);
	if (ch === 'N') {
		sln('');
		lln('  `2You turn, and run back to the realm.');
		more_nomail();
		exit(0);
	}
	sclrscr();
	sln('');
	sln('');
	lln('  `0Aragorn `2smiles, to welcome you.  Many piles of gold are stacked');
	sln('  by his side.  The dingy room radiates a rare type of warmth, and ');
	sln('  you are compelled to sit down in the chair opposite him.');
	sln('');
	tempstr = (player.sex === 'F' ? 'daughter' : 'son');
	lln('  `%"`0My '+tempstr+', I play a very simple game.  You have 20');
	sln('  seconds and 15 tries to guess my number, from 1 to 1000.  If');
	sln('  you get it right, then I will give you your money, and half');
	sln('  again.  If you do not, then you lose all.');
	sln('');
	if (player.gold < 1) {
		lln('  `0Aragorn `2suddenly stands up.');
		lln('  `%"`0You have no money!  Why don\'t you get some, and come back later!');
		more_nomail();
		exit(0);
	}
	lln('  `%"`0Good.  Now, do you wish to play?`%"');
	sln('');
	lw('  `2[`5Y`2]:`% ');
	ch = getkey().toUpperCase();
	if (ch !== 'N')
		ch = 'Y';
	sln(ch);
	if (ch === 'N') {
		sln('');
		lln('  `0Aragorn `2dismisses you with a wave of his hand.');
		more_nomail();
		exit(0);
	}
	astate.played[player.Record] = true;
	astate.put();
	sclrscr();
	sln('');
	sln('');
	do {
		lln('  `2How much of your `$'+pretty_int(player.gold)+' `2will you wager?`% ');
		sw('  ');
		tempnum = parseInt(getstr(0, 0, 10, 0, 7, '', {integer:true}), 10);
	} while (isNaN(tempnum) || tempnum < 0 || tempnum > player.gold);
	cashbet = tempnum;
	player.gold -= cashbet;
	sln('');
	lln('  `0Aragon `2smiles.  He turns over his timeglass.');
	sln('');
	realnum = random(1000) + 1;
	tempnum = get_guess();
	endtime = (new Date()).valueOf() + 20000;
	while (tempnum !== realnum && !done) {
		tempnum = get_guess();
	}
	if (done) {
		sln('');
		if (times < 16)
			lln('  `2Time\'s Up!')
		else
			lln('  `2Guess Limit Exceeded!');
		sln('');
		lln('  `0Aragorn `2smiles softly to himself, and deposits your `$'+pretty_int(cashbet));
		lln('  `2into his pocket.  He then sits down, and waves you away.');
		sln('');
		lln('  `2You trudge out of the cave, very disappointed.  Maybe tomorrow.');
		more_nomail();
		exit(0);
	}
	sln('');
	lln('  `0Aragorn `2looks extremely surprised.  You try to hide the smug look');
	sln('  on your face.');
	sln('');
	player.gold += cashbet;
	cashbet = parseInt(cashbet / 2, 10);
	player.gold += cashbet;
	lln('  `%"`0So you beat me once... Beginner\'s luck.  Anyway, I do play fair,');
	lln('   so here is the `$'+pretty_int(cashbet)+' `0gold I owe you.');
	sln('');
	lln('  `2You think that this would be a good time to depart, before `0Aragon');
	lln('  `2grabs his money back.  You skip home, delighted!');
	more_nomail();
	exit(0);
}

function chkmaint()
{
	var i;

	as = new RecordFile(js.exec_dir+'aragtime.dat', Arag_Defs);
	js.on_exit('as.locks.forEach(function(x) {as.unLock(x); as.file.close()});');
	if (as.length < 1) {
		astate = as.new(true);
	}
	else {
		astate = as.get(0, true);
	}
	if (astate.day != state.days) {
		for (i = 0; i < astate.played.length; i++)
			astate.played[i] = false;
		astate.day = state.days;
		astate.put(false);
	}
	else
		astate.unLock();
	if (astate.played[player.Record] === true) {
		sclrscr();
		sln('');
		sln('');
		lln('  `0Aragorn`2\'s Cave looks darker than it usually does.');
		lln('  `2Maybe he\'s not home...');
		sln('');
		more_nomail();
		exit(0);
	}
}

// TODO CONFIG
if (argc == 1 && argv[0] == 'INSTALL') {
	var install = {
		desc:'`%Aragorn`2\'s Timer'
	}
	exit(0);
}
else {
	chkmaint();
	menu();
}
