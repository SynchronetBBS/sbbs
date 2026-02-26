//Olodrin's Orphans v0.1 - a LoRD 5.00 JS IGM by Underminer
//
//I used a the "header" for sections from Mortifis' Grab Bag IGM
//I also used the install and entry from Deuce's version of Barak's almost verbatim
//(other than the name)
//
//If you modify this IGM, you must make it clear that your version is not the original,
//and you must give credit. Failure to do these things brands you unfavourably.


var boy_names = [];
var girl_names = [];
var how_they_died = [];

var looking_for_waif_text = [
	"After skulking through the grass a while, you spot your quarry!",
	"You try wandering casually down a path, lunging out as a youth passes",
	"Playing dead, you jump up when a curious youth approaches",
	"You are completely surprised as you are suddenly jumped!",
	"You charge into the fields screaming bloody murder!",
	"Thinking you're sneaking, you clatter loudly down the path",
	"'I suure hate eating this chocolate!' you scream out. Suddenly...",
	"You stand perfectly still until a wild youth looks at you, puzzled"
]

var intermediateaction = [
	"You flail wildly, trying to get a hand on the quick youngster",
	"You decide your best bet is to sweep the leg",
	"A smarter individual would have brought a net...",
	"You grab a chunk of flesh and bite down hard",
	"You close your eyes and let an inner voice guide you"
]
var goodcatch = [
	"Success! You vow to hug him and squeeze him and call him George",
	"Congratulations! Under that mess of hair is a little girl to win over",
	"You manage to get hold of a scamp and calm them down!",
	"Your first attempt fails, but chocolate bribery works!"

]

var badcatch = [
	"You get hit with a stick behind the knee and go down hard",
	"Somehow you manage to kick YOURSELF in the face",
	"You feel teeth dig into your arm and scream out in pain!",
	"You feel an impact in the back of the head and see stars",
	"After feeling pain, you stop biting yourself and take stock"
]

var orphan_guardian = [
	{
		title:'Father',
		sex:'Male'
	},
	{
		title:'Mother',
		sex:'Female'
	},	
	{
		title:'Grandfather',
		sex:'Male'
	},
	{
		title:'Grandmother',
		sex:'Female'
	},
	{
		title:'Papa',
		sex:'Male'
	},
	{
		title:'Mama',
		sex:'Female'
	},		
	{
		title:'Aunt',
		sex:'Female'
	},			
	{
		title:'Uncle',
		sex:'Male'
	},
	{
		title:'Brother',
		sex:'Male'
	},
	{
		title:'Sister',
		sex:'Female'
	},
	{
		title:'Uncle Daddy',
		sex:'Male'
	},
	{
		title:'Sister Mama',
		sex:'Female'
	}

];

function getboynames()
{
	var boynamefile = new File(gamedir('/oorphans/boynames.dat'));

	if (!boynamefile.open('r')) {
		return;
	}
	var i;
	var rname;
	while (true){
		rname = boynamefile.readln();
		if (rname === null) {
			break;
		}		
		boy_names.push(rname);
	}
	boynamefile.close();
}
function getgirlnames()
{
	var girlnamefile = new File(gamedir('/oorphans/girlnames.dat'));

	if (!girlnamefile.open('r')) {
		return;
	}
	var i;
	var rname;
	while (true){
		rname = girlnamefile.readln();
		if (rname === null) {
			break;
		}		
		girl_names.push(rname);
	}
	girlnamefile.close();
}
function gethowdiedlist(){
	var howdiedfile = new File(gamedir('/oorphans/howdied.dat'));

	if (!howdiedfile.open('r')){
		return;
	}
	var i;
	var diedway;
	while (true){
		diedway = howdiedfile.readln();
		if (diedway === null){
			break;
		}
		how_they_died.push(diedway);
	}
	howdiedfile.close();
}



// get_head pulled from grabbag IGM.
function get_head(str)
{
	lln('`r0`0`2`c  `%' + str);
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	sln('');
}


function orphan_description(){
	var sexrand = random(2);
	if (sexrand == 1){
		var orphan_name = boy_names[random(boy_names.length)];
		var gaurdiannum = random(orphan_guardian.length);
		var diedby = how_they_died[random(how_they_died.length)];
		if (orphan_guardian[gaurdiannum].sex == "Female"){
			var gaurdian_name = girl_names[random(girl_names.length)]
		}
		else{
			var gaurdian_name = boy_names[random(boy_names.length)]
		}		
		//"Male"
		sln('');
		lln(" `2This is `!" + orphan_name);
		lln(' `2His last guardian was his `!' + orphan_guardian[gaurdiannum].title + ", `$" + gaurdian_name + ", `2who... ");
		lln(" `4" + diedby);
		lln( ' `2Needless to say, he ended up in our care')
	}
	else{
		var orphan_name = girl_names[random(girl_names.length)];
		var gaurdiannum = random(orphan_guardian.length);
		var diedby = how_they_died[random(how_they_died.length)];
		if (orphan_guardian[gaurdiannum].sex == "Female"){
			var gaurdian_name = girl_names[random(girl_names.length)]
		}
		else{
			var gaurdian_name = boy_names[random(boy_names.length)]
		}		
		//"Female"
		sln('');
		lln(" `2This is `!" + orphan_name);
		lln(' `2Her last guardian was her `!' + orphan_guardian[gaurdiannum].title + ", `$" + gaurdian_name + ", `2who... ");
		lln(" `4" + diedby);
		lln( ' `2Needless to say, she ended up in our care')		
	}
	return orphan_name;
}
function buy_orphans(){
	var orphanprice = Math.round((player.level * player.level)*1000)
	forphanprice = String(orphanprice).replace(/(.)(?=(\d{3})+$)/g,'$1,')
	sclrscr();
	get_head('Get yourself a waif!');
	var newname = orphan_description();
	more_nomail();
	sln('');	
	lln(" It will cost you `$" + forphanprice + " gold `2 to adopt this child.")
	sln('');
	sln('');
	lln(" Do it? [y/N]:")
	ch = getkey().toUpperCase();
	if ('YN'.indexOf(ch) == -1) {
		ch = 'N';
	}
	if (ch == "Y"){
		if (player.gold < orphanprice){
			sln('');
			lln(" `!You fool! `2You don't have enough gold!");
			more_nomail();
		}
		else{
			player.gold = (player.gold - orphanprice);
			player.kids += 1;
			sln('');
			lln(" `2 Take good care of `!" + newname + "!`2");
			more_nomail();			
		}
	}
}
function sell_orphans(){
	var orphanprice = Math.round(((player.level * player.level)*1000)/14)
	//var orphanprice = Math.round(((player.level * player.level)*1000)*1000)
	forphanprice = String(orphanprice).replace(/(.)(?=(\d{3})+$)/g,'$1,')
	if (player.kids < 1) {
		sln('');
		sln('');
		lln(" `!You fool! You don't have any children!");
		lln(" `3Did you get hit in the head one too many times?");
		sln('');
		sln('');
		sln('');
		more_nomail();
	}
	else{
		sclrscr();
		get_head('Sell your rugrats');
		lln(" `2'Hmmm...' says `!Olodrin, `2looking over the youngster you push forward.");
		lln(" `2I will give you `$" + forphanprice + "`2 for this child.")
		lln(" Do it? [y/N]:")
		ch = getkey().toUpperCase();
		if ('YN'.indexOf(ch) == -1) {
			ch = 'N';
		}
		if (ch == "Y"){
			player.gold = (player.gold += orphanprice);
			player.kids = (player.kids - 1);
			sln('');
			lln(" `2 Don't worry, we'll take good care of `!" + boy_names[random(boy_names.length)] +"!`2");
			more_nomail();			
		}
		else{
			sln('');
			lln(" `2 Suit yourself!`2");
			more_nomail();			
		}

	}

	
}
function trade_orphans_for_horse(){
	var orphanstotrade = (player.level * player.level);
	forphanstotrade = String(orphanstotrade).replace(/(.)(?=(\d{3})+$)/g,'$1,')
	if (player.horse){
		sln('');
		sln('');
		lln(" `!You stupid nit! You already have a horse!");
		sln('');
		more_nomail();
	}
	else{
		sclrscr();
		get_head('Trade ragamuffins for a Steed!');
		lln(" `!Olodrin `2opens his mouth to speak:");
		lln(" For you, I will give you a horse in exchange for `$" + forphanstotrade + " `2children.");
		lln(" Do it? [y/N]:")
		ch = getkey().toUpperCase();
		if ('YN'.indexOf(ch) == -1) {
			ch = 'N';
		}
		if (ch == "Y"){
			if (player.kids >= orphanstotrade){
				player.kids = (player.kids - orphanstotrade);
				player.horse = true;
				sln('');
				lln(" `2 Don't worry, we'll take good care of `!THESE `2children...");
				more_nomail();							
			}
			else{
				sln('');
				sln('');
				lln(" `!You stupid nit! You only have `$" + player.kids + " `2children!");
				sln('');
				more_nomail();
			}

		}
		else{
			sln('');
			lln(" `2 Suit yourself!`2");
			more_nomail();			
		}
	}

}
function catch_orphans(){
	sclrscr();
	get_head("Trying to catch a wild child");
	lln(" " + looking_for_waif_text[random(looking_for_waif_text.length)]);
	more_nomail();
	lln(" " + intermediateaction[random(intermediateaction.length)]);
	more_nomail();
	var resultselect = random(7);
	if ((resultselect == 1) || (resultselect == 6)){
		lln(" `2" + goodcatch[random(goodcatch.length)]);
		more_nomail();
		player.kids += 1;
	}
	else if (resultselect == 2){
		lln(" `2" + badcatch[random(badcatch.length)]);
		var tolose = random(5);
		more_nomail();
		lln(" `2You Lose `$" + tolose + "`2 charm due to injuries");
		player.cha = (player.cha - tolose);
		more_nomail();
	}
	else if ((resultselect == 0) || (resultselect == 3)){
		lln(" `2" + badcatch[random(badcatch.length)]);
		var rnglose = random(20);
		var tolose = Math.round(((rnglose/100) * player.gold))
		var ftolose = String(tolose).replace(/(.)(?=(\d{3})+$)/g,'$1,')
		more_nomail();
		lln(" `2That scamp made off with `$" + ftolose + "`2 of your gold!");
		player.gold = (player.gold - tolose);
		more_nomail();
	}	
	else if (resultselect == 4){
		lln(" `2" + badcatch[random(badcatch.length)]);
		var rnglose = random(10);
		var tolose = Math.round(((rnglose/100) * player.exp))
		var ftolose = String(tolose).replace(/(.)(?=(\d{3})+$)/g,'$1,')
		more_nomail();
		lln(" `2You get hit so hard you forget your own birthday...");
		lln(" `2You lose `$" + ftolose + "`2 experience!");
		player.exp = (player.exp - tolose);
		more_nomail();
	}	
	else if (resultselect == 5){
		lln(" `2" + badcatch[random(badcatch.length)]);
		var rnglose = random(25);
		rnglose += 50
		var tolose = Math.round(((rnglose/100) * player.gem))
		var ftolose = String(tolose).replace(/(.)(?=(\d{3})+$)/g,'$1,')
		more_nomail();
		lln(" `2Those little guttersnipes make off with `$" + ftolose + "`2 of your Gems!");
		player.gem = (player.gem - tolose);
		more_nomail();
	}
	else{
		lln(" `2Suddenly you find yourself fighting with yourself. There is no scamp in sight.")
		more_nomail();
	}	
}
function igminfo(){
	sclrscr();
	sln('');
	sln('');
	lln("                               `!Olodrin's Orphanage");
	lln("                `2A `4LoRD`2 5.00 `2IGM by `$Underminer `2in the year `$2020.`2");
	more_nomail();	
}

function welcome(){
	sclrscr();
	sln('');
	lln('  `2You travel for what feels like ages down a beaten dirt path.');
	more_nomail();
	lln('  `2Finally, up ahead you see a dilapidated sign lit dimly by `$two torches.`2');
	sln('');
	more_nomail();
	lln("  `2It would seem you have arrived at `4Olodrin's Orphanage`2");
	sln('');
	more_nomail();

}
function olodrin_main(){
	sclrscr();
	get_head("Olodrin's Orphanage");
	lln('  `!Olodrin`2 stands before you, arms crossed');
	lln('  `2He waits for you to make a decision');
	sln('');
	lln('  `2(`0A`2)dopt an orphan');
	lln('  `2(`0G`2)ive child up for adoption');
	lln('  `2(`0C`2)atch a feral child');
	lln('  `2(`0T`2)rade some of your children for a horse');
	lln('  `2(`0Q`2)uit and go home');
	sln('');
	lw('  `2You decide to... [`0Q`2] :`%');	
}

function good_bye(what) {
	if(what === undefined) what = 'uneventful';
	sln('');
	lln("  `2You have had a `$'+what+' `2visit to the Olodrin's Orphans today.");
	sln('');
	lw('  You continue your on you way back to `4');
	mswait(1000);
	say_slow(' The Undermine!');
	mswait(1000);
	exit(0);
}



function main(){
	if (!dk.console.ansi){
		sln(' NOTE: This IGM has been written with an ANSI terminal in mind.');
		sln(' While we have made efforts to ensure ASCII compatability, an ANSI terminal');
		sln(' will offer a superior experience.');
		sln(' ');
		sln(" If your terminal supports ANSI but it wasn't detected,  you can switch to");
		sln(' ANSI mode by pressing 3 from the main menu. DO NOT do this if you are unsure!');
	}
	getboynames();
	getgirlnames();
	gethowdiedlist();
	welcome();
	igminfo();
	var exitigm = false;
	while(!exitigm){
		olodrin_main();
		ch = getkey().toUpperCase();
		if ('QAGC?T'.indexOf(ch) == -1) {
			ch = 'Q';
		}		
		if (ch == 'A'){
			buy_orphans();
		}
		if (ch == 'G'){
			sell_orphans();
		}		
		if (ch == 'C'){
			catch_orphans();
		}	
		if (ch == 'T'){
			trade_orphans_for_horse();
		}
		if (ch == 'Q'){
			sln('');
			sln('');
			sln('');
			lln(' `2Olodrin asks "`4Leaving already?`2" as you turn back to town.')
			sln('');
			sln('');
			more_nomail();
			igminfo();
			good_bye();
		}	
		if (ch== '?'){
			olodrin_main();
		}
	}

}
// install based on example in Barak's House IGM
if (argc == 1 && argv[0] == 'INSTALL') {
	var install = {
		desc:"`9Olodrin's `3Orphanage",
	}
	exit(0);
}
else {
	main();
	exit(0);
}
