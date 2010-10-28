var Battle_Commands=[];
var Editor_Commands=[];
var Item_Commands=[];
var RPG_Commands=[];

/* MAIN BOT COMMANDS */
Bot_Commands["CREATE"] = new Bot_Command(0,2,false);
Bot_Commands["CREATE"].usage = 
	get_cmd_prefix() + "CREATE HELP";
Bot_Commands["CREATE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var current_character=players.player.(@name == onick);
	if(current_character.length() > 0) {
		srv.o(target,"You already have a character!");
		return;
	}
	
	var player_race=cmd.shift().toLowerCase();
	var player_class=cmd.shift().toLowerCase();
	if(!races.player_race.(@id == player_race) || !classes.player_class.(@id == player_class)) {
		srv.o(target,"You have chosen an invalid class/race");
		return;
	}
	
	var player=create_player(onick,races.player_race.(@id == player_race).@id,classes.player_class.(@id == player_class).@id);
	players.appendChild(player);
	srv.o(target,
		"Congratulations " + onick + ", you are now a level " +
		player.level + " " + player.player_race + " " + player.player_class + "!");
	return;
}

Bot_Commands["CLASSES"] = new Bot_Command(0,false,false);
Bot_Commands["CLASSES"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var list=[];
	for(var c in classes.player_class) {
		list.push(classes.player_class[c].@id);
	}
	srv.o(target,"classes: " + list.join(", "));
}

Bot_Commands["RACES"] = new Bot_Command(0,false,false);
Bot_Commands["RACES"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var list=[];
	for(var r in races.player_race) {
		list.push(races.player_race[r].@id);
	}
	srv.o(target,"races: " + list.join(", "));
}

Bot_Commands["ZONES"] = new Bot_Command(0,false,false);
Bot_Commands["ZONES"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var list=[];
	for(var z in zones) {
		list.push(zones[z].map.@name);
	}
	srv.o(target,"Zones: " + list.join(", "));
}

Bot_Commands["LOGIN"] = new Bot_Command(0,false,false);
Bot_Commands["LOGIN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var player=players.player.(@name == onick);
	if(!player.length() > 0) {
		srv.o(target,"You have not created a character.");
		return;
	}
	if(player.@active == 1) {
		if(player.@channel == target) {
			srv.o(target,"You are already logged in!");
			return;
		}
		player.@channel=target;
		srv.o(target,"You are now active in this channel.");
		display_room(srv,player,player.@zone,player.@room);
		return;
	}
	

	if(!zones[player.@zone]) {
		player.@zone=0;
	}
	var zone=zones[player.@zone];
	var map=zone.map;

	if(!map.room.(@id == player.@room).length() > 0) {
		player.@room = 0;
	}
	
	
	player.@active=1;
	player.@channel=target;
	display_room(srv,player,player.@zone,player.@room);
}

Bot_Commands["EDIT"] = new Bot_Command(80,false,false);
Bot_Commands["EDIT"].usage = 
	get_cmd_prefix() + "EDIT HELP";
Bot_Commands["EDIT"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var player=players.player.(@name == onick);
	if(player.@active != 1) {
		srv.o(target,"You are not logged in.");
		return;	
	}

	var zone=zones[player.@zone];
	var map=zone.map;
	var room=map.room.(@id == player.@room);

	cmd.shift();
	var edit_cmd=cmd.shift();
	var value=cmd.shift();
	
	if(!edit_cmd) {
		if(zone.editor[player.@name] == undefined) {
			player.@editing = "room";
			edit_cmd="room";
			value=player.@room;
		} else {
			player.@editing=undefined;
			zone.editor[player.@name]=undefined;
			srv.o(target,"Editor off.");
			return;
		}
	}

	if(!value) {
		switch(edit_cmd) {
		case "room":
			value=player.@room;
			break;
		case "item":
			if(zone.editor[player.@name]) value=zone.editor[player.@name].item;
			break;
		case "mob":
			if(zone.editor[player.@name]) value=zone.editor[player.@name].mob;
			break;
		}
		if(!value) {
			srv.o(target,"see 'help edit' for command usage.","NOTICE");
			return;
		}
	}
	
	edit_cmd=("" + edit_cmd).toLowerCase();
	value=("" + value).toLowerCase();
	
	/* If creating something new, initialize a new default object and continue */
	if(edit_cmd == "new") {
		var index=create_new_object(zone,map,player,value);
		if(index >= 0) {
			srv.o(target, green + value + " " + index + " created.");
			edit_cmd=value;
			value=index;
		} else {
			srv.o(target,"Unable to create object.");
			return;
		}
	}
	
	switch(edit_cmd) {
	case "item":
	case "room":
	case "mob":
	case "zone":
	case "exit":
		for(var e in zone.editor) {
			if(zone.editor[e] && zone.editor[e][edit_cmd] == value) {
				srv.o(target,red + edit_cmd + " " + value + " locked for editing by " + e);
				return;
			}
		}
		if(zone.editor[player.@name] == undefined) {
			zone.editor[player.@name]=new Editor();
		}
		break;
	default:
		srv.o(target,"see 'help edit' for usage.");
		delete zone.editor[player.@name];
		break;
	}
	
	/* set the object as being edited */
	switch(edit_cmd) {
	case "item":
		init_item_edit(srv,target,zone,player,value);
		break;
	case "room":
		init_room_edit(srv,target,zone,player,value);
		break;
	case "mob":
		init_mob_edit(srv,target,zone,player,value);
		break;
	case "zone":
		init_zone_edit(srv,target,zone,player,value);
		break;
	case "exit":
		init_exit_edit(srv,target,zone,player,value);
		break;
	}
	return;
}

Bot_Commands["LOGOUT"] = new Bot_Command(0,false,false);
Bot_Commands["LOGOUT"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var player=players.player.(@name == onick);
	if(!player.length() > 0) {
		srv.o(target,"You have not created a character.");
		return;
	}
	if(player.@active == 0) {
		srv.o(target,"You are not logged in.");
		return;
	}
	player.@active=0;
	srv.o(target,"You have been logged out.");
}

Bot_Commands["SAVE"] = new Bot_Command(80,false,false);
Bot_Commands["SAVE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var player=players.player.(@name == onick);
	if(player.@active != 1) {
		srv.o(target,"You are not logged in.");
		return;	
	}
	if(player.@editing == undefined) {
		srv.o(target,"You must be editing to save.");
		return;
	}
	
	var zone=zones[player.@zone];
	var map=zone.map;
	mkdir(z_dir + map.@name.toLowerCase());
	var z_file=new File(z_dir+map.@name.toLowerCase()+"/map.xml");
	if(!z_file.open("w+")) return;
	z_file.write(map);
	srv.o(target,"Zone saved");
	return;
}

Bot_Commands["HELP"] = new Bot_Command(0,false,false);
Bot_Commands["HELP"].command = function (target,onick,ouh,srv,lvl,cmd) {
	log(cmd);
	cmd.shift();
	var topic=cmd.shift();
	if(!topic) {
		var cmd_str="";
		for(var ec in Editor_Commands) {
			cmd_str+=", " + ec.toLowerCase();
		}
		srv.o(target,"editor commands: " + cmd_str.substr(2),"NOTICE");
		cmd_str="";
		for(var bc in Battle_Commands) {
			cmd_str+=", " + bc.toLowerCase();
		}
		srv.o(target,"battle commands: " + cmd_str.substr(2),"NOTICE");
		cmd_str="";
		for(var ic in Item_Commands) {
			cmd_str+=", " + ic.toLowerCase();
		}
		srv.o(target,"item commands: " + cmd_str.substr(2),"NOTICE");
		cmd_str="";
		for(var rc in RPG_Commands) {
			cmd_str+=", " + rc.toLowerCase();
		}
		srv.o(target,"rpg commands: " + cmd_str.substr(2),"NOTICE");
		srv.o(target,"for detailed command info, type 'rpg help <command>'","NOTICE");
		srv.o(target,"for help creating a characer, type 'rpg help create'","NOTICE");
		srv.o(target,"for help with the editor, type 'rpg help editor'","NOTICE");
		return;
	}
	
	if(Help_Topics[topic.toUpperCase()]) {
		for each(var l in Help_Topics[topic.toUpperCase()])
			srv.o(target,l,"NOTICE");
		return;
	}
	
	srv.o(target,"topic not found","NOTICE");
	return;
}

Bot_Commands["RESTORE"] = new Bot_Command(80,false,false);
Bot_Commands["RESTORE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var player=players.player.(@name == onick);
	if(player.@active != 1) {
		srv.o(target,"You are not logged in.");
		return;	
	}
	if(player.@editing == undefined) {
		srv.o(target,"You must be editing to restore.");
		return;
	}
	var zone=zones[player.@zone];
	var zone_dir=z_dir+map.@name.toLowerCase();
	zone=load_zone(zone_dir);
	if(!zone) {
		srv.o(target,"Error loading zone");
		return;
	}
	zones[zone.map.@id]=zone;
	srv.o(target,"Reverted current zone to saved data");
	return;
}



/* BATTLE COMMANDS */
Battle_Commands["FLEE"] = function (srv,target,map,room,cmd,player) {
	var chance=random(100);
	if(chance<25) {
		srv.o(target,"You couldn't escape!");
		return false;
	}
	var directions=room.exit;
	var random_exit=directions[random(directions.length())];
	
	if(random_exit.door) {
		var door=map.door.(@id == random_exit.door);
		if(door.open==false) {
			srv.o(target,"You couldn't escape!");
			return false;
		}
	}
	
	delete (player_attacks[player.@name]);
	
	srv.o(target,"You flee from battle!");
	if(zones[random_exit.@zone]) player.@zone=<zone>{random_exit.@zone}</zone>;
	player.@room=<room>{random_exit.@target}</room>;
	display_room(srv,player,player.@zone,player.@room);
	return;
}

Battle_Commands["KILL"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var obj=cmd.shift();
	if(!obj) {
		srv.o(target,"Kill whom?");
		return false;
	}
	
	var mob_index=find_mob(room,obj);
	if(!mob_index) {
		srv.o(target,"They're not here!");
		return false;
	}
	var mob=room.mob[mob_index];
	add_player_attack(player,mob);
	add_mob_attack(mob,player);
}



/* EDITOR COMMANDS */
Editor_Commands["SET"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var set=cmd.shift();
	if(!set) {
		srv.o(target,"Set what?");
		return;
	}
	
	switch(set.toLowerCase()) {
	case "autolink":
		if(settings.autolink) {
			settings.autolink=false;
			srv.o(target,red + "Auto-link disabled.");
		} else {
			settings.autolink=true;
			srv.o(target,green + "Auto-link enabled.");
		}
		return;
	}
	
	var zone=zones[player.@zone];
	switch(player.@editing.toString()) {
	case "room":
		edit_room_data(srv,target,zone,player,set,cmd);
		break;
	case "item":
		edit_item_data(srv,target,zone,player,set,cmd);
		break;
	case "mob":
		edit_mob_data(srv,target,zone,player,set,cmd);
		break;
	case "exit":
		edit_exit_data(srv,target,zone,player,set,cmd);
		break;
	case "zone":
		edit_zone_data(srv,target,zone,player,set,cmd);
		break;
	default:
		log("unknown edit mode: '" + player.@editing + "'");
		break;
	}
}

Editor_Commands["LINK"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var link=cmd.shift();
	if(!link) {
		srv.o(target,"Type 'help link' for help");
		return;
	}
	
	switch(link.toLowerCase()) {
	case "north":
	case "south":
	case "east":
	case "west":
	case "up":
	case "down":
		var room_id=cmd.shift();
		if(!room_id) {
			var start=new Coords(0,0,0);
			var finish=get_exit_coords(start,link.toLowerCase());
			room_id=find_link(map,room,start,finish,undefined);
			if(room_id == -1) {
				srv.o(target,red + "There is no unlinked exit there");
				return;
			}

			var target_room=map.room.(@id == room_id);
			var exit=room.exit.(@name == link.toLowerCase());
			
			/* set exit target for current room */
			if(exit.@target.toString()=="") {
				room.exit += <exit name={link.toLowerCase()} target={room_id}/>;
				srv.o(target,green + "Room exit added");
				/* change corresponding exit for target room */
				if(settings.autolink) 
					target_room.exit.(@name == reverse_dir(link.toLowerCase())).(@target = room.@id);
			} else {
				srv.o(target,red + "That exit is already linked");
			}
			
		} else {
			var target_room=map.room.(@id == room_id);
			var exit=room.exit.(@name == link.toLowerCase());
			
			/* set exit target for current room */
			if(exit.@target.toString()=="") {
				room.exit += <exit name={link.toLowerCase()} target={room_id}/>;
				srv.o(target,green + "Room exit added");

			/* change exit target for current room */
			} else {
				exit.@target=room_id;
				srv.o(target,green + "Room target changed");
			}
			
			/* change corresponding exit for target room */
			if(settings.autolink) 
				target_room.exit.(@name == reverse_dir(link.toLowerCase())).(@target = room.@id);
		}
		break;
	default:
		srv.o(target,"usage: link <direction> <target>");
		break;
	}
}

Editor_Commands["UNLINK"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var link=cmd.shift();
	if(!link) {
		srv.o(target,"Type 'help unlink' for help");
		return;
	}

	switch(link.toLowerCase()) {
	case "north":
	case "south":
	case "east":
	case "west":
	case "up":
	case "down":
		target_id=room.exit.(@name == link.toLowerCase()).@target;
		target_room=map.room.(@id == target_id);

		/* clear link if no target specified */
		room.exit.(@name == link.toLowerCase()).@target=undefined;
		srv.o(target,red + "Room exit deleted");

		/* clear corresponding link in original target room */
		if(settings.autolink) 
			target_room.exit.(@name == reverse_dir(link.toLowerCase())).@target=undefined;
		break;
	default:
		srv.o(target,"usage: unlink <direction>");
		break;
	}
}

Editor_Commands["MOVE"] = function (srv,target,map,room,cmd,player) {
	var exit=room.exit.(@name == cmd[1].toLowerCase());
	if(exit.@target.toString()=="") {
		create_new_room(srv,target,map,room,player,cmd[1].toLowerCase());
		return true;
	}
	
	if(zones[exit.@zone]) player.@zone=<zone>{exit.@zone}</zone>;
	player.@room=<room>{exit.@target}</room>;

	zone=zones[player.@zone];
	room=map.room.(@id == player.@room);
	
	list_room_verbose(srv,player,map,room);
	list_exits_verbose(srv,player,zone.map);
}

Editor_Commands["MOBS"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var param=cmd.shift();
	if(param && param.toUpperCase() == "ZONE") {
		var zone=zones[player.@zone];
		list_mobs_verbose(srv,target,zone.mobs);
		return;
	}
	list_mobs_verbose(srv,target,room);
	return;
}

Editor_Commands["ITEMS"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var param=cmd.shift();
	if(param && param.toUpperCase() == "ZONE") {
		var zone=zones[player.@zone];
		list_items_verbose(srv,target,zone.items);
		return;
	}
	list_items_verbose(srv,target,room);
	return;
}

Editor_Commands["EXITS"] = function (srv,target,map,room,cmd,player) {
	list_exits_verbose(srv,player,map);
}

Editor_Commands["TITLE"] = function (srv,target,map,room,cmd,player) {
	srv.o(target,"[title] " + room.title);
}

Editor_Commands["DESC"] = function (srv,target,map,room,cmd,player) {
	srv.o(target,"[desc] " + strip_ctrl(room.description));
}

Editor_Commands["GOTO"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var dest=cmd.shift();
	if(!dest) {
		srv.o(target,"Where do you want to go?");
		return false;
	}
	var room=cmd.shift();
	if(room) {
		player.@zone=<zone>{dest}</zone>;
		zone=zones[player.@zone];
	} else {
		room=dest;
	}
	player.@room=<room>{room}</room>;
	var room=map.room.(@id == player.@room);
	srv.o(target,"You teleport to " + room.title);
	list_room_verbose(srv,player,map,room);
	list_exits_verbose(srv,player,zone.map);
}

Editor_Commands["LOOK"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var obj=cmd.shift();
	
	/*	if no target specified, look at room */
	if(!obj) {
		show_room_details(srv,player,map);
		list_mobs_verbose(srv,player.@channel,map,room);
		list_items_verbose(srv,player.@channel,map,room);
		list_exits_verbose(srv,player,map);
		return;
	}
	
	/*	if the second word is "in", attempt to look inside a container */
	if(obj.toLowerCase()=="in") {
		var cont=cmd.shift();
		if(!cont) {
			srv.o(target,"Look in what?");
			return false;
		}
		
		var container=false;
		var container_index=find_object(player.inventory,cont);
		if(container_index) container=player.inventory.item[container_index];
		
		/*	attampt to match specified container with room items */
		else container_index=find_object(room,cont); 
		if(!container && container_index) container=room.item[container_index];
		
		/*	if player has this container, check items in container for specified object */
		if(container) {
			if(container.type!="container") {
				srv.o(target,"That isn't a container!");
				return false;
			}
			var list=[];
			for each(var i in container.item) {
				list.push(i.title);
			}
			var str=list.length?list.join(", "):"nothing";
			srv.o(target,"contents (" + container.title + "):" + str);
			return true;
		}
		srv.o(target,"You can't find it!");
		return false;
	}
	
	/*	handle looking at items, mobs, doors */
	var mob_index=find_mob(room,obj);
	if(mob_index) {
		var mob=room.mob[mob_index];
		if(mob.description.length() > 0) srv.o(target,mob.description);
		var list=[];
		var eq=mob.equipment..item;
		for each(var i in eq) {
			list.push("<" + i.parent().@id + "> " + i.title);
		}
		var str=list.length?list.join(", "):"nothing";
		srv.o(target,"eq: " + str);
		return true;
	}
	
	var item_index=find_object(room,obj);
	if(item_index) {
		var item=room.item[item_index];
		srv.o(target,item.description);
		return true;
	}
	
	srv.o(target,"You don't see that here.");
	return false;
}



/* ITEM COMMANDS */
Item_Commands["DROP"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var obj=cmd.shift();
	if(!obj) {
		srv.o(target,"Drop what?");
		return false;
	}
	
	if(obj.toLowerCase() == "all") {
		while(player.inventory.item.length()>0) {
			var move=move_by_index(player.inventory,room,0);
			if(move.result == true) {
				srv.o(target,"You drop " + move.item.title);
			} else {
				srv.o(target,"You couldn't let go of " + move.item.title);
				return true;
			}
		}
		return true;
	}
	
	var move=move_by_name(player.inventory,room,obj);
	if(move.result == true) {
		srv.o(target,"You drop " + move.item.title);
		return true;
	}

	srv.o(target,"You don't have it!");
	return false;
}

Item_Commands["GET"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var obj=cmd.shift();
	if(!obj) {
		srv.o(target,"Get what?");
		return false;
	}
	var cont=cmd.shift();
	
	/*	if no container has been specified */
	if(!cont) {

		if(obj.toLowerCase() == "all") {
			while(room.item.length()>0) {
				var move=move_by_index(room,player.inventory,0);
				if(move.result == true) {
					srv.o(target,"You get " + move.item.title);
				} else {
					srv.o(target,"Your inventory is full");
					return true;
				}
			}
			return true;
		}
		/*	look for items in the room */
		var move=move_by_name(room,player.inventory,obj);
		if(move.result == true) {
			srv.o(target,"You get " + move.item.title);
			return true;
		}
	} else {
		/*	attampt to match specified container with inventory */
		var container=false;
		var container_index=find_object(player.inventory,cont);
		if(container_index) container=player.inventory.item[container_index];
		
		/*	attampt to match specified container with room items */
		else container_index=find_object(room,cont); 
		if(!container && container_index) container=room.item[container_index];
		
		/*	if player has this container, check items in container for specified object */
		if(container) {
			if(container.type!="container") {
				srv.o(target,"That isn't a container!");
				return false;
			}
			
			if(obj.toLowerCase() == "all") {
				while(container.item.length() > 0) {
					var move=move_by_index(container,player.inventory,0);
					if(move.result == true) {
						srv.o(target,"You get " + move.item.title + " from " + container.title);
					} else {
						srv.o(target,"Your inventory is full");
						return true;
					}
				}
				return true;
			}
			
			var move=move_by_name(container,player.inventory,obj);
			if(move.result == true) {
				srv.o(target,"You get " + move.item.title + " from " + container.title);
				return true;
			}
		} 
	}
	
	srv.o(target,"You can't find it!");
	return false;
}

Item_Commands["PUT"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var obj=cmd.shift();
	if(!obj) {
		srv.o(target,"Put what?");
		return false;
	}

	var cont=cmd.shift();
	if(!cont) {
		srv.o(target,"Put it where?");
		return false;
	} else {
		/*	attampt to match specified container with inventory */
		var container=false;
		var container_index=find_object(player.inventory,cont);
		if(container_index) container=player.inventory.item[container_index];
		
		/*	attampt to match specified container with room items */
		else container_index=find_object(room,cont); 
		if(!container && container_index) container=room.item[container_index];
		
		/*	if player has this container, check items in container for specified object */
		if(container) {
			if(container.type!="container") {
				srv.o(target,"That isn't a container!");
				return false;
			}
			
			if(obj.toLowerCase() == "all") {
				while(player.inventory.item.length()>0) {
					var move=move_by_index(player.inventory,container,0);
					if(move.result == true) {
						srv.o(target,"You put " + move.item.title + " in " + container.title);
					} else {
						srv.o(target,container.title + " can't hold any more items");
						return true;
					}
				}
				return true;
			}
			
			var move=move_by_name(player.inventory,container,obj);
			if(move.result == true) {
				srv.o(target,"You put " + move.item.title + " in " + container.title);
				return true;
			}
		} else {
			/*	search containers in room */
		}
	}
	
	srv.o(target,"You can't find it!");
	return false;
}

Item_Commands["UNLOCK"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Unlock what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(map,room,name);
	if(door) {
		if(door.locked == false) {
			srv.o(target,"It is already unlocked.");
			return false;
		}
		
		var key=player.inventory.item.(@id == door.key);
		if(!key.length() > 0) {
			srv.o(target,"You don't have the key.");
			return false;
		}
		
		srv.o(target,"You unlock the " + door.@name + ".");
		door.locked=false;
		return true;
		
	} else {
		srv.o(target,"There is no door there.");
		return false;
	}
	
	/* then check containers */
}

Item_Commands["LOCK"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Lock what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(map,room,name);
	if(door) {
		if(door.locked == true) {
			srv.o(target,"It is already locked.");
			return false;
		}
		
		if(door.open == true) {
			srv.o(target,"You should close it first.");
			return false;
		}
		
		var key=player.inventory.item.(@id == door.key);
		if(!key.length() > 0) {
			srv.o(target,"You don't have the key.");
			return false;
		}
		
		srv.o(target,"You lock the " + door.@name + ".");
		door.locked=true;
		return true;
		
	} else {
		srv.o(target,"There is no door there.");
		return false;
	}
	
	/* then check containers */
}

Item_Commands["OPEN"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Open what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(map,room,name);
	if(door) {
		if(door.open == true) {
			srv.o(target,"It is already open.");
			return false;
		}
		if(door.locked == true) {
			srv.o(target,"It is locked.");
			return false;
		}
		
		srv.o(target,"You open the " + door.@name + ".");
		door.open=true;
		return true;
	} else {
		srv.o(target,"There is no door there.");
		return false;
	}

	/*	then check containers in the room */
	
	/*	then check containers in inventory */
}

Item_Commands["CLOSE"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Close what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(map,room,name);
	if(door) {
		if(door.open == false) {
			srv.o(target,"It is already closed.");
			return false;
		}
		
		srv.o(target,"You close the " + door.@name + ".");
		door.open=false;
		return true;
	} else {
		srv.o(target,"There is no door there.");
		return false;
	}

	/*	then check containers in the room */
	
	/*	then check containers in inventory */
}

Item_Commands["REMOVE"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var keyword=cmd.join(" ");
	if(!keyword) {
		srv.o(target,"Remove what?");
		return false;
	}
	
	var container=false;
	var item_index=false;
	for(var s in player.equipment.slot) {
		item_index=find_object(player.equipment.slot[s],keyword);
		if(item_index) {
			container=player.equipment.slot[s];
			break;
		}
	}
	if(!item_index) {
		srv.o(target,"You aren't using it!");
		return false;
	}
	
	var move=move_by_index(container,player.inventory,item_index);
	if(move.result == true) {
		srv.o(target,"You stop using " + move.item.title);
		return true;
	}
}

Item_Commands["WEAR"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var keyword=cmd.join(" ");
	if(!keyword) {
		srv.o(target,"Equip what?");
		return false;
	}
	
	var item_index=find_object(player.inventory,keyword);
	if(!item_index) {
		srv.o(target,"You can't find it!");
		return false;
	}
	
	var item=player.inventory.item[item_index];
	if(item.equip.@id == undefined) {
		srv.o(target,"You can't equip that!");
		return false;
	}
	
	var slot=player.equipment.slot.(@id == item.equip.@id);
	if(slot.@qty == slot.item.length()) {
		srv.o(target,"That equipment slot is not empty!");
		return false;
	}
	
	var move=move_by_index(player.inventory,slot,item_index);
	if(move.result == true) {
		srv.o(target,"You " + get_equip_message(move.item) + " " + move.item.title);
		return true;
	}
}
Item_Commands["EQUIP"] = Item_Commands["WEAR"];
Item_Commands["WIELD"] = Item_Commands["WEAR"];



/* GAMEPLAY COMMANDS */
RPG_Commands["SCORE"] = function (srv,target,map,room,cmd,player) {
	srv.o(target,
		"player: " + player.@name + 
		" level: " + player.level + 
		" race: " + player.player_race +
		" class: " + player.player_class);
	srv.o(target,
		"str: " + player.strength +
		" wis: " + player.wisdom +
		" int: " + player.intelligence +
		" dex: " + player.dexterity +
		" con: " + player.constitution);
	srv.o(target,
		"experience: " + player.experience +
		" movement: " + player.movement + 
		" hitpoints: " + player.hitpoints +
		" mana: " + player.mana + 
		" gold: " + player.gold);
}

RPG_Commands["STATUS"] = function (srv,target,map,room,cmd,player) {
	srv.o(target,"<HP:" + player.hitpoints + " MA:" + player.mana + " MV:" + player.movement + ">");
}

RPG_Commands["MOVE"] = function (srv,target,map,room,cmd,player) {
	var exit=room.exit.(@name == cmd[1].toLowerCase());
	if(exit.@target.toString()=="") {
		srv.o(target,"You cannot go that way.");
		return false;
	}
	
	if(exit.door) {
		var door=map.door.(@id == exit.door);
		if(door.open == false) {
			srv.o(target,"The " + door.@name + " is closed.");
			return false;
		}
	}
	
	if(zones[exit.@zone]) player.@zone=<zone>{exit.@zone}</zone>;
	player.@room=<room>{exit.@target}</room>;

	zone=zones[player.@zone];
	room=map.room.(@id == player.@room);

	srv.o(target,"You " + player.travel + " " + exit.@name + " to " + room.title + ". " + get_exit_string(map,room,player));
}

RPG_Commands["EQUIPMENT"] = function (srv,target,map,room,cmd,player) {
	var list=[];
	var eq=player.equipment..item;
	for each(var i in eq) {
		list.push("<" + i.parent().@id + "> " + i.title);
	}
	var str=list.length?list.join(", "):"nothing";
	srv.o(target,"eq: " + str);
}

RPG_Commands["INVENTORY"] = function (srv,target,map,room,cmd,player) {
	var list=[];
	for each(var i in player.inventory.item) {
		list.push(i.title);
	}
	var str=list.length?list.join(", "):"nothing";
	srv.o(target,"inventory: " + str);
}

RPG_Commands["LOOK"] = function (srv,target,map,room,cmd,player) {
	cmd.shift();
	var obj=cmd.shift();
	
	/*	if no target specified, look at room */
	if(!obj) {
		display_room(srv,player,player.@zone,player.@room);
		return;
	}
	
	/*	if the second word is "in", attempt to look inside a container */
	if(obj.toLowerCase()=="in") {
		var cont=cmd.shift();
		if(!cont) {
			srv.o(target,"Look in what?");
			return false;
		}
		
		var container=false;
		var container_index=find_object(player.inventory,cont);
		if(container_index) container=player.inventory.item[container_index];
		
		/*	attampt to match specified container with room items */
		else container_index=find_object(room,cont); 
		if(!container && container_index) container=room.item[container_index];
		
		/*	if player has this container, check items in container for specified object */
		if(container) {
			if(container.type!="container") {
				srv.o(target,"That isn't a container!");
				return false;
			}
			var list=[];
			for each(var i in container.item) {
				list.push(i.title);
			}
			var str=list.length?list.join(", "):"nothing";
			srv.o(target,"contents (" + container.title + "):" + str);
			return true;
		}
		srv.o(target,"You can't find it!");
		return false;
	}
	
	/*	handle looking at items, mobs, doors */
	var mob_index=find_mob(room,obj);
	if(mob_index) {
		var mob=room.mob[mob_index];
		if(mob.description.length() > 0) srv.o(target,mob.description);
		var list=[];
		var eq=mob.equipment..item;
		for each(var i in eq) {
			list.push("<" + i.parent().@id + "> " + i.title);
		}
		var str=list.length?list.join(", "):"nothing";
		srv.o(target,"eq: " + str);
		return true;
	}
	
	var item_index=find_object(room,obj);
	if(item_index) {
		var item=room.item[item_index];
		srv.o(target,item.description);
		return true;
	}
	
	srv.o(target,"You don't see that here.");
	return false;
}



/* SERVER COMMANDS */
Server_Commands["PRIVMSG"] = function (srv,cmd,onick,ouh)	{ 
	var player=players.player.(@name == onick);

	if (cmd[0][0] == "#" || cmd[0][0] == "&") {
		var chan=srv.channel[cmd[0].toUpperCase()];
		if(!chan) return;
		
		/* if the user has no RPG player or is not logged in, don't process RPG commands */
		if(player.@active != 1) return;

		if(player.@channel == chan.name) {
			cmd.shift();
			/*	handle commands such as north, south, east, west.... */
			try {
				handle_command(srv,cmd,player,player.@channel);
			} catch(e) {
				srv.o(chan.name,e,"NOTICE");
			}
		}
	}
}

Server_Commands["PART"] = function (srv,cmd,onick,ouh)	{
	var player=players.player.(@name == onick);
	if(player.@active==1) {
		player.@active=0;
	}
}
Server_Commands["QUIT"]=Server_Commands["KICK"]=Server_Commands["PART"];

Server_Commands["JOIN"]= function (srv,cmd,onick,ouh) {
	if (cmd[0][0] == ":")
		cmd[0] = cmd[0].slice(1);
	var chan = srv.channel[cmd[0].toUpperCase()];
	
	if(!chan) {
		log("channel not in server channel list");
		return;
	}
	
	// Me joining.
	if (onick == srv.curnick) {
		if(!chan.is_joined) {
			chan.is_joined = true;
			srv.writeout("WHO " + cmd[0]);
		}
		return;
	}
	
	var player=players.player.(@name == onick);
	/* if the user has no RPG player, greet them */
	if(!player.length() > 0) {
		srv.o(chan.name,"Welcome to RPG test, " + onick + "!","NOTICE");
		srv.o(chan.name,"Type 'rpg help' to get started.","NOTICE");
	}
}













