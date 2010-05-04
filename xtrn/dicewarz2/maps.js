load("funclib.js");
load("commclient.js");
load(game_dir+"player.js");
load(game_dir+"rolldice.js");

var settings=	loadSettings(game_dir+"dice.ini");
var players=	new PlayerData(game_dir+settings.player_file);
var game_data=	new GameData(game_dir+"*.dw");
var stream=	new ServiceConnection("Dice Warz ][","dicewars");

function Char(ch,fg,bg)
{
	this.ch=ch;
	this.fg=fg;
	this.bg=bg;
	this.draw=function() {
		console.attributes=this.fg+this.bg;
		console.putmsg(this.ch,P_SAVEATR);
	}
}
function MapData(game_number)
{
	this.width=22;
	this.height=10;
	this.grid=createGrid(this.width,this.height);
	this.tiles=[];
	this.players=[];
	this.history=[];
	this.turn=0;
	this.round=0;
	this.attacking=false;
	this.in_progress=false;
	this.single_player=false;
	this.winner=false;
	this.game_number=game_number?game_number:getNewGameNumber();
	this.last_update=time();
}
function Tile(id)
{
	this.id=id;
	this.coords=[];
	this.home;
	this.owner;
	this.dice=0;
	this.assign=function(owner,dice)
	{
		this.owner=owner;
		this.dice=dice;
	}
}
function Coords(x,y,z)
{
	this.x=x;
	this.y=y;
	this.z=z;
}
function GameData(filemask)
{
	this.files=filemask;
	this.list=[];
	this.last_game_number=0;
	this.last_update=0;
	
	const update_frequency=5 //SECONDS BETWEEN GAME UPDATE SCANS
	
	this.count=function()
	{
		var count=0;
		for(var i in this.list) {
			count++;
		}
		return count;
	}
	this.init=function()
	{
		this.update();
	}
	this.load=function(filename) 
	{
		var file=new File(filename);
		file.open('r+',true);
		
		var game_number=getGameNumber(filename);
		if(game_number>this.last_game_number) this.last_game_number=game_number;
		var map=new MapData(game_number);
		debug("loading game: " + game_number,LOG_DEBUG);
		
		map.num_players=file.iniGetValue(null,"num_players");
		map.single_player=file.iniGetValue(null,"single_player");
		map.in_progress=file.iniGetValue(null,"in_progress");
		map.winner=file.iniGetValue(null,"winner");
		map.turn=file.iniGetValue(null,"turn");
		
		var players=file.iniGetAllObjects("index","p");
		var aifile=new File(game_dir + settings.ai_file);
		aifile.open("r",true);
		for(var p=0;p<players.length;p++) {
			var player=players[p];
			map.players[player.index]=new Player(player.name,player.vote);
			map.players[player.index].reserve=player.reserve;
			if(player.AI) {
				var sort=aifile.iniGetValue(player.name, "sort");
				var check=aifile.iniGetValue(player.name, "check");
				var qty=aifile.iniGetValue(player.name, "quantity");
				map.players[player.index].AI=new AI(sort,check,qty);
			}
		}
		aifile.close();
		
		var array=file.iniGetAllObjects("index","t");
		for(var t=0;t<array.length;t++) {
			var tile=new Tile(array[t].index);
			tile.assign(array[t].o,array[t].d);
			
			var home=array[t].h.split("-");
			tile.home=new Coords(home[0],home[1]);
			
			var sectors=array[t].s.split(",");
			for(var s=0;s<sectors.length;s++) {	
				var sector=sectors[s].split("-");
				var coords=new Coords(sector[0],sector[1]);
				tile.coords.push(coords);
				map.grid[coords.x][coords.y]=array[t].index;
			}
			map.tiles[array[t].index]=tile;
		}
		for(var p=0;p<map.players.length;p++) {
			if(countTiles(map,p)==0) map.players[p].active=false;
		}
		this.list[game_number]=map;
	}
	this.save=function(map)
	{
		this.list[map.game_number]=map;
		if(map.game_number>this.last_game_number) this.last_game_number=map.game_number;
		
		menuText("Storing game data");
		this.saveData(map);
		
		menuText("Storing player data");
		for(var p=0;p<map.players.length;p++)
		{
			menuText(".",true);
			var player=map.players[p];
			this.savePlayer(map,player,p);
		}
		
		menuText("Storing map data");
		for(var t=0;t<map.tiles.length;t++)
		{
			menuText(".",true);
			var tile=map.tiles[t];
			this.saveTile(map,tile);
		}
	}
	this.closeGameFile=function(file,map)
	{
		if(file.is_open) file.close();
		map.last_update=file_date(file.name);
	}
	this.openGameFile=function(map) 
	{
		var filename=getFileName(map.game_number);
		var file=new File(filename);
		if(!file.is_open) file.open((file_exists(file.name) ? 'r+':'w+'),true); 
		return file;
	}
	this.saveActivity=function(map,activity)
	{
	}
	this.savePlayer=function(map,p,n)
	{
		var file=this.openGameFile(map);
		file.iniSetValue("p"+n,"name",p.name);
		file.iniSetValue("p"+n,"reserve",p.reserve);
		file.iniSetValue("p"+n,"vote",p.vote);
		file.iniSetValue("p"+n,"AI",p.AI?true:false);
		this.closeGameFile(file,map);
	}
	this.saveData=function(map)
	{
		var file=this.openGameFile(map);
		file.iniSetValue(null,"num_players",map.num_players);
		file.iniSetValue(null,"single_player",map.single_player);
		file.iniSetValue(null,"in_progress",map.in_progress);
		file.iniSetValue(null,"winner",map.winner);
		file.iniSetValue(null,"turn",map.turn);
		file.iniSetValue(null,"round",map.round);
		this.closeGameFile(file,map);
	}
	this.saveTile=function(map,tile)
	{
		var file=this.openGameFile(map);
		file.iniSetValue("t"+tile.id,"d",tile.dice);
		file.iniSetValue("t"+tile.id,"o",tile.owner);
		file.iniSetValue("t"+tile.id,"h",tile.home.x + "-" + tile.home.y);
		var slist=[];
		for(var c=0;c<tile.coords.length;c++)
		{
			var sector=tile.coords[c];
			slist.push(sector.x+"-"+sector.y);
		}
		file.iniSetValue("t"+tile.id,"s",slist);
		this.closeGameFile(file,map);
		
		var data=new Packet("tile");
		data.tile=tile;
		stream.send(data);
	}
	this.update=function()
	{
		if(time()-this.last_update<update_frequency) return false;
		
		var game_list=directory(this.files);
		for(var g=0;g<game_list.length;g++) {
			var game_number=getGameNumber(game_list[g]);
			if(this.list[game_number]) {
				if(this.list[game_number].last_update<file_date(game_list[g])) {
					debug("file has been modified, reloading: " + game_list[g]);
					this.load(game_list[g]);
				}
			} else {
				this.load(game_list[g]);
			}
		}
		this.last_update=time();
	}
	this.init();
}
function Packet(type)
{
	this.type=type;
}

	//GAME DATA FUNCTIONS
	function loadSettings(filename)
	{
		var file=new File(filename);
		file.open('r',true);
		
		var data=file.iniGetObject(null);
		data.background_colors=file.iniGetObject("background_colors");
		data.foreground_colors=file.iniGetObject("foreground_colors");
		data.text_colors=file.iniGetObject("text_colors");
		data.point_set=file.iniGetObject("point_set");
		
		file.close();
		return data;
	}
	function getPlayerColors(map,p)
	{
		var colors=new Object();
		colors.bg=map.players[p].active?getColor(settings.background_colors[p]):BG_BLACK;
		colors.fg=map.players[p].active?getColor(settings.foreground_colors[p]):BLACK;
		colors.txt=map.players[p].active?getColor(settings.text_colors[p]):LIGHTGRAY;
		return colors;
	}
	function sortGameData(array)
	{
		var sorted=new Object();
		sorted.started=[];
		sorted.waiting=[];
		sorted.finished=[];
		sorted.yourgames=[];
		sorted.yourturn=[];
		sorted.eliminated=[];
		sorted.yourwins=[];
		sorted.singleplayer=[];
		
		for(var i in array) {
			var game=array[i];
			var in_game=findPlayer(game,user.alias);
			
			if(game.single_player) {
				if(in_game) sorted.singleplayer.push(i);
			} else {
				if(in_game) {
					sorted.yourgames.push(i);
					if(game.turn==in_game) sorted.yourturn.push(i);
				}
				if(game.in_progress) sorted.started.push(i);
				else if(game.winner) {
					sorted.finished.push(i);
					if(game.winner==user.alias) sorted.yourwins.push(i);
					else sorted.eliminated.push(i);
				} else {
					sorted.waiting.push(i);
				}
			}
		}
		return sorted;
	}
	function getFileName(game_number)
	{
		var name=game_number;
		if(name<10) name="0"+name;
		return game_dir+name+".dw";
	}
	function getGameNumber(filename)
	{
		filename=file_getname(filename);
		var game_number=parseInt(filename.substring(0,filename.indexOf(".")));
		return game_number;
	}
	function getNewGameNumber()
	{
		var next=1;
		while(file_exists(getFileName(next))) 
		{
			next++;
		}
		if(next>settings.max_games) return false;
		return next;
	}

	//MAP DISPLAY FUNCTIONS
	function getxy(x,y)
	{
		var offset=(x%2);
		var posx=(x*2)+3;
		var posy=(y*2)+2+offset;
		var coords=new Coords(posx,posy);
		console.gotoxy(coords);
		return coords;
	}
	function drawTile(map,tile)
	{
		for(var c=0;c<tile.coords.length;c++) {
			var coords=tile.coords[c];
			drawSector(map,coords.x,coords.y);
		}
		var borders=getBorders(map,tile);
		for(var x in borders) {
			if(borders[x]) {
				for(var y in borders[x]) {
					drawSector(map,x,y);
				}
			}
		}
	}
	function drawSector(map,x,y) // YAY 168 LINES OF CODE TO DRAW 4 CHARACTERS
	{
		var current_coords=new Coords(x,y);
		var neighbors=getNeighboringSectors(current_coords,map.grid);
		var current=findTile(map.grid,current_coords);
		var north=findTile(map.grid,neighbors.north);
		var northwest=findTile(map.grid,neighbors.northwest);
		var northeast=findTile(map.grid,neighbors.northeast);
		var south=findTile(map.grid,neighbors.south);
		var southwest=findTile(map.grid,neighbors.southwest);
		var southeast=findTile(map.grid,neighbors.southeast);
		
		var top_char;
		var left_char;
		var middle_char;
		var right_char;
		var bottom_char;

		var current_colors;
		var current_tile;
		
		if(current>=0)	{
			var current_tile=map.tiles[current];
			current_colors=getPlayerColors(map,current_tile.owner);
			//MIDDLE CHAR
			if(x==current_tile.home.x && y==current_tile.home.y) {
				middle_char=new Char(current_tile.dice,current_colors.txt,current_colors.bg);
			} else {
				middle_char=new Char(" ",current_colors.fg,current_colors.bg);
			}
			//TOP CHAR
			if(north>=0) {
				var north_tile=map.tiles[north];
				var north_colors=getPlayerColors(map,north_tile.owner);
				if(north==current) {
					top_char=new Char(" ",current_colors.fg,current_colors.bg);
				} else if(north_tile.owner==current_tile.owner) {
					top_char=new Char("\xC4",BLACK,current_colors.bg);
				} else top_char=new Char("\xDF",north_colors.fg,current_colors.bg);
			} else {
				top_char=new Char("\xDC",current_colors.fg,BG_BLACK);
			}
			//LEFT CHAR
			if(northwest==current && southwest==current) {
				left_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else if(northwest==southwest) {
				if(northwest>=0 && map.tiles[northwest].owner==current_tile.owner) {
					left_char=new Char("\xB3",BLACK,current_colors.bg);
				} else	if(northwest>=0) {
					var colors=getPlayerColors(map,map.tiles[northwest].owner);
					left_char=new Char("\xDD",colors.fg,current_colors.bg);
				} else {
					left_char=new Char("\xDE",current_colors.fg,BG_BLACK);
				}
			} else if(northwest>=0 && southwest>=0 && map.tiles[northwest].owner==current_tile.owner && map.tiles[southwest].owner==current_tile.owner && northwest!=current && southwest!=current) {
				left_char=new Char("\xB4",BLACK,current_colors.bg);
			} else if(northwest>=0 && map.tiles[northwest].owner==current_tile.owner && northwest!=current) {
				left_char=new Char("\xD9",BLACK,current_colors.bg);
			} else if(southwest>=0 && map.tiles[southwest].owner==current_tile.owner && southwest!=current) {
				left_char=new Char("\xBF",BLACK,current_colors.bg);
			} else if(northwest>=0 || southwest>=0) {
				left_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else {
				left_char=new Char("\xDE",current_colors.fg,BG_BLACK);
			}
			//RIGHT CHAR
			if(northeast==current && southeast==current) {
				right_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else if(northeast==southeast) {
				if(northeast>=0 && map.tiles[northeast].owner==current_tile.owner) {
					right_char=new Char("\xB3",BLACK,current_colors.bg);
				} else	if(northeast>=0) {
					var colors=getPlayerColors(map,map.tiles[northeast].owner);
					right_char=new Char("\xDE",colors.fg,current_colors.bg);
				} else {
					right_char=new Char("\xDD",current_colors.fg,BG_BLACK);
				}
			} else if(northeast>=0 && southeast>=0 && map.tiles[northeast].owner==current_tile.owner && map.tiles[southeast].owner==current_tile.owner && northeast!=current && southeast!=current) {
				right_char=new Char("\xC3",BLACK,current_colors.bg);
			} else if(northeast>=0 && map.tiles[northeast].owner==current_tile.owner && northeast!=current) {
				right_char=new Char("\xC0",BLACK,current_colors.bg);
			} else if(southeast>=0 && map.tiles[southeast].owner==current_tile.owner && southeast!=current) {
				right_char=new Char("\xDA",BLACK,current_colors.bg);
			} else if(northeast>=0 || southeast>=0) {
				right_char=new Char(" ",current_colors.fg,current_colors.bg);
			} else {
				right_char=new Char("\xDD",current_colors.fg,BG_BLACK);
			}
			//BOTTOM CHAR
			if(south<0) {
				bottom_char=new Char("\xDF",current_colors.fg,BG_BLACK);
			}
		} else {
			middle_char=new Char("~",BLUE,BG_BLACK);
			if(north>=0) {
				var north_tile=map.tiles[north];
				var north_colors=getPlayerColors(map,north_tile.owner);
				top_char=new Char("\xDF",north_colors.fg,BG_BLACK);
			} else	top_char=new Char(" ",BLACK,BG_BLACK);
			if(northwest>=0 && northwest==southwest) {
				var southwest_colors=getPlayerColors(map,map.tiles[southwest].owner);
				left_char=new Char("\xDD",southwest_colors.fg,BG_BLACK);
			} else left_char=new Char(" ",BLACK,BG_BLACK);
			if(northeast>=0 && northeast==southeast) {
				var southeast_colors=getPlayerColors(map,map.tiles[southeast].owner);
				right_char=new Char("\xDE",southeast_colors.fg,BG_BLACK);
			} else right_char=new Char(" ",BLACK,BG_BLACK);
		}

		getxy(x,y);
		top_char.draw();
		console.down();
		console.left(2);
		left_char.draw();
		middle_char.draw();
		right_char.draw();
		if(bottom_char) {
			console.down();
			console.left(2);
			bottom_char.draw();
		}
	}
	function drawMap(map)
	{
		for(var x=0;x<map.grid.length;x++) {
			for(var y=0;y<map.grid[x].length;y++) {
				drawSector(map,x,y);
			}
		}
	}
	function drawTiles(map)
	{
		for(var t in map.tiles)
		{
			drawTile(map,map.tiles[t]);
		}
	}

	//MAP SCANNING FUNCTIONS
	function connected(tile1,tile2,map)
	{
		for(var c=0;c<tile1.coords.length;c++) {
			var valid_directions=getNeighboringSectors(tile1.coords[c],map.grid);
			for(var d in valid_directions) {
				var dir=valid_directions[d];
				if(map.grid[dir.x][dir.y]==tile2.id) return true;
			}
		}
		return false;
	}
	function getBorders(map,tile)
	{
		var borders=[];
		for(var c in tile.coords) {
			var coord=tile.coords[c];
			var neighbors=getNeighboringSectors(coord,map.grid);
			for(var n in neighbors) {
				var neighbor=neighbors[n];
				if(map.grid[neighbor.x][neighbor.y]!=tile.id) {
					if(!borders[neighbor.x]) borders[neighbor.x]=[];
					borders[neighbor.x][neighbor.y]=true;
				}
			}
		}
		return borders;
	}
	function getNeighboringTiles(tile,map) 
	{
		var found=[];
		var neighbors=[];
		for(var c=0;c<tile.coords.length;c++) {
			var valid_directions=getNeighboringSectors(tile.coords[c],map.grid);
			for(var d in valid_directions) {
				var dir=valid_directions[d];
				var t=map.grid[dir.x][dir.y];
				if(t>=0 && t!=tile.id && !found[t]) {
					found[t]=true;
					neighbors.push(map.tiles[t]);
				}
			}
		}
		return neighbors;
	}
	function getNeighboringSectors(relative_to_position,on_grid)
	{
		var xlimit=on_grid.length;
		var ylimit=on_grid[0].length;
		var offset=relative_to_position.x%2;
		var neighbors=[];
		var dir_names=["northwest","north","northeast","southwest","south","southeast"];
		var dir_x=[-1,0,1,-1,0,1];
		var dir_y=[-1+offset,-1,-1+offset,0+offset,1,0+offset];
		for(var i=0;i<dir_x.length;i++) {
			var x=parseInt(relative_to_position.x,10)+dir_x[i];
			var y=parseInt(relative_to_position.y,10)+dir_y[i];
			if(x>=xlimit || y>=ylimit || x<0 || y<0) continue;
			neighbors[dir_names[i]]=new Coords(x,y);
		}
		return neighbors;
	}
	function getRandomDirection(from_position,on_grid)
	{
		var valid_directions=getNeighboringSectors(from_position,on_grid);
		var available=[];
		for(var d in valid_directions) {
			available.push(d);
		}
		while(available.length) {
			var rand=random(available.length);
			var random_dir=valid_directions[available[rand]];
			available.splice(rand,1);
			if(on_grid[random_dir.x][random_dir.y]>=0) continue;
			return random_dir;
		}
		return false;
	}
	function landNearby(grid,x,y)
	{
		var valid_directions=getNeighboringSectors(new Coords(x,y),grid);
		var land=0;
		for(var d in valid_directions) {
			var coords=valid_directions[d];
			if(grid[coords.x][coords.y]>=0) land++;
		}
		return land;
	}
	function findTile(grid,coords)
	{
		if(coords && grid[coords.x][coords.y]>=0) return grid[coords.x][coords.y];
		else return -1;
	}
	function canAttack(map,base)
	{
		
		var valid_targets=[];
		if(base.dice>1) {
			var neighbors=getNeighboringTiles(base,map);
			for(var n=0;n<neighbors.length;n++) {
				var tile=neighbors[n];
				if(tile.owner!=base.owner) valid_targets.push(tile);
			}
		}
		return valid_targets;
	}
	function countConnected(map,array,player)
	{	
		var counted=[];
		var largest_group=0;
		var match=player;
		
		for(var t in array) {
			if(!counted[array[t].id]) {
				var count=[];
				count[array[t].id]=true;
				var grid=getBorders(map,array[t]);
				var connections=trace(map,grid,count,match);
				counted.concat(connections);
				var group_size=countSparseArray(connections);
				if(group_size>largest_group) largest_group=group_size;
			}
		}
		return largest_group;
	}
	function trace(map,grid,counted,match)
	{
		for(var x in grid) {
			for(var y in grid[x]) {
				if(map.grid[x][y]>=0) {
					var tile=map.tiles[map.grid[x][y]];
					if(tile.owner==match) {
						if(!counted[tile.id]) {
							counted[tile.id]=true;
							counted.concat(trace(map,getBorders(map,tile),counted,match));
						}
					}
				}
			}
		}
		return counted;
	}

	//MAP INIT FUNCTIONS
	function addComputers(map)
	{
		var num=settings.num_players-map.players.length;
		if(num>0) {
			var aifile=new File(game_dir + settings.ai_file);
			aifile.open("r",true);
			var possibleplayers=aifile.iniGetSections();
			while(num>0) {
				var p=random(possibleplayers.length);
				var name=possibleplayers[p];
				
				var player=new Player(name,true);
				var sort=aifile.iniGetValue(name, "sort");
				var check=aifile.iniGetValue(name, "check");
				var qty=aifile.iniGetValue(name, "quantity");
				player.AI=new AI(sort,check,qty);
				map.players.push(player);
				
				possibleplayers.splice(p,1);
				num--;
			}
			aifile.close();
		}
	}
	function addPlayer(map,name,vote)
	{
		map.players.push(new Player(name,vote));
	}
	function shufflePlayers(map)
	{
		map.players=shuffle(map.players);
	}
	function dispersePlayers(map)
	{
		var tiles_per_player=map.tiles.length/map.players.length;
		var placeholder=[];
		
		for(var p=0;p<map.players.length;p++)
		{
			var t=tiles_per_player;
			var occupied=[];
			
			while(t>0)
			{
				var rand_t=random(map.tiles.length);
				if(placeholder[rand_t]) continue;
				
				map.tiles[rand_t].assign(p,1);
				placeholder[rand_t]=true;
				occupied.push(rand_t);
				t--;
			}

			var d=tiles_per_player;
			if(p==map.players.length-1) d+=2;
			else if(p>=(map.players.length-1)/2) d++;
			while(d>0)
			{
				var rand_t=occupied[random(occupied.length)];
				if(map.tiles[rand_t].dice==settings.max_dice) continue;
				map.tiles[rand_t].dice++;
				d--;
			}
		}
	}
	function generateMap(map)
	{		
		var total=map.width*map.height;
		var min_size=settings.min_tile_size;
		var max_size=settings.max_tile_size;
		
		map.num_players=settings.num_players;
		var num_tiles=parseInt(total/(max_size-1),10);
		num_tiles-=num_tiles%map.players.length;
		
		for(var t=0;t<num_tiles;t++)
		{
			map.tiles[t]=new Tile(t);
			var size=random(max_size-min_size)+min_size;
			
			while(1) 
			{
				x=random(map.width-1)+1;
				y=random(map.height-1)+1;
				
				if(map.grid[x][y]>=0) continue;
				if(t==0) break;
				
				var land_nearby=landNearby(map.grid,x,y);
				if(land_nearby>0 && land_nearby<6) break;
			}
			
			var sector=new Coords(x,y);
			map.grid[sector.x][sector.y]=t;
			map.tiles[t].coords.push(new Coords(x,y));
			
			for(var s=1;s<size;s++)
			{
				sector=getRandomDirection(sector,map.grid);
				if(!sector) break;
				map.grid[sector.x][sector.y]=t;
				map.tiles[t].coords.push(sector);
			}
			
			var home=random(map.tiles[t].coords.length);
			map.tiles[t].home=map.tiles[t].coords[home];
		}
	}
	function createGrid(w,h)	
	{
		var grid=new Array(w);
		for(var x=0;x<grid.length;x++) grid[x]=new Array(h);
		return grid;
	}

	//MAP DATA FUNCTIONS
	function updateStatus(map)
	{
		debug("updating game status");
		for(var p=0;p<map.players.length;p++) {
			if(map.players[p].active) {
				if(countTiles(map,p)==0) {
					debug("inactive player: " + map.players[p].name);
					map.players[p].active=false;
				}
			}
		}
		if(map.single_player && map.in_progress) {
			for(var p=0;p<map.players.length;p++) {
				var player=map.players[p];
				/* for a single player game, find the non-AI player and check whether active. if not, game over */
				if(!player.AI && !player.active) {
					debug("inactive game");
					map.in_progress=false;
					findWinner(map);
					return;
				}
			}
		}
		if(countActivePlayers(map)==1) findWinner(map);
	}
	function findWinner(map)
	{
		var winner=false;
		var most_tiles=0;
		
		for(var p=0;p<map.players.length;p++){
			if(map.players[p].active) {
				var num_tiles=countTiles(map,p);
				if(num_tiles>most_tiles) winner=p;
			}
		}
		map.winner=winner;
		players.scoreWin(map.players[map.winner].name);
	}
	function placeReinforcements(map,pt,reinforcements)
	{
		var placed=[];
		while(reinforcements>0 && pt.length>0) {
			var rand=random(pt.length);
			var tile=pt[rand];
			if(tile.dice<settings.max_dice) {
				tile.dice++;
				reinforcements--;
				if(!placed[tile.id]) placed[tile.id]=0;
				placed[tile.id]++;
			} else {
				pt.splice(rand,1);
			}
		}
		while(map.players[map.turn].reserve>0 && pt.length>0) {
			var rand=random(pt.length);
			var tile=pt[rand];
			if(tile.dice<settings.max_dice) {
				tile.dice++;
				map.players[map.turn].reserve--;
				if(!placed[tile.id]) placed[tile.id]=0;
				placed[tile.id]++;
				game_data.saveTile(map,tile);
			} else {
				pt.splice(rand,1);
			}
		}
		for(var p in placed) {
			game_data.saveTile(map,map.tiles[p]);
		}
		return placed;
	}
	function placeReserves(map,num)
	{
		var placed=0;
		if(map.players[map.turn].reserve<settings.max_reserve) { 
			map.players[map.turn].reserve+=num;
			placed=num;
			if(map.players[map.turn].reserve>settings.max_reserve) {
				placed-=map.players[map.turn].reserve-settings.max_reserve;
				map.players[map.turn].reserve=settings.max_reserve;
			}
			game_data.savePlayer(map,map.players[map.turn],map.turn);
		}
		return placed;
	}
	function nextTurn(map)
	{
		if(map.turn==map.players.length-1) {
			map.turn=0;
			map.round++;
		} else map.turn++;
		if(!map.players[map.turn].active) {
			nextTurn(map);
		}
		else {
			var data=new Packet("turn");
			data.turn=map.turn;
			stream.send(data);
		}
	}
