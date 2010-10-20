function handle_command(srv,cmd,player,target) {
	var zone=zones[player.@zone];
	var map=zone.map;
	var room=map.room.(@id == player.@room);
	
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
	
	if(player.@editing == 1) {
		if(Editor_Commands[cmd[0]]) {
			Editor_Commands[cmd[0]](srv,target,map,room,cmd,player);
		} else if(RPG_Commands[cmd[0]] ||
			Battle_Commands[cmd[0]] ||
			Item_Commands[cmd[0]]) {
			srv.o(target,"You can't do that while editing.");
		}
		return;
	}
	if(RPG_Commands[cmd[0]]) {
		RPG_Commands[cmd[0]](srv,target,map,room,cmd,player);
		return;
	} 
	if(Battle_Commands[cmd[0]]) {
		Battle_Commands[cmd[0]](srv,target,map,room,cmd,player);
		return;
	}
	if(Item_Commands[cmd[0]]) {
		Item_Commands[cmd[0]](srv,target,map,room,cmd,player);
		return;
	}
	if(Editor_Commands[cmd[0]]) {
		srv.o(target,"Type 'rpg edit' to enable editing.");
		return;
	}
	
	for(var m in room.mob) {
		var mob=room.mob[m];
		for(var t in mob.trigger) {
			if(cmd.join().match(mob.trigger[t].@id.toUpperCase())) {
				srv.o(target,mob.@name + " says, '" + mob.trigger[t].response + "'");
			}
		}
	}
}

/*	game activity functions */
function display_room(srv,player,z,r) {
	var zone=zones[z];
	var map=zone.map;
	var room=map.room.(@id == r);
	
	show_room_title(srv,player.@channel,map,room);
	show_room_description(srv,player.@channel,map,room);
	show_room_exits(srv,player.@channel,map,room);
	show_room_items(srv,player.@channel,map,room);
	show_room_mobs(srv,player.@channel,map,room);
	show_room_players(srv,player,map,room);
}

function show_room_description(srv,target,map,room) {
	srv.o(target,strip_ctrl(room.description));
}

function show_room_title(srv,target,map,room) {
	srv.o(target,room.title);
}

function show_room_items(srv,target,map,room) {
	for each(var i in room.item) {
		srv.o(target,i.appearance);
	}
}

function show_room_mobs(srv,target,map,room) {
	for each(var m in room.mob) {
		if(room.mob[m].@status != 0) srv.o(target,m.appearance);
	}
}

function show_room_players(srv,player,map,room) {
	for each(var p in players.player.(
		@active == 1 && 
		@room == room.@id && 
		@name != player.@name && 
		@zone == map.@id )) 	{
		srv.o(player.@channel,p.@name + " is here.");
	}
}

function show_room_exits(srv,target,map,room) {
	srv.o(target,get_exit_string(map,room));
}

function get_exit_string(map,room) {
	var exit_str="";
	var exits=room.exit.(@target != "");
	for(var e in exits) {
		var prefix="";
		if(exits[e].door) {
			var door=map.door.(@id == exits[e].door);
			if(door.locked==true) prefix="@";
			else if(door.open==false) prefix="#";
		}
		exit_str+=", "+prefix+exits[e].@name;
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

function find_door(map,room,name) {
	for(var e in room.exit) {
		if(room.exit[e].door) {
			var door=map.door.(@id == room.exit[e].door);
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
		if(mob.@status == 0)  continue;
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
	var map=zone.map;
	var room=map.room.(@id == object.@room);
	create_corpse(room,object);
	object.@status=0;
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

/* game init functions */
function load_zone(dir) {
	var z_file=new File(dir + "map.xml");
	var m_file=new File(dir + "mobs.xml");
	var i_file=new File(dir + "items.xml");
	
	var items=load_items(i_file);
	var mobs=load_mobs(m_file);
	var map=load_map(z_file);
	
	for(var r in map.room) {
		var room=map.room[r];
		for(var m in room.mob) {
			room.mob[m]=mobs.mob.(@id == room.mob[m].@id).copy();
			room.mob[m].@id=mob_count++;
			room.mob[m].@room=room.@id;
			room.mob[m].@zone=map.@id;
			room.mob[m].@status=1;
		}
	}
	for (var i in map..item) {
		map..item[i]=items.item.(@id == map..item[i].@id).copy();
	}
	
	var zone={
		map:map,
		items:items,
		mobs:mobs,
		editor:[]
	}
	
	return zone;
}

function load_map(file) {
	if(!file.open("r+")) return false;
	var zone=new XML(file.read()); 
	file.close();
	return zone;
}

function load_items(file) {
	if(!file.open("r+")) return false;
	var items=new XML(file.read());
	file.close();
	return items;
}

function load_mobs(file) {
	if(!file.open("r+")) return false;
	var mobs=new XML(file.read());
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
		var player=new XML(p_file.read());
		p_file.close();
		player.@active=0;
		player.@editing=0;
		if(!zones[player.@zone]) {
			player.@zone=0;
			player.@room=1;
		}
		players.appendChild(player);
	}
}

function load_zones() {
	var z_list=directory(z_dir + "*.*");
	for(var z in z_list) { 
		var zone_dir=z_list[z];
		var zone=load_zone(zone_dir);
		zones[zone.map.@id]=zone;
	}
}

function load_classes() {
	if(!c_file.open("r+")) return false;
	writeln("loading class data: " + c_file.name);
	classes=new XML(c_file.read());
	c_file.close();
}

function load_races() {
	if(!r_file.open("r+")) return false;
	writeln("loading race data: " + r_file.name);
	races=new XML(r_file.read());
	r_file.close();
}

/* editor display functions */
function list_room_verbose(srv,player,map) {
	var room=map.room.(@id == player.@room);
	srv.o(player.@channel,"editing '" + blue + map.@name + black + 
		"' [z:" + blue + " " + map.@id + black + 
		" r:" + blue + " " + room.@id + black + "]");
}

function list_exits_verbose(srv,player,map) {
	var target=player.@channel;
	var room=map.room.(@id == player.@room);
	var exits=["north","south","east","west","up","down"];
	for each(var e in exits) {
		var exit=room.exit.(@name == e);
		if(exit.@target != undefined) {
			var exit_str=red + "[" + e +"]";
			if(zones[exit.@zone]) exit_str+= black + " z:" + blue + " " + exit.@zone;
			exit_str+= black + " r:" + blue + " " + exit.@target;
			if(exit.door != undefined) {
				var door=map.door.(@id == exit.door);
				exit_str+= black + " door:" + blue + " " + door.@id;
				exit_str+= black + " lock:" + blue + " " + door.locked;
				exit_str+= black + " open:" + blue + " " + door.open;
			}
			srv.o(target,exit_str);
		} else {
			var start=new Coords(0,0,0);
			var finish=get_exit_coords(start,e);
			
			result=find_link(map,room,start,finish,undefined);
			
			if(result >= 0) {
				var target_room=map.room.(@id == result);
				var exit=target_room.exit.(@name == reverse_dir(e));

				var exit_str=red + "[" + e +"]";
				exit_str+= black + " r:" + blue + " " + target_room.@id;
				
				if(exit.door != undefined) {
					var door=map.door.(@id == exit.door);
					exit_str+= black + " door:" + blue + " " + door.@id;
					exit_str+= black + " lock:" + blue + " " + door.locked;
					exit_str+= black + " open:" + blue + " " + door.open;
				}
				exit_str+= red + " (unlinked)";
				srv.o(target,exit_str);
			}
		}
	}
}

function list_mobs_verbose(srv,target,list) {
	for each(var m in list.mob) {
		srv.o(target,
			red + "[mob]" +  
			black + " id:" + blue + " " + m.@id +
			black + " name:" + blue + " " + m.@name);
	}
}

function list_items_verbose(srv,target,list) {
	for each(var i in list.item) {
		srv.o(target,
			red + "[item]" +  
			black + " id:" + blue + " " + i.@id +
			black + " name:" + blue + " " + i.title);
	}
}

/* object display functions */
function show_item_details(srv,target,zone,id) {
	var item=zone.items.item.(@id == id);
	srv.o(target,"editing item '" + item.title + black + "' id:" + blue + " " + id);
}

function show_mob_details(srv,target,zone,id) {
	var mob=zone.mobs.mob.(@id == id);
	srv.o(target,"editing mob '" + mob.@name + black + "' id:" + blue + " " + id);
}

function show_exit_details(srv,target,map,room,id) {
	var exit=room.exit.(@name == id);
	
	if(exit.@target != undefined) {
		srv.o(target,"editing exit '" + blue + exit.@name + black + "'");
		if(zones[exit.@zone]) 
			srv.o(target,"zone:" + blue + " " + exit.@zone);
		srv.o(target,"room:" + blue + " " + exit.@target);
		if(exit.door != undefined) {
			var door=map.door.(@id == exit.door);
			srv.o(target,black + " door:" + blue + " " + door.@id
				+ black + " lock:" + blue + " " + door.locked
				+ black + " open:" + blue + " " + door.open);
		}
	} else {
		srv.o(target,"editing exit '" + blue + exit.@name + black + "'" + red + " (unlinked)");

		var start=new Coords(0,0,0);
		var finish=get_exit_coords(start,e);
		var result=find_link(map,room,start,finish,undefined);
		
		if(result >= 0) {
			var target_room=map.room.(@id == result);
			var exit=target_room.exit.(@name == reverse_dir(e));

			srv.o(target,"room:" + blue + " " + target_room.@id);
			if(exit.door != undefined) {
				var door=map.door.(@id == exit.door);
				srv.o(target,black + " door:" + blue + " " + door.@id
					+ black + " lock:" + blue + " " + door.locked
					+ black + " open:" + blue + " " + door.open);
			}
			srv.o(target,exit_str);
		}
	}
}

/*  editor initialization functions */
function init_room_edit(srv,target,zone,player,id) {
	var editor=zone.editor[player.@name];
	
	if(editor.mob != undefined) {
		srv.o(target,"unlocking mob:" + blue + " " + editor.mob);
		editor.mob=undefined;
	}
	if(editor.item != undefined) {
		srv.o(target,"unlocking item:" + blue + " " + editor.item);
		editor.item=undefined;
	}
	if(editor.room != undefined) {
		srv.o(target,"unlocking room:" + blue + " " + editor.room);
		editor.room=undefined;
	}
	
	editor.room=id;
	player.@editing="room";
	player.@room=id;
	list_room_verbose(srv,player,zone.map);
}

function init_item_edit(srv,target,zone,player,id) {
	var editor=zone.editor[player.@name];
	
	if(editor.mob != undefined) {
		srv.o(target,"unlocking mob:" + blue + " " + editor.mob);
		editor.mob=undefined;
	}
	if(editor.item != undefined) {
		srv.o(target,"unlocking item:" + blue + " " + editor.item);
	}
	
	editor.item=id;
	player.@editing="item";
	show_item_details(srv,target,zone,id);
}

function init_zone_edit(srv,target,zone,player,id) {
	if(player.@zone != id) {
		srv.o(target,"unlocking zone:" + blue + " " + player.@zone);
		delete zone.editor[player.@name];
	}
	player.@editing="zone";
	player.@zone=id;
	player.@room=0;
	list_zone_verbose(srv,player,zone);
}

function init_mob_edit(srv,target,zone,player,id) {
	var editor=zone.editor[player.@name];
	if(editor.item != undefined) {
		srv.o(target,"releasing item:" + blue + " " + editor.item);
		editor.item=undefined;
	}
	if(editor.mob != undefined) {
		srv.o(target,"releasing mob:" + blue + " " + editor.mob);
	}
	
	editor.mob=id;
	player.@editing="mob";
	show_mob_details(srv,target,zone,id);
}

function init_exit_edit(srv,target,zone,player,id) {
	var editor=zone.editor[player.@name];
	
	if(editor.mob != undefined) {
		srv.o(target,"unlocking mob:" + blue + " " + editor.mob);
		editor.mob=undefined;
	}
	if(editor.item != undefined) {
		srv.o(target,"unlocking item:" + blue + " " + editor.item);
		editor.item=undefined;
	}

	editor.exit=id;
	player.@editing="exit";
	show_exit_details(srv,target,zone.map,zone.map.room.(@id == player.@room),id);
}

/* data editor functions */
function edit_room_data(srv,target,zone,player,set,cmd) {
	var room=zone.map.room.(@id == player.@room);
	
	switch(set.toLowerCase()) {
	case "default":
	case "def":
		switch(cmd.shift().toLowerCase()) {
		case "title":
			settings.default_room_title=cmd.join(" ");
			srv.o(target,green + "default room title set to: " + settings.default_room_title);
			break;
		case "description":
		case "desc":
			settings.default_room_description=cmd.join(" ");
			srv.o(target,green + "default room description set to: " + settings.default_room_description);
			break;
		default:
			srv.o(target,"usage: set default <property> <value>");
			break;
		}
		break;
	case "title":
		room.title=cmd.join(" ");
		srv.o(target,green + "current room title set to: " + room.title);
		break;
	case "description":
	case "desc":
		room.description=cmd.join(" ");
		srv.o(target,green + "current room description set to: " + room.description);
		break;
	default:
		srv.o(target,"usage: set <property> <value>, set default <property> <value>");
		break;
	}
}

function edit_item_data(srv,target,zone,player,set,cmd) {
	var item_id=zone.editor[player.@name].item;
	var item=zone.items.item.(@id == item_id);
	
	switch(set.toLowerCase()) {
	}
}

/* object creation functions */
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
				{global_items.item.(@id == "1").copy()}
				{global_items.item.(@id == "2").copy()}
				{global_items.item.(@id == "3").copy()}
				{global_items.item.(@id == "4").copy()}
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

function create_new_object(zone,map,player,object) {
	switch(object.toUpperCase()) {
	case "ITEM":
		var item_id=get_new_item_id(zone.items);
		var item=
			<item id={item_id}>
				<type>{settings.default_item_type}</type>
				<title>{settings.default_item_title}</title>
				<keywords>{settings.default_item_keywords}</keywords>
				<description>{settings.default_item_description}</description>
				<appearance>{settings.default_item_appearance}</appearance>
				<weight>{settings.default_item_weight}</weight>
			</item>;
		if(zone.items.item.length() > 0) zone.items.item += item;
		else zone.items=<items>{item}</items>;
		return item_id;
	case "MOB":
		break;
	case "ZONE":
		var zone_id=get_new_zone_id();
		var zone={
			map:
				<zone name={settings.default_zone_title} id={zone_id} level={settings.default_zone_level}>
					<room id="0">
						<title>{settings.default_room_title}</title>
						<description>{settings.default_room_description}</description>
					</room>
				</zone>,
			items:
				<></>,
			mobs:
				<></>
		}
		zones[zone_id]=zone;
		srv.o(target,green + "zone '" + zone_title + "' created.");
		return zone_id;
	}
	return -1;
}

function create_new_room(srv,target,map,room,player,dir) {
	var new_id=get_new_room_id(map);
	var old_id=room.@id;
	/* add this room to previous room's exit list */
	room.exit +=<exit name={dir} target={new_id}/>;
	
	/* add new room data to zone */
	map.room += 
		<room id={new_id}>
			<title>{settings.default_room_title}</title>
			<description>{settings.default_room_description}</description>
		</room>;
		
	/* get direction we came from and add that exit to new room */
	dir=reverse_dir(dir);
	var new_room=map.room.(@id == new_id);
	new_room.exit += <exit name={dir} target={old_id}/>;
	
	if(settings.autolink) {
		var dirs=get_dir_coords(dir);
		for(var d in dirs) {
			if(!dirs[d]) continue;
			var result=find_link(map,new_room,undefined,dirs[d],undefined);
			if(result >= 0) {
				/* add link to found exit */
				new_room.exit += <exit name={d} target={result}/>;
				/* add corresponding exit for target room */
				var target_room=map.room.(@id == result);
				target_room.exit += <exit name={reverse_dir(d)} target = {new_id}/>;
			}
		}
	} 
	
	/* move player to new room */
	player.@room=new_id;
	srv.o(target,"Room " + new_id + " created");
	list_room_verbose(srv,player,map,new_room);
}

function get_dir_coords(exclude) {
	var dirs={
		north:new Coords(0,1,0),
		south:new Coords(0,-1,0),
		east:new Coords(1,0,0),
		west:new Coords(-1,0,0),
		up:new Coords(0,0,1),
		down:new Coords(0,0,-1),
	}
	if(exclude) delete dirs[exclude];
	return dirs;
}

/* editor data functions */
function find_link(map,room,start_coords,finish_coords,checked) {
	

	/* cumulative array for tracking scanned rooms (by room.@id) */
	checked=checked || [];
	if(checked[room.@id]) return -1;
	checked[room.@id] = true;
	
	
	if(!start_coords) start_coords=new Coords(0,0,0);
	else {
		/* if the passed starting coordinates match our query, return the current room id */
		if(start_coords.x == finish_coords.x &&
			start_coords.y == finish_coords.y &&
			start_coords.z == finish_coords.z) {
			return room.@id;
		}
	}
	/* store all room exits in an array */
	var dirs=[];
	for(var e=0;e<room.exit.length();e++) {
		if(room.exit[e].@target == "" || zones[room.exit[e].@zone]) continue;
		
		var dir=room.exit[e].@name;
		var target_id=room.exit[e].@target;
		var new_coords=get_exit_coords(start_coords,dir);
		var target=map.room.(@id == target_id);
		
		/* do recursive scan on all rooms */
		var result=find_link(map,target,new_coords,finish_coords,checked);
		if(result >= 0) return result;
	}
	return -1;
}

function get_exit_coords(coords,dir) {
	var new_coords=new Coords(coords.x,coords.y,coords.z);
	switch(dir.toString()) {
	case "north":
		new_coords.y++;
		break;
	case "south":
		new_coords.y--;
		break;
	case "east":
		new_coords.x++;
		break;
	case "west":
		new_coords.x--;
		break;
	case "up":
		new_coords.z++;
		break;
	case "down":
		new_coords.z--;
		break;
	default:
		log("unknown dir: " + dir);
		break;
	}
	return new_coords;
}

function reverse_dir(dir) {
	if(dir=="north") dir="south";
	else if(dir=="south") dir="north";
	else if(dir=="east") dir="west";
	else if(dir=="west") dir="east";
	else if(dir=="up") dir="down";
	else if(dir=="down") dir="up";
	return dir;
}

function get_new_room_id(map) {
	var id = 0;
	var r = map.room.(@id == id);
	while(r != undefined) {	
		id++;
		r=map.room.(@id == id);
	}
	return id;
}

function get_new_zone_id() {
	var id = 0;
	var z = zones[id];
	while(z != undefined) {	
		id++;
		z=zones[id];
	}
	return id;
}

function get_new_item_id(items) {
	var id = 0;
	var i = items.item.(@id == id);
	while(i.length() > 0) {	
		id++;
		i=items.item.(@id == id);
	}
	return id;
}
