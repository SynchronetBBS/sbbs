var p_dir=this.dir+"players/";
var z_dir=this.dir+"zones/";
var c_file=new File(this.dir+"classes.xml");
var r_file=new File(this.dir+"races.xml");

var players=[];
var zones=[];
var classes=[];
var races=[];
var global_items=[];
var player_attacks=[];
var mob_attacks=[];

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
		p_file.writeln(player.normalize());
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

function create_player(name,player_race,player_class) {
	var player=
		<player name={name} active="0" zone="0" room="0">
			<experience>0</experience>
			<hitpoints>{base_hitpoints}</hitpoints>
			<movement>{base_movement}</movement>
			<mana>{base_mana}</mana>
			<gold>0</gold>
			<strength>{base_stats}</strength>
			<dexterity>{base_stats}</dexterity>
			<wisdom>{base_stats}</wisdom>
			<intelligence>{base_stats}</intelligence>
			<constitution>{base_stats}</constitution>
			<level>1</level>
			<travel>walk</travel>
			<player_class>{player_class}</player_class>
			<player_race>{player_race}</player_race>
			<inventory>
				{items.item.(@id == "1").copy()}
				{items.item.(@id == "2").copy()}
				{items.item.(@id == "3").copy()}
				{items.item.(@id == "4").copy()}
			</inventory>
			<equipment>
				<slot id="head" qty="1"/>
				<slot id="neck" qty="1"/>
				<slot id="about body" qty="1"/>
				<slot id="body" qty="1"/>
				<slot id="waist" qty="1"/>
				<slot id="wrist" qty="2"/>
				<slot id="hands" qty="1"/>
				<slot id="finger" qty="2"/>
				<slot id="wielded" qty="1"/>
				<slot id="legs" qty="1"/>
				<slot id="feet" qty="1"/>
			</equipment>
		</player>;
	return player;
}

/*	game objects */
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
load_players();
/* 	TODO: make zones load on demand, 
	as loading them all at the start could become a problem 
	when the world gets bigger */
load_zones(); 
