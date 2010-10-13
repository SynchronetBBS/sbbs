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
var global_items=[];
var player_attacks=[];
var mob_attacks=[];
var global_items=load_items(gi_file);

var activity_timeout=300;	// seconds
var tick_time=4;	// seconds
var max_armor=200;
var base_hitpoints=20;
var base_movement=60;
var base_stats=10;
var base_mana=20;
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
	if(time()-last_activity<tick_time) {
		return false;
	} else {
		for(var u in srv.users) {
			var player=players.player.(@name.toUpperCase() == u);
			if(player.@active == 1) {
				if(time()-srv.users[u].last_spoke<activity_timeout){
					active=true;
				} else {
					player.@active=0;
					srv.o(player.@channel,"You have been idle too long. Type 'RPG LOGIN' to resume playing RPG.");
				}
			}
		}
		last_activity=time();
	}
	return active;
}

/*	game objects */
function Coords(x,y,z)
{
	this.x=x;
	this.y=y;
	this.z=z;
}
function Settings()
{
	this.autolink=true;
	this.title="New Room";
	this.description="A new room.";
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


