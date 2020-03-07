/*jslint bitwise, this, devel, getset, for, eval*/

// TODO: Version record format with update functions...
/*
 * All records need to be loaded from a single function.
 * That function needs to check a record version, and
 * update as needed.
 *
 * TODO: Record revisions.
 * 
 * Do a transaction-like thing where you can only write the
 * same revision you fetched.
 */
'use strict';
js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js");
dk.console.auto_pause = false;
if (js.global.console !== undefined) {
	console.ctrlkey_passthru = dk_old_ctrlkey_passthru;
	console.ctrlkey_passthru = '+[';
}
delete dk.console.local_screen;
delete dk.console.remote_screen;
require("recordfile.js", "RecordFile");
require('recorddefs.js', 'Player_Def');

var ver = '5.00 JS';
var pfile;
var psock;
var statefile;
var txtfile;
var txtindex = {};
var player;
var which_castle = random(5) + 1;
var state;
var curlinenum = 1;
var curcolnum = 1;
var morechk = true;
var cleanup_files = [];
var whitelist = [];

var trainer_stats = [
	{
		// You don't need to level up from zero...
	},
	{
		is_arena:true,
		name:'Halder',
		weapon:'Short Sword',
		swear:'Belar!!! You are truly a great warrior!',
		needstr1:'Gee, your muscles are getting bigger than mine...',
		needstr2:'',
		death:'  You blew your master away!',
		str:15,
		hp:30,
		hp_gained:10,
		str_gained:5,
		def:2,
		need:100
	},
	{
		is_arena:true,
		name:'Barak',
		weapon:'Battle Axe',
		swear:'Children of Mara!!! You have bested me??!',
		needstr1:'You know, you are actually getting pretty good with',
		needstr2:'that thing..."',
		death:'  You blew your master away!',
		str:17,
		hp:40,
		hp_gained:15,
		str_gained:7,
		def:3,
		need:400
	},
	{
		is_arena:true,
		name:'Aragorn',
		weapon:'Twin Swords',
		swear:'Torak\'s Eye!!!  You are a great warrior!',
		needstr1:'You have learned everything I can teach you.',
		needstr2:'',
		death:'  You blew your master away!',
		str:35,
		hp:70,
		hp_gained:20,
		str_gained:10,
		def:5,
		need:1000
	},
	{
		is_arena:true,
		name:'Olodrin',
		weapon:'Power Axe',
		swear:'Ye Gods!!  You are a master warrior!',
		needstr1:'You are becoming a very skilled warrior.',
		needstr2:'',
		death:'  You blew your master away!',
		str:70,
		hp:120,
		hp_gained:30,
		str_gained:12,
		def:10,
		need:4000
	},
	{
		is_arena:true,
		name:'Sandtiger',
		weapon:'Blessed Sword',
		swear:'Very impressive...Very VERY impressive.',
		needstr1:'Gee - You really know how to handle your shaft!',
		needstr2:'',
		death:'  You blew your master away!',
		str:100,
		hp:200,
		hp_gained:50,
		str_gained:20,
		def:15,
		need:10000
	},
	{
		is_arena:true,
		name:'Sparhawk',
		weapon:'Double Bladed Sword',
		swear:'This Battle is yours...You have fought with honor.',
		needstr1:'You\'re getting the hang of it now!',
		needstr2:'',
		death:'  You blew your master away!',
		str:150,
		hp:400,
		hp_gained:75,
		str_gained:35,
		def:22,
		need:40000
	},
	{
		is_arena:true,
		name:'Atsuko Sensei',
		weapon:'Huge Curved Blade',
		swear:'Even though you beat me, I am proud of you.',
		needstr1:'You are ready to be tested on the battle field!',
		needstr2:'',
		death:'  You blew your master away!',
		str:250,
		hp:600,
		hp_gained:125,
		str_gained:50,
		def:35,
		need:100000
	},
	{
		is_arena:true,
		name:'Aladdin',
		weapon:'Shiny Lamp',
		swear:'I don\'t need a genie to see that you beat me, man!',
		needstr1:'You REALLY know how to use your &PWE!!!',
		needstr2:'',
		death:'  You blew your master away!',
		str:350,
		hp:800,
		hp_gained:185,
		str_gained:75,
		def:60,
		need:400000
	},
	{
		is_arena:true,
		name:'Prince Caspian',
		weapon:'Flashing Rapier',
		swear:'Good show, chap!  Jolly good show!',
		needstr1:'Something tells me that you are as good as I am now.',
		needstr2:'',
		death:'  You blew your master away!',
		str:500,
		hp:1200,
		hp_gained:250,
		str_gained:110,
		def:80,
		need:1000000
	},
	{
		is_arena:true,
		name:'Gandalf',
		weapon:'Huge Fireballs',
		swear:'Torak\'s Tooth!  You are great!',
		needstr1:'You are becoming a very skilled warrior',
		needstr2:'',
		death:'  You blew your master away!',
		str:800,
		hp:1800,
		hp_gained:350,
		str_gained:150,
		def:120,
		need:4000000
	},
	{
		is_arena:true,
		name:'Turgon',
		weapon:'Ables Sword',
		swear:'  You are a master warrior!',
		needstr1:'You are truly the BEST warrior in the realm.',
		needstr2:'',
		death:'  You blew your master away!',
		str:1200,
		hp:2500,
		hp_gained:550,
		str_gained:200,
		def:150,
		need:10000000
	}
];

var monster_stats = [
	{
		name:'Small Thief',
		str:6,
		gold:56,
		weapon:'Small Dagger',
		exp:2,
		hp:9,
		death:'You disembowel the little thieving menace!'
	},
	{
		name:'Rude Boy',
		str:3,
		gold:7,
		weapon:'Cudgel',
		exp:3,
		hp:7,
		death:'You quietly watch as the very Rude Boy bleeds to death.'
	},
	{
		name:'Old Man',
		str:5,
		gold:73,
		weapon:'Cane',
		exp:4,
		hp:13,
		death:'You finish him off, by tripping him with his own cane.'
	},
	{
		name:'Large Green Rat',
		str:3,
		gold:32,
		weapon:'Sharp Teeth',
		exp:1,
		hp:4,
		death:'A well placed step ends this small rodents life.'
	},
	{
		name:'Wild Boar',
		str:10,
		gold:58,
		weapon:'Sharp Tusks',
		exp:5,
		hp:9,
		death:'You impale the boar between the eyes!'
	},
	{
		name:'Ugly Old Hag',
		str:6,
		gold:109,
		weapon:'Garlic Breath',
		exp:4,
		hp:9,
		death:'You emotionally crush the hag, by calling her ugly!'
	},
	{
		name:'Large Mosquito',
		str:2,
		gold:46,
		weapon:'Blood Sucker',
		exp:2,
		hp:3,
		death:'With a sharp slap, you end the Mosquitos life.'
	},
	{
		name:'Bran The Warrior',
		str:12,
		gold:234,
		weapon:'Short Sword',
		exp:10,
		hp:15,
		death:'After a hardy duel, Bran lies at your feet, dead.'
	},
	{
		name:'Evil Wretch',
		str:7,
		gold:76,
		weapon:'Finger Nail',
		exp:3,
		hp:12,
		death:'With a swift boot to her head, you kill her.'
	},
	{
		name:'Small Bear',
		str:9,
		gold:154,
		weapon:'Claws',
		exp:6,
		hp:7,
		death:'After a swift battle, you stand holding the Bears heart!'
	},
	{
		name:'Small Troll',
		str:6,
		gold:87,
		weapon:'Uglyness',
		exp:5,
		hp:14,
		death:'This battle reminds you how of how much you hate trolls.'
	},
	{
		name:'Green Python',
		str:13,
		gold:80,
		weapon:'Dripping Fangs',
		exp:6,
		hp:17,
		death:'You tie the mighty snake\'s carcass to a tree.'
	},
	{
		name:'Gath The Barbarian',
		str:12,
		gold:134,
		weapon:'Huge Spiked Club',
		exp:9,
		hp:13,
		death:'You knock Gath down, ignoring his constant groaning.'
	},
	{
		name:'Evil Wood Nymph',
		str:15,
		gold:160,
		weapon:'Flirtatios Behavior',
		exp:11,
		hp:10,
		death:'You shudder to think of what would have happened, had you given in.'
	},
	{
		name:'Fedrick The Limping Baboon',
		str:8,
		gold:97,
		weapon:'Scary Faces',
		exp:6,
		hp:23,
		death:'Fredrick will never grunt in anyones face again.'
	},
	{
		name:'Wild Man',
		str:13,
		gold:134,
		weapon:'Hands',
		exp:8,
		hp:14,
		death:'Pitting your wisdom against his brawn has one this battle.'
	},
	{
		name:'Brorandia The Viking',
		str:21,
		gold:330,
		weapon:'Hugely Spiked Mace',
		exp:20,
		hp:18,
		death:'You consider this a message to her people, "STAY AWAY!".'
	},
	{
		name:'Huge Bald Man',
		str:19,
		gold:311,
		weapon:'Glare From Forehead',
		exp:16,
		hp:19,
		death:'It wasn\'t even a close battle, you slaughtered him.'
	},
	{
		name:'Senile Senior Citizen',
		str:13,
		gold:270,
		weapon:'Crazy Ravings',
		exp:13,
		hp:11,
		death:'You may have just knocked some sense into this old man.'
	},
	{
		name:'Membrain Man',
		str:10,
		gold:190,
		weapon:'Strange Ooze',
		exp:11,
		hp:16,
		death:'The monstrosity has been slain.'
	},
	{
		name:'Bent River Dryad',
		str:12,
		gold:150,
		weapon:'Pouring Waterfall',
		exp:9,
		hp:16,
		death:'You cannot resist thinking the Dryad is "All wet".'
	},
	{
		name:'Rock Man',
		str:8,
		gold:300,
		weapon:'Large Stones',
		exp:12,
		hp:27,
		death:'You have shattered the Rock Mans head!'
	},
	{
		name:'Lazy Bum',
		str:19,
		gold:380,
		weapon:'Unwashed Body Odor',
		exp:18,
		hp:29,
		death:'"This was a bum deal" You think to yourself.'
	},
	{
		name:'Two Headed Rotwieler',
		str:18,
		gold:384,
		weapon:'Twin Barking',
		exp:17,
		hp:32,
		death:'You have silenced the mutt, once and for all.'
	},
	{
		name:'Purple Monchichi',
		str:14,
		gold:763,
		weapon:'Continous Whining',
		exp:23,
		hp:29,
		death:'You cant help but realize you have just killed a real loser.'
	},
	{
		name:'Bone',
		str:27,
		gold:432,
		weapon:'Terrible Smoke Smell',
		exp:16,
		hp:11,
		death:'Now that you have killed Bone, maybe he will get a life..'
	},
	{
		name:'Red Neck',
		str:19,
		gold:563,
		weapon:'Awfull Country Slang',
		exp:19,
		hp:16,
		death:'The dismembered body causes a churning in your stomach.'
	},
	{
		name:'Winged Demon Of Death',
		str:42,
		gold:830,
		weapon:'Red Glare',
		exp:28,
		hp:23,
		death:'You cut off the Demons head, to be sure of its death.'
	},
	{
		name:'Black Owl',
		str:28,
		gold:711,
		weapon:'Hooked Beak',
		exp:26,
		hp:29,
		death:'A well placed blow knocks the winged creature to the ground.'
	},
	{
		name:'Muscled Midget',
		str:26,
		gold:870,
		weapon:'Low Punch',
		exp:32,
		hp:19,
		death:'You laugh as the small man falls to the ground.'
	},
	{
		name:'Headbanger Of The West',
		str:23,
		gold:245,
		weapon:'Ear Shattering Noises',
		exp:43,
		hp:27,
		death:'You slay the rowdy noise maker and destroy his evil machines.'
	},
	{
		name:'Morbid Walker',
		str:28,
		gold:764,
		weapon:'Endless Walking',
		exp:9,
		hp:10,
		death:'Even lying dead on its back, it is still walking.'
	},
	{
		name:'Magical Evil Gnome',
		str:24,
		gold:638,
		weapon:'Spell Of Fire',
		exp:28,
		hp:25,
		death:'The Gnome\'s small body is covered in a deep red blood.'
	},
	{
		name:'Death Dog',
		str:36,
		gold:1150,
		weapon:'Teeth',
		exp:36,
		hp:52,
		death:'You rejoice as the dog wimpers for the very last time.'
	},
	{
		name:'Weak Orc',
		str:27,
		gold:900,
		weapon:'Spiked Club',
		exp:25,
		hp:32,
		death:'A solid blow removes the Orcs head!'
	},
	{
		name:'Dark Elf',
		str:43,
		gold:1070,
		weapon:'Small bow',
		exp:33,
		hp:57,
		death:'The Elf falls at your feet, dead.'
	},
	{
		name:'Evil Hobbit',
		str:35,
		gold:1240,
		weapon:'Smoking Pipe',
		exp:46,
		hp:95,
		death:'The Hobbit will never bother anyone again!'
	},
	{
		name:'Short Goblin',
		str:34,
		gold:768,
		weapon:'Short Sword',
		exp:24,
		hp:45,
		death:'A quick lunge renders him dead!'
	},
	{
		name:'Huge Black Bear',
		str:67,
		gold:1765,
		weapon:'Razor Claws',
		exp:76,
		hp:48,
		death:'You bearly beat the Huge Bear...'
	},
	{
		name:'Rabid Wolf',
		str:45,
		gold:1400,
		weapon:'Deathlock Fangs',
		exp:43,
		hp:39,
		death:'You pull the dogs lifeless body off you.'
	},
	{
		name:'Young Wizard',
		str:64,
		gold:1754,
		weapon:'Weak Magic',
		exp:64,
		hp:35,
		death:'This Wizard will never cast another spell!'
	},
	{
		name:'Mud Man',
		str:56,
		gold:870,
		weapon:'Mud Balls',
		exp:43,
		hp:65,
		death:'You chop up the Mud Man into sushi!'
	},
	{
		name:'Death Jester',
		str:34,
		gold:1343,
		weapon:'Horrible Jokes',
		exp:32,
		hp:46,
		death:'You feel no pity for the Jester, his jokes being as bad as they were.'
	},
	{
		name:'Rock Man',
		str:87,
		gold:1754,
		weapon:'Large Stones',
		exp:76,
		hp:54,
		death:'You have shattered the Rock Mans head!'
	},
	{
		name:'Pandion Knight',
		str:64,
		gold:3100,
		weapon:'Orkos Broadsword',
		exp:98,
		hp:59,
		death:'You are elated in the knowledge that you both fought honorably.'
	},
	{
		name:'Jabba',
		str:61,
		gold:2384,
		weapon:'Whiplashing Tail',
		exp:137,
		hp:198,
		death:'The fat thing falls down, never to squirm again.'
	},
	{
		name:'Manoken Sloth',
		str:54,
		gold:2452,
		weapon:'Dripping Paws',
		exp:97,
		hp:69,
		death:'You have cut him down, spraying a neaby tree with blood.'
	},
	{
		name:'Trojan Warrior',
		str:73,
		gold:3432,
		weapon:'Twin Swords',
		exp:154,
		hp:87,
		death:'You watch, as the ants claim his body.'
	},
	{
		name:'Misfit The Ugly',
		str:75,
		gold:2563,
		weapon:'Strange Ideas',
		exp:120,
		hp:89,
		death:'You cut him cleanly down the middle, in a masterfull stroke.'
	},
	{
		name:'George Of The Jungle',
		str:56,
		gold:2230,
		weapon:'Echoing Screams',
		exp:128,
		hp:43,
		death:'You thought the story of George was a myth, until now.'
	},
	{
		name:'Silent Death',
		str:113,
		gold:4711,
		weapon:'Pale Smoke',
		exp:230,
		hp:98,
		death:'Instead of spilling blood, the creature seems filled with only air.'
	},
	{
		name:'Bald Medusa',
		str:78,
		gold:4000,
		weapon:'Glare Of Stone',
		exp:256,
		hp:120,
		death:'You are lucky you didnt look at her... Man was she ugly!'
	},
	{
		name:'Black Alligator',
		str:65,
		gold:3245,
		weapon:'Extra Sharp Teeth',
		exp:123,
		hp:65,
		death:'With a single stroke, you sever the creatures head right off.'
	},
	{
		name:'Clancy, Son Of Emporor Len',
		str:52,
		gold:4764,
		weapon:'Spiked Bull Whip',
		exp:324,
		hp:324,
		death:'Its a pity so many new warriors get so proud.'
	},
	{
		name:'Black Sorcerer',
		str:86,
		gold:2838,
		weapon:'Spell Of Lightning',
		exp:154,
		hp:25,
		death:'Thats the last spell this Sorcerer will ever cast!'
	},
	{
		name:'Iron Warrior',
		str:100,
		gold:6542,
		weapon:'3 Iron',
		exp:364,
		hp:253,
		death:'You have bent the Iron warrior\'s Iron!'
	},
	{
		name:'Black Soul',
		str:112,
		gold:5865,
		weapon:'Black Candle',
		exp:432,
		hp:432,
		death:'You have released the black soul.'
	},
	{
		name:'Gold Man',
		str:86,
		gold:8964,
		weapon:'Rock Arm',
		exp:493,
		hp:354,
		death:'You kick the body of the Gold man to reveal some change..'
	},
	{
		name:'Screaming Zombie',
		str:98,
		gold:5322,
		weapon:'Gaping Mouth Full Of Teeth',
		exp:354,
		hp:286,
		death:'The battle has rendered the zombie even more unattractive then he was.'
	},
	{
		name:'Satans Helper',
		str:112,
		gold:7543,
		weapon:'Pack Of Lies',
		exp:453,
		hp:165,
		death:'Apparently you have seen through the Devil\'s evil tricks'
	},
	{
		name:'Wild Stallion',
		str:78,
		gold:4643,
		weapon:'Hoofs',
		exp:532,
		hp:245,
		death:'You only wish you could have spared the animal\'s life.'
	},
	{
		name:'Belar',
		str:120,
		gold:9432,
		weapon:'Fists Of Rage',
		exp:565,
		hp:352,
		death:'Not even Belar can stop you!'
	},
	{
		name:'Empty Armour',
		str:67,
		gold:6431,
		weapon:'Cutting Wind',
		exp:432,
		hp:390,
		death:'The whole battle leaves you with a strange chill.'
	},
	{
		name:'Raging Lion',
		str:98,
		gold:3643,
		weapon:'Teeth And Claws',
		exp:365,
		hp:274,
		death:'You rip the jaw bone off the magnificent animal!'
	},
	{
		name:'Huge Stone Warrior',
		str:112,
		gold:4942,
		weapon:'Rock Fist',
		exp:543,
		hp:232,
		death:'There is nothing left of the stone warrior, except a few pebbles.'
	},
	{
		name:'Magical Evil Gnome',
		str:89,
		gold:6384,
		weapon:'Spell Of Fire',
		exp:321,
		hp:234,
		death:'The Gnome\'s small body is covered in a deep, red blood.'
	},
	{
		name:'Emporer Len',
		str:210,
		gold:12043,
		weapon:'Lightning Bull Whip',
		exp:764,
		hp:432,
		death:'His last words were.. "I have failed to avenge my son."'
	},
	{
		name:'Night Hawk',
		str:220,
		gold:10433,
		weapon:'Blood Red Talons',
		exp:686,
		hp:675,
		death:'Your last swing pulls the bird out of the air, landing him at your feet.'
	},
	{
		name:'Charging Rhinoceros',
		str:187,
		gold:9853,
		weapon:'Rather Large Horn',
		exp:654,
		hp:454,
		death:'You finally felled the huge beast, but not without a few scratches.'
	},
	{
		name:'Goblin Pygmy',
		str:165,
		gold:13252,
		weapon:'Death Squeeze',
		exp:754,
		hp:576,
		death:'You laugh at the little Goblin\'s puny attack.'
	},
	{
		name:'Goliath',
		str:243,
		gold:14322,
		weapon:'Six Fingered Fist',
		exp:898,
		hp:343,
		death:'Now you know how David felt...'
	},
	{
		name:'Angry Liontaur',
		str:187,
		gold:13259,
		weapon:'Arms And Teeth',
		exp:753,
		hp:495,
		death:'You have laid this mythical beast to rest.'
	},
	{
		name:'Fallen Angel',
		str:154,
		gold:12339,
		weapon:'Throwing Halos',
		exp:483,
		hp:654,
		death:'You slay the Angel, then watch as it gets sucked down into the ground.'
	},
	{
		name:'Wicked Wombat',
		str:198,
		gold:13283,
		weapon:'The Dark Wombats Curse',
		exp:786,
		hp:464,
		death:'It\'s hard to believe a little wombat like that could be so much trouble.'
	},
	{
		name:'Massive Dinosaur',
		str:200,
		gold:16753,
		weapon:'Gaping Jaws',
		exp:1204,
		hp:986,
		death:'The earth shakes as the huge beast falls to the ground.'
	},
	{
		name:'Swiss Butcher',
		str:230,
		gold:8363,
		weapon:'Meat Cleaver',
		exp:532,
		hp:453,
		death:'You\'re glad you won...You really didn\'t want the haircut..'
	},
	{
		name:'Death Gnome',
		str:270,
		gold:10000,
		weapon:'Touch Of Death',
		exp:654,
		hp:232,
		death:'You watch as the animals pick away at his flesh.'
	},
	{
		name:'Screeching Witch',
		str:300,
		gold:19753,
		weapon:'Spell Of Ice',
		exp:2283,
		hp:674,
		death:'You have silenced the witch\'s infernal screeching.'
	},
	{
		name:'Rundorig',
		str:330,
		gold:17853,
		weapon:'Poison Claws',
		exp:2748,
		hp:675,
		death:'Rundorig, once your friend, now lies dead before you.'
	},
	{
		name:'Wheeler',
		str:250,
		gold:23433,
		weapon:'Annoying Laugh',
		exp:1980,
		hp:786,
		death:'You rip the wheeler\'s wheels clean off!'
	},
	{
		name:'Death Knight',
		str:287,
		gold:21923,
		weapon:'Huge Silver Sword',
		exp:4282,
		hp:674,
		death:'The Death knight finally falls, not only wounded, but dead.'
	},
	{
		name:'Werewolf',
		str:230,
		gold:19474,
		weapon:'Fangs',
		exp:3853,
		hp:543,
		death:'You have slaughtered the Werewolf. You didn\'t even need a silver bullet.'
	},
	{
		name:'Fire Ork',
		str:267,
		gold:24933,
		weapon:'FireBall',
		exp:3942,
		hp:674,
		death:'You have put out this Fire Ork\'s flame!'
	},
	{
		name:'Wans Beast',
		str:193,
		gold:17141,
		weapon:'Crushing Embrace',
		exp:2432,
		hp:1243,
		death:'The hairy thing has finally stopped moving.'
	},
	{
		name:'Lord Mathese',
		str:245,
		gold:24935,
		weapon:'Fencing Sword',
		exp:2422,
		hp:875,
		death:'You have wiped the sneer off his face once and for all.'
	},
	{
		name:'King Vidion',
		str:400,
		gold:28575,
		weapon:'Long Sword Of Death',
		exp:6764,
		hp:1243,
		death:'You feel lucky to have lived. Things could have gone sour..'
	},
	{
		name:'Baby Dragon',
		str:176,
		gold:25863,
		weapon:'Dragon Smoke',
		exp:3675,
		hp:2322,
		death:'This Baby Dragon will never grow up.'
	},
	{
		name:'Death Gnome',
		str:356,
		gold:31638,
		weapon:'Touch Of Death',
		exp:2300,
		hp:870,
		death:'You watch as the animals pick away at his flesh.'
	},
	{
		name:'Pink Elephant',
		str:434,
		gold:33844,
		weapon:'Stomping',
		exp:7843,
		hp:1232,
		death:'You have witnessed the Pink Elephant...And you aren\'t even drunk!'
	},
	{
		name:'Gwendolens Nightmare',
		str:490,
		gold:35846,
		weapon:'Dreams',
		exp:8232,
		hp:764,
		death:'This is the first Nightmare you have put to sleep.'
	},
	{
		name:'Flying Cobra',
		str:400,
		gold:37694,
		weapon:'Poison Fangs',
		exp:8433,
		hp:1123,
		death:'The creature falls to the ground with a sickening thud.'
	},
	{
		name:'Rentakis Pet',
		str:556,
		gold:37584,
		weapon:'Gaping Maw',
		exp:9854,
		hp:987,
		death:'You vow to find Rentaki and tell him what you think about his new pet.'
	},
	{
		name:'Ernest Brown',
		str:432,
		gold:34833,
		weapon:'Knee',
		exp:9754,
		hp:2488,
		death:'Ernest has finally learned his lesson, it seems.'
	},
	{
		name:'Scallian Rap',
		str:601,
		gold:22430,
		weapon:'Way Of Hurting People',
		exp:6784,
		hp:788,
		death:'Scallian\'s dead...Looks like you took out the trash...'
	},
	{
		name:'Apeman',
		str:498,
		gold:38955,
		weapon:'Hairy Hands',
		exp:7202,
		hp:1283,
		death:'The battle is over...Nothing is left but blood and hair.'
	},
	{
		name:'Hemo-Glob',
		str:212,
		gold:27853,
		weapon:'Weak Insults',
		exp:4432,
		hp:1232,
		death:'The battle is over.. And you really didn\'t find him particularly scary.'
	},
	{
		name:'FrankenMoose',
		str:455,
		gold:31221,
		weapon:'Butting Head',
		exp:5433,
		hp:1221,
		death:'That Moose was a perversion of nature!'
	},
	{
		name:'Earth Shaker',
		str:767,
		gold:37565,
		weapon:'Earthquake',
		exp:7432,
		hp:985,
		death:'The battle is over...And it looks like you shook him up...'
	},
	{
		name:'Gollums Wrath',
		str:621,
		gold:42533,
		weapon:'Ring Of Invisibilty',
		exp:13544,
		hp:2344,
		death:'Gollum\'s ring apparently wasn\'t powerful enough.'
	},
	{
		name:'Toraks Son, Korak',
		str:921,
		gold:46575,
		weapon:'Sword Of Lightning',
		exp:13877,
		hp:1384,
		death:'You have slain the son of a God!  You ARE great!'
	},
	{
		name:'Brand The Wanderer',
		str:643,
		gold:38755,
		weapon:'Fighting Quarter Staff',
		exp:13744,
		hp:2788,
		death:'Brand will wander no more.'
	},
	{
		name:'The Grimest Reaper',
		str:878,
		gold:39844,
		weapon:'White Sickle',
		exp:14237,
		hp:1674,
		death:'You have killed that which was already dead.  Odd.'
	},
	{
		name:'Death Dealer',
		str:765,
		gold:47333,
		weapon:'Stare Of Paralization',
		exp:13877,
		hp:1764,
		death:'The Death Dealer has been has been dealt his last hand.'
	},
	{
		name:'Tiger Of The Deep Jungle',
		str:587,
		gold:43933,
		weapon:'Eye Of The Tiger',
		exp:9766,
		hp:3101,
		death:'The Tiger\'s cubs weep over their dead mother.'
	},
	{
		name:'Sweet Looking Little Girl',
		str:989,
		gold:52322,
		weapon:'Demon Strike',
		exp:14534,
		hp:1232,
		death:'If it wasn\'t for her manners, you might have gotten along with her.'
	},
	{
		name:'Floating Evil Eye',
		str:776,
		gold:43233,
		weapon:'Evil Stare',
		exp:13455,
		hp:2232,
		death:'You really didn\'t like the look of that Eye...'
	},
	{
		name:'Slock',
		str:744,
		gold:56444,
		weapon:'Swamp Slime',
		exp:14333,
		hp:1675,
		death:'Walking away fromm the battle, you nearly slip on the thing\'s slime.'
	},
	{
		name:'Adult Gold Dragon',
		str:565,
		gold:56444,
		weapon:'Dragon Fire',
		exp:15364,
		hp:3222,
		death:'He was strong, but you were stronger.'
	},
	{
		name:'Black Sorcerer',
		str:86,
		gold:2838,
		weapon:'Spell Of Lightning',
		exp:187,
		hp:25,
		death:'That\'s the last spell this Sorcerer will ever cast!'
	},
	{
		name:'Kill Joy',
		str:988,
		gold:168844,
		weapon:'Terrible Stench',
		exp:25766,
		hp:3222,
		death:'Kill Joy has fallen, and can\'t get up.'
	},
	{
		name:'Gorma The Leper',
		str:1132,
		gold:168774,
		weapon:'Contagous Desease',
		exp:26333,
		hp:2766,
		death:'It looks like the lepers fighting strategy has fallen apart..'
	},
	{
		name:'Shogun Warrior',
		str:1143,
		gold:165433,
		weapon:'Japenese Nortaki',
		exp:26555,
		hp:3878,
		death:'He was tough, but not nearly tough enough.'
	},
	{
		name:'Apparently Weak Old Woman',
		str:1543,
		gold:173522,
		weapon:'*GODS HAMMER*',
		exp:37762,
		hp:1878,
		death:'You pull back the old woman\'s hood, to reveal an eyeless skull.'
	},
	{
		name:'Ables Creature',
		str:985,
		gold:176775,
		weapon:'Bear Hug',
		exp:28222,
		hp:2455,
		death:'That was a mighty creature.  Created by a mighty man.'
	},
	{
		name:'White Bear Of Lore',
		str:1344,
		gold:65544,
		weapon:'Snow Of Death',
		exp:16775,
		hp:1875,
		death:'The White Bear Of Lore DOES exist you\'ve found.  Too bad it\'s now dead.'
	},
	{
		name:'Mountain',
		str:1544,
		gold:186454,
		weapon:'Landslide',
		exp:38774,
		hp:1284,
		death:'You have knocked the mountain to the ground.  Now it IS the ground.'
	},
	{
		name:'Sheena The Shapechanger',
		str:1463,
		gold:165755,
		weapon:'Deadly Illusions',
		exp:26655,
		hp:1898,
		death:'Sheena is now a quivering mass of flesh.  Her last shapechange.'
	},
	{
		name:'ShadowStormWarrior',
		str:1655,
		gold:162445,
		weapon:'Mystical Storm',
		exp:26181,
		hp:2767,
		death:'The storm is over, and the sunshine greets you as the victor.'
	},
	{
		name:'Madman',
		str:1265,
		gold:149564,
		weapon:'Chant Of Insanity',
		exp:25665,
		hp:1764,
		death:'Madman must have been mad to think he could beat you!'
	},
	{
		name:'Vegetable Creature',
		str:111,
		gold:4838,
		weapon:'Pickled Cabbage',
		exp:2187,
		hp:172,
		death:'For once you finished off your greens...'
	},
	{
		name:'Cyclops Warrior',
		str:1744,
		gold:204000,
		weapon:'Fire Eye',
		exp:49299,
		hp:2899,
		death:'The dead Cyclop\'s one eye stares at you blankly.'
	},
	{
		name:'Corinthian Giant',
		str:2400,
		gold:336643,
		weapon:'De-rooted Tree',
		exp:60333,
		hp:2544,
		death:'You hope the giant has brothers, more sport for you.'
	},
	{
		name:'The Screaming Eunich',
		str:1488,
		gold:197888,
		weapon:'High Pitched Voice',
		exp:78884,
		hp:2877,
		death:'If it wasn\'t for his ugly features, you thought he looked female.'
	},
	{
		name:'Black Warlock',
		str:1366,
		gold:168483,
		weapon:'Satanic Choruses',
		exp:58989,
		hp:2767,
		death:'You have slain Satan\'s only son.'
	},
	{
		name:'Kal Torak',
		str:876,
		gold:447774,
		weapon:'Cthrek Goru',
		exp:94663,
		hp:6666,
		death:'You have slain a God!  You are the ultimate warrior!'
	},
	{
		name:'The Mighty Shadow',
		str:1633,
		gold:176333,
		weapon:'Shadow Axe',
		exp:51655,
		hp:2332,
		death:'The mighty Shadow is now only a Shadow of his former self.'
	},
	{
		name:'Black Unicorn',
		str:1899,
		gold:336693,
		weapon:'Shredding Horn',
		exp:41738,
		hp:1587,
		death:'You have felled the Unicorn, not the first, not the last.'
	},
	{
		name:'Mutated Black Widow',
		str:2575,
		gold:434370,
		weapon:'Venom Bite',
		exp:98993,
		hp:1276,
		death:'A well placed stomp ends this Spider\'s life.'
	},
	{
		name:'Humongous Black Wyre',
		str:1166,
		gold:653834,
		weapon:'Death Talons',
		exp:76000,
		hp:3453,
		death:'The Wyre\'s dead carcass covers the whole field!'
	},
	{
		name:'The Wizard Of Darkness',
		str:1497,
		gold:224964,
		weapon:'Chant Of Insanity',
		exp:39878,
		hp:1383,
		death:'This Wizard of Darkness will never bother you again'
	},
	{
		name:'Great Ogre Of The North',
		str:1800,
		gold:524838,
		weapon:'Spiked Steel Mace',
		exp:112833,
		hp:2878,
		death:'No one is going to call him The "Great" Ogre Of The North again.'
	}
];

var armour_stats = [
	{name:'Nothing!', price:0, num:0},
	{name:'Coat', price:200, num:1},
	{name:'Heavy Coat', price:1000, num:3},
	{name:'Leather Vest', price:3000, num:10},
	{name:'Bronze Armour', price:10000, num:15},
	{name:'Iron Armour', price:30000, num:25},
	{name:'Graphite Armour', price:100000, num:35},
	{name:'Erdricks Armour', price:150000, num:50},
	{name:'Armour Of Death', price:200000, num:75},
	{name:'Able\'s Armour', price:400000, num:100},
	{name:'Full Body Armour', price:1000000, num:150},
	{name:'Blood Armour', price:4000000, num:225},
	{name:'Magic Protection', price:10000000, num:300},
	{name:'Belars\'s Mail', price:40000000, num:400},
	{name:'Golden Armour', price:100000000, num:600},
	{name:'Armour Of Lore', price:400000000, num:1000}
];

var weapon_stats = [
	{name:'Fists', price:0, num:0},
	{name:'Stick', price:200, num:5},
	{name:'Dagger', price:1000, num:10},
	{name:'Short Sword', price:3000, num:20},
	{name:'Long Sword', price:10000, num:30},
	{name:'Huge Axe', price:30000, num:40},
	{name:'Bone Cruncher', price:100000, num:60},
	{name:'Twin Swords', price:150000, num:80},
	{name:'Power Axe', price:200000, num:120},
	{name:'Able\'s Sword', price:400000, num:180},
	{name:'Wans\' Weapon', price:1000000, num:250},
	{name:'Spear of Gold', price:4000000, num:350},
	{name:'Crystal Shard', price:10000000, num:500},
	{name:'Nira\'s Teeth', price:40000000, num:800},
	{name:'Blood Sword', price:100000000, num:1200},
	{name:'Death Sword', price:400000000, num:1800}
];

var castles = [
	'Dumbass World',
	'Castle Coldrake',
	'Fortress Liddux',
	'Gannon Keep',
	'Penyon Manor',
	'Dema\'s Lair',
	'Why are you snooping? Go away!'
];

function beef_up(m)
{
	var drk;
	var max;
	var adj;

	if (settings.beef_up) {
		if (player.drag_kills > 0 && random(5) < 2) {
			if (player.drag_kills < 6) {
				drk = player.drag_kills;
			}
			else {
				drk = 5;
			}
			max = 2000000000 - m.hp;
			adj = random(((drk * (random(10)+random(10))) * m.hp) / 100);
			if (adj < max) {
				m.hp += adj;
			}
			max = 2000000000 - m.str;
			adj = random(((drk * (random(10)+random(10))) * m.str) / 100);
			if (adj < max) {
				m.str += adj;
			}
		}
	}
}

function psock_put(leave_locked)
{
	var json;

	json = JSON.stringify(this, whitelist);
	psock.write('PutPlayer '+this.Record+' '+json.length+'\r\n'+json+'\r\n');
	if (psock.readln() !== 'OK') {
		throw('Out of sync with server after PutPlayer');
	}
}

function psock_unLock()
{
	// TODO: Stub
}

function gamedir(fname)
{
	var gpre;

	if (settings.game_prefix.length > 0) {
		if (settings.game_prefix[0] === '/' || settings.game_prefix[0] === '\\'
		    || (settings.game_prefix[1] === ':' && (settings.game_prefix[2] === '\\' || settings.game_prefix[2] === '/'))) {
			gpre = settings.game_prefix;
		}
		else {
			gpre = js.exec_dir + settings.game_prefix;
		}
	}
	else {
		gpre = js.exec_dir;
	}
	if (file_isdir(gpre)) {
		gpre = backslash(gpre);
	}
	return (gpre + fname);
}

function game_or_exec(fname)
{
	var gpre;

	if (settings.menu_dir.length > 0) {
		if (settings.menu_dir[0] === '/' || settings.menu_dir[0] === '\\'
		    || (settings.menu_dir[1] === ':' && (settings.menu_dir[2] === '\\' || settings.menu_dir[2] === '/'))) {
			gpre = settings.menu_dir;
		}
		else {
			gpre = js.exec_dir + settings.menu_dir;
		}
		if (!file_isdir(gpre)) {
			gpre = undefined;
		}
	}
	if (gpre === undefined) {
		if (settings.game_prefix.length > 0) {
			if (settings.game_prefix[0] === '/' || settings.game_prefix[0] === '\\'
			    || (settings.game_prefix[1] === ':' && (settings.game_prefix[2] === '\\' || settings.game_prefix[2] === '/'))) {
				gpre = settings.game_prefix;
			}
			else {
				gpre = js.exec_dir + settings.game_prefix;
			}
		}
		else {
			gpre = js.exec_dir;
		}
	}
	if (file_isdir(gpre)) {
		gpre = backslash(gpre);
	}
	if (file_exists(gpre + fname)) {
		return (gpre + fname);
	}
	return js.exec_dir + fname;
}

function psock_reInit(leavelocked)
{
	Player_Def.forEach(function(o) {
		this[o.prop]=eval(o.def.toSource()).valueOf();
	}, this);
}

function pfile_init()
{
	var m;

	if (pfile === undefined && psock === undefined) {
		if (settings.remote_game !== undefined && settings.remote_game.length > 0) {
			m = settings.remote_game.match(/^(.*)\.([0-9]+)$/);
			if (m === null) {
				throw('Unable to parse remote address: '+settings.remote_game);
			}
			psock = new ConnectedSocket(m[1], m[2]);
			psock.ssl_session = true;
			psock.nonblocking = false;
			if (psock === undefined) {
				throw('Unable to connect to remote server: '+settings.remote_game);
			}
			psock.writeln('Auth '+settings.game_user+' '+settings.game_pass);
			if (psock.readln() !== 'OK') {
				throw('Unable to authenticate!');
			}
			Player_Def.forEach(function(o) {
				whitelist.push(o.prop);
			});
			// TODO: Can't win a remote game.
			settings.win_deeds = 0;
			tournament_settings.enabled = false;
		}
		else {
			pfile = new RecordFile(gamedir('player.bin'), Player_Def);
		}
	}
}

function psock_reLoad(leave_locked)
{
	return player_get(this.Record);
}

function player_new(leave_locked)
{
	var ret;
	var m;
	var resp;
	var len;
	var json;

	if (pfile === undefined && psock === undefined) {
		pfile_init();
	}
	if (pfile !== undefined) {
		ret = pfile.new(leave_locked);
		if (ret !== null) {
			ret.Yours = true;
		}
	}
	else if(psock !== undefined) {
		psock.writeln('NewPlayer');
		resp = psock.readln();
		m = resp.match(/^PlayerRecord ([0-9]+)$/);
		if (m === null) {
			throw('Invalid NewPlayer server response: '+resp);
		}
		len = parseInt(m[1], 10);
		json = psock.read(len);
		if (json.length !== len) {
			throw('Short read of '+json.length+' bytes');
		}
		if (psock.read(2) !== '\r\n') {
			throw('No terminating newline after NewPlayer response');
		}
		ret = JSON.parse(json);
		ret.put = psock_put;
		ret.unLock = psock_unLock;
		ret.reInit = psock_reInit;
		ret.reLoad = psock_reLoad;
	}
	return ret;
}

function player_get(rec, leave_locked)
{
	var m;
	var ret;
	var json;
	var resp;
	var len;

	if (pfile === undefined && psock === undefined) {
		pfile_init();
	}
	if (pfile !== undefined) {
		ret = pfile.get(rec, leave_locked);
		if (ret !== null) {
			ret.Yours = true;
		}
	}
	else if(psock !== undefined) {
		psock.writeln('GetPlayer '+rec);
		resp = psock.readln();
		m = resp.match(/^PlayerRecord ([0-9]+)$/);
		if (m === null) {
			throw('Invalid server response to GetPlayer: '+resp);
		}
		len = parseInt(m[1], 10);
		json = psock.read(len);
		if (json.length !== len) {
			throw('Short read of '+json.length+' bytes');
		}
		if (psock.read(2) !== '\r\n') {
			throw('No terminating newline after GetPlayer response');
		}
		ret = JSON.parse(json);
		ret.put = psock_put;
		ret.unLock = psock_unLock;
		ret.reInit = psock_reInit;
		ret.reLoad = psock_reLoad;
	}
	return ret;
}

function player_length()
{
	var ret;

	if (pfile === undefined && psock === undefined) {
		pfile_init();
	}
	if (pfile !== undefined) {
		return pfile.length;
	}
	if(psock !== undefined) {
		psock.writeln('RecordCount');
		ret = psock.readln();
		if (ret === undefined || ret.search(/^[0-9]+$/) !== 0) {
			throw('Invalid response from server');
		}
		return parseInt(ret, 10);
	}
}

function all_players()
{
	var len = player_length();
	var ret = [];
	var i;
	var p;
	var req;
	var resp;
	var m;
	var json;
	var jslen;

	if (pfile !== undefined) {
		for (i=0; i<len; i += 1) {
			p = pfile.get(i);
			ret.Yours = true;
			ret.push(p);
		}
	}
	else {
		req = '';
		for (i=0; i<len; i += 1) {
			req += 'GetPlayer '+i+'\r\n';
		}
		psock.write(req);
		for (i=0; i<len; i += 1) {
			resp = psock.readln();
			m = resp.match(/^PlayerRecord ([0-9]+)$/);
			if (m === null) {
				throw('Invalid server response to GetPlayer: '+resp);
			}
			jslen = parseInt(m[1], 10);
			json = psock.read(jslen);
			if (json.length !== jslen) {
				throw('Short read of '+json.length+' bytes');
			}
			if (psock.read(2) !== '\r\n') {
				throw('No terminating newline after GetPlayer response');
			}
			p = JSON.parse(json);
			p.put = psock_put;
			p.unLock = psock_unLock;
			p.reInit = psock_reInit;
			p.reLoad = psock_reLoad;
			ret.push(p);
		}
	}
	return ret;
}

function try_fmutex(fname, str)
{
	fname += '.lock';
	if (str === undefined) {
		if(!file_mutex(fname)) {
			return false;
		}
	}
	else {
		if(!file_mutex(fname, str)) {
			return false;
		}
	}
	cleanup_files.push(fname);
	return true;
}

function fmutex(fname, str)
{
	var end = time() + 15;
	fname += '.lock';
	if (str === undefined) {
		while(!file_mutex(fname)) {
			if (time() > end)
				throw("Unable to create "+fname+" please notify Sysop");
			mswait(1);
		}
	}
	else {
		while(!file_mutex(fname, str)) {
			if (time() > end)
				throw("Unable to create "+fname+" please notify Sysop");
			mswait(1);
		}
	}
	cleanup_files.push(fname);
	return true;
}

function rfmutex(fname)
{
	var idx;

	fname += '.lock';
	idx = cleanup_files.indexOf(fname);
	if (idx === -1) {
		throw('Removing unknown fmutex '+fname);
	}
	file_remove(fname);
	cleanup_files.splice(idx, 1);
}

function get_armourweap(num, stats, fname)
{
	var fn = game_or_exec(fname);
	var f;
	var ret;

	if (num < 1) {
		return (stats[0]);
	}
	if (num > 15) {
		num = 15;
	}

	if (!file_exists(fn)) {
		return stats[num];
	}
	f = new RecordFile(fn, Weapon_Armor_Def);
	if (f.length < num) {
		f.file.close();
		return stats[num];
	}
	ret = f.get(num - 1);
	f.file.close();
	ret.is_arena = true;
	return ret;
}

function get_trainer(num)
{
	var fn = game_or_exec('trainers.bin');
	var f;
	var ret;

	if (num < 1) {
		return (trainer_stats[1]);
	}
	if (num >= trainer_stats.length) {
		num = trainer_stats.length - 1;
	}

	if (!file_exists(fn)) {
		return trainer_stats[num];
	}
	f = new RecordFile(fn, Trainer_Def);
	if (f.length < num) {
		f.file.close();
		return trainer_stats[num];
	}
	ret = f.get(num - 1);
	f.file.close();
	ret.is_arena = true;
	return ret;
}

function get_armour(num)
{
	return get_armourweap(num, armour_stats, 'armour.bin');
}

function get_weapon(num)
{
	return get_armourweap(num, weapon_stats, 'weapon.bin');
}

// Default settings here.
var settings = {
	delete_days:15,		// Inactive players are deleted after this many days
	del_1xp:true,		// Delete players with 1 xp daily
	clean_mode:false,	// CLEAN MODE
	win_deeds:3,		// Heroic deeds to win
	transfers_on:true,
	transfers_per_day:2,
	transfer_amount:2000000,
	pvp_fights_per_day:3,
	forest_fights:15,	// Forest fights per day
	res_days:3,		// Number of days to ressurect inactive players for before leaving them dead until they return
	notime:'',		// Out of time filename
	safe_node:true,		// Creates node.xx files and if it exists, marks the player as off.
	use_fancy_more:true,	// If false, does not erase the MORE prompt.
	nochat:false,		// If true, NPCs don't chat in the inn.
	olivia:true,		// If true, creepy event (olivia) occurs
	funky_flowers:true,	// Allow `rX and `Bx codes in flowers (only remove ``)
	shop_limit:true,	// Don't restrict weapons and armour by base def/str
	old_skill_points:false,	// Require 5 skill points for use points for Death Knight and Thief
	def_for_pk:false,	// Get bonus defence for killing a player.
	str_for_pk:false,	// Get bonus strength for killing player. (You can still get the message that you did though)
	remote_game:'',		// address.port of remote server
	game_prefix:'',		// Directory/file prefix for game data files.
	game_user:(js.global.system !== undefined ? system.qwk_id : ''),
	game_pass:'',
	menu_dir:'',
	beef_up:false,		// Beef up monsters after you kill the dragon
	old_steal:true,
	sleep_dragon:false,	// Anti-camping dragon attacks.
	dk_boost:false,		// Make death knight attack multiple 3.3 instead of 3.
	new_ugly_stick:false,	// Fair ugly stick means the new one, old one was always 5 pretty/1 ugly
	no_lenemy_bin:false,	// Never use the lenemy file
	no_igms_allowed:false	// Never allow any IGMs
};

// Maps settings keys to ini file keys
var settingsmap = {
	delete_days:'InactivityDays',
	del_1xp:'Delete1XP',
	clean_mode:'CleanMode',
	win_deeds:'DeedsToWin',
	transfers_on:'BankTransfersAllowed',
	transfers_per_day:'BankTransfersPerDay',
	transfer_amount:'MaximumTransferAmount',
	pvp_fights_per_day:'PlayerFightsPerDay',
	forest_fights:'ForestFightsPerDay',
	res_days:'RessurectionDays',
	notime:'OutOfTimeFile',
	safe_node:'SafeNode',
	use_fancy_more:'UseFancyMore',
	nochat:'NoNPCChatter',
	olivia:'Olivia',
	funky_flowers:'FunkyFlowers',
	shop_limit:'ShopLimit',
	old_skill_points:'FiveSkillPoints',
	def_for_pk:'BonusDefForPlayerKill',
	str_for_pk:'BonusStrForPlayerKill',
	remote_game:'GameServer',
	game_prefix:'GamePrefix',
	game_user:'GameUser',
	game_pass:'GamePassword',
	menu_dir:'MenuDir',
	beef_up:'BeefUpForHeroes',
	old_steal:'OldStealAmounts',
	dk_boost:'DeathKnightBoost',
	sleep_dragon:'WakeUpDragon',
	new_ugly_stick:'NewUglyStick',
	no_lenemy_bin:'NoLEnemy',
	no_igms_allowed:'NoIGMs'
};

var tournament_settings = {
	enabled:false,
	days:0,		// Set to zero for stat mode, non-zero for days mode
	winstat:0,	// Enum xp, dkills, pkills, level, lays
	xp:0,
	dkills:0,
	pkills:0,
	level:0,
	lays:0
};

var tournament_settings_map = {
	enabled:'Enabled',
	days:'Days',
	// Don't map winstat, we need our own enum thing here instead.
	xp:'Experience',
	dkills:'DragonKills',
	pkills:'PlayerKills',
	level:'Level',
	lays:'Lays'
};

// Outputs text no CRLF, not ` parsing.
function sw(str)
{
	if (str === '') {
		return;
	}
	if (str === '\r\n') {
		curcolnum = 1;
	}
	dk.console.print(str);
	if (str !== '\r\n') {
		curcolnum += str.length;
	}
}

function display_file(fname, canskip, more)
{
	var f = new File(fname);
	var lc = 0;
	var ch;
	var ln;
	var ms = morechk;

	if (canskip === undefined) {
		canskip = true;

	}
	if (more === undefined) {
		more = true;
	}

	if (!f.open('r')) {
		throw('Unable to open '+fname);
	}

	morechk = false;
read_loop:
	while (true) {
		ln = f.readln();
		if (ln === null) {
			break;
		}
		lc += 1;
		if (more && (lc % (dk.console.rows - 1) === 0)) {
			more_nomail();
		}
		lln(ln, true);
		if (canskip) {
			do {
				ch = dk.console.getkey();
				if (ch === '\x03' || ch === ' ') {
					sln('');
					break read_loop;
				}
			} while (ch !== undefined);
		}
	}
	f.close();
	morechk = ms;
}

function getkeyw()
{
	var timeout = time() + 60 * 5;
	var tl;
	var now;

	do {
		now = time();
		// TODO: dk.console.getstr() doesn't support this stuff... (yet)
		tl = (dk.user.seconds_remaining + dk.user.seconds_remaining_from - 30) - now;
		if (tl < 1) {
			if (settings.notime.length > 0) {
				display_file(settings.notime, false, true);
			}
			else {
				sln('(*** GOD STRIKES YOU UNCONSCIOUS (OUT OF BBS TIME!) ***)');
			}
			player.on_now = false;
			player.put();
			exit(0);
		}
		if (now >= timeout) {
			sln('');
			sln('');
			sln('  Ack!  Apperently you didn\'t find this door very exciting, because');
			sln('  you have obviously fallen asleep.  Please come back sometime when');
			sln('  you feel like actually playing.');
			sln('');
			if (player !== undefined) {
				player.on_now = false;
				player.put();
			}
			exit(0);
		}
	} while(!dk.console.waitkey(1000));
	return dk.console.getkey();
}

// Reads a key with translation...
function getkey()
{
	var ch;
	var a;

	curlinenum = 1;
	ch = '\x00';
	do {
		ch = getkeyw();
		if (ch === null || ch.length !== 1) {
			ch = '\x00';
		}
		a = ascii(ch);
		if (a >= 1 && a <= 7) {
			ch = '.';
		}
		if (a >= 9 && a <= 12) {
			ch = '.';
		}
		if (a >= 14 && a <= 26) {
			ch = '.';
		}
		if (a >= 127) {
			ch = '.';
		}
	} while (ch === '\x00');

	return ch;
}

// Outputs a single line without parsing ` codes.
function more_nomail()
{
	var oa = dk.console.attr.value;

	curlinenum = 1;
	lw('  `2<`0MORE`2>');
	getkey();
	if (!dk.user.ansi_supported || settings.use_fancy_more === false) {
		sln('');
		curlinenum = 1;
	}
	else {
		dk.console.print('\r');
		dk.console.cleareol();
	}
	dk.console.attr.value = oa;
}

function sln(str)
{
	// *ALL* CRLFs should be sent from here!
	if (str !== '') {
		sw(str);
	}
	sw('\r\n');
	curlinenum += 1;
	if (morechk) {
		if (curlinenum > dk.console.rows - 1) {
			more_nomail();
			curlinenum = 1;
		}
	}
	else {
		curlinenum = 1;
	}
}

function sclrscr()
{
	var oa = dk.console.attr.value;

	dk.console.clear();
	dk.console.attr.value = oa;
	curlinenum = 1;
}

function foreground(col)
{
	if (col > 15) {
		col = 0x80 | (col & 0x0f);
	}
	dk.console.attr.value = (dk.console.attr.value & 0x70) | col;
}

function background(col)
{
	if (col > 7 || col < 0) {
		return;
	}
	dk.console.attr.value = (dk.console.attr.value & 0x8f) | (col << 4);
}

function pretty_int(int)
{
	var ret = parseInt(int, 10).toString();
	var i;

	for (i = ret.length - 3; i > 0; i-= 3) {
		ret = ret.substr(0, i)+','+ret.substr(i);
	}
	return ret;
}

function looks(sex, charm)
{
	var i;
	var ln;
	var f = new File(game_or_exec(sex === 'M' ? 'mlooks.txt' : 'flooks.txt'));

	if (!f.open('r')) {
		throw('Unable to open '+f.name);
	}
	if (charm === undefined || charm < 1) {
		charm = 1;
	}
	if (charm > 100) {
		charm = 101;
	}
	for (i = 0; i < charm; i += 1) {
		ln = f.readln();
	}
	f.close();
	return ln;
}

// Outputs a line with ` codes
function lln(str, ext)
{
	lw(str, ext);
	sln('');
}

function show_lord_file(fname, quote, mail)
{
	var f = new File(fname);
	var qf = new File(gamedir('quote'+player.Record+'.lrd'));
	var ln = '';
	var lc = 0;
	var ns = false;
	var ch;
	var ms = morechk;

	if (!f.open("r")) {
		return;
	}
	morechk = false;
	while (true) {
		ln = f.readln();
		if (ln === null) {
			break;
		}
		if (quote) {
			if (!qf.open('a')) {
				throw('Unable to open '+qf.name);
			}
			qf.writeln(ln);
			qf.close();
		}
		lln(ln, mail);
		lc += 1;
		if (ns === false && lc >= dk.console.rows - 1) {
			lw(' `2(`5C`2)ontinue, (`5S`2)top, (`5N`2)onstop `0: `%');
			ch = getkey().toUpperCase();
			lc = 0;
			if (ch === 'S') {
				dk.console.print('\r');
				dk.console.cleareol();
				break;
			}
			if (ch === 'N') {
				ns = true;
			}
			dk.console.print('\r');
			dk.console.cleareol();
		}
	}
	morechk = ms;
	f.close();
}

function show_lord_buffer(buf, quote, mail)
{
	var lines;
	var ln = '';
	var lc = 0;
	var ns = false;
	var ch;
	var ms = morechk;
	var qf = new File(gamedir('quote'+player.Record+'.lrd'));

	morechk = false;
	lines = buf.split(/\r?\n/);
	while (lines.length > 0) {
		ln = lines.shift();
		if (quote) {
			if (!qf.open('a')) {
				throw('Unable to open '+qf.name);
			}
			qf.writeln(ln);
			qf.close();
		}
		lln(ln, mail);
		lc += 1;
		if (ns === false && lc >= dk.console.rows - 1) {
			lw(' `2(`5C`2)ontinue, (`5S`2)top, (`5N`2)onstop `0: `%');
			ch = getkey().toUpperCase();
			lc = 0;
			if (ch === 'S') {
				dk.console.print('\r');
				dk.console.cleareol();
				break;
			}
			if (ch === 'N') {
				ns = true;
			}
			dk.console.print('\r');
			dk.console.cleareol();
		}
	}
	morechk = ms;
}

function mail_check()
{
	if (pfile !== undefined) {
		return file_exists(gamedir('mail'+player.Record+'.lrd'));
	}
	else {
		psock.writeln('CheckMail '+player.Record);
		switch(psock.readln()) {
			case 'Yes':
				return true;
			default:
				return false;
		}
	}
}

function check_mail()
{
	var mf = gamedir('mail'+player.Record+'.lrd');
	var resp;
	var m;
	var len;

	if (pfile !== undefined) {
		fmutex(mf+'.tmp');
		file_remove(mf+'.tmp');
		fmutex(mf);
		if (file_exists(mf+'.tmp')) {
			file_remove(mf+'.tmp');
		}
		if (file_exists(mf)) {
			file_rename(mf, mf+'.tmp');
			rfmutex(mf);
			foreground(15);
			sln('  ** YOU ARE STOPPED BY A MESSENGER WITH THE FOLLOWING NEWS: **');
			sln('');
			show_lord_file(mf+'.tmp', true, true);
			file_remove(mf+'.tmp');
			sln('');
		}
		else {
			rfmutex(mf);
		}
		rfmutex(mf+'.tmp');
	}
	else {
		if (mail_check()) {
			psock.writeln('GetMail '+player.Record);
			foreground(15);
			sln('  ** YOU ARE STOPPED BY A MESSENGER WITH THE FOLLOWING NEWS: **');
			sln('');
			resp = psock.readln();
			m = resp.match(/^Mail ([0-9]+)$/);
			if (m !== null) {
				len = parseInt(m[1], 10);
				resp = psock.read(len);
				if (psock.read(2) !== '\r\n') {
					throw('Out of sync with server after GetMail');
				}
				show_lord_buffer(resp, true, true);
			}
		}
	}
}

function more()
{
	var oa = dk.console.attr.value;

	// DIFF: There's some weird mail_off and new_game checking here...
	//       It appears it's to keep player writes to a minimum.
	curlinenum = 1;
	player.put();
	check_mail();
	lw('  `2<`0MORE`2>');
	getkey();
	if (!dk.user.ansi_supported || settings.use_fancy_more === false) {
		sln('');
		curlinenum = 1;
	}
	else {
		dk.console.print('\r');
		dk.console.cleareol();
	}
	dk.console.attr.value = oa;
}

function put_state()
{
	// TODO: Handle server-side stuff here...
	state.put();
	rfmutex(statefile.file.name);
}

function tournament_over()
{
	var a = [];
	var sk;
	var winstats = ['exp','drag_kills','pvp_kills','level','laid'];

	all_players().forEach(function(p) {
		if (p.name !== 'X') {
			a.push(p);
		}
	});

	sk = winstats[tournament_settings.winstat];

	a.sort(function(a,b) {
		if (b[sk] !== a[sk]) {
			return b[sk] - a[sk];
		}
		if (b.exp !== a.exp) {
			return b.exp - a.exp;
		}
		if (b.drag_kills !== a.drag_kills) {
			return b.drag_kills - a.drag_kills;
		}
		if (b.pvp !== a.pvp) {
			return b.pvp - a.pvp;
		}
		if (b.level !== a.level) {
			return b.level - a.level;
		}
		if (b.laid !== a.laid) {
			return b.laid - a.laid;
		}
		if (b.gem !== a.gen) {
			return b.gem - a.gem;
		}
		return 0;
	});

	sclrscr();
	sln('');
	sln('');
	foreground(10);
	lln('  The tournament is now over! The winner is: '+a[0].name);
	sln('');
	sln('  Now taking you back to the BBS..');
	sln('');
	lln('  `2<`0MORE`2>');
	getkey();
	if (psock !== undefined) {
		psock.writeln('NewHero '+a[0].name);
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after NewHero');
		}
	}
	state.won_by = a[0].Record;
	state.latesthero = a[0].name;
	put_state();
	if (player !== undefined) {
		player.on_now = false;
		player.put();
	}
	exit(0);
}

function killmail(to)
{
	if (pfile !== undefined) {
		file_remove(gamedir('mail'+to+'.lrd'));
	}
	else {
		psock.writeln('KillMail '+to);
		if (psock.readln() !== 'OK') {
			throw('Unable to KillMail for '+to);
		}
	}
}

function divorce_seth() {
	state.married_to_seth = -1;
	if (psock !== undefined) {
		psock.writeln('SethMarried -1');
		if (['Yes','No'].indexOf(psock.readln()) === -1) {
			throw('Out of sync with server after SethMarried -1');
		}
	}
}

function divorce_violet() {
	state.married_to_violet = -1;
	if (psock !== undefined) {
		psock.writeln('VioletMarried -1');
		if (['Yes','No'].indexOf(psock.readln()) === -1) {
			throw('Out of sync with server after VioletMarried -1');
		}
	}
}

function violet_marriage()
{
	var mstr = '\n`%  Latest news about `#Violet`%, your wife.\n`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n';
	var husband = player_get(state.married_to_violet);

	if (random(5) < 1) {
		divorce_violet();
		husband.cha = parseInt(husband.cha / 2, 10);
		husband.put();
		switch (random(3)) {
			case 0:
				log_line('`2  `#Violet`2 has `%DIVORCED `0'+husband.name+'`2 for cheating on her with\n'+
				    '`2  `4Grizelda!  `#Violet`2 got her barmaid job back!');
				mstr += '  `2Violet has divorced you because Grizelda said you kissed her!  You\n';
				mstr += '  curse Grizelda!  What will people think?\n';
				break;
			case 1:
				log_line('`2  `#Violet`2 has `%LEFT `0'+husband.name+'`2 so she could get her old job\n'+
				    '`2  back at the Inn!  `0'+husband.name+' `2is heartbroken.');
				mstr += '  `2You hunt around the house for Violet, and all you find is a note!  She\n';
				mstr += '  left you!  She could not resist the temptation of working at the Inn\n';
				mstr += '  once again.  You try to control your convulsive sobs.\n';
				break;
			case 2:
				log_line('`2  `0'+husband.name+' `2has `%DIVORCED `#Violet`2 because she refused to do\n'+
				    '`2  any housework!  She got her old job back at the Inn!');
				mstr += '  `2You ask Violet to wash the dishes, and she refuses!  You have a big\n';
				mstr += '  fight!  You decide she isn\'t the women you married, and divorce her.\n';
				mstr += '  The whole experience has left you bitter.\n';
				break;
		}
		mstr += '\n';
		mstr += '  `4CHARM DROPS TO '+pretty_int(husband.cha)+'\n';
		mail_to(husband.Record, mstr);
	}
	else {
		switch(random(4)) {
			case 0:
				log_line('`2  `#Violet`2 has `%PMS!  `0'+husband.name+'`2 is understanding, and peace\n'+
				    '`2  is restored.  For now.');
				mstr += '  `2Violet gets angry over little things!  You realize she has PMS this\n';
				mstr += '  morning.  You treat her gently and calamity is avoided.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(100 * husband.level)+' EXPERIENCE.\n';
				husband.exp += (100 * husband.level);
				break;
			case 1:
				log_line('`2  `#Violet`2 bears `0'+husband.name+'`2 a male child.');
				mstr += '  `2Violet bears you a male child.  You have never loved her more.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(150 * husband.level)+' EXPERIENCE.\n';
				husband.exp += (150 * husband.level);
				husband.kids += 1;
				break;
			case 2:
				log_line('`2  `#Violet`2 bears `0'+husband.name+'`2 a female child.');
				mstr += '  `2Violet bears you a female child.  You are a little disappointed.\n';
				mstr += '  However, you are very pleased she retained her voluptuous figure.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(50 * husband.level)+' EXPERIENCE.\n';
				husband.exp += (50 * husband.level);
				husband.kids += 1;
				break;
			case 3:
				log_line('`2  `#Violet`2 and `0'+husband.name+'`2 didn\'t appear to get much sleep'+
				    '`2  last night.  The town is mystified.');
				mstr += '  `2Violet pleases you in ways you had only dreamed about.  Nothing\n';
				mstr += '  has ever felt so good as your wife giving herself freely to you.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(100 * husband.level)+' EXPERIENCE.\n';
				husband.exp += (100 * husband.level);
				break;
		}
		husband.put();
		mail_to(husband.Record, mstr);
	}
}

function seth_marriage()
{
	var mstr = '\n`%  Latest news about `0Seth Able`%, your husband.\n`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n';
	var wife = player_get(state.married_to_seth);

	if (random(5) < 1) {
		divorce_seth();
		wife.cha = parseInt(wife.cha / 2, 10);
		wife.put();
		switch(random(3)) {
			case 0:
				log_line('`2  `%Seth Able`2 has `%DIVORCED `0'+wife.name+'`2 for cheating on him with\n'+
				    '`2  the Bartender!  He now sings a song of woe!');
				mstr += '  `2Seth Able has divorced you because the Bartender said you kissed him!\n';
				mstr += '  You curse him!  What will people think?\n';
				break;
			case 1:
				log_line('`2  `0Seth Able `2has been `%BOOTED `2by `0'+wife.name+'`2!  Artistic\n'+
				    '`2  differences are to be blamed.');
				mstr += '  `2You are getting tired of seeing Seth hang around the house in his\n';
				mstr += '  underwear \'composing\' music.  You tell him to get a real job.\n';
				mstr += '  There is a fight - And you end up booting this artist.\n';
				break;
			case 2:
				log_line('`2  `0'+wife.name+' `2has `%DIVORCED `0Seth Able`2 because he refused to do\n'+
				    '`2  any housework!  It is "womans work" was his reply!');
				mstr += '  `2You ask Seth to wash the dishes, and he refuses!  You have a big\n';
				mstr += '  fight!  You decide he isn\'t the man you married, and divorce him.\n';
				mstr += '  The whole experience has left you bitter.\n';
				break;
			
		}
		mstr += '\n';
		mstr += '  `4CHARM DROPS TO '+pretty_int(wife.cha)+'\n';
		mail_to(wife.Record, mstr);
	}
	else {
		switch(random(4)) {
			case 0:
				log_line('`2  `0'+wife.name+'`2 screams at `%Seth Able `2for leaving\n'+
				    '`2  the lid up!  Seth is able to calm her down - peace is restored.');
				mstr += '  `2You wake up to find the tiolet seat up - AGAIN!  You scream your\n';
				mstr += '  objections at your man - He promises to be more carefull in the future.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(100 * wife.level)+' EXPERIENCE.\n';
				wife.exp += (100 * wife.level);
				wife.put();
				mail_to(wife.Record, mstr);
				break;
			case 1:
				log_line('`2  `0'+wife.name+'`2 bears a male child - `%Seth Able`2 is proud.');
				mstr += '  You bear a male child!  Seth Able is extremely pleased with you.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(150 * wife.level)+' EXPERIENCE.\n';
				wife.exp += (150 * wife.level);
				wife.kids += 1;
				wife.put();
				mail_to(wife.Record, mstr);
				break;
			case 2:
				log_line('`2  `0'+wife.name+'`2 bears a female child - `%Seth Able `2approves.');
				mstr += '  `2You bear a female child!  You are puzzled why Seth is not as happy\n';
				mstr += '  as you.  He refuses to speak about it.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(50 * wife.level)+' EXPERIENCE.\n';
				wife.exp += (50 * wife.level);
				wife.kids += 1;
				wife.put();
				mail_to(wife.Record, mstr);
				break;
			case 3:
				log_line('`2  `%Seth Able`2 and `0'+wife.name+'`2 didn\'t appear to get much sleep\n'+
				    '`2  last night.  The town is mystified.');
				mstr += '  `2Seth Able pleases you in ways you had only dreamed about.  Nothing\n';
				mstr += '  has ever felt so good as your husband doing the chores around the house.\n';
				mstr += '\n';
				mstr += '  `%YOU RECEIVE '+pretty_int(100 * wife.level)+' EXPERIENCE.\n';
				wife.exp += (100 * wife.level);
				wife.put();
				mail_to(wife.Record, mstr);
				break;
		}
	}
}

function daily_maint()
{
	var i;
	var pl;
	var bar;
	var plen;
	var tp;

	function talk_now(file, pl) {
		var all = [];
		var n;
		var full;

		if (settings.nochat) {
			return;
		}
		n = random(24);
		if (n > 11) {
			return;
		}
		if (pl.name === 'X') {
			return;
		}
		if (psock === undefined) {
			if (!file.open('r+')) {
				return;
			}
			all = file.readAll();
		}
		switch(n) {
			case 0:
				all.push(pl.sex === 'F' ? '  `%Seth Able:' : '  `%Violet:');
				if (pl.cha < 3) {
					all.push('  `0'+pl.name+'`0..  Have you showered recently?');
				}
				else if(pl.cha < 21) {
					all.push('  `0Greetings, '+pl.name+'`0.. Don\'t let them get you down.'); }
				else {
					all.push('  `0Hey '+pl.name+'`0, come closer to me..');
				}
				break;
			case 1:
				all.push('  `%Bartender:');
				all.push('  `0You jabber too much, '+pl.name+'.');
				break;
			case 2:
				all.push('  `%Barak:');
				if (pl.level > 2) {
					all.push('  `0You think you\'re tough, '+pl.name+' `0\'cuz you got a '+pl.weapon+'?');
				}
				else {
					all.push('  `0You really got a big mouth, '+pl.name+'`0.  Ugly one too.');
				}
				break;
			case 3:
				all.push('  `%Turgon:');
				if (pl.level < 6) {
					all.push('  `0Are you saying what I think you\'re saying '+pl.name+'`0?');
				}
				else if (pl.sex === 'M') {
					all.push('  `0Man I need to get wasted...'); }
				else {
					all.push('  `0Heed the words of '+pl.name+', `0For she is a great warrior.');
				}
				break;
			case 4:
				all.push('  `%Aragorn:');
				all.push('  `0Hey, '+pl.name+'`0, whats the deal with Jennie Garth?');
				break;
			case 5:
				all.push('  `%Seth Able:');
				all.push('  `0Damn, I need a drink.');
				break;
			case 6:
				all.push('  `%Grizelda:');
				if (pl.sex === 'M') {
					all.push('  `0'+pl.name+'`0! You\'re a cute one!  Come to mama!');
				}
				else if(settings.clean_mode) {
					all.push('  `0'+pl.name+'`0..Stay away from my man, wench!'); }
				else {
					all.push('  `0'+pl.name+'`0..Stay away from my man, slut.');
				}
				break;
			case 7:
				all.push('  `%Old Women From The Corner:');
				all.push('  `0There is no `4Dragon`0...The children all just ran away.');
				break;
			case 8:
				all.push('  `%Violet:');
				all.push('  `0I\'m so tired.  Will you escort me to my room,'+pl.name+'`0?');
				break;
			case 9:
				all.push('  `%Hooded Warrior:');
				all.push('  `0Watch you\'re back, '+pl.name+'`0.');
				break;
			case 10:
				all.push('  `%Old Man:');
				if (pl.cha > 3) {
					all.push('  `0Well met, '+pl.name+'`0!');
				}
				else {
					all.push('  `0'+pl.name+'`0!  Why did you kick my ass the other day?!');
				}
				break;
			case 11:
				all.push('  `%Barak:');
				if (pl.sex === 'M') {
					all.push('  `0'+pl.name+'`0.  Interesting name...');
				}
				else if (pl.cha < 4) {
					all.push('  `0'+pl.name+'`0!  You have no breasts!  Are you a man!?  Har!'); }
				else if (pl.cha < 8) {
					all.push('  `0Finally!  A woman with some sense!'); }
				else {
					all.push('  `0'+pl.name+'`0!  I think I\'m in love!  Marry me honey!  Har!');
				}
				break;
		}
		while(all.length > 18) {
			all.shift();
		}
		if (psock === undefined) {
			file.position = 0;
			file.truncate(0);
			file.writeAll(all);
			file.close();
		}
		else {
			full = all.join('\n');
			psock.write('AddToConversation bar '+full.length+'\r\n'+full+'\r\n');
			if (psock.readln() !== 'OK') {
				throw('Lost synchronization with server after AddToConversation');
			}
		}
	}

	// We have the state lock here, and state will be written after we return.
	sln('');
	foreground(10);
	switch(random(4)) {
		case 0:
			sw('  You stop to notice a bird singing this bright morning.');
			break;
		case 1:
			sw('  You take the time to clean your weapon this morning.');
			break;
		case 2:
			sw('  Time seems to stand still as you contemplate life.');
			break;
		case 3:
			sw('  Waking up is slow this morning.  You blame lastnights ale.');
			break;
	}
	sw('.');
	state.days += 1;
	sw('.');
	if (psock === undefined && tournament_settings.enabled && tournament_settings.days > 0 && tournament_settings.days <= state.days) {
		tournament_over();
	}
	sw('.');
	if (psock !== undefined) {
		psock.writeln('ClearPlayer');
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after ClearPlayer');
		}
	}
	// NPC talks to last person who chatted in the bar...
	if (psock === undefined && file_exists(gamedir('bar.lrd'))) {
		bar = new File(gamedir('bar.lrd'));
		fmutex(bar.name);
		if (state.last_bar !== -1) {
			tp = player_get(state.last_bar);
			if (tp.Yours) {
				talk_now(bar, tp);
				state.last_bar = -1;
			}
		}
		rfmutex(bar.name);
	}
	if (psock !== undefined) {
		if (state.last_bar !== -1) {
			tp = player_get(state.last_bar);
			if (tp.Yours) {
				talk_now(bar, tp);
				state.last_bar = -1;
			}
		}
	}

	sln('.');
	// this is the start of the actual player maintenance
	plen = player_length();
	for (i=0; i<plen; i += 1) {
		pl = player_get(i);
		if (pl.Yours) {
			if (!pl.on_now) {
				if (psock === undefined) {
					file_remove(gamedir('war'+pl.Record+'.bin'));
					file_remove(gamedir('mess'+pl.Record+'.bin'));
				}
			}
			if (!pl.on_now) {
				if (pl.name === 'X') {
					if (state.married_to_seth === i) {
						state.married_to_seth = -1;
						if (psock !== undefined) {
							psock.writeln('SethMarried -1');
							if (['Yes','No'].indexOf(psock.readln()) === -1) {
								throw('Out of sync with server after SethMarried -1');
							}
						}
					}
					if (state.married_to_violet === i) {
						state.married_to_violet = -1;
						if (psock !== undefined) {
							psock.writeln('VioletMarried -1');
							if (['Yes','No'].indexOf(psock.readln()) === -1) {
								throw('Out of sync with server after VioletMarried -1');
							}
						}
					}
				}
				else if (settings.del_1xp && pl.exp === 1 && pl.drag_kills === 0) {
					pl.name = 'X';
					pl.put();
					killmail(pl.Record);
					if (state.married_to_seth === i) {
						state.married_to_seth = -1;
						if (psock !== undefined) {
							psock.writeln('SethMarried -1');
							if (['Yes','No'].indexOf(psock.readln()) === -1) {
								throw('Out of sync with server after SethMarried -1');
							}
						}
					}
					if (state.married_to_violet === i) {
						state.married_to_violet = -1;
						if (psock !== undefined) {
							psock.writeln('VioletMarried -1');
							if (['Yes','No'].indexOf(psock.readln()) === -1) {
								throw('Out of sync with server after VioletMarried -1');
							}
						}
					}
				}
				else if (settings.delete_days > 0 && pl.time < (state.days - settings.delete_days)) {
					pl.name = 'X';
					pl.put();
					killmail(pl.Record);
					if (state.married_to_seth === i) {
						state.married_to_seth = -1;
						if (psock !== undefined) {
							psock.writeln('SethMarried -1');
							if (['Yes','No'].indexOf(psock.readln()) === -1) {
								throw('Out of sync with server after SethMarried -1');
							}
						}
					}
					if (state.married_to_violet === i) {
						state.married_to_violet = -1;
						if (psock !== undefined) {
							psock.writeln('VioletMarried -1');
							if (['Yes','No'].indexOf(psock.readln()) === -1) {
								throw('Out of sync with server after VioletMarried -1');
							}
						}
					}
				}
				else if (state.days > 3) {
					if (pl.time < (state.days - 2)) {
						if (pl.last_reincarnated < (state.days - 2)) {
							if (pl.dead) {
								if (pl.gone < settings.res_days) {
									pl.dead = false;
									lln('  `2(`0'+pl.name+' `2is raised from the dead)');
									pl.inn = false;
									pl.last_reincarnated = state.days;
									if (state.res_days > 0) {
										pl.gone += 1;
									}
									pl.divorced = false;
									pl.put();
								}
							}
						}
					}
				}
			}
		}
	}

	sln('');
	plen = player_length();
	if (state.married_to_seth > -1) {
		if (state.married_to_seth > plen) {
			divorce_seth();
		}
		else {
			tp = player_get(state.married_to_seth);
			if (tp.Yours) {
				if (tp.name === 'X') {
					divorce_seth();
				}
				else {
					seth_marriage();
				}
			}
		}
	}
	if (state.married_to_violet > -1) {
		if (state.married_to_violet > plen) {
			divorce_violet();
		}
		else {
			tp = player_get(state.married_to_violet);
			if (tp.Yours) {
				if (tp.name === 'X') {
					divorce_violet();
				}
				else {
					violet_marriage();
				}
			}
		}
	}
	if (psock !== undefined) {
		psock.writeln('SetPlayer '+player.Record);
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after SetPlayer');
		}
	}
	more();
}

function get_state(lock)
{
	var m;
	var resp;
	var st;

	if (statefile === undefined) {
		statefile = new RecordFile(gamedir('state.bin'), State_Def);
	}
	fmutex(statefile.file.name);
	if (statefile.length < 1) {
		state = statefile.new(1000);
	}
	else {
		state = statefile.get(0);
	}
	if (!lock) {
		rfmutex(statefile.file.name);
	}
	if (psock !== undefined) {
		psock.writeln('GetState');
		resp = psock.readln();
		m = resp.match(/StateData ([0-9]+)$/);
		if (m === null) {
			throw('Invalid response to StateData request');
		}
		st = psock.read(parseInt(m[1], 10));
		if (psock.read(2) !== '\r\n') {
			throw('Out of sync with server after GetState');
		}
		st = JSON.parse(st);
		Object.keys(st).forEach(function(k) {
			state[k] = st[k];
		});
	}
}

function create_log(havelock)
{
	var f = new File(gamedir('lognow.lrd'));
	var now = new Date();
	// Needs to be unique for each calendar day...
	var tday = now.getYear() * 366 + now.getMonth() * 31 + now.getDate();
	var happenings = ['`4  A Child was found today!  But scared deaf and dumb.',
	    '`4  More children are missing today.',
	    '`4  A small girl was missing today.',
	    '`4  The town is in grief.  Several children didn\'t come home today.',
	    '`4  Dragon sighting reported today by a drunken old man.',
	    '`4  Despair covers the land - more bloody remains have been found today.',
	    '`4  A group of children did not return from a nature walk today.',
	    '`4  The land is in chaos today.  Will the abductions ever stop?',
	    '`4  Dragon scales have been found in the forest today..Old or new?',
	    '`4  Several farmers report missing cattle today.'];

	if (pfile !== undefined) {
		if (!havelock) {
			fmutex(f.name);
		}

		get_state(true);
		if (file_exists(f.name)) {
			if (state.log_date !== tday) {
				file_remove(gamedir('logold.lrd'));
				file_rename(gamedir('lognow.lrd'), gamedir('logold.lrd'));
			}
			else {
				put_state();
				if (!havelock) {
					rfmutex(f.name);
				}
				return;
			}
		}
		else {
			put_state();
		}
		if (!f.open('a')) {
			rfmutex(f.name);
			throw('Unable to open '+f.name);
		}
		f.writeln('`2  The Daily Happenings....');
		f.writeln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		f.writeln(happenings[random(happenings.length)]);
		f.writeln('`.                                `2-`0=`2-`0=`2-`0=`2-');
		state.log_date = tday;
		// TODO: IGM Maint and out check things.
		f.close();
		if (!havelock) {
			rfmutex(f.name);
		}
		daily_maint();
		put_state();
	}
	else {
		get_state(true);
		if (state.log_date !== tday) {
			state.log_date = tday;
			daily_maint();
			// TODO: IGM Maint and out check things.
			state.log_date = tday;
		}
		put_state();
	}
}

function log_line(text)
{
	var f = new File(gamedir('lognow.lrd'));

	if (pfile !== undefined) {
		fmutex(f.name);
		if (!file_exists(f.name)) {
			create_log(true);
		}
		if (!f.open('a')) {
			throw('Unable to open '+f.name);
		}
		f.writeln(text);
		f.writeln('`.                                `2-`0=`2-`0=`2-`0=`2-');
		f.close();
		rfmutex(f.name);
	}
	else {
		psock.write('LogEntry '+text.length+'\r\n'+text+'\r\n');
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after LogEntry');
		}
	}
}

function clean_str(str)
{
	var ret = '';
	var i;

	for (i = 0; i < str.length; i += 1) {
		if (str[i] !== '`') {
			ret += str[i];
		}
		else {
			switch (str[i+1]) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '!':
				case '@':
				case '#':
				case '$':
				case '%':
					ret += str[i];
					break;
				default:
					i += 1;
			}
		}
	}

	// TODO: This is where badwords replacements should happen...

	return ret;
}

function format_date()
{
	var now = new Date();

	return format("%02d/%02d/%04d", now.getMonth()+1, now.getDate(), now.getFullYear());
}

function write_mail(to, quote)
{
	var qf = new File(gamedir('quote'+player.Record+'.lrd'));
	var l;
	var wrap='';
	var mf = new File(gamedir('msg'+player.Record+'.lrd'));
	var mb = new File(gamedir('mail'+to+'.lrd'));
	var ch;
	var t;
	var lines = 0;
	var derps = ['`.`%  Greetings.  How fare you, traveler?',
		'`.`%  Well met.  Any news you can share?',
		'`.`%  How goes it, fellow adventurer?',
		'`.`%  Didn\'t I see you on that table in the Dark Cloak tavern?',
		'`.`%  Sorry - I forgot what I was going to say, old bean.'];
	var bs = '\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08';
	var ws = '                                                                            ';

	if (quote === undefined) {
		quote = false;
	}

	sln('');
	sln('  Enter message now..Blank line quits!');
	if (!mf.open('w+')) {
		throw('Unable to open '+mf.name);
	}
	mf.writeln('`. ');
	mf.writeln('`.  `0' + player.name + '`2 sent you this on '+format_date());
	mf.writeln('`.`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2');
	if (quote && qf.exists) {
		qf.open('r');
		while (true) {
			l=qf.readln();
			if (l === null) {
				break;
			}
			if (l.substr(0,3) === '`% ') {
				l = '`0>'+l.substr(3);
				mf.writeln(l);
				lln(l);
			}
		}
		qf.close();
	}
	file_remove(qf.name);
	do {
		l = wrap;
		wrap = '';
		sln('');
		foreground(2);
		sw(' >');
		foreground(15);
		sw(l);
		do {
			ch = getkey();
			sw(ch);
			if (ch === '\x08') {
				l = l.slice(0, -1);
				sw(' \x08');
			}
			else if (ch !== '\x1b') {
				if (ch !== '\r') {
					l += ch;
				}
				if (l.length > 75) {
					t = l.lastIndexOf(' ');
					if (t !== -1) {
						wrap = l.slice(t+1);
						l = l.slice(0, t);
						sw(bs.substr(0, wrap.length));
						sw(ws.substr(0, wrap.length));
					}
					ch = '\r';
				}
			}
		} while (ch !== '\r');
		if (l.length === 0 && lines === 0) {
			// DIFF: Derps weren't passed through clean_str(),
			//       so kept the `. and therefore weren't quoted.
			mf.writeln(clean_str(derps[random(derps.length)]));
			break;
		}
		lines += 1;
		l = clean_str('`.`%  ' + l);
		mf.writeln(l);
	} while (l.length >= 5);
	mf.writeln('`-'+format('%03d', player.Record));
	if (pfile !== undefined) {
		mf.position = 0;
		fmutex(mb.name);
		if (!mb.open('a')) {
			rfmutex(mb.name);
			throw('Failed to open '+mb.name);
		}
		while (true) {
			l = mf.readln();
			if (l === null) {
				break;
			}
			mb.writeln(l);
		}
		mb.close();
		rfmutex(mb.name);
		mf.close();
		file_remove(mf.name);
	}
	else {
		mf.close();
		psock.writeln('WriteMail '+to+' '+file_size(mf.name));
		psock.sendfile(mf.name);
		psock.write('\r\n');
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after WriteMail');
		}
	}
	if (to === player.Record) {
		sln('');
		sln('  You are a very stupid individual.');
		sln('');
		more_nomail();
	}
}

function mail_to(to, mail)
{
	var mb = new File(gamedir('mail'+to+'.lrd'));

	if (pfile !== undefined) {
		fmutex(mb.name);
		if (!mb.open('a')) {
			rfmutex(mb.name);
			throw('Failed to open '+mb.name);
		}
		mb.writeln(mail);
		mb.close();
		rfmutex(mb.name);
	}
	else {
		psock.write('WriteMail '+to+' '+mail.length+'\r\n'+mail+'\r\n');
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after WriteMail');
		}
	}
}

function show_looks(op)
{
	var upronoun = op.sex === 'M' ? 'His' : 'Her';
	var upronoun2 = op.sex === 'M' ? 'He' : 'She';

	if (op.cha < 30) {
		lln('  `4WARNING:  `2' + upronoun + ' Charm Rating is only `%' + pretty_int(op.cha) + '`2!');

	}
	else {
		lln('  `2' + upronoun + ' Charm Rating is `%' + pretty_int(op.cha) + '`2.');
	}
	sln('');
	lln('  `2' + upronoun2 + ' '+looks(op.sex, op.cha));
	sln('');
}

function dead_screen(op)
{
	sln('');
	lln('  `4You have been killed by '+op.name+'`2.');
	sln('');
	more_nomail();
	lln('  `2GOLD ON HAND WAS `4LOST`2.  ');
	sln('');
	lln('  `2TEN PERCENT OF EXPERIENCE `4LOST`2.');
	sln('  ');
	foreground(2);
	sln('  You have been defeated on your way to glory.  The road to success');
	lln('  is long and hard.  You have encountered a minor setback.  But do `0NOT`2');
	sln('  lose heart, you can continue your struggle tomorrow.');
	sln('');
	more_nomail();
}

function tournament_check()
{
	var op;

	if (!tournament_settings.enabled) {
		return;
	}
	if (tournament_settings.days > 0) {
		return;
	}
	if (player.exp < tournament_settings.xp) {
		return;
	}
	if (player.drag_kills < tournament_settings.dkills) {
		return;
	}
	if (player.pvp < tournament_settings.pkills) {
		return;
	}
	if (player.level < tournament_settings.level) {
		return;
	}
	if (player.laid < tournament_settings.lays) {
		return;
	}

	get_state(true);
	if (state.won_by < 0) {
		state.won_by = player.Record;
		if (psock !== undefined) {
			psock.writeln('NewHero '+player.name);
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after NewHero');
			}
		}
		state.latesthero = player.name;
		put_state();
		sclrscr();
		sln('');
		sln('');
		foreground(10);
		sln('  Congratulations! You have won the tournament!');
		sln('');
		lln('  `2<`0MORE`2>');
		getkey();
		player.on_now = false;
		player.put();
	}
	else {
		put_state();
		op = player_get(state.won_by);
		sclrscr();
		sln('');
		sln('');
		foreground(10);
		lln('  You have completed the tournament, but '+op.name);
		lln('  got there first.');
		sln('');
		lln('  `2<`0MORE`2>');
		getkey();
		player.on_now = false;
		player.put();
	}
	exit(0);
}

var cantaunt = true;	// TODO: Remove this variable... icky.
function ob_cleanup() {
	if (psock === undefined) {
		file_remove(gamedir('war'+player.Record+'.bin'));
		rfmutex(gamedir('fight'+player.Record+'.rec'));
		file_remove(gamedir('mess'+player.Record+'.bin'));
	}
}

function ob_read_msg(fname) {
	var f = new File(fname);
	var msg;

	if (psock !== undefined) {
		return undefined;
	}
	do {
		while (!f.open('r')) {
			mswait(1);
		}
		msg = f.read();
		f.close();
	} while (msg === undefined || msg.length < 1 || msg[msg.length-1] !== '\n');
	msg = msg.replace(/[\r\n]/g,'');

	file_remove(fname);
	return msg;
}

function ob_send_resp(op, str) {
	if (psock !== undefined) {
		psock.writeln('SendBattleResponse '+str);
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after SendBattleResponse');
		}
	}
	else {
		file_mutex(gamedir('war'+op.Record+'.bin'), str+'\n');
	}
}

function ob_get_message(op) {
	var msg;

	if (psock !== undefined) {
		return false;
	}
	if (file_exists(gamedir('mess'+player.Record+'.bin'))) {
		sln('');
		sln('');
		lln('  `0'+op.name+' `2screams something at you!`%');
		sln('');
		msg = ob_read_msg(gamedir('mess'+player.Record+'.bin'));
		lln(msg);
		cantaunt = true;
		sln('');
		more_nomail();
		return true;
	}
	return false;
}

function ob_get_response(op) {
	var dh;
	var loops = 0;
	var msg;
	var done = false;
	var m;

	sln('');
	lw('  `2Waiting`0.');
	if (psock !== undefined) {
		psock.writeln('WaitBattleResponse');
	}
	do {
		if (pfile !== undefined) {
			op.reLoad();
			if (!op.on_now) {
				return 'R';
			}
		}
		if (dk.console.waitkey(0)) {
			dh = getkey().toUpperCase();
		}
		else {
			dh = '';
		}
		if (loops > 70 && dh === 'R') {
			if (psock !== undefined) {
				psock.writeln('AbortBattleWait');
				while(psock.readln() !== 'OK')
					{}
			}
			log_line('`.  `0'+player.name+' `2has been eluded by `0'+op.name+'`2.');
			ob_send_resp(op, 'R');
			sln('');
			return 'ABORT';
		}
		sw('.');
		if (loops === 70) {
			sln('');
			lln('  `2(`0R`2)un Away');
			sln('');
			lw('  `2Continuing wait`0.');
		}
		if (ob_get_message(op)) {
			sln('');
			lw('  `2Continuing wait`0.');
		}
		loops += 1;
		if (pfile !== undefined) {
			mswait(600);
			if (file_exists(gamedir('war'+player.Record+'.bin'))) {
				done = true;
			}
		}
		else {
			if (psock.poll(0.6) === 1) {
				msg = psock.readln();
				m = msg.match(/^Yell: (.*)$/);
				if (m !== null) {
					sln('');
					sln('');
					lln('  `0'+op.name+' `2screams something at you!`%');
					sln('');
					lln(m[1]);
					cantaunt = true;
					sln('');
					more_nomail();
					sln('');
					lw('  `2Continuing wait`0.');
					psock.writeln('WaitBattleResponse');
				}
				else {
					return msg;
				}
			}
		}
	} while(!done);
	msg = ob_read_msg(gamedir('war'+player.Record+'.bin'));
	sln('');
	return msg;
}

function on_battle(op, first, action) {
	var ch;
	var tmp;

	function on_struck(op, amount) {
		var his = op.sex === 'M' ? 'his' : 'her';

		cantaunt = true;
		amount = parseInt(amount, 10);

		if (amount < 1) {
			lln('  `0'+op.name+' `2misses you!');
			sln('');
			return;
		}
		lln('  `0'+op.name+' `2hits you for `4'+pretty_int(amount)+'`2 points of damage!');
		player.hp -= amount;
		sln('');
		if (player.hp < 1) {
			lln('  You fall to the ground.  You think '+op.name+' `2is going to let you');
			lln('  go... Just before you feel '+his+' '+op.weapon+' sliding');
			sln('  through your chest. ');
			sln('');
			sln('  You distantly feel your lungs filling with blood, when everything becomes');
			sln('  black.');
			player.dead = true;
			if (psock !== undefined) {
				psock.writeln('DoneOnlineBattle');
				if (psock.readln() !== 'OK') {
					throw('Out of sync with server after DoneOnlineBattle');
				}
			}
			log_line('`.  `0'+op.name+' `2has killed `5'+player.name+' `2in an Online Duel!');
			player.gold = 0;
			player.exp -= parseInt(player.exp / 10, 10);
			player.hp = 0;
			player.on_now = false;
			player.put();
			more_nomail();
			dead_screen(op);
			sln('');
		}
	}

	function wait_for_hit(op) {
		var waction;

		lln('  You wait as '+op.name+' `2strikes.');
		waction = ob_get_response(op);
		if (waction === 'ABORT') {
			sln('');
			sln('  You run as fast as your legs will carry you!');
			sln('');
			more_nomail();
			return;
		}
		if (waction === 'R') {
			lln('  `0The sniveling '+op.name+' `0has run away.  Unfortunatly, this battle');
			sln('  is OVER.');
			log_line('  `0'+player.name+' `2& `0'+op.name+' `2had an uneventful online duel.');
			sln('');
			more_nomail();
			return;
		}
		on_struck(op, waction);
	}

	if (first) {
		lln('  `%** YOUR SKILL ALLOWS YOU TO ATTACK FIRST **');
		sln('');
		more_nomail();
	}
	else {
		lln('  `%** YOUR ENEMY STRIKES YOU FIRST **');
		if (action === undefined) {
			wait_for_hit(op);
		}
		else {
			on_struck(op, action);
		}
	}

	cantaunt = true;
outer:
	while (true) {
		op.reLoad();
		if (player.hp < 1 || op.hp < 1) {
			break;
		}
		lln('  `2Your Hitpoints:`0 '+pretty_int(player.hp));
		lln('  `0'+op.name+'`2\'s Hitpoints:`0 '+pretty_int(op.hp));
		sln('');
		lln('  `2(`0A`2)ttack Your Enemy');
		if (cantaunt) {
			lln('  `2(`0Y`2)ell Something To Your Enemy');
		}
		lln('  `2(`0R`2)un For Your Life');
		sln('');
		lw('  `2Your Command ?  :`0 ');
		ch = getkey().toUpperCase();
		sln(ch);
		if (!ob_get_message(op)) {
			switch(ch) {
				case 'A':
					sln('');
					tmp = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
					tmp -= op.def;
					if (tmp < 1) {
						lln('  `2You miss `0'+op.name+'`2 Completely!');
						sln('');
						ob_send_resp(op, '0');
						wait_for_hit(op);
						break;
					}
					lln('  `2You HIT `0'+op.name+'`2 for `4'+pretty_int(tmp)+'`2 points of damage!');
					ob_send_resp(op, tmp);
					op.hp -= tmp;
					if (op.hp < 1) {
						sln('');
						lln('  You have killed '+op.name+'`2.');
						player.killedaplayer = true;
						sln('');
						tmp = player.gold;
						player.gold += op.gold;
						if (player.gold > 2000000000) {
							player.gold = 2000000000;
						}
						lw('  `2You receive `%'+pretty_int(player.gold - tmp)+'`2 gold, and '+pretty_int(op.exp / 2)+' experience!');
						if (op.gem > 2) {
							tmp = player.gem;
							player.gem += parseInt(op.gem / 2, 10);
							if (player.gem > 32000) {
								player.gem = 32000;
							}
							// DIFF: You got an extra pvp fight here?!?!
							lw('  You also find '+pretty_int(player.gem - tmp)+' Gems!');
						}
						sln('');
						player.exp += parseInt(op.exp / 2, 10);
						if (psock !== undefined) {
							psock.writeln('DoneOnlineBattle');
							if (psock.readln() !== 'OK') {
								throw('Out of sync with server after DoneOnlineBattle');
							}
						}
						player.put();
						more_nomail();
						tournament_check();
						break outer;
					}
					wait_for_hit(op);
					break;
				case 'Y':
					if (cantaunt) {
						sln('');
						sln('  Yell what?');
						sln('');
						lw(' `0>`2');
						tmp = '  '+dk.console.getstr();
						if (psock === undefined) {
							file_mutex(gamedir('mess'+op.Record+'.bin', tmp+'\n'));
						}
						else {
							psock.writeln('SendBattleResponse Yell: '+tmp);
							if (psock.readln() !== 'OK') {
								throw('Out of sync with server after SendBattleResponse Yell:');
							}
						}
						sln('');
						sln('');
						lln('  `%** MESSAGE YELLED **');
						sln('');
						cantaunt = false;
					}
					else {
						sln('');
						lln('  `2You feel you had better stop taunting '+op.name+'`2, or you will');
						sln('  lose your attack!');
						sln('');
					}
					break;
				case 'R':
					ob_send_resp(op, 'R');
					sln('');
					lln('  `2You run like the dickens!  You manage to elude `0'+op.name+'`2!');
					break outer;
			}
		}
	}
}

// Outputs text with ` codes (no CRLF)
function lw(str, ext)
{
	var i;
	var to;
	var oop;
	var snip = '';
	var lwch;

	function answer_mail(to) {
		var op = player_get(to);
		var ch;
		var he;

		sln('');
		foreground(10);
		sw('  Answer '+(op.sex === 'M' ? 'him?' : 'her?'));
		lw(' `2[`5Y`2] `0: ');
		ch = getkey().toUpperCase();
		if (ch !== 'N') {
			ch = 'Y';
		}
		sln(ch);
		if (ch === 'N') {
			file_remove(gamedir('quote'+player.Record+'.lrd'));
			return;
		}
		if (to === player.Record) {
			sln('');
			sln('  ** THE MESSENGER REFUSES TO DELIVER THE MESSAGE! **');
			return;
		}
		he = op.sex === 'M' ? 'he' : 'she';
		sln('');
		sln('');
		foreground(10);
		sw('  Quote what '+he+' said? : ');
		lw('`2[`5Y`2] `0: ');
		ch = getkey().toUpperCase();
		if (ch !== 'N') {
			ch = 'Y';
		}
		sln(ch);
		write_mail(to, ch === 'Y');
		file_remove(gamedir('quote'+player.Record+'.lrd'));
	}

	function smile_mail(to) {
		var op = player_get(to);
		var him;
		var he;
		var ch;

		sln('');
		him = op.sex === 'M' ? 'him' : 'her';
		he = player.sex === 'M' ? 'He' : 'She';
		show_looks(op);
		lln('  `2(`0P`2)ointedly ignore '+him);
		lln('  `2(`0S`2)mile hugely');
		sln('');
		lw('  What do you do about it? `0[`2S`0]`2 : ');
		ch = getkey().toUpperCase();
		if (ch !== 'P') {
			ch = 'S';
		}
		sln(ch);
		sln('');
		lln('  `%** Please wait, writing '+him+' back **');
		if (ch !== 'P') {
			mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' smiles back, encouragingly!\n'+
			    '`0  \n'+
			    '`2  You receive `%'+pretty_int(5*op.level)+' `2experience points!\n'+
			    '`E'+(5*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
		}
		else {
			mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' ignores you!  You are snubbed good!\n'+
			    '`0  \n'+
			    '`2  You `4LOSE `%'+pretty_int(5*op.level)+' `2experience points!\n'+
			    '`V'+(5*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
		}
		file_remove(gamedir('quote'+player.Record+'.lrd'));
	}

	function kiss_mail(to) {
		var op = player_get(to);
		var him;
		var ch;
		var he;

		sln('');
		him = op.sex === 'M' ? 'him' : 'her';
		he = player.sex === 'M' ? 'He' : 'She';
		show_looks(op);
		lln('  `2(`0P`2)ointedly ignore '+him);
		lln('  `2(`0K`2)iss '+him+' soundly');
		sln('');
		lw('  What do you do about it? `0[`2K`0]`2 : ');
		ch = getkey().toUpperCase();
		if (ch !== 'P') {
			ch = 'K';
		}
		sln(ch);
		sln('');
		lln('  `%** Please wait, writing '+him+' back **');
		if (ch !== 'P') {
			mail_to(to,'`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' gives you a big wet kiss!\n'+
			    '`2  You receive `%'+pretty_int(10*op.level)+' `2experience points!\n'+
			    '`E'+(10*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
		}
		else {
			mail_to(to,'`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' ignores you!  You are snubbed really good!\n'+
			    '`0  \n'+
			    '`2  You `4LOSE `%'+pretty_int(10*op.level)+' `2experience points!\n'+
			    '`V'+(10*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
		}
		file_remove(gamedir('quote'+player.Record+'.lrd'));
	}

	function dinner_mail(to) {
		var op = player_get(to);
		var him;
		var he;
		var ch;

		sln('');
		him = op.sex === 'M' ? 'him' : 'her';
		he = player.sex === 'M' ? 'He' : 'She';
		show_looks(op);
		lln('  `2(`0P`2)ointedly ignore '+him);
		lln('  `2(`0G`2)o to dinner with '+him);
		sln('');
		lw('  What do you do about it? `0[`2G`0]`2 : ');
		ch = getkey().toUpperCase();
		if (ch !== 'P') {
			ch = 'G';
		}
		sln(ch);
		sln('');
		lln('  `%** Please wait, writing '+him+' back **');
		if (ch !== 'P') {
			mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' agrees to go to dinner with you!\n'+
			    '`0  \n'+
			    '`0  You both have a wonderful time, and learn a lot from each other.\n'+
			    '`0  '+
			    '`2  You receive `%'+pretty_int(20*op.level)+' `2experience points!\n'+
			    '`E'+(20*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
			log_line('`%  '+player.name+' `2agrees to have dinner with `0'+op.name+'`2!');
			foreground(2);
			sln('');
			sln('  You have a wonderful time at dinner, and both learn a lot about');
			sln('  each other.  You both vow to do it again sometime.');
			sln('');
			more_nomail();
		}
		else {
			mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' laughs in your face!  You are snubbed so bad.\n'+
			    '`0  \n'+
			    '`2  You `4LOSE `%'+pretty_int(20*op.level)+' `2experience points!\n'+
			    '`V'+(20*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
			log_line('`%  '+player.name+' `2refuses to dine with `0'+op.name+'`2!');
		}
		file_remove(gamedir('quote'+player.Record+'.lrd'));
	}

	function sleep_mail(to) {
		var op = player_get(to);
		var him;
		var he;
		var ohe;
		var ch;

		sln('');
		him = op.sex === 'M' ? 'him' : 'her';
		he = player.sex === 'M' ? 'He' : 'She';
		ohe = op.sex === 'M' ? 'he' : 'she';
		show_looks(op);
		lln('  `2(`0P`2)ointedly tell '+him+' absolutely not!');
		lln('  `2(`0S`2)leep with '+him);
		sln('');
		lw('  What do you do about it? `0[`2P`0]`2 : ');
		ch = getkey().toUpperCase();
		if (ch !== 'S') {
			ch = 'P';
		}
		sln(ch);
		sln('');
		lln('  `%** Please wait, writing '+him+' back **');
		if (ch === 'S') {
			player.laid += 1;
			mail_to(to,'`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' agrees to sleep with you!\n'+
			    '\n'+
			    '  You both have a wonderfully sweaty time, and learn very little\n'+
			    '  from each other.  (but you had fun!)\n'+
			    '\n'+
			    '`2  You receive `%'+pretty_int(50*op.level)+' `2experience points!\n'+
			    '`E'+pretty_int(50*op.level)+'\n'+
			    '`{\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
			log_line('`%  '+player.name+' `2got laid by `0'+op.name+'`2!');
			lln('`c`%  ** UPSTAIRS IN THE INN **');
			sln('');
			lln('  `2When you finally arrive at '+op.name+'`2\'s bedroom at the appointed');
			sln('  time, you are nervous as ever!');
			sln('');
			more_nomail();
			sln('  Your knocks are unanswered.  Since the door is slightly ajar, you ');
			sln('  see no harm in letting yourself in.  You push through the beads into a');
			sln('  smokey room.  The smell of herbs fills your nostrils, and the sound of');
			sln('  soft music fills your ears.');
			sln('');
			more_nomail();
			lln('  And then a skimpily clad '+op.name+'`2 walks into the room!');
			sln('');
			more_nomail();
			lln('  Without a word, '+ohe+' points at the heart-shaped bed.');
			sln('');
			more_nomail();
			sln('  A good while later you are drenched in sweat, tired, but feeling');
			lln('  good.  '+op.name+'`2 holds you close, as if '+ohe+' never wanted');
			sln('  this moment to end.');
			sln('');
			if (random(2) === 0) {
				more_nomail();
				if (player.sex === 'M') {
					lln('  `%Seth Able hi-fives you on the way out!');
				}
				else {
					lln('  `%Violet gives you a dirty look on the way out!');
				}
			}
			sln('');
			more_nomail();
			tournament_check();
		}
		else {
			mail_to(to,'`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+he+' laughs in your face!  You are snubbed the worst you\n'+
			    '  have ever been snubbed before in your life.\n'+
			    '  \n'+
			    '`2  You `4LOSE `%'+pretty_int(50*op.level)+' `2experience points!\n'+
			    '`V'+(50*op.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
			log_line('`%  '+op.name+' `2got seriously snubbed by `0'+player.name+'`2!');
			foreground(15);
			sln('');
			sln('  Ouch!');
			sln('');
		}
		file_remove(gamedir('quote'+player.Record+'.lrd'));
	}

	function marry_mail(to) {
		var mop = player_get(to);
		var bae;
		var she;
		var him;
		var her;
		var ch;
		
		him = mop.sex === 'M' ? 'him' : 'her';
		she = player.sex === 'M' ? 'He' : 'She';
		her = player.sex === 'M' ? 'Her' : 'His';

		lln('  `2(`0S`2)ay YES!');
		lln('  `2(`0T`2)urn '+him+' down!');
		sln('');
		lw('  What do you do about it? `0[`2T`0]`2 : ');
		ch = getkey().toUpperCase();
		if (ch !== 'S') {
			ch = 'T';
		}
		sln(ch);
		sln('');
		lln('  `%** Please wait, writing '+him+' back **');
		if (ch === 'S') {
			player.married_to = to;
			if (psock !== undefined) {
				psock.writeln('Marry '+player.Record+' '+to);
				if (['Yes','No'].indexOf(psock.readln()) === -1) {
					throw('Out of sync with server after Marry');
				}
			}
			mop = mop.reLoad(true);
			// DIFF: See what happens here... this check is new.
			if (mop.married_to === -1 || mop.married_to === player.Record) {
				mop.married_to = player.Record;
				if (pfile !== undefined) {
					mop.put(false);
				}
				player.put();
				mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
				    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
				    '`0  '+she+' agrees to marry you!\n'+
				    '\n'+
				    '  The wedding is simple, but beautiful.   Your significant other\n'+
				    '  stands elequently beside you, as you echange vows.)\n'+
				    '\n'+
				    '  `2Knowing the elders don\'t take marriage lightly, you hope it\n'+
				    '  will last...\n'+
				    '`?'+(player.Record));
				log_line('`%** `0'+player.name+' `2said YES to `0'+mop.name+'`2\'s marriage proposal! `%**');
				sclrscr();
				foreground(15);
				sln('');
				sln('');
				sln('  ** THE WEDDING **');
				sln('');
				foreground(2);
				sln('  Your spouse is overjoyed at your answer.  '+her+' face beams so');
				sln('  brightly,  you know you\'ve made the right choice.');
				sln('');
				more_nomail();
				if (player.sex === 'M') {
					sln('  The vows are short and to the point - but heartfelt.  You are especially ');
					sln('  proud of the one "To have; To hold..  And to satisfy anytime she needs it"');
					lln('  (you made that one up yourself)  `%Seth Able `2laughs at the back of the');
					sln('  church at this.  Many faces turn in his direction - making him turn');
					sln('  a bright purple.');
					sln('');
					more_nomail();
					if (random(2) === 0) {
						lln('  `0A figure dressed in black attracts your attention in the back...');
						sln('');
						more_nomail();
						lln('  `2You recognize the voloptous curves to be `#Violet`2\'s.');
						sln('');
						sln('  A single tear glistens on her cheek.');
					}
					sln('');
					more_nomail();
					lln('  `2Turgon solomnly gives you permission to kiss the bride.');
					sln('');
					more_nomail();
				}
				else {
					sln('  The vows are short and to the point - but heartfelt.  Violet giggles as she');
					sln('  hands you the ring.  (You wonder why)  Even through the veil covering');
					sln('  your face - your man looks so handsome - so strong.  ');
					sln('');
					more_nomail();
					if (random(2) === 0) {
						lln('  `0A figure dressed in black attracts your attention in the back...');
						sln('');
						sln('');
						more_nomail();
						lln('  `2You recognize the individual by the tightly clutched mandolin.');
						sln('');
						sln('  A single tear glistens on his cheek.');
						sln('');
						more_nomail();
					}
					sln('  Turgon solomnly gives your husband his permission to kiss you.');
					sln('');
					more_nomail();
				}
				lln('  `%THE ENTIRE CROWD ROARS ITS APPROVAL!');
				sln('');
			}
			else {
				mop.unLock();
				bae = player_get(mop.married_to);
				lln('`0  '+mop.name+' laughs in your face!');
				lln('  You never answered, so I married '+bae.name+' instead!');
				sln('  '+she+' can at least answer a question.');
				log_line('`%** `0'+player.name+' `2said YES to `0'+mop.name+'`2\'s marriage proposal! `%**\n`2  (But not until after `0'+mop.name+'`2 already married `0'+bae.name+'`2!');
				lln('`4  YOU ARE `)ULTRA `4SNUBBED!');
				lln('`0  ');
				lln('`2  You `4LOSE `%'+pretty_int(100*mop.level)+' `2experience points!');
				player.exp -= 100 * mop.level;
			}
		}
		else {
			mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
			    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
			    '`0  '+she+' laughs in your face!\n'+
			    '`4  YOU ARE `)ULTRA `4SNUBBED!\n'+
			    '`0  \n'+
			    '`2  You `4LOSE `%'+pretty_int(100*mop.level)+' `2experience points!\n'+
			    '`V'+(100*mop.level)+'\n'+
			    '```|');
			    //DIFF: Returns didn't have `|, so the "Return from" bit was quoted in the NEXT message.
			    //      Starting the first line with `. would also fix the problem.
			log_line('`%  '+mop.name+' `2got `)TURNED DOWN `2by `0'+player.name+'`2!');
			sln('');
			lln('  `%Yowzers you\'re cold!');
			sln('');
		}
		file_remove(gamedir('quote'+player.Record+'.lrd'));
	}

	if (ext === undefined) {
		ext = false;
	}
	for (i=0; i<str.length; i += 1) {
		if (str[i] === '`') {
			sw(snip);
			snip = '';
			i += 1;
			if (i > str.length) {
				break;
			}
			switch(str[i]) {
				case '.':
					break;
				case 'n':
					if (curcolnum <= dk.console.cols) {
						sln('');
					}
					break;
				case '`':
					getkey();
					break;
				case 'l':
					foreground(2);
					sw('  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					break;
				case '1':		// BLUE
					foreground(1);
					break;
				case '2':		// GREEN
					foreground(2);
					break;
				case '3':		// CYAN
					foreground(3);
					break;
				case '4':		// RED
					foreground(4);
					break;
				case '5':		// MAGENTA
					foreground(5);
					break;
				case '6':		// YELLOW
					foreground(6);
					break;
				case '7':		// WHITE/GRAY
					foreground(7);
					break;
				case '8':
					foreground(8);
					break;
				case '9':
					foreground(9);
					break;
				case '0':
					foreground(10);
					break;
				case '!':
					foreground(11);
					break;
				case '@':
					foreground(12);
					break;
				case '#':
					foreground(13);
					break;
				case '$':
					foreground(14);
					break;
				case '%':
					foreground(15);
					break;
				case '^':
					foreground(0);
					break;
				case ')':
					foreground(20);
					break;
				case 'c':
					sclrscr();
					sln('');
					sln('');
					break;
				case 'C':
					sclrscr();
					break;
				case '|':
					file_remove(gamedir('quote'+player.Record+'.lrd'));
					break;
				case 'B':
					i += 1;
					if (i > str.length) {
						break;
					}
					switch (str[i]) {
						case '1':		// BLUE
							foreground(17);
							break;
						case '2':		// GREEN
							foreground(18);
							break;
						case '3':		// CYAN
							foreground(19);
							break;
						case '4':		// RED
							foreground(20);
							break;
						case '5':		// MAGENTA
							foreground(21);
							break;
						case '6':		// YELLOW
							foreground(22);
							break;
						case '7':		// WHITE/GRAY
							foreground(23);
							break;
						case '8':
							foreground(24);
							break;
						case '9':
							foreground(25);
							break;
						case '0':
							foreground(26);
							break;
						case '!':
							foreground(27);
							break;
						case '@':
							foreground(28);
							break;
						case '#':
							foreground(29);
							break;
						case '$':
							foreground(30);
							break;
						case '%':
							foreground(31);
							break;
						case '^':
							foreground(16);
							break;
					}
					break;
				case 'r':
					i += 1;
					if (i > str.length) {
						break;
					}
					switch (str[i]) {
						case '0':
							background(0);
							break;
						case '1':
							background(1);
							break;
						case '2':
							background(2);
							break;
						case '3':
							background(3);
							break;
						case '4':
							background(4);
							break;
						case '5':
							background(5);
							break;
						case '6':
							background(6);
							break;
						case '7':
							background(7);
							break;
					}
					break;
				default:
					if (i === 1 && ext) {
						switch(str[i]) {
							case '-':
								answer_mail(parseInt(str.substr(2)), 10);
								return;
							case 'b':
								player.bank += parseInt(str.substr(2), 10);
								if (player.bank > 2000000000) {
									player.bank = 2000000000;
								}
								return;
							case 'E':
								player.exp += parseInt(str.substr(2), 10);
								if (player.exp > 2000000000) {
									player.exp = 2000000000;
								}
								tournament_check();
								return;
							case '{':
								player.laid += 1;
								tournament_check();
								return;
							case 'G':
								player.gold += parseInt(str.substr(2), 10);
								if (player.gold > 2000000000) {
									player.gold = 2000000000;
								}
								return;
							case ',':
								player.forest_fights += parseInt(str.substr(2), 10);
								if (player.forest_fights > 32000) {
									player.forest_fights = 32000;
								}
								return;
							case ':':
								player.pvp_fights += parseInt(str.substr(2), 10);
								if (player.pvp_fights > 32000) {
									player.pvp_fights = 32000;
								}
								return;
							case ';':
								player.hp += parseInt(str.substr(2), 10);
								if (player.hp > 2000000000) {
									player.hp = 2000000000;
								}
								return;
							case '+':
								player.cha = parseInt(str.substr(2), 10);
								return;
							case '}':
								player.cha += 1;
								if (player.cha > 32000) {
									player.cha = 32000;
								}
								return;
							case 'K':
							case 'k':
								player.kids += 1;
								if (player.kids > 32000) {
									player.kids = 32000;
								}
								return;
							case 'V':
								player.exp -= parseInt(str.substr(2), 10);
								if (player.exp < 1) {
									player.exp = 1;
								}
								return;
							case 'M':
								player.str += parseInt(str.substr(2), 10);
								if (player.str > 2000000000) {
									player.str = 2000000000;
								}
								return;
							case 'D':
								player.def += parseInt(str.substr(2), 10);
								if (player.def > 2000000000) {
									player.def = 2000000000;
								}
								return;
							case 'T':
								smile_mail(parseInt(str.substr(2), 10));
								return;
							case 'Y':
								kiss_mail(parseInt(str.substr(2), 10));
								return;
							case 'U':
								dinner_mail(parseInt(str.substr(2), 10));
								return;
							case 'I':
								sleep_mail(parseInt(str.substr(2), 10));
								return;
							case '?':
								to = parseInt(str.substr(2), 10);
								if (to > -1) {
									oop = player_get(to);
									if (oop.married_to === player.Record) {
										player.married_to = to;
										player.put();
									}
									else {
										if (to === -1) {
											player.married_to = -1;
											player.put();
										}
									}
								}
								file_remove(gamedir('quote'+player.Record+'.lrd'));
								return;
							case 'o':
								// TODO: This is extra fragile since it's holding the mail lock...
								file_remove(gamedir('quote'+player.Record+'.lrd'));
								to = parseInt(str.substr(2), 10);
								oop = player_get(to);
								if (oop.on_now) {
									sclrscr();
									sln('');
									sln('');
									foreground(15);
									sln('                             ** ONLINE BATTLE **');
									foreground(10);
									sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
									player.put();
									lln('  `0'+oop.name+' `2has challenged you to an Online Duel!');
									sln('');
									lw('  Do you accept? [`%N`2] : ');
									lwch = getkey().toUpperCase();
									if (lwch !== 'Y') {
										lwch = 'N';
									}
									sln(lwch);
									if (lwch === 'N') {
										ob_send_resp(oop, 'R');
										if (psock !== undefined) {
											psock.writeln('DoneOnlineBattle');
											if (psock.readln() !== 'OK') {
												throw('Out of sync with server after DoneOnlineBattle');
											}
										}
										ob_cleanup();
										break;
									}
									sln('');
									sln('  As is the tradition, you are allowed to completely heal yourself first.');
									sln('');
									if (psock === undefined) {
										fmutex(gamedir('fight'+player.Record+'.rec'), 'IN BATTLE\n');
									}
									player.hp = player.hp_max;
									foreground(15);
									if (random(10) < 5) {
										on_battle(oop, true);
									}
									else {
										ob_send_resp(oop, 'A');
										on_battle(oop, false);
									}
									ob_cleanup();
									if (player.hp < 1 || player.dead) {
										player.hp = 0;
										player.dead = true;
										player.online = false;
										player.put();
										exit(0);
									}
								}
								break;
							case 'P':
								to = parseInt(str.substr(2), 10);
								oop = player_get(to);
								sln('');
								get_state(false);
								if (player.married_to > -1 || state.married_to_seth === player.Record || state.married_to_violet === player.Record) {
									foreground(15);
									sln('  BECAUSE YOU ARE ALREADY MARRIED, YOU ARE FORCED TO DECLINE!');
									sln('');
									mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
									    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-\n'+
									    '`0  '+player.name+'`2 is already married, and cannot say yes.');
									log_line('`%** `0'+player.name+' `2 had to decline `0'+oop.name+'`2\'s marriage proposal! `%**');
									more_nomail();
								}
								if (oop.married_to > -1 || state.married_to_seth === oop.Record || state.married_to_violet === oop.Record) {
									lln('  `%'+oop.name+' `0is already married!  No is the only answer.');
									sln('');
									mail_to(to, '`%  ** Romantic Mail Return From '+player.name+'`% **\n'+
									    '`2-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-'+
									    '`0  '+player.name+' `2refuses to marry you!  You are already married!');
									log_line('`%** `0'+player.name+' `2decides not to marry a married person. `%**');
									more_nomail();
								}
								marry_mail(to);
								return;
							case 'S':
								switch (player.clss) {
									case 1:
										player.skillw += 1;
										if (player.skillw > 40) {
											player.skillw = 40;
										}
										break;
									case 2:
										player.skillm += 1;
										if (player.skillm > 40) {
											player.skillm = 40;
										}
										break;
									case 3:
										player.skillr += 1;
										if (player.skillr > 40) {
											player.skillr = 40;
										}
										break;
								}
								return;
						}
					}
			}
		}
		else {
			snip += str[i];
		}
	}
	sw(snip);
}

function quit_msg()
{
	sclrscr();
	sln('');
	switch (random(5)) {
		case 0:
			lln('`2  The very earth groans at your depature.');
			return;
		case 1:
			lln('`2  The very trees seem to moan as you leave.');
			return;
		case 2:
			lln('`2  Echoing screams fill the wastelands as you close your eyes.');
			return;
		case 3:
			lln('`2  The black thing inside rejoices at your departure.');
			return;
		case 4:
			lln('`2  Your very soul aches as you wake up from your favorite dream.');
			return;
	}
}

function lrdfile(fname, more)
{
	var ln;
	var mc = morechk;

	if (more === undefined) {
		more = false;

	}

	if (txtindex[fname] === undefined) {
		return;

	}
	txtfile.position = txtindex[fname];

	if (!more) {
		morechk = false;

	}
	while(true) {
		ln = txtfile.readln(65535);
		if (ln === null) {
			break;
		}
		if (ln.search(/^@#/) === 0) {
			break;
		}
		lln(ln);
	}
	morechk = mc;
	curlinenum = 1;
}

function instructions()
{
	lrdfile('HINTS', true);
	more_nomail();
	sln('');
}

function disp_len(str)
{
	var len = 0;
	var i;

	for (i = 0; i < str.length; i += 1) {
		if (str[i] === '`') {
			i += 1;
			if (str[i] === 'r') {
				i += 1;
			}
		}
		else {
			len += 1;
		}
	}

	return len;
}

function disp_str(str)
{
	var ret = '';
	var i;

	for (i = 0; i < str.length; i += 1) {
		if (str[i] === '`') {
			i += 1;
			if (str[i] === 'r') {
				i += 1;
			}
		}
		else {
			ret += str[i];
		}
	}

	return ret;
}

function center(str)
{
	var spaces = '                                        ';
	return spaces.substr(0,(80-disp_len(str))/2)+str;
}

function check_name(str)
{
	var resp = '';

	str = disp_str(str).toUpperCase();
	
	switch (str) {
		case 'BARAK':
			resp = 'Naw, the real Barak would decapitate you if he found out.';
			break;
		case 'SETH':
			resp = 'You are not Seth Able!  Don\'t take his name in vain!';
			break;
		case 'SETH ABLE':
			resp = 'You are not God!';
			break;
		case 'TURGON':
			resp = 'Haw.  Hardly - Turgon has muscles.';
			break;
		case 'VIOLET':
			resp = 'Haw.  Hardly - Violet has breasts.';
			break;
		case 'RED DRAGON':
			resp = 'Oh go plague some other land!';
			break;
		case 'DRAGON':
			resp = 'You ain\'t Bruce Lee, so get out!';
			break;
		case 'JENNIE GARTH':
			resp = 'You are not a goddess, don\'t use her name!';
			break;
		case 'KIRSTEN DUNST':
			resp = 'Hardly! You only wish you were in a movie with Wynona!';
			break;
		case 'BUSH':
			resp = 'Lower my taxes!';
			break;
		case 'BAGGIO':
			resp = 'Darius sucks!';
			break;
		case 'DAVID FOLLEY':
			resp = 'You rule, dude - but use a handle or you will mobbed!';
			break;
		case 'ARNOLD PALMER':
			resp = 'Ha!  You\'re too old to be playing games.';
			break;
		case 'BARTENDER':
			resp = 'Nah, the bartender is smarter than you!';
			break;
		case 'CHANCE':
			resp = 'Why not go take a chance with a rattlesnake?';
			break;
		case 'MICHAEL PRESLAR':
			resp = 'You want to be a small town kid?';
			break;
		case 'GOD':
		case 'JESUS':
			resp = 'Why arent you in church?';
			break;
	}

	if (resp !== '') {
		sln('');
		lln('  `)** `%' + resp + ' `)**`2');
		sln('');
		return false;
	}

	return true;
}

function choose_profession(dh_tavern)
{
	var ch;

	if (dh_tavern) {
		lln('  `2You concentrate deeply.');
	}
	else {
		lln('  `%As you remember your childhood, you remember...');
		sln('');
		lln('  `0(`5K`0)illing A Lot Of Woodland Creatures');
		lln('  `0(`5D`0)abbling In The Mystical Forces');
		lln('  `0(`5L`0)ying, Cheating, And Stealing From The Blind');
	}

	do {
		sln('');
		lw('  `2Pick one.  (`0K`2,`0D`2,`0L`2) : `%');
		ch = getkey().toUpperCase();
		sln(ch);
		foreground(10);
		sln('');
		sln('');
		switch (ch) {
			case 'K':
				sln('  Now that you\'ve grown up, you have decided to study the ways of the');
				sln('  the Death Knights.  All beginners want the power to use their body');
				sln('  and weapon as one.  To inflict twice the damage with the finesse only');
				sln('  a warrior of perfect mind can do.');
				break;
			case 'D':
				sln('  You have always wanted to explain the unexplainable.  To understand the');
				sln('  powerful forces that rule the earth.  To tame the beast that oversees');
				sln('  all things.  Of course, having the power to burn someone by making a');
				sln('  gesture wouldn\'t hurt.');
				break;
			case 'L':
				sln('  You decide to follow your instincts.  To get better at what you\'ve');
				sln('  always done best.  So you decide to lead a dishonest lifestyle.');
				sln('  Of course, your ultimate goal will always be to join the Master Thieves');
				sln('  Guild.  To arrive, remember one thing, "Even thieves have honor."');
				break;
			default:
				sln('');
				sln('  This is a very important choice.  Pay attention idiot!');
		}
	} while ('KDL'.indexOf(ch) === -1);
	more_nomail();
	return ' KDL'.indexOf(ch);
}

function find_player()
{
	var name;
	var ucname;
	var tname;
	var op;
	var i;
	var ch;
	var plen;

	sln('');
	lln('  `2(full or `0PARTIAL`2 name)');
	lw('  NAME: `%');
	name = clean_str(dk.console.getstr());
	ucname = disp_str(name).toUpperCase();
	plen = player_length();
	for (i = 0; i < plen; i += 1) {
		op = player_get(i);
		if (op.name === 'X')
			continue;
		tname = disp_str(op.name).toUpperCase();
		if (tname.indexOf(ucname) !== -1) {
			sln('');
			lw('  `2You mean "`0' + op.name + '"? `2[`%Y`2] `: ');
			ch = getkey().toUpperCase();
			if (ch !== 'N') {
				ch = 'Y';
			}
			sln(ch);
			if (ch === 'Y') {
				sln('');
				return op.Record;
			}
		}
	}
	return -1;
}

function getstr(x, y, len, c, c1, str, mode)
{
	var oa = dk.console.attr.value;
	var ret;

	if (mode === undefined) {
		mode = {};

	}
	mode.len = len;
	mode.edit = str;
	mode.attr = new Attribute(c<<4 | c1);
	mode.input_box = true;
	mode.crlf = false;
	if (str.length > 0) {
		mode.select = true;
	}
	curlinenum = 1;
	if (x !== 0 && y !== 0) {
		dk.console.gotoxy(x-1, y-1);
	}
	background(c);
	foreground(c1);
	ret = dk.console.getstr(mode);
	dk.console.attr.value = oa;
	return ret;
}

function romance(prompt, mopts, fopts, to, shorttxt, mailtxt, kind)
{
	var line;

	sw(prompt);
	if (player.sex === 'M') {
		line = mopts[random(mopts.length)];
	}
	else {
		line = fopts[random(fopts.length)];
	}

	line = clean_str(getstr(0, 0, 53, 1, 15, line));
	sln('');
	sln('');

	if (line.length < 5) {
		sln(shorttxt);
		return false;
	}

	foreground(15);
	sln('  ** WRITING ROMANTIC MAIL, PLEASE WAIT **');
	line = '`0  "'+line+'`0"';
	mail_to(to, ' \n'+
	    '  `2Romantic Message From `0' + player.name + '`2!\n'+
	    '`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2\n'+
	    mailtxt+'\n'+
	    '  `2\n'+
	    line+'\n'+
	    '`'+kind+player.Record);
	return true;
}

function compose_mail()
{
	var to;
	var op;
	var ppronoun;
	var ppronoun2;
	var uppronoun;
	var pronoun;
	var upronoun;
	var ch;
	var sent = false;
	var propose_log = ['`0  '+player.name+' `2has been smitten - with love.', '`0  '+player.name+' `2is sick - lovesick, that is.'];

	sln('');
	sln('  Who would you like to send mail to?');
	to = find_player();
	if (to === -1) {
		foreground(15);
		sln('  No matching names found.');
	}
	else {
		op = player_get(to);
		if (player.sex !== op.sex && !player.flirted) {
			pronoun = op.sex === 'M' ? 'him' : 'her';
			ppronoun = player.sex === 'M' ? 'him' : 'her';
			ppronoun2 = player.sex === 'M' ? 'his' : 'her';
			uppronoun = player.sex === 'M' ? 'He' : 'She';
			upronoun = op.sex === 'M' ? 'Him' : 'Her';
			lw('  `2Say something `0Romantic`2 to ' + pronoun + '`2? : `2[`5N`2] `0: ');
			ch = getkey().toUpperCase();
			if (ch !== 'Y') {
				ch = 'N';
			}
			sln(ch);
			sln('');
			if (ch === 'Y') {
				lln('`c                     `%** ROMANTIC MESSAGE **');
				lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				lln('  `0Your hand trembles as you try to conjure up something to stir');
				lln('  your beloved ' + op.name + '.');
				sln('');
				show_looks(op);
				more_nomail();
				lln('  `2(`0N`2)ever mind.');
				lln('  `2(`0F`2)latter '+upronoun);
				lln('  `2(`0A`2)sk For A Kiss');
				lln('  `2(`0B`2)uy '+upronoun+' Dinner');
				if (!settings.clean_mode) {
					lln('  `2(`0I`2)nvite '+upronoun+' To Your Room At The Inn');
				}
				if (player.cha > 99) {
					lln('  `2(`0P`2)ropose To '+upronoun);
				}
				sln('');
				lw('  `2Which way would you like to show your love? : ');
				ch = getkey().toUpperCase();
				if ('FABIP'.indexOf(ch) === -1) {
					ch = 'N';
				}
				if (settings.clean_mode && ch === 'I') {
					ch = 'N';
				}
				if (player.cha < 100 && ch === 'P') {
					ch = 'N';
				}
				sln(ch);
				sln('');
				switch (ch) {
					case 'F':
						sent = romance('  Flattering Text? :', [
							'I like, um, think you are, uh, nice?',
							'You are just as cute as my dog!',
							'You have like, big uh, thingies.',
							'Your lips are like roses.',
							'May I drink in your beauty?  You\'re a tankard of joy!',
							'I\'d pick up yer hankie anywheres! Hyuck!'],
							['I like, um, think you are, uh, nice?',
							'You are just as cute as my dog!',
							'I like the way you kill people.',
							'I\'ve been watching you for a while...',
							'Might I let you know that you are handsome?',
							'Ya wanna do something sometime, somewhere?'],
							to, '  Somehow you don\'t think that will turn '+pronoun+' on.', 
							'  `2'+player.name+' is flirting with you!', 'K');
						player.flirted = sent;
						break;
					case 'A':
						sent = romance('  By saying what? :', [
							'Gimmie a kiss, woman!',
							'Git over here, you!',
							'Please give me the honor of touching your lips.',
							'I\'m an explorer.  Can I explore your mouth?',
							'Kiss me, I use a mouth wash!',
							'Uh, if you kiss me, I\'ll turn back into a prince.'],
							['Kiss me you big hunk oh man!',
							'Pleeeeeeeeease kiss me!',
							'I\'m an adventurer.  Can I explore your mouth?',
							'If you kiss me, our relationship might grow.',
							'Kiss me, lover!  I brush regularly.',
							'Can I teach you a french custom I know?'],
							to, '  Somehow you don\'t think that will turn '+pronoun+' on.',
							'  `2'+player.name+' wants you to kiss '+ppronoun+'!','Y');
						player.flirted = sent;
						break;
					case 'B':
						sent = romance('  And just how? :', [
							'Please let me buy you dinner.  It will be good.',
							'Let me take you out - I won\'t expect you to put out!',
							'No obligations - Just dinner, baby.',
							'You ever eat at the Red Dragon Inn?  It\'s on me.',
							'Come on sweet thing, I know you wanna eat!',
							'Please?  I\'ll cook Dragon for ya!'],
							['I\'m not a feminist, but I\'ll pay for it!',
							'Pleeeeeeeeease let me buy you a warm meal.',
							'I\'ll cook it myself!  A woman\'s place is the kitchen!',
							'If you\'re not busy, I\'d REALLY apreciate the company.',
							'I\'ll give you a real meal - Not crap.',
							'If you clean your plate, I\'ll even dance for you!'],
							to, '  Somehow you don\'t think that will turn '+pronoun+' on.',
							'  `2'+player.name+' wants to treat you to dinner!','U');
						player.flirted = sent;
						break;
					case 'I':
						sent = romance('  Worded how? :', [
							'Um, do you uh, um, think you could, uh.. wanna do it?',
							'Glorious girl.  Please be my valentine.  Tonight.',
							'Come take a ride on the wild stallion, baby!',
							'Make me a man tonight, honey!  Pleeeeeease?!',
							'I know you want my body, I know you think I\'m sexy...',
							'I\'ve been waiting to ask you all my life for this.'],
							['I\'ll pleasure you like no one has ever pleasured a man.',
							'Say yes, honey.  I swear I\'ll be here in the morning.',
							'Say yes.  Your body says yes, listen to it.',
							'Do it!  I swear I\'ve been tested recently..I think?',
							'You\'ll never regret a night spent with me, studmuffin.',
							'If you say no, I\'ll never ask again!'],
							to, '  Somehow you don\'t think that will turn '+pronoun+' on.',
							'  `2'+player.name+'`2 wants you to join '+ppronoun+' in a night of\n  `2unbridled passion, in '+ppronoun2+' room at the Inn.','I');
						player.flirted = sent;
						break;
					case 'P':
						if (player.cha > 99) {
							get_state(false);
							if (player.married_to > -1 || state.married_to_seth === player.Record || state.married_to_violet === player.Record) {
								sln('');
								sln('  Er - you sort of ARE married.  That kind of puts a crink in your');
								sln('  romantic desires, now doesn\'t it?');
								sln('');
								more_nomail();
							}
							else if (op.married_to > -1 || state.married_to_seth === op.Record || state.married_to_violet === op.Record) {
								sln('');
								lln('  `)PROBLEM! `2 You cannot marry a married person!  No way!');
								sln('');
								more_nomail();
							}
							else {
								sent = romance('  Worded how? :',
									['Would you do me the honor of being my wife?',
									'I love you, '+op.name+'.  Marry me.',
									'Be my wife.  Make me the happiest man in the world.',
									'Please - I\'ve watched you for the longest time..',
									'Say yes!  It\'s such a wonderful easy word!',
									'I cannot live another day without you.'],
									['Would you do me the honor of being my husband?',
									'I love you, '+op.name+'.  Marry me.',
									'Be mine.  Make me the happiest woman in the world.',
									'Please - I\'ve watched you for the longest time..',
									'Marry me...So I can say "I gotta man!"...',
									'I\'ve put my heart on my sleeve.  Don\'t break it.'],
									to, '  Somehow you don\'t think that will convince '+pronoun+'...',
									'  `2'+player.name+' has publicly declared '+ppronoun2+' love for\n  `2you.  '+uppronoun+' is asking for your hand in marriage.','P');
								player.flirted = sent;
								if (sent) {
									log_line(propose_log[random(propose_log.length)]);
								}
							}
						}
						break;
				}
			}
			player.put();
		}
		sln('');
		if (!sent) {
			write_mail(to, false);
		}
	}
}

function have_baby()
{
	var op;
	var n;
	var nm;
	var mstr;

	sln('');
	lln('  `0** `%YOU FEEL SICK THIS MORNING! `0**');
	sln('');
	more_nomail();
	lln('  `2You head to the healers in dismay, what can it be?');
	sln('');
	more_nomail();
	sclrscr();
	sln('');
	sln('');
	foreground(15);
	sln('  Healers');
	foreground(1);
	sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	foreground(2);
	lln('  `0"Madam!  You are with child, and it\'s due right now!"');
	foreground(2);
	sln('  Nathan, an elderly healer yells.');
	sln('');
	more_nomail();
	sln('  At this moment, you feel something ripping your insides out.  You');
	sln('  realize that it\'s childbirth pain.');
	sln('');
	more_nomail();
	sln('  Then the fun begins.  Three hours of sweat and screaming later, it');
	sln('  is over.  Nathan hands you a small bundle with tears in his eyes.');
	sln('');
	more_nomail();
	lw('  `0The baby is `%');
	n = random(20) + 1;
	if (n === 20) {
		lln('`4Not breathing`%.');
		sln('');
		more_nomail();
		foreground(2);
		sln('  Your pain cannot be expressed in words.  You feel as though you');
		sln('  have lost your own heart.  From this day on, everytime you think of');
		sln('  this small helpless child who never got a chance to live, you cry.');
		sln('');
		sln('  Live is precious.  Period.');
		sln('');
		if (player.married_to === -1) {
			log_line('  `0'+player.name+' `2is grief stricken over a great loss.');
		}
		else {
			op = player_get(player.married_to);
			log_line('  `0'+player.name+' `2and `0'+op.name+' `2are grief stricken.');
			mail_to(player.married_to,'  `0TRAGEDY STRIKE.\n'+
			    '  `2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n'+
			    '  `0Your wife had a miscarriage today.  You feel you\n'+
			    '  `0should console her.\n'+
			    '`|');
		}
	}
	else if (n < 10) {
		lln('`%A beautiful baby boy!');
		sln('');
		more_nomail();
		player.kids += 1;
		foreground(2);
		sln('  What would you like to name this child?');
		sw('  Name :');
		nm = clean_str(getstr(0,0,40,1,15,'')).trim();
		mstr = '  `0'+player.name+' `2gives birth to a boy!  His name is `%'+nm+'`2.';
		if (player.married_to > -1) {
			op = player_get(player.married_to);
			mstr += '\n  `0'+op.name+' `2is extremely proud.';
		}
		log_line(mstr);
		sln('');
		sln('');
		if (player.married_to === -1) {
			lln('  `2Oddly, he seems to  look a little like `%The Bard`2...');
		}
		else {
			lln('  `2He looks just like `0'+op.name+'`2.');
			mstr = '  `0JOYOUS OCCASION!\n';
			mstr += '  `%-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n';
			mstr += '  `2Your wife had a baby `%boy `2today!  Her face glows\n';
			mstr += '  `2like only a mothers can.  This baby is as much yours,\n';
			mstr += '  `2so you aspire to teach little `%'+nm+'`2 much.\n';
			mstr += '`K\n';
			mstr += '`|';
			mail_to(player.married_to, mstr);
		}
		sln('');
	}
	else {
		lln('`%A beautiful baby girl!');
		sln('');
		more_nomail();
		player.kids += 1;
		foreground(2);
		sln('  What would you like to name this child?');
		lw('  `2Name :');
		nm = clean_str(getstr(0,0,40,1,15,'')).trim();
		mstr = '  `0'+player.name+' `2gives birth to a girl!  Her name is `%'+nm+'`2.';
		if (player.married_to > -1) {
			op = player_get(player.married_to);
			mstr += '\n  `0'+op.name+' `2is pleased.';
		}
		log_line(mstr);
		if (player.married_to > -1) {
			mstr = '  `0JOYOUS OCCASION!\n';
			mstr += '  `%-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n';
			mstr += '  `2Your wife had a baby `)girl `2today!  Her face glows\n';
			mstr += '  `2like only a mothers can.  This baby is as female, but\n';
			mstr += '  `2you hope to make a warrior out of `%'+nm+'`2 yet.\n';
			mstr += '`K\n';
			mstr += '`|';
			mail_to(player.married_to, mstr);
		}
		sln('');
		sln('');
		foreground(2);
		sln('  You can tell she will be a heartbreaker.');
		sln('');
	}
}

function check_marriage()
{
	var op;
	var mstr;
	var ch;
	var good;
	var bad;
	var good_mood = ['Happy', 'Wonderful', 'Joyous', 'Excited', 'Loving',
	    'Lucky', 'Providential', 'Felicitous', 'Glad', 'Light-hearted'];
	var bad_mood = ['Angry','Resentful','Disgusted','Revolted','Repulsed',
	    'Sickened','Nauseated','Betrayed','Let Down','Cheated On'];

	if (state.married_to_seth === player.Record) {
		return;

	}
	if (state.married_to_violet === player.Record) {
		return;
	}
	if (player.married_to < 0) {
		return;
	}
	op = player_get(player.married_to);
	if (op.name === 'X') {
		sln('');
		lln('  `2Your lover has been declared MISSING!');
		sln('');
		lln('  `%YOU FEEL HEARTBROKEN!  (CHARM DROPS 50%)');
		sln('');
		player.married_to = -1;
		player.cha = parseInt(player.cha / 2, 10);
		more_nomail();
		return;
	}
	lln('  `2You notice `0'+op.name+'`2 is asleep in your bed.');
	sln('');
	lln('  `%You feel...');
	sln('');
	good = good_mood[random(good_mood.length)];
	bad = bad_mood[random(bad_mood.length)];
	lln('  `2(`01`2) `%'+good);
	lln('  `2(`02`2) `%'+bad);
	sln('');
	lw('  `0Choose.  [`21`0] :`%');
	ch = getkey();
	if (ch !== '2') {
		ch = 1;
	}
	sln(ch);
	sln('');
	if (ch === 1) {
		lln('  `2You smile at `%'+op.name+'`2.  Life is good.');
		sln('');
	}
	else {
		lln('  `2You scowl at the snoring figure in your bed.  Suddenly');
		sln('  you feel your rage boiling over!');
		sln('');
		more_nomail();
		lln('  You kick '+op.name+' `2into consciousness.');
		sln('');
		more_nomail();
		if (player.sex === 'M') {
			lln('  The woman awakes in confusion.  `0"Get out, woman." `2you tell her');
			sln('  harshly.');
			sln('');
			sln('  She slaps you in the face and walks out.');
			sln('');
			lln('  `%YOU FEEL FED UP WITH WOMEN!  (CHARM `)DROPS`% TO 50)');
		}
		else {
			lln('  The pig awakes in confusion.  `0"Get out of my house." `2you tell him');
			sln('  coldly.');
			sln('');
			sln('  A short while later, he stands outside your door, naked, gawking in');
			sln('  disbelief. ');
			sln('');
			lln('  `%YOU FEEL FED UP WITH MEN!  (CHARM `)DROPS`% 50%)');
		}
		sln('');
		log_line('`0  '+player.name+' `2has `)DIVORCED `0'+op.name+'`2!');
		player.divorced = true;
		player.cha = parseInt(player.cha / 2, 10);
		if (psock !== undefined) {
			psock.writeln('Divorce '+player.Record+' '+op.Record);
			if (['Yes', 'No'].indexOf(player.Record) === -1) {
				throw('Out of sync with server after Divorce');
			}
		}
		else {
			op.reLoad(true);
			op.married_to = -1;
			op.put(false);
		}
		player.married_to = -1;
		mstr = '`*\n';
		mstr += '  `)BAD NEWS!\n';
		mstr += '`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n';
		mstr += '  `0'+player.name+' `2has `4DIVORCED `2you!\n';
		mstr += '\n';
		mstr += '  `0(CHARM DROPS 50%)';
		mail_to(op.Record, mstr);
	}
}

function wake_up()
{
	var mstr;
	var vds = ['AIDS!','HERPES!','SYPHILIS!','GONORRHEA!','the HIV virus!'];
	var seth_support = ['  `0I love you, and still do not regret what we did. Nor will I ever.\n',
				    '  `0You must be strong - Do not let the gossip hurt you.\n',
				    '  `0Teach our young to be strong - To be like her mother.\n',
				    '  `0I am sending what I can - Not because of the law, I want to do it.\n'];
	var salutations = ['  `0My dearest ','  `0My lovely ','  `0My goddess ','  `0Dear '];
	var orig_dead = player.dead || (player.hp < 1);

	get_state(false);
	player.hp = player.hp_max;
	player.seen_master = false;
	player.tranferred_gold = 0;
	player.flirted = false;
	player.forest_fights = 15;
	player.seen_bard = false;
	player.last_reincarnated = state.days;
	player.time = state.days;
	player.pvp_fights = settings.pvp_fights_per_day;
	if (random(5) + 1 === 3) {
		player.weird = true;
	}
	player.seen_dragon = false;
	player.seen_violet = false;
	player.dead = false;
	player.leftbank = false;
	if (settings.forest_fights > 32000) {
		settings.forest_fights = 32000;
	}
	if (settings.forest_fights > 0 && settings.forest_fights <= 32000) {
		player.forest_fights = settings.forest_fights;
	}
	// do some other player maint ..  but not if theyve been gone for too long
	if (player.gone < settings.res_days) {
		if (settings.old_skill_points) {
			player.levelw = parseInt(player.skillw / 5, 10);
		}
		else {
			player.levelw = parseInt(player.skillw / 4, 10);
		}
		player.levelm = player.skillm;
		if (settings.old_skill_points) {
			player.levelt = parseInt(player.skillt / 5, 10);
		}
		else {
			player.levelt = parseInt(player.skillt / 4, 10);
		}
		if (player.clss === 1) {
			player.levelw += 1;
		}
		if (player.clss === 2) {
			player.levelm += 1;
		}
		if (player.clss === 3) {
			player.levelt += 1;
		}
		if (player.clss === 1 || player.clss === 3) {
			foreground(15);
			if (settings.old_skill_points) {
				sln('                 (Remember, 5 Skill Points = 1 Use Point)');
			}
			else {
				sln('                 (Remember, 4 Skill Points = 1 Use Point)');
			}
			sln('');
		}
		foreground(10);
		if (player.clss === 1) {
			lln('  For being a `%Death Knight`0, you get an extra `%Death Knight`0 use point!');
		}
		else if (player.clss === 2) {
			lln('  For being a `5Mystical Skills`0 student, you get an extra use point for today!'); }
		else if (player.clss === 3) {
			lln('  For being a Pilferer, you get an extra `%Thieving Skills`0 use point!'); }

		sln('');
		if (player.kids > 0) {
			foreground(2);
			sw('  You are inspired by your child');
			if (player.kids === 1) {
				sln('.');
			}
			else {
				sln('ren.');
			}
			sln('');
			lln('  `%YOU RECEIVE `0'+pretty_int(player.kids)+' `%EXTRA FOREST FIGHTS FOR TODAY.');
			sln('');
			player.forest_fights += player.kids;
			if (player.forest_fights > 32000) {
				player.forest_fights = 32000;
			}
		}
		if (player.bank >= 2000000000) {
			lln('  `$No interest earned today.  The coins won\'t fit in the bank!');
			player.bank = 2000000000;
		}
		else {
			player.bank += parseInt(player.bank / 10, 10);
			if (player.bank > 2000000000) {
				player.bank = 2000000000;
				lln('  `0Wow! You have a lot of gold!');
			}
		}
	}
	else {
		player.gone = 0;
		lln('  `0The townspeople thought you had abandoned them!');
		sln('');
	}
	if (settings.sleep_dragon && orig_dead === false && (player.level > 12 || player.exp > 15000000)) {
		lln('`c  `2You awake to odd movement in the middle of the night...');
		sln('');
		lln('  It feels like you\'re on a boat, a slow rolling side to side lulling');
		lln('  you back to sleep...');
		sln('');
		more_nomail();
		lln('  Suddenly, you bolt awake!  You went to sleep '+(player.inn ? 'at the inn' : 'in the fields')+'!');
		sln('');
		more_nomail();
		lln('  The `4Red Dragon `2spits you out onto the ground in front of it.');
		lln('  Unprepared, you fumble for your '+player.weapon+'`2.');
		sln('');
		fight_dragon(true);
		more_nomail();
	}
	if (player.dead || player.hp < 1) {
		player.online = false;
		player.put();
	}
	else {
		if (player.sex === 'F' && player.kids > 3 && player.laid > 13) {
			if (player.married_to < 0 && state.married_to_seth !== player.Record) {
				if (random(5) === 3) {
					mstr = '  `0Seth Able `2sent you this...\n';
					mstr += '`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2\n';
					mstr += salutations[random(salutations.length)];
					mstr += player.name + ',\n';
					mstr += seth_support[random(seth_support.length)];
					mstr += '  `0\n';
					mstr += '  `0Visit me today, please.\n';
					mstr += ' \n';
					mstr += '  `%BANK NOTICE:\n';
					mstr += '`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2\n';
					mstr += '  `0Seth Able `2has transfered `%'+pretty_int(player.kids * 1000 * player.level)+' `2gold\n';
					mstr += '  to your account.\n';
					mstr += '`b'+(player.kids * 1000 * player.level)+'\n';
					mail_to(player.Record, mstr);
				}
			}
		}
		if (random(3) + 1 > 1) {
			lln('  `2You are in `0high `2spirits today.');
			player.high_spirits = true;
			sln('');
		}
		else {
			lln('  `2You are in `0low `2spirits today.');
			sln('');
			player.high_spirits = false;
		}
		if (player.horse) {
			lln('  `)** `%You ready your horse `)**`2');
			sln('');
			player.forest_fights += parseInt(player.forest_fights / 4, 10);
			if (player.forest_fights > 32000) {
				player.forest_fights = 32000;
			}
		}
		if (player.inn) {
			sln('  You wake up early, sheathe your weapon, and head down to the bar.');
		}
		else {
			sw('  You wake up early, strap your ');
			lw(player.weapon);
			foreground(2);
			sln(' to your back, and');
			sln('  head out to the Town Square, seeking adventure, fame, and honor.');
			sln('');
		}
		if (player.sex === 'F' && random(34)+1 === 11 && player.laid * 5 > player.kids) {
			have_baby();
		}
		check_marriage();
		if (player.laid > 50) {
			if (random(23) === 13) {
				sln('');
				lln('  `0** `%YOU FEEL SICK THIS MORNING! `0**');
				sln('');
				player.hp = 1;
				log_line('`0  '+player.name+' `2went to the healers to get cured of `4VD`2!');
				more_nomail();
				lln('  `2You head to the healers in dismay, what can it be?');
				sln('');
				more_nomail();
				sclrscr();
				sln('');
				sln('');
				foreground(15);
				sln('  Healers');
				foreground(1);
				sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				foreground(2);
				lln('  `0"My God!" `2Nathan, the lead healer screams.');
				sln('');
				more_nomail();
				lw('`%  You are diagnosed with ');
				sln(vds[random(vds.length5)]);
				sln('');
				lln('  `2You are fairly sure you didn\'t get it off a toilet seat.');
				sln('');
				more_nomail();
				lln('  `2Luckily, Nathan has a potion that can cure it.  This time.');
				sln('  However, you still feel quite weak from the experience.');
				sln('');
			}
		}
	}
}

function space_pad(str, len, left)
{
	if (left === undefined) {
		left = false;
	}

	while (disp_len(str) < len) {
		if (left) {
			str = ' ' + str;
		}
		else {
			str += ' ';
		}
	}
	return str;
}

function psock_igm_lines(who)
{
	var resp;

	psock.writeln('GetIGM '+who);
	resp = psock.readln();
	resp = resp.match(/^IGMData ([0-9]+)$/);
	if (resp === null) {
		throw('Invalid response to GetIGM');
	}
	resp = psock.read(parseInt(resp[1], 10)).split(/\r?\n/);
	if (resp.length !== 2) {
		throw('Invalid response to GetIGM');
	}
	if (psock.read(2) !== '\r\n') {
		throw('Out of sync with server after GetIGM');
	}
	return resp;
}

function warriors_on_now(inhello)
{
	var i;
	var on = [];
	var out;
	var where;

	sln('');
	foreground(15);
	background(0);
	sln('                         Warriors in the Realm Now');
	lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	all_players().forEach(function(op) {
		if (op.name !== 'X' && op.on_now) {
			on.push(op);
		}
	});
	for (i = 0; i < on.length; i += 1) {
		out = new File(gamedir('out'+on[i].Record+'.lrd'));
		if (psock !== undefined) {
			where = psock_igm_lines(on[i].Record);
			if (where[0].length > 0) {
				if (where[1].length > 0) {
					lln('  `0'+on[i].name+'`2 "' + where);
				}
				else {
					lln('  `0'+on[i].name+'`2 is in `0"' + where + '`0".');
				}
			}
			else {
				lln('  `0'+space_pad(on[i].name, 25)+'   `2Arrived At`%                   '+on[i].time_on);
			}
		}
		else {
			fmutex(out.name);
			if (file_exists(out.name)) {
				if (!out.open('r')) {
					throw('Unable to open '+out.name);
				}
				where = out.readln();
				if (out.readln() === null) {
					out.close();
					rfmutex(out.name);
					lln('  `0'+on[i].name+'`2 is in `0"' + where + '`0".');
				}
				else {
					out.close();
					rfmutex(out.name);
					lln('  `0'+on[i].name+'`2 "' + where);
				}
			}
			else {
				rfmutex(out.name);
				lln('  `0'+space_pad(on[i].name, 25)+'   `2Arrived At`%                   '+on[i].time_on);
			}
		}
		if (inhello && on[i].Record !== player.Record) {
			mail_to(on[i].Record, '`0  '+player.name+' `2has entered the realm.');
		}
	}
}

function show_log()
{
	var ch;
	create_log(false);

	function do_log(today) {
		var start = new Date();
		var end;
		var resp;
		var m;
		var len;
		var log;

		start.setHours(0, 0, 0, 0);
		if (!today) {
			if (pfile !== undefined) {
				show_lord_file(gamedir('logold.lrd'), false, false);
			}
			else {
				end = start;
				end.setDate(start.getDate() - 1);
				psock.writeln('GetLogRange '+start.valueOf()+' '+end.valueOf());
				
			}
		}
		else {
			if (pfile !== undefined) {
				show_lord_file(gamedir('lognow.lrd'), false, false);
			}
			else {
				psock.writeln('GetLogFrom '+start.valueOf());
			}
		}
		if (pfile === undefined) {
			resp = psock.readln();
			m = resp.match(/^LogData ([0-9]+)$/);
			if (m === null) {
				throw('Unable to get LogData length from '+resp);
			}
			len = parseInt(m[1], 10);
			log = psock.read(len);
			if (psock.read(2) !== '\r\n') {
				throw('Out of sync after LogData');
			}
			show_lord_buffer(log, false, false);
		}
	}

	do {
		sclrscr();
		sln('');
		sln('');
		do_log(true);
		do {
			sln('');
			foreground(2);
			lw('`2  (`5C`2)ontinue   (`5T`2)odays happenings again  (`5Y`2)esterdays `0[`5C`0] : ');
			ch = getkey().toUpperCase();
			if ('TY'.indexOf(ch) === -1) {
				ch = 'C';
			}
			sw(ch);
			if (ch === 'T') {
				break;
			}
			if (ch === 'Y') {
				if (pfile !== undefined && !file_exists(gamedir('logold.lrd'))) {
					sln('');
					sln('');
					sln('  Apparently nothing of importance happened yesterday.');
				}
				else {
					sclrscr();
					sln('');
					sln('');
					do_log(false);
				}
			}
		} while (ch !== 'C');
	} while (ch !== 'C');
	sln('');
}

function show_stats()
{
	var tp;

	sclrscr();
	sln('');
	lln('`%  ' + player.name + '`2\'s Stats...');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	lln('  `2Experience   : `0'+pretty_int(player.exp));
	lln('`2  Level        : `0'+space_pad(pretty_int(player.level),17)+' `2HitPoints          : `0'+pretty_int(player.hp)+'`2 of `0'+pretty_int(player.hp_max));
	lln('`2  Forest Fights: `0'+space_pad(pretty_int(player.forest_fights),17)+' `2Player Fights Left : `0'+pretty_int(player.pvp_fights));
	lln('`2  Gold In Hand : `0'+space_pad(pretty_int(player.gold),17)+' `2Gold In Bank       : `0'+pretty_int(player.bank));
	lln('`2  Weapon       : `0'+space_pad(player.weapon,17)+' `2Attack Strength    : `0'+pretty_int(player.str));
	lln('`2  Armour       : `0'+space_pad(player.arm,17)+' `2Defensive Strength : `0'+pretty_int(player.def));
	lln('`2  Charm        : `0'+space_pad(pretty_int(player.cha),17)+' `2Gems               : `0'+pretty_int(player.gem));
	sln('');
	get_state(false);
	if (state.married_to_violet === player.Record) {
		lln('  `2You are married to `#Violet`2.');
		sln('');
	}
	if (state.married_to_seth === player.Record) {
		lln('  `2You are married to `%Seth Able`2.');
		sln('');
	}
	if (player.married_to !== -1) {
		tp = player_get(player.married_to);
		if (tp !== undefined) {
			lln('  `2You are married to `%' + tp.name + '`2.');
			sln('');
		}
	}
	if (player.kids === 1) {
		lln('  `2You have `01`2 child.');
		sln('');
	}
	if (player.kids > 1) {
		lln('  `2You have `0'+pretty_int(player.kids)+'`2 children.');
		sln('');
	}
	if (player.horse) {
		lln('  `2You are on `%horseback`2.');
	}
	if (player.has_fairy !== undefined && player.has_fairy) {
		lln('  `2You have a `#fairy`2 in your pocket.');
	}
	if (player.levelw > 0 || player.skillw > 0) {
		lw('`2  Death Knight Skills: `0');
		if (player.skillw === 0) {
			sw('NONE       ');
		}
		else if (player.skillw >= 40) {
			sw('MASTERED   '); }
		else if (player.skillw > 0) {
			sw(space_pad(pretty_int(player.skillw),11)); }
		lln('`2 Uses Today: (`0'+pretty_int(player.levelw)+'`2)');
	}
	if (player.levelm > 0 || player.skillm > 0) {
		lw('`2  The Mystical Skills: `0');
		if (player.skillm === 0) {
			sw('NONE       ');
		}
		else if (player.skillm >= 40) {
			sw('MASTERED   '); }
		else if (player.skillm > 0) {
			sw(space_pad(pretty_int(player.skillm),11)); }
		lln('`2 Uses Today: (`0'+pretty_int(player.levelm)+'`2)');
	}
	if (player.levelt > 0 || player.skillt > 0) {
		lw('`2  The Thieving Skills: `0');
		if (player.skillt === 0) {
			sw('NONE       ');
		}
		else if (player.skillt >= 40) {
			sw('MASTERED   '); }
		else if (player.skillt > 0) {
			sw(space_pad(pretty_int(player.skillt),11)); }
		lln('`2 Uses Today: (`0'+pretty_int(player.levelt)+'`2)');
	}
	switch (player.clss) {
		case 1:
			lln('  `0You are currently interested in `%Death Knight`0 skills.');
			break;
		case 2:
			lln('  `0You are currently interested in `%The Mystical`0 skills.');
			break;
		case 3:
			lln('  `0You are currently interested in `%The Thieving`0 skills.');
			break;
	}
	if (player.amulet) {
		lln('  `2You are wearing an `%Amulet of Accuracy`2.');
		sln('');
	}
	more();
}

function hello()
{
	var op;
	var no = new File(gamedir('nodeon.'+dk.connection.node));
	var tmp;

	file_remove(gamedir('quote' + player.Record+'.lrd'));
	file_remove(gamedir('msg' + player.Record+'.lrd'));
	file_remove(gamedir('temp' + player.Record));

	if (settings.safe_node && psock === undefined) {
		if (file_exists(no.name)) {
			if (no.open('r')) {
				tmp = no.readln();
				if (tmp.search(/^[0-9]+$/) !== -1) {
					tmp = parseInt(tmp, 10);
					if (tmp !== player.Record) {
						op = player_get(tmp);
						op.on_now = false;
						op.put(false);
					}
				}
				no.close();
			}
		}
		if (no.open('w')) {
			cleanup_files.push(no.name);
			no.writeln(player.Record);
			no.close();
		}
	}

	sln('');
	// NOTE: Likely wrap checks... not needed.
	if (player.bank < 0) {
		player.bank = 0;
	}
	if (player.gold < 0) {
		player.gold = 0;
	}
	if (player.exp < 0) {
		player.exp = 0;
	}
	player.on_now = true;
	player.put();

	player.time_on = strftime('%I:%M', time());
	player.put();
	warriors_on_now(true);
	sln('');
	create_log(false);
	if (player.time === state.days) {
		foreground(2);
		sln('');
		sln('  You have already been on today.');
		if (player.inn) {
			sln('  You wake up early, sheathe your weapon, and head down to the bar.');
		}
		if (player.dead || player.hp < 1) {
			player.dead = true;
			lln('  `2You have been `4killed`2 today.  You will not be allowed');
			sln('  to play until TOMORROW.');
			player.on_now = false;
			player.put();
		}
		sln('');
	}
	else {
		wake_up();
	}
	if (player.married_to > -1) {
		op = player_get(player.married_to);
		if (op.name === 'X') {
			sln('');
			lln('  `2Your lover has been declared MISSING!');
			sln('');
			lln('  `%YOU FEEL HEARTBROKEN!  (CHARM DROPS 50%)');
			sln('');
			player.married_to = -1;
			player.cha = parseInt(player.cha / 2, 10);
			more_nomail();
		}
	}
	more_nomail();
	show_stats();
	if (mail_check()) {
		sln('');
		lln('  `%You have mail today!');
		sln('');
		more_nomail();
		check_mail();
	}
	show_log();
	if (player.clss === 0) {
		sln('');
		sln('');
		sln('  ** You feel listless, you need a goal in life **');
		player.clss = choose_profession();
	}
	player.put();
}

function new_player()
{
	var name;
	var np = {};
	var tp;
	var i;
	var ch;
	var plen;

	function np_to_player(newp) {
		Object.keys(newp).forEach(function(npkey) {
			player[npkey] = newp[npkey];
		});
	}

	sclrscr();
	if (player_length() > 147) {
		sln('');
		lln('`c  `%                      ** THE REALM IS FULL **');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		lln('  `2An over-population problem is keeping you from moving in.');
		sln('');
		lln('  `0Take heart. `2 Although `%LORD '+ver+'`2 is NEVER reset, players who do not');
		sln('  visit for a certain number of days are deleted every morning.');
		sln('');
		sln('  (You may want to ask your Sysop to make the # of days less)');
		sln('');
		sln('  Please try again tomorrow.  Until then, you can only dream of the');
		sln('  wonderful world that awaits...');
		sln('');
		more_nomail();
		exit(0);
	}
	sln('');
	sln('');
	sln('                      ** Welcome to the realm new warrior! **');
	ch = '';
	do {
		sln('');
		sln('');
		lln('  `0What would you like as an alias? ');
		lln('');
		lw('  `2Name: `%');
		name = dk.console.getstr({len:20});
		if (name === '') {
			sln('');
			lln('  `)YOU\'RE NOTHING BUT A WEENIE!');
			sln('');
			player.on_now = false;
			player.put();
			exit(0);
		}
		foreground(10);
		name = name.trim();
		name = clean_str(name);
		np.name = name;
		if (name.length > 18) {
			sln('  Try a shorter name!');
			sln('');
		}
		else if (name.length < 3) {
			sln('  Try a longer name!');
			sln('');
		}
		else if (disp_len(name) < 3) {
			sln('  You are such a sneaky boy.  Yet so simplistic.');
			sln('');
		}
		else {
			plen = player_length();
			for (i = 0; i < plen; i += 1) {
				tp = player_get(i);
				if (disp_str(tp.name) === disp_str(name)) {
					sln('');
					lln('  `2You recall hearing that another warrior currently goes by that name.  Not');
					lln('  wanting to dishonor `0'+tp.name+' `2you decide to pick another.');
					sln('');
					break;
				}
			}
			if (i === plen) {
				if (check_name(name)) {
					lw('  `0' + name + '`2? `2[`0Y`2] : `%');
					ch = getkey().toUpperCase();
					if (ch !== 'N') {
						ch = 'Y';
					}
					foreground(15);
					sln(ch);
					foreground(10);
				}
			}
		}
	} while(ch !== 'Y');
	// TODO: Race condition in name... it's checked for dupes when you enter it, not at creation.
	lw('  `2And your gender?  (`0M`2/`0F`2) [`0M`2]: `%');
	ch = getkey().toUpperCase();
	if (ch !== 'F') {
		ch = 'M';
	}
	sln(ch);
	np.sex = ch;
	if (ch === 'M') {
		switch (random(5)) {
			case 0:
				lln('  With a name like "' + np.name + '`0", no one is going to believe it.');
				break;
			case 1:
				sln('  ALL RIGHT!!  A member of the more ADVANCED sex.  You had better win.');
				break;
			case 2:
				sln('  Good.  Men rule this earth.  We own and run EVERYTHING.');
				break;
			case 3:
				sln('  Then don\'t be wearing any dresses, eh.');
				break;
			case 4:
				sln('  Very good.  If a woman ever beats you in battle, go into exile.');
				break;
		}
	}
	else {
		switch (random(5)) {
			case 0:
				sln('  Good.  Teach those men that they do NOT rule the world.');
				break;
			case 1:
				sln('  ALL RIGHT!!  A member of the more ADVANCED sex.  You had better win.');
				break;
			case 2:
				sln('  Excellent.  Taunt the men, tease them, and break their hearts!');
				break;
			case 3:
				sln('  Be warned, you are going to have to fight, kill and maim here.');
				break;
			case 4:
				sln('  Good.  There are way too many men in this land..');
				break;
		}
	}
	sln('');
	np.clss = choose_profession(false);
	plen = player_length();
	for (i = 0; i < plen; i += 1) {
		player = player_get(i, true);
		if (player.name === 'X' && player.Yours) {
			player.reInit();
			np_to_player(np);
			// TODO: This should be user.number since real name isn't unique!
			player.real_name = dk.user.full_name;
			player.put(false);
			break;
		}
		player.unLock();
	}
	if (i === plen) {
		player = player_new(true);
		np_to_player(np);
		// TODO: This should be user.number since real name isn't unique!
		player.real_name = dk.user.full_name;
		player.put(false);
	}
	if (settings.res_days > 0) {
		player.gone = 0;
	}
	else {
		player.gone = -1;
	}
	killmail(player.Record);
	get_state(true);
	// Can this even happen?  Isn't this done when the player is deleted?
	if (state.married_to_seth === player.Record) {
		state.married_to_seth = -1;
		if (psock !== undefined) {
			psock.writeln('SethMarried -1');
			if (['Yes','No'].indexOf(psock.readln()) === -1) {
				throw('Out of sync with server after SethMarried -1');
			}
		}
	}
	if (state.married_to_violet === player.Record) {
		state.married_to_violet = -1;
		if (psock !== undefined) {
			psock.writeln('VioletMarried -1');
			if (['Yes','No'].indexOf(psock.readln()) === -1) {
				throw('Out of sync with server after VioletMarried -1');
			}
		}
	}
	put_state();
	hello();
}

function load_player(create)
{
	var i;
	var ch;
	var op;
	var f;
	var plen;

	plen = player_length();
	for (i = 0; i < plen; i += 1) {
		player = player_get(i);
		// TODO: This should be user.number since real name isn't unique!
		if (player.real_name === dk.user.full_name && player.Yours) {
			break;
		}
	}
	// TODO: This should be user.number since real name isn't unique!
	if (player === undefined || player.real_name !== dk.user.full_name || !player.Yours) {
		if (create) {
			sclrscr();
			sln('');
			sln('');
			foreground(15);
			sln('  Joining The Game');
			foreground(10);
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			foreground(2);
			sln('  You have never visted the realm before.  Do you want to join this place');
			sln('  of Dragons, Knights, Magic, Friends & Foes and Good & Evil?');
			sln('');
			lln('  `2(`0Y`2)es.');
			lln('  `2(`0N`2)o.');
			sln('');
			foreground(10);
			lw('  `2Your choice?  `2[`0Y`2] : `%');
			ch = getkey().toUpperCase();
			if (ch !== 'N') {
				ch = 'Y';
			}
			sln(ch);
			if (ch !== 'Y') {
				quit_msg();
				exit(0);
			}
			new_player();
		}
	}
	else {
		player.on_now = true;
		player.put();
		if (pfile !== undefined) {
			f = new File(gamedir('battle'+player.Record+'.rec.lock'));
			if (file_exists(f.name)) {
				if (f.open('r')) {
					i = f.readln();
					f.close();
					if (i !== null) {
						op = player_get(parseInt(i, 10));
						lln('  `%'+op.name+'`2 is currently battling you');
						do {
							mswait(500);
							player.reLoad();
							op.reLoad();
							sw('\r');
							dk.console.print('\r');
							dk.console.cleareol();
							lw('Your HP: '+pretty_int(player.hp)+'\tHis HP: '+pretty_int(op.hp));
						} while (file_exists(f.name));
					}
				}
			}
		}
		else {
			psock.writeln('CheckBattle '+player.Record);
			i = psock.readln();
			if (i !== 'No') {
				if (i.search(/^[0-9]+$/) !== 0) {
					throw('Out of sync with server after CheckBattle');
				}
				op = player_get(parseInt(i, 10));
				lln('  `%'+op.name+'`2 is currently battling you');
				i = true;
				// TODO: Some sort of battle wait thing?
				do {
					mswait(500);
					player.reLoad();
					op.reLoad();
					sw('\r');
					dk.console.print('\r');
					dk.console.cleareol();
					lw('Your HP: '+pretty_int(player.hp)+'\tHis HP: '+pretty_int(op.hp));
					psock.writeln('CheckBattle');
					i = psock.readln();
					if (i === 'No') {
						break;
					}
					if (i.search(/^[0-9]+$/) !== 0) {
						throw('Out of sync with server after CheckBattle');
					}
				} while (true);
			}
		}
		hello();
	}
}

function build_txt_index()
{
	var l;

	txtfile = new File(game_or_exec('lordtxt.lrd'));
	if (!txtfile.open('r')) {
		sln('Unable to open '+txtfile.name+'!');
		exit(1);
	}

	while (true) {
		l = txtfile.readln();
		if (l === null) {
			break;
		}
		if (l.substr(0,2) === '@#') {
			txtindex[l.substr(2)] = txtfile.position;
		}
	}
}

function command_prompt()
{
	// Subtract 30 seconds from time remaining.
	var tl = (dk.user.seconds_remaining + dk.user.seconds_remaining_from - 30) - time();
	var remain = parseInt(tl / 60, 10)+':'+(tl % 60);

	sln('');
	lw('`2  Your command,`0 ' + player.name + '`2? `2[`%' + remain + '`2] : `%');
}

function get_a_room()
{
	var ch;

	sln('');
	sln('');
	foreground(2);
	sln('  The bartender approaches you at the mention of a room.');
	foreground(5);
	sln('  "You want a room, eh?  That\'ll be ' + pretty_int(400 * player.level) + ' gold!"');
	lw('  `2Do you agree? [`0Y`2] : `%');
	ch = getkey().toUpperCase();
	if (ch !== 'N') {
		ch = 'Y';
	}
	sln(ch);
	if (ch !== 'Y') {
		sln('');
		sln('  The bartender grunts, then walks away uninterested.');
		return false;
	}

	sln('');
	sln('');
	if (player.cha  > 99) {
		if (player.sex === 'M') {
			sln('  "You seem like a nice guy, and I hear you\'re tough, so tell you what,');
			sln('  I\'ll just give you the room for free...."');
		}
		else {
			sln('  "Hey good lookin\'!  I\'ll be glad to give you a freebie...');
			sln('  A free room I mean, of course.  Har!"');
		}
		sln('');
		more_nomail();
	}
	else {
		if (player.gold <  400 * player.level) {
			foreground(4);
			sln('  "Hey!  You stupid fool! You don\'t have that much gold!"');
			return false;
		}
		if (player.cha < 100) {
			player.gold -= 400 * player.level;
		}
	}

	player.inn = true;
	sclrscr();
	sln('');
	sln('');
	foreground(15);
	sln('  The Room At The Inn');
	foreground(10);
	sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	foreground(2);
	sln('  You are escorted to a small but cozy room in the Inn.');
	sln('  You relax on the soft bed, and soon fall asleep, still');
	sln('  wearing your armour.');
	sln('');
	sln('');
	player.on_now = false;
	player.put();
	exit(0);

	return true;
}

function add_hp(num)
{
	player.hp_max += num;
	if (player.hp_max > 32000) {
		player.hp_max = 32000;
	}
}

function add_str(num)
{
	player.str += num;
	if (player.str > 32000) {
		player.str = 32000;
	}
}

function add_def(num)
{
	player.def += num;
	if (player.def > 32000) {
		player.def = 32000;
	}
}

function lord_to_ansi(str)
{
	var ret = '';
	var i;

	for (i=0; i<str.length; i += 1) {
		if (str[i] === '`') {
			i += 1;
			switch(str[i]) {
				case 'c':
					ret += '\x1b[2J\x1b[H';
					break;
				case '1':
					ret += '\x1b[0;34m';
					break;
				case '*':
					ret += '\x1b[0;30;41m';
					break;
				case '2':
					ret += '\x1b[0;32m';
					break;
				case '3':
					ret += '\x1b[0;36m';
					break;
				case '4':
					ret += '\x1b[0;31m';
					break;
				case '5':
					ret += '\x1b[0;35m';
					break;
				case '6':
					ret += '\x1b[0;33m';
					break;
				case '7':
					ret += '\x1b[0;37m';
					break;
				case '8':
					ret += '\x1b[1;30m';
					break;
				case '9':
					ret += '\x1b[1;34m';
					break;
				case '0':
					ret += '\x1b[1;32m';
					break;
				case '!':
					ret += '\x1b[1;36m';
					break;
				case '@':
					ret += '\x1b[1;31m';
					break;
				case '#':
					ret += '\x1b[1;35m';
					break;
				case '$':
					ret += '\x1b[1;33m';
					break;
				case '%':
					ret += '\x1b[1;37m';
					break;
				case 'r':
					i += 1;
					switch(str[i]) {
						case 0:
							ret += '\x1b[40m';
							break;
						case 1:
							ret += '\x1b[44m';
							break;
						case 2:
							ret += '\x1b[42m';
							break;
						case 3:
							ret += '\x1b[43m';
							break;
						case 4:
							ret += '\x1b[41m';
							break;
						case 5:
							ret += '\x1b[45m';
							break;
						case 6:
							ret += '\x1b[47m';
							break;
						case 7:
							ret += '\x1b[46m';
							break;
					}
					break;
			}
		}
		else {
				ret += str[i];
		}
	}
	return ret;
}

function generate_rankings(fname, all, on, inn)
{
	var a = [];
	var i;
	var f = new File(fname);
	var l;

	if (all === undefined) {
		all = false;

	}
	if (on === undefined) {
		on = false;
	}
	if (inn === undefined) {
		inn = false;
	}

	all_players().forEach(function(p) {
		if (p.name !== 'X') {
			if (all || !p.dead) {
				if (inn) {
					if (p.inn) {
						a.push(p);
					}
				}
				else if (all || !p.inn) {
					a.push(p);
				}
			}
		}
	});
	a.sort(function(a,b) { return b.exp - a.exp; });
	fmutex(f.name);
	if (!f.open('wb')) {
		rfmutex(f.name);
		throw('Unable to open '+f.name);
	}
	f.writeln('');
	f.writeln('');
	f.writeln('`%'+center('Legend Of The Red Dragon '+ver+' - Player Rankings'));
	f.writeln('');
	f.writeln('`2    Name                    Experience    Level    Mastered    Status');
	f.writeln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	for (i = 0; i < a.length; i += 1) {
		l = '';
		if (a[i].sex === 'F') {
			l += '`#F ';
		}
		else {
			l += '  ';
		}
		switch(a[i].clss) {
			case 1:
				l += '`0D ';
				break;
			case 2:
				l += '`#M ';
				break;
			case 3:
				l += '`9T ';
				break;
			default:
				l += '  ';
		}
		l += '`2' + space_pad(a[i].name, 22) + '`2';
		l += space_pad(pretty_int(a[i].exp), 13, true);
		l += '    ';
		l += '`%';
		l += space_pad(pretty_int(a[i].level), 2);
		l += '        ';
		if (a[i].skillw > 39) {
			l += '`%D';
		}
		else if(a[i].skillw > 19) {
			l += '`0D'; }
		else {
			l += ' ';
		}
		if (a[i].skillm > 39) {
			l += '`%M';
		}
		else if(a[i].skillm > 19) {
			l += '`#M'; }
		else {
			l += ' ';
		}
		if (a[i].skillt > 39) {
			l += '`%T';
		}
		else if(a[i].skillt > 19) {
			l += '`9T'; }
		else {
			l += ' ';
		}
		l += '     ';
		if (a[i].dead) {
			l += '`4Dead ';
		}
		else {
			l += '`%Alive';
		}
		if (on && a[i].on_now) {
			l += '  `%On';
		}
		f.writeln(l);
	}
	f.close();
	rfmutex(f.name);
}

function enemy_attack(op, pfight)
{
	var tmp;
	var atk;
	var son;
	var his;

	if (pfight === undefined) {
		pfight = false;

	}

	atk = random(op.str / 2) + parseInt(op.str / 2, 10);
	if (op.is_dragon !== undefined && op.is_dragon) {
		switch (random(4)) {
			case 0:
				op.weapon = 'Huge Claw';
				break;
			case 1:
				op.weapon = 'Swishing Tail';
				break;
			case 2:
				op.weapon = '`4Flaming Breath';
				atk += atk;
				break;
			case 3:
				op.weapon = 'Stomping The Ground';
				break;
		}
	}
	if (random(30) === 1) {
		atk+= parseInt(atk/ 2, 10);
		lln('  `4** `0'+op.name+'`4 Executes A Power Move **');
		sln('');
	}
	atk = atk - player.def;
	if (atk < 1) {
		lln('  `%** `0'+op.name+'`2 misses you Completely! `%**');
		return;
	}
	if (player.kids > 5 && ((random(100) + 1) < (player.kids - 5))) {
		if (random(2) === 1) {
			son = 'son';
			his = 'him';
		}
		else {
			son = 'duaghter';
			his = 'her';
		}
		player.kids--;
		lln('  `0Your enemy is startled - someone is attacking him from behind!');
		// DIFF: Originally, this had an off-by-one where hold and hit were set the same...
		tmp = parseInt(op.hp / 2, 10);
		op.hp -= tmp;
		sln('');
		more_nomail();
		lln('  `%** `2Your `0'+son+'`2 hits '+op.name+' `2for `4'+pretty_int(tmp)+' `2points of damage! `%**');
		sln('');
		more_nomail();
		lln('  `0'+op.name+' `2points its `4'+op.weapon+' `2at '+his+'!');
		sln('');
		more_nomail();
		lln('  `2You jump to intercept the blow - but you are too late.  Your child');
		lln('  is struck down.  Your `0'+son+'`2 dies in your arms.');
		sln('');
		more_nomail();
		
	}
	else {
		if (pfight) {
			his = op.sex === 'M' ? 'his' : 'her';
			lw('  `4** `0'+op.name+' `2hits with '+his+' `0'+op.weapon+' `2for ');
		}
		else {
			lw('  `4** `0'+op.name+' `2hits with its `0'+op.weapon+' `2for ');
		}
		if (player.light_shield !== undefined && player.light_shield) {
			atk = parseInt(atk / 2, 10);
		}
		player.hp -= atk;
		if (player.hp < 1) {
			player.dead = true;
		}
		foreground(4);
		lln(pretty_int(atk) +' `2damage! `4**');
		if (player.has_fairy !== undefined && player.has_fairy && player.dead) {
			sln('');
			lln('  `%YOU FEEL A BUZZING IN YOUR POUCH!');
			sln('');
			more_nomail();
			lln('  `2Knowing you are too weak to go on, you decide to release the');
			sln('  fairy in your pocket.');
			sln('');
			more_nomail();
			lln('  `0The tiny thing rises in the air - And blows you a kiss!');
			sln('');
			lln('  `%YOU FEEL MUCH BETTER!');
			sln('');
			more_nomail();
			player.has_fairy = false;
			player.dead = false;
			player.hp = player.hp_max;
		}
	}
	// DIFF: You could horse-kill someone after you die...
	if (player.hp > 0 && player.horse && !pfight && random(25) === 10) {
		if (op.is_dragon !== undefined && op.is_dragon) {
			foreground(10);
			sln('');
			sln('In an unexpected move, the Dragon makes a quick swipe towards you!');
			sln('');
			more_nomail();
			sln('Your horse moves its huge body to intercept the attack');
			sln('');
			sln('You\'re horse is killed by the Dragon\'s attack!');
			// DIFF: Horse wasn't killed before...
			player.horse = false;
		}
		else {
			sln('');
			op.hp = 0;
			player.horse = 0;
			lln('  `0"Prepare to die, fool!" `2'+op.name+' `2screams.');
      			sln('');
			more_nomail();
			lln('  `2He takes a `)Death Crystal`2 from his cloak and throws it at');
			sln('  you!');
			sln('');
			more_nomail();
			lln('  `2Your horse moves its huge body to intercept the crystal.');
			sln('');
			lln('  `4YOUR HORSE IS VAPORIZED!');
			sln('');
			more_nomail();
			lln('  `2Tears of anger flow down your cheeks.  Your valiant steed must');
			lln('  be avenged.');
			sln('');
			more_nomail();
			lln('  `%YOU PUMMEL `0'+op.name+' `%WITH BLOWS!');
			sln('');
			lln('  `2A few seconds later, your adversary is dead.');
			sln('');
			more_nomail();
			lln('  `0You bury your horse in a small clearing.  The best');
			sln('  friend you ever had.');
			sln('');
		}
	}
}

function battle_prompt(op)
{
	sln('');
	lln('  `2Your Hitpoints : `0'+pretty_int(player.hp));
	lln('  `2'+op.name+'`2\'s Hitpoints : `0'+pretty_int(op.hp));
	sln('');
	lln('  `2(`5A`2)ttack');
	lln('  (`5S`2)tats');
	lln('  (`5R`2)un');
	if (player.fairy_lore !== undefined && player.fairy_lore) {
		lln('  `2(`5H`2)eal');
	}
	if (player.levelw > 0 || player.levelm > 0 || player.levelt > 0) {
		sln('');
	}
	if (player.levelw > 0) {
		lln('  `2(`0D`2)`0eath Knight Attack (`%'+player.levelw+'`0)');
	}
	if (player.levelm > 0) {
		lln('  `2(`0M`2)`0ystical Skills     (`%'+player.levelm+'`0)');
	}
	if (player.levelt > 0) {
		lln('  `2(`0T`2)`0hieving Skills     (`%'+player.levelt+'`0)');
	}
	sln('');
	lw('  `2Your command, `0'+player.name+'`2?  [`5A`2] : ');
}

// DIFF: This is new for the training...
function battle_prompt_lines()
{
	var ret = 8;
	if (player.fairy_lore !== undefined && player.fairy_lore) {
		ret += 1;
	}
	if (player.levelw > 0 || player.levelm > 0 || player.levelt > 0) {
		ret += 1;
	}
	if (player.levelw > 0) {
		ret += 1;
	}
	if (player.levelm > 0) {
		ret += 1;
	}
	if (player.levelt > 0) {
		ret += 1;
	}
	return ret;
}

function handle_hit(atk, op, pfight)
{
	sln('');
	foreground(2);
	if (pfight) {
		atk -= op.def;
	}
	if (atk < 1) {
		atk = 1;
	}
	lln('  `2You hit `0'+op.name+'`2 for `0'+pretty_int(atk)+' `2damage!');
	op.hp -= atk;
	if (atk > player.str && op.hp < 1) {
		sln('');
		// DIFF: This was unconditional in original...
		if (!pfight) {
			lln('  '+op.death);
		}
		// DIFF: In 4.07, this was random(2) so more gold never happened.
		// rand(1,3) is what's used in the PHP version, and what we use here.
		switch (random(3)) {
			case 0:
				sln('');
				lln('  `2You find a `%Gem`2!');
				player.gem += 1;
				break;
			case 1:
				sln('');
				sln('  You find more gold than expected!');
				op.gold *= 2;
				break;
		}
	}
	if (op.hp > 0) {
		enemy_attack(op, pfight);
	}
}

function do_attack(op, pfight)
{
	var atk;
	var c;
	var ret = '';

	if (pfight === undefined) {
		pfight = false;

	}

	sln('');
	atk = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
	c = random(10) + 1;
	if (player.amulet) {
		c += 2;
	}
	if (pfight) {
		atk -= op.def;
	}
	if (player.amulet && atk < 1) {
		atk = 1;
	}
	if (atk < 1) {
		sln('');
		lw('  `2You miss `0'+op.name+'`2 completely!');
		sln('');
		enemy_attack(op, pfight);
		return '';
	}
	if (c > 9) {
		atk *= 3;
		sln('');
		foreground(15);
		sln('  **POWER MOVE**');
		if (pfight) {
			ret += '\n  `0'+player.name+' `2did a power move for `4'+pretty_int(atk)+' `2damage!';
		}
	}
	sln('');
	lln('  `2You hit `0'+op.name+'`2 for `0'+pretty_int(atk)+' `2damage!');
	op.hp -= atk;
	if (atk > player.str && op.hp < 1) {
		sln('');
		if (!pfight) {
			lln('  '+op.death);
		}
		// DIFF: In 4.07, this was random(2) so more gold never happened.
		// rand(1,3) is what's used in the PHP version, and what we use here.
		switch (random(3)) {
			case 0:
				sln('');
				lln('  `2You find a `%Gem`2!');
				player.gem += 1;
				break;
			case 1:
				sln('');
				sln('  You find more gold than expected!');
				op.gold *= 2;
				break;
		}
	}
	if (op.hp > 0) {
		enemy_attack(op, pfight);
	}
	return ret;
}

function try_running(op, pfight, cant_run)
{
	if (op.is_arena !== undefined && op.is_arena) {
		sln('  You cannot run from an Arena!!!  You came here to prove');
		sln('  your worth, and that\'s what you are going to do.');
		battle_prompt(op);
		return '';
	}

	if (cant_run || (random(9) === 1)) {
		sln('');
		lln('  `0'+op.name+' `2sees you!');
		enemy_attack(op, pfight);
		return '';
	}

	player.ran_away = true;
	if (pfight) {
		sln('');
		lln('  `2You barely manage to escape!  `0'+op.name+'`2 laughs as you');
		sln('  scurry away.');
		return '\n  `0'+player.name+'`2 has run away like a scared rat!';
	}

	if (op.is_dragon !== undefined && op.is_dragon) {
		sln('');
		lln('  `2You barely flip out of the way, as the `4Dragon`2 breathes huge');
		sln('  amounts of fire where you were a second ago!  You run towards');
		sln('  the forest, screaming all the way!');
	}
	return '';
}

function use_death_knight(op, pfight)
{
	var atk;

	sln('');
	sln('');
	if (pfight && player.level >= op.level) {
		sln('  Your honor stops you from using the more unorthodox methods of ');
		sln('  battle.');
		sln('');
		battle_prompt(op);
		return;
	}
	if (op.is_arena !== undefined && op.is_arena) {
		sln('  Your honor stops you from using the more unorthodox methods of ');
		sln('  battle against your teacher.');
		sln('');
		battle_prompt(op);
		return;
	}
	if (pfight && player.level < op.level) {
		sln('  In a situation like this, you need every advantage you can get.');
		sln('');
	}
	lln('  `%** ULTRA POWERFUL MOVE **');
	sln('');
	foreground(2);
	switch(random(7)) {
		case 0:
			lln('  In a swift move you have `0'+op.name+'`2 by the neck and begin to');
			sln('  exert huge amounts of pressure.  You hear a sickening crack.');
			break;
		case 1:
			lln('  In a spectacular move, you drive your weapon up between `0'+op.name+'`2\'s');
			lln('  legs.  You grin as your '+player.weapon+' crunches noisily.');
			break;
		case 2:
			sln('  Seeing an opening, you feel your tendons strain as you swing your');
			lln('  '+player.weapon+'. `0'+op.name+'\'s`2 left arm falls to the ground,');
			sln('  a gout of blood pulsating at the stump.');
			break;
		case 3:
			sln('  You duck an uncontrolled swing, and retaliate by slicing a wide gash');
			lln('  under `0'+op.name+'\'s`2 belly.  You feel sick as steamy intestines');
			sln('  slither out into a pile at your enemy\'s feet.');
			break;
		case 4:
			lln('  In a scream of rage, you brandish your '+player.weapon+' wildly.  The');
			lln('  surprised `0'+op.name+'`2 misses a parry, and you are able to send your');
			sln('  enemy\'s nose into the air with a smooth blow.');
			break;
		case 5:
			lln('  After exchanging blows & blocks for nearly a minute, `0'+op.name);
			foreground(2);
			sln('  gets too fancy, and as a result an attempt to jump a low swing results');
			sln('  in the severing of your enemy\'s left leg.  His remaining leg slips in');
			sln('  blood.');
			break;
		case 6:
			lln('  You swing your '+player.weapon+' as hard as you can, intending to');
			sln('  drive it into the fiend\'s side.  Instead he ducks, and your blade slides');
			sln('  right into the top part of his skull.  He stares at you in horror.  The');
			sln('  creature\'s warm brain slides smoothly from the cloven skull and lands');
			sln('  with a soft plop on your shoe.  You kick it away, disgusted.');
			break;
	}

	atk = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
	if (settings.dk_boost) {
		atk = parseInt(atk * 3.3, 10);
	}
	else {
		atk *= 3;
	}
	handle_hit(atk, op, pfight);
	player.levelw -= 1;
}

function use_thief_skill(op, pfight)
{
	var atk;

	sln('');
	sln('');
	if (pfight && player.level >= op.level) {
		sln('  Your honor stops you from using the more unorthodox methods of ');
		sln('  battle.');
		sln('');
		battle_prompt(op);
		return;
	}
	if (op.is_arena !== undefined && op.is_arena) {
		sln('  Your honor stops you from using the more unorthodox methods of ');
		sln('  battle against your teacher.');
		sln('');
		battle_prompt(op);
		return;
	}
	if (pfight && player.level < op.level) {
		sln('  In a situation like this, you need every advantage you can get.');
		sln('');
	}
	lln('  `%** ULTRA SNEAKY MOVE **');
	sln('');
	foreground(2);
	switch(random(7)) {
		case 0:
			lln('  You point behind your enemy, and yell "Whats that?!"  `0'+op.name);
			lln('  `2turns around stupidly, before the fool realizes its error, there are');
			sln('  two daggers planted in its back.');
			break;
		case 1:
			lln('  You suddenly find `0'+op.name+'\'s`2 right eye very unnatractive.');
			sln('  In a smooth motion you slide a dagger from your boot into the air. End');
			sln('  over end it flies, finding its mark.  Your enemy\'s right eye "pops".');
			break;
		case 2:
			sln('  A devious plan enters your mind.  You fall to the ground, clutching');
			lln('  at your chest.  The dumbfounded `0'+op.name+'`2 drops his guard and');
			sln('  leans over you.  You scream "Surprise you hoochie fiend!" and drive your');
			lln('  '+player.weapon+' into his neck.  It\'s nice to be covered with someone');
			sln('  else\'s blood for once.');
			break;
		case 3:
			sln('  You duck an uncontrolled swing, and retaliate by slicing a wide gash');
			lln('  under `0'+op.name+'\'s`2 belly.  You feel sick as steamy intestines');
			sln('  slither out into a pile at your enemy\'s feet.');
			break;
		case 4:
			lln('  In a scream of rage, you brandish your '+player.weapon+' wildly.  The');
			lln('  surprised `0'+op.name+'`2 misses a parry, and you are able to send your');
			sln('  enemy\'s nose into the air with a smooth blow.');
			break;
		case 5:
			lln('  After exchanging blows & blocks for nearly a minute, `0'+op.name);
			lln('  `2gets too fancy, and as a result an attempt to jump a low swing results');
			sln('  in the severing of your enemy\'s left leg.  His remaining leg slips in');
			sln('  blood.');
			break;
		case 6:
			lln('  You swing your '+player.weapon+' as hard as you can, intending to');
			sln('  drive it into the fiend\'s side.  Instead he ducks, and your blade slides');
			sln('  right into the top part of his skull.  He stares at you in horror.  The');
			sln('  creature\'s warm brain slides smoothly from the cloven skull and lands');
			sln('  with a soft plop on your shoe.  You kick it away, disgusted.');
			break;
	}

	atk = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
	atk *= 3;
	handle_hit(atk, op, pfight);
	player.levelt -= 1;
}

function use_mystical_skill(op, pfight)
{
	var atk;
	var ch;
	var valid;

	function too_tired() {
		foreground(10);
		sln('  Your face contorts in concentration.  You reach deep within yourself');
		sln('  for the power, but gasp as you realize is just isn\'t there.  You need');
		sln('  rest before your mind will be ready for this.');
		sln('');
		more();
	}

	sln('');
	sln('');
	lln('  `%** MYSTICAL SKILLS **');
	sln('');
	if (pfight && player.level >= op.level) {
		sln('  Your honor stops you from using the more unorthodox methods of ');
		sln('  battle.');
		sln('');
		battle_prompt(op);
		return;
	}
	if (op.is_arena !== undefined && op.is_arena) {
		sln('  Your honor stops you from using the more unorthodox methods of ');
		sln('  battle against your teacher.');
		sln('');
		battle_prompt(op);
		return;
	}
	if (pfight && player.level < op.level) {
		sln('  In a situation like this, you need every advantage you can get.');
		sln('');
	}
	sln('');
	foreground(2);
	switch(random(7)) {
		case 0:
			lln('  `2You quickly decide which mystical skill to use.');
			break;
		case 1:
			lln('  `2Your enemy gives you a chance to contemplate your next action.');
			break;
		case 2:
			lln('  `2You decide a little magic might change the outcome of this battle.');
			break;
		case 3:
			lln('  `2You struggle to keep your anger under control.');
			break;
		case 4:
			lln('  `2Your mind carefully goes over what you have learned.');
			break;
		case 5:
			lln('  `2You feel power dancing in your mind, maybe it\'s time to use it.');
			break;
		case 6:
			lln('  `0'+op.name+' `2looks dumbfounded as you sheathe your '+player.weapon+'.');
			break;
	}
	sln('');
	lln('  `5(`#P`5)inch Real Hard                 `5(`%1`5)');
	valid = 'P';
	if (player.levelm > 3 && player.skillm > 3) {
		lln('  `5(`#D`5)isappear                       `5(`%4`5)');
		valid += 'D';
	}
	if (player.levelm > 7 && player.skillm > 7) {
		lln('  `5(`#H`5)eat Wave                       `5(`%8`5)');
		valid += 'H';
	}
	if (player.levelm > 11 && player.skillm > 11) {
		lln('  `5(`#L`5)ight Shield                    `5(`%12`5)');
		valid += 'L';
	}
	if (player.levelm > 15 && player.skillm > 15) {
		lln('  `5(`#S`5)hatter                         `5(`%16`5)');
		valid += 'S';
	}
	if (player.levelm > 19 && player.skillm > 19) {
		lln('  `5(`#M`5)ind Heal                       `5(`%20`5)');
		valid += 'M';
	}
	sln('');

	lw('  `5You Have `%'+(player.levelm)+'`5 Use Points.  Choose.  [`#Nothing`5] : ');
	ch = getkey().toUpperCase();
	if (valid.indexOf(ch) === -1) {
		ch = 'Nothing';
	}
	sln(ch);
	if (ch === 'P') {
		if (player.levelm < 1) {
			too_tired();
			return;
		}
		lln('  `0You whisper the word.  You smile as `2'+op.name+'`0 screams out in pain.');
		atk = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
		atk = (atk * 2) - parseInt((atk * 2) / 4, 10);
		handle_hit(atk, op, pfight);
		player.levelm -= 1;
		// TODO: This seems to be the only one that redraw the menu?
		if (player.hp > 0 && op.hp > 0) {
			battle_prompt(op);
		}
	}
	if (ch === 'D') {
		if (player.levelm < 4) {
			too_tired();
			return;
		}
		foreground(10);
		sln('  You imagine yourself being in a different part of the forest, and begin');
		sln('  to concentrate.  The next instant you are standing in a cool glade, nowhere');
		lln('  near your enemy.  You almost laugh remembering `2'+op.name+'\'s`0 befuddled');
		player.levelm -= 4;
		player.ran_away = true;
	}
	if (ch === 'H') {
		if (player.levelm < 8) {
			too_tired();
			return;
		}
		lln('  `0Your face contorts in anger.  How DARE `2'+op.name+'`0 presume that IT');
		sln('  can beat YOU?  The air in front of you begins to shimmer.  A few seconds');
		sln('  later, the fiend is engulfed by flames.');
		player.levelm -= 8;
		atk = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
		atk = (atk * 2) + parseInt((atk * 2) / 4, 10);
		handle_hit(atk, op, pfight);
	}
	if (ch === 'L') {
		if (player.levelm < 12) {
			too_tired();
			return;
		}
		lln('  `0You look into the clouds.  A beam of light covers your face.  You hear');
		lln('  `2'+op.name+'`0 scream as the light expands to cover your whole body.');
		sln('  The beam disappears, but you are still glowing.');
		player.levelm -= 12;
		player.light_shield = true;
	}
	if (ch === 'S') {
		if (player.levelm < 16) {
			too_tired();
			return;
		}
		lln('  `0You visualize the bones in your enemy, then imagine them imploding.');
		lln('  `2'+op.name+'`0 pierces the air with a scream of horror that makes your');
		lln('  blood run cold.  Even so, you feel no remorse.');
		player.levelm -= 16;
		atk = random(parseInt(player.str / 2, 10)) + parseInt(player.str / 2, 10);
		atk = (atk * 4) + parseInt((atk * 4) / 4);
		handle_hit(atk, op, pfight);
	}
	if (ch === 'M') {
		if (player.levelm < 20) {
			too_tired();
			return;
		}
		lln('  `0You look down at your bloody body.  This will never do.  With an');
		lln('  inhuman scream, you will yourself to be healed.  `2'+op.name);
		lln('  `0gasps as missing pieces of your flesh crawl back into place.');
		sln('');
		lln('  `%YOU ARE HEALED.`2');
		player.levelm -= 20;
		player.hp = player.hp_max;
	}
}

function battle(op, pfight, cant_run)
{
	var tmp;
	var ch;
	var mail='';

	if (pfight === undefined) {
		pfight = false;

	}

	tmp = random(99) + 1;
	if (pfight) {
		if (op.level > player.level) {
			if (tmp > 60) {
				tmp = 95;
			}
		}
	}

	if (tmp > 90) {
		lln('  `0'+op.name+' `2surprises you.');
		enemy_attack(op, pfight);
	}
	else {
		sln('  Your skill allows you to get the first strike.');
	}

	player.ran_away = false;
	while (true) {
		if (!player.dead && player.hp > 0) {
			battle_prompt(op);
		}
		if (player.dead) {
			break;
		}
		if (player.hp < 1) {
			break;
		}
		// NOTE: This prompt is being left non-normalized...
		ch = getkey().toUpperCase();
		if (ch === '\r') {
			ch = 'A';
		}
		sw(ch);
		switch(ch) {
			case 'Q':
				sln('');
				sln('');
				sln('  You are in combat!  Try running.');
				sln('');
				break;
			case 'A':
				mail += do_attack(op, pfight);
				break;
			case 'R':
				mail += try_running(op, pfight, cant_run);
				break;
			case 'S':
				show_stats();
				break;
			case 'L':
				sln('');
				sln('');
				sln('  What?!  You want to fight two at once?');
				sln('');
				break;
			case 'D':
				if (player.levelw < 1) {
					sln('');
					sln('');
					if (player.skillw < 1) {
						sln('  You don\'t know any at the moment!');
					}
					else {
						sln('  You will need rest before you can use those again.');
					}
					sln('');
				}
				else {
					use_death_knight(op, pfight);
				}
				break;
			case 'T':
				if (player.levelt < 1) {
					sln('');
					sln('');
					if (player.skillt < 1) {
						sln('  You don\'t know any at the moment!');
					}
					else {
						sln('  You will need rest before you can use those again.');
					}
					sln('');
				}
				else {
					use_thief_skill(op, pfight);
				}
				break;
			case 'M':
				if (player.levelm < 1) {
					sln('');
					sln('');
					if (player.skillm < 1) {
						sln('  You don\'t know any at the moment!');
					}
					else {
						sln('  You will need rest before you can use those again.');
					}
					sln('');
				}
				else {
					use_mystical_skill(op, pfight);
				}
				break;
			case 'H':
				sln('');
				sln('');
				if (player.fairy_lore === undefined || !player.fairy_lore) {
					sln('  You are in combat, and they don\'t make house calls!');
				}
				else {
					foreground(10);
					sln('  Thanks to your fairy lore training today, you are able to heal yourself.');
					lw('  `2Do it? [`%Y`2] : `%');
					ch = getkey().toUpperCase();
					if (ch !== 'N') {
						ch = 'Y';
					}
					sln(ch);
					foreground(10);
					if (ch === 'Y') {
						player.hp = player.hp_max;
						player.fairy_lore = false;
						sln('  Concentrating only on your wounds, they heal themselves!');
					}
				}
				sln('');
				break;
		}
		if (player.ran_away !== undefined && player.ran_away) {
			break;
		}
		if (player.dead) {
			break;
		}
		if (player.hp < 1) {
			break;
		}
		if (op.hp < 1) {
			break;
		}
	}

	player.ran_away = false;
	return mail;
}

function online_battle(op)
{
	var action;

	lln('`c  `%                           ** ONLINE BATTLE **');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	if (psock === undefined) {
		if (file_exists(gamedir('fight'+op.Record+'.rec'))) {
			sln('');
			sln('  Sorry!  That user is currently already in battle.  Try again later.');
			sln('');
			more_nomail();
			ob_cleanup();
			return;
		}
		fmutex(gamedir('fight'+player.Record+'.rec'));
	}
	mail_to(op.Record, '`o'+player.Record);
	// DIFF: You got free full healing here!
	// player.hp = player.hp_max
	player.put();
	lln('  `2Issuing an Online Duel challenge to '+op.name+'`2.');
	action = ob_get_response(op);
	if (action === 'ABORT') {
		sln('');
		sln('  You decide you are really not ready for battle anyway.');
		sln('');
		more_nomail();
		ob_cleanup();
		return;
	}
	sln('');
	if (action[0] === 'R') {
		lln('  `2You call out, but `0'+op.name+'`2 hides from you. ');
		sln('');
		more_nomail();
		ob_cleanup();
		return;
	}
	player.pvp_fights -= 1;
	lln('  `0'+op.name+' `2draws his '+op.weapon+' for battle!');
	sln('');
	if (action[0] === 'A') {
		on_battle(op, true);
	}
	else {
		on_battle(op, false, action);
	}
	ob_cleanup();
}

function custom_saying()
{
	var ch;
	var ret = '';

	sln('');
	lw('  `2Say something to the press? [`0N`2] : `%');
	ch = getkey().toUpperCase();
	if (ch !== 'Y') {
		ch = 'N';
	}
	sw(ch);
	if (ch === 'N') {
		return '';
	}
	while (true) {
		sln('');
		sln('');
		lln(' `2Share your feelings now.. (Max 50 char!)');
		sln('                                             Max');
		sln('                                              ^');
		lw('  `0"`2');
		ret = dk.console.getstr();
		ret = clean_str(ret);
		if (ret.length > 50) {
			sln('');
			lln('  `2The press thinks you are boring and leaves!');
			sln('');
			more_nomail();
			return '';
		}
		if (ret.length < 5) {
			sln('');
			lln('  `2That just isn\'t interesting enough.  Sorry man.');
			sln('');
			more_nomail();
			return '';
		}
		ret = '`0  "'+ret+'"`2 ';
		sln('');
		lln('  `2(`0H`2)appy');
		lln('  `2(`0S`2)ad');
		lln('  `2(`0M`2)oaning');
		lln('  `2(`0C`2)ursing');
		lln('  `2(`0E`2)ffing Mad');
		sln('');
		lw('  Your emotional state? `2[`0H`2] : `%');
		ch = getkey().toUpperCase();
		if ('HSMCE'.indexOf(ch) === -1) {
			ch = 'H';
		}
		sw(ch);
		switch(ch) {
			case 'H':
				ret += 'laughs `%'+player.name+'`2.';
				break;
			case 'S':
				ret += '`%'+player.name+'`2 declares sadly.';
				break;
			case 'M':
				ret += 'moans `%'+player.name+'`2.';
				break;
			case 'C':
				ret += 'curses `%'+player.name+'`2.';
				break;
			case 'E':
				ret += 'screams `%'+player.name+'`2 in anger.';
				break;
		}
		sln('');
		sln('');
		lln(ret);
		sln('');
		lw('  `2Does this look ok? [`0Y`2] :`% ');
		ch = getkey().toUpperCase();
		if (ch !== 'N') {
			ch = 'Y';
		}
		sln(ch);
		if (ch !== 'N') {
			sln('');
			lln('  Your comment has been noted.`2');
			return ret;
		}
	}
}

function say(fname, enemy, type, header, force)
{
	var f = new File(fname);
	var i;
	var c;

	if (force === undefined) {
		force = '';

	}
	if (force === '') {
		if (!f.open('r')) {
			throw('Unable to open '+f.name);
		}
		f.readln();
		c = parseInt(f.readln(), 10);
		c = random(c);
		for (i = 0; i < c; i += 1) {
			force = '`2  '+f.readln();
		}
		f.close();
		force = force.replace(/`([neg])/g, function(ignore, c) {
			if (c === 'n') {
				return '\n';
			}
			switch(type) {
				case 1:
					if (c === 'e') {
						return '`0'+enemy.name+'`2';
					}
					if (c === 'g') {
						return '`5'+player.name+'`2';
					}
					break;
				case 2:
					if (c === 'e') {
						return '`5'+enemy.name+'`2';
					}
					if (c === 'g') {
						return '`0'+player.name+'`2';
					}
					break;
				case 3:
					if (c === 'e') {
						return '`0'+enemy.name+'`2';
					}
					if (c === 'g') {
						return '`5'+player.name+'`2';
					}
					break;
			}
		});
	}
	log_line(header+'\n'+force);
}

function bad_say(enemy, header)
{
	say(game_or_exec('badsay.lrd'), enemy, 3, header, custom_saying());
}

function good_say(enemy, header)
{
	say(game_or_exec('goodsay.lrd'), enemy, 2, header, custom_saying());
}

function attack_player(op, inn)
{
	var pronoun;
	var mail;
	var tmp;
	var ch;
	var out = new File(gamedir('out'+op.Record+'.lrd'));
	var where;
	var wep;

	if (player.pvp_fights < 1) {
		sln('');
		sln('  You are too tired to look for that warrior.  Try again tomorrow.');
		sln('');
		more();
		return;
	}
	pronoun = (op.sex === 'M' ? 'his' : 'her');
	if (psock !== undefined) {
		psock.writeln('BattleStart '+op.Record);
		tmp = psock.readln().split(' ', 2);
		switch(tmp[0]) {
			case 'OK':
				break;
			case 'Out:':
				sln('');
				sln('  That warrior is currently visiting another section of the realm.');
				lln('  `2('+op.name+' '+tmp[1]+'`2)');
				more_nomail();
				return;
			case 'Online':
				sln('');
				sln('  That warrior is currently online!');
				sln('');
				online_battle(op);
				return;
			case 'Dead':
				sln('');
				sln('  Your jaw drops as a shadowy figure finishes '+pronoun+' off!');
				sln('');
				more_nomail(op);
				return;
			case 'InBattle':
				sln('');
				op = player_get(parseInt(tmp[1], 10));
				lln('  As you move forward, '+pronoun+' is attacked by `2'+op.name+'`2.');
				sln('  You fade back, vowing to return soon.');
				sln('');
				more_nomail(op);
				return;
			default:
				throw('Out of sync with server after BattleStart');
		}
	}
	else {
		fmutex(out.name);
		if (file_exists(out.name)) {
			if (!out.open('r')) {
				throw('Unable to open '+out.name);
			}
			where = out.readln();
			out.close();
			rfmutex(out.name);
			sln('');
			sln('  That warrior is currently visiting another section of the realm.');
			lln('  `2('+op.name+' '+where+'`2)');
			more_nomail();
			return;
		}
		rfmutex(out.name);
		fmutex(gamedir('battle'+op.Record+'.rec'), player.Record+'\n');
		if (op.on_now) {
			rfmutex(gamedir('battle'+op.Record+'.rec'));
			sln('');
			sln('  That warrior is currently online!');
			sln('');
			online_battle(op);
			return;
		}
	}
	mail = ' \n  `%YOU HAVE BEEN ATTACKED!\n`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2\n  `0'+player.name+'`2 has attacked you!';
	player.pvp_fights -= 1;
	sln('');
	sln('');
	lln('  `2** `%PLAYER FIGHT `2**');
	sln('');
	lln('  You have encountered '+op.name+'`2!!');
	sln('');
	if (op.hp < op.hp_max) {
		op.hp = op.hp_max;
	}
	mail += battle(op, true, false);
	if (player.dead) {
		sln('');
		player.gold = 0;
		player.exp -= parseInt(player.exp / 10, 10);
		mail += '\n`.  `2You have killed `0'+player.name+' `2in self defense!\n`.  `2You receive `%'+pretty_int(player.exp / 2)+'`2 experience!';
		if (psock !== undefined) {
			psock.writeln('WonBattle '+op.Record);
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after WonBattle');
			}
		}
		else {
			op.pvp += 1;
			if (op.pvp > 32000) {
				op.pvp = 32000;
			}
			op.exp += parseInt(player.exp / 2, 10);
			if (op.exp > 2000000000) {
				op.exp = 2000000000;
			}
			op.put();
		}
		mail_to(op.Record, mail);
		if (psock === undefined) {
			rfmutex(gamedir('battle'+op.Record+'.rec'));
		}
		dead_screen(op);
		bad_say(op, '`.  `0'+op.name+' `2has killed `5'+player.name+' `2in self defence!');
		sln('');
		return;
	}

	if (op.hp < 1) {
		foreground(15);
		sln('');
		lln('  You have killed '+op.name+'`%!');
		sln('');
		foreground(2);
		tmp = player.gold;
		player.gold += op.gold;
		if (player.gold >= 2000000000) {
			player.gold = 2000000000;
		}
		lw('  You receive `%'+pretty_int(player.gold - tmp)+' `2gold, ');
		tmp = player.exp;
		player.exp += parseInt(op.exp / 2, 10);
		if (player.exp >= 2000000000) {
			player.exp = 2000000000;
		}
		lln('and '+pretty_int(player.exp - tmp)+' experience!');
		if (op.gem >= 2) {
			tmp = player.gem;
			player.gem += parseInt(op.gem / 2, 10);
			sln('');
			tmp = player.gem - tmp;
			lln('  `2You also find `0'+pretty_int(tmp)+' `%'+(tmp === 1 ? 'Gem' : 'Gems')+'`2!');
		}
		if (player.def < (32000 - 3)) {
			if (settings.def_for_pk) {
				lw('  `2You also gain `%3 `2defense points');
				player.def += 3;
				if (player.str < (32000 - 2)) {
					// In 4.07, you don't actually get the str points!
					lw(', and you gain `%2 `2strength points');
					if (settings.str_for_pk) {
						player.str += 2;
					}
				}
			}
			sln('');
		}
		if (!settings.def_for_pk && settings.str_for_pk) {
			if (player.str < (32000 - 2)) {
				lw('  `2You also gain `%2 `2strength points');
				player.str += 2;
			}
		}
		if (psock !== undefined) {
			psock.writeln('LostBattle '+op.Record);
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after LostBattle');
			}
		}
		else {
			op.gem -= parseInt(op.gem / 2, 10);
			op.exp = op.exp - parseInt(op.exp / 10, 10);
			op.gold = 0;
			op.dead = true;
			op.inn = false;
		}
		mail+='\n`.  `0'+player.name+' `2has killed you!';

		// TODO: This is a massive pain in the arse to get right in remote games.  Don't try.
		if (inn && psock === undefined) {
			if (random(10) === 1) {
				if (player.level < (op.level - 1)) {
					if (op.weapon_num > player.weapon_num) {
						sln('');
						lln('  `2Do you wish to trade your '+player.weapon+' `2for '+pronoun);
						lln('  `%'+op.weapon+'`2? `0[N] : `%');
						ch = getkey().toUpperCase();
						if (ch !== 'Y') {
							ch = 'N';
						}
						sln(ch);
						if (ch === 'Y') {
							wep = get_weapon(op.weapon);
							op.str -= wep.num;
							player.str += wep.num;
							wep = get_weapon(player.weapon_num);
							op.str += wep.num;
							player.str -= wep.num;
							tmp = op.weapon;
							op.weapon = player.weapon;
							player.weapon = tmp;
							tmp = op.weapon_num;
							op.weapon_num = player.weapon_num;
							player.weapon_num = tmp;
							mail += '\n  `$'+player.name+' took your weapon!';
							lln('  `2Done! You now have a '+player.weapon+'`2!');
						}
					}
				}
			}
		}
		mail_to(op.Record, mail);
		sln('');
		player.killedaplayer = true;
		if (player.pvp < 32000) {
			player.pvp += 1;
		}
		if (psock === undefined) {
			op.put();
			rfmutex(gamedir('battle'+op.Record+'.rec'));
		}
		good_say(op, '`.  `0'+player.name+' `2has killed `5'+op.name+'`2!');
		tournament_check();
	}
	else if (psock !== undefined) {
		psock.writeln('RanFromBattle '+op.Record);
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after RanFromBattle');
		}
	}

	player.light_shield = false;
}

function attack_in_inn()
{
	var ch;
	var done = false;
	var atk;
	var op;
	var pronoun;

	generate_rankings(gamedir('temp'+player.Record), true, true, true);
	sclrscr();
	display_file(gamedir('temp'+player.Record), true, true);
	file_remove(gamedir('temp'+player.Record));
	sln('');
	do {
		sln('');
		foreground(5);
		sln('  "What next, kid?"');
		sln('');
		foreground(2);
		lln('  `2(`5L`2)ist Warriors in the Inn');
		lln('  `2(`5S`2)laughter Warriors in the Inn');
		lln('  `2(`5R`2)eturn to Bar');
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch (ch) {
			case 'L':
				generate_rankings(gamedir('temp'+player.Record), true, true, true);
				display_file(gamedir('temp'+player.Record), true, true);
				file_remove(gamedir('temp'+player.Record));
				break;
			case 'R':
				foreground(5);
				sln('');
				sln('  "So ya aren\'t going to attack anyone?  Here is half your money back...');
				lln('\'   The other half will cover my time!" `2the bartender laughs harshly.');
				player.gold += 400 * (player.level * 2);
				sln('');
				more_nomail();
				done = true;
				break;
			case 'S':
				sln('  Who would you like to attack?');
				atk = find_player();
				if (atk === -1) {
					sln('  No warriors found.');
					break;
				}
				sln('');
				op = player_get(atk);
				pronoun = op.sex === 'M' ? 'he' : 'she';
				if (atk === player.Record) {
					sln('  You wish to attack yourself?!!  You decide against it.');
					break;
				}
				if (op.dead) {
					sln('  That warrior isn\'t at the Inn at the moment.');
					sln('  You recall seeing in the news that ' + pronoun + ' was dead..');
					break;
				}
				if (!op.inn) {
					sln('  That warrior is not staying at the Inn today,');
					sln('  '+pronoun+' is probably in the fields.');
					break;
				}
				if (op.level + 1 < player.level) {
					sln('  A child could beat that wimp!  Attack someone else!');
					break;
				}
				if (op.sex === 'M') {
					lln('  You enter '+op.name+'`2s room...He is sleeping.');
					lln('  You notice he has a dangerous looking '+op.weapon+' `2by his bed..');
					lln('  Are you sure you want to attack him?');
				}
				else {
					lln('  You enter '+op.name+'`2s room...She is sleeping.');
					lln('  You notice she has a dangerous looking '+op.weapon+' `2by her bed..');
					lln('  Are you sure you want to attack her?');
				}
				sln('');
				lw('  `2Attack `5'+op.name+' `2[`0Y`2] :`0');
				ch = getkey().toUpperCase();
				if (ch !== 'N') {
					ch = 'Y';
				}
				sln(ch);
				if (ch === 'N') {
					break;
				}
				sln('');
				sln('  Being a trained warrior, '+pronoun+' jumps up suddenly, aware of your');
				sln('  presence!');
				mail_to(atk, '  `%YOU HAVE BEEN ATTACKED IN YOUR ROOM!\n' +
					'`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2\n'+
					'  `0'+player.name+'`2 has broken into your room at the Inn!');
				more();
				attack_player(op, true);
				done = true;
				break;
		}
	} while(!done);
}

function talk_with_bartender()
{
	var done = false;
	var i;
	var ch;
	var tmp;
	var nname;
	var class_names = ['lazy', 'warrior', 'mystical skills user', 'thief'];

	function unique_name(name)
	{
		var ui;
		var uop;
		var plen;

		plen = player_length();
		for (ui = 0; ui < plen; ui += 1) {
			if (ui !== player.Record) {
				uop = player_get(ui);
				if (name === disp_str(uop.name).trim()) {
					sln('');
					foreground(2);
					sln('  You recall hearing that another warrior currently goes by that name.  Not');
					lln('  wanting to dishonor '+uop.name+'`2 you decide to pick another.');
					sln('');
					return false;
				}
			}
		}
		return true;
	}

	function menu() {
		if (player.level > 11) {
			lrdfile(player.sex === 'M' ? 'BT1' : 'BT1F');
		}
		if (player.level < 12) {
			lrdfile(player.sex === 'M' ? 'BT' : 'BTF');
		}
	}

	sln('');
	sln('');
	foreground(2);
	sln('  You find the bartender and ask if he will talk');
	sln('  privately with you.');
	sln('');

	if (player.level === 1) {
		foreground(5);
		sln('  "I don\'t recall ever hearing the name');
		lln('  `0'+player.name+'`5 before!  Get outta my face!"');
		foreground(2);
		sln('');
		return;
	}

	menu();

	do {
		foreground(5);
		sln('');
		sw('  "Well?"');
		lln(' The bartender inquires.  (`0? for menu`2)');
		command_prompt();
		ch = getkey().toUpperCase();
		if (ch === '\r') {
			ch = 'R';
		}
		sln(ch);
		switch (ch) {
			case '?':
				menu();
				break;
			case 'R':
			case 'Q':
				done = true;
				break;
			case 'V':
				if (player.sex === 'M') {
					sln('');
					foreground(5);
					lln('  "Ya want to know about `#Violet`5 do ya?  She is every warrior\'s');
					sln('  wet dream...But forget it, Lad, she only goes for the type');
					sln('  of guy who would help old people..."');
				}
				break;
			case 'S':
				if (player.sex === 'F') {
					sln('');
					foreground(5);
					lln('  "Ya want to know about `%Seth Able`5 the Bard, eh?  Well... He');
					sln('  has a good voice, and can really play that mandolin.  I don\'t');
					sln('  think he would go for your type tho,  he likes he type of girl');
					sln('  that would help an old man or something... A lot of Charm is');
					sln('  is what you would need.  However, I could sure go for your type');
					sln('  Har har har!"');
				}
				break;
			case 'G':
				sln('');
				foreground(5);
				lln('  "You have `%Gems`5, eh?  I\'ll give ya a pint of magic elixir for two."');
				foreground(2);
				lw('  Buy how many elixirs?  [`0'+pretty_int(player.gem / 2)+'`2] : ');
				foreground(15);
				ch = dk.console.getstr({edit:parseInt(player.gem / 2, 10).toString(), len:10, integer:true});
				if (ch === undefined || ch === '') {
					ch = '0';
				}
				i = parseInt(ch, 10);
				if (isNaN(i)) {
					i = 0;
				}
				if (i === 0) {
					foreground(5);
					sln('');
					sln('  "They are vary valuable, please reconsider it.."');
					break;
				}
				if (i > 2000000) {
					foreground(5);
					sln('');
					sln('  "Ha!  There isn\'t enough brew in all the world for that much!"');
					break;
				}
				if (player.gem / 2 < i) {
					sln('');
					foreground(5);
					lln('  "You don\'t have `%'+pretty_int(i*2)+' `5Gems.  Find some..."');
					break;
				}
				if (i < 0) {
					sln('');
					foreground(5);
					sln('  "Ha.. Like I\'m really gonna give you a negetive amount, idiot."');
					break;
				}
				player.gem -= i * 2;
				foreground(2);
				sln('');
				switch (i) {
					case 1:
						ch = 'small cup';
						break;
					case 2:
					case 3:
					case 4:
					case 5:
						ch = 'steaming mug';
						break;
					case 6:
					case 7:
					case 8:
					case 9:
						ch = 'huge tankard';
						break;
					default:
						ch = 'keg \'o brew';
				}
				foreground(2);
				sln('  The bartender retrieves a '+ch+' from the back room.');
				sln('  Before you drink it, what do you wish for?');
				sln('');
				lln('  `2(`0H`2)it Points');
				lln('  `2(`%S`2)trength');
				lln('  `2(`#V`2)itality');
				command_prompt();
				ch = getkey().toUpperCase();
				if ('HSV'.indexOf(ch) === -1) {
					ch = 'V';
				}
				sln(ch);
				foreground(15);
				sln('');
				switch (ch) {
					case 'H':
						sln('  YOU DRINK THE BREW AND FEEL REFRESHED!');
						sln('');
						add_hp(i);
						break;
					case 'V':
						sln('  YOU DRINK THE BREW AND YOUR SOUL REJOICES!');
						sln('');
						add_def(i);
						break;
					case 'S':
						sln('  YOU DRINK THE BREW AND FEEL STRONGER!');
						sln('');
						add_str(i);
						break;
				}
				break;
			case 'C':
				sln('');
				foreground(5);
				sln('  "Ya wanna change your name, eh?  Yeah..');
				lln('  '+player.name+' `5the '+class_names[player.clss]+' does sound kinda funny..');
				sw('  it would cost ya '+pretty_int(player.level * 500)+' gold... Deal?"');
				lw('  `2Change your name?  [`0N`2] : ');
				ch = getkey().toUpperCase();
				if (ch !== 'Y') {
					ch = 'N';
				}
				sln(ch);
				if (ch !== 'Y') {
					foreground(5);
					sln('  "Fine.. Keep your stupid name.. See if I care.."');
					break;
				}
				sln('');
				if (player.gold < 500 * player.level) {
					foreground(4);
					sln('');
					sln('  "Hey!  You stupid fool! You don\'t have that much gold!"');
					sln('');
					break;
				}
				player.gold -= 500 * player.level;

				while (true) {
					foreground(5);
					sln('  "What would you like as an alias?" ');
					foreground(2);
					sw('  NAME: ');
					nname = dk.console.getstr({len:50});
					sln('');
					nname = nname.trim();
					nname = clean_str(nname);
					tmp = disp_str(nname).trim();
					if (nname.length > 18) {
						foreground(4);
						sln('  "Try a shorter name ya stupid, ugly troll!"');
						sln('');
					}
					else if (nname.length < 3) {
						foreground(4);
						sln('  Try a longer name, bonehead!');
						sln('');
					}
					else if (tmp.length < 3) {
						foreground(4);
						sln('  You think yer pretty damn smart, don\'t ya.  Ha!');
						sln('');
					}
					else if (unique_name(tmp) && check_name(tmp)) {
						lw('  `5"'+nname+'`5?  That\'s kinda flaky, you sure?"  `2[`0Y`2] : `%');
						ch = getkey().toUpperCase();
						if (ch !== 'N') {
							ch = 'Y';
						}
						sln(ch);
						if (ch === 'Y') {
							break;
						}
						sln('');
					}
				}
				// DIFF: This did not happen previously,
				// so people could steal the heros name
				// and write in the dirt.
				get_state(true);
				if (state.latesthero === player.name) {
					state.latesthero = nname;
				}
				put_state();
				player.name = nname;
				sln('');
				sln('');
				lln('  `%"Done!  You are now '+player.name+'`%!"');
				sln('');
				more();
				break;
			case 'B':
				foreground(5);
				sln('');
				sln('  "Ahh.. Bribe.. Now you are speaking my language friend!');
				sln('  I will let you borrow my room keys.. on one condition..');
				sw('  That ya pay me '+pretty_int(player.level * 1600)+' gold!!"');
				lw('   `2Deal?  [`0N`2] : `%');
				ch = getkey().toUpperCase();
				if (ch !== 'Y') {
					ch = 'N';
				}
				sln(ch);
				if (ch === 'N') {
					sln('');
					foreground(5);
					sln('  "Fine.. Forget I offered that deal to you.."');
					break;
				}
				sln('');
				sln('');
				if (player.gold < (800 * (player.level * 2))) {
					foreground(4);
					sln('  "Hey! You slobbering idiot! You don\'t have that much gold!"');
					break;
				}
				player.gold -= (800 * (player.level * 2));
				attack_in_inn();
				if (player.dead) {
					done = true;
				}
				break;
			case 'D':
				if (player.level === 12) {
					foreground(5);
					sln('');
					sln('  "Ahhh...If anyone could kill that Dragon, it would be you..');
					sln('  I have heard rumors that strange noises have been heard in the');
					sw('  forest... You might try (');
					lw('`2S`5');
					sln(')earching the forest...My son did,"');
					sln('  and never came back.');
					sln('');
					more_nomail();
				}
				break;
		}
	} while (!done);

	sln('');
}

function init_bar(darkhorse)
{
	var bfnames = ['start1.lrd','start2.lrd','start3.lrd','start4.lrd','start5.lrd'];
	var fname = gamedir(darkhorse ? 'darkbar.lrd' : 'bar.lrd');

	if (psock !== undefined) {
		return;
	}
	fmutex(fname);
	if (!file_exists(fname)) {
		if (darkhorse) {
			file_copy(game_or_exec('dstart.lrd'), fname);
		}
		else {
			file_copy(game_or_exec(bfnames[random(bfnames.length)]), fname);
		}
	}
	rfmutex(fname);
}

function converse(darkhorse)
{
	var l;
	var f = new File(gamedir(darkhorse ? 'darkbar.lrd' : 'bar.lrd'));
	var ch;
	var all;
	var resp;
	var m;

	init_bar(darkhorse);
	sclrscr();
	lln('  `%Conversation at the Bar`#');
	lln('`l');
	if (psock !== undefined) {
		psock.writeln('GetConversation '+(darkhorse ? 'darkbar' : 'bar'));
		resp = psock.readln();
		m = resp.match(/^Conversation ([0-9]+)/);
		if (m === null) {
			throw('Invalid response to GetConversation');
		}
		all = psock.read(parseInt(m[1], 10));
		if (psock.read(2) !== '\r\n') {
			throw('Out of sync with server after GetConversation');
		}
		all = all.split(/\r?\n/);
		all.forEach(function(li) {
			lln(li);
		});
	}
	else {
		fmutex(f.name);
		if (!f.open('r+')) {
			rfmutex(f.name);
			throw('Unable to open '+f.name);
		}
		while (true) {
			l = f.readln();
			if (l === null) {
				break;
			}
			lln(l);
		}
		f.close();
		rfmutex(f.name);
	}
	sln('');
	lw('  `2(`5C`2)ontinue  (`5A`2)dd to Conversation `0[`5C`0] : ');
	ch = getkey().toUpperCase();
	if (ch !== 'A') {
		ch = 'C';
	}
	sln(ch);
	if (ch === 'A') {
		sln('');
		foreground(2);
		sln(' Share your feelings now.. (Max 75 char!)');
		sln('                                                                       Max');
		sln('                                                                         ^');
		lw(' `0>`2');
		l = clean_str(dk.console.getstr());
		if (l.length < 2) {
			sln('  You decide not to speak..You really don\'t have anything to say.');
			sln('  (ENTRY NOT ENTERED)');
			return;
		}
		if (l.length > 75) {
			sln('  You talk so long, that it bores the other warriors!');
			sln('  (ENTRY NOT ENTERED)');
			return;
		}
		if (psock === undefined) {
			fmutex(f.name);
			if (!f.open('r+')) {
				rfmutex(f.name);
				throw('Unable to open '+f.name);
			}
			all = f.readAll();
			all.push('  `%'+player.name+':');
			all.push('  `2'+l);
			while(all.length > 18) {
				all.shift();
			}
			f.position = 0;
			f.truncate(0);
			f.writeAll(all);
			f.close();
		}
		else {
			all = '  `%'+player.name+':\n  `2'+l;
			psock.write('AddToConversation '+(darkhorse ? 'darkbar' : 'bar') + ' ' + all.length + '\r\n' + all + '\r\n');
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after AddToConversation');
			}
		}

		if (!darkhorse) {
			get_state(true);
			state.last_bar = player.Record;
			put_state();
		}

		if (psock === undefined) {
			rfmutex(f.name);
		}
	}
}

function single_seth()
{
	var ch;
	var done = false;
	var op;

	function seduce() {
		var insults = ['filthy harlot','slut'];

		sln('  You turn on the charm, moving your body seductively');
		sln('  against him, you ask him to come upstairs with you...');
		sln('');
		mswait(2000);
		player.seen_violet = true;
		if (player.married_to > -1) {
			sln('');
			foreground(10);
			sln('  Seth Able is appalled that you would even suggest such');
			sln('  a thing!');
			sln('');
			sln('  He calls you a filthy harlot!');
			log_line( '`%  Seth Able `2calls `0'+player.name+' `2a `)'+insults[random(insults.length)]+'`2!');
			return;
		}
		if (player.cha >= 32) {
			switch (random(4)) {
				case 2:
					sln('  He smiles invitingly...  ');
					mswait(4000);
					sln('');
					foreground(2);
					sln('  Hours later.. You saunter downstairs. When a bearded drunk');
					sln('  asks you all the racket was, you smile knowingly and look away..');
					sln('  The drunks are mystified!');
					sln('');
      					lln('  `0YOU GET `%'+(player.level*40)+' `0EXPERIENCE!');
					player.exp += player.level * 40;
					log_line('`0  '+player.name+' `2got laid by `%Seth Able`2!');
					player.laid += 1;
					tournament_check();
					break;
				case 0:
					sln('  He tells you he has a headache!');
					sln('  You are very disappointed.');
					break;
				case 1:
					sln('  He tells you he is not in the mood!');
					sln('  You saunter around the bar disappointed.');
					break;
				case 3:
					sln('  He tells you he is too exausted from last night!');
					sln('  You saunter around the bar disappointed.');
					break;
			}
		}
		else {
			sln('');
			foreground(2);
			sln('  He shoves you away harshly!');
			sln('  He calls you a filthy whore!');
			sln('  You trudge away from him dejectedly..');
			sln('  The entire bar laughs at your misfortune!!');
			sln('');
			foreground(4);
			sln('  YOUR HITPOINTS GO DOWN TO 1!');
			player.hp = 1;
			log_line('`5  '+player.name+' `2 was called a whore by `%Seth Able`2!');
		}
		more();
	}

	function wink() {
		player.seen_violet = true;
		if (player.cha >= 1) {
			sln('');
			foreground(15);
			sln('  You wink at Seth Able seductively..');
			mswait(5000);
			foreground(2);
			sln('  He blushes and smiles!!');
			sln('  You are making progress with him!');
			sln('');
			foreground(15);
			sln('  You receive '+pretty_int(player.level*5)+' experience!');
			player.exp += player.level * 5;
			tournament_check();
		}
		else {
			foreground(15);
			sln('  You wink at Seth Able seductively..');
			mswait(5000);
			foreground(4);
			sln('  He looks the other way!');
			sln('  You nearly die of embarrassment!');
		}
		more();
	}

	function flutter() {
		player.seen_violet = true;
		foreground(15);
		sln('  You turn to Seth Able and flutter your eyelashes violently..');
		mswait(5000);
		foreground(2);
		if (player.cha >= 2) {
			sln('  He smiles broadly and winks back!');
			sln('  Your relationship with him is taking off!');
			sln('');
			foreground(15);
			sln('  You receive '+pretty_int(player.level * 10)+' experience!');
			player.exp += player.level * 10;
			tournament_check();
		}
		else {
			sln('  He doesn\'t seem interested and starts talking to Violet!');
			sln('  You nearly faint from embarresment!');
			sln('');
			foreground(4);
			sln('  YOU LOSE '+pretty_int(player.level)+' HIT POINTS!');
			player.hp -= player.level;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	function hanky() {
		foreground(15);
		sln('  You innocently drop your lace hanky on the ground..');
		mswait(5000);
		player.seen_violet = true;
		foreground(2);
		if (player.cha >= 4) {
			sln('  Seth sees it, picks it up and offers it to you!');
			sln('  He smiles hugely!  You are elated!');
			sln('');
			foreground(15);
			sln('  YOU RECEIVE '+pretty_int(player.level*20)+' EXPERIENCE!');
			player.exp += player.level * 20;
			tournament_check();
		}
		else {
			sln('  He ignores your plea for attention, and looks away!');
			sln('  You nearly faint from embarresment!');
			sln('');
			foreground(4);
			sln('  YOU LOSE '+pretty_int(player.level*3)+' HIT POINTS!');
			player.hp -= player.level * 3;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	function drink() {
		foreground(15);
		sln('  You wave Seth over and ask him to buy you a drink..');
		mswait(5000);
		player.seen_violet = true;
		foreground(2);
		if (player.cha >= 8) {
			sln('  He buys you the most expensive drink in the house, and gives');
			sln('  you a hug!  You cherish his warmth!');
			sln('');
			foreground(15);
			sln('  YOU RECEIVE '+pretty_int(player.level*30)+' EXPERIENCE!');
			player.exp += player.level * 30;
			tournament_check();
		}
		else {
			sln('  He looks startled at your advances and mumbles a lame excuse!');
			sln('  You nearly die of embarresment!');
			sln('');
			foreground(4);
			sln('  YOU LOSE '+pretty_int(player.level*5)+' HIT POINTS!');
			player.hp -= player.level * 5;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	function kiss() {
		foreground(15);
		sln('  You reach over his mandolin, and kiss him soundly...');
		foreground(2);
		mswait(5000);
		player.seen_violet = true;
		if (player.cha >= 16) {
			sln('  He doesn\'t object!  He kisses you back! ');
			sln('  You become over excited and force yourself away!');
			sln('');
			foreground(15);
			sln('  YOU RECEIVE '+pretty_int(player.level*40)+' EXPERIENCE!');
			player.exp += player.level * 40;
			tournament_check();
		}
		else {
			sln('  He pushes your face away, and says he is just');
			sln('  not ready for that kind of relationship!');
			sln('  You nearly die of embarresment!');
			sln('');
			foreground(4);
			sln('  YOU LOSE '+pretty_int(player.level*10)+' HITPOINTS!');
			player.hp -= player.level * 10;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	function marriage() {
		var mop;
		var tmp;

		player.seen_violet = true;
		if (player.cha < 10) {
			sln('');
			sln('');
			sln('  Seth Able looks at you seriously.');
			sln('');
			lln('  `0"'+player.name+'`0!  I will not consider it!"`2');
			sln('');
			sln('  His words carry through the air and everyone looks at you.');
			sln('');
			sln('  Your world crumbles to the ground!');
			sln('');
			more_nomail();
		}
		else if (player.cha < 20) {
			sln('');
			sln('');
			sln('  Seth Able looks at you seriously.');
			sln('');
			lln('  `0"'+player.name+'`0..We are just not right for each other.. We are');
			sln('   so different - And in this case opposites DON\'T attract."');
			sln('');
			foreground(2);
			sln('  His words carry through the air and everyone looks at you.');
			sln('');
			sln('  You feel so stupid.');
			sln('');
			more_nomail();
		}
		else if (player.cha < 50) {
			sln('');
			sln('');
			sln('  Seth Able looks at you seriously.');
			sln('');
			lln('  `0"'+player.name+'`0.. I cannot say yes.  I do find you attractive,');
			sln('   but my life is here, serving the people."');
			sln('');
			lln('  `2His words carry through the air and everyone looks at you.');
			sln('');
			sln('  Your heart hurts.');
			sln('');
			more_nomail();
		}
		else if (player.cha < 80) {
			sln('');
			sln('');
			sln('  Seth Able looks at you seriously.');
			sln('');
			lln('  `0"'+player.name+'`0.. I do love you - You are one of the most');
			sln('   wonderful women in the realm!  I still cannot say yes."');
			sln('');
			lln('  `2His words carry through the air and everyone looks at you.');
			sln('');
			sln('  Humiliation turns your face a deep scarlet.');
			sln('');
			more_nomail();
		}
		else if(player.cha < 100) {
			sln('');
			sln('');
			sln('  Seth Able looks at you seriously.');
			sln('');
			lln('  `0"'+player.name+'`0.. You are truly a thing of beauty.  Please');
			sln('   have mercy on me and do not hate me when I tell I cannot."');
			sln('');
			lln('  `2His words carry through the air and everyone looks at you.');
			sln('');
			sln('  A fierce anger burns inside.');
			sln('');
			more_nomail();
		}
		else if(player.cha < 120) {
			sln('');
			sln('');
			sln('  Seth Able looks at you very seriously.');
			sln('');
			lln('  `0"'+player.name+'`0.. You are so dear to me.  You are truly THE');
			sln('  most beautiful woman in the realm.  I pray to God you will');
			sln('  not hate me when I tell you I cannot mary you.  You see, I');
			sln('  was married once before.  My wife was killed many years ago.');
			sln('  I cannot go through that pain again.  I am so sorry.');
			sln('');
			lln('  `2His words carry through the air and everyone looks at you.');
			sln('');
			sln('  You see tears in the eyes of many onlookers.');
			sln('');
			more_nomail();
		}
		else {
			sln('');
			sln('');
			sln('  Seth Able looks at you very seriously.');
			sln('');
			lln('  `0"'+player.name+'`0.. The many times you\'ve visited me have');
			sln('   truly done my heart good."');
			sln('');
			more_nomail();
			sln('   "I do love you.  And if you\'ll have me, I will be the best');
			sln('   husband I can be for you."');
			sln('');
			more_nomail();
			lln('  `2The bard then takes you in his arms and squeezes you tightly.');
			sln('');
			sln('  The news of the wedding travels like wildfire - shortly, a vast');
			sln('  assemblege of townspeople are sitting rather impatiently in the');
			sln('  the great church found near the outskirts of town.');
			sln('');
			more_nomail();
			lln('  `c`%                    ** THE WEDDING **');
			sln('');
			if (psock !== undefined) {
				psock.writeln('SethMarried '+player.Record);
				tmp = psock.readln();
				if (tmp !== 'Yes' && tmp !== 'No') {
					throw('Out of sync with server after SethMarried');
				}
			}
			get_state(true);
			if (state.married_to_seth === -1 || state.married_to_seth === player.Record) {
				state.married_to_seth = player.Record;
				put_state();
				if (player.laid > 0) {
					sln('  Wearing a slightly off-white dress you walk demurely down the aisle.');
					log_line('`)  ** `0'+player.name+' `2has MARRIED `%Seth Able`2!`) **');
				}
				else {
					lln('  `0Wearing a PURE `%white `0dress you walk demurely down the asle.');
					sln('');
					lln('  `0Your heart is completely fulfilled.');
					log_line('`)  ** `0'+player.name+' `2has MARRIED `%Seth Able`2 in `%WHITE`2!`) **');
					sln('');
					player.exp += player.level * 5000;
					lln('  `%You feel heavenly!  You receive '+pretty_int(player.level * 5000)+' experience!');
				}
				sln('');
				lln('  `0You barely notice the ceremony taking place - your eyes and');
				sln('  your husbands never leave each other.  ');
				sln('');
				more_nomail();
				sln('  At the appropriate time, the bard kisses you deeply and long.');
				sln('');
				lln('  `%THE ENTIRE CHURCH ROARS ITS APPROVAL!');
				sln('');
				more_nomail();
				tournament_check();
			}
			else {
				put_state();
				mop = player_get(state.married_to_seth);
				if (player.laid > 0) {
					sln('  Wearing a slightly off-white dress you walk demurely to the aisle.');
				}
				else {
					lln('  `0Wearing a PURE `%white `0dress you walk demurely to the aisle.');
				}
				more_nomail();
				lln('  `0When you get there, you can\'t believe your ears...');
				lln('  `0"I now pronounce you husband and wife... you may kiss the bride"');
				sln('');
				lln('  `0Seth Able has just married '+mop.name+'!');
				more_nomail();
				lln('  `0You run away sobbing.');
			}
		}
	}

	if (player.seen_violet) {
		sln('  The Bard seems occupied...Maybe tomorrow...');
		sln('');
		return;
	}
	sln('');
	lrdfile('SETH');
	do {
		sln('');
		lw('  `0`2Your choice?  (`0? for menu`2) : ');
		ch = getkey().toUpperCase();
		if (ch === '\r') {
			ch = 'R';
		}
		sln(ch);
		sln('');
		switch (ch) {
			case 'N':
			case 'R':
			case 'Q':
				done = true;
				break;
			case '?':
				sln('');
				lrdfile('SETH');
				break;
			case 'W':
				wink();
				break;
			case 'F':
				flutter();
				break;
			case 'D':
				hanky();
				break;
			case 'A':
				drink();
				break;
			case 'K':
				kiss();
				break;
			case 'C':
				if (!settings.clean_mode) {
					seduce();
				}
				else {
					sln('');
					sln('');
					sln('  Sorry, your sysop has disabled that function...');
					sln('');
				}
				break;
			case 'M':
				get_state(false);
				if (player.divorced) {
					sln('  Since you\'ve just gotten out of a serious relationship, you feel');
					sln('  it\'d be moving to quickly to get married again.');
				}
				else if (player.married_to > -1) {
					op = player_get(player.married_to);
					sln('');
					lln('  `0You kind of doubt you husband `)'+op.name+' `0would');
					sln('  like that...');
					more_nomail();
				}
				else if (state.married_to_violet === player.Record) {
					sln('');
					lln('  `0You kind of doubt you wife `#Violet `0would');
					sln('  like that...');
					more_nomail();
				}
				else {
					marriage();
				}
				break;
		}
	} while((!done) && (!player.seen_violet));
	sln('');
}

function bard_song()
{
	sln('');
	sln('');
	if (player.seen_bard) {
		sln('  "I\'m sorry, but my throat is too dry..  Perhaps tomorrow."');
		sln('');
	}
	else {
		player.seen_bard = true;
		sln('');
		sln('');
		lln('  `%(Seth Able clears his throat)`2');
		sln('');
		if (random(2) === 1) {
			switch(random(5)) {
				case 0:
					lln('  `2..."The children are missing, the children are gone"...');
					sln('  ..."They have no pillow to lay upon,"...');
					sln('  ..."The hopes of the people are starting to dim"...');
					lln('  ..."They are gone, because the `4Dragon `2has eaten them"...');
					sln('');
					sln('  Tears run down your face.  You swear to avenge the children.');
					sln('');
					lln('  `#YOU RECEIVE AN EXTRA USER BATTLE FOR TODAY!');
					player.pvp_fights += 1;
					break;
				case 1:
					lln('  `2..."The Gods have powers, the Gods are just"...');
					sln('  ..."The Gods help us people, when they must"...');
					sln('  ..."Gods can heal the sick, even the cancered"...');
					sln('  ..."Pray to the Gods, and you will be answered"...');
					sln('');
					sln('  You find yourself wishing for more money.');
					sln('');
					lln('  `#SOMEWHERE, MAGIC HAS HAPPENED!');
					player.bank *= 2;
					if (player.bank > 2000000000) {
						player.bank = 2000000000;
					}
					break;
				case 2:
					lln('  `2..."This is the story of a bard"...');
					sln('  ..."Stabbed in the hand with a shard"...');
					sln('  ..."They said he\'d never play, but they would rue the day"...');
					sln('  ..."He practiced all the time, becoming good before long"...');
					sln('  ..."Able got his revenge, by proving them wrong"...');
					sln('');
					sln('  The song makes you feel sad, yet happy.');
					if (player.forest_fights < 32000) {
						player.forest_fights += 2;
						if (player.forest_fights > 32000) {
							player.forest_fights = 32000;
						}
						sln('');
						lln('  `#YOU RECEIVE TWO MORE FOREST FIGHTS FOR TODAY!');
					}
					break;
				case 3:
					lln('  `2..."Let me tell you the Legend, of the Red Dragon"...');
					sln('  ..."They said he was old, that his claws were saggin"...');
					sln('  ..."It\'s not so, don\'t be fooled"...');
					sln('  ..."He\'s alive and well, still killing where he ruled"...');
					sln('');
					sln('  The song makes you feel a strange wonder, an awakening.');
					if (player.forest_fights < 32000) {
						player.forest_fights += 1;
						sln('');
						lln('  `#YOU RECEIVE ONE MORE FOREST FIGHT FOR TODAY!');
					}
					break;
				case 4:
					lln('  `2..."&hero `2has a story, that must be told"...');
					sln('  ..."He is already a legend, and he ain\'t even old"...');
					sln('  ..."He can drink a river of blood and not burst"...');
					sln('  ..."He can swallow a desert and never thirst"...');
					sln('');
					sln('  The hero has inspired you, and in doing so, made you a better warrior.');
					sln('');
					lln('  `#YOUR HITPOINTS INCREASE BY ONE!');
					player.hp_max += 1;
					if (player.hp_max > 32000) {
						player.hp_max = 32000;
					}
					break;
			}
		}
		else {
			if (player.sex === 'M') {
				switch(random(4)) {
					case 0:
						lln('  `2..."`0'+player.name+' `2was a warrior, a man"...');
						sln('  ..."When he wants to do a thing, he can"...');
						sln('  ..."To live, to die, it\'s all the same"...');
						sln('  ..."But it is better to die, than live in shame"...');
						sln('');
						sln('  The song makes you feel your heritage.');
						sln('');
						lln('  `#YOUR HIT POINTS ARE MAXED OUT!');
						player.hp = player.hp_max;
						break;
					case 1:
						lln('  `2..."There once was a warrior, with a beard"...');
						lln('  ..."He had a power, yes `0'+player.name+' `2was feared"...');
						sln('  ..."Nothing he did, could ever be wrong"...');
						sln('  ..."He was quick, and he was strong"...');
						sln('');
						sln('  The song makes you feel powerful!');
						if (player.forest_fights < 32000) {
							player.forest_fights += 3;
							if (player.forest_fights > 32000) {
								player.forest_fights = 32000;
							}
							sln('');
							lln('  `#YOU RECEIVE THREE MORE FOREST FIGHTS FOR TODAY!');
						}
						break;
					case 2:
						lln('  `2..."Waiting in the forest waiting for his prey"...');
						lln('  ..."`0'+player.name+' `2didn\'t care what they would say"...');
						sln('  ..."He killed in the town, the lands"...');
						sln('  ..."He wanted evil\'s blood on his hands"...');
						sln('');
						sln('  The song makes you feel powerful!');
						if (player.forest_fights < 32000) {
							player.forest_fights += 3;
							if (player.forest_fights > 32000) {
								player.forest_fights = 32000;
							}
							sln('');
							lln('  `#YOU RECEIVE THREE MORE FOREST FIGHTS FOR TODAY!');
						}
						break;
					case 3:
						lln('  `2..."A true man was `0'+player.name+'`2, a warrior proud"...');
						sln('  ..."He voiced his opinions meekly, never very loud"...');
						lln('  ..."But he ain\'t no wimp, he took `#Violet`2 to bed"...');
						sln('  ..."He\'s definately a man, at least that\'s what she said!"...');
						sln('');
						sln('  The song makes you glad you are male!');
						if (player.forest_fights < 32000) {
							player.forest_fights += 2;
							if (player.forest_fights > 32000) {
								player.forest_fights = 32000;
							}
							sln('');
							lln('  `#YOU RECEIVE TWO MORE FOREST FIGHTS FOR TODAY!');
						}
						break;
				}
			}
			else {
				switch(random(2)) {
					case 0:
						lln('  `2..."`0'+player.name+' `2was a warrior, a queen"...');
						sln('  ..."She was a beauty, and she was mean"...');
						sln('  ..."She could melt a heart, at a glance"...');
						sln('  ..."And men would pay, to see her dance!"...');
						sln('');
						sln('  The song makes you feel pretty!');
						sln('');
						lln('  `#YOU RECEIVE A CHARM POINT!');
						player.cha += 1;
						if (player.cha > 32000) {
							player.cha = 32000;
						}
						break;
					case 1:
						lln('  `2..."There once was a woman, of exceeding fame"...');
						lln('  ..."She had a power, `0'+player.name+' `2was her name"...');
						sln('  ..."Nothing she did, could ever be wrong"...');
						sln('  ..."She was quick, and she was strong"...');
						sln('');
						sln('  The song makes you feel powerful!');
						if (player.forest_fights < 32000) {
							player.forest_fights += 3;
							if (player.forest_fights > 32000) {
								player.forest_fights = 32000;
							}
							sln('');
							lln('  `#YOU RECEIVE THREE MORE FOREST FIGHTS FOR TODAY!');
						}
						break;
				}
			}
		}
	}
}

function talk_to_bard()
{
	var ch;
	var done = false;
	var op;

	function menu() {
		lrdfile(player.sex === 'M' ? 'BARD' : 'BARDF');
	}

	get_state(false);
	menu();
	do {
		sln('');
		if (state.married_to_seth === player.Record) {
			lln('  `2Seth Able looks at you lovingly. (`0? for menu`2)');
		}
		else {
			lln('  `2Seth Able looks at you expectantly.  (`0? for menu`2)');
		}
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch (ch) {
			case '?':
				menu();
				break;
			case 'R':
			case 'Q':
			case '\r':
				done = true;
				break;
			case 'A':
				bard_song();
				break;
			case 'F':
				if (player.sex === 'M') {
					sln('');
					sln('  You would rather flirt with Violet.');
					break;
				}
				get_state(false);
				if (state.married_to_seth > -1) {
					if (state.married_to_seth === player.Record) {
						sln('');
						if (player.seen_violet) {
							lln('  `2You don\'t wish to interrupt his song.');
						}
						else {
							player.seen_violet = true;
							switch (random(8)) {
								case 0:
									lln('  `0"I love you too, honey!"');
									break;
								case 1:
									lln('  `2Seth gives you a quick peck on the cheek.');
									break;
								case 2:
									lln('  `0"I\'d love to sweetie, but duty calls."');
									break;
								case 3:
									lln('  `0"We\'ll have plenty of time for that tonight!"');
									break;
								case 4:
									lln('  `2In the roar of the crowd, he doesn\'t hear.');
									break;
								case 5:
									lln('  `2You don\'t wish to bother your busy husband.');
									break;
								case 6:
									lln('  `0"After work, you\'re mine!" `2he laughs.');
									break;
								case 7:
									lln('  `0"Later, honey!" `2Seth winks hugely at you.');
									break;
							}
						}
					}
					else {
						sln('');
						foreground(2);
						op = player_get(state.married_to_seth);
						if (player.seen_violet) {
							if (player.cha > op.cha) {
								sln('  You decide you\'ve done enough ');
								sln('  homewrecking for one day.');
							}
							else {
								sln('  You are too shamed to even think');
								sln('  such a thought.');
							}
						}
						else {
							sln('  You are about to give Seth your best, when');
							lln('  he turns away.  You see a shiny new `0ring `2on');
							sln('  his hand. ');
							sln('');
							lw('  Attempt to seduce him anyway? `![`0N`!] : ');
							ch = getkey().toUpperCase();
							if (ch !== 'Y') {
								ch = 'N';
							}
							sln(ch);
							sln('');
							if (ch === 'Y') {
								sln('  You rub your body against the man in');
								sln('  the most disarming way you can.');
								sln('');
								more_nomail();
								player.seen_violet = true;
								if (player.cha > op.cha) {
									lln('`2  Seth looks at you hungrily.');
									sln('');
									lln('  `0"Dear '+player.name+'`0..  You make me');
									lln('  think unworthy thoughts!" `2the bard');
									sln('  cries in agony.');
									sln('');
									log_line('  `0'+player.name+' `2has made `%Seth Able`2 falter.');
									mail_to(state.married_to_seth, '\n' +
										'  `0**** `)UGLY RUMOR `0****\n' +
										'`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n' +
										'  `2Anonymous sources tell you '+player.name+'`2 was seen' +
										'  `2attempting to seduce your husband!');
									more_nomail();
									lln('  `2You have caused the bard to falter!');
									sln('');
									lln('  `%YOU RECEIVE '+pretty_int(player.level * 100)+' EXPERIENCE!');
									player.exp += player.level * 100;
									tournament_check();
								}
								else {
									lln('`2  Seth looks at you angrily.');
									sln('');
									lln('  `0"'+player.name+'`0..  You are an extremely');
									sln('  silly wench.  My God woman, you');
									sln('  would need at least '+pretty_int((op.cha)-player.cha)+' more');
									sln('  charm to be considered female!"');
									sln('');
									log_line('  `%Seth Able`2 called `0'+player.name+' `2a silly wench!');
								}
							}
						}
					}
				}
				else {
					single_seth();
				}
				break;
		}
	} while(!done);
	sln('');
}

function announce()
{
	var ch;
	var msg = '';
	var l;

	sln('');
	sln('');
	foreground(2);
	sln('  Are you sure you want to announce something?  It will appear to');
	sln('  EVERYONE in the daily happenings.');
	sln('');
	lw('  Make Announcement? [`0Y`2] :`% ');
	ch = getkey().toUpperCase();
	if (ch !== 'N') {
		ch = 'Y';
	}
	sln(ch);
	if (ch === 'N') {
		return;
	}

	msg = ' \n  `0'+player.name+'`2 Announces:`%';
	sln('');
	sln('  Enter message now..Blank line quits!');
	do {
		lw(' `2>`% ');
		l = '  `%' + clean_str(dk.console.getstr({len:75}));
		if (l !== '  `%') {
			msg = msg + '\n' + l;
		}
	} while (l !== '  `%');
	log_line(msg);
	sln('');
	sln('  Announcement Made!');
}

function single_violet()
{
	var ch;
	var op;

	function no() {
		sln('');
		sln('  Your hopeful smile slowly turns into a look of deep sadness.');
		sln('');
		lln('  `%THE ENTIRE BAR LAUGHS AT YOUR MISFORTUNE!');
		sln('');
		player.seen_violet = true;
		log_line('`2  `#Violet`2 has refused to marry `0'+player.name+'`2!');
		more();
	}

	function marry() {
		var mop;

		if (psock !== undefined) {
			psock.writeln('VioletMarried '+player.Record);
			if (['Yes','No'].indexOf(psock.readln()) === -1) {
				throw('Out of sync with server after VioletMarried -1');
			}
		}
		get_state(true);
		if (state.married_to_violet !== -1 && state.married_to_violet !== player.Record) {
			put_state();
			mop = player_get(state.married_to_violet);
			lln('`c                        `%** THE BLESSED DAY ARRIVES **`0');
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			lln('  `2As you walk up to the chapel, you see Violet walking out...');
			lln('  `2on the arm of '+mop.name+'!');
			sln('');
		}
		else {
			state.married_to_violet = player.Record;
			put_state();
			lln('`c                        `%** THE BLESSED DAY ARRIVES **`0');
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			lln('  `2Violet agrees to marry you!');
			sln('');
			sln('  After a short ceremony you are finally able to take her into your arms.');
			sln('  (Well, you\'ve done that quite a few times, but not as your wife!)');
			sln('');
			sln('  She agrees to quit her job and take care of your house.');
			sln('');
			sln('  You look forward to inspiration from her every day.');
			sln('');
			lln('  `%YOU RECEIVE '+pretty_int(1000 * player.level)+' EXPERIENCE!');
			player.exp += player.level * 1000;
			sln('');
			player.seen_violet = false;
			// DIFF: The second line wasn't going into the log...
			log_line('`2  `#Violet`2 has `%MARRIED `0'+player.name+'`2!!!!!\n`2  She `%QUITS`2 her job at the bar to the towns dismay!');
			tournament_check();
		}
	}

	function marriage() {
		if (player.divorced) {
			sln('  Since you\'ve just gotten out of a serious relationship, you feel');
			sln('  it\'d be moving to quickly to get married again.');
			return;
		}
		sln('');
		lln('  `2You take Violet\'s hand and squeeze it gently.  `0"My sweet Violet.');
		sln('  will you make me the happiest man alive?  Will you marry me?"');
		sln('');
		lw('  `2With your heart on your sleeve, you await her answer.');
		mswait(1500);
		sw('.');
		mswait(1500);
		sw('.');
		mswait(1500);
		sln('');
		sln('');

		if (player.cha < 3) {
			lln('`#  "Marry YOU?!" `2 She laughs as if she just heard the funniest joke on');
			sln('   earth.');
		}
		else if(player.cha < 6) {
			lln('`#  "Is this a joke?"`2 she asks seriously.');
		}
		else if(player.cha < 10) {
			lln('`#  "You\'re sweet, but I just don\'t like you in that way." `2she tells you');
			sln('   seriously.');
		}
		else if (player.cha < 50) {
			lln('`#  "I like you, I really do!  I just don\'t feel ready for that kind of');
			lln('   commitment right now!" `2she explains sadly.');
		}
		else if(player.cha < 80) {
			lln('`#  "I think I\'m in love with you!  But I can\'t marry you.  My last');
			lln('   marriage was a disaster." `2she explains sadly.');
		}
		else if(player.cha < 100) {
			lln('`#  "I...I can\'t say yes.  Ask me another time. Please." `2she begs.');
		}
		else {
			lln('`#  "Yes!  I WILL marry you!" `2she laughs!');
			sln('');
		}

		if (player.cha < 100) {
			no();
		}
		else {
			marry();
		}
		more_nomail();
	}

	function carry() {
		sln('  You pick Violet up and roughly carry her upstairs and throw her on');
		sln('  the bed...');
		mswait(1500);
		sln('');
		player.seen_violet = true;
		if (player.married_to > -1) {
			sln('');
			lln('  `#Violet `0is appalled that you would even suggest such');
			sln('  a thing!');
			sln('');
			sln('  She calls you a dirty old man!');
			log_line('`#  Violet `2calls `0'+player.name+' `2a `)'+(random(2) ? 'dirty old man' : 'bastard')+'`2!');
		}
		else {
			if (player.cha > 32) {
				if (random(3) === 1) {
					lln('  `0She smiles invitingly...  ');
					mswait(1500);
					sln('');
					lln('  `2Half an hour later....You saunter downstairs. When a bearded drunk');
					sln('  asks you all the racket was, you smile knowingly and look away...');
					sln('  The drunks are mystified!');
					sln('');
					lln('  `0YOU GET `%'+pretty_int(player.level * 40)+'`0 EXPERIENCE!');
					player.exp += player.level * 40;
					log_line('`5  '+player.name+' `%Got laid by `#Violet`%!');
					player.laid += 1;
					tournament_check();
				}
				else {
					switch(random(4)) {
						case 0:
							sln('  She tells you she has a headache!');
							sln('  You saunter down to the bar disappointed.');
							break;
						case 1:
							sln('  She tells you she has to wash her hair!');
							sln('  You saunter down to the bar disappointed.');
							break;
						case 2:
							sln('  She tells you she is just too busy!');
							sln('  You saunter down to the bar disappointed.');
							break;
						case 3:
							sln('  She tells you she just doesn\'t have the urge today!');
							sln('  You saunter down to the bar - Wondering if she\'s been faithful!?');
							break;
					}
				}
			}
			else {
				sln('');
				foreground(4);
				sln('  She jumps off the bed screaming at you!  She savagely kicks you in the');
				sln('  groin!  You trudge down the stairs dejectedly..');
				sln('  The entire bar laughs at your misfortune!!');
				sln('');
				sln('  YOUR HITPOINTS GO DOWN TO 1!');
				player.hp = 1;
				log_line('`0  '+player.name+' `2got kicked in the groin by `#Violet`2!');
			}
		}
		more();
	}

	function wink() {
		player.seen_violet = true;
		sln('');
		lln('  `%You wink at Violet seductively..');
		mswait(1500);
		if (player.cha >= 1) {
			player.exp += player.level * 5;
			lln('  `2She blushes and smiles!!');
			sln('  You are making progress with her!');
			sln('');
			lln('  `%You receive '+pretty_int(player.level * 5)+' experience!');
			tournament_check();
		}
		else {
			lln('  `4She dumps a tankard of ale on your head!!');
			sln('  The entire bar laughs at your misfortune!!');
		}
		more();
	}

	function kiss() {
		player.seen_violet = true;
		if (player.cha >= 2) {
			lln('  `%You take Violet\'s hand and kiss it!');
			mswait(1500);
			lln('  `2She pulls her hand away and laughs merrily!!');
			sln('  Your relationship with her is taking off!');
			sln('');
			lln('  `%You receive '+pretty_int(player.level*10)+' experience!');
			player.ext += player.level * 10;
		}
		else {
			lln('  `%You take her hand and kiss it!');
			mswait(1500);
			lln('  `2She pulls her hand away and slaps your face!');
			sln('  The entire bar laughs at your misfortune!!');
			sln('');
			lln('  `4YOU LOSE '+pretty_int(player.level)+' HIT POINTS!');
			player.hp -= player.level;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	function peck() {
		lln('  `%You attempt to meekly kiss her on the lips, but...');
		mswait(1500);
		player.seen_violet = true;
		if (player.cha >= 4) {
			player.exp += player.level * 20;
			lln('  `2She pulls you into a rough embrace, and kisses you');
			sln('  soundly!!   You are elated!');
			sln('');
			lln('  `%YOU RECEIVE '+pretty_int(player.level*20)+' EXPERIENCE!');
			tournament_check();
		}
		else {
			lln('  `2She pushes you away and kicks you in the shin!');
			sln('  The entire bar laughs at your misfortune!!');
			sln('');
			lln('  `4YOU LOSE '+pretty_int(player.level*3)+' HIT POINTS!');
			player.hp -= player.level * 3;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	function sit() {
		lln('  `%You grab her and throw her on your lap...');
		mswait(1500);
		player.seen_violet = true;
		if (player.cha >= 8) {
			lln('  `2She laughs and hugs you around the neck!!');
			sln('  You cherish her warmth!!');
			sln('');
			lln('  `%YOU RECEIVE '+pretty_int(player.level*30)+' EXPERIENCE!');
			player.exp += player.level * 30;
			tournament_check();
		}
		else {
			sln('  She jumps off your lap, and punches you hard!');
			sln('  Your chair tips over backwards!');
			sln('  The entire bar laughs at your misfortune!!');
			lln('');
			player.hp -= player.level * 5;
			if (player.hp < 1) {
				player.hp = 1;
			}
			lln('  `4YOU LOSE '+pretty_int(player.level*5)+' HIT POINTS!');
		}
		more();
	}

	function grab() {
		lln('  `%You palm her shapely cheeks...`2');
		mswait(1500);
		player.seen_violet = true;
		if (player.cha >= 16) {
			player.exp += player.level * 40;
			sln('  She doesn\'t object!  ');
			sln('  You become over excited and force your hand away!');
			sln('');
			lln('  `%YOU RECEIVE '+pretty_int(player.level*40)+' EXPERIENCE!');
			tournament_check();
		}
		else {
			sln('  She breaks a tankard over your head!');
			sln('  You swoon and fall on your face!');
			sln('  The entire bar laughs at your misfortune!!');
			sln('');
			lln('  `4YOU LOSE '+pretty_int(player.level*10)+' HITPOINTS!');
			player.hp -= player.level * 10;
			if (player.hp < 1) {
				player.hp = 1;
			}
		}
		more();
	}

	sln('');
	lrdfile('VIOLET');
	do {
		sln('');
		lw('  `0`2Your choice?  (`0? for menu`2) : ');
		ch = getkey().toUpperCase();
		if (ch === '\r') {
			ch = 'R';
		}
		sln(ch);
		sln('');
		switch(ch) {
			case '?':
				sln('');
				lrdfile('VIOLET');
				break;
			case 'N':
			case 'R':
			case 'Q':
				return;
			case 'K':
				kiss();
				break;
			case 'W':
				wink();
				break;
			case 'P':
				peck();
				break;
			case 'S':
				sit();
				break;
			case 'G':
				grab();
				break;
			case 'C':
				if (settings.clean_mode) {
					sln('');
					sln('');
					sln('  Sorry, your sysop has disabled that function!');
					sln('');
				}
				else {
					carry();
				}
				break;
			case 'M':
				get_state(false);
				if (player.married_to > -1) {
					op = player_get(player.married_to);
					sln('');
					lln('  `0You kind of doubt you wife `)'+op.name+' `0would');
					sln('  like that...');
					more_nomail();
					return;
				}
				else if (state.married_to_seth === player.Record) {
					sln('');
					lln('  `0You kind of doubt you husband `%Seth Able `0would');
					sln('  like that...');
					more_nomail();
					return;
				}
				else {
					marriage();
					return;
				}
			}
	} while(!player.seen_violet);
}

function flirt_with_violet()
{
	var op;

	if (player.sex === 'F') {
		sln('');
		sln('');
		sln('  You would rather flirt with Seth Able.');
		return;
	}
	get_state(false);
	if (state.married_to_violet > -1) {
		if (player.seen_violet) {
			sln('');
			sln('');
			lln('  `2You are still shaking from your last encounter with `4Grizelda`2!');
			sln('');
			more();
			return;
		}
		else {
			sln('');
			sln('');
			sln('  You whistle loudly for the barmaid, hardly containing your glee at the');
			sln('  thought of patting Violet\'s soft supple hips, but when you pat...');
			sln('');
			more();
			lln('  `4YOU FEEL HUGE LUMPS OF CELLULITE!');
			sln('');
			lln('`2  The obese barmaid intruduces her portly self as `4Grizelda`2!');
			sln('');
			if (state.married_to_violet === player.Record) {
				sln('  You remember now that Violet quit work when she married you!');
				sln('');
				sln('  You feel sorry for all the other warriors who must suffer at the hands of');
				lln('  `4Grizelda`2.');
			}
			else {
				op = player_get(state.married_to_violet);
				lln('  You suddenly remember seeing something in the news about `0'+op.name +'`2');
				sln('  marrying Violet!  As Grizelda grabs you for a kiss, her buckteeth jab');
				lln('  you painfully.  You curse `0'+op.name+'`2 as you scream in horror.');
			}
			sln('');
			player.seen_violet = true;
			more();
			return;
		}
	}
	if (player.seen_violet) {
		sln('');
		sln('');
		sln('  You feel you had better not go too fast, maybe tomorrow.');
		sln('');
		return;
	}
	single_violet();
}

function red_dragon_inn()
{
	var ch;
	var ret = 0;
	var done = false;

	function menu()
	{
		if (!player.expert) {
			lrdfile(player.sex === 'M' ? 'RDI' : 'RDIF');	// TODO: RDI is ANSI, RDIF is lordtext!
		}
	}

	foreground(5);
	sln('');
	player.inn = false;
	sclrscr();
	menu();

	do {
		check_mail();
		foreground(5);
		sln('');
		lln('  The Red Dragon Inn   `8(? for menu)');
		sln('  (C,D,F,T,G,V,H,M,R)');
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch (ch) {
			case 'R':
			case 'Q':
			case '\r':
				done=true;
				break;
			case '?':
				menu();
				break;
			case 'V':
			case 'Y':
				show_stats();
				break;
			case 'W':
				compose_mail();
				break;
			case 'G':
				if (get_a_room()) {
					ret = -1;
					done = true;
				}
				break;
			case 'D':
				show_log();
				menu();
				break;
			case 'T':
				talk_with_bartender();
				if (player.dead) {
					done = true;
				}
				break;
			case 'C':
				converse(false);
				break;
			case 'H':
				talk_to_bard();
				break;
			case 'M':
				announce();
				break;
			case 'F':
				flirt_with_violet();
				break;
		}
	} while (!done);
	return ret;
}

function king_arthurs()
{
	var over = false;
	var ch;

	function display_weapons() {
		var i;
		var w;
		var l;
		var p;
		var n;
		var mc = morechk;

		morechk = false;
		for (i = 1; i < 16; i += 1) {
			w = get_weapon(i);
			l = w.name;
			p = pretty_int(w.price);
			n = format('%2d', i);
			while (disp_len(l) + p.length < 42) {
				l += '.';
			}
			dk.console.gotoxy(17, i + 3);
			lln('`2' + n + '. ' + l + '`0'+p);
		}
		morechk = mc;
	}

	function sell_weapon() {
		var oldw;
		var neww;
		var price;
		var mult;
		var ich;

		lln('`c  `%King Arthurs Weapons');
		foreground(2);
		sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		sln('');
		if (player.weapon_num === 0) {
			lln('  `2"`0What the...?!!`2" the stout man shouts. `2"`0You don\'t have');
			lln('  a weapon to sell!`2"');
			sln('');
			sln('');
			more();
			return;
		}
		oldw = get_weapon(player.weapon_num);
		mult = player.level * player.cha * player.level;
		if (mult > 0 && mult < 65530) {
			price = parseInt(oldw.price / 2 + random(mult));
		}
		else {
			price = parseInt(oldw.price / 2 + random(65535));
		}
		if (price > oldw.price - (price / 3)) {
			price = oldw.price - parseInt(oldw.price / 3);
		}
		lln('  `2"`0Hmmm I will buy your `%'+player.weapon+'`0 for `%'+pretty_int(price)+'`0, Agreed?`2"');
		sln('');
		lw('   Sell it?  [`0N`2] : `%');
		ich = getkey().toUpperCase();
		if (ich !== 'Y') {
			ich = 'N';
		}
		sln(ich);
		if (ich !== 'Y') {
			sln('');
			lln('  `2"`0You don\'t want to sell?!  Fine!  I don\'t want your stinken\' weapon!`2"');
			sln('');
			more_nomail();
			return;
		}
		sln('');
		lln('  `2"`0Great!`2" The fat man takes your weapon, and gives you the');
		sln('  money.');
		player.weapon_num = 0;
		neww = get_weapon(player.weapon_num);
		player.weapon = neww.name;
		player.gold += price;
		if (player.gold > 2000000000) {
			player.gold = 2000000000;
			lln('Wow, you have a lot of money!');
		}
		player.str -= oldw.num;
		if (player.str < 5) {
			player.str = 5;
		}
		more_nomail();
	}

	function str_needed(weap) {
		var t;
		var i;
		var ret = 10;

		if (!settings.shop_limit) {
			return 0;
		}
		if (weap < 3) {
			return 0;

		}
		for (i = 1; i < weap - 1 && i <trainer_stats.length; i += 1) {
			t = get_trainer(i);
			ret += t.str_gained;
		}
		return ret;
	}

	function buy_weapon() {
		var n;
		var oldw;
		var neww;
		var need;
		var ich;

		sclrscr();
		if (!player.expert) {
			sclrscr();
			lrdfile('BUYWEP');
			display_weapons();
		}
		sln('');
		sln('');
		lln('  `2(`0Gold: `%'+pretty_int(player.gold)+'`2)  (`00 to exit`2)');
		lw('  `0Number Of Weapon `2: `%');
		n = parseInt(dk.console.getstr({len:2, integer:true}), 10);
		if (isNaN(n)) {
			n = 0;
		}
		if (n > 0 && n < 16) {
			oldw = get_weapon(player.weapon_num);
			neww = get_weapon(n);
			need = str_needed(n);
			sclrscr();
			sln('');
			sln('');
			foreground(15);
			sln('  King Arthurs Weapons');
			foreground(2);
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			sln('');
			lln('  `2"`0Hmmm I will sell you my FAVORITE `%'+neww.name+'`0 for `%'+pretty_int(neww.price)+' `0gold!`2"');
			sln('');
			sln('');
			lln('  `2Note: It takes `%'+pretty_int(need)+' `2strength points to weild this weapon.');
			lln('  `2You currently have `%'+pretty_int(player.str - oldw.num) + ' `2strength points.');
			sln('');
			lw('  `2Buy it?  [`0N`2] : `%');
			ich = getkey().toUpperCase();
			if (ich !== 'Y') {
				ich = 'N';
			}
			sln(ich);
			if (ich === 'N') {
				sln('');
				lln('  `2"`0Fine..You will come back...`2" the man grunts.');
				sln('');
				more_nomail();
				return;
			}
			if (player.str < need) {
				sln('');
				lln('  `2"`0You silly fool! You aren\'t strong enough to carry');
				lln('  that weapon!`2"');
				sln('');
				more_nomail();
				return;
			}
			if (player.weapon_num > 0) {
				sln('');
				lln('  `2"`0You fool!  You already have a weapon, and you can\'t carry');
				lln('  two!`2"  You realize he is right.');
				sln('');
				more_nomail();
				return;
			}
			if (player.gold < neww.price) {
				sln('');
				lln('  `2"`0You stupid fool!  You don\'t have that much gold!');
				lln('  I knew you were up to no good the moment I saw you!`2"');
				sln('');
				sln('');
				// DIFF: This more wasn't here...
				more_nomail();
				return;
			}
			sln('');
			lln('  `2"`0Great!`2" The fat man takes your money, and gives you the');
			sln('  weapon.');
			player.weapon_num = n;
			player.weapon = neww.name;
			player.gold -= neww.price;
			player.str += neww.num;
			if (player.str > 32000) {
				player.str = 32000;
			}
			sln('');
			more_nomail();
		}
	}

	function menu() {
		if (!player.expert) {
			lrdfile('ARTHUR');
		}
	}

	function prompt() {
		sln('');
		lln('  `2Current weapon: `0'+player.weapon);
		lln('  `5King Arthur\'s Weapons `8(B,S,Y,R)  (? for menu)');
	}

	sclrscr();
	menu();

	do {
		prompt();
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch (ch) {
			case 'L':
				sclrscr();
				lrdfile('BUYWEP');
				display_weapons();
				break;
			case 'Y':
				show_stats();
				break;
			case 'S':
				sell_weapon();
				break;
			case 'B':
				buy_weapon();
				break;
			case 'R':
			case 'Q':
			case '\r':
				over = true;
				break;
			// DIFF ' ' would cycle, but not call prompt()...
			case '?':
				if (player.expert) {
					lrdfile('ARTHUR');
				}
				menu();
				break;
		}
	} while (!over);
}

function abduls_armour()
{
	var over = false;
	var ch;

	function display_armour() {
		var i;
		var a;
		var l;
		var p;
		var n;
		var mc = morechk;

		morechk = false;
		for (i = 1; i < 16; i += 1) {
			a = get_armour(i);
			l = a.name;
			p = pretty_int(a.price);
			n = format('%2d', i);
			while (disp_len(l) + p.length < 42) {
				l += '.';
			}
			dk.console.gotoxy(17, i + 3);
			lln('`2' + n + '. ' + l + '`0'+p);
		}
		morechk = mc;
	}

	function def_needed(arm) {
		var t;
		var i;
		var ret = 0;

		if (!settings.shop_limit) {
			return 0;
		}
		if (arm < 3) {
			return 0;
		}
		for (i = 1; i < arm - 1 && i < trainer_stats.length; i += 1) {
			t = get_trainer(i);
			ret += t.def;
		}
		return ret;
	}

	function buy_armour() {
		var n;
		var olda;
		var newa;
		var need;
		var ich;

		if (!player.expert) {
			sclrscr();
			lrdfile('BUYARM');
			background(0);
			display_armour();
		}
		sln('');
		sln('');
		lln('`2  (`0Gold: `%'+pretty_int(player.gold)+'`2)  (`00 to Exit`2) ');
		lw('  `0Number Of Armour `2: `%');
		n = parseInt(dk.console.getstr({len:2, integer:true}), 10);
		if (isNaN(n)) {
			n = 0;
		}
		if (n > 0 && n < 16) {
			newa = get_armour(n);
			olda = get_armour(player.arm_num);
			need = def_needed(n);
			sclrscr();
			sln('');
			sln('');
			foreground(15);
			sln('  Abduls Armour Shop');
			foreground(2);
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			sln('');
			lln('  "`0Hmmm I will sell you a nice `%'+newa.name+'`0 for `%'+pretty_int(newa.price)+'`0.');
			lln('   Agreed, friend?`2"');
			sln('');
			sln('');
			foreground(2);
			lln('  Note: It takes `%'+pretty_int(need)+'`2 defense points to wear this armor.');
			lln('  `2You currently have `%'+pretty_int(player.def - olda.num)+' `2defense points.');
			sln('');
			lw('   `2Buy it?  [`0N`2] : `%');
			ich = getkey().toUpperCase();
			if (ich !== 'Y') {
				ich = 'N';
			}
			sln(ich);
			sln('');
			if (ich === 'N') {
				lln('`2  "`0Ok!  No rush!`2" the girl smiles.');
			}
			else if (player.def < need) {
				lln('  `2"`0I\'m sorry, but you are not strong enough to wear');
				lln('  that armor.`2"');
			}
			else if (player.arm_num > 0) {
				lln('  `2"`0You already have armour, and you can\'t wear');
				lln('  two!`2" You realize she is right.');
			}
			else if (player.gold < newa.price) {
				lln('  `2"`0I\'m sorry, but you seem to be lacking funds at the');
				lln('  moment.`2"  the girl tells you.');
			}
			else {
				lln('  `2"`0Wonderful!`2" The girl takes your money, and helps you');
				sln('  into your new armour.');
				player.arm = newa.name;
				player.arm_num = n;
				player.gold -= newa.price;
				player.def += newa.num;
				// DIFF: Used to compare to 3200 and set to 32000!
				if (player.def > 32000) {
					player.def = 32000;
				}
			}
			sln('');
			more_nomail();
		}
	}

	function sell_armour() {
		var ich;
		var olda;
		var mult;
		var price;

		sclrscr();
		foreground(15);
		sln('  Abduls Armour Shop');
		foreground(2);
		sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		sln('');
		if (player.arm_num === 0) {
			lln('  `2"`0You silly kidder!!`2" Paula laughs, "`0You don\'t have');
			lln('  any armour to sell!`2"  ');
			sln('');
			sln('');
			more();
			return;
		}
		olda = get_armour(player.arm_num);
		mult = player.level * player.cha * player.level;
		if (mult >= 0 && mult <= 65530) {
			price = parseInt(olda.price  / 2 + random(mult), 10);
		}
		else {
			price = parseInt(olda.price  / 2 + random(65530), 10);
		}
		if (price > olda.price - (price / 3)) {
			price = olda.price - parseInt(olda.price / 3, 10);
		}
		lln('  `2"`0Hmmm I will buy your `%'+player.arm   +' `0for `%'+pretty_int(price)+'`0 gold.');
		lln('  Agreed, friend?`2"');
		sln('');
		lw('  `2Sell it?  [`0N`2] : `%');
		ich = getkey().toUpperCase();
		// DIFF: This was after the print...
		if (ich !== 'Y') {
			ich = 'N';
		}
		sln(ich);
		if (ich === 'N') {
			sln('');
			lln('  `2"`0Thats ok.  Your armour probably has sentimental value to you.`2"');
			return;
		}
		sln('');
		lln('  `2"`0Good doing business with you!`2" The girl takes your armour');
		sln('  and gives you the money.');
		player.arm = 'Nothing!';
		player.arm_num = 0;
		player.gold += price;
		if (player.gold > 2000000000) {
			player.gold = 2000000000;
			sln('Wow, you have a lot of money!');
		}
		player.def -= olda.num;
		if (player.def < 1) {
			player.def = 0;
		}
		more_nomail();
	}

	function menu() {
		sclrscr();
		if (!player.expert) {
			lrdfile('ABDUL');
		}
	}

	menu();

	do {
		sln('');
		lln('  `2Current armour: `0'+player.arm);
		lln('  `5Abduls Armour `8(B,S,Y,R)  (? for menu)');
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch(ch) {
			case 'L':
				sclrscr();
				lrdfile('BUYARM');
				display_armour();
				break;
			case 'V':
			case 'Y':
				show_stats();
				break;
			case 'S':
				sell_armour();
				break;
			case 'B':
				buy_armour();
				break;
			case 'R':
			case 'Q':
			case '\r':
				over = true;
				break;
			case '?':
				if (player.expert) {
					lrdfile('ABDUL');
				}
				menu();
				break;
		}
	} while(!over);
}

function raise_class()
{
	sln('');

	if (player.skillw > 39 && player.skillm > 39 && player.skillt > 39) {
		lln('  `%** `0YOU HAVE ALREADY MASTERED ALL SKILLS `%**');
		return;
	}
	switch(player.clss) {
		case 1:
			if (player.skillw > 39) {
				lln('  `%** `0YOU HAVE ALREADY MASTERED THIS CLASS `%**');
				sln('');
				return;
			}
			break;
		case 2:
			if (player.skillm > 39) {
				lln('  `%** `0YOU HAVE ALREADY MASTERED THIS CLASS `%**');
				sln('');
				return;
			}
			break;
		case 3:
			if (player.skillt > 39) {
				lln('  `%** `0YOU HAVE ALREADY MASTERED THIS CLASS `%**');
				sln('');
				return;
			}
			break;
	}

	lln('  `%** `0YOUR CLASS SKILL IS RAISED BY ONE! `%**');
	sln('');
	if (player.clss === 1) {
		player.skillw += 1;
		if (settings.old_skill_points) {
			switch (player.skillw % 5) {
				case 0:
					lln('  `2You now have `0'+pretty_int(parseInt(player.skillw / 4, 10))+'`2 uses of Death Knight Skills a day.');
					player.levelw += 1;
					sln('');
					if (player.skillw === 40) {
						sln('  You have mastered The Death Knight Skills Completely.  You may choose to');
						sln('  learn a NEW skill now.');
						sln('');
						player.clss = choose_profession(false);
					}
					else {
						sln('  (five more lessons needed for next raise in uses per day)');
					}
					break;
				case 4:
					sln('  You need one more lesson to also raise your Death Knight Uses Per Day.');
					break;
				case 3:
					sln('  You need two more lesson to also raise your Death Knight Uses Per Day.');
					break;
				case 2:
					sln('  You need three more lessons to also raise your Death Knight Uses Per Day.');
					break;
				case 1:
					sln('  You need four more lessons to also raise your Death Knight Uses Per Day.');
					break;
			}
		}
		else {
			switch (player.skillw % 4) {
				case 0:
					lln('  `2You now have `0'+pretty_int(parseInt(player.skillw / 4, 10))+'`2 uses of Death Knight Skills a day.');
					player.levelw += 1;
					sln('');
					if (player.skillw === 40) {
						sln('  You have mastered The Death Knight Skills Completely.  You may choose to');
						sln('  learn a NEW skill now.');
						sln('');
						player.clss = choose_profession(false);
					}
					else {
						sln('  (four more lessons needed for next raise in uses per day)');
					}
					break;
				case 3:
					sln('  You need one more lesson to also raise your Death Knight Uses Per Day.');
					break;
				case 2:
					sln('  You need two more lessons to also raise your Death Knight Uses Per Day.');
					break;
				case 1:
					sln('  You need three more lessons to also raise your Death Knight Uses Per Day.');
					break;
			}
		}
	}
	else if (player.clss === 2) {
		player.skillm += 1;
		sln('');
		lln('  `2You now have `0'+pretty_int(player.skillm)+'`2 Mystical Skill points a day.');
		player.levelm += 1;
		switch (player.skillm % 4) {
			case 0:
				sln('');
				more();
				lln('`c  `%                    **  MYSTICAL INSTRUCTION **');
				sln('');
				foreground(2);
				sln('  An old man with a long white beard suddenly appears next to you.');
				sln('');
				lln('  `0"You\'re ready for your next lesson '+(player.sex === 'F' ? 'my girl!' : 'boy!'));
				sln('');
				more();
				if (player.skillm === 4) {
					lln('  `0"You ever been chased by a monster that just wouldn\'t quit?  I have,');
					sln('   Muh wife!  Heehee! In a case like that there is only one thing to do!');
					sln('   Disappear!"');
				}
				if (player.skillm === 8) {
					lln('  `0"Ok, you\'re still new at this, but I think you\'re ready for a little');
					sln('   trick I call The Heat Wave.  This little \'beaut will blow a wind that');
					sln('   not only warms your enemy, it cooks him."');
				}
				if (player.skillm === 12) {
					lln('  `0"Ok, pardner, lemmie give it to ya straight.  Sometimes yer enemy is');
					sln('   stronger then ya.  Ya need an advantage.  For instance, having a');
					sln('   protective Light Shield wrapped around ya.  Half damage!"');
				}
				if (player.skillm === 16) {
					lln('  `0"Ok, sometimes ya lose your temper, and you want to vent your anger');
					lln('   in a contructive way?  Am I right?  Causing someone\'s bones to shatter');
					lln('   is a great way to relieve stress."');
				}
				if (player.skillm === 20) {
					lln('  `0"This is also your LAST lesson!" `2the old man\'s face turns sober.  `0"You');
					sln('   have finally shown enough control to master the most complicated thing.');
					sln('   Your body.  Healing yourself with your mind is an awesome power."');
				}
				sln('');
				lln('  `2The old man vanishes as quickly as he appeared.');
				if (player.skillm === 40) {
					sln('');
					sln('  You have mastered the Mystical skills Completely.  You may choose to');
					sln('  learn a NEW skill now.');
					sln('');
					player.clss = choose_profession(false);
				}
				sln('  (four more lessons needed to learn a new Mystical Skill)');
				break;
			case 3:
				sln('  You need only one more lesson to learn a new Mystical Skill.');
				break;
			case 2:
				sln('  You need two more lessons to learn a new Mystical Skill.');
				break;
			case 1:
				sln('  You need three more lessons to learn a new Mystical Skill.');
				break;
		}
	}
	else if (player.clss === 3) {
		player.skillt += 1;
		if (settings.old_skill_points) {
			switch(player.skillt % 5) {
				case 0:
					lln('  `2You now have `0'+pretty_int(parseInt(player.skillt / 4, 10))+'`2 uses of The Thieving Skills a day.');
					player.levelt += 1;
					sln('');
					if (player.skillt === 40) {
						sln('');
						sln('  You have mastered The Thieving Knight Skills Completely.  You may choose to');
						sln('  learn a NEW skill now.');
						sln('');
						player.clss = choose_profession(false);
					}
					sln('  (five more lessons needed for next raise in uses per day)');
					break;
				case 4:
					sln('  You need only one more lesson to also raise your Thieving Uses Per Day.');
					break;
				case 3:
					sln('  You need two more lesson to also raise your Thieving Uses Per Day.');
					break;
				case 2:
					sln('  You need three more lessons to also raise your Thieving Uses Per Day.');
					break;
				case 1:
					sln('  You need four more lessons to also raise your Thieving Uses Per Day.');
					break;
			}
		}
		else {
			switch(player.skillt % 4) {
				case 0:
					lln('  `2You now have `0'+pretty_int(parseInt(player.skillt / 4, 10))+'`2 uses of The Thieving Skills a day.');
					player.levelt += 1;
					sln('');
					if (player.skillt === 40) {
						sln('');
						sln('  You have mastered The Thieving Knight Skills Completely.  You may choose to');
						sln('  learn a NEW skill now.');
						sln('');
						player.clss = choose_profession(false);
					}
					sln('  (four more lessons needed for next raise in uses per day)');
					break;
				case 3:
					sln('  You need only one more lesson to also raise your Thieving Uses Per Day.');
					break;
				case 2:
					sln('  You need two more lessons to also raise your Thieving Uses Per Day.');
					break;
				case 1:
					sln('  You need three more lessons to also raise your Thieving Uses Per Day.');
					break;
			}
		}
	}
	sln('');
}

function healers()
{
	var ch;

	function heal_all() {
		var afford;
		var need;
		var ret = false;

		sclrscr();
		sln('');
		sln('');
		lln('  `%Healers`#');
		sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(2);
		sln('');
		// DIFF: was just =
		if (player.hp >= player.hp_max) {
			lln('  `0"You look fine to us!"');
		}
		else {
			afford = parseInt(player.gold / 5 / player.level, 10);
			need = player.hp_max - player.hp;
			// DIFF: Was >, not >=
			if (player.gold >= (need * 5 * player.level)) {
				player.hp += need;
				lln(' `0 '+pretty_int(need)+'`2 hit points are healed and you feel much better.');
				player.gold -= (need * 5 * player.level);
				ret = true;
			}
			else if (afford < need) {
				player.hp += afford;
				sln('  '+pretty_int(afford)+' hit points are healed and you feel much better.');
				player.gold -= afford * 5 * player.level;
			}
		}
		sln('');
		more();
		return ret;
	}

	function heal_some() {
		var amt;

		sclrscr();
		sln('');
		sln('');
		lln('  `%Healers`#');
		lln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		sln('');
		lw('  `2HitPoints: (`0'+pretty_int(player.hp)+' `2of `0'+pretty_int(player.hp_max)+'`2)');
		lln('  `2Gold: `0'+pretty_int(player.gold));
		lln('  `2(it costs `%'+pretty_int(5 * player.level)+' `2to heal 1 hitpoint)');
		sln('');
		sln('  "How many hit points would you like healed?"');
		lw('  `0AMOUNT : `%');
		amt = parseInt(dk.console.getstr({len:5, integer:true}), 10);
		if (isNaN(amt)) {
			amt = 0;
		}
		sln('');
		if (amt === 0) {
			sln('  "Maybe some other time.."');
		}
		else if (amt < 0) {
			sln('  "Uh...Wouldn\'t that be hurting yourself?!"'); }
		else {
			if (amt * (5 * player.level) > player.gold) {
				sln('  "I\'m afraid you don\'t have enough gold to cover that."');
			}
			else if (amt > (player.hp_max - player.hp)) {
				sln('  "It would be deadly to over heal yourself!!"'); }
			else {
				sln('  Done!');
				player.gold = (player.gold - (amt * 5 * player.level));
				player.hp += amt;
			}
		}
	}

	if (player.hp < 0) {
		player.hp = 1;

	}

	function menu() {
		sclrscr();
		lrdfile('HEAL');
		// DIFF: Was just ===
		if (player.hp >= player.hp_max) {
			sln('');
			sln('');
			foreground(15);
			lln('  `%Healers`#');
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			lln('  `4`0"You look fine to us!"`2 the healers tell you.');
			sln('');
			more();
			return true;
		}
		return false;
	}

	if (menu()) {
		return;
	}

	do {
		sln('');
		lln('  `3`2HitPoints: (`0'+pretty_int(player.hp)+' `2of`0 '+pretty_int(player.hp_max)+'`2)');
		lln('  `2Gold: `0'+pretty_int(player.gold));
		lln('  `2(it costs `%'+pretty_int(5 * player.level)+'`2 to heal 1 hitpoint)');
		sln('');
		lln('  `5The Healers`2   (H,C,R)  (`0? for menu`2)');
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch(ch) {
			case '1':
				sln('');
				sln('  I wonder what\'s cooking..');
				sln('');
				break;
			case '2':
				sln('');
				sln('  Eye am not sure what you\'re');
				sln('  looking at.');
				sln('');
				break;
			case '3':
				sln('');
				sln('  Used to be a patient..  Hmmm.');
				sln('');
				break;
			case 'C':
				heal_some();
				break;
			case '?':
				if (menu()) {
					return;
				}
				break;
			case 'H':
				if (heal_all()) {
					return;
				}
				break;
			// DIFF: space used to continue without show HitPoints etc...
		}
	} while ('RQ\r'.indexOf(ch) === -1);
}

// TODO: Option to disable?
function blackjack()
{
	var startgold = player.gold;
	var ch;
	var suits = ['\x06', '\x03', '\x05', '\x04'];
	var scol = ['`r2', '`r2`4', '`r2', '`r2`4'];
	var faces = ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K'];
	var values = [11, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10];

	function play_hand() {	//sam2
		var cards;
		var hands = {};
		var i;
		var bet;
		var res;
		var tmp;
		var mc = morechk;
		var shuffle_strs = ['/\\', '==', '\xcd\xcd'];

		function hand_total(hand, min) {
			var hti;
			var aces11 = 0;
			var ret = 0;

			if (min === undefined) {
				min = false;

			}
			for (hti = 0; hti < hand.length; hti += 1) {
				if (hand[hti].val === 0) {
					aces11 += 1;
				}
				ret += values[hand[hti].val];
				if (ret > 21) {
					if (aces11 > 0) {
						ret -= 10;
						aces11 -= 1;
					}
				}
			}
			if (min) {
				while (aces11 > 0) {
					ret -= 10;
					aces11 -= 1;
				}
			}
			return ret;
		}

		function shuffle() {
			var si;
			var j;
			var stmp;

			cards = [];
			for (j = 0; j < 4; j += 1) {
				for (si = 0; si < 13; si += 1) {
					cards.push({suit:j, val:si});
				}
			}
			for (si = cards.length; si > 1; si -= 1) {
				j = random(si);
				stmp = cards[si - 1];
				cards[si - 1] = cards[j];
				cards[j] = stmp;
			}
		}

		function show_total(play, total, split) {
			if (split === 0 && hands.player.length > 1) {
				split = 1;
			}
			if (play) {
				if (split === 1) {
					dk.console.gotoxy(0, 22);
					lw('`r0`2Hand1: `0'+total);
				}
				else if (split === 2) {
					dk.console.gotoxy(13, 22);
					lw('`r0`2Hand2: `0'+total);
				}
				else {
					dk.console.gotoxy(0, 22);
					lw('`r0`2You: `0'+total);
				}
			}
			else {
				dk.console.gotoxy(68, 22);
				lw('`r0`2Dealer: `0'+total);
			}
		}

		function wizclr_scr(num) {
			var y;

			switch(num) {
				case 1:
					background(0);
					for (y = 11; y <= 19; y += 1) {
						dk.console.gotoxy(1,y);
						sw('            ');
					}
					break;
				case 2:
					background(0);
					for (y = 11; y <= 19; y += 1) {
						dk.console.gotoxy(67,y);
						sw('            ');
					}
					break;
				case 3:
					background(2);
					for (y = 1; y <= 7; y += 1) {
						dk.console.gotoxy(24, y);
						sw('                              ');
					}
					break;
				case 4:
					background(2);
					for (y = 10; y <= 16; y += 1) {
						dk.console.gotoxy(16, y);
						sw('                       ');
					}
					break;
				case 5:
					background(2);
					for (y = 10; y <= 16; y += 1) {
						dk.console.gotoxy(40, y);
						sw('                       ');
					}
					break;
				case 6:
					background(1);
					dk.console.gotoxy(37, 18);
					sw('        ');
					break;
			}
		}

		function wizmore() {
			var wi;

			dk.console.gotoxy(4, 19);
			lw('`2<`0MORE`2>');
			getkey();
			for (wi = 0; wi < 6; wi += 1) {
				sw('\b');
			}
			for (wi = 0; wi < 6; wi += 1) {
				sw(' ');
			}
		}

		function get_cpic(card) {
			return scol[card.suit]+faces[card.val]+suits[card.suit];
		}

		function deal(who, count, show, split) {
			if (split === undefined) {
				split = false;
			}
			var cpic;
			var card;
			var di;
			var x;
			var y;
			var hand = hands[who === '1' ? 'dealer' : 'player'][split ? 1 : 0];
			var full;
			var first = false;
			var dtmp;

			for (di = 0; di < count; di += 1) {
				card = cards.shift();
				cpic = get_cpic(card);
				mswait(500);
				hand.push(card);
				if (who === 1 && hand.length === 2) {
					first = true;
				}
				full = !!((hand.length % 2) === 1);
				dtmp = hand.length;
				if (split) {
					dtmp = 14 - hand.length;
					if (hand.length % 2 === 1) {
						dtmp -= 2;
					}
				}
				x = 17 + parseInt((dtmp - 1) / 2, 10) * 8;
				if (who === 1) {
					x += 8;
				}
				y = (who === 1 ? 1 : 10) + ((dtmp % 2) * 2);
				background(2);
				foreground(0);
				dk.console.gotoxy(x-1, y);
				sw('\xd5\xcd\xcd\xcd\xcd\xcd\xb8');
				dk.console.gotoxy(x-1, y+1);
				if (first) {
					sw('\xb3\xb1\xb1\xb1\xb1\xb1');
					foreground(0);
					dk.console.gotoxy(x + 5, y+1);
					sw('\xb3');
				}
				else {
					lw('\xb3'+cpic);
					foreground(0);
					dk.console.gotoxy(x + 5, y+1);
					sw('\xb3');
				}
				if (full) {
					dk.console.gotoxy(x-1, y+2);
					sw('\xb3     \xb3');
					dk.console.gotoxy(x-1, y+3);
					sw('\xb3     \xb3');
					dk.console.gotoxy(x-1, y+4);
					sw('\xc0\xc4\xc4\xc4\xc4\xc4\xd9');
				}
			}
			if (show) {
				show_total(who === 2, hand_total(hand, false), split ? 2 : 0);
			}
			else {
				show_total(who === 2, '?', 0);
			}
			hands[who === '1' ? 'dealer' : 'player'][split ? 1 : 0] = hand;
		}

		function card_check() {
			var cch;
			var candouble = true;
			var cctmp;
			var ptotal;
			var dtotal;
			var dealcards = true;
			var hand = 0;

			if (hands.player[0][0].val === hands.player[0][1].val) {
				wizclr_scr(1);
				foreground(10);
				dk.console.gotoxy(1,11);
				sw('Would you');
				dk.console.gotoxy(1,12);
				sw('like to');
				dk.console.gotoxy(1,13);
				sw('split?');
				dk.console.gotoxy(67,11);
				lw('`2(`0Y`2)es (`0N`2)o. ');
				cch = getkey().toUpperCase();
				if (cch !== 'Y') {
					cch = 'N';
				}
				if (cch === 'Y') {
					if (player.gold < bet) {
						wizclr_scr(1);
						foreground(10);
						dk.console.gotoxy(1,11);
						sw('Hey! you');
						dk.console.gotoxy(1,12);
						sw('don\'t have');
						dk.console.gotoxy(1,13);
						sw('enough gold!');
						wizmore();
						candouble = false;
					}
					else {
						hands.player[1] = [hands.player[0].pop()];
						candouble = false;
						player.gold -= bet;
						dk.console.gotoxy(0,21);
						lw('`r0`2Gold on hand: `$'+pretty_int(player.gold));
						dk.console.gotoxy(37, 18);
						lw('`r2`4'+(bet * 2));
						dk.console.gotoxy(16, 10);
						lw('`r2`2       ');
						dk.console.gotoxy(16,11);
						sw('       ');
						foreground(0);
						dk.console.gotoxy(56, 12);
						sw('\xd5\xcd\xcd\xcd\xcd\xcd\xb8');
						dk.console.gotoxy(56, 13);
						lw('\xb3'+get_cpic(hands.player[1][0]));
						foreground(0);
						dk.console.gotoxy(62, 14);
						sw('\xb3');
						dk.console.gotoxy(56, 14);
						sw('\xb3     \xb3');
						dk.console.gotoxy(56, 15);
						sw('\xb3     \xb3');
						dk.console.gotoxy(56, 16);
						sw('\xc0\xc4\xc4\xc4\xc4\xc4\xd9');
						dk.console.gotoxy(0, 22);
						lw('`r0`2Hand1:       Hand2:  ');
						deal(2, 1, true);
						deal(2, 1, true, true);
					}
				}
			}
			if (hand_total(hands.player[0]) === 21 && hands.player[0].length === 2) {
				candouble = false;
			}
			if (candouble) {
				cctmp = hand_total(hands.player[0], true);
				if (cctmp > 8 && cctmp < 12) {
					while(true) {
						wizclr_scr(1);
						foreground(10);
						dk.console.gotoxy(1,11);
						sw('Would you');
						dk.console.gotoxy(1,12);
						sw('like to');
						dk.console.gotoxy(1,13);
						sw('double down?');
						dk.console.gotoxy(67,11);
						lw('`2(`0Y`2)es (`0N`2)o. ');
						cch = getkey().toUpperCase();
						if (cch === 'N') {
							break;
						}
						if (cch === 'Y') {
							if (player.gold < bet) {
								wizclr_scr(1);
								foreground(10);
								dk.console.gotoxy(1,11);
								sw('Hey! you');
								dk.console.gotoxy(1,12);
								sw('don\'t have');
								dk.console.gotoxy(1,13);
								sw('enough gold!');
								wizmore();
								break;
							}
							player.gold -= bet;
							bet *= 2;
							dk.console.gotoxy(0,21);
							lw('`r0`2Gold on hand:                  ');
							dk.console.gotoxy(0,21);
							lw('`r0`2Gold on hand: `$'+pretty_int(player.gold));
							dk.console.gotoxy(37, 19);
							lw('`r2`4'+bet);
							deal(2, 1, true);
							ptotal = hand_total(hands.player[0]);
							if (ptotal > 21) {
								return 'BUST';
							}
							dealcards = false;
							break;
						}
					}
				}
			}
			while(dealcards) {
				ptotal = hand_total(hands.player[hand]);
				if (hands.player.length > 1) {
					if (ptotal > 21) {
						wizclr_scr(1);
						dk.console.gotoxy(1, 11);
						lw('`0Sorry!');
						dk.console.gotoxy(1, 12);
						sw('That hand');
						dk.console.gotoxy(1, 13);
						sw('busted.');
						wizmore();
						if (hand === 1) {
							break;
						}
						hand += 1;
						ptotal = hand_total(hands.player[hand]);
					}
				}
				else {
					if (ptotal > 21) {
						return 'BUST';
					}
					if (hands.player[0].length === 2 && ptotal === 21) {
						return 'BLACKJACK';
					}
				}
				// start3
				wizclr_scr(1);
				wizclr_scr(2);
				foreground(10);
				dk.console.gotoxy(1,11);
				sw('Would you');
				dk.console.gotoxy(1,12);
				sw('like to hit');
				dk.console.gotoxy(1,13);
				sw('or stay?');
				if (hands.player.length > 1) {
					dk.console.gotoxy(1, 15);
					sw('Hand'+(hand+1));
				}
				dk.console.gotoxy(67, 11);
				lw('`2(`0H`2)it');
				dk.console.gotoxy(67, 12);
				lw('`2(`0S`2)tay');
				cch = getkey().toUpperCase();
				if (cch === 'S') {
					if (hands.player.length === 1) {
						break;
					}
					if (hand === 1) {
						break;
					}
					hand += 1;
				}
				else if (cch === 'H') {
					deal(2, 1, true, !(hand === 0));
				}
			}
			lw('`r2');
			foreground(0);
			dk.console.gotoxy(24, 1);
			sw('\xd5\xcd\xcd\xcd\xcd\xcd\xb8');
			dk.console.gotoxy(24, 2);
			sw('\xb3\xb1\xb1\xb1\xb1 \xb3');
			mswait(500);
			dk.console.gotoxy(24, 2);
			sw('\xb3\xb1\xb1\xb1  \xb3');
			mswait(500);
			dk.console.gotoxy(24, 2);
			sw('\xb3\xb1\xb1   \xb3');
			mswait(500);
			dk.console.gotoxy(24, 2);
			sw('\xb3     ');
			dk.console.gotoxy(24, 2);
			lw('\xb3'+get_cpic(hands.dealer[0][1]));
			foreground(0);
			dk.console.gotoxy(30, 2);
			sw('\xb3');
			show_total(false, hand_total(hands.dealer[0]), 0);
			while(true) {
				dtotal = hand_total(hands.dealer[0]);
				if (dtotal > 21) {
					if (hands.player.length > 1) {
						return 'WW';
					}
					return 'DBUST';
				}
				if (dtotal === 21 && hands.dealer[0].length === 2) {
					return 'DBLACKJACK';
				}
				if (dtotal < 17) {
					deal(1, 1, true);
				}
				else {
					break;
				}
			}
			if (hands.player.length === 1) {
				if (dtotal === ptotal) {
					return 'PUSH';
				}
				if (dtotal > ptotal) {
					return 'DWIN';
				}
				return 'WIN';
			}
			cctmp = '';
			ptotal = hand_total(hands.player[0]);
			if (dtotal === ptotal) {
				cctmp += 'P';
			}
			else if (dtotal > ptotal || ptotal > 21) {
				cctmp += 'D'; }
			else {
				cctmp += 'W';
			}
			ptotal = hand_total(hands.player[1]);
			if (dtotal === ptotal) {
				cctmp += 'P';
			}
			else if (dtotal > ptotal) {
				cctmp += 'D'; }
			else {
				cctmp += 'W';
			}
			return cctmp;
		}

		morechk = false;
hand:
		while(true) {
			shuffle();
			dk.console.gotoxy(1, 11);
			lw('`r0`%Shuffling.');
			for (i=0; i<9; i += 1) {
				mswait(100);
				dk.console.gotoxy(1, 12);
				sw(shuffle_strs[i%shuffle_strs.length]);
			}
			hands.dealer = [[]];
			hands.player = [[]];
			// DIFF: startgold was reset to player.gold here.
			// startgold = player.gold;
			dk.console.gotoxy(0, 21);
			lw('`r0`2Gold on hand: `$'+pretty_int(player.gold));
			dk.console.gotoxy(0, 22);
			lw('`r0                                                                              ');
			show_total(true, 0, 0);
			show_total(false, 0, 0);
			while(true) {
				wizclr_scr(1);
				foreground(10);
				dk.console.gotoxy(1, 11);
				sw('How much');
				dk.console.gotoxy(1, 12);
				sw('ya gonna');
				dk.console.gotoxy(1, 13);
				sw('wager?');
				foreground(4);
				background(2);
				dk.console.gotoxy(37, 19);
				bet = parseInt(getstr(38, 19, 8, 1, 15, '200', {integer:true, input_box:true, select:true}).trim(), 10);
				if (isNaN(bet) || bet === 0) {
					wizclr_scr(1);
					foreground(10);
					dk.console.gotoxy(1, 11);
					sw('Fine, maybe');
					dk.console.gotoxy(1, 12);
					sw('later.');
					wizmore();
					break hand;
				}
				if (bet < 200) {
					dk.console.gotoxy(1, 14);
					lw('`0`r0Too little!');
					wizmore();
					wizclr_scr(1);
				}
				else if (bet > player.level * 1000) {
					dk.console.gotoxy(1, 14);
					lw('`0`r0Too much!');
					wizmore();
					wizclr_scr(1);
				}
				else if (player.gold < bet) {
					wizclr_scr(1);
					foreground(10);
					dk.console.gotoxy(1, 11);
					sw('Hey! you');
					dk.console.gotoxy(1, 12);
					sw('don\'t have');
					dk.console.gotoxy(1, 13);
					sw('enough gold!');
					wizmore();
					break hand;
				}
				else {
					break;
				}
			}
			player.gold -= bet;
			dk.console.gotoxy(0, 21);
			lw('`r0`2Gold on hand: `$'+pretty_int(player.gold)+'            ');
			deal(1, 1, false);
			deal(2, 1, true);
			deal(1, 1, false);
			deal(2, 1, true);
			res = card_check();
			switch (res) {
				case 'BUST':
					foreground(10);
					wizclr_scr(1);
					dk.console.gotoxy(1,11);
					switch (random(3)) {
						case 0:
							sw('You busted!');
							dk.console.gotoxy(1,12);
							sw('Better luck');
							dk.console.gotoxy(1,13);
							sw('next time.');
							break;
						case 1:
							sw('Oh too bad!');
							dk.console.gotoxy(1,12);
							sw('Maybe next');
							dk.console.gotoxy(1,13);
							sw('hand? (haw!)');
							break;
						case 2:
							sw('You busted!');
							dk.console.gotoxy(1,12);
							sw('Have you');
							dk.console.gotoxy(1,13);
							sw('played this');
							dk.console.gotoxy(1,14);
							sw('game before?');
							break;
					}
					wizmore();
					break;
				case 'BLACKJACK':
					foreground(10);
					bet *= 3;
					player.gold += bet;
					if (player.gold > 2000000000) {
						player.gold = 2000000000;
					}
					wizclr_scr(1);
					dk.console.gotoxy(1,11);
					sw('You got a');
					dk.console.gotoxy(1,12);
					sw('Blackjack!?');
					dk.console.gotoxy(1,13);
					sw('Are you');
					dk.console.gotoxy(1,14);
					sw('cheating?!');
					dk.console.gotoxy(1,16);
					sw('You win');
					dk.console.gotoxy(1,17);
					lw('`$'+pretty_int(bet)+'`0');
					dk.console.gotoxy(1,18);
					sw('Gold.');
					wizmore();
					break;
				case 'DBUST':
					foreground(10);
					bet *= 2;
					player.gold += bet;
					if (player.gold > 2000000000) {
						player.gold = 2000000000;
					}
					wizclr_scr(1);
					dk.console.gotoxy(1,11);
					switch(random(3)) {
						case 0:
							sw('I busted!');
							dk.console.gotoxy(1,12);
							sw('See? You');
							dk.console.gotoxy(1,13);
							sw('win ');
							dk.console.gotoxy(1,15);
							lw('`$'+pretty_int(bet)+'`0');
							dk.console.gotoxy(1,16);
							sw('Gold.');
							break;
						case 1:
							sw('Looks like');
							dk.console.gotoxy(1,12);
							sw('I busted.');
							dk.console.gotoxy(1,14);
							sw('You win');
							dk.console.gotoxy(1,15);
							lw('`$'+pretty_int(bet)+'`0');
							dk.console.gotoxy(1,16);
							sw('Gold... Arg.');
							break;
						case 2:
							sw('I busted!');
							dk.console.gotoxy(1,12);
							sw('Damnit!');
							dk.console.gotoxy(1,14);
							sw('You win');
							dk.console.gotoxy(1,15);
							lw('`$'+pretty_int(bet)+'`0');
							dk.console.gotoxy(1,16);
							sw('Gold.');
							break;
					}
					wizmore();
					break;
				case 'DBLACKJACK':
					foreground(10);
					wizclr_scr(1);
					dk.console.gotoxy(1, 11);
					switch(random(3)) {
						case 0:
							sw('Dealer gets');
							dk.console.gotoxy(1,12);
							sw('Blackjack!');
							dk.console.gotoxy(1,13);
							sw('Aw, too bad');
							dk.console.gotoxy(1,14);
							sw('for the big');
							dk.console.gotoxy(1,15);
							sw('human.');
							break;
						case 1:
							sw('Blackjack!');
							dk.console.gotoxy(1,12);
							sw('I\'m hot');
							dk.console.gotoxy(1,13);
							sw('today!');
							break;
						case 2:
							sw('Dealer gets');
							dk.console.gotoxy(1,12);
							sw('Blackjack!');
							dk.console.gotoxy(1,13); 
							sw('Not your');
							dk.console.gotoxy(1,14);
							sw('day is it?');
							break;
					}
					wizmore();
					break;
				case 'PUSH':
					foreground(10);
					wizclr_scr(1);
					dk.console.gotoxy(1,11);
					sw('It\'s a push!');
					dk.console.gotoxy(1,12);
					sw('I guess it\'s');
					dk.console.gotoxy(1,13);
					sw('better than');
					dk.console.gotoxy(1,14);
					sw('losing.');
					player.gold += bet;
					if (player.gold > 2000000000) {
						player.gold = 2000000000;
					}
					wizmore();
					break;
				case 'DWIN':
					foreground(10);
					wizclr_scr(1);
					dk.console.gotoxy(1,11);
					sw('Looks like');
					dk.console.gotoxy(1,12);
					sw('you lose.');
					dk.console.gotoxy(1,13);
					sw('Oh well!');
					dk.console.gotoxy(1,14);
					sw('better luck');
					dk.console.gotoxy(1,15);
					sw('next time.');
					wizmore();
					break;
				case 'WIN':
					foreground(10);
					wizclr_scr(1);
					dk.console.gotoxy(1,11);
					bet *= 2;
					player.gold += bet;
					if (player.gold > 2000000000) {
						player.gold = 2000000000;
					}
					switch(random(3)) {
						case 0:
							sw('That beats');
							dk.console.gotoxy(1,12);
							sw('my hand!');
							dk.console.gotoxy(1,14);
							sw('You win');
							dk.console.gotoxy(1,15);
							lw('`$'+pretty_int(bet)+'`0');
							dk.console.gotoxy(1,16);
							sw('Gold, Loser.');
							break;
						case 1:
							sw('You win!');
							dk.console.gotoxy(1,12);
							sw('Are you');
							dk.console.gotoxy(1,13);
							sw('counting?');
							dk.console.gotoxy(1,15);
							sw('You win');
							dk.console.gotoxy(1,16);
							lw('`$'+pretty_int(bet)+'`0');
							dk.console.gotoxy(1,17);
							sw('Gold.');
							break;
						case 2:
							sw('You win!');
							dk.console.gotoxy(1,12);
							sw('Not too bad');
							dk.console.gotoxy(1,14);
							sw('for a kid...');
							dk.console.gotoxy(1,15);
							sw('You win');
							dk.console.gotoxy(1,16);
							lw('`$'+pretty_int(bet)+'`0');
							dk.console.gotoxy(1,17);
							sw('Gold.');
							break;
					}
					wizmore();
					break;
				case 'PP':
				case 'PD':
				case 'PW':
				case 'DP':
				case 'DD':
				case 'DW':
				case 'WP':
				case 'WD':
				case 'WW':
					tmp = 0;
					wizclr_scr(1);
					dk.console.gotoxy(1, 11);
					lln('`0I have '+hand_total(hands.dealer[0])+'.');
					mswait(1500);
					dk.console.gotoxy(1, 12);
					switch (res[0]) {
						case 'W':
							sw('Hand1 Wins!');
							tmp += bet * 2;
							break;
						case 'P':
							sw('Hand1 Push!');
							tmp += bet;
							break;
						case 'D':
							sw('Hand1 Loses!');
							break;
					}
					dk.console.gotoxy(1, 14);
					switch (res[1]) {
						case 'W':
							sw('Hand2 Wins!');
							tmp += bet * 2;
							break;
						case 'P':
							sw('Hand2 Push!');
							tmp += bet;
							break;
						case 'D':
							sw('Hand2 Loses!');
							break;
					}
					dk.console.gotoxy(1, 15);
					sw('You win');
					dk.console.gotoxy(1, 16);
					lw('`$'+tmp+'`0');
					dk.console.gotoxy(1, 17);
					sw('Gold.');
					player.gold += tmp;
					if (player.gold > 2000000000) {
						player.gold = 2000000000;
					}
					wizmore();
					break;
			}
			wizclr_scr(1);
			wizclr_scr(2);
			dk.console.gotoxy(1, 11);
			foreground(10);
			sw('Play again?');
			dk.console.gotoxy(67, 11);
			lw('`2(`0Y`2)es (`0N`2)o. ');
			ch = getkey().toUpperCase();
			if (ch !== 'Y') {
				ch = 'N';
			}
			if (ch === 'N') {
				break;
			}
			wizclr_scr(1);
			wizclr_scr(2);
			wizclr_scr(3);
			wizclr_scr(4);
			wizclr_scr(5);
			wizclr_scr(6);
		}
		foreground(10);
		if (player.gold > startgold) {
			wizclr_scr(1);
			dk.console.gotoxy(1,11);
			sw('Quitting');
			dk.console.gotoxy(1,12);
			sw('while your');
			dk.console.gotoxy(1,13);
			sw('ahead?');
			dk.console.gotoxy(1,14);
			sw('Smart move.');
		}
		else if (player.gold < startgold) {
			wizclr_scr(1);
			dk.console.gotoxy(1,11);
			sw('Come back');
			dk.console.gotoxy(1,12);
			sw('soon. We');
			dk.console.gotoxy(1,13);
			sw('enjoyed');
			dk.console.gotoxy(1,14);
			sw('your money!');
			dk.console.gotoxy(1,15);
			sw('::snicker::');
		}
		else {
			wizclr_scr(1);
			dk.console.gotoxy(1,11);
			sw('It\'s been');
			dk.console.gotoxy(1,12);
			sw('nice doing');
			dk.console.gotoxy(1,13);
			sw('business');
			dk.console.gotoxy(1,14);
			sw('with you...');
		}
		wizmore();
		dk.console.gotoxy(0,23);
		morechk = mc;
		curlinenum = 1;
	}

	while(true) {
		lln('`c  `%A meeting of chance.`2');
		sln('');
		sln('  A sly looking dwarf hops out of the brush.');
		sln('');
		lln('  `2"`0How about a game, friend?`2"');
		sln('');
		lln('  `2(`0G`2)ive the game a chance');
		lln('  (`0T`2)ell the Dwarf to screw off');
		do {
			command_prompt();
			ch = getkey().toUpperCase();
		} while('GT?'.indexOf(ch) === -1);
		sln('');
		foreground(0);
		switch(ch) {
			case '?':
				break;
			case 'T':
				sln('');
				lln('  `2"`0Your loss, kid.  Forget you ever saw me!`2"');
				sln('');
				more();
				return;
			case 'G':
				sln('');
				lln('  `0"Excellent!" `2the dwarf exclaims as he pulls out a makeshift black jack');
				sln('  table!');
				sln('');
				lln('  `2"`0As for the rules... I have to stay on a 17.  You can double down on a');
				lln('  9, 10 or 11, and I do allow splitting pairs multiple times.`2"');
				sln('');
				sln('  You rub your chin - with any luck you\'ll double your money...');
				sln('');
				more();
				sclrscr();
				lrdfile('BJTABLE');
				play_hand();
				return;
		}
	}
}

function fight_dragon(cant_run) {
	var dragon;

	function story() {
		if (player.clss === 1) {
			lln('`c                   `%EPILOGUE `2- `0The Warrior\'s Ending');
			lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-');
			lln('  `2After your bloody duel with the huge Dragon, your first inpulse is to rip');
			lln('  its head off and bring it town.  Carefull thought reveals it is much to');
			lln('  big for your horse, so that plan is moot.  Your second notion is bring back');
			lln('  the childrens bones.  Bags and bags of them for proper burial, but you');
			lln('  realize this would only cause the town\'s inhabitants `0MORE`2 pain.  You');
			lln('  finally decide on the Dragon\'s heart.  After adding ten years to your');
			lln('  sword\'s life, you finally chip off enough scales to wallow in the huge');
			lln('  beast\'s insides.');
			sln('');
			lln('  When you are finished, and fit the still heart in a gunny sack you brought,');
			lln('  (who would have thought this would be its use?) you make your way back to');
			lln('  town.  As you share your story to a crowd of excited onlookers, this crowd');
			lln('  becomes a gathering, and this gathering becomes an assemblage, and this');
			lln('  assemblage becomes a multitude!');
			sln('');
			lln('  This multitude nearly becomes a mob, but thinking quick, you make a');
			lln('  speech.');
			sln('');
			more();
			lln('  `0"PEOPLE!" `2your voice booms.  `0"It is true I have ridden this town of');
			lln('  its curse, the `4Red Dragon`0.  And this is his heart."');
			sln('');
			lln('  `2You dump the bloody object onto the ground.  From the back, Barak\'s');
			lln('  voice is heard.  `5"How do we know where you got that thing?  It looks');
			lln('  like you skinned a sheep!"  `2A flicker of annoyance crosses your face,');
			lln('  but you force a smile.  `0"Why Barak, would you doubt me?  A LEVEL 12');
			lln('  warrior?  If I am not mistaken, you are quite a bit lower, still at level');
			lln('  two, eh?"');
			sln('');
			lln('  `2Barak gives you no more trouble, and you are declared a hero by all.');
			if (player.sex === 'M') {
				lln('  `#Violet `2tops off the evening by giving you a kiss on the cheek, and a ');
			}
			else {
				lln('  `%Seth `2tops off the evening by giving you a kiss on the cheek, and a ');
			}
			lln('  whisper of things to come later that night makes even you almost');
			lln('  blush.  Almost.');
			sln('');
			more_nomail();
		}
		if (player.clss === 2) {
			lln('`c            `%EPILOGUE `2- `#The Mystical Skills User Ending');
			lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-');
			lln('`2  Still shaking from the battle, you decide it is time to return to town');
			lln('  and share the good tidings.  You close your eyes and concentrate.');
			lln('  `0"There is no place like Town"`2...A when you open your eyes, you are');
			lln('  under a cow in a farm outside - Not far from Abduls Armour.  You trek the');
			lln('  distance to the Armoury, cursing as you go.  Wizards were not made for this');
			lln('  kind of hardship you tell yourself.');
			sln('');
			lln('  Miss Abdul is estatic when you tell her about your escapades.  She agrees');
			lln('  to accompany you to the Inn.  She lends you a horse when you tell her of how');
			lln('  your feet ache.  ');
			sln('');
			lln('  `2When you enter the smokey bar with a loud clatter, merry makers stop their');
			lln('  carousing, people hunched over their meals stop chewing and Seth Able stops');
			lln('  playing in mid-strum.');
			sln('');
			more();
			lln('  `0"People!  I have slain the beast!"`2 you shout triumphantly. A voice is');
			lln('  heard from the back. `0"What beast?  A large rat or something?"`2  You');
			lln('  scowl.  It is Barak.  You nonchalantly make a gesture with one hand. A');
			lln('  few moments later, Barak stands up suprised.  He looks wildly around, then');
			lln('  makes a bolt for the door.');
			sln('');
			lln('  `0"What happened to him?" `2Miss Adbul asks you in puzzlement.');
			sln('');
			lln('  `0"Nature called." `2you smile.  Your face becomes serious as you address');
			lln('  the bar.  `0"The `4Red Dragon`0 is no more."`2 `#Violet `2sets her serving');
			lln('  tray down to hear better. `0"We cannot bring back those dead, but I have');
			lln('  stopped this from happening again." ');
			sln('');
			lln('  `0"And he did it with Armour from Abduls Armour!"`2 Miss Abdul adds.');
			sln('');
			lln('  `0"Er, thank you.  Anyway, make sure you put something in the Daily');
			lln('  happenings about this!"`2  The crowd gives you a standing ovation.');
			sln('');
			more();
		}
		if (player.clss === 3) {
			lln('`c                      `%EPILOGUE `2- `9The Thief\'s Ending');
			lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-');
			lln('`2  You breathe a sigh of relief, retrieve your daggers and carefully clean');
			lln('  them.  Although you realized some may think it cold hearted if they ever');
			lln('  found out, you pick through the childrens bones, picking up a gold piece');
			lln('  here, a silver there.  Afterall, these children didn\'t need it to buy a');
			lln('  meal anymore... They WERE a meal!  You smile at your own dry wit and realize');
			lln('  you had better get to town and share the news.');
			sln('');
			lln('  The hike to town is long, but you are used to it, and rather enjoy the');
			lln('  peace it brings.  You find the town deserted.  You enter the Inn, hoping');
			if (player.sex === 'M') {
				lln('  to find some clue, but all you find is `#Violet`2.');
				sln('');
				lln('  `0"Via, Where has everyone gone?  Have they finally given up hope of ever');
				lln('  stopping the `4Red Dragon`0, and have gone to seek a new lifestyle?');
				lln('  ');
				lln('`2  There is a pause, then she responds.');
				sln('');
				more();
				lln('  `0"No, they are having a feast at Turgons Place.  Naturally SOMEONE had');
				lln('  to stay here, and OF COURSE it would be me...I never get to have any fun!"');
				sln('');
				lln('  `2You give her a wink.  `0"That just isn\'t so...Remember last night?"');
				lln('  `2She giggles, and after a few more naughty sayings you have her cheeks as');
				lln('  deep a scarlet as a rose.');
			}
			else {
				lln('  to find some clue, but all you find is `%The Bard`2.');
				sln('');
				lln('  `0"Seth, Where has everyone gone?  Have they finally given up hope of ever');
				lln('  stopping the `4Red Dragon`0, and have gone to seek a new lifestyle?');
				lln('  ');
				lln('`2  There is a pause, then he responds.');
				sln('');
				more();
				lln('  `0"No, they are having a feast at Turgons Place.  I didn\'t feel like ');
				lln('  going to a party.  At least not by myself.  I\'m just not the party type."');
				sln('');
				lln('  `2You give him a wink.  `0"That just isn\'t so...Remember last night?"');
				lln('  `2He laughs, and after a few more naughty sayings you have him ready to');
				lln('  take a cold shower.  (But you do something else instead) ;> ');
			}
			sln('');
			lln('`2  When you finally meet up at Turgon\'s, you share your story.  You can\'t help');
			lln('  but be pleased at seeing so many faces in awe over your doings, so you');
			lln('  \'spice\' up the story in a few places... As you are finishing, a woman in the');
			lln('  back cries out.');
			sln('');
			lln('  `0"That Thief is wearing the ring I gave my Ellie the last birthday before');
			lln('  she disappeared!"`2');
			sln('');
			lln('  You decide now would be the perfect time to make your departure.');
			sln('');
			more();
			sln('');
			more();
		}
	}

	// TODO: This could be rolled into lenemy.bin easily enough...
	if (file_exists(game_or_exec('dragon.bin')) && !settings.no_lenemy_bin) {
		var dragonfile = new RecordFile(game_or_exec('dragon.bin'), Monster_Def);
		dragon = dragonfile.get(0);
		dragonfile.file.close();
		dragon.is_dragon = true;
	}
	else {
		dragon = {
			name:'`4The Red Dragon`2',
			str:2000,
			gold:1000,
			weapon:'`)Scorching Flame`2',
			exp:1000,
			hp:15000,
			death:'  The earth shakes as the mighty beast falls.',
			is_dragon:true
		};
	}
	player.seen_dragon = true;
	beef_up(dragon);
	sln('');
	sln('');
	lln('`%  **`4DRAGON ENCOUNTER`%**');
	sln('');
	lln('  `2The Red Dragon approaches.');
	sln('');
	battle(dragon, false, cant_run);
	foreground(2);
	if (player.dead) {
		player.hp = 0;
		player.dead = true;
		player.gold = 0;
		player.put();
		sln('');
		sclrscr();
		sln('');
		sln('');
		sln('  The Dragon pauses to look at you, then snorts in a Dragon laugh, and');
		sln('  delicately rips your head off, with the finesse only a Dragon well practiced');
		sln('  in the art could do.  ');
		sln('');
		log_line('  `2The `4Red Dragon `2has killed `5'+player.name+'`2!');
		more();
	}
	if (dragon.hp < 1) {
		sln('');
		sln('  You have defeated The Red Dragon!');
		sln('');
		more();
		sclrscr();
		sln('');
		sln('');
		sln('  You have defeated the Dragon, and saved the town.  Your stomach churns');
		sln('  at the site of stacks of clean white bones - Bones of small children.');
		sln('');
		sln('  THANKS TO YOU, THE HORROR HAS ENDED!');
		sln('');
		log_line('`.  `%'+player.name+' `2has slain the `4Red Dragon`2 and become a hero.');
		more();
		story();
		foreground(15);
		sln('             Thanks for being tough enough to win the game, ');
		//sln('                 and thank your sysop for registering!');
		sln('');
		foreground(10);
		sln('                              -Seth Able');
		sln('');
		more();
		player.level = 1;
		player.hp_max = 20;
		player.hp = player.hp_max;
		player.weapon_num = 1;
		player.weapon = 'Stick';
		player.gold = 500;
		player.bank = 0;
		player.def = 1;
		player.str = 10;
		player.gem = 10;
		player.arm = 'Coat';
		player.arm_num = 1;
		player.dead = false;
		player.inn = false;
		player.exp = 10;
		if (settings.forest_fights > 32000) {
			settings.forest_fights = 32000;
		}
		if (settings.forest_fights + player.kids > 32000) {
			player.forest_fights = 32000;
		}
		else {
			player.forest_fights = settings.forest_fights + player.kids;
		}
		player.pvp_fights = settings.pvp_fights_per_day;
		player.flirted = false;
		player.high_spirits = true;
		player.drag_kills += 1;
		player.put();
		if (psock !== undefined) {
			psock.writeln('NewHero '+player.name);
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after NewHero');
			}
		}
		get_state(true);
		state.latesthero = player.name;
		tournament_check();
		if (settings.win_deeds > 0 && player.drag_kills >= settings.win_deeds) {
			state.won_by = player.Record;
			put_state();
			lln('`c  `%** YOUR QUEST IS OVER **');
			sln('');
			lln('  `2You must indeed be the chosen one.  The ancient magic that');
			lln('  kept the `4dragon `2alive is now truly no more.');
			sln('');
			more_nomail();
			lln('  `0Now begone, blessed among warriors - Your fight is over.');
			player.on_now = false;
			player.put();
			exit(0);
		}
		put_state();
		lln('`c  `%                ** YOUR QUEST IS NOT OVER **');
		sln('');
		lln('  `2You are a hero.  Bards will sing of your deeds, but that doesn\'t');
		lln('  mean your life doesn\'t go on.  ');
		sln('');
		lln('  `%YOUR CHARACTER WILL NOW BE RESET.  `2But you will keep a few things');
		lln('  you have earned.  Like the following.');
		sln('');
		lln('  `%ALL SPECIAL SKILLS.');
		lln('  CHARM.');
		lln('  A FEW OTHER THINGS.');
		sln('');
		more();
		lln('`c  `%YOU FEEL STRANGE.');
		sln('');
		lln('  `2Apparently, you have been sleeping.  You dust yourself off, and');
		lln('  regain your bearings.  You feel like a new person!');
		sln('');
		more();
	}
}

function forest()
{
	var ch;
	var over = false;
	var enemy;

	function darkhorse_tavern() {
		var dch;
		var games_played = 0;

		function menu() {
			lrdfile('CLOAK');
		}

		function gamble() {
			var bet;
			var mynum;
			var hisnum;

			function wager() {
				var ret = -1;

				do {
					lln('  `2How much gold of your `%'+pretty_int(player.gold)+'`2 will you hazard? (`00`2 to chicken out)');
					lw('  `2WAGER : `0');
					ret = parseInt(dk.console.getstr({len:11, integer:true}), 10);
					if (isNaN(ret)) {
						ret = 0;
					}
					sln('');
					if (ret > player.gold) {
						lln('  `2Betting what you don\'t have is `4NOT`2 a good idea.');
						sln('');
						ret = -1;
					}
					else if (ret < 0) {
						lln('  `2You don\'t think that will go over too big.');
						sln('');
					}
				} while (ret < 0);
				sln('');
				return ret;
			}

			lln('`c`%  ** GAMBLE TIME! **');
			sln('');
			foreground(2);
			sln('  You saunter over to the bar and demand that someone gamble with you.');
			sln('');
			if (games_played > 1) {
 				sln('  No one seems too thrilled at the prospect.  Perhaps if you came back');
				sln('  another time.');
				sln('');
				if (games_played > 2) {
					sln('  (You wonder if it had anything to do with your making a joke out');
					sln('  of the word honor)');
					sln('');
				}
				return;
			}

			games_played += 1;
			switch (random(3)) {
				case 0:
					sln('  The old man stops etching on the table he is at and walks over to you.');
					sln('');
					lln('  `0"I\'ll play a game with ya, kid!  I\'ll bet I can guess what number');
					sln('  you are thinkin\'!"');
					sln('');
					more();
					bet = wager();
					if (bet === 0) {
						lln('  `2The old man laughs in your face then continues his carving in the');
						sln('  table.');
						break;
					}
					lln('  `0"Fine!  `%'+pretty_int(bet)+'`0 it is!  Concentrate on a number."`2');
					sln('');
					mynum = random(100) + 1;
					lln('  `2You concentrate on the number.');
					mswait(500);
					sw('.');
					mswait(500);
					sw('.');
					mswait(500);
					lln('`%'+mynum);
					sln('');
					lln('  `2The old man studies you quietly for a moment.  Then screams in delight.');
					sln('');
					more_nomail();
					// DIFF: This was clearly broken, and he always guess right.
					// it was hisnum !== mynum in the loop test.
					hisnum = random(100) + 1;
					if (hisnum > 55) {
						hisnum = random(100) + 1;
						while(hisnum === mynum) {
							hisnum = random(100) + 1;
						}
					}
					else {
						hisnum = mynum;
					}
					lw('  `0"The number is');
					mswait(500);
					sw('.');
					mswait(500);
					sw('.');
					mswait(500);
					lw('.`%');
					lln(hisnum+'`0 isn\'t it?!!!!!!!!!!!');
					sln('');
					if (hisnum !== mynum) {
						lln('  `2You sadly inform the Old Man of his mistake, and he grudgingly gives');
						lln('  you `%'+pretty_int(bet)+'`2 gold from his pouch.');
						player.gold += bet;
						if (player.gold > 2000000000) {
							player.gold = 2000000000;
						}
						break;
					}
					lln('  `2You feel obliged to admit that he chose correctly.  You count out');
					lln('  `%'+pretty_int(bet)+'`2 gold and give it to him with a scowl.');
					player.gold -= bet;
					sln('');
					lln('  `2The old man dances a jig of joy!');
					break;
				case 1:
					sln('  The old man stops etching on the table he is at and walks over to you.');
					sln('');
					lln('  `0"I\'ll play a game with ya, kid!"');
					sln('');
					lln('  `2The old man grabs two wooden mugs from a table and slaps them down');
					lln('  in front of you upside down.  `0"Guess which one I hid muh teeth in!"');
					sln('');
					more();
					bet = wager();
					if (bet === 0) {
						lln('  `2The old man laughs in your face then continues his carving in the');
						sln('  table.');
						break;
					}
					lln('  `0"Agreed!" `2The old man waits for your response.');
					sln('');
					more();
					switch (random(2)) {
						case 0:
							lln('  `2You demand to see what\'s in the first mug!');
							break;
						case 1:
							lln('  `2You demand to see what\'s in the second mug!');
							break;
					}
					sln('');
					hisnum = random(100) + 1;
					lw('  `2The old man slowly turns over the mug.');
					mswait(500);
					sw('.');
					mswait(500);
					sw('.');
					mswait(500);
					sw('.');
					if (hisnum > 45) {
						lln('`%IT HAS HIS WOODEN TEETH IN IT!');
						player.gold += bet;
						if (player.gold > 2000000000) {
							player.gold = 2000000000;
						}
						sln('');
						lln('  `2The entire bar cheers at your success!');
						sln('');
						sln('  The old man groans and hands you the gold you\'ve won.');
					}
					else {
						lln('`4IT IS EMPTY SAVE SOME STALE BEER!');
						player.gold -= bet;
						sln('');
						lln('  `2The old man howls in delight as you pay him.');
					}
					break;
				case 2:
					sln('  The old man stops etching on the table he is at and walks over to you.');
					sln('');
					lln('  `0"I\'ll play a game with ya, kid!"');
					sln('');
					lln('  `2The old man walks over to you and hands you a small dagger.  Then he');
					sln('  moves to the other side of the Tavern, and picks up a tankard of brew.');
					sln('');
					lln('  `0"I\'ll bet you can\'t knock this off my head without getting me wet!"');
					sln('');
					more();
					bet = wager();
					if (bet === 0) {
						lln('  `2The old man laughs in your face then continues his carving in the');
						sln('  table.');
						break;
					}
					lln('  `2The old man positions himself carefully, and places the Mug on his head.');
					sln('');
					lln('  `0"You\'ll never hit it, sharpshooter!  Throw it already!"');
					sln('');
					more();
					lln('  `2You give it your best shot.');
					sln('');
					lw('  `%** `0WHO');
					for (mynum = 0; mynum < 15; mynum += 1) {
						mswait(100);
						sw('O');
					}
					lln('SH `%**');
					sln('');
					if ((random(100) + 1) > 44) {
						lln('  `2YOU HAVE KNOCKED IT OFF LEAVING THE OLD MAN HIGH AND DRY!');
						sln('');
						lln('  The old man swears sourly, but pays you the `%'+pretty_int(bet)+'`2 gold.');
						sln('');
						player.gold += bet;
						if (player.gold > 2000000000) {
							player.gold = 2000000000;
						}
						break;
					}
					foreground(4);
					sln('  YOU MISS YOUR TARGET!');
					foreground(2);
					sln('');
					mswait(500);
					switch(random(6)) {
						case 0:
							lln('  `2The dagger ricochetted off the wall and into `%Chance`2\'s drink!');
							sln('');
							sln('  He looks furious!');
							break;
						case 1:
							sln('  The dagger smoothly implants itself into the old mans forehead!');
							sln('');
							sln('  But he is ok!');
							break;
						case 2:
							sln('  The dagger stabs deeply into the oak behind the old mans head!');
							break;
						case 3:
							sln('  The dagger stabs near your foot!  The entire tavern laughs at you!');
							break;
						case 4:
							sln('  The dagger hits the bar, and slides to a screeching stop in front of a');
							sln('  young warrior.  His face is white as a sheet - The entire bar laughs!');
							break;
						case 5:
							sln('  The dagger flies through an open window!');
							break;
					}
					sln('');
					sln('  The old man laughs with glee.  You mumble curses as you pay him.');
					player.gold -= bet;
					break;
			}
			sln('');
		}

		function chance() {
			var ich;
			var op;
			var pl;
			var him;
			var he;
			var prof;
			var class_strs = ['Nobody', '`0Warrior', '`#Mystical Skills User`0', '`9Thief`0'];

			function cmenu() {
				lrdfile('CHANCE');
			}

			cmenu();

			do {
				lw(' `2 Your command? (`0? `2for menu)  [`0R`2] : ');
				ich = getkey().toUpperCase();
				if (ich === '\r') {
					ich = 'R';
				}
				sln(ich);
				sln('');
				if (ich === 'L') {
					lln('  `0"I know many things about many people.  Who is your enemy?"`2');
					pl = find_player();
					if (pl === -1) {
						lln('');
						lln('  `0"I don\'t know anyone with a name even close to that."`2');
						sln('');
						cmenu();
					}
					else if (pl === player.Record) {
						if (player.sex === 'M') {
							him = 'him';
							he = 'he';
						}
						else {
							him = 'her';
							he = 'she';
						}
						lln('  `0"Yes..I know '+him+'.  '+he+' is a favorite customer of mine!"');
						lln('  `2Chance laughs heartily.');
						lln('');
					}
					else {
						op = player_get(pl);
						prof = class_strs[op.clss];
						lln('  `2Chance\'s face turns somber.');
						lln('  `0"'+op.name+'`0 the '+prof+'?  I know who that is."');
						sln('');
						lln('  `0"This information was not easily come by, and I am going to have to charge');
						lln('  two `%Gems`0 for it.  Now you know why this tavern is REALLY here."');
						lln('');
						if (player.gem < 2) {
							lln('`2  Not having two `%Gems`2, you decline.');
							sln('');
						}
						else {
							lw('  `2Pay Chance two `%Gems`2 for the info? [`0Pay \'Em`2] : ');
							ich = getkey().toUpperCase();
							// DIFF: Previously, 'N' was forced to 'Y', and anything except N or Y would be an effective "no"
							if (ich !== 'N') {
								ich = 'Y';
							}
							sln(ich);
							sln('');
							if (ich !== 'Y') {
								lln('  `0"No problem!  I know how it is these days."');
								sln('');
							}
							else {
								player.gem -= 2;
								lln('  `0"Alright.  Come with me."');
								sln('');
								lln('  `2Chance leads you to a small room in back of the tavern, and you take');
								sln('  a comfortable seat.');
								sln('');
								lln('  `0"Well...Here is everthing I know about '+op.name+'`0."');
								sln('');
								more();
								if (op.sex === 'M') {
									him = 'him';
									he = 'he';
								}
								else {
									him = 'her';
									he = 'she';
								}
								lln('  `0"Fights with a '+op.weapon+'`0 and has a total Strength of `%'+pretty_int(op.str)+'`0."');
								lln('  `0"Wears a '+op.arm+'`0 and has a total Defense of `%'+pretty_int(op.def)+'`0."');
								sln('');
								if (op.cha < 3) {
									lln('  `0"'+op.name+' is very ugly."');
								}
								else if (op.cha < 5) {
									lln('  `0"'+op.name+' is kind of blah looking."'); }
								else if (op.cha < 10) {
									lln('  `0"'+op.name+' is fairly good looking."'); }
								else if (op.cha < 50) {
									lln('  `0"'+op.name+' has a very fair countenance."'); }
								else if (op.cha < 90) {
									// DIFF: This used the *players* sex!
									if(op.sex === 'F') {
										lln('  `0"'+op.name+' is a very good looking woman."');
									}
									else {
										lln('  `0"'+op.name+' gets all the women...The lucky brute!"');
									}
								}
								else {
									// DIFF: This used the *players* sex!
									if (op.sex === 'F') {
										lln('  `0"I have heard '+op.name+' has the face and body of a Goddess."');
									}
									else {
										lln('  `0"'+op.name+' is a good looking bastard."');
									}
								}
								sln('');
								lln('  `0"Total worth in gold is '+pretty_int(op.gold +op.bank)+'."');
								sln('');
								lln('  `0"Last time we checked, '+he+' had '+pretty_int(op.gem)+' `%Gems`0."');
								sln('');
								if (op.kids === 0) {
									lln('  `0"'+op.name+' has no offspring."');
								}
								else {
									lln('  `0"'+op.name+' has `%'+pretty_int(op.kids)+' `2offspring.');
								}
								if (op.horse) {
									lln('  `0That person owns a horse.');
									sln('');
								}
								// DIFF: Didn't check Seth and Violet marriages...
								get_state(false);
								if (op.married_to > -1 || state.married_to_seth === op.Record || state.married_to_violet === op.Record) {
									lln('  `0That person is married.');
									sln('');
								}
								sln('');
								more_nomail();
							}
						}
					}
				}
				else if (ich === '?') {
					cmenu();
				}
				// DIFF: The 'S' key was not in here, but it was in the menu.
				else if (ich === 'C' || ich === 'S') {
					lln('  `0"Tired of what you do?  I know how it is.  To figure out what you');
					lln('  REALLY want to do in your life, think about your childhood."`2 ');
					sln('');
					lln('  `0"Remember.  You NEVER forget what you learn in ANY profession."`2');
					player.clss = choose_profession(true);
					sln('');
					cmenu();
				}
				else if (ich === 'T') {
					sln('');
					lln('  `0"C`3o`4l`5o`6r`7s`8?`0"`2, Chance laughs, `0"They are easy."');
					sln('');
					sln('  `1 `2 `3 `4 `5 `6 `7 `8 `9 `0 `! `@ `# `$ `%');
					lln('  `1^^ `2^^ `3^^ `4^^ `5^^ `6^^ `7^^ `8^^ `9^^ `0^^ `!^^ `@^^ `#^^ `$^^ `%^^');
					sln('');
					sln('  `5Colors are `%FUN`5! would look like...');
					sln('');
					lln('  `5Colors are `%FUN`5!');
					sln('');
					lln('  `0"Using that symbol and a number, you can make any color.  They don\'t');
					sln('  work in mail, but try \'em talking to people or write it in the dirt."');
					lln('`2');
				}
				else if (ich === 'P') {
					lln('  `0"Ok, enter your practice line."');
					lw('  `2Text `0: `%');
					pl = clean_str(dk.console.getstr({len:69}));
					sln('');
					lln('  `0"Here is what that would look like."');
					lln('`2  `%'+pl);
					sln('');
				}
				else {
					if (ich === 'R') {
						ch = 'Q';
					}
				}
			} while (ch !== 'Q');
		}

		function old_man() {
			var ich;
			var pl;
			var him;
			var op;
			var l;

			lrdfile('OLDMAN');
			do {
				sln('');
				lw(' `2 Your command? (`0? `2for menu)  [`0R`2] : ');
				ich = getkey().toUpperCase();
				if ('V?E'.indexOf(ich) === -1) {
					ich = 'R';
				}
				sln(ich);
				sln('');
				switch (ich) {
					case '?':
						lrdfile('OLDMAN');
						break;
					case 'V':
						lln('  `0"Who would you like to know more about?"`2');
						pl = find_player();
						if (pl === -1) {
							sln('');
							lln('  `0"I don\'t know anyone with a name even close to that."`2');
							sln('');
						}
						else if (pl === player.Record) {
							lln('  `0"Why, Id hope you know what you\'ve said about yourself.."`2,');
							sln('  the old man cackles.');
							lln('');
						}
						else {
							op = player_get(pl);
							him = (op.sex === 'M' ? 'him' : 'her');
							lln('  `2The Old Man thinks for a minute..');
							if (!op.has_des) {
								lln('  `0"'+op.name+'?  I haven\'t heard anything about '+him+'."');
							}
							else {
								lln('  `0"'+op.name+'?  I know who that is. Last I heard.."');
								sln('');
								lln('    `2'+op.des1);
								lln('    `2'+op.des2);
							}
						}
						break;
					case 'E':
						lln('  `0"What do you want me to remember about you?"');
						lw(' `2-> `%');
						// TODO: Is a different string input than everything else...
						l = clean_str(dk.console.getstr({len:70}));
						if (l !== '') {
							player.des1 = l;
							player.des = true;
							lw(' `2-> `%');
							l = clean_str(dk.console.getstr({len:70}));
							player.des2 = l;
							sln('');
							lln('  `2"`0It has been noted..`2"');
							player.put();
						}
						break;
				}
			} while (ich !== 'R');
		}

		function rank_lays(fname) {
			var i;
			var pl = [];
			var f = new File(fname);

			all_players().forEach(function(op) {
				if (op.name !== 'X' && op.laid > 0) {
					pl.push(op);
				}
			});
			pl.sort(function(a,b) {
				if (a.laid !== b.laid) {
					return b.laid - a.laid;
				}
				return b.cha - a.cha;
			});
			if (!f.open('w')) {
				throw('Unable to open '+f.name);
			}
			f.writeln('');
			f.writeln('');
			f.writeln('                          `%The Old Man\'s Ranking');
			f.writeln('');
			if (settings.clean_mode) {
				f.writeln('  `0Name                             Evil Deeds             Player Kills');
			}
			else {
				f.writeln('  `0Name                                Lays                Player Kills');
			}
			f.writeln('`#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			for (i = 0; i < pl.length; i += 1) {
				f.writeln('  `0'+space_pad(pl[i].name, 22)+'            `%'+format("%5d", pl[i].laid)+'                      `4 '+format("%6.6s",pretty_int(pl[i].pvp)));
			}
			if (pl.length === 0) {
				f.writeln('  `0Sad times indeed, no one has managed to make this list.');
			}
			f.close();
		}

		menu();
		do {
			sln('');
			lln('  `0DarkCloak Tavern `2(C,E,T,V,D,G,W,R) (? for menu)');
			sln('');
			lw('  `2Your command? : ');
			dch = getkey().toUpperCase();
			if ('\rQ'.indexOf(dch) !== -1) {
				dch = 'R';
			}
			sln(dch);
			switch(dch) {
				case 'D':
					show_log();
					break;
				case 'Y':
					show_stats();
					break;
				case 'G':
					gamble();
					break;
				case 'T':
					chance();
					break;
				case 'W':
					old_man();
					break;
				case 'C':
					converse(true);
					break;
				case '?':
					menu();
					break;
				case 'E':
					rank_lays(gamedir('temp'+player.Record));
					display_file(gamedir('temp'+player.Record), true, true);
					sln('');
					file_remove(gamedir('temp'+player.Record));
					more_nomail();
					break;
			}
		} while (dch !== 'R');

		if (random(25) === 17) {
			blackjack();

		}
	}

	function find_lost_gold() {
		var found;
		var left = parseInt(player.gold / 15, 10);
		var m;

		if (left < 100) {
			left = 100;
		}
		if (psock !== undefined) {
			psock.writeln('GetForestGold '+left);
			found = psock.readln();
			m = found.match(/^ForestGold ([0-9]+)$/);
			if (m === null) {
				throw('Out of sync with server after GetForestGold');
			}
			found = parseInt(m[1], 10);
		}
		else {
			get_state(true);
			found = state.forest_gold;
			state.forest_gold = left;
			put_state();
		}
		if (found < 100) {
			found = 100;
		}
		if (player.gold + found > 2000000000) {
			found = 2000000000 - player.gold;
		}
		lln('`c  `%Event In The Forest`0');
		lln('`l');
		sln('');
		lln('  `2Fortune smiles, and you find `%'+pretty_int(found)+' `2gold!');
		player.gold += found;
		sln('');
		more();
		sclrscr();
	}

	function rescue_the_princess() {
		var him;
		var lad;
		var ich;
		var castle;

		sclrscr();
		sln('');
		sln('');
		foreground(15);
		sln('                         FOREST EVENT');
		foreground(2);
		sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(10);
		if (player.horse) {
			sln('  Your horse stumbles.');
		}
		else {
			sln('  You stumble.');
		}
		sln('');
		foreground(2);
		if (player.done_tower) {
			sln('  Sure enough.  Another dead bird with a scroll.');
			sln('');
		}
		else {
			sln('  This would normally not be noted, since this happens all the');
			sln('  time - but what is interesting here is WHAT you stumbed over.');
			sln('');
			more();
			foreground(10);
			sln('  The object in question is a large dead bird.  Its stink is great.');
			sln('');
			foreground(2);
			sln('  This would normally not be noted either, since the forest is full of');
			sln('  dead things, and sometimes they rot before being eaten.');
			sln('');
			foreground(10);
			sln('  But the bird has a small scroll carefully tied to one leg.');
			sln('');
		}
		more();
		lln('  `r1  `%THE SCROLL READS:  `r0');
		sln('');
		foreground(7);
		switch(random(3)) {
			case 0:
				sln('  Dear whomever:');
				sln('');
				sln('  My ruthless uncle has trapped me in his castle.  He is angered');
				sln('  because I will not submit to him - in the way that he wants me');
				sln('  to.  ');
				sln('');
				sln('  Please come save me,');
				sln('        -Sleepless in a tower');
				break;
			case 1:
				sln('  Dear Brave Heart:');
				sln('');
				sln('  I am to wed one against my will.  My father tells me I am');
				sln('  selfish, because this political marriage will bring peace.');
				sln('');
				sln('  Get me out of here,');
				sln('        -a prisoner of war');
				break;
			case 2:
				sln('  To whom it may concern:');
				sln('');
				sln('  Geez am I bored.  I\'ve been locked in the highest peak');
				sln('  of this castle for an ever so long time.  Crikey,  I would');
				sln('  explain why, but I\'m running out of ink.  You see, I\'ve been');
				sln('  up here for such a long time, it\'s drying up.  Well what do you');
				sln('  know!  Guess I have enough to explain after all!  Isn\'t that a');
				sln('  lucky break?  Ok, I\'m up here because I wa..');
				sln('');
				sln('  The note isn\'t signed.');
				break;
		}
		sln('');
		foreground(15);
		sln('  You quickly swipe a tear from your eye as you put down the note.');
		sln('');
		more();
		if (player.sex === 'M') {
			him = 'her';
			lad = 'girl';
		}
		else {
			him = 'him';
			lad = 'lad';
		}
		lln('  `2(`0S`2)ave '+him);
		lln('  `2(`0I`2)gnore the '+lad);
		sln('');
		lw('  `2Well? [`%S`2] `8: `%');
		ich = getkey().toUpperCase();
		if (ich !== 'I') {
			ich = 'S';
		}
		sln(ich);
		sln('');
		if (ich === 'I') {
			sln('  You look at the note a moment - then blithely toss it in a');
			sln('  nearby creek.  You skip away merrily! (evil can be fun!)');
			sln('');
			more();
			return;
		}
		lln('  `0You\'re quite a hero.  Unfortunatly, the '+lad+' seems to have');
		sln('  forgotten the return address.  You\'ll have to guess.');
		sln('');
		lln('  `2(`0C`2)astle Coldrake');
		lln('  `2(`0F`2)ortress Liddux');
		lln('  `2(`0G`2)annon Keep');
		lln('  `2(`0P`2)enyon Manor');
		lln('  `2(`0D`2)ema\'s Lair');
		sln('');
		lw('  Where do we go now? `8: `%');
		do {
			ich = getkey().toUpperCase();
		} while ('CFGPD'.indexOf(ich) === -1);
		castle = 'CFGPD'.indexOf(ich) + 1;
		sln(ich);
		sln('');
		lln('  `2You set your jaw resolutely and journey to `%'+castles[castle]+'`2.');
		sln('');
		more();
		sclrscr();
		lrdfile('TOWER');
		ich = getkey();
		sclrscr();
		sln('');
		sln('');
		lln('                         `%THE RESCUE');
		lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		player.done_tower = true;
		lln('  `%THE DOOR SWINGS WIDE OPEN!');
		sln('');
		more();
		if (castle === which_castle) {
			which_castle = random(5)+1;
			lln('  `0"You\'ve come for me!" `2shouts an overjoyed (and darn good looking)');
			lln('  '+lad+'.');
			sln('');
			lln('  `2You breathe a sigh of relief.  This was the right place.');
			sln('');
			if (player.sex === 'M') {
				if (!settings.clean_mode) {
					lln('  `2The girl eyes you dreamily.  `0"I can never repay you, and I.."');
					sln('');
					lln('  `%"Oh but you can.  Is that your bed?" `2you interrupt.');
				}
				lln('');
				more();
				lln('  `2'+pretty_int(player.cha)+' minute'+(player.cha > 1 ? 's' : '')+' later, you feel quite repaid.');
			}
			sln('');
			lln('  `0YOU GET `%'+pretty_int((player.level * player.level) * 20) +' `0EXPERIENCE.');
			player.exp += player.level * player.level * 20;
			if (player.exp > 2000000000) {
				player.exp = 2000000000;
			}
			sln('');
			more();
			if (player.sex === 'M') {
				if (!settings.clean_mode) {
					lln('  `%"Well, that was fun, gotta go," `2you mutter as you throw your tunic');
					sln('  back on.');
					sln('');
					player.laid += 1;
					lln('  A sweet voice from the bed stops you as you hit the stairs. `0"Wait!');
					sln('  I have something else for you too."');
					lln('');
					more();
				}
				lln('  `2Your blank face turns to joy as she hands you a pouch.');
			}
			else {
				lln('`2  Your blank face turns to joy as he hands you a pouch.');
			}

			sln('');
			lln('  `0YOU GET `%'+pretty_int(3*player.level)+' `0GEMS FOR YOUR TROUBLE.');
			player.gem += 3 * player.level;
			if (player.gem > 2000000000) {
				player.gem = 2000000000;
			}
			sln('');
			if (player.sex === 'M') {
				log_line('  `0'+player.name+' `2saved a princess today!');
			}
			else {
				log_line('  `0'+player.name+' `2saved a prince today!');
			}
			tournament_check();
		}
		else {
			switch(random(2)) {
				case 0:
					lln('  `2The room is empty, save a giant chest in the middle.');
					// DIFF: This didn't decrease your HP if it was less than 4...
					player.hp = parseInt(player.hp / 4, 10);
					if (player.hp < 1) {
						player.hp = 1;
					}
					sln('');
					lln('  `%"Hello?  Anybody home?" `2you call softly.');
					sln('');
					sln('  You jump in surprise when you hear a voice answer - from the chest.');
					sln('');
					lln('  `0"Help me!  I\'m in here!  I wrote the note!"`2 the voice pleads.');
					sln('');
					more();
					lln('  `%"Um, how did you write the note from in there?" `2you wonder out loud.');
					sln('');
					lln('  `0"Nevermind that!  Just help me!"');
					sln('');
					more();
					lln('  `%"Fine." `2you open the chest.  Surprisingly, there is not a fair');
					if (player.sex === 'M') {
						sln('  maiden inside.  Instead, you find a..');
					}
					else {
						sln('  prince inside.  Instead, you find a..');
					}
					sln('');
					more();
					if (player.sex === 'M') {
						lln('  `%"`bMY GOD A HOLL!  `%A HALF HUMAN HALF TROLL WOMAN!  NOOO!" `2you ');
					}
					else {
						lln('  `%"`bMY GOD A HOLL!  `%A HALF HUMAN HALF TROLL MAN!  NOOO!" `2you ');
					}
					sln('  scream. ');
					sln('');
					more();
					lln('`c                         `%THE UGLY PART');
					foreground(2);
					sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					if (!settings.clean_mode) {
						if (player.sex === 'M') {
							sln('  You are violated by this hairy beast.  Apparently she needs love');
						}
						else {
							sln('  You are violated by this hairy beast.  Apparently he needs love');
						}
						sln('  like anyone else.');
					}
					else {
						sln('  The thing beats you up.  Yeah.  Real bad.');
					}
					sln('');
					foreground(4);
					if (player.sex === 'M') {
						sln('  YOU WEAKLY CRAWL AWAY FROM HER SOMETIME LATER.');
					}
					else {
						sln('  YOU WEAKLY CRAWL AWAY FROM HIM SOMETIME LATER.');
					}
					sln('');
					log_line('  `0'+player.name+' `2had a nasty run in with a Holl!');
					more();
					return;
				case 1:
					foreground(10);
					if (player.sex === 'M') {
						sln('  You see two beautiful women playing chess.');
						sln('');
						lln('  `%"Hello, ladys.  Which one of you needs rescuing?" `2you ask politely.');
					}
					else {
						sln('  You see two handsome men playing chess.');
						sln('');
						lln('  `%"Hello, boys.  Which one of you needs rescuing?" `2you ask politely.');
					}
					sln('');
					more();
					lln('  `0"Neither!" `2they chime.');
					sln('');
					if (player.sex === 'M') {
						lln('  `2You scratch your chin in confusion.  `%"So where is the damn damsel');
					}
					else {
						lln('  `2You scratch your chin in confusion.  `%"So where is the stinkin\' prince');
					}
					sln('  in distress?!"');
					sln('');
					more();
					lln('  `0At this moment, a messager burst through the broken doorway.');
					sln('');
					lln('  `5"My friends, I bear terrible news - `%'+castles[castle]+'`5 has been');
					lln('  attacked.  Your father the King is dead." `2the messenger is now audibly');
					sln('  sobbing.');
					sln('');
					more();
					lln('  `%"Ah.  Yes.  Well, this is all very tragic, but I uh, need to be going." `2you');
					sln('  studder uncomfortably.');
					sln('');
					foreground(10);
					if (player.sex === 'M') {
						sln('  The now ashen white faced women look at you dumbfounded as you make');
					}
					else {
						sln('  The now ashen white faced men look at you dumbfounded as you make');
					}
					sln('  your exit.');
					sln('');
					if (player.sex === 'M') {
						log_line('  `0'+player.name+' `2showed courage today by trying to save a princess.');
					}
					else {
						log_line('  `0'+player.name+' `2showed courage today by trying to save a prince.');
					}
					break;
			}
		}
		more();
	}

	function olivia() {
		var och;
		var tmp;

		function olivia_getch(chars) {
			var ich;

			do {
				ich = getkey().toUpperCase();
				if (ich === '\r') {
					ich = chars[0];
				}
			} while(chars.indexOf(ich) === -1);
			sln(ich);
			return ich;
		}

		function asshole() {
			var ich;
			var cannot;

			lln('`c  `%THE WAILING GROWS LOUDER.');
			sln('');
			lln('  You investigate - only to find a womans head on the ground.');
			lln('');
			if (player.sex === 'M') {
				lln('  `0"I see you, foolish boy.  Leave me alone!" `2the head screams savagely.');
			}
			else {
				lln('  `0"I see you, foolish girl.  Leave me alone!" `2the head shouts.');
			}
			sln('');
			lln('  `2(`0A`2)pologize for what you did last time');
			lln('  `2(`0P`2)lay some "head ball"');
			sln('');
			lw('  `2Well? [`0A`2] `2: `0');
			ich = olivia_getch('AP');
			sln('');
			if (ich === 'P') {
				switch (random(3)) {
					case 0:
						lln('  `2You are distracted by a huge spider climbing up the cave wall.');
						sln('');
						lln('  `0Thinking quickly, you pick up the head and throw it at it!');
						sln('');
						more();
						lln('  `2You hear a satisfying squishing sound.  As you leave you hear the');
						lln('  head screaming after you.  `0"How dare you!!  You\'re going to pay!"');
						sln('');
						lln('  `%THE EXCERCISE GIVES YOU STRENGTH FOR ANOTHER FOREST FIGHT!');
						player.forest_fights += 1;
						if (player.forest_fights > 32000) {
							player.forest_fights = 32000;
						}
						sln('');
						more();
						return;
					case 1:
						lln('  `2Seeing the fine texture of the heads hair and the rip in your');
						sln('  garment, you decide to do something about it.');
						sln('');
						more();
						lln('  `0"PUT ME DOWN!" `2the head screams as you cut off a few strands of');
						sln('  its long hair and mend the tear.');
						sln('');
						cannot = false;
						if (player.clss === 3) {
							if (player.level < 2) {
								cannot = true;
							}
						}
						else {
							if (player.level < 6) {
								cannot = true;
							}
						}
						if (cannot) {
							lln('  `4You lack the skill needed to sew your '+player.arm+'.');
						}
						else {
							lln('  `2There!  Your '+player.arm+' is better than ever!');
							sln('');
							lln('  `%YOU FEEL SO CLEVER YOU GAIN THE STRENGTH FOR ANOTHER FOREST FIGHT!');
							player.forest_fights += 1;
							if (player.forest_fights > 32000) {
								player.forest_fights = 32000;
							}
						}
						sln('');
						more();
						return;
				}
			}
			switch (random(3)) {
				case 0:
					lln('  `0"I\'m very sorry, ma\'am.  I was a jerk."');
					sln('');
					lln('  `%She narrows her eyes at you.');
					sln('');
					more();
					lln('  `0"And I uh, think you are perfectly beheading.  I mean, becoming!"');
					sln('');
					lln('  `#"AWK!! GO DIE YOU PIECE OF <choking spasm>" `2she screams.');
					sln('');
					lln('  `4YOU FEEL SO WOEBEGONE YOU LOSE A FOREST FIGHT FOR TODAY.');
					sln('');
					break;
				case 1:
					lln('  `0"Hey?  My old pal!  What are you doing in this \'neck\' of the');
					lln('  woods?" `2you inquire sincerely.');
					sln('');
					lln('  `%She narrows her eyes at you.');
					sln('');
					more();
					lln('  `0"Don\'t be mad!  Please!  Geez, time to try decapit..I mean,');
					sln('  decaffinated!"');
					sln('');
					lln('  `#"GO EAT BUGS AND DIE, YOU HEARTLESS TROLL!" `2she screams.');
					sln('');
					lln('  `4YOU FEEL SO CRAPPY YOU LOSE A FOREST FIGHT FOR TODAY.');
					sln('');
					break;
				case 2:
					lln('  `0"You know before?  I was just kidding \'round!  Use your head');
					lln('  woman! `2you say earnestly.');
					sln('');
					lln('  `%She narrows her eyes at you.');
					sln('');
					more();
					lln('  `0Yeah, just joking!  Man you are so guillotine..I mean, er');
					lln('  gullible!"');
					sln('');
					lln('  `#"I HATE YOU!" `2she screams.');
					sln('');
					lln('  `4YOU FEEL SO WOEBEGONE YOU LOSE A FOREST FIGHT FOR TODAY.');
					sln('');
					break;
			}
			player.forest_fights -= 1;
			more();
		}

		if (player.olivia_asshole) {
			asshole();
			return;
		}
		if (!player.olivia) {
			lln('`c  `%THE WAILING GROWS LOUDER');
			sln('');
			lln('  `2You catch a glimpse of something moving in the mouth of the');
			sln('  cave.');
			sln('');
			lln('  `2(`0I`2)nvestigate further');
			lln('  `2(`0G`2)et smart and leave it alone');
			sln('');
			lw('  `2Well? [`0I`2] : `0');
			och = olivia_getch('IG');
			if (och === 'G') {
				sln('');
				lln('  `2You hurry away from this evil place.');
				sln('');
				more();
				return;
			}
			sln('');
			sln('  You slowly make your way inside the the damp cave.  You feel a cold');
			sln('  breeze - or perhaps you just shivered for another reason.');
			sln('');
			lln('  `4YOU TRIP OVER SOMETHING!');
			sln('');
			more_nomail();
			lln('  `2You blindly reach down `8- `2your fingers find something wet `8- `2You have');
			lln('  put your hand inside the mouth of a severed head.  `0You scream like a child!');
			sln('');
			more_nomail();
			lln('  `#"Oh, do shut up!" `2the head implores you, scowling.');
			sln('');
			if (player.sex === 'M') {
				lln('  `2You stare at the head in shock.  (which really isn\'t bad looking)');
			}
			else {
				lln('  `2You stare at the head in shock.');
			}
			sln('');
			lln('  `2(`0A`2)sk the head who she is');
			lln('  `2(`0B`2)oot her a distance');
			sln('');
			lln('  `2After careful consideration you.. [`0A`2] `8: `%');
			och = olivia_getch('AB');
			sln('');
			if (och === 'B') {
				lln('  `0"C\'mere you freak of nature!" `2you taunt, grabbing her by the hair.');
				sln('');
				lln('  `#"NooOoooOooo!" `2she screams as you toss her deep into the underbrush.');
				sln('');
				if (player.sex === 'M') {
					lln('  `2You can\'t stop thinking about what she would be like in bed.');
				}
				else {
					lln('  `2You can\'t help but wonder if you made the right choice.');
				}
				sln('');
				player.olivia_asshole = true;
				more();
				return;
			}
			player.olivia = true;
			lrdfile('OLIVIA');
			return;
		}
		lln('`c  `%WAIT A SEC!');
		sln('');
		lln('  `2It\'s just your old pal `%Olivia `2the bodyless woman.');
		sln('');
		if (player.olivia_count === 0) {
			lln('  `0"I was wondering if you were coming back," `2she informs you.');
			sln('');
		}
		if (player.olivia_count === 1) {
			lln('  `0"You want more information I suppose." `2she says rather sadly.');
			sln('');
		}
		if (player.olivia_count === 2) {
			lln('  `0"Life is worthless, I am worthless." `2she drones.');
			sln('');
			sln('  You realize she is dangerously depressed.');
			sln('');
		}
		if (player.olivia_count === 3) {
			lln('  `0"I wish I had a friend.  You just use me." `2she rebukes.');
			sln('');
			lln('  She\'s still depressed apparently.');
			sln('');
		}
		if (player.olivia_count === 4) {
			lln('  `0"It\'s about time.  I do get bored you know," `2she scolds.');
			lln('');
		}
		if (player.olivia_count === 5) {
			lln('  `0"You\'re looking sharp today, '+player.name+'`0," `2she complements.');
			lln('');
		}
		if (player.olivia_count === 6) {
			lln('  `0"'+player.name+'`0! I\'ve missed you!" `2she exclaims.');
			sln('');
			sln('  Odd, considering you were just here not too long ago.');
			sln('');
		}
		if (player.olivia_count > 7 && player.sex === 'F') {
			player.olivia_count = 7;
		}
		if (player.olivia_count === 7) {
			lln('  `0"Hello, '+player.name+'`0.  I\'m glad you could visit."`2 she smiles.');
			sln('');
			sln('  She seems happy.');
			sln('');
		}
		if (player.olivia_count === 8) {
			lln('  `0"Do you find me attractive?" `2she asks pensively.');
			sln('');
			lln('  `%"Why?  Are you unattached?" `2you smirk.');
			sln('');
			lln('  `0"Do shutup!" `2she screeches.');
			sln('');
		}
		if (player.olivia_count === 9) {
			lln('  `0"Am I pretty?" `2she asks hopefully.');
			sln('');
			lln('  `%"Well, uh, you don\'t weigh too much." `2you smirk.');
			sln('');
			lln('  `0"I\'m being serious!" `2she screeches.');
			lln('');
		}
		// DIFF: The test for !== 25 wasn't here... always show the history now.
		if (player.olivia_count === 10 || (player.olivia_count !== 25 && player.olivia_count > 10 && random(7) === 1)) {
			player.olivia_count += 1;
			if (!settings.clean_mode) {
				if (player.olivia_count === 10) {
					lln('  `0"I\'m through with the hints!  Do you want to mess around?" `2she');
					sln('  states bluntly.');
					sln('');
					lln('  `%"Olivia, I\'m flattered - But .. you don\'t have a body."');
					sln('');
					more();
					lln('  `0"If we put our minds to it, we can figure out a way." `2she grins');
					lln('  wickedly.');
					sln('');
					more();
				}
				else {
					lln('  `0"I\'m ready for love again.    Do you want to mess around?" `2she');
					sln('  states bluntly.');
					lln('');
				}
				lln('  `2(`0B`2)e pleasured by Olivia');
				lln('  `2(`0T`2)urn her down.');
				sln('');
				lw('  What will it be? [`0B`2] `8:`% ');
				och = olivia_getch('BT');
				if (och === 'B') {
					sln('');
					lln('  `0You stroke her stump lovingly.');
					sln('');
					more();
					lln('  `2Olivia gives all she has.  And in fact, what she is.');
					lln('');
					more();
					if (player.has_fairy) {
						lln('  `b** `%UH OH! `b**');
						lln('');
						lln('`2  You feel a buzzing in your pocket.  You smile, your old twig &');
						lln('  berries still works.  `0The buzzing grows uncomfortably loud.');
						lln('');
						log_line('  `0'+player.name+'\'s `2fairy escapes.');
						player.has_fairy = false;
						player.olivia_count = 0;
						more();
						sln('');
						lln('`%  A tiny light shoots out of your trousers!');
						sln('');
						lln('  `2Olivia blinks in surprise.');
						sln('');
						more();
						sln('  You wince - Your fairy is loose!  You attempt to suavely cover up');
						sln('  your \'accident\'.');
						sln('');
						lln('  `%"Wow baby!  Uh, that was great, gotta go!"');
						sln('');
						more();
						lln('  `2You leave an angry Olivia while you search around her cave for the');
						sln('  little creature.  Looks like it\'s gone for good.');
						sln('');
						lln('  `4FOR BLOWING IT AND MAKING OLIVIA MAD, YOU GET NO EXPERIENCE.');
						sln('');
						more();
						return;
					}
					log_line('  `0'+player.name+' `2walks out of the forest acting chipper!');
					lln('  `2A very short time later...');
					sln('');
					lln('  `%"That was wonderfull!  Can I put you in my napsack and take you');
					lln('  with me?" `2you ask hopefully.');
					sln('');
					lln('  `0"No, I don\'t think so.  My place is here, I would just be a freak');
					lln('  and an oddity anywhere else." `2she answers sadly.');
					sln('');
					more();
					lln('  `%"But.. that\'s what you are here too!  Keep your chin up, honey." `2you');
					sln('  clumsly encourage.');
					sln('');
					lln('  `2She smiles wanly.');
					sln('');
					tmp = player.level * player.level * 25;
					lln('  `%YOU RECIEVE '+pretty_int(tmp)+' EXPERIENCE.');
					player.exp += tmp;
					if (player.exp > 2000000000) {
						player.exp = 2000000000;
					}
					player.olivia_count += 1;
					sln('');
					more();
					tournament_check();
					return;
				}
			}
			if (settings.clean_mode) {
				lln('  `0"I\'ll never find a husband!" `2Olivia cries for no apparent reason.');
				lln('');
			}
			sln('');
			lln('  `%"Lets not get a ..head of ourselves.  You need someone who you');
			lln('  can relate too.  Someone who... well also is a severed head?" `2you');
			sln('  let her down as easy as you can.');
			sln('');
			lln('  `0"But I\'ll NEVER find him!!" `2Olivia wails.');
			sln('');
			more();
			lln('  `0"Oh yes you will!  Someday." `2you lie glibly.');
			sln('');
			lln('  `2Feeling uncofortably, you decide to take off.');
			sln('');
			more();
			return;
		}
		if (player.olivia_count === 25) {
			lln('  `2Olivia is perched on a rock waiting for you.');
			sln('');
			lln('  `0"Hello.  I need to tell you something.  I need to tell you');
			sln('  why they beheaded me."');
			sln('');
			more();
			lln('  `%"Fine.  If you need to get this off your che, <cough>, uh, go ahead."');
			lln('  `2you encourage.');
			sln('');
			more();
			lln('`c                         `%OLIVIAS HISTORY');
			lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			lln('  `2Olivia was not beheaded publicly.  She was not even beheaded');
			sln('  by a decree of the king.');
			sln('');
			lln('  She was decapitated by a man named `0Earnest Drinklewip`2. ');
			sln('');
			sln('  Why did he do it?  Olivia tells you because she would not find');
			sln('  favor with him.  His reason was she was a witch.  She assures you');
			sln('  she is not.');
			sln('');
			sln('  You don\'t know what to believe - all you know for sure is this man');
			sln('  slew her, and put her head in this cave to rot.  The problem is it');
			sln('  didn\'t. ');
			sln('');
			more();
			lln('  `%"So why aren\'t you dead and rotting, Olivia?" `2you ask, confused.');
			sln('');
			lln('  `0"I don\'t know!  Maybe I\'m charmed, maybe I\'m cursed.  I don\'t know."');
			sln('');
			more();
			lln('  `%"I\'m going to find \'em!  And find out what happened to the rest of');
			sln('  you!  Goodbye for now!"');
			sln('');
			player.olivia_count += 1;
			more();
			return;
		}
		if (player.olivia_count > 10 && player.olivia_count < 18) {
			lln('  `0Olivia warmly greets you.');
			sln('');
		}
		if (player.olivia_count > 17 && player.olivia_count < 21) {
			lln('  `0Olivia greets you with a butterfly kiss.');
			sln('');
		}
		if (player.olivia_count > 20) {
			lln('  `0Olivia greets you with a head hug.');
			lln('');
		}
		if (player.olivia_count > 100) {
			player.olivia_count = 100;
		}
		tmp = 'G';
		lln('  `2(`0G`2)et inside her head');
		if (player.sex === 'M') {
			tmp += 'A';
			lln('  `2(`0A`2)sk for a kiss.');
		}
		else {
			lln('  `2(`0C`2)onsole her.');
			lln('  `2(`0D`2)o her hair.');
			tmp += 'CD';
		}
		sln('');
		lw('  What will it be? [`0G`2] `8:`% ');
		och = olivia_getch(tmp);
		player.olivia_count += 1;
		player.put();
		switch (och) {
			case 'C':
				if (player.sex !== 'F') {
					break;
				}
				sln('');
				lln('`2  You feel sorry for the head, and attempt to solace her.');
				lw('  `%"');
				switch(random(5)) {
					case 0:
						lln('Don\'t feel bad Olivia, I\'ll help you cope." `2you offer.');
						break;
					case 1:
						lln('We girls will always stick together!" `2you swear.');
						break;
					case 2:
						lln('You\'re lucky, you don\'t have to watch your weight."`2you tell her.');
						break;
					case 3:
						lln('Why Olivia, I love that uh, hair style you have today!" `2you compliment.');
						break;
					case 4:
						lln('I\'m your friend always.  Lets do each others hair." `2you offer.');
						break;
				}
				sln('');
				if (player.olivia_count < 6) {
					lln('  `2She thanks you, and feels better!');
					sln('');
					lln('  `%HELPING ANOTHER LIFTS YOUR SPIRITS.');
					player.high_spirits = true;
				}
				else {
					lln('  `0"But '+player.name+'`0, I\'m not depressed anymore!" `2she laughs.');
					sln('');
				}
				sln('');
				more();
				return;
			case 'D':
				if (player.sex !== 'F') {
					break;
				}
				sln('');
				lln('`2  You take your brush to Olivia\'s tangled locks.');
				sln('');
				if (player.olivia_count < 6) {
					lln('  `0"Don\'t touch me!" `2Olivia screeches.');
					sln('');
				}
				else {
					lln('  `2She thanks you, and feels better!');
					sln('');
					lln('  `%HELPING ANOTHER LIFTS YOUR SPIRITS.');
					player.high_spirits = true;
					sln('');
				}
				more();
				return;
			case 'A':
				if (player.sex !== 'M') {
					break;
				}
				lln('`c                        `%LOVE IN YOUR HAND');
				lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				if (random(2) === 0) {
					lln('`2  You pick Olivia up by the ears and kiss her soundly.');
				}
				else {
					lln('`2  You carefully wipe the dirt of Olivia\'s lips and kiss her.');
				}
				if (player.olivia_acount < 6) {
					lln('  `0"Don\'t touch me!" `2Olivia screeches.');
					sln('');
				}
				else {
					sln('');
					lln('  `%YOU LEAVE IN HIGH SPIRITS!');
					sln('');
					player.high_spirits = true;
				}
				more();
				return;
			case 'G':
				lln('`c                        `%A FREUDIAN SLIP');
				lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				sw('  "');
				foreground(15);
				switch(random(5)) {
					case 0:
						lln('Penny for your thoughts?" `2you inquire.');
						break;
					case 1:
						lln('Watcha thinkin\'?" `2you ask politely.');
						break;
					case 2:
						lln('Will you tell me about yourself?" `2you ask Olivia.');
						break;
					case 3:
						lln('What\'s on your mind?" `2you inquire.');
						break;
					case 4:
						lln('You must be thinking hard, since thats all you can do." `2you laugh.');
						break;
				}
				sln('');
				lln('  `2Olivia nudges herself to a better speaking position.');
				sln('');
				if (random(2) === 0) {
					lln('  `0"I was just thinking about my past.  I grew up in a great palace.');
					sln('  I was pampered and treated like a queen.  In fact, I would have');
					lln('  been queen too - if not for the evil Duke of..`%'+castles[which_castle]+'`0 was');
					sln('  it?  Something like that.  And then.."');
					sln('');
					more();
				}
				else {
					lln('  `0"Well..  I was thinking about when I had a body.  A very beautiful');
					sln('  one if I do say myself.  Men followed me like flies on honey.  Especially');
					lln('  one man.  At the time he was a town crier at `%'+castles[which_castle]+'`0. ');
					sln('');
					sln('  It is because of him I am like this..You see, he.."');
					sln('');
					more();
				}
				break;
		}
		lln('  `%"Getting bored here.  Gotta go, see ya," `2you break in rudely.');
		sln('');
		sln('  You travel back to the forest entrance.');
		lln('');
		more();
		return;
	}

	function load_monster(num) {
		var fname = game_or_exec('lenemy.bin');
		var f;
		var ret;

		if (file_exists(fname) && !settings.no_lenemy_bin) {
			f = new RecordFile(fname, Monster_Def);
			ret = f.get(num);
			f.file.close();
		}
		else {
			ret = {};
			Object.keys(monster_stats[num]).forEach(function(s) {
				ret[s] = monster_stats[num][s];
			});
		}
		beef_up(ret);
		return (ret);
	}

	function save_forest_gold(amt) {
		amt = parseInt(amt / 15, 10);
		if (psock !== undefined) {
			psock.writeln('AddForestGold '+amt);
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after AddForestGold');
			}
		}
		get_state(true);
		state.forest_gold += amt;
		put_state(true);
	}

	function death_knight_level() {
		var title;
		var ich;
		var guilty;

		sln('');
		sln('');
		foreground(15);
		sln('  Event In The Forest');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(2);
		sln('  While trekking through the forest, you come to the hidden castle of');
		sln('  The Black Knights.  You are immediately greeted by a score of men in shiny');
		sln('  black armour.');
		sln('');
		title = (player.sex === 'F' ? 'Lady' : 'Lord');

		if (player.skillw > 39) {
			lln('  `0"Well met '+title+' `%'+player.name+'`0!  A fellow Black Knight is');
			lln('  Always welcome here."`2  You walk the grounds, and eat with your');
			sln('  comrades.  You are fully refreshed.');
			lln('');
			if (player.hp < player.hp_max) {
				player.hp = player.hp_max;
			}
			player.levelw += 1;
			lln('  `%HIT POINTS FILLED AND YOU RECEIVE THE ENERGY FOR 1 DEATH KNIGHT ATTACK!');
			sln('');
			more();
			return;
		}
		if (player.skillw < 20) {
			lln('  `0"Well met '+title+' `%'+player.name+'`0!  We have been expecting you.  We');
			sln('   know you aspire to join us, and become a Death Knight.  We will');
			sln('   teach you a lesson today, but only if you pass a test of our device."');
		}
		else {
			lln('  `0"Greetings '+title+' `%'+player.name+'`0!  What are you doing around here?!');
			lln('   Even tho you are already a member, now is a great time to practice');
			lln('   your skills.  To become an even better warrior.  You know the routine..."');
		}

		sln('');
		foreground(2);
		sln('  THEY LEAD YOU TO THE DEATH KNIGHT DUNGEON.');
		sln('');
		more_nomail();
		foreground(15);
		sclrscr();
		sln('');
		sln('');
		sln('                            ** THE TEST **');
		sln('');
		lln('  `2You ignore the shrieks of pain, the suffering of this dungeon\'s occupants.');
		sln('  You are shown a man kneeling over a stained chopping block.');
		sln('');
		lln('  `0"This man is accused of a crime.  Is he innocent or guilty?"');
		sln('');
		lln('  `2(`01`2) Decapitate Him');
		lln('  (`02`2) Release Him');
		sln('');
		command_prompt();
		ich = getkey().toUpperCase();
		if (ich !== '2') {
			ich = '1';
		}
		sln(ich);
		sln('');
		guilty = !!(random(2) === 1);
		sln('');
		if (ich === '1') {
			lln('  `2You take the axe and bring it down as hard as you can.  After');
			sln('  a sickening (but satisfying) crunch the deed is done.');
			sln('');
		}
		else {
			lln('  `%"That man is innocent!  You shall not harm a hair on his');
			lln('  head, as long as I have a breath in me to fight!" `2 you ');
			sln('  shout dramatically.');
		}
		lw('  `0"You have chosen.');
		mswait(500);
		sw('.');
		mswait(500);
		sw('.');
		mswait(500);
		if (guilty) {
			if (ich === '1') {
				lln(' `%WISELY!`0"');
				sln('  "You have done this country justice today."');
				foreground(2);
				sln('');
				more();
				lln('');
				raise_class();
			}
			else {
				lln(' `4POORLY`0."');
				sln('  "That man raped 6 women.  And you defend him?  Good God man!');
				sln('');
				more();
				sln('');
				return;
			}
		}
		else {
			if (ich === 1) {
				lln(' `4POORLY`0."');
				sln('  "This man did no crime.  He was the father of 6 children.  "Was"');
				sln('   being the key word here.  Perhaps another time."');
				foreground(2);
				sln('');
				if (player.olivia_count > 7) {
					foreground(2);
					sln('  Thinking of Olivia, you pick up the severed head and check it for life.');
					sln('');
					foreground(15);
					sln('  IT\'S DEAD, BUT YOUR KINDNESS MAKES YOU BEAUTIFUL. (1 CHARM ADDED)');
					sln('');
					player.cha += 1;
				}
				more();
				sln('');
				return;
			}
			lln(' `%WISELY!`0"');
			sln('  "You speak eloquently.  Your words do not fall upon deaf ears.');
			sln('   We believe you.  You are wise today."');
			sln('');
			more();
			sln('');
			raise_class();
		}
	}

	function mystical_level() {
		var ich;
		var thenum;
		var guesses = 0;
		var guess;

		sln('');
		sln('');
		foreground(15);
		sln('  Event In The Forest');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(2);
		sln('  While trekking through the forest, you come upon a small hut.');
		sln('');
		lln('  `2(`0K`2)nock On The Door');
		lln('  (`0B`2)ang On The Door');
		lln('  (`0L`2)eave It Be');
		sln('');
		command_prompt();
		ich = getkey().toUpperCase();
		if ('LB'.indexOf(ich) === -1) {
			ich = 'K';
		}
		sln(ich);
		if (ich === 'L') {
			sln('  You walk away.  Who needs magical instruction anyway!');
			sln('');
			more();
			return;
		}
		sln('');
		if (ich === 'K') {
			sln('  You politely knock on the knotted wooden door.');
		}
		if (ich === 'B') {
			sln('  You bang on the door as hard you can!');
		}
		sln('');
		if (random(4) === 1) {
			sln('  You wait a while but no one is home.  Maybe next time.');
			sln('');
			more();
			return;
		}
		lln('`2  You are about to leave, when you hear a voice from above:');
		if (player.sex === 'M') {
			lln('  `0"Watcha doin\' down there Sonny?!" `2 You look up and see a wizened');
		}
		else {
			lln('  `0"Watcha doin\' down there Miss?!"  `2You look up and see a wizened');
		}
		sln('  old man.');
		sln('');
		lln('  `0"Tell ya what!  I\'ll give ya a mystical lesson if you can pass my');
		lln('   test!" `2the old man giggles.');
		sln('');
		more();
		lln('`c`%                            ** THE TEST **');
		sln('');
		lln('  `0"All right now!  I\'m thinking of a number between 1 and 100.  I\'ll');
		sln('   give ya six guesses."');
		sln('');
		lln('`2  (The old man leans even farther out the window in anticipation)');
		sln('');
		thenum = random(100) + 1;
		while (guesses < 6) {
			guesses += 1;
			lw('  `2Guess `0'+guesses+' `2: `%');
			guess = parseInt(dk.console.getstr({len:3, integer:true}), 10);
			if (isNaN(guess)) {
				guess = 0;
			}
			if (guess > thenum) {
				lln('  `0"The number is lower than that!"');
			}
			else if (guess < thenum) {
				lln('  `0"The number is higher than that!"'); }
			else if (guess === thenum) {
				sln('');
				lln('  `0"That\'s right! That\'s the number I was thinking of!  You read my mind!"');
				lln('  `2The old man nearly falls from his window in his excitement!');
				lln('');
				lln('`%                         ** YOU HAVE PASSED THE TEST **');
				lln('');
				more_nomail();
				if (player.skillm > 39) {
					sln('');
					sln('  The old man attempts to teach you something, but fails.  You know more');
					sln('  than him.  The only thing he is able to give you is hope.');
					sln('');
					foreground(15);
					sln('  YOU RECEIVE FOUR EXTRA MYSTICAL SKILLS USE POINTS!');
					sln('');
					player.levelm += 4;
					more();
					return;
				}
				raise_class();
				return;
			}
		}

		if (guess !== thenum) {
			sln('');
			lln('  `2The old man drops his head and shakes it sadly.  You notice small');
			lln('  dandruff flakes drifting down from the window. `0"No, no, NO!  The number');
			sln('  was '+thenum+'!  Geez!  I won\'t teach such an unpromising student!"');
			sln('');
			if (player.skillm > 19) {
				lln('  `0"I\'m already a Master anyway, old man," `2you retort coldy.');
				sln('');
			}
			lln('  `2He slams his window shut, and leaves you no recourse but to leave.');
			sln('');
			more();
		}
	}

	function thief_level() {
		var ich;
		sln('');
		sln('');
		foreground(15);
		sln('  Event In The Forest');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(2);

		if (player.skillt > 39) {
			lln('`1`2  You are carefully moving through the forest, making absolutely no noise');
			sln('  when your sensitive ears pick up a twig breaking.  You circle around');
			lln('  towards the noise, and find it\'s not an animal, but The Master Thieves!');
			sln('');
			lln('  As they pass under a tree you are in, you call out. `0"Ahh... Master');
			lln('  Thieves!  Do you think it would be possible to make even MORE noise?!"`2');
			sln('');
			sln('  The group is very embarrassed, but they overcome it to chew the fat with');
			sln('  you.');
			sln('');
			lln('  `%EXTRA THIEVING USE FOR TODAY!');
			lln('');
			player.levelt += 1;
			more();
			return;
		}

		lln('`2  You are innocently skipping through the forest, when you suddenly');
		sln('  notice you are surrounded by a group of rogues!');
		sln('');

		if (player.skillt < 20) {
			lln('  `0"Greetings, `%'+player.name+'`0.  We are members of the Master Thieves');
			sln('   Guild.  We know you are struggling to learn our ways.  We will');
			sln('   give you a lesson, for the price of one Gem."');
		}
		else {
			lln('  `0"Greetings, `%'+player.name+'`0.  We are proud that you have mastered');
			lln('   the skills.  But, we will continue to teach you anyway..For  ');
			lln('   the usual price!" `2The scarred man laughs gleefully.');
		}

		while (true) {
			sln('');
			lln('  `2(`0G`2)ive Them A Hard Earned Gem');
			lln('  `2(`0S`2)pit In Their Faces');
			lln('  `2(`0M`2)umble Apologies And Run');
			sln('');
			command_prompt();
			ich = getkey().toUpperCase();
			sln(ich);
			if (ich === 'M') {
				sln('');
				lln('  `0"You are nothing but a coward!  We will never let you join us!"');
				sln('');
				more();
				return;
			}
			if (ich === 'S') {
				sln('');
				lln('  `2You hawk a good sized piece of phlegm into the leader\'s face.');
				sln('');
				more_nomail();
				if (player.sex === 'F') {
					lln('  `0"You have spirit girl!  Maybe next time!"`2');
				}
				else {
					lln('  `0"You have spirit boy!  Maybe next time!"`2');
				}
				sln('  The man laughs.');
				sln('');
				more_nomail();
				return;
			}
			if (ich === 'G') {
				if (player.gem < 1) {
					sln('');
					sln('');
					lln('  `2You fumble through your pockets and find you don\'t posses a Gem.  You');
					sln('  don\'t think it would be wise to try to pull one over on the Master');
					sln('  Thieves\' Guild.  You have a reputation to worry about!');
					sln('');
					more_nomail();
				}
				else {
					player.gem -= 1;
					sln('');
					sln('  You nonchalantly flip them a sparkling Gem.  The Thieves look impressed.');
					sln('');
					lln('  `0"Nice rock.  Alright...True to our word, we will instruct you."`2');
					sln('');
					more();
					raise_class();
					return;
				}
			}
		}
	}

	function flower_garden() {
		var ich;
		var message;
		var pname;
		var f = new File(gamedir('garden.lrd'));
		var lines=[];
		var i;
		var gmat;

		sln('');
		sln('');
		foreground(15);
		sln('  Event In The Forest');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(2);
		sln('  You find a beautiful Garden, and decide to take a little rest...');
		sln('  You drink from the brook, and smell the flowers.');
		sln('');
		foreground(15);
		sln('  YOU ARE REFRESHED, AND GET ONE MORE FOREST FIGHT FOR TODAY!');
		sln('');
		player.forest_fights += 1;
		if (player.forest_fights > 32000) {
			player.forest_fights = 32000;
		}
		if (player.hp < player.hp_max) {
			player.hp = player.hp_max;
		}
		more();
		lln('  `0You notice the flowers seem to be arranged.');
		lw('  Study them? [`%Y`2] : `%');
		ich = getkey().toUpperCase();
		// DIFF: This wasn't done originally...
		if (ich !== 'N') {
			ich = 'Y';
		}
		sln(ich);
		if (ich === 'Y') {
			sclrscr();
			sln('');
			sln('');
			lln('  `%Understanding Pedals & Dew`2');
			sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			if (psock === undefined) {
				fmutex(f.name);
				if (!file_exists(f.name)) {
					if (file_exists(game_or_exec('gstart.lrd'))) {
						file_copy(game_or_exec('gstart.lrd'), gamedir('garden.lrd'));
					}
				}
				if (!file_exists(f.name)) {
					file_mutex(f.name, '  `0Fairy Tisha         `%-"`0Oooh!  I love flowers!  And I love to kiss.`%"\n');
				}
				// DIFF: No more prompt since we're holding a lock.
				display_file(f.name, true, false);
				rfmutex(f.name);
			}
			else {
				psock.writeln('GetConversation garden');
				message = psock.readln();
				gmat = message.match(/^Conversation ([0-9]+)$/);
				if (gmat === null) {
					throw('Invalid response to GetConversation');
				}
				message = psock.read(parseInt(gmat[1], 10));
				if (psock.read(2) !== '\r\n') {
					throw('Out of sync with server after GetConversation');
				}
				if (!settings.funky_flowers) {
					message = clean_str(message);
				}
				message = message.split(/\r?\n/).join('\r\n');
				show_lord_buffer(message, false, false);
			}
			sln('');
			lw('  `0Arrange them yourself? [`%N`2] : `%');
			ich = getkey().toUpperCase();
			if (ich !== 'Y') {
				ich = 'N';
			}
			sln(ich);
			if (ich === 'Y') {
				lln('  `2What would you like them to symbolize?');
				sw('  ');
				message = getstr(0, 0, 53, 1, 15, '');
				if (settings.funky_flowers) {
					message = message.replace(/``/g, '');
				}
				else {
					message = clean_str(message);
				}
				sln('');
				sln('');
				// DIFF: This was after the length check...
				message = message.replace(/\u0020+$/, '');
				if (disp_len(message) < 3) {
					sln('  You decide to arrange later.');
					sln('');
					more();
				}
				else {
					pname = space_pad(player.name, 20);
					if (psock === undefined) {
						fmutex(f.name);
						if (!f.open('r+')) {
							throw('Unable to open '+f.name);
						}
						f.position = 0;
						lines = f.readAll();
						lines.push('  `0'+pname+'`%- "`0'+message+'`r0`%"');
						while (lines.length > 20) {
							lines.shift();
						}
						f.position = 0;
						f.truncate(0);
						for (i = 0; i < lines.length; i += 1) {
							f.writeln(lines[i]);
						}
						f.close();
						rfmutex(f.name);
					}
					else {
						message = '  `0'+pname+'`%- "`0'+message+'`r0`%"';
						psock.write('AddToConversation garden '+message.length+'\r\n'+message+'\r\n');
						if (psock.readln() !== 'OK') {
							throw('Out of sync with server after AddToConversation garden');
						}
					}
					sln('');
					sln('');
				}
			}
		}
	}

	function look_to_kill() {
		var mnum;
		var ich;
		var tmp;

		sclrscr();
		if (random(5) === 1) {
			// TODO: Allow JS events...
			switch(random(15+(player.horse ? 1 : 0))) {
				case 0:
					lln('`c  `%Event In The Forest`0');
					lln('`l');
					sln('');
					lln('  `2You come across an old man.  He seems confused and asks if');
					sln('  you would direct him to the Inn.  You know that if you do,');
					sln('  you will lose time for one fight today.');
					sln('');
					lw('  Do you take the old man? [`0Y`2] : ');
					ich = getkey().toUpperCase();
					if (ich !== 'N') {
						ich = 'Y';
					}
					sln(ich);
					sln('');
					if (ich === 'Y') {
						sln('');
						tmp = player.level * 500;
						player.gold += tmp;
						lln('  `2You gladly take the old man to the Inn. He is pleased');
						lln('  with you, and gives you `%'+pretty_int(tmp)+' `2gold!');
						player.cha += 1;
						player.forest_fights -= 1;
						sln('');
						lln('  `%**CHARM GOES UP BY 1**');
					}
					else {
						if (player.cha < 10) {
							lln('  `2"`0I don\'t have time for you old man..Goodbye..`2"');
							sln('  You tell him coldly.  The old man shakes his head very sadly.');
						}
						else {
							lln('  `2"`0Old man, I think it\'s time you learned the way yourself.`2"');
							lln('  You tell him coldly.  The old man shakes his head sadly.');
						}
					}
					sln('');
					more_nomail();
					break;
				case 1:
					lln('`c  `%Event In The Forest`0');
					lln('`l');
					sln('');
					if (player.hp === player.hp_max) {
						lln('  `2You come across an ugly old hag.  `5"Give me a gem!  You won\'t be sorry');
						lln('  my pet!"`2 she screeches.');
					}
					else {
						lln('  `2You come across an ugly old hag.  `5"Give me a gem and I will completely');
						lln('  heal you warrior!"`2 she screeches.');
					}
					sln('');
					lw('  Give her the gem? [`0N`2] : ');
					ich = getkey().toUpperCase();
					if (ich !== 'Y') {
						ich = 'N';
					}
					sln(ich);
					sln('');
					if (ich === 'Y') {
						if (player.gem < 1) {
							lln('  `5"You have no gems, fool!"`2  the old woman screams at you.  She then');
							lln('   hits you in a sensitive spot with her cane, and you feel VERY weak.');
							player.hp = 1;
						}
						else {
							player.gem -= 1;
							sln('  You give her a gem.  She waves her wand strangely.');
							sln('');
							lln('  `%YOU FEEL BETTER');
							if (player.hp < player.hp_max) {
								player.hp = player.hp_max;
							}
							else {
								player.hp_max += 1;
							}
						}
					}
					else {
						lln('  `5"Hurumph!" `2 the old woman grunts sourly as you leave.');
					}
					sln('');
					more_nomail();
					break;
				case 2:
					lln('`c  `%Event In The Forest`0');
					lln('`l');
					sln('');
					tmp = (random(500) + 250) * player.level * player.level;
					lln('  `2You find a sack with `%'+pretty_int(tmp)+' `2gold in it!');
					player.gold += tmp;
					sln('');
					more_nomail();
					sclrscr();
					break;
				case 3:
					player.hp = player.hp_max;
					lln('`c  `%Event In The Forest`0');
					lln('`l');
					sln('');
					lln('  `2You stumble upon a group of Merry Men!  After partying with them');
					sln('  for 2 hours you feel totally refreshed.');
					sln('');
					more_nomail();
					sclrscr();
					break;
				case 4:
					lln('`c  `%Event In The Forest`0');
					lln('`l');
					sln('');
					lln('  `2Fortune smiles, and you find a gem!');
					sln('');
					player.gem += 1;
					more_nomail();
					sclrscr();
					break;
				case 5:
					if (random(2) === 1) {
						flower_garden();
					}
					else {
						lln('`c  `%Event In The Forest`0');
						lln('`l');
						sln('');
						lln('  `2You find a `%Hammer Stone`2!');
						sln('');
						more_nomail();
						lln('  `2As is the tradition, you break it in two with your');
						lln('  `0'+player.weapon+'`2. Your weapon tingles!');
						sln('');
						lln('  `%ATTACK STRENGTH RAISED.');
						sln('');
						player.str += 1;
						sln('');
						more_nomail();
						sclrscr();
					}
					break;
				case 6:
					switch(player.clss) {
						case 1:
							death_knight_level();
							break;
						case 2:
							mystical_level();
							break;
						case 3:
							thief_level();
							break;
					}
					break;
				case 7:
					/* for the longest time, this event has always given 5 charm and
					 * taken 1 charm. Doing that kinda throws off the balance of the game,
					 * since a player could get very very pretty and never have a chance
					 * of getting ugly. Randomizing it helps that a bit. */
					tmp = random(2) + 1;
					if (random(3) === 1) {
						if (!settings.new_ugly_stick) {
							tmp = 5;
						}
						sln('');
						sln('');
						lln('  `%MEGA EVENT IN THE FOREST');
						lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
						// DIFF: LADY indent was one char... changed to two 'cause that's what everything uses.
						lln('  `2You are whacked with a pretty stick by an old man!');
						sln('');
						sln('  He giggles and runs away!');
						sln('');
						lln('  `%YOU GET '+tmp+' CHARM!');
						sln('');
						player.cha += tmp;
					}
					else {
						if (!settings.new_ugly_stick) {
							tmp = 1;
						}
						sln('');
						sln('');
						lln('  `%MEGA EVENT IN THE FOREST');
						lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
						lln('  `2You are wacked with an `4ugly`2 stick by an old man!');
						sln('');
						lln('  `2He giggles and runs away!');
						sln('');
						lln('  `4YOU LOSE '+tmp+' CHARM!');
						sln('');
						player.cha -= tmp;
					}
					more_nomail();
					sclrscr();
					break;
				case 8:
inner:
					while (true) {
						tmp = player.level * 10000;
						lln('`c  `%Event In The Forest`0');
						lln('`l');
						sln('');
						lln('  `2Walking through the forest, you stumble across a clearing in the woods.');
						sln('  Looking around, you see a small man and a lot of horses. The man walks up');
						lln('  to you and asks you, "`0What can I do for you, mmhmm?`2"');
						sln('');
						lln('  `2(`0B`2)uy a horse');
						lln('  `2(`0S`2)ell your horse');
						lln('  `2(`0G`2)o back to the forest');
						sln('');
						lw('  `0Your command `2[`0GBS`2] : ');
						ich = getkey().toUpperCase();
						if ('GBS'.indexOf(ich) === -1) {
							ich = 'G';
						}
						sln(ich);
						switch (ich) {
							case 'B':
								if (player.horse) {
									sln('');
									lln('  `2"`0Now, now. You already have a horse and I cannot sell you another.`2"');
									sln('');
									more_nomail();
									break;
								}
								sln('');
								lw('  `2"`0Buy one of my horses for`% '+pretty_int(tmp)+'`0?`2" ');
								ich = getkey().toUpperCase();
								if (ich !== 'N') {
									ich = 'Y';
								}
								sln(ich);
								if (ich === 'N') {
									lln('  `2"`0Okay, mmhmm, maybe some other time.`2"');
									more_nomail();
									break;
								}
								if (player.gold < tmp) {
									lln('  `2Chuckling, the hairyfoot says to you `2"`0I am sorry, mmhmm, but I cannot');
									lln('  make a deal with someone that does not have enough money.`2"');
									sln('');
									more_nomail();
									break;
								}
								player.gold -= tmp;
								player.horse = true;
								sln('');
								lln('  `2"`0Good deal! Mmhmm, there you are.`2"');
								sln('');
								more_nomail();
								break;
							case 'S':
								if (!player.horse) {
									sln('');
									lln('  `2The happy look on the gnome\'s face goes sour. "`0I am sorry, mmhmm,');
									lln('  but I cannot make a deal with someone that does not have a horse.`2"');
									sln('');
									more_nomail();
									break;
								}
								tmp = player.level * 5000;
								sln('');
								lw('  `2"`0Sell your horse for`% '+tmp+'`0?`2" ');
								ich = getkey().toUpperCase();
								if (ich !== 'N') {
									ich = 'Y';
								}
								sln(ich);
								if (ich === 'N') {
									sln('');
									lln('  `2"`0Okay.. Maybe some other time..`2"');
									sln('');
									more_nomail();
									break;
								}
								player.gold += tmp;
								player.horse = false;
								sln('');
								lln('  `2"`0Thank you, kind person. Here is the `%'+tmp+'`0 gold I promised.`2"');
								sln('');
								break;
							case 'G':
								sln('');
								lln('  `0"Good day, kind person. May the Beast not play a deadly part in');
								lln('  your future, mmhmm."');
								sln('');
								more_nomail();
								break inner;
						}
					}
					sclrscr();
					break;
				case 9:
					lrdfile('FAIRY');
					more();
					sclrscr();
					sln('');
					sln('');
					foreground(15);
					sln('  YOU ARE NOTICED!');
					sln('');
					foreground(2);
					sln('  The small things encircle you.  A small wet female bangs your');
					lln('  shin.  `0"How dare you spy on us, human!" `2you can\'t help but');
					sln('  smile, the defiance in her silvery voice is truly a sight, you');
					sln('  think to yourself.  Further contemplation is interrupted by');
					sln('  another sharpfully painful prod.');
					sln('');
					lln('  `2(`0A`2)sk for a blessing');
					if (!player.has_fairy) {
						lln('  `2(`0T`2)ry to catch one to show your friends');
					}
					sln('');
					lw('  `0Your choice? [`2A`0] : `%');
					ich = getkey().toUpperCase();
					if (ich !== 'T') {
						ich = 'A';
					}
					if (player.has_fairy && ich === 'T') {
						ich = 'A';
					}
					sln(ich);
					if (ich === 'A') {
						sln('');
						lln('  `%"Bless me!" `2you implore the small figure.');
						sln('');
						lln('  `0"Very well." `2she agrees, `0"But we\'re still angry at you!');
						sln('');
						lw('  `0Your blessing is');
						mswait(500);
						sw('.');
						mswait(500);
						sw('.');
						mswait(500);
						sw('.');
						mswait(500);
						sw('.');
						switch(random(3+(player.has_horse ? 0 : 1))) {
							case 0:
								sw('a kiss from ');
								if (player.sex === 'M') {
									sln('Teesha!');
								}
								else {
									sln('Nolmar!');
								}
								sln('');
								more_nomail();
								foreground(2);
								sln('  A fairy near her wordlessly upstretches its arms to you.');
								sln('');
								lln('  `%THE KISS IS STRANGELY FULFILLING! `2(You\'re refreshed)');
								player.hp = player.hp_max;
								sln('');
								more_nomail();
								break;
							case 1:
								sln('an incredibly sad story!"');
								more_nomail();
								foreground(2);
								sln('  The small body beckons you to lift her.  As you bring her');
								sln('  closer, she begins to whisper.');
								sln('');
								more_nomail();
								sln('  You almost immediately begin to cry.');
								sln('');
								lln('  `%YOUR TEARS TURNS INTO GEMS AND FALL INTO YOUR HANDS!');
								sln('');
								player.gem += 2;
								if (player.gem > 32000) {
									player.gem = 32000;
								}
								more_nomail();
								break;
							case 2:
								sln('a forest melody!"');
								more_nomail();
								foreground(2);
								sln('  Immediately a small figure in the back raises her tiny reed');
								sln('  flute to her lips and begins to play.');
								sln('');
								more_nomail();
								sln('  The strange sounds send thousands of images to your mind.');
								sln('');
								foreground(15);
								sln('  YOU LEARN FAIRY LORE');
								tmp = player.exp;
								player.exp += (10 * (player.level * player.level));
								if (player.exp > 2000000000) {
									player.exp = 2000000000;
								}
								tmp = player.exp - tmp;
								if (tmp > 0) {
									lln(' - AND GET '+pretty_int(tmp)+' EXPERIENCE!');
								}
								player.fairy_lore = true;
								sln('');
								more_nomail();
								tournament_check();
								break;
							case 3:
								sln('a companion for your travels!"');
								sln('');
								more_nomail();
								foreground(2);
								if (random(2) === 1) {
									sln('  A shiny black stallion surfaces its head in the lake!');
								}
								else {
									sln('  A pure white mare nudges your back!');
								}
								sln('');
								foreground(15);
								sln('  YOU FEEL THE DAY GROW LONGER.');
								player.forest_fights = parseInt(player.forest_fights * 1.25, 10);
								player.horse = true;
								sln('');
								more_nomail();
								break;
						}
					}
					else if (!player.has_fairy) {
						sln('');
						foreground(15);
						sln('  YOU MAKE A WILD GRAB FOR THE SMALL FIGURES!');
						sln('');
						more_nomail();
						foreground(2);
						sw('  Your hand finally connects with');
						mswait(500);
						sw('.');
						mswait(500);
						sw('.');
						mswait(500);
						sw('.');
						mswait(500);
						sw('.');
						if (random(2) === 1) {
							lln('`4a thornberry bush`2!');
							sln('');
							player.hp = 1;
							more_nomail();
							lln(' `4 (HIT POINTS GO DOWN....WAAAAY DOWN)');
							sln('');
							lln('  `2The fairy leader laughs at you!  You slink away in defeat.');
							sln('');
							more_nomail();
						}
						else {
							foreground(15);
							sln('  A FAIRY!');
							sln('');
							player.has_fairy = true;
							foreground(2);
							sln('  You throw the screaming creature into your pouch.  What hidden');
							sln('  powers could it have?');
							sln('');
							sln('  You think now would be splendid time to leave.');
							sln('');
							more_nomail();
						}
					}
					break;
				case 10:
					if (settings.olivia === true) {
						lrdfile('CREEPY');
						ich = getkey();
						olivia();
					}
					break;
				case 11:
					rescue_the_princess();
					break;
				case 12:
					find_lost_gold();
					break;
				case 13:
					// Ive always wondered why a random event couldnt kill you..
					// Ive also wondered why there arent as many bad guys in the random
					// events.. Another funny thought. This must be a huge forest.. How
					// else could there be soo many monsters? :)

					// The player will lose 1 to 10 hitpoints... except thieves...
					// DIFF: This used to override and always do damage...
					if (player.clss === 3) {
						tmp = 0;
					}
					else {
						tmp = random(10) + 1;
					}
					sln('');
					sln('');
					lln('  `%MEGA EVENT IN THE FOREST');
					lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					lln('  `2An ugly troll jumps out from behind and tries to steal');
					sln('  your coin purse!');
					sln('');
					player.hp -= tmp;
					if (player.hp > 0) {
						if (player.clss === 3) {
							sln('  But being the sly theif that you are, you');
							sln('  fend him off easily!');
						}
						else {
							lln('  You fend him off, but you lose `%'+tmp+'`2 hitpoints!');
							sln('');
						}
					}
					else {
						// If the player is smart, he wont "look for something to kill" while
						// he's low on hitpoints.. If he isnt keen on the idea of protecting
						// himself, well, the troll will get him..
						player.gold = 0;
						player.gem = 0;
						player.dead = true;
						// Lord will save the player's data when he gets back from the event,
						// but some unscrupulous people will drop carrier when they see that theyve
						// been killed. So save the player data now so they cant cheat the game.
						player.put();
						sln('  You valiantly try to fend off the little troll, but at the');
						sln('  last moment, he pulls a dagger out of his boot and gives you');
						sln('  final blow!');
						sln('');
						lln('  `2The troll made off with you coin purse and gem sack!');
						sln('');
						lln('  `4You have been killed by the troll!');
						sln('');
					}
					more_nomail();
					// psst: one day, this lil bugger of a troll is going to try and make off
					// with your weapon.. your armor? nah. its hard enough to take off as it is.
					// but not hard to grab your spear of gold and take off running...
					break;
				case 14:
					break;	// TODO: Nothing.. annoy the player. :D
				case 15:
					sln('');
					sln('');
					foreground(15);
					sln('  Event In The Forest');
					lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					foreground(2);
					sln('  In the gloom of the shady forest, you see smoke coming from a bright');
					sln('  chimney.');
					sln('');
					more();
					darkhorse_tavern();
					break;
			}
			return;
		}

		if (player.level === 1) {
			mnum = random(10);
		}
		else {
			if (random(6) !== 2) {
				mnum = ((player.level-1)*11)+random(10);
			}
			else {
				mnum = (random(player.level) * 11) + random(10);
			}
		}
		enemy = load_monster(mnum);
		player.forest_fights -= 1;
		sln('');
		sln('');
		lln('  `2**`%FIGHT`2**');
		sln('');
		lln('  You have encountered '+enemy.name+'`2!!');
		sln('');
		battle(enemy, false, false);
		if (player.dead) {
			player.put();
			say(game_or_exec('normsay.lrd'), enemy, 1, '  `5'+player.name+' `2has been killed by `0'+enemy.name+'`2!', '');
			save_forest_gold(player.gold);
			player.gold = 0;
			player.exp = player.exp - parseInt(player.exp / 10, 10);
			player.on_now = false;
			player.put();
			sln('');
			dead_screen(enemy);
			sln('');
			exit(0);
		}
		if (enemy.hp < 1) {
			foreground(15);
			sln('');
			lln('  You have killed '+enemy.name+'`%!');
			sln('');
			foreground(2);
			if (enemy.gold > 0) {
				tmp = player.gold;
				player.gold += enemy.gold;
				if (player.gold > 2000000000) {
					player.gold = 2000000000;
				}
				if (tmp === player.gold) {
					sln('  You don\'t find any gold, but you do get');
				}
				else {
					lw('  `2You receive `0'+pretty_int(player.gold - tmp)+' `2gold, and');
				}
			}
			else {
				sln('  You don\'t find any gold, but you do get');
			}
			lln('`0 '+pretty_int(enemy.exp)+'`2 experience!');
			player.exp += enemy.exp;
			if (player.exp > 2000000000) {
				player.exp = 2000000000;
				foreground(10);
				sln('Wow, you have a LOT of experience!');
			}
			sln('');
			more();
			tournament_check();
		}
	}

	function forest_special() {
		var g;

		lln('`c`%                        ** WIERD EVENT **');
		lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		lln('  `0You are heading into the forest, when you hear the voice of');
		sln('  angels singing.');
		lln('');
		more_nomail();
		sln('  You follow the sound for some time - when you are about to give');
		sln('  up...');
		lln('');
		more_nomail();
		lw('  `2You find.');
		mswait(40);
		sw('.');
		mswait(40);
		sw('.');
		g = random(player.level) + 1;
		sln(g > 1 ? g + ' Gems!' : '1 Gem!');
		sln('');
		player.gem += g;
		player.weird = false;
		more_nomail();
	}

	function attack_dragon() {
		function dragon_screen() {
			lrdfile('LAIRANS');
		}

		if (player.seen_dragon) {
			sln('');
			sln('');
			foreground(2);
			sln('  You are shaking so badly from your previous encounter,');
			sln('  you deem it wise to wait and gather your strength!');
			sln('');
			more();
			return;
		}
		dragon_screen();
		do {
			do {
				command_prompt();
				ch = getkey().toUpperCase();
				sw(ch);
			} while ('?AR'.indexOf(ch) === -1);
			sln(ch);
			switch(ch) {
				case '?':
					dragon_screen();
					break;
				case 'A':
					fight_dragon(false);
					break;
				case 'R':
					sln('');
					foreground(2);
					sln('  You decide it would be wise to depart from this wicked place.');
					sln('');
					more();
					return;
			}
		} while(!player.seen_dragon);
	}

	function menu() {
		if (player.weird) {
			forest_special();
		}
		if (!player.expert) {
			lrdfile('FOREST');
			foreground(2);
			sln('');
			lln('  `2(`0L`2)ook for something to kill       (`0H`2)ealer\'s Hut');
			lw('  `2(`0R`2)eturn to town                   ');
			if (player.horse) {
				lw('`2(`0T`2)ake ');
				sln('Horse To DarkCloak Tavern');
			}
			else {
				sln('');
			}
		}
	}
	
	function prompt() {
		sln('');
		lw('  `2HitPoints: (`0'+pretty_int(player.hp)+'`2 of `0'+pretty_int(player.hp_max)+'`2)');
		lw('  Fights: `0'+pretty_int(player.forest_fights)+'`2 Gold: `0'+pretty_int(player.gold));
		lw('  `2Gems: `0'+pretty_int(player.gem));
		sln('');
		lw('  `5The Forest');
		foreground(2);
		sw('   (L,H,R,');
		if (player.horse) {
			sw('T,');
		}
		sln('Q)  (? for menu)');
	}

	sclrscr();
	menu();
	do {
		prompt();
		command_prompt();
		ch = getkey().toUpperCase();
		if (ch !== 'J') {
			sln(ch);
		}
		switch(ch) {
			case 'B':
				if (player.gold > 0) {
					sln('');
					sln('');
					sln('  You throw your gold pouch up into the air gleefully.');
					sln('');
					lln('  `0AN UGLY VULTURE `)GRABS `0IT IN MID AIR!');
					player.bank += player.gold;
					if (player.bank > 2000000000) {
						player.bank = 2000000000;
					}
					player.gold = 0;
				}
				break;
			case 'R':
			case 'Q':
			case '\r':
				over = true;
				break;
			// DIFF: Removed explicit space handling...
			case 'V':
				show_stats();
				break;
			case 'L':
				if (player.forest_fights < 1) {
					sln('');
					sln('');
					sln('  You are too tired.');
					sln('');
					sln('  Try again tomorrow.');
					sln('');
					more();
				}
				else {
					look_to_kill();
					if (player.dead) {
						over = true;
					}
				}
				break;
			case 'T':
				if (player.horse) {
					sln('');
					lln('  `2You nudge your horse deeper into the woods.');
					sln('');
					more_nomail();
					sln('');
					sln('');
					foreground(15);
					sln('  Event In The Forest');
					lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					foreground(2);
					sln('  In the gloom of the shady forest, you see smoke coming from a bright');
					sln('  chimney.');
					sln('');
					more();
					darkhorse_tavern();
					sclrscr();
					menu();
					break;
				}
				sln('');
				sln('');
				sln('  Your Thieving skills cannot help you here.');
				sln('');
				break;
			case 'M':
				sln('');
				sln('');
				sln('  Your Mystical skills cannot help you here.');
				sln('');
				break;
			case 'D':
				sln('');
				sln('');
				sln('  Your Death Knight skills cannot help you here.');
				sln('');
				break;
			case 'A':
				sln('');
				sln('');
				sln('  You brandish your weapon dramatically.');
				sln('');
				break;
			case 'H':
				healers();
				break;
			case '?':
				if (player.expert) {
					lrdfile('FOREST');
				}
				menu();
				break;
			case 'S':
				if (player.level < 12) {
					show_stats();
				}
				else {
					attack_dragon();
					if (player.dead) {
						over = true;
						break;
					}
				}
				sclrscr();
				menu();
				break;
			case 'J':
				if (player.high_spirits) {
					player.high_spirits = false;
					if (getkey().toUpperCase() === 'E' &&
					    getkey().toUpperCase() === 'N' &&
					    getkey().toUpperCase() === 'N' &&
					    getkey().toUpperCase() === 'I' &&
					    getkey().toUpperCase() === 'E') {
						sln('');
						sln('');
						lln('  `0Jennie?  Jennie Garth?');
						lw('  `2Define her. ');
						ch = clean_str(getstr(0,0,4,1,15,'').toUpperCase());
						curlinenum = 1;
						sln('');
						sln('');
						switch (ch) {
							case 'BABE':
								lln('  `0That is correct. `2(YOU RECIEVE AN EXTRA FOREST FIGHT!)');
								player.forest_fights += 1;
								if (player.forest_fights > 32000) {
									player.forest_fights = 32000;
								}
								break;
							case 'SEXY':
								lln('  `0Exellent. `2(YOU RECIEVE AN EXTRA USER FIGHT!)');
								player.pvp_fights += 1;
								if (player.pvp_fights > 32000) {
									player.pvp_fights = 32000;
								}
								break;
							case 'LADY':
								lln('  `0Very true.  `2(YOU GET SOME GOLD!)');
								player.gold += 1000 * player.level;
								if (player.gold > 2000000000) {
									player.gold = 2000000000;
								}
								break;
							case 'DUMB':
								lln('  `0You idiot.  You will `)never`0 be a useful member of society.');
								break;
							case 'STAR':
								lln('  `0A huge star, infant.');
								lln('  `4(YOU GET NOTHING, YOU STATED THE OBVIOUS)');
								break;
							case 'DUNG':
								if (player.sex === 'M') {
									lln('  `0You are a fool, sir.  `4(YOU ARE TURNED INTO A FROG)');
								}
								else {
									lln('  `0You are a fool, woman.  `4(YOU ARE TURNED INTO A FROG)');
								}
								lln('');
								more_nomail();
								do {
									lln('  `c`2The Forest Floor');
									sln('');
									lln('  `2(`0H`2)op Like Crazy');
									lln('  `2(`0A`2)pologize');
									sln('');
									lw('  `2Your command, greeny? : `%');
									ch = getkey().toUpperCase();
									if (ch !== 'A') {
										ch = 'H';
									}
									sln(ch);
									if (ch !== 'A') {
										foreground(2);
										sln('  You hop around like a crazy frog.  What');
										sln('  is this accomplishing, pray tell?');
										sln('');
										more_nomail();
									}
								} while (ch !== 'A');
								sln('');
								lln('  `2You apologize humbly, knowing what you did was wrong.');
								lln('');
								lln('  `%(YOU ARE CHANGED BACK TO YOUR (MOSTLY) HUMAN FORM)');
								break;
							case 'FOXY':
								lln('  `0Very wise. `%(YOU RECIEVE AN EXTRA GEM!)');
								player.gem += 1;
								if (player.gem > 32000) {
									player.gem = 32000;
								}
								break;
							// TODO: Toggle for old (opposite) behaviour
							case 'FAIR':
								lln('  `0Very fair. `2(YOU FEEL EXCITED!)');
								player.flirted = false;
								break;
							case 'UGLY':
								player.on_now = false;
								player.put();
								lln('  `0You understand nothing.  `4(YOU ARE BITCH SLAPPED!)');
								player.hp = 1;
								sln('');
								more_nomail();
								player.on_now = false;
								exit(0);
								break;	// For syncjslint...
							case 'HOTT':
								lln('  `0"Hot" is spelled with only one T.. But good job, nonetheless.');
								sln('');
								player.hp = parseInt(player.hp_max + player.hp_max / 5, 10);
								if (player.hp > 32000) {
									player.hp = 32000;
								}
								lln('  `%(YOU FEEL ENERGIZED!)');
								break;
							case 'COOL':
								lln('  `0Why, you are cool to notice that.');
								sln('');
								if (player.hp < player.hp_max) {
									lln('  `%GOD NOTICES YOU ARE WOUNDED AND PITIES YOU.  YOU LOOK BETTER!');
									player.cha += 1;
									if (player.cha > 32000) {
										player.cha = 32000;
									}
								}
								break;
							// TODO: Option to turn off
							case 'GIFT':
								lln('  `0Yes, she is this.  And now, a magical gift for you.');
								sln('');
								switch(player.clss) {
									case 2:
										if (player.skillm < 1 || player.magically_delicious) {
											lln('  `%You are unable to accept the gift.');
										}
										else {
											lln('  `5YOU FEEL MAGICALLY DELICIOUS.');
											player.levelm = player.skillm;
											player.magically_delicious = true;
										}
										break;
									case 1:
										if (player.skillw < 1 || player.magically_delicious) {
											lln('  `%You are unable to accept the gift.');
										}
										else {
											lln('  `5YOU FEEL MAGICALLY DELICIOUS.');
											player.levelw = player.skillw;
											player.magically_delicious = true;
										}
										break;
									case 3:
										if (player.skillt < 1 || player.magically_delicious) {
											lln('  `%You are unable to accept the gift.');
										}
										else {
											lln('  `5YOU FEEL MAGICALLY DELICIOUS.');
											player.levelt = player.skillt;
											player.magically_delicious = true;
										}
										break;
								}
								break;
							// TODO 'NICE' (Duh (nothing special) only in versions prior to 4)
							default:
								if (player.sex === 'M') {
									sln('  You do not understand her, my son.');
								}
								else {
									sln('  Perhaps if you were male you might understand better.');
								}
						}
						sln('');
						more_nomail();
					}
				}
				break;
		}
	} while (!over);
}

function turgons()
{
	var trainer;
	var ch;

	function ask(trainer) {
		var mc = morechk;

		sclrscr();
		sln('');
		lln('  `%Questioning Your Master`2');
		sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(10);
		morechk = false;
		lrdfile('LEVEL'+player.level+(player.sex === 'M' ? 'MALE' : 'FEMALE'), true);
		sln('');
		if (player.exp > trainer.need) {
			lln('  `0'+trainer.name+' `2looks at your carefully and says: ');
			sln('');
			if (trainer.needstr1 !== '') {
				lln('  `2"`0'+trainer.needstr1.replace(/\&PWE/i, player.weapon)+'`2"');
			}
			if (trainer.needstr2 !== '') {
				lln('  `2"`0'+trainer.needstr2+'`2"');
			}
		}
		else {
			sln('');
			lln('  `0'+trainer.name+'`2 looks at you carefully.');
			sln('');
			lln('  `2"`0You need about `%'+pretty_int(trainer.need-player.exp)+'`0 more experience before');
			lln('   you will be as good as I am.`2"');
		}
		sln('');
		morechk = mc;
		more();
	}

	function rank_king(fname) {
			var i;
			var pl = [];
			var f = new File(fname);

			all_players().forEach(function(op) {
				if (op.name !== 'X' && op.drag_kills > 0) {
					pl.push(op);
				}
			});
			pl.sort(function(a,b) {
				if (a.drag_kills !== b.drag_kills) {
					return b.drag_kills - a.drag_kills;
				}
				if (a.level !== b.level) {
					return b.level - a.level;
				}
				return b.exp - a.exp;
			});
			if (!f.open('w')) {
				throw('Unable to open '+f.name);
			}
			f.writeln('');
			f.writeln('');
			f.writeln('                           `%Heroes Of The Realm');
			f.writeln('');
			f.writeln('  `0Name                       Heroic Deeds Done         Current Level');
			f.writeln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			for (i=0; i<pl.length; i += 1) {
				f.writeln('`0  '+space_pad(pl[i].name, 22)+'           `%'+format("%3d",pl[i].drag_kills)+'                       `0'+format("%2d",pl[i].level));
			}
			if (pl.length === 0) {
				f.writeln('  `0Sad times indeed, there are no heroes in this realm.');
			}
			f.close();
	}

	function attack_master(trainer) {
		var son;
		var mline;

		sclrscr();
		sln('');
		sln('');
		lln('  `%Fighting Your Master');
		foreground(2);
		sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		foreground(10);
		if (player.seen_master) {
			son = (player.sex === 'M' ? 'son' : 'daughter');
			sln('  "I would like to battle again, but it is too late my '+son+'."');
			lln('  `2'+trainer.name+' tells you.  You figure you will try again');
			sln('  tomorrow.');
			sln('');
			more_nomail();
			return;
		}
		if (player.exp < trainer.need) {
			sln('  You are escorted down the hallway and into the battle arena.');
			sln('');
			foreground(10);
			sln('  THE BATTLE BEGINS!');
			sln('');
			more_nomail();
			lln('  `2You raise your `0'+player.weapon+' `2to strike!  You wonder why everyone');
			sln('  is looking at you with grins on their faces...');
			sln('');
			more_nomail();
			sln('  Your weapon is gone!  You are holding air!');
			sln('');
			more_nomail();
			sln('  Your Master is holding it!  The entire crowd is laughing at you!');
			sln('');
			sln('  You meekly accept the fact that you are not ready for your testing.');
			sln('');
			more_nomail();
			player.seen_master = true;
			return;
		}
		lln('`2  You enter the fighting arena, ready with your `0'+player.weapon +'`2.');
		sln('');
		sln('  When your name is called, you move to the proper position and take a');
		sln('  fighting stance against your master.');
		player.seen_master = true;
		sln('');
		sln('');
		lln('  `2**`%MASTER FIGHT`2**');
		sln('');
		lln('  You have encountered '+trainer.name+'`2!!');
		sln('');
		// DIFF: Added this pause when the battle prompt would pause.
		if (curlinenum + battle_prompt_lines() > (dk.console.rows - 1)) {
			more_nomail();
		}
		battle(trainer, false, false);
		if (player.dead) {
			player.dead = false;
			player.hp = player.hp_max;
			sln('');
			lln('  `0'+trainer.name+' `2raises his '+trainer.weapon+'`2 to kill you!');
			sln('');
			more_nomail();
			sln('  At the last minute, he reaches down and helps you up.  He tells you not to');
			sln('  be discouraged, and for good gesture has you healed before you go.');
			sln('');
			more();
		}
		else if (trainer.hp < 1) {
			player.dead = false;
			sln('');
			lln('  `%You have bested '+trainer.name+'`%!');
			sln('');
			lln('`%  '+trainer.swear);
			sln('');
			player.hp_max += trainer.hp_gained;
			player.hp = player.hp_max;
			player.str += trainer.str;
			player.def += trainer.def;
			player.level += 1;
			lw('`2  You receive `0'+pretty_int(trainer.hp_gained)+'`2 hitpoints,');
			lw(' `0'+pretty_int(trainer.str)+'`2 strength');
			lln(' and `0'+pretty_int(trainer.def)+'`2 defense points!');
			sln('');
			lln('  `%YOU ARE NOW LEVEL '+pretty_int(player.level)+'.');
			sln('');
			mline = '  `0'+player.name+' `2has beaten `%'+trainer.name+'!';
			if (player.level === 12) {
				mline += '\n';
				if (player.sex === 'M') {
					mline += '  He ';
				}
				else {
					mline += '  She ';
				}
				mline += 'has become the Ultimate Warrior!';
			}
			log_line(mline);
			raise_class();
			player.seen_master = false;
			tournament_check();
		}
	}

	function menu() {
		sclrscr();
		lrdfile('TURGON');
	}

	function prompt() {
		if (player.level < 12) {
			trainer = get_trainer(player.level);
		}
		else {
			trainer = get_trainer(11);
		}
		lln('  `3`2Your master is `%'+trainer.name+'`2.');
		sln('');
		lln('  `5Turgon\'s Warrior Training`2');

		if (player.level > 11) {
			sln('');
			sln('  You pay your respects to Turgon, and stroll around the grounds.  Lesser');
			sln('  warriors bow low as you pass.  Turgon\'s last words advise you to find and');
			lln('  kill the `4Red Dragon`2..');
			sln('');
			more();
			return true;
		}
		lln('`2  (Q,A,V,R)  (`0? for menu`2)');
		return false;
	}

	menu();
	do {
		if (prompt()) {
			return;
		}

		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch(ch) {
			case 'Q':
				ask(trainer);
				break;
			case 'R':
			case '\r':
				break;
			case 'A':
				attack_master(trainer);
				break;
			case '?':
				menu();
				break;
			// DIFF: Removed space which just skipped prompt.
			case 'V':
				rank_king(gamedir('temp'+player.Record));
				show_lord_file(gamedir('temp'+player.Record), false, false);
				file_remove(gamedir('temp'+player.Record));
				sln('');
				more_nomail();
				foreground(2);
				break;
		}
	} while ('R\r'.indexOf(ch) === -1);
}

function ye_old_bank()
{
	var ch;
	var amt;
	var to;
	var op;
	var mline;
	var class_list = ['stranger', 'warrior', 'magician', 'thief'];

	function menu() {
		sclrscr();
		lrdfile('BANK');
	}

	function prompt() {
		sln('');
		lw('  `2Gold In Hand: `0'+pretty_int(player.gold));
		lln('  `2Gold In Bank: `0'+pretty_int(player.bank));
		lln('  `5The Bank `8(W,D,R,T,Q)  (? for menu)');
	}

	menu();
	prompt();
	do {
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch(ch) {
			case 'W':
				lln('`c  `%Ye Olde Bank`#');
				sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				lln('  `2Gold In Hand: `0'+pretty_int(player.gold)+'  `2Gold In Bank: `0'+pretty_int(player.bank));
				sln('');
				lln('  `2"How much gold would you like to withdraw?" `0(1 for ALL of it)');
				sln('');
				lw('  `0AMOUNT : ');
				amt = parseInt(dk.console.getstr({len:11, integer:true}), 10);
				if (isNaN(amt)) {
					amt = 0;
				}
				if (amt === 1) {
					amt = player.bank;
				}
				if (player.gold + amt > 2000000000) {
					amt = 2000000000 - player.gold;
				}
				if (amt === 0) {
					sln('');
					sln('  "Okay. Maybe another time."');
				}
				else if (amt < 0) {
					sln('');
					sln('  "Uh...Wouldn\'t that be depositing?!"');
				}
				else if (amt > player.bank) {
					sln('');
					sln('  "I\'m afraid you don\'t have that much in your account, ');
					sln(player.sex === 'M' ? 'sir."' : 'ma\'am."');
				}
				else {
					sln('');
					sln('  Done! '+pretty_int(amt)+' withdrawn.');
					player.bank -= amt;
					player.gold += amt;
				}
				break;
			case 'D':
				lln('`c  `%Ye Olde Bank`#');
				sln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				lln('  `2Gold In Hand: `0'+pretty_int(player.gold)+'   `2Gold In Bank: `0'+pretty_int(player.bank));
				sln('');
				lln('  `2"How much gold would you like to deposit?" `0(1 for ALL of it)');
				sln('');
				lw('  `0AMOUNT: ');
				amt = parseInt(dk.console.getstr({len:11, integer:true}), 10);
				if (isNaN(amt)) {
					amt = 0;
				}
				if (amt === 1) {
					amt = player.gold;
				}
				if (player.bank + amt > 2000000000) {
					amt = 2000000000 - player.bank;
				}
				if (amt === 0) {
					sln('');
					sln('  "Okay. Maybe another time."');
				}
				else if (amt < 0) {
					sln('');
					lln('  "Uh...Wouldn\'t that be withdrawing?!"');
				}
				else if (amt > player.gold) {
					sln('');
					sln('  "I\'m afraid you don\'t have that much on you, ');
					sln(player.sex === 'M' ? 'sir."' : 'ma\'am."');
				}
				else if (player.bank >= 2000000000) {
					player.bank = 2000000000;
					sln('');
					sln('  "I\'m sorry, but we can only keep 2,000,000,000 gold at a time. ');
				}
				else {
					sln('');
					sln('  Done! '+pretty_int(amt)+' deposited.');
					player.gold -= amt;
					player.bank += amt;
				}
				break;
			case 'T':
				if (settings.transfers_on) {
					if (settings.transfers_per_day > 0 && player.transferred_gold >= settings.transfers_per_day) {
						lln('  `0"Very sorry, but all couriers are busy.');
						sln('');
						break;
					}
					lln('`c  `%Ye Olde Bank`#');
					lln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					lw('  `2Gold In Hand: `0'+pretty_int(player.gold));
					lln('  `2Gold In Bank: `0'+pretty_int(player.bank));
					foreground(2);
					sln('  "How much gold would you like to transfer?"');
					lw('  `0AMOUNT : ');
					amt = parseInt(dk.console.getstr({len:11, integer:true}), 10);
					if (isNaN(amt)) {
						amt = 0;
					}
					if (amt === 0) {
						sln('');
						sln('  "Okay. Maybe some other time."');
					}
					else if(amt < 0) {
						sln('');
						sln('  "Uh...Wouldn\'t that be taking money from his account?!"');
					}
					else if(amt > player.bank) {
						sln('');
						sln('  "I\'m afraid you don\'t have that much in your account, ');
						sln(player.sex === 'M' ? 'sir."' : 'woman."');
					}
					else if(amt > settings.transfer_amount) {
						sln('');
						sln('  "Our messenger will not carry more than '+pretty_int(settings.transfer_amount)+' gold pieces at a time!"');
					}
					else {
						sln('');
						sln('  "And who would you like to send this gold to?"');
						to = find_player();
						if (to === -1 || to === player.Record) {
							sln('');
							sln('  Gold Not Sent!');
							break;
						}
						op = player_get(to);
						player.bank -= amt;
						player.transferred_gold += 1;
						player.put();
						sln('');
						lln('  Done!  '+op.name+' will be notified!');
						mline = ' \n  `%BANK NOTICE:\n`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-`2\n  `0'+player.name+' `2has transferred `%'+amt+' `2gold\n  to your account.\n`b'+amt;
						mail_to(to, mline);
					}
				}
				else {
					sln('');
					sln('');
					lln('  `0I\'m sorry, but all of our carriers are on permanent vacation.');
					sln('');
				}
				break;
			case 'R':
			case 'Q':
			case '\r':
				break;
			case '?':
				menu();
				prompt();
				break;
			case '1':
				sln('');
				sln('');
				sln('  Nice rock.');
				sln('');
				break;
			case '4':
				sln('');
				sln('');
				lln('  `2Look wise guy, these are `)NOT `2secret keys.  They say these things');
				sln('  when you click on pictures in RIP.  Nothing more. (Then again...)');
				sln('');
				break;
			case '2':
				sln('');
				sln('');
				if (player.clss === 3) {
					if (player.has_fairy) {
						lln('  `0** `%TRICKY EVENT! `0**`2');
						sln('');
						sln('  The buzzing in your pocket reminds you');
						sln('  that the little fairy can feel your emotions.');
						sln('');
						more_nomail();
						lln('  `%IT ESCAPES FROM YOUR POCKET AND PICKS THE LOCK!');
						sln('');
						more_nomail();
						if (settings.old_steal) {
							amt = ((random(500) + 500) * player.level * player.level * player.level);
						}
						else {
							amt = ((random(500) + 500) * player.level);
						}
						player.has_fairy = false;
						lln('  `2You steal `0'+pretty_int(amt)+' `2while the banker isn\'t looking.');
						player.gold += amt;
						sln('');
						more_nomail();
						prompt();
						break;
					}
					sln('  You\'re a thief, find the key.');
				}
				else {
					sln('  Steal?  Never!');
				}
				sln('');
				break;
			case '3':
				sln('');
				sln('');
				sln('  Maybe there is a Genie in there.');
				sln('');
				break;
		}
	} while ('RQ\r'.indexOf(ch) === -1);
	if (player.leftbank === false) {
		player.leftbank = true;
		if (random(30) === 25) {
			if (!player.amulet) {
				to = class_list[player.clss];
				amt = player.level * 1000;
				sclrscr();
				lln('  `%A random event!');
				lln('`#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				sln('');
				lln('  `2While leaving the bank, a stranger approaches you..');
				sln('');
				lln('  `2"`0Hello there '+to+'. My name is Aidan. I have something that might');
				sln('  interest you. It\'s just a little necklace I found in the forest, but');
				lln('  the old man in the Dark Cloak Tavern called it an \'`%Amulet of Accuracy`2\'');
				lln('  I have no use for it. I\'ll sell it to you - only '+amt+' gold`2"');
				sln('');
				sln('');
				lw('  `2Buy the Amulet? `2[`0N`2] : ');
				ch = getkey().toUpperCase();
				if (ch !== 'Y') {
					ch = 'N';
				}
				sln(ch);
				if (ch === 'Y') {
					if (player.gold < amt) {
						lln('  `2"`0I\'m sorry, but it seems you dont have enough gold to make');
						lln('  the trade. Maybe some other time.`2"');
					}
					else if (player.amulet) {
						lln('  `2"`0I\'m sorry, but you already have an amulet. I cannot sell');
						lln('  you another.`2"');
					}
					else {
						lln('  `2"`0Thank you. I believe you\'ll enjoy that little amulet.');
						lln('  It may come in handy in battle.`2"');
						player.gold -= amt;
						player.amulet = true;
					}
				}
			}
		}
	}
}

function conjugality_list()
{
	var mt;
	var some = false;
	var l = 0;
	var phrase = ['hitched with','attached to','in love with','a love slave to',
	    'smitten with love for','in matrimony with','in marital bliss with','wedded to'];

	lln('`c                     `%** CONJUGALITY LIST **');
	lln('                    `2-=-=-=-=-=-=-=-=-=-=-=-=-');
	player.put();
	sln('');
	get_state(false);
	all_players().forEach(function(op, i) {
		// DIFF: This check if name length was over 2 chars...
		if (op.name !== 'X') {
			if (op.married_to > -1 && i < op.married_to) {
				mt = player_get(op.married_to);
				some = true;
				lln('  `0'+op.name+' `2is '+phrase[l]+' `0'+mt.name+'`2.');
				l += 1;
				if (l >= phrase.length) {
					l = 0;
				}
			}
			if(i === state.married_to_seth) {
				some = true;
				lln('  `0'+op.name+' `2is the property of `0Seth Able`2.');
			}
			if(i === state.married_to_violet) {
				some = true;
				lln('  `0'+op.name+' `2belongs to `0Violet`2.');
			}
		}
	});
	if (!some) {
		lln('  `%No one is married in this realm.');
	}
	sln('');
	more_nomail();
}

function show_game_stats()
{
	var tmp;

	get_state(false);
	lln('`c                     `%** GAME STATISTICS '+ver+' **');
	sln('');
	lln(' `2 This game has been running for `%'+pretty_int(state.days)+'`2 days.');
	lln('  `2You are playing a `0J`2A`0V`2A`0S`2C`0R`2I`0P`2T game.');
	if (settings.win_deeds > 0) {
		lln('  `2Game can be `0FINISHED `2by getting '+pretty_int(settings.win_deeds)+' heroic deeds');
	}
	else {
		lln('  `2Game is set to run indefinitely.');
	}
	sln('');
	tmp = 0;
	all_players().forEach(function(op) {
		if (op.name !== 'X') {
			tmp += 1;
		}
	});
	lln('  `2There are currently `%'+pretty_int(tmp)+' `2people playing.');
	sln('');
	lln('  `2Players are deleted after `%'+pretty_int(settings.delete_days)+' `2days of inactivity.');
	sln('');
	if (settings.clean_mode) {
		lln('  `2Clean mode is `0ENABLED`2.');
	}
	else {
		lln('  `2Clean mode is OFF!  (Thank the stars!)');
	}
	sln('');
	if (settings.transfers_on) {
		lln('  `2Transferring Funds is enabled, max of `%'+pretty_int(settings.transfer_amount)+'`2 per transfer.');
		if (settings.transfers_per_day > 0) {
			lln('  `2You can do this up to `%'+pretty_int(settings.transfers_per_day)+'`2 times a day.');
		}
		else {
			lln('  `2You can do this `%unlimited`2 times per day.');
		}
	}
	else {
		lln('  `2Bank option Transferring Funds is disabled by Sysop');
	}
	sln('');
	lln('  `2Need an ego boost?  This is what you overhear being said about yourself.');
	if (player.sex === 'M') {
		lln('  `0"He '+looks(player.sex, player.cha)+'"');
	}
	else {
		lln('  `0"She '+looks(player.sex, player.cha)+'"');
	}
	sln('');
	lln('  `0Current user tasker: '+system.os_version+' and you\'re on node '+pretty_int(dk.connection.node));
	lw('`2  ');
	if (!settings.del_1xp) {
		lw('NK1 ');			// No Kill 1XP
	}
	if (settings.res_days !== 3) {
		lw('R'+settings.res_days+' ');	// Resurection Days
	}
	if (!settings.olivia) {
		lw('NO ');			// No Olivia
	}
	if (!settings.funky_flowers) {
		lw('BF ');			// Boring Flowers
	}
	if (!settings.shop_limit) {
		lw('NL ');			// No Shop Limit
	}
	if (settings.old_skill_points) {
		lw('5S ');			// 5 skill points per use
	}
	if (settings.def_for_pk) {
		lw('DP ');			// Def for pk
	}
	if (settings.str_for_pk) {
		lw('SP ');			// Str for pk
	}
	if (settings.beef_up) {
		lw('BEEF ');			// Beef up monsters
	}
	if (!settings.old_steal) {
		lw('CT ');			// Crappy Thieves
	}
	if (settings.dk_boost) {
		lw('DK ');			// Great Death Knights
	}
	if (settings.sleep_dragon) {
		lw('SD ');			// Anti-camping Dragon attacks
	}
	sln('');
	more_nomail();
}

function list_players()
{
	var f = gamedir('temp'+player.Record);

	sln('');
	generate_rankings(f, true, true, false);
	display_file(f, true, true);
	file_remove(f);
}

function slaughter_others()
{
	var ch;
	var tk;
	var line;
	var lines;
	var dirt = new File(gamedir('dirt.lrd'));
	var atk;
	var enemy;
	var out;
	var where;
	var lmat;

	function top_killer() {
		var ret = {pvp:-1};

		all_players().forEach(function(pl) {
			if (pl.name !== 'X' && pl.pvp > ret.pvp) {
				ret = pl;
			}
		});
		return ret;
	}

	sclrscr();
	if (!player.expert) {
		lrdfile('WAR');
	}
	do {
		sln('');
		foreground(5);
		sln('  Slaughter Other Players');
		foreground(2);
		sln('');
		sln('  (S,L,E,W,R)  (? for menu)');
		check_mail();
		command_prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch(ch) {
			case 'L':
				generate_rankings(gamedir('temp'+player.Record), false, true, false);
				display_file(gamedir('temp'+player.Record), true, true);
				file_remove(gamedir('temp'+player.Record));
				break;
			case 'R':
			case 'Q':
			case '\r':
				return;
			case '?':
				lrdfile('WAR');
				break;
			case 'V':
				show_stats();
				break;
			case 'S':
				sln('  Who would you like to attack?');
				atk = find_player();
				if (atk === -1) {
					sln('  No warriors found.');
				}
				else if (atk === player.Record) {
					sln('  You wish to attack yourself?!!  You decide against it.'); }
				else {
					enemy = player_get(atk);
					if (psock !== undefined) {
						out = psock_igm_lines(enemy.Record);
					}
					else {
						out = new File(gamedir('out'+enemy.Record+'.lrd'));
						fmutex(out.name);
					}
					if ((psock !== undefined && out[0].length > 0) || (pfile !== undefined && file_exists(out.name))) {
						if (pfile !== undefined) {
							if (!out.open('r')) {
								throw('Unable to open '+out.name);
							}
							where = out.readln();
							rfmutex(out.name);
						}
						else {
							where = out[0];
						}
						sln('');
						sln('  That warrior is currently visiting another section of the realm.');
						lln('  `2('+enemy.name+' '+where+'`2)');
						sln('');
						more_nomail();
					}
					else if (enemy.dead) {
						if (pfile !== undefined) {
							rfmutex(out.name);
						}
						sw('  You look for that warrior...And you find ');
						sln(enemy.sex === 'M' ? 'him...' : 'her...');
						sln('  A rotting corpse...Looks like you were a little late..');
					}
					else if (enemy.inn) {
						if (pfile !== undefined) {
							rfmutex(out.name);
						}
						sln('  You search the fields but do not find that warrior.');
						sw('  You conclude ');
						if (enemy.sex === 'F') {
							sw('s');
						}
						sln('he is staying at the Inn.');
					}
					else {
						if (pfile !== undefined) {
							rfmutex(out.name);
						}
						player.put();
						lw('  `2You hunt around for `0'+enemy.name+'`2...');
						if (enemy.sex === 'M') {
							sln('YOU FIND HIM!');
							sw('  He');
						}
						else {
							sln('YOU FIND HER!');
							sw('  She');
						}
						lln(' is brandishing a dangerous looking '+enemy.weapon+'.');
						sln('');
						lw('  `2Attack `5'+enemy.name+' `2[`0Y`2] : `%');
						ch = getkey().toUpperCase();
						if (ch !== 'N') {
							ch = 'Y';
						}
						sln(ch);
						foreground(2);
						if (ch === 'Y') {
							attack_player(enemy, false);
						}
						if (player.dead) {
							return;
						}
					}
				}
				break;
			case 'E':
				lln('`c`%                         ** EXAMINING THE DIRT **');
				sln('');
				tk = top_killer();
				lln('  `2The town elders have named `0'+tk.name+' `2the most dangerous');
				lln('  `2warrior in the realm with a whopping `%'+pretty_int(tk.pvp)+'`2 kills.');
				sln('');
				if (psock === undefined) {
					if (file_exists(dirt.name)) {
						fmutex(dirt.name);
						show_lord_file(dirt.name, false, false);
						rfmutex(dirt.name);
					}
					else {
						lln('  `2The dirt appears soft and malleable.');
					}
				}
				else {
					psock.writeln('GetConversation dirt');
					line = psock.readln();
					lmat = line.match(/^Conversation ([0-9]+)$/);
					if (lmat === null) {
						throw('Invalid response to GetConversation dirt');
					}
					lines = psock.read(parseInt(lmat[1], 10));
					if (psock.read(2) !== '\r\n')  {
						throw('Out of sync with server after GetConversation dirt');
					}
					if (lines.length === 0) {
						lln('  `2The dirt appears soft and malleable.');
					}
					else {
						show_lord_buffer(lines, false, false);
					}
				}
				break;
			case 'W':
				lln('`c `%                        ** WRITING IN THE DIRT **');
				sln('');
				if ((!player.killedaplayer) && player.name !== state.latesthero) {
					lln('  `2You are about to inscribe something, when you hear `0'+state.latesthero+'\'`2s');
					sln('  voice in your head!');
					sln('');
					lln('  `0"My son, you have not challenged and killed another warrior this day."');
					sln('');
					lln('  `2You lower your eyes in homage to your better.');
					sln('');
					lln('  `0"You may share wisdom when you have killed, and have it."');
					lln('');
					more();
				}
				else {
					if (player.name === state.lastesthero) {
						lln('  `2The crowd parts in awe, as you approach the dirt patch.  You smile, maybe');
						sln('  slaying that Dragon did more for your reputation than you know.');
						sln('');
					}
					lln('  `2You pick up a nearby stick and inscribe some of your wisdom.');
					sln('');
					lines = [];
					// DIFF: Previously, no max length on dirt...
					do {
						lw('`. `0>`%');
						line = '  '+clean_str(dk.console.getstr());
						lines.push(line);
					} while (line !== '  ' && lines.length < 20);
					if (psock === undefined) {
						fmutex(dirt.name);
						dirt.open('w');
						dirt.writeAll(lines);
						dirt.close();
						rfmutex(dirt.name);
					}
					else {
						lines = lines.join('\n');
						psock.write('AddToConversation dirt '+lines.length+'\r\n'+lines);
						if (psock.readln() !== 'OK') {
							throw('Out of sync with server after AddToConversation dirt');
						}
					}
				}
				break;
		}
	} while(true);
}

function check_fields()
{
	if (player.hp < 0) {
		player.hp = 0;
	}
	if (player.hp > 32000) {
		player.hp = 32000;
	}
	if (player.hp_max < 0) {
		player.hp_max = 0;
	}
	if (player.hp_max > 32000) {
		player.hp_max = 32000;
	}
	if (player.forest_fights < 0) {
		player.forest_fights = 0;
	}
	if (player.forest_fights > 32000) {
		player.forest_fights = 32000;
	}
	if (player.pvp_fights < 0) {
		player.pvp_fights = 0;
	}
	if (player.pvp_fights > 32000) {
		player.pvp_fights = 32000;
	}
	if (player.gold < 0) {
		player.gold = 0;
	}
	if (player.gold > 2000000000) {
		player.gold = 2000000000;
	}
	if (player.bank < 0) {
		player.bank = 0;
	}
	if (player.bank > 2000000000) {
		player.bank = 2000000000;
	}
	if (player.def < 0) {
		player.def = 0;
	}
	if (player.def > 32000) {
		player.def = 32000;
	}
	if (player.str < 0) {
		player.str = 0;
	}
	if (player.str > 32000) {
		player.str = 32000;
	}
	if (player.cha < 0) {
		player.cha = 0;
	}
	if (player.cha > 32000) {
		player.cha = 32000;
	}
	if (player.exp < 0) {
		player.exp = 0;
	}
	if (player.exp > 2000000000) {
		player.exp = 2000000000;
	}
	if (player.laid < 0) {
		player.laid = 0;
	}
	if (player.laid > 32000) {
		player.laid = 32000;
	}
	if (player.kids < 0) {
		player.kids = 0;
	}
	if (player.kids > 32000) {
		player.kids = 32000;
	}
}

function handle_igm(place)
{
	var cmd = place.cmdline;
	var args;
	var m;
	var ret;
	var out = new File(gamedir('out'+player.Record+'.lrd'));
	var lns;

	sln('');
	lln('  `)** `%HOLD ON.. `)**');
	sln('');
	cmd = cmd.replace(/\*/g, dk.connection.node);
	args = cmd.split(/\s/);
	cmd = args.shift();
	if (cmd[0] !== '/' && cmd[0] !== '\\' && (cmd.search(/^[A-Z]+:[\\\/]/) === -1)) {
		cmd = js.exec_dir + cmd;
	}
	m = cmd.match(/^(.*[\\\/])([^\\\/]+)/);
	if (m === null) {
		lln('Unable to execute '+place.cmdline);
		log(LOG_ERROR, 'Unable to execute '+place.cmdline);
		return;
	}
	args.unshift(new function(){}());
	args.unshift(m[1]);
	args.unshift(m[2]);
	player.put();
	if (psock !== undefined) {
		psock.write('IGMData '+(place.desc.length + 2)+'\r\n'+place.desc+'\r\n\r\n');
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after IGMData');
		}
		file_remove(out.name);
	}
	else {
		fmutex(out.name);
		if (!out.open('w')) {
			throw('Unable to open '+out.name);
		}
		out.writeln(place.desc);
		out.close();
		rfmutex(out.name);
	}
	ret = js.exec.apply(this,args);
	if (ret !== null && ret !== 0) {
		log(LOG_ERROR, place.cmdline+' returned '+ret.toSource());
	}
	check_fields();
	fmutex(out.name);
	if (file_exists(out.name)) {
		if (!out.open('r')) {
			throw('Unable to open '+out.name);
		}
		lns = out.readAll();
	}
	else {
		lns = [];
	}
	if (lns.length > 1) {
		out.close();
		if (lns[1] === 'QUIT') {
			file_remove(out.name);
			if (psock !== undefined) {
				psock.write('IGMData 2'+'\r\n\r\n\r\n');
				if (psock.readln() !== 'OK') {
					throw('Out of sync with server after IGMData');
				}
			}
		}
		else {
			if (psock !== undefined) {
				psock.write('IGMData '+(lns[0].length + lns[1].length + 2)+'\r\n'+lns[0]+'\r\n'+lns[1]+'\r\n');
				if (psock.readln() !== 'OK') {
					throw('Out of sync with server after IGMData');
				}
			}
		}
		rfmutex(out.name);
		return true;
	}
	if (out.is_open) {
		out.close();
	}
	file_remove(out.name);
	if (psock !== undefined) {
		psock.write('IGMData 2'+'\r\n\r\n\r\n');
		if (psock.readln() !== 'OK') {
			throw('Out of sync with server after IGMData');
		}
	}
	rfmutex(out.name);
	return false;
}

function create_other_places() {
	var f = new File(gamedir('3rdparty.lrd'));
	var oi = 0;
	var l;
	var ret = [];
	var tmp;

	if (settings.no_igms_allowed)
		return ret;
	if (!file_exists(f.name)) {
		if (f.open('w')) {
			f.writeln(';-=-=-=-= LORD\'S INTERNAL 3RD PARTY SOFTWARE OPTION =-=-=-');
			f.writeln(';Example of correct usage:');
			f.writeln(';barak/barak.js <PARMS>  (* will have lord.js interject node #)');
			f.writeln(';`9Name `3As `3It `2Appears `%to `2Users');
			f.writeln(';(Yes, two lines are needed for each app)');
			f.close();
		}
	}
	if (!f.open('r')) {
		throw('Unable to open '+f.name);
	}
	while (true) {
		l = f.readln();
		if (l === null) {
			break;
		}
		if (l[0] !== ';') {
			oi += 1;
			tmp = {cmdline:l};
			l = f.readln();
			if (l === null) {
				break;
			}
			tmp.desc = l;
			tmp.menu = '  `2(`0'+oi+'`2) '+l;
			ret.push(tmp);
		}
	}
	f.close();
	return ret;
}

function main()
{
	var quit = false;
	var ch;
	var oplaces = [];
	var i;
	var igmsec = new File(gamedir('igmsec.lrd'));
	var line;

	function goodbye() {
		sclrscr();
		sln('');
		sln('');
		lln('  `%Quitting To The Fields...');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		lln('`2  You find a comfortable place to sleep under a small tree...');
		sln('');
		player.on_now = false;
		player.put();
		exit(0);
	}

	function do_quit() {
		sln('');
		sln('');
		lw('`2  Quit game?  [`0Y`2] : ');
		ch = getkey().toUpperCase();
		if (ch !== 'N') {
			ch = 'Y';
		}
		sln(ch);
		if (ch === 'Y') {
			return true;
		}
		return false;
	}

	function prompt() {
		sln('');
		foreground(5);
		sln('  The Town Square');
		foreground(8);
		sln('  (? for menu)');
		sln('  (F,S,K,A,H,V,I,T,Y,L,W,D,C,O,X,M,P,Q)');
		command_prompt();
	}

	function menu() {
		if (player.bank < 0) {
			player.bank = 0;
		}
		if (player.gold < 0) {
			player.gold = 0;
		}
		if (player.exp < 0) {
			player.exp = 0;
		}
		if (!player.expert) {
			lrdfile('MAIN');
		}
	}

	function other_places_menu() {
		if (file_exists(gamedir('3rdalt.lrd'))) {
			display_file(gamedir('3rdalt.lrd'));
		}
		else {
			lln(' `%`r0`c  Other Options');
			lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			if (oplaces.length === 0) {
				lln('  `0(`2Currently Undeveloped Land`0)');
			}
			else {
				oplaces.forEach(function(place) {
					lln(place.menu);
				});
			}
		}
	}

	foreground(5);
	sln('');

	if (player.inn) {
		red_dragon_inn();
		if (player.dead) {
			return;
		}
	}

	menu();
	do {
		foreground(5);
		check_mail();
		prompt();
		ch = getkey().toUpperCase();
		sln(ch);
		switch(ch) {
			case 'V':
				show_stats();
				menu();
				break;
			case 'D':
				show_log();
				menu();
				break;
			case 'R':
				if (mail_check()) {
					check_mail();
				}
				else {
					sln('');
					sln('  You have no mail.');
				}
				sln('');
				break;
			case '?':
				// DIFF: Couldn't see menu in expert mode before...
				if (player.expert) {
					lrdfile('MAIN');
				}
				menu();
				break;
			case 'K':
				king_arthurs();
				menu();
				break;
			case 'S':
				slaughter_others();
				if (player.dead) {
					quit = true;
				}
				sln('');
				break;
			case 'A':
				abduls_armour();
				menu();
				sln('');
				break;
			case 'T':
				turgons();
				sln('');
				break;
			case 'W':
				compose_mail();
				break;
			case 'L':
				list_players();
				break;
			case 'O':
				foreground(10);
				background(0);
				oplaces = create_other_places();
				if (player.expert) {
					sclrscr();
					sln('');
					sln('');
				}
				else {
					other_places_menu();
				}
				check_mail();
				do {
					sln('');
					lw('  `2What\'s your pleasure? [`0Q`2] (`0? for menu`2) : `%');
					if (oplaces.length < 10) {
						ch = getkey().toUpperCase();
					}
					else {
						ch = getstr(0, 0, oplaces.length.toString().length, 1, 15, '').toUpperCase();
					}
					if (oplaces.length > 9) {
						if (ch[0] === '?') {
							ch = '?';
						}
						if (ch === '') {
							ch = 'Q';
						}
					}
					else {
						if (ch === '\r') {
							ch = 'Q';
						}
					}
					if (ch.search(/^[0-9]+$/) !== -1) {
						if (file_exists(igmsec.name)) {
							if (igmsec.open('r')) {
								line = igmsec.readln();
								line = parseInt(line, 10);
								if (!(isNaN(line))) {
									if (dk.user.level < line) {
										igmsec.readln();
										igmsec.readln();
										while (true) {
											line = igmsec.readln();
											if (line === null) {
												break;
											}
											lln(line);
										}
										break;
									}
								}
								igmsec.close();
							}
						}
						i = parseInt(ch, 10);
						if (i > 0 && i <= oplaces.length) {
							if (handle_igm(oplaces[i - 1])) {
								exit(0);
							}
						}
						other_places_menu();
						check_mail();
					}
					else if (ch === '?') {
						other_places_menu();
						check_mail();
					}
				} while(ch !== 'Q');
				sln('');
				sln('');
				break;
			case 'F':
				forest();
				if (player.dead) {
					quit = true;
				}
				sln('');
				break;
			case 'H':
				healers();
				sln('');
				break;
			case 'I':
				red_dragon_inn();
				if (player.dead || player.inn) {
					quit = true;
					sln('');
					break;
				}
				menu();
				sln('');
				break;
			case '2':	// TODO: RIP (Hah!)
				break;
			case 'M':
				announce();
				break;
			case 'P':
				warriors_on_now(false);
				sln('');
				break;
			case '1':
				show_game_stats();
				sln('');
				break;
			case 'C':
				conjugality_list();
				sln('');
				break;
			case '3':	// TODO
				sln('');
				break;
			case '4':	// TODO
				sln('');
				break;
			case 'X':
				player.expert = !player.expert;
				sln('');
				sln('');
				if (player.expert) {
					sln('  EXPERT MODE ON');
				}
				else {
					sln('  EXPERT MODE OFF');
				}
				break;
			case 'Q':
				if (!do_quit()) {
					break;
				}
				goodbye();	// goodbye() exits...
				sln('');
				break;
			case ' ':
				sln('');
				break;
			case 'Y':
				ye_old_bank();
				sln('');
				break;
			default:
				sln('');
		}
	} while (!quit);
}

function show_alone() {
	var alone = true;

	all_players().forEach(function(tp) {
		if (tp.on_now) {
			alone = false;
		}
	});
	if (!alone) {
		lln(center('`0You are `%NOT ALONE`0 here.  `2(Someone else is playing also)'));
	}
}

function intro_menu()
{
	var ch;

	do {
		sln('');
		lln('                         `2(`0E`2)nter the realm of the Dragon');
		lln('                         `2(`0I`2)nstructions');
		lln('                         `2(`0L`2)ist Warriors');
		lln('                         `2(`0Q`2)uit back to BBS');
		do {
			sln('');
			lw('                         `2Your choice, warrior? [`0E`2]: `%');
			ch = getkey().toUpperCase();
			if ('SE\rILQ1234567890'.indexOf(ch) === -1) {
				sln('');
				sln('');
				lln(center('`2Try entering a valid option, it might actually do something.'));
			}
		} while ('SE\rILQ1234567890'.indexOf(ch) === -1);
		sln(ch);
		switch (ch) {
			case '1':
				lrdfile('INTRO1');
				break;
			case '2':
				lrdfile('DRAG');
				break;
			case '3':
				lrdfile('INTRO2');
				break;
			case '4':
				lrdfile('DRAGON3');
				break;
			case '5':
				lrdfile('SM-LORD');
				break;
			case '6':
				lrdfile('LORD');
				break;
			case '7':
				lrdfile('FOOT');
				break;
			case '8':
				lrdfile('DEMON');
				break;
			case '9':
				lrdfile('LONGINTRO');
				break;
			case '0':
				lrdfile('ACCESSD');
				break;
			case 'Q':
				quit_msg();
				break;
			case 'S':
				lln('`c  `2You have pressed the `0S`2 key.  `0S`2 is for story.');
				sln('');
				more_nomail();
				lrdfile('STORY', true);
				more_nomail();
				sln('');
				break;
			case 'L':
				if (!file_exists(gamedir('player.bin'))) {
					sln('');
					sln('');
					lln(center('The game has never been played before.'));
				}
				else {
					generate_rankings(gamedir('tempn'+dk.connection.node), true, true, false);
					sclrscr();
					display_file(gamedir('tempn'+dk.connection.node),true,true);
					file_remove(gamedir('tempn'+dk.connection.node));
					sln('');
					sln('');
				}
				break;
			case 'I':
				instructions();
				break;
		}
	} while	('E\rQ'.indexOf(ch) === -1);
	return ch;
}

function check_gameover()
{
	var wb;

	get_state(false);
	if (state.won_by >= 0) {
		wb = player_get(state.won_by);
		load_player(false);
		if (player === undefined) {
			player = {Record:-1};
		}
		lrdfile('WIN');
		more_nomail();
		lln('`c                      `%PAY HOMAGE TO YOUR BETTER!     ');
		sln('');
		lln('  `0The incredible warrior whose deeds will grace every tongue of');
		sln('  every minstrel in every town every song of every day is the master');
		lln('  warrior known as `%'+wb.name+'`2.  ');
		sln('');
		more_nomail();
		if (state.won_by === player.Record) {
			lln('  `0You smile modestly.  If only people knew, that incredible');
			sln('  warrior was you.');
		}
		else {
			lln('  `2You bow your head in reverence - vowing to follow the teachings');
			sln('  of this great person - to learn whatever this Godlike wonder can');
			sln('  show you.');
			sln('');
			lln('  `#(ASK YOUR SYSOP TO USE THE (R)ESET OPTION IN LORDCFG.EXE)`%');
		}
		sln('');
		more_nomail();
		player.on_now = false;
		player.put();
		exit(0);
	}
}

function start() {
	var igm = {};
	var out;

	build_txt_index();

	// Should this allow a global file?
	if (file_exists(gamedir('hello.lrd'))) {
		display_file(gamedir('hello.lrd'), false, false);
	}

	sclrscr();

	switch (random(10)) {
		case 0:
			lrdfile('INTRO1');
			break;
		case 1:
			lrdfile('DRAG');
			break;
		case 2:
			lrdfile('INTRO2');
			break;
		case 3:
			lrdfile('DRAGON3');
			break;
		case 4:
			lrdfile('SM-LORD');
			break;
		case 5:
			lrdfile('LORD');
			break;
		case 6:
			lrdfile('FOOT');
			break;
		case 7:
			lrdfile('DEMON');
			break;
		case 8:
			lrdfile('LONGINTRO');
			break;
		case 9:
			lrdfile('ACCESSD');
			break;
	}
	getkey();
	sclrscr();
	sln('');
	sln('');
	lln(center('`0L`2egend `0O`2f The `0R`2ed `0D`2ragon'));
	sln('');
	get_state(false);
	lln(center('`2The current game has been running `0'+state.days+'`2 days.'));
	lln(center('`2Players are deleted after `0'+settings.delete_days+'`2 days of inactivity.'));

	show_alone();

	if (intro_menu() === 'Q') {
		if (player !== undefined) {
			player.on_now = false;
			player.put();
		}
		exit(0);
	}

	check_gameover();
	load_player(true);
	// TODO: LOCKOUT.DAT support

	js.on_exit('if (player !== undefined) { player.on_now = false; player.put(); } while(cleanup_files.length) file_remove(cleanup_files.shift()); if (psock !== undefined) psock.close();');

	if (player.on_now) {
		if (psock !== undefined) {
			psock.writeln('SetPlayer '+player.Record);
			if (psock.readln() !== 'OK') {
				throw('Out of sync with server after SetPlayer');
			}
		}
		if (!player.dead && player.hp > 0) {
			if (psock !== undefined) {
				out = psock_igm_lines(player.Record);
				igm.desc = out[0];
				igm.cmdline = out[1];
				if (igm.cmdline.length > 1) {
					if (handle_igm(igm)) {
						exit(0);
					}
				}
			}
			else {
				out = new File(gamedir('out'+player.Record+'.lrd'));
				fmutex(out.name);
				if (file_exists(out.name)) {
					if (!out.open('r')) {
						throw('Unable to open '+out.name);
					}
					igm.desc = out.readln();
					igm.cmdline = out.readln();
					out.close();
					rfmutex(out.name);
					if (igm.cmdline !== null) {
						if (handle_igm(igm)) {
							exit(0);
						}
					}
				}
				else {
					rfmutex(out.name);
				}
			}
			main();
		}

		player.on_now = false;
		player.put();
		sclrscr();
		sln('');
		sln('');
		sln('  Leaving the realm.');
	}
}

function get_qwk_pass()
{
	var scope = (new function(){}());
	var msgcfg;
	var pw;

	if (js.global.system === undefined) {
		return 'QWKPASS';
	}
	require(scope, "cnflib.js", "CNF");
	msgcfg = scope.CNF.read(system.ctrl_dir + 'msgs.cnf');
	if (msgcfg.qwknet_hub !== undefined) {
		msgcfg.qwknet_hub.forEach(function(h) {
			var m;

			if (pw !== undefined) {
				return;
			}
			m = h.call.match(/.?qnet-(?:ftp|http)\s+%s\s+(?:dove|cvs|vert|bbs)\.synchro\.net\s+([^\s]+)$/);
			if (m !== null) {
				pw = m[1].toUpperCase();
			}
		});
	}
	return (pw === undefined ? 'QWKPASS' : pw);
}

function cmdline()
{
	var i;
	var j;
	var ret = true;
	var scope;
	var trdp;
	var f = new File(gamedir('3rdparty.lrd'));
	var all;

	function readini() {
		var ini = new File(gamedir('lord.ini'));
		var tmp;
		var tenum = ['xp', 'dragonkills', 'playerkills', 'level', 'lays'];

		if (!ini.open('r')) {
			return;
		}

		Object.keys(settings).forEach(function(key) {
			if (settingsmap[key] === undefined) {
				throw('Unmapped setting "'+key+'"');
			}
			settings[key] = ini.iniGetValue(null, settingsmap[key], settings[key]);
		});

		Object.keys(tournament_settings).forEach(function(key) {
			if (key === 'winstat') {
				// Hack in an enum type...
				tmp = ini.iniGetValue('Tournament', 'WinStat');
				if (typeof tmp === 'string' && tenum.indexOf(tmp.toLowerCase()) !== -1) {
					tmp = tenum.indexOf(tmp.toLowerCase());
				}
				else if (typeof tmp !== 'number' || tmp >= tenum.length || tmp < 0) {
					tmp = tournament_settings.winstat;
				}
				tournament_settings.winstat = tmp;
			}
			else if (tournament_settings_map[key] === undefined) {
				throw('Unmapped setting "'+key+'"');
			}
			else {
				settings[key] = ini.iniGetValue('Tournament', tournament_settings_map[key], tournament_settings[key]);
			}
		});
		ini.close();
	}

	function check_3rdp(tp, install) {
		var ii;

		for (ii = 0; ii < tp.length; ii += 1) {
			if (tp[ii].desc === install.desc) {
				return false;
			}
		}
		return true;
	}

	/*
	 * Already handled by dorkit:
	 * -t, -telnet
	 * -s, -socket
	 * -l, -local
	 * -d, -dropfile
	 */

	// We can't use argc since dorkit has removed args, but argc can't be modified.
	for (i = 0; i < argv.length; i += 1) {
		if (argv[i].toUpperCase() === '+IGM' && argv.length > (i+1)) {
			scope = (new function(){}());
			i += 1;
			if (js.exec(argv[i], scope, 'INSTALL') === 0) {
				if (scope.install === undefined) {
					stdout.writeln('Script '+argv[i]+' does not support the install parameter');
				}
				else {
					trdp = create_other_places();
					if (!check_3rdp(trdp, scope.install)) {
						stdout.writeln('Script '+argv[i]+' is already installed');
					}
					else {
						if (scope.install.desc !== undefined) {
							trdp.push({cmdline:argv[i], desc:scope.install.desc});
							if (!f.open('a')) {
								stdout.writeln('Unable to open '+f.name);
							}
							else {
								f.writeln(argv[i]);
								f.writeln(scope.install.desc);
								f.close();
							}
						}
					}
				}
				ret = false;
			}
			else {
				stdout.writeln(argv[i]+' returned invalid value');
			}
		}
		else if (argv[i].toUpperCase() === '-IGM' && argv.length > (i+1)) {
			scope = (new function(){}());
			i += 1;
			if (js.exec(argv[i], scope, 'INSTALL') === 0) {
				if (scope.install === undefined) {
					stdout.writeln('Script '+argv[i]+' does not support the install parameter');
				}
				else {
					trdp = create_other_places();
					if (check_3rdp(trdp, argv[i], scope.install)) {
						stdout.writeln('Script '+argv[i]+' is not installed');
					}
					else {
						if (scope.install.desc !== undefined) {
							trdp.push({cmdline:argv[i], desc:scope.install.desc});
							if (!f.open('r+')) {
								stdout.writeln('Unable to open '+f.name);
							}
							else {
								all = f.readAll();
								f.position = 0;
								for (j = 0; j < all.length; j += 1) {
									if (all[j][0] === ';') {
										f.writeln(all[j]);
									}
									else if (j === all.length - 1) {
										f.writeln(all[j]);
									}
									else if (all[j+1] !== scope.install.desc) {
										f.writeln(all[j]);
										j += 1;
										f.writeln(all[j]);
									}
									else {
										j += 1;
										stdout.writeln(argv[i]+' removed.');
									}
								}
								f.truncate(f.position);
							}
						}
					}
				}
				ret = false;
			}
			else {
				stdout.writeln(argv[i]+' returned invalid value');
			}
		}
		else if (argv[i].toUpperCase() === '-p' && argv.length > (i+1)) {
			i += 1;
			settings.game_prefix = argv[i];
		}
		else if (argv[i].toUpperCase() === '-m' && argv.length > (i+1)) {
			i += 1;
			settings.menu_dir = argv[i];
		}
		else if (argv[i].toUpperCase() === 'RESET') {
			file_remove(gamedir('state.bin'));
			file_remove(gamedir('player.bin'));
		}
	}
	if (ret) {
		readini();
	}
	if (settings.game_user === 'QWKID' && js.global.system !== undefined) {
		settings.game_user = js.global.system.qwk_id;
	}
	if (settings.game_pass === 'QWKPASS' && js.global.system !== undefined) {
		settings.game_pass = get_qwk_pass();
	}
	return ret;
}

js.gc(true);
if (cmdline()) {
	start();
	exit(0);
}
