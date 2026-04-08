/* editor display functions */
function list_exits_verbose(srv,player,map) {
	var target=player.@channel;
	var room=map.room.(@id == player.@room);
	var exits=["north","south","east","west","up","down"];
	for each(var e in exits) {
		var exit=room.exit.(@name == e);
		if(exit.@target != undefined) {
			var exit_str=blue + "[" + e +"]";
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

				var exit_str=blue + "[" + e +"]";
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
	var count=0;
	for each(var m in list.mob) {
		srv.o(target,
			blue + "[mob]" +  
			black + " id:" + blue + " " + m.@id +
			black + " name:" + blue + " " + m.@name);
		count++;
	}
	if(count == 0) {
		srv.o(target,red + "there are no mobs to list.");
	}
}

function list_items_verbose(srv,target,list) {
	var count=0;
	for each(var i in list.item) {
		srv.o(target,
			blue + "[item]" +  
			black + " id:" + blue + " " + i.@id +
			black + " name:" + blue + " " + i.title);
		count++;
	}
	if(count == 0) {
		srv.o(target,red + "there are no items to list.");
	}
}

function show_room_details(srv,player,map) {
	var room=map.room.(@id == player.@room);
	srv.o(player.@channel,"editing '" + blue + map.@name + black + 
		"' [z:" + blue + " " + map.@id + black + 
		" r:" + blue + " " + room.@id + black + "]");
	srv.o(player.@channel,blue + "[title]" + black + " " + room.title);
	srv.o(player.@channel,blue + "[desc]" + black + " " + strip_ctrl(room.description));
}

function show_item_details(srv,target,zone,id) {
}

function show_mob_details(srv,target,zone,id) {
}

function show_zone_details(srv,target,zone,id) {
	
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
		srv.o(target,"releasing mob:" + blue + " " + editor.mob);
		editor.mob=undefined;
	}
	if(editor.item != undefined) {
		srv.o(target,"releasing item:" + blue + " " + editor.item);
		editor.item=undefined;
	}
	if(editor.room != undefined) {
		srv.o(target,"releasing room:" + blue + " " + editor.room);
		editor.room=undefined;
	}
	
	editor.room=id;
	player.@editing="room";
	player.@room=id;
	show_room_details(srv,player,zone.map);
	list_exits_verbose(srv,player,zone.map);
}

function init_item_edit(srv,target,zone,player,id) {
	var editor=zone.editor[player.@name];
	
	if(editor.mob != undefined) {
		srv.o(target,"releasing mob:" + blue + " " + editor.mob);
		editor.mob=undefined;
	}
	if(editor.item != undefined) {
		srv.o(target,"releasing item:" + blue + " " + editor.item);
	}
	
	editor.item=id;
	player.@editing="item";
	var item=zone.items.item.(@id == id);
	
	srv.o(target,"editing item '" + item.title + black + "' id:" + blue + " " + id);
}

function init_zone_edit(srv,target,zone,player,id) {
	if(player.@zone != id) {
		srv.o(target,"releasing zone:" + blue + " " + player.@zone);
		delete zone.editor[player.@name];
	}
	player.@editing="zone";
	player.@zone=id;
	player.@room=0;
	
	zone=zones[id];
	
	show_zone_details(srv,player,zone);
	show_room_details(srv,player,zone.map);
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
	var mob=zone.mobs.mob.(@id == id);
	
	srv.o(target,"editing mob '" + mob.@name + black + "' id:" + blue + " " + id);
}

function init_exit_edit(srv,target,zone,player,id) {
	var editor=zone.editor[player.@name];
	
	if(editor.mob != undefined) {
		srv.o(target,"releasing mob:" + blue + " " + editor.mob);
		editor.mob=undefined;
	}
	if(editor.item != undefined) {
		srv.o(target,"releasing item:" + blue + " " + editor.item);
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
	case "title":
		item.title=cmd.join(" ");
		srv.o(target,green + "current item title set to: " + item.title);
		break;
	case "appearance":
	case "app":
		item.appearance=cmd.join(" ");
		srv.o(target,green + "current item appearance set to: " + item.appearance);
		break;
	case "description":
	case "desc":
		item.description=cmd.join(" ");
		srv.o(target,green + "current item description set to: " + item.description);
		break;
	case "keywords":
		item.keywords=cmd.join(" ");
		srv.o(target,green + "current item keywords set to: " + item.keywords);
		break;
	case "type":
		item.type=cmd.join(" ");
		srv.o(target,green + "current item type set to: " + item.type);
		break;
	case "weight":
	case "wt":
		item.weight=cmd.join(" ");
		srv.o(target,green + "current item weight set to: " + item.weight);
		break;
	}
}

function edit_mob_data(srv,target,zone,player,set,cmd) {
	var mob_id=zone.editor[player.@name].item;
	var mob=zone.mobs.mob.(@id == mob_id);
	
	switch(set.toLowerCase()) {
	case "name":
		mob.@name=cmd.join(" ");
		srv.o(target,green + "current mob name set to: " + mob.@name);
		break;
	case "appearance":
	case "app":
		mob.appearance=cmd.join(" ");
		srv.o(target,green + "current mob appearance set to: " + mob.appearance);
		break;
	case "keywords":
		mob.keywords=cmd.join(" ");
		srv.o(target,green + "current mob keywords set to: " + mob.keywords);
		break;
	case "hitpoints":
	case "hp":
		mob.hitpoints=cmd.join(" ");
		srv.o(target,green + "current mob hitpoints set to: " + mob.hitpoints);
		break;
	}
}

/* object creation functions */
function create_new_object(zone,map,player,object) {
	switch(object.toUpperCase()) {
	case "ITEM":
		var item_id=get_new_object_id(zone.items.item);
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
		var mob_id=get_new_object_id(zone.mobs.mob);
		var mob=
			<mob id={mob_id} name={settings.default_mob_name}>
				<keywords>{settings.default_mob_keywords}</keywords>
				<appearance>{settings.default_mob_appearance}</appearance>
				<hitpoints>{settings.default_mob_hitpoints}</hitpoints>
			</mob>;
		if(zone.mobs.mob.length() > 0) zone.mobs.mob += mob;
		else zone.mobs=<mobs>{mob}</mobs>;
		return mob_id;
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
				<></>,
			editor:[]
		}
		zones[zone_id]=zone;
		return zone_id;
	}
	return -1;
}

function create_new_room(srv,target,map,room,player,dir) {
	var new_id=get_new_object_id(map.room);
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
	list_exits_verbose(srv,player,zone.map);
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

function get_new_object_id(list) {
	var id = 0;
	var i = list.(@id == id);
	while(i != undefined) {	
		id++;
		i=list.(@id == id);
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