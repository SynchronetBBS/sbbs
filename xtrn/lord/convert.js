js.load_path_list.unshift(js.exec_dir+"load/");
require('recordfile.js', 'RecordFile');
require('recorddefs.js', 'Monster_Def');
require('screen.js', 'Screen');

LEnemy_Def = [
	{prop:'name', type:'PString:60'},
	{prop:'strength', type:'SignedInteger'},
	{prop:'gold', type:'SignedInteger'},
	{prop:'weapon', type:'PString:60'},
	{prop:'exp_points', type:'SignedInteger'},
	{prop:'hit_points', type:'SignedInteger'},
	{prop:'death', type:'PString:100'},
];

Trainers_Def = [
	{prop:'name', type:'PString:30'},
	{prop:'weapon', type:'PString:20'},
	{prop:'swear', type:'PString:50'},
	{prop:'needstr1', type:'PString:50'},
	{prop:'needstr2', type:'PString:50'},
	{prop:'strength', type:'SignedInteger16'},
	{prop:'hit', type:'SignedInteger16'},
	{prop:'hit_gained', type:'SignedInteger16'},
	{prop:'strength_gained', type:'SignedInteger16'},
	{prop:'defense_gained', type:'SignedInteger16'},
	{prop:'need', type:'SignedInteger'},
];

WeapArm_Def = [
	{prop:'name', type:'PString:255'},
	{prop:'price', type:'SignedInteger'},
	{prop:'num', type:'SignedInteger'},
];

Player_Info_Def = [
	{prop:'names', type:'PString:20'},
	{prop:'real_names', type:'PString:50'},
	{prop:'hit', type:'SignedInteger16'},
	{prop:'bad', type:'SignedInteger16'},
	{prop:'rate', type:'SignedInteger16'},
	{prop:'hit_max', type:'SignedInteger16'},
	{prop:'weapon_num', type:'SignedInteger16'},
	{prop:'weapon', type:'PString:20'},
	{prop:'seen_master', type:'SignedInteger16'},
	{prop:'fights_left', type:'SignedInteger16'},
	{prop:'human_left', type:'SignedInteger16'},
	{prop:'gold', type:'SignedInteger'},
	{prop:'bank', type:'SignedInteger'},
	{prop:'def', type:'SignedInteger16'},
	{prop:'strength', type:'SignedInteger16'},
	{prop:'charm', type:'SignedInteger16'},
	{prop:'seen_dragon', type:'SignedInteger16'},
	{prop:'seen_violet', type:'SignedInteger16'},
	{prop:'level', type:'SignedInteger16'},
	{prop:'time', type:'Integer16'},
	{prop:'arm', type:'PString:20'},
	{prop:'arm_num', type:'SignedInteger16'},
	{prop:'dead', type:'SignedInteger8'},
	{prop:'inn', type:'SignedInteger8'},
	{prop:'gem', type:'SignedInteger'},
	{prop:'exp', type:'SignedInteger'},
	{prop:'sex', type:'SignedInteger8'},
	{prop:'seen_bard', type:'SignedInteger8'},
	{prop:'who_knows', type:'SignedInteger16'},
	{prop:'who_cares', type:'SignedInteger16'},
	{prop:'why', type:'SignedInteger16'},
	{prop:'on_now', type:'Boolean'},
	{prop:'m_time', type:'SignedInteger16'},
	{prop:'time_on', type:'PString:5'},
	{prop:'class', type:'SignedInteger8'},
	{prop:'horse', type:'SignedInteger16'},
	{prop:'love', type:'PString:25'},
	{prop:'married', type:'SignedInteger16'},
	{prop:'kids', type:'SignedInteger16'},
	{prop:'king', type:'SignedInteger16'},
	{prop:'skillw', type:'SignedInteger8'},
	{prop:'skillm', type:'SignedInteger8'},
	{prop:'skillt', type:'SignedInteger8'},
	{prop:'levelw', type:'SignedInteger8'},
	{prop:'levelm', type:'SignedInteger8'},
	{prop:'levelt', type:'SignedInteger8'},
	{prop:'inn_random', type:'Boolean'},
	{prop:'married_to', type:'SignedInteger16'},
	{prop:'v1', type:'SignedInteger'},
	{prop:'v2', type:'SignedInteger16'},
	{prop:'v3', type:'SignedInteger16'},
	{prop:'v4', type:'Boolean'},
	{prop:'v5', type:'SignedInteger8'},
	{prop:'new_stat1', type:'SignedInteger8'},
	{prop:'new_stat2', type:'SignedInteger8'},
	{prop:'new_stat3', type:'SignedInteger8'},
];

Player_Ext_Def = [
	{prop:'olivia', type:'Array:150:Boolean'},
	{prop:'asshole', type:'Array:150:Boolean'},
	{prop:'olivia2', type:'Array:150:Boolean'},
	{prop:'timeswith', type:'Array:150:SignedInteger8'},
	{prop:'done_tower', type:'Array:150:Boolean'},
	{prop:'olivia5', type:'Array:150:Boolean'},
	{prop:'inv1', type:'Array:150:Boolean'},
	{prop:'inv2', type:'Array:150:Boolean'},
	{prop:'time', type:'SignedInteger'},
	{prop:'string1', type:'PString:60'},
	{prop:'string2', type:'PString:60'},
	{prop:'lordcfg', type:'PString:10'},
	{prop:'hey', type:'SignedInteger'},
	{prop:'myka', type:'Boolean'},
	{prop:'delete', type:'Boolean'},
	{prop:'safe_node', type:'Boolean'},
	{prop:'j1', type:'Boolean'},
	{prop:'j2', type:'Boolean'},
	{prop:'j3', type:'Boolean'},
	{prop:'j4', type:'Boolean'},
	{prop:'j5', type:'Boolean'},
	{prop:'j6', type:'Boolean'},
	{prop:'j7', type:'Boolean'},
	{prop:'j8', type:'Boolean'},
	{prop:'j9', type:'Boolean'},
];

Player_Ext2_Def = [
	{prop:'des', type:'Boolean'},
	{prop:'des1', type:'PString:255'},
	{prop:'des2', type:'PString:255'},
];

function convert_lenemy(from, to)
{
	var f = new RecordFile(from, LEnemy_Def);
	var t = new RecordFile(to, Monster_Def);
	var fr;
	var tr;
	var i;

	t.file.position = 0;
	t.file.truncate(0);
	for (i = 0; i < f.length; i++) {
		fr = f.get(i);
		tr = t.new();

		tr.name = fr.name;
		tr.str = fr.strength;
		tr.gold = fr.gold;
		tr.weapon = fr.weapon;
		tr.exp = fr.exp_points;
		tr.hp = fr.hit_points;
		tr.death = fr.death;
		tr.put();
	}
}

function convert_trainers(from, to)
{
	var f = new RecordFile(from, Trainers_Def);
	var t = new RecordFile(to, Trainer_Def);
	var fr;
	var tr;
	var i;

	t.file.position = 0;
	t.file.truncate(0);
	for (i = 0; i < f.length; i++) {
		fr = f.get(i);
		tr = t.new();

		tr.name = fr.name;
		tr.weapon = fr.weapon;
		tr.swear = fr.swear;
		tr.needstr1 = fr.needstr1;
		tr.str = fr.strength;
		tr.hp = fr.hit;
		tr.hp_gained = fr.hitgained;
		tr.str_gained = fr.strength_gained;
		tr.def = fr.defence_gained;
		tr.need = fr.need;
		tr.put();
	}
}

function convert_weap_arm(from, to)
{
	var f = new RecordFile(from, WeapArm_Def);
	var t = new RecordFile(to, Weapon_Armor_Def);
	var fr;
	var tr;
	var i;

	t.file.position = 0;
	t.file.truncate(0);
	for (i = 0; i < f.length; i++) {
		fr = f.get(i);
		tr = t.new();

		tr.name = fr.name;
		tr.price = fr.price;
		tr.num = fr.num;
		tr.put();
	}
}

function convert_player(from, ext, p2, to)
{
	var f = new RecordFile(from, Player_Info_Def);
	var e = new RecordFile(ext, Player_Ext_Def);
	var p = new RecordFile(ext, Player_Ext2_Def);
	var t = new RecordFile(to, Player_Def);
	var fr;
	var tr;
	var pr;
	var i;
	var er;

	if (e === null || e.length < 1) {
		er = {olivia:new Array(150),
			asshole:new Array(150),
			olivia2:new Array(150),
			timeswith:new Array(150),
			done_tower:new Array(150),
			olivia5:new Array(150),
			inv1:new Array(150),
			inv2:new Array(150),
			time:0,
			string1:'',
			string2:'',
			lordcfg:'',
			hey:0,
			myka:false,
			delete:false,
			safe_node:false,
			j1:false,
			j2:false,
			j3:false,
			j4:false,
			j5:false,
			j6:false,
			j7:false,
			j8:false,
			j9:false};
	}
	else
		er = e.get(0);

	t.file.position = 0;
	t.file.truncate(0);
	for (i = 0; i < f.length; i++) {
		fr = f.get(i);
		tr = t.new();
		if (p !== null)
			pr = p.get(i);
		else
			pr = null;
		if (pr === null)
			pr = {des:false, des1:'', des2:''};

		tr.name = fr.names;
		tr.real_name = fr.real_names;
		tr.hp = fr.hit;
		tr.divorced = !!(fr.bad & 0x02);
		tr.amulet = !!(fr.bad & 0x01);
		tr.gone = fr.rate;
		tr.hp_max = fr.hit_max;
		tr.weapon_num = fr.weapon_num;
		tr.weapon = fr.weapon;
		tr.seen_master = !!(fr.seen_master === 5);
		tr.forest_fights = fr.fights_left;
		tr.pvp_fights = fr.human_left;
		tr.gold = fr.gold;
		tr.bank = fr.bank;
		tr.def = fr.def;
		tr.str = fr.strength;
		tr.cha = fr.charm;
		tr.seen_dragon = !!(fr.seen_dragon === 5);
		tr.seen_violet = !!(fr.seen_violet === 5);
		tr.level = fr.level;
		tr.time = fr.time;
		tr.arm = fr.arm;
		tr.arm_num = fr.arm_num;
		tr.dead = !!(fr.dead === 5);
		tr.inn = !!(fr.inn === 5);
		tr.gem = fr.gem;
		tr.exp = fr.exp;
		tr.sex = (fr.sex === 5 ? 'F' : 'M');
		tr.seen_bard = !!(fr.seen_bard === 5);
		tr.last_reincarnated = fr.who_knows;
		tr.laid = fr.who_cares;
		tr.got_delicious = fr.why;
		tr.on_now = fr.now;
		tr.time_on = fr.time_on;
		tr.clss = fr.class;
		tr.horse = !!(fr.horse === 1);
		tr.kids = fr.kids;
		tr.drag_kills = fr.king;
		tr.skillw = fr.skillw;
		tr.skillm = fr.skillm;
		tr.skillt = fr.skillt;
		tr.levelw = fr.levelw;
		tr.levelm = fr.levelm;
		tr.levelt = fr.levelt;
		tr.married_to = fr.married_to;
		tr.transferred_gold = fr.v1;
		tr.pvp = fr.v2;
		tr.weird = !!(fr.weird === 5);
		tr.high_spirits = fr.v4;
		tr.flirted = !!(fr.flirted === 5);
		tr.olivia = er.olivia[tr.Record];
		tr.asshole = er.asshole[tr.Record];
		tr.olivia_count = er.timeswith[tr.Record];
		tr.done_tower = er.done_tower[tr.Record];
		tr.has_des = p.des;
		tr.des1 = p.des1;
		tr.des2 = p.des2;
		tr.put();
	}
}

function convert_menus(ltf, lef, lgf, ttf, from, to)
{
	var f = new File(from);
	var t = new File(to);
	var lt = new File(ltf);
	var le = new File(lef);
	var lg = new File(lgf);
	var tt = new File(ttf);
	var s = new Screen(80, 1000, 7, ' ', true);
	var fmap = {
		BJTABLE:lg,
		CREEPY:le,
		OLIVIA:le,
		TOWER:le,
		OLDMAN:lt,
		OLDMAN:lt,
		CLOAK:lt,
		TURGON:lt,
		BT:lt,
		ABDUL:lt,
		ARTHUR:lt,
		BANK:lt,
		BARD:lt,
		BARDF:lt,
		BT1:lt,
		BUYARM:lt,
		BUYWEP:lt,
		FOREST:lt,
		SETH:lt,
		HEAL:lt,
		HINTS:lt,
		RDI:lt,
		RDIF:lt,
		VIOLET:lt,
		WAR:lt,
		LAIR:lt,
		LONGINTRO:lt,
		INTRO:lt,
		INTRO1:lt,
		INTRO2:lt,
		DRAG:lt,
		MAIN:lt,
		CHANCE:lt,
		BT1F:lt,
		LAIRANS:lt,
		BTF:lt,
		STORY:lt,
		FAIRY:lt,
		DRAGON3:lt,
		'SM-LORD':lt,
		LORD:lt,
		WIN:lt,
		FOOT:lt,
		DEMON:lt,
		LONGINTRO:lt,
		ACCESSD:lt,
		LEVEL1MALE:tt,
		LEVEL1FEMALE:tt,
		LEVEL2MALE:tt,
		LEVEL2FEMALE:tt,
		LEVEL3MALE:tt,
		LEVEL3FEMALE:tt,
		LEVEL4MALE:tt,
		LEVEL4FEMALE:tt,
		LEVEL5MALE:tt,
		LEVEL5FEMALE:tt,
		LEVEL6MALE:tt,
		LEVEL6FEMALE:tt,
		LEVEL7MALE:tt,
		LEVEL7FEMALE:tt,
		LEVEL8MALE:tt,
		LEVEL8FEMALE:tt,
		LEVEL9MALE:tt,
		LEVEL9FEMALE:tt,
		LEVEL10MALE:tt,
		LEVEL10FEMALE:tt,
		LEVEL11MALE:tt,
		LEVEL11FEMALE:tt
	};

	function get_menu(name) {
		var mf;
		var ret = [];
		var l;
		var found = false;

		if (fmap[name] === undefined || fmap[name] === null)
			mf = f;
		else
			mf = fmap[name];

		mf.position = 0;
		do {
			l = mf.readln();
			if (l === null) {
				if (ret.length > 0)
					return ret;
				if (mf === f)
					return ret;
				else {
					found = false;
					mf = f;
					mf.position = 0;
				}
			}
			else if (found) {
				if (l.indexOf('@#') === 0)
					return ret;
				ret.push(l);
			}
			else if (l.indexOf('@#'+name) === 0)
				found = true;
		} while(true);
		return ret;
	}

	function restspaces(buf, pos, len, la) {
		var i;

		for (i = 0; i < len*2; i += 2) {
			if (buf[pos + i] !== 32 && buf[pos + i] !== 0) {
				return false;
			}
			if (la === -1 || (buf[pos + i + 1] & 0x70) !== (la & 0x70)) {
				return false;
			}
		}
		return true;
	}

	if (!f.open('r')) {
		throw('Unable to open '+f.name);
	}
	if (!t.open('w')) {
		throw('Unable to open '+w.name);
	}
	if (!lt.open('r'))
		lt = null;
	if (!le.open('r'))
		le = null;
	if (!lg.open('r'))
		lg = null;
	if (!tt.open('r'))
		tt = null;
	Object.keys(fmap).sort().forEach(function(name) {
		var mnu = get_menu(name);
		var i;
		var ansi = false;
		var clear = false;
		var nnmu;
		var ll;
		var la;
		var x;
		var y;
		var nl;
		var ch;
		var attr;

		if (name !== name.toUpperCase())
			return;
		if (mnu.length < 1)
			throw('Unable to read '+name);
		for (i = 0; i < mnu.length; i++) {
			if (mnu[i].indexOf('\x1b[2J') != -1)
				clear = true;
			if (mnu[i].indexOf('\x1b') != -1) {
				ansi = true;
				break;
			}
		}
		if (ansi) {
			s.print('\x1b[2J\x1b[1;1H');
			la = -1;
			print(name+' is ANSI');
			for (i = 0; i < mnu.length; i++) {
				s.print(mnu[i]+'\r\n');
			}
			ll = s.pos.y;
			nmnu = [];
			nl = '';
			for (y = 0; y < ll; y++) {
				for (x = 0; x < 80; x++) {
					if (clear) {
						nl += '`C';
						clear = false;
					}
					ch = ascii(s.graphic.puttext[(y*80+x)*2]);
					attr = s.graphic.puttext[(y*80+x)*2+1]
					if (la !== attr && ((la & 0x70) !== (attr & 0x70) || ch !== ' ')) {
						if (la === -1 || (la & 0x70) != (attr & 0x70)) {
							nl += '`r'+['0','1','2','3','4','5','6','7'][(attr & 0x70)>>4];
							if (la !== -1)
								la = (la & 0x8f) | (attr & 0x70);
						}
						if (la === -1 || ((la & 0x8f) != (attr & 0x8f) && ch !== ' ')) {
							nl += '`';
							if (attr & 0x80)
								nl += 'B';
							nl += ['^', '1', '2','3','4','5','6','7','8','9','0','!','@','#','$','%'][attr & 0x0f];
							if (la !== -1)
								la = (la & 0x70) | (attr & 0x8f);
						}
						if (la === -1)
							la = attr;
					}
					if (restspaces(s.graphic.puttext, (y*80+x)*2, 80-x, la))
						break;
					nl += ch;
				}
				if (x === 80)
					nl += '`n';
				else {
					nmnu.push(nl);
					nl = '';
				}
			}
			if (nl != '')
				nmnu.push(nl);
			// Leave up to one blank line at the end?
			while (nmnu[nmnu.length-1] === '') {
				nmnu.pop();
				print("Removing blank line from "+name);
			}
			mnu = nmnu;
		}
		else
			print(name+' is already LORD format');
		t.writeln('@#'+name);
		mnu.forEach(function(l) {
			t.writeln(l);
		});
	});
}

//convert_menus(js.exec_dir + 'LORDTXT.DAT', js.exec_dir + 'LORDEXT.DAT', js.exec_dir + 'LGAMETXT.DAT', js.exec_dir + 'TRAINTXT.DAT', js.exec_dir + 'lordtxt.lrd', js.exec_dir + 'lordtxt.new');

