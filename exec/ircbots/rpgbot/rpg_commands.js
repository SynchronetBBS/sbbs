Bot_Commands["CREATE"] = new Bot_Command(0,2,false);
Bot_Commands["CREATE"].help = 
	"This command creates a new player.";
Bot_Commands["CREATE"].usage = 
	get_cmd_prefix() + "CREATE <race> <class>";
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
Bot_Commands["CLASSES"].help = 
	"This provides a list of possible player classes.";
Bot_Commands["CLASSES"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var list=[];
	for(var c in classes.player_class) {
		list.push(classes.player_class[c].@id);
	}
	srv.o(target,"classes: " + list.join(", "));
}

Bot_Commands["RACES"] = new Bot_Command(0,false,false);
Bot_Commands["RACES"].help = 
	"This provides a list of possible player races.";
Bot_Commands["RACES"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var list=[];
	for(var r in races.player_race) {
		list.push(races.player_race[r].@id);
	}
	srv.o(target,"races: " + list.join(", "));
}

Bot_Commands["ZONES"] = new Bot_Command(0,false,false);
Bot_Commands["ZONES"].help = 
	"This provides a list of zones.";
Bot_Commands["ZONES"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var list=[];
	for(var z in zones) {
		list.push(zones[z].@name);
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
	player.@active=1;
	player.@channel=target;
	display_room(srv,player,player.@zone,player.@room);
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

/*	Game commands */
var RPG_Commands=[];

RPG_Commands["NEW"] = function (srv,target,zone,room,cmd,player) {
	if(player.@editing != 1) {
		srv.o(target,"Editor is OFF. Type 'Editor ON' to enable editing.");
		return;
	}

	cmd.shift();
	var editor_cmd=cmd.shift();
	
	switch(editor_cmd.toUpperCase()) {
	case "ZONE":
		var zone_level=cmd.shift();
		var zone_title=cmd.join(" ");

		if(!zone_title || !zone_level) {
			srv.o(target,"usage: NEW ZONE <level> <title>");
			return;
		}
		var zone_id=get_new_zone_id();
		zones[zone_id]=
			<zone name={zone_title} id={zone_id} level={zone_level}>
				<room id="0">
					<title>{settings.title}</title>
					<description>{settings.description}</description>
				</room>
			</zone>;
		srv.o(target,"Zone '" + zone_title + "' created.");
		player.@room=0;
		player.@zone=zone_id;
		display_room(srv,player,zone_id,0);
		break;
	case "ITEM":
		srv.o(target,"TODO: Allow dynamic item creation");
		break;
	case "MOB":
		srv.o(target,"TODO: Allow dynamic mob creation");
		break;
	case "DOOR":
		srv.o(target,"TODO: Allow dynamic door creation");
		break;
	}
}

RPG_Commands["EDITOR"] = function (srv,target,zone,room,cmd,player) {
	cmd.shift();
	var editor_cmd=cmd.shift();
	
	switch(editor_cmd.toUpperCase()) {
	case "ON":
		if(player.@editing != 1) {
			player.@editing = 1;
			srv.o(target,"Zone editor ON");
		}
		return;
	case "OFF":
		if(player.@editing == 1) {
			player.@editing = 0;
			srv.o(target,"Zone editor OFF");
		}
		return;
	}
	
	if(player.@editing != 1) {
		srv.o(target,"Editor is OFF. Type 'Editor ON' to enable editing.");
		return;
	}

	switch(editor_cmd.toUpperCase()) {
	case "SAVE":
		mkdir(z_dir + zone.@name.toLowerCase());
		var z_file=new File(z_dir+zone.@name.toLowerCase()+"/map.xml");
		if(!z_file.open("w+")) return;
		z_file.write(zone);
		srv.o(target,"Zone saved");
		break;
	case "RESTORE":
		var zone_dir=z_dir+zone.@name.toLowerCase();
		var zone=load_zone(zone_dir);
		if(!zone) {
			srv.o(target,"Error loading zone");
			break;
		}
		zones[zone.@id]=zone;
		srv.o(target,"Reverted current zone to saved data");
		break;
	}
	
	return;
}

RPG_Commands["SET"] = function (srv,target,zone,room,cmd,player) {
	if(player.@editing == 0) {
		srv.o(target,"You aren't currently editing.");
		return;
	}
	
	cmd.shift();
	var set=cmd.shift();
	if(!set) {
		srv.o(target,"Set what?");
		return;
	}

	switch(set.toUpperCase()) {
	case "NEW":
		switch(cmd.shift().toUpperCase()) {
		case "TITLE":
			settings.title=cmd.join(" ");
			srv.o(target,"New room title set to: " + settings.title);
			break;
		case "DESCRIPTION":
			settings.description=cmd.join(" ");
			srv.o(target,"New room description set to: " + settings.description);
			break;
		default:
			srv.o(target,"usage: set new <property> <value>");
			break;
		}
		break;
	case "CURRENT":
		switch(cmd.shift().toUpperCase()) {
		case "TITLE":
			room.title=cmd.join(" ");
			srv.o(target,"Current room title set to: " + room.title);
			break;
		case "DESCRIPTION":
			room.description=cmd.join(" ");
			srv.o(target,"Current room description set to: " + room.description);
			break;
		default:
			srv.o(target,"usage: set current <property> <value>");
			break;
		}
		break;
	case "AUTOLINK":
		if(settings.autolink) {
			settings.autolink=false;
			srv.o(target,"Auto-link disabled.");
		} else {
			settings.autolink=true;
			srv.o(target,"Auto-link enabled.");
		}
		break;
	case "LINK":
		var dir=cmd.shift().toLowerCase()
		switch(dir) {
		case "north":
		case "south":
		case "east":
		case "west":
		case "up":
		case "down":
			var room_id=cmd.shift();

			if(!room_id) {
				target_id=room.exit.(@name == dir).@target;
				target_room=zone.room.(@id == target_id);

				/* clear link if no target specified */
				room.exit.(@name == dir).(@target="");
				srv.o(target,"Room exit deleted");

				/* clear corresponding link in original target room */
				if(settings.autolink) target_room.exit.(@name == reverse_dir(dir)).(@target="");
			} else {
				var target_room=zone.room.(@id == room_id);
				var exit=room.exit.(@name == dir);
				
				/* set exit target for current room */
				if(exit.@target.toString()=="") {
					room.exit += <exit name={dir} target={room_id}/>;
					srv.o(target,"Room exit added");

				/* change exit target for current room */
				} else {
					exit.@target=room_id;
					srv.o(target,"Room target changed");
				}
				
				/* change corresponding exit for target room */
				if(settings.autolink) target_room.exit.(@name == reverse_dir(dir)).(@target = room.@id);
			}
			break;
		default:
			srv.o(target,"usage: set <direction> <target>");
			break;
		}
		break;
	deftault:
		srv.o(target,"Set what?");
		break;
	}
}

RPG_Commands["FLEE"] = function (srv,target,zone,room,cmd,player) {
	var chance=random(100);
	if(chance<25) {
		srv.o(target,"You couldn't escape!");
		return false;
	}
	var directions=room.exit;
	var random_exit=directions[random(directions.length())];
	
	if(random_exit.door) {
		var door=zone.door.(@id == random_exit.door);
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

RPG_Commands["KILL"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["SCORE"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["STATUS"] = function (srv,target,zone,room,cmd,player) {
	srv.o(target,"<HP:" + player.hitpoints + " MA:" + player.mana + " MV:" + player.movement + ">");
}

RPG_Commands["MOVE"] = function (srv,target,zone,room,cmd,player) {
	var exit=room.exit.(@name == cmd[1].toLowerCase());
	if(exit.@target.toString()=="") {
		if(player.@editing == 1) {
			create_new_room(srv,target,zone,room,player,cmd[1].toLowerCase());
			return true;
		} else {
			srv.o(target,"You cannot go that way.");
			return false;
		}
	}
	
	if(exit.door) {
		var door=zone.door.(@id == exit.door);
		if(door.open == false && player.@editing != 1) {
			srv.o(target,"The " + door.@name + " is closed.");
			return false;
		}
	}
	
	if(zones[exit.@zone]) player.@zone=<zone>{exit.@zone}</zone>;
	player.@room=<room>{exit.@target}</room>;

	zone=zones[player.@zone];
	room=zone.room.(@id == player.@room);

	srv.o(target,"You " + player.travel + " " + exit.@name + " to " + room.title + ". " + get_exit_string(zone,room,player));
}

RPG_Commands["UNLOCK"] = function (srv,target,zone,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Unlock what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(zone,room,name);
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

RPG_Commands["EQUIPMENT"] = function (srv,target,zone,room,cmd,player) {
	var list=[];
	var eq=player.equipment..item;
	for each(var i in eq) {
		list.push("<" + i.parent().@id + "> " + i.title);
	}
	var str=list.length?list.join(", "):"nothing";
	srv.o(target,"eq: " + str);
}

RPG_Commands["INVENTORY"] = function (srv,target,zone,room,cmd,player) {
	var list=[];
	for each(var i in player.inventory.item) {
		list.push(i.title);
	}
	var str=list.length?list.join(", "):"nothing";
	srv.o(target,"inventory: " + str);
}

RPG_Commands["LOCK"] = function (srv,target,zone,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Lock what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(zone,room,name);
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

RPG_Commands["OPEN"] = function (srv,target,zone,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Open what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(zone,room,name);
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

RPG_Commands["CLOSE"] = function (srv,target,zone,room,cmd,player) {
	cmd.shift();
	var name=cmd.shift();
	if(!name) {
		srv.o(target,"Close what?");
		return false;
	}
	
	/*	first check doors in the room */
	var door=find_door(zone,room,name);
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

RPG_Commands["DROP"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["GET"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["PUT"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["LOOK"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["GOTO"] = function (srv,target,zone,room,cmd,player) {
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
	var room=zone.room.(@id == player.@room);
	srv.o(target,"You teleport to " + room.title);
	display_room(srv,player,player.@zone,player.@room);
	return;
}

RPG_Commands["REMOVE"] = function (srv,target,zone,room,cmd,player) {
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

RPG_Commands["WEAR"] = function (srv,target,zone,room,cmd,player) {
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
RPG_Commands["EQUIP"] = RPG_Commands["WEAR"];
RPG_Commands["WIELD"] = RPG_Commands["WEAR"];