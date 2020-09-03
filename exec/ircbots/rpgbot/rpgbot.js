/* 
	$Id: rpgbot.js,v 1.6 2010/10/28 03:34:57 mcmlxxix Exp $
	Synchronet IRC bot module for ircbot.js
	
	Role-playing game bot (MUD) with in-game zone editor
	using XML data files. 
	
	TODO: 
		add in-game item creation/editing
		add in-game mob creation/editing
		add time-based events and zone-cycles
		...more when I think of it
		
	by Matt Johnson : MCMLXXIX (2010)
*/

var p_dir=this.dir+"players/";
var z_dir=this.dir+"zones/";
var c_file=new File(this.dir+"classes.xml");
var r_file=new File(this.dir+"races.xml");
var gi_file=new File(this.dir+"items.xml");

var settings=new Settings();
var players=[];
var zones=[];
var classes=[];
var races=[];
var player_attacks=[];
var mob_attacks=[];
var global_items=load_items(gi_file);

var black=String.fromCharCode(3)+"1";
var blue=String.fromCharCode(3)+"2";
var green=String.fromCharCode(3)+"3";
var red=String.fromCharCode(3)+"5";

var last_activity=0;	// time_t activity tracker
var mob_count=0;
var item_count=0;

function save() {
	var player_list=players.player;
	for(var p in player_list) {
		var player=player_list[p];
		var p_file=new File(p_dir + player.@name + ".xml");
		if(!p_file.open("w+")) continue;
		debug("saving RPG player: " + player.@name);
		p_file.write(player);
		p_file.close();
	}
}

function main(srv,target) {	
	if(!update_game_status(srv)) return;
	
	/* cycle through players currently in battle (in this channel or logged in via MSG */
	for(var p in player_attacks) {
		var attack=player_attacks[p];
		if(Number(attack.attacker.hitpoints)<=0) {
			if(attack.attacker.@channel.toString().length > 0) {
				srv.o(attack.attacker.@channel,"You are dead!");
				attack.attacker.active=0;
			}
			delete player_attacks[p];
		}
		if(Number(attack.defender.hitpoints)<=0) {
			if(attack.attacker.@channel.toString().length > 0) {
				srv.o(attack.attacker.@channel,attack.defender.@name + " is dead!");
			}
			kill(srv,attack.defender);
			delete player_attacks[p];
		}
		if(attack.attacker.@room == attack.defender.@room) {
			attack_round(srv,attack.attacker,attack.defender);
		}
	}
	
	for(var m in mob_attacks) {
		var attack=mob_attacks[m];
		if(Number(attack.attacker.hitpoints)<=0) {
			if(attack.attacker.@channel.toString().length > 0) {
				srv.o(attack.attacker.@channel,"You are dead!");
				attack.attacker.active=0;
			}
			delete mob_attacks[m];
		}
		if(Number(attack.defender.hitpoints)<=0) {
			if(attack.attacker.@channel.toString().length > 0) {
				srv.o(attack.attacker.@channel,attack.defender.@name + " is dead!");
			}
			kill(srv,attack.defender);
			delete mob_attacks[m];
		}
		if(attack.attacker.@room == attack.defender.@room) {
			attack_round(srv,attack.attacker,attack.defender);
		}
	}
	/* cycle through player rooms and perform timed events */
}

function update_game_status(srv) {
	var active=false;
	if(time()-last_activity<settings.tick_time) {
		return false;
	} else {
		for(var u in srv.users) {
			var player=players.player.(@name.toUpperCase() == u);
			if(player.@active == 1) {
				if(time()-srv.users[u].last_spoke<settings.activity_timeout){
					active=true;
				} else {
					player.@active=0;
					srv.o(player.@channel,
						"You have been idle too long. Type 'login' to resume playing.");
				}
			}
		}
		last_activity=time();
	}
	return active;
}

/*	game objects */
function Editor(r,e,i,m)
{
	this.room=r;
	this.exit=e;
	this.item=i;
	this.mob=m;
}
function Coords(x,y,z)
{
	this.x=x;
	this.y=y;
	this.z=z;
}
function Settings()
{
	/* game settings */
	this.activity_timeout=300;	// seconds
	this.tick_time=4;	// seconds
	this.max_armor=200;
	this.base_hitpoints=20;
	this.base_movement=60;
	this.base_stats=10;
	this.base_mana=20;
	
	/* editor settings */
	this.autolink=true;
	
	this.default_room_title="New Room";
	this.default_room_description="a new room.";
	
	this.default_item_title="a new item";
	this.default_item_appearance="a new item hovers in mid-air.";
	this.default_item_description="the item appears new.";
	this.default_item_keywords="new item";
	this.default_item_type="new";
	this.default_item_weight="1";
	
	this.default_mob_name="a new mob";
	this.default_mob_appearance="a new mob hovers in mid-air.";
	this.default_mob_keywords="new mob";
	this.default_mob_hitpoints="1";
	
	this.default_zone_title="New Zone";
	this.default_zone_level=1;
	
}
function Attack(attacker,defender)
{
	this.attacker=attacker;
	this.defender=defender;
}
function Move(item,result)
{
	this.item=item;
	this.result=result;
}

/*	Init */
load_classes();
load_races();
/* 	TODO: make zones load on demand, 
	as loading them all at the start could become a problem 
	when the world gets bigger */
load_zones(); 
load_players();


