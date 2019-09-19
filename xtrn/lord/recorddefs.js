// Record file layouts for LORD...

var Player_Def = [
	{
		prop:'name',
		name:'Name',
		type:'String:20',
		def:''
	},
	{
		prop:'real_name',
		name:'BBS Name',
		type:'String:50',
		def:''
	},
	{
		prop:'hp',
		name:'Hit Points',
		type:'SignedInteger16',
		def:20
	},
	{
		prop:'divorced',
		name:'Divorced Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'amulet',
		name:'Has Amulet of Accuracy',
		type:'Boolean',
		def:false
	},
	{
		prop:'gone',
		name:'Days player has been gone',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'hp_max',
		name:'Max Hit Points',
		type:'SignedInteger16',
		def:20
	},
	{
		prop:'weapon_num',
		name:'Weapon Number',
		type:'SignedInteger8',
		def:1
	},
	{
		prop:'weapon',
		name:'Weapon Name',
		type:'String:20',
		def:'Stick'
	},
	{
		prop:'seen_master',
		name:'Seen Master Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'forest_fights',
		name:'Forest Fights Today',
		type:'SignedInteger16',
		def:15
	},
	{
		prop:'pvp_fights',
		name:'Player Fights',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'gold',
		name:'Gold on Hand',
		type:'SignedInteger',
		def:500
	},
	{
		prop:'bank',
		name:'Gold in Bank',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'def',
		name:'Defence Points',
		type:'SignedInteger16',
		def:1
	},
	{
		prop:'str',
		name:'Strength Points',
		type:'SignedInteger16',
		def:10
	},
	{
		prop:'cha',
		name:'Charm Points',
		type:'SignedInteger16',
		def:1
	},
	{
		prop:'seen_dragon',
		name:'Seen Dragon Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'seen_violet',
		name:'Seen Violet Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'level',
		name:'Level',
		type:'SignedInteger8',
		def:1
	},
	{
		prop:'time',
		name:'Day # Last On',
		type:'Integer',
		def:2147483648
	},
	{
		prop:'arm',
		name:'Armor Name',
		type:'String:20',
		def:'Coat'
	},
	{
		prop:'arm_num',
		name:'Armor Number',
		type:'SignedInteger8',
		def:1
	},
	{
		prop:'dead',
		name:'Dead',
		type:'Boolean',
		def:false
	},
	{
		prop:'inn',
		name:'Sleeing at Inn',
		type:'Boolean',
		def:false
	},
	{
		prop:'gem',
		name:'Number of Gems',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'exp',
		name:'Experience Points',
		type:'SignedInteger',
		def:1
	},
	{
		prop:'sex',
		name:'Sex',
		type:'String:1',
		def:'M'
	},
	{
		prop:'seen_bard',
		name:'Seen Seth Abel Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'last_reincarnated',
		name:'Day Number Last Reincarnated',
		type:'Integer',
		def:0
	},
	{
		prop:'laid',
		name:'Number of times Laid',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'got_delicious',
		name:'Got GIFT via Jenny today',
		type:'Boolean',
		def:false
	},
	{
		prop:'on_now',
		name:'Currently Playing',
		type:'Boolean',
		def:false
	},
	{
		prop:'time_on',
		name:'Time Started Playing',
		type:'String:5',
		def:'00:00'
	},
	{
		prop:'clss',
		name:'Class',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'horse',
		name:'Has a Horse',
		type:'Boolean',
		def:false
	},
	{
		prop:'kids',
		name:'Number of Kids',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'drag_kills',
		name:'Number of Dragon Kills (times won)',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'skillw',
		name:'Death Knight Skill Points',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'skillm',
		name:'Mystical Skill Points',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'skillt',
		name:'Thieving Skill Points',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'levelw',
		name:'Death Knight Skill Uses Today',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'levelm',
		name:'Mystical Skill Uses Today',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'levelt',
		name:'Thieving Skill Uses Today',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'married_to',
		name:'Currently Married To',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'transferred_gold',
		name:'Time Gold Transferred Today',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'pvp',
		name:'Total Player Kills',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'weird',
		name:'Weird Event in Forest Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'high_spirits',
		name:'In High Spirits Today',
		type:'Boolean',
		def:true
	},
	{
		prop:'flirted',
		name:'Flirted Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'olivia',
		name:'Met Olivia',
		type:'Boolean',
		def:false
	},
	{
		prop:'asshole',
		name:'Kicked Olivia',
		type:'Boolean',
		def:false
	},
	{
		prop:'olivia_count',
		name:'Number of Times Seen Olivia',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'done_tower',
		name:'Done a Tower Event Before',
		type:'Boolean',
		def:false
	},
	{
		prop:'has_des',
		name:'Has a Description',
		type:'Boolean',
		def:false
	},
	{
		prop:'des1',
		name:'Description of Self Line 1',
		type:'String:255',
		def:''
	},
	{
		prop:'des2',
		name:'Description of Self Line 2',
		type:'String:255',
		def:''
	},
	{
		prop:'leftbank',
		name:'Left bank today',
		type:'Boolean',
		def:false
	},
	{
		prop:'padding',
		name:'Padding for Compatibility',
		type:'String:99',
		def:''
	}
];

var SPlayer_Def = Player_Def.slice();
SPlayer_Def.push({
	prop:'SourceSystem',
	name:'Source Account',
	type:'String:20',	// TODO: This is the max BBS ID length
	def:''
});
SPlayer_Def.push({
	prop:'InIGM',
	name:'IGM name player is in',
	type:'String:255',	// TODO: This can likely be shorter...
	def:''
});
SPlayer_Def.push({
	prop:'IGMCommand',
	name:'Command-line for IGM player is in',
	type:'String:255',	// TODO: This may need to be longer...
	def:''
});

var Weapon_Armor_Def = [
	{
		prop:'name',
		name:'Weapon or Armor Name',
		type:'String:20',
		def:''
	},
	{
		prop:'price',
		name:'Price',
		type:'Integer',
		def:0
	},
	{
		prop:'num',
		name:'Defence/Strength Bonus',
		type:'Integer',
		def:0
	}
];

var Trainer_Def = [
	{
		prop:'name',
		name:'Trainers Name',
		type:'String:30',
		def:''
	},
	{
		prop:'weapon',
		name:'Trainers Weapon',
		type:'String:20',
		def:'Fists'
	},
	{
		prop:'swear',
		name:'Phrase When Beaten',
		type:'String:50',
		def:'Oh sugar!'
	},
	{
		prop:'needstr1',
		name:'Phrase When You Can Fight',
		type:'String:50',
		def:'You\'re really improving!'
	},
	{
		prop:'needstr2',
		name:'Phrase When You Can Fight',
		type:'String:50',
		def:'Come at me!'
	},
	{
		prop:'str',
		name:'Strength',
		type:'SignedInteger16',
		def:5
	},
	{
		prop:'hp',
		name:'Hit Points',
		type:'SignedInteger16',
		def:1
	},
	{
		prop:'hp_gained',
		name:'HP Gained',
		type:'SignedInteger16',
		def:1
	},
	{
		prop:'str_gained',
		name:'Strength Gained',
		type:'SignedInteger16',
		def:1
	},
	{
		prop:'def',
		name:'Defence Gained',
		type:'SignedInteger16',
		def:1
	},
	{
		prop:'need',
		name:'XP Needed to Fight',
		type:'SignedInteger',
		def:1
	}
];

var Monster_Def = [
	{
		prop:'name',
		name:'Monsters Name',
		type:'String:60',
		def:''
	},
	{
		prop:'str',
		name:'Strength',
		type:'SignedInteger',
		def:5
	},
	{
		prop:'gold',
		name:'Gold on Hand',
		type:'SignedInteger',
		def:500
	},
	{
		prop:'weapon',
		name:'Weapon',
		type:'String:60',
		def:'Fists'
	},
	{
		prop:'exp',
		name:'Experience Points',
		type:'SignedInteger',
		def:1
	},
	{
		prop:'hp',
		name:'Hit Points',
		type:'SignedInteger',
		def:1
	},
	{
		prop:'death',
		name:'Death Saying',
		type:'String:100',
		def:'Aw Nuts'
	}
];

var State_Def = [
	{
		prop:'latesthero',
		name:'Latest Hero',
		type:'String:20',
		def:'Master Turgon'
	},
	{
		prop:'married_to_seth',
		name:'Player who is married to Seth Able',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'married_to_violet',
		name:'Player who is married to Violet',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'days',
		name:'Days the game has been running',
		type:'Integer',
		def:0
	},
	{
		prop:'won_by',
		name:'Player who won the game when game is ended',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'log_date',
		name:'Day on month of latest log',
		type:'Integer',
		def:0
	},
	{
		prop:'last_bar',
		name:'Laster Person to Converse at Bar',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'forest_gold',
		name:'Gold Lost in Forest',
		type:'SignedInteger',
		def:100
	}
];

var Server_State_Def = [
	{
		prop:'latesthero',
		name:'Latest Hero',
		type:'String:20',
		def:'Master Turgon'
	},
	{
		prop:'married_to_seth',
		name:'Player who is married to Seth Able',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'married_to_violet',
		name:'Player who is married to Violet',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'won_by',
		name:'Player who won the game when game is ended',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'last_bar',
		name:'Laster Person to Converse at Bar',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'forest_gold',
		name:'Gold Lost in Forest',
		type:'SignedInteger',
		def:100
	}
];
