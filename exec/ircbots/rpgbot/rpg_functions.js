function Server_command(srv,cmdline,onick,ouh) {
	var cmd=IRC_parsecommand(cmdline);
	var player=players.player.(@name == onick);
	switch (cmd[0]) {
		case "PART":
		case "QUIT":
		case "KICK":
			if(player.@active==1) {
				player.@active=0;
			}
			break;
		case "JOIN":
			if (cmd[1][0] == ":")
				cmd[1] = cmd[1].slice(1);
			var target=cmd[1];
			//srv.o(target,"Welcome to RPG test!","NOTICE");
			//srv.o(target,"For a list of classes, type " + get_cmd_prefix() + "RPG CLASSES","NOTICE");
			//srv.o(target,"For a list of races, type " + get_cmd_prefix() + "RPG RACES","NOTICE");
			//srv.o(target,"To create a new character, type " + get_cmd_prefix() + "RPG CREATE <race> <class>","NOTICE");
			//srv.o(target,"Type RPG LOGIN to start playing!","NOTICE");
			break;
		case "PRIVMSG":
			var target=cmd[1];
			if ((cmd[1][0] == "#") || (cmd[1][0] == "&")) {
				/* respond only to private message commands */
				if(player.@active == 1 && player.@channel == target) {
					cmd.shift();
					cmd.shift();
					/*	handle commands such as north, south, east, west.... */
					try {
						handle_command(srv,cmd,player,player.@channel);
					} catch(e) {
						srv.o(target,e);
					}
				}
			} else if (cmd[1].toUpperCase() == srv.curnick.toUpperCase()) {
				if(player.@active ==1 ) {
					cmd.shift();
					cmd.shift();
					/*	handle commands such as north, south, east, west.... */
					try {
						handle_command(srv,cmd,player,onick);
					} catch(e) {
						srv.o(target,e);
					}
				}
			}
			break;
		default:
			break;
	}
}

function handle_command(srv,cmd,player,target) {
	var zone=zones[player.@zone];
	var room=zone.room.(@id == player.@room);
	
	cmd[0]=cmd[0].substr(1).toUpperCase();
	switch(cmd[0]) 
	{
	case "N":
		cmd[0]="NORTH";
		break;
	case "S":
		cmd[0]="SOUTH";
		break;
	case "E":
		cmd[0]="EAST";
		break;
	case "W":
		cmd[0]="WEST";
		break;
	case "U":
		cmd[0]="UP";
		break;
	case "D":
		cmd[0]="DOWN";
		break;
	case "L":
		cmd[0]="LOOK";
		break;
	case "IN":
		cmd[0]="INVENTORY";
		break;
	case "EQ":
		cmd[0]="EQUIPMENT";
		break;
	}
	switch(cmd[0]) 
	{
		case "NORTH":
		case "SOUTH":
		case "EAST":
		case "WEST":
		case "UP":
		case "DOWN":
			cmd.unshift("MOVE");
			break;
	}
	
	if(RPG_Commands[cmd[0]]) {
		RPG_Commands[cmd[0]](srv,target,zone,room,cmd,player);
		return;
	}
	
	for(var m in room.mob) {
		var mob=room.mob[m];
		for(var t in mob.trigger) {
			if(cmd.join().match(mob.trigger[t].@id.toUpperCase())) {
				srv.o(target,mob.name + " says, '" + mob.trigger[t].response + "'");
			}
		}
	}
}

/*	game activity functions */

function display_room(srv,player,z,r) {
	log("displaying zone: " + z + " room: " + r);

	var zone=zones[z];
	var room=zone.room.(@id == r);
	
	show_room_title(srv,player.@channel,zone,room);
	show_room_description(srv,player.@channel,zone,room);
	show_room_exits(srv,player.@channel,zone,room);
	show_room_items(srv,player.@channel,zone,room);
	show_room_mobs(srv,player.@channel,zone,room);
	show_room_players(srv,player,zone,room);
}

function show_room_description(srv,target,zone,room) {
	srv.o(target,strip_ctrl(room.description));
}

function show_room_title(srv,target,zone,room) {
	srv.o(target,room.title);
}

function show_room_items(srv,target,zone,room) {
	for each(var i in room.item) {
		srv.o(target,i.appearance);
	}
}

function show_room_mobs(srv,target,zone,room) {
	for each(var m in room.mob) {
		srv.o(target,m.appearance);
	}
}

function show_room_players(srv,player,zone,room) {
	for each(var p in players.player.(
		@active == 1 && 
		@room == room.@id && 
		@name != player.@name && 
		@zone == zone.@id )) 	{
		srv.o(player.@channel,p.@name + " is here.");
	}
}

function show_room_exits(srv,target,zone,room) {
	srv.o(target,get_exit_string(zone,room));
}

function get_exit_string(zone,room) {
	var exit_str="";
	for(var e in room.exit) {
		var prefix="";
		if(room.exit[e].door) {
			var door=zone.door.(@id == room.exit[e].door);
			if(door.locked==true) prefix="@";
			else if(door.open==false) prefix="#";
		}
		exit_str+=", "+prefix+room.exit[e].@name;
	}
	return ("exits: " + exit_str.substr(2));
}

function add_player_attack(attacker,defender) {
	if(player_attacks[attacker.@name]) return false;
	player_attacks[attacker.@name]=new Attack(attacker,defender);
}

function add_mob_attack(attacker,defender) {
	if(mob_attacks[attacker.@id]) return false;
	mob_attacks[attacker.@id]=new Attack(attacker,defender);
}

function find_door(zone,room,name) {
	for(var e in room.exit) {
		if(room.exit[e].door) {
			var door=zone.door.(@id == room.exit[e].door);
			if(door.@name.toLowerCase().match(name.toLowerCase()) ||
				room.exit[e].@name.toLowerCase().match(name.toLowerCase())) {
				return door;
			}
		} 
	}
	return false;
}

function get_equip_message(item) {
	switch(item.equip.@id.toString()) {
		case "wielded":
			return "wield";
		case "head":
		case "body":
		case "hands":
		case "legs":
		case "feet":
		case "neck":
		case "ring":
			return "wear";
		case "shield":
			return "hold";
	}
}

function get_attack_message(item) {
	switch(item.attack.@id.toString()) {
		case "sword":
			return "slash";
		case "dagger":
			return "stab";
		case "mace":
		case "hammer":
			return "pound";
		case "whip":
		case "flail":
			return "whip";
		default:
			return "hit";
	}
}

function find_object(container,keyword) {
	for(var i in container.item) {
		var item=container.item[i];
		if(item.keywords.match(keyword.toLowerCase())) {
			return i;
		}
	}
	return false;
}

function find_mob(room,keyword) {
	for(var m in room.mob) {
		var mob=room.mob[m];
		if(mob.keywords.match(keyword.toLowerCase())) {
			return m;
		}
	}
	return false;
}

function move_by_name(from,to,obj) {
	var result=false;
	var item=false;
	var item_index=find_object(from,obj);
	if(item_index) {
		item=from.item[item_index];
		if(add_item(to,from.item[item_index])) {
			remove_by_index(from,item_index);
			result=true;
		}
	}
	return new Move(item,result);
}

function move_by_index(from,to,index) {
	var result=false;
	var item=from.item[index];
	if(add_item(to,from.item[index])) {
		remove_by_index(from,index);
		result=true;
	}
	return new Move(item,result);
}

function remove_by_index(container,item_index) {
	delete container.item[item_index];
}

function add_item(container,item) {
	container.appendChild(new XML(item.toXMLString()));
	return true;
}

/*	game battle functions */

function roll_them_dice(num_dice,num_sides) {
	var total=0;
	for(var d=0;d<num_dice;d++) {
		total+=random(num_sides)+1;
	}
	return total;
}

function attack_round(srv,attacker,defender) {
	
	if(!calculate_hit(attacker)) {
		srv.o(defender.@name,"You miss your attack!");
	}
	var attack_damage=calculate_attack(attacker);
	var damage_reduction=calculate_damage(defender);
	var actual_damage=(damage_reduction * attack_damage);
	defender.hitpoints = Number(defender.hitpoints)-actual_damage;

	if(attacker.@channel.toString().length > 0) srv.o(attacker.@channel,"You hit " + defender.@name + " for " + actual_damage);
	if(defender.@channel.toString().length > 0) srv.o(defender.@channel,attacker.@name + " hits you for " + actual_damage);
	var players_in_room=players.player.(
		@active == 1 &&
		@room == attacker.@room && 
		@name != attacker.@name && 
		@name != defender.@name);
	for each(var p in players_in_room) {
		srv.o(p.@channel,attacker.@name + " hits " + defender.@name + " for " + actual_damage);
	}
}

function calculate_attack(attacker) {
	/* consider things like armor, protective spells, hitroll, damroll */
	
	/* tally weapons */
	var weapons=attacker.equipment.slot.(@id == "wielded").item;
	var damage=0;
	for(var w in weapons) {
		var weapon=weapons[w];
		damage+=roll_them_dice(Number(weapon.dice.@qty),Number(weapon.dice.@sides));
	}
	return damage;
}

function calculate_damage(defender) {
	return 1;
}

function calculate_hit(attacker) {
	/*	consider things like dexterity, armor, protective spells, etc.... */
	return true;
}

function kill(srv,object) {
	var zone=zones[object.@zone];
	var room=zone.room.(@id == object.@room);
	create_corpse(room,object);
	delete object;
}

function create_corpse(room,object) {
	log("generating corpse");
	var corpse=<item>
			<type>container</type>
			<title>{"the corpse of " + object.@name}</title>
			<keywords>corpse</keywords>
			<description>the corpse looks dead.</description>
			<appearance>{"the corpse of " + object.@name + " lies here."}</appearance>
			<weight>100</weight>
		</item>;
	corpse.appendChild(object.inventory.children().copy());
	corpse.appendChild(object.equipment.slot.children().copy());
	room.appendChild(corpse);
}

/*	game init functions */

function load_zone(dir) {
		var z_file=new File(dir + "map.xml");
		var m_file=new File(dir + "mobs.xml");
		var i_file=new File(dir + "items.xml");

		log("loading zone file: " + z_file.name);
		if(!z_file.open("r+")) return false;
		var zone=new XML(z_file.readAll().join()); 
		z_file.close();
		
		zone.directory=<directory>{dir}</directory>;
		var items=load_items(i_file);
		var mobs=load_mobs(m_file);
		
		for(var r in zone.room) {
			var room=zone.room[r];
			for(var m in room.mob) {
				var mob=mobs.mob.(@id == room.mob[m].@id).copy();
				mob.@id=mob_count++;
				mob.@room=room.@id;
				mob.@zone=zone.@id;
				room.mob[m].appendChild(mob.children());
			}
		}
		for (var i in zone..item) {
			zone..item[i]=items.item.(@id == zone..item[i].@id).copy();
		}
		return zone;
}

function load_items(file) {
	if(!file.open("r+")) return false;
	var items=new XML(file.readAll().join());
	file.close();
	return items;
}

function load_mobs(file) {
	if(!file.open("r+")) return false;
	var mobs=new XML(file.readAll().join());
	file.close();
	return mobs;
}

function load_players() {
	var p_list=directory(p_dir + "*.xml");
	players=<players></players>;
	for(var f in p_list) {
		var p_file=new File(p_list[f]);
		if(!p_file.open("r+")) return false;
		log("loading player data: " + p_file.name);
		var player=new XML(p_file.readAll().join());
		p_file.close();
		player.@active=0;
		players.appendChild(player);
	}
}

function load_zones() {
	var z_list=directory(z_dir + "*.*");
	for(var z in z_list) { 
		var zone_dir=z_list[z];
		var zone=load_zone(zone_dir);
		zones[zone.@id]=zone;
	}
}

function load_classes() {
	if(!c_file.open("r+")) return false;
	writeln("loading class data: " + c_file.name);
	classes=new XML(c_file.readAll().join());
	c_file.close();
}

function load_races() {
	if(!r_file.open("r+")) return false;
	writeln("loading race data: " + r_file.name);
	races=new XML(r_file.readAll().join());
	r_file.close();
}