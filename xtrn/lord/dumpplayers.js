load('recordfile.js');

require('recorddefs.js', 'Player_Def');

var pfile = new RecordFile('/synchronet/sbbs/xtrn/lord/player.dat',Player_Def);
var i;
var pl;
for (i=0; i<pfile.length; i++) {
	pl = pfile.Get(i);
	writeln(pl.name+' ('+pl.real_name+') - '+pl.sex+' '+pl.Record);
	writeln('-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	writeln('HP: '+pl.hp+'/'+pl.hp_max);
	writeln('DEF: '+pl.def+' \tSTR: '+pl.str+' \tCHA: '+pl.cha);
	writeln('XP: '+pl.exp+' ('+pl.level+')');
	writeln('Weapon: '+pl.weapon_num+' ('+pl.weapon+')');
	writeln('Armour: '+pl.arm_num+' ('+pl.arm+')');
	writeln('FF: '+pl.forest_fights+' \tPVP: '+pl.pvp_fights);
	writeln('Gold: '+pl.gold+'('+pl.bank+') \tGems: '+pl.gem);
	writeln('Laid: '+pl.laid);
	writeln('Class: '+pl.clss);
	writeln('Kids: '+pl.kids);
	writeln('Dragon Kills: '+pl.drag_kills);
	writeln('Skills: D:'+pl.skillw+'/'+pl.levelw+' M:'+pl.skillm+'/'+pl.levelm+' T:'+pl.skillt+'/'+pl.levelt);
	writeln('Married to: '+pl.married_to);
	writeln('Transfers today: '+pl.transferred_gold);
	writeln('PVP Kills: '+pl.pvp);
	writeln('Olivia Count: '+pl.olivia_count);
	writeln('des1: "'+pl.des1+'"');
	writeln('des2: "'+pl.des2+'"');
	writeln('Days Gone: '+pl.gone);
	writeln('Last on day: '+pl.time);
	writeln('Last reincarnated: '+pl.last_reincarnated);
	writeln('On: '+pl.time_on);
	write("Flags: ");
	write(pl.divorced ?		'D' : ' ');
	write(pl.amulet ?		'A' : ' ');
	write(pl.seen_master ?		'M' : ' ');
	write(pl.seen_dragon ?		'S' : ' ');
	write(pl.seen_violet ?		'V' : ' ');
	write(pl.dead ?			'X' : ' ');
	write(pl.inn ?			'I' : ' ');
	write(pl.seen_bard ?		'B' : ' ');
	write(pl.got_delicious ?	'J' : ' ');
	write(pl.on_now ?		'O' : ' ');
	write(pl.horse ?		'H' : ' ');
	write(pl.weird ?		'W' : ' ');
	write(pl.high_spirits ?		'P' : ' ');
	write(pl.flirted ?		'F' : ' ');
	write(pl.olivia ?		'C' : ' ');
	write(pl.asshole ?		'K' : ' ');
	write(pl.done_tower ?		'T' : ' ');
	write(pl.has_des ?		'E' : ' ');
	writeln('');

	writeln('');
}
