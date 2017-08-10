/* master game data */
function GameData() 
{
	this.profiles={};
	this.games={};
	this.boards={};
	this.online={};
	this.updated=true;
	
	this.loadGames=function() {
		this.games = client.read(game_id,"games",1);
		if(!this.games)
			this.games = {};
	}
	this.loadPlayers=function() {
		this.profiles=client.read(game_id,"profiles",1);
		if(!this.profiles)
			this.profiles = {};
		var alias = user.alias.replace(/\./g,"_");
		if(!this.profiles[alias]) {
			var p = new Profile(alias);
			this.profiles[p.name] = p;
			client.write(game_id,"profiles."+p.name,p,2);
		}
	}
	this.who=function() {
		this.online=client.who(game_id,"games");
	}
	this.addPlayer=function(gameNumber,profile) {
		var player = new Player(profile.name,profile.avatar,profile.color);
		this.games[gameNumber].players[player.name] = player;
		client.write(game_id,"games." + gameNumber + ".players." + player.name,	player,2);
	}
	this.delPlayer=function(gameNumber,profile) {
		delete this.games[gameNumber].players[profile.name];
		client.remove(game_id, "games." + gameNumber + ".players." + profile.name, 2);
	}
	
	this.loadGames();
	this.loadPlayers();
	this.who();
}
/* player score and other information */
function Profile(name) 
{
	this.name=name;
	this.score=0;
	this.lines=0;
	this.level=0;
	this.wins=0;
	this.losses=0;
}
/* in-game player object */
function Player(name)
{
	this.name=name;
	this.ready=false;
}
/* individual game data */
function Game(gameNumber) 
{
	this.gameNumber=gameNumber;
	this.players={};
	this.garbage=settings.garbage;
	this.status=status.WAITING;
	this.pieces=[];
}
/* lobby game icon and status */
function GameInfo(x,y) 
{
	this.frame = new Frame(x,y,18,10,BG_BLACK+CYAN,frame);
	this.open = function() {
		this.frame.open();
	}
	this.close = function() {
		this.frame.close();
	}
	this.frame.load(root + "icon.bin",18,6);
}
/* game board */
function GameBoard(frame,name,x,y) 
{
	this.name=name;
	this.ready=false;
	this.lines=0;
	this.score=0;
	this.level=1;
	this.active=true;
	this.swapped=false;
	this.currentPiece;
	this.nextPiece;
	this.holdPiece;
	this.grid;
	
	this.frame=new Frame(x,y,26,24,undefined,frame);
	this.stack = new Frame(x+1,y+2,20,21,undefined,this.frame);
	this.open = function() {
		this.frame.open();
	}
	this.close = function() {
		this.frame.close();
	}
	
	this.frame.load(root + "board.bin",26,24);
}
/* game menu controller */
function Menu(items,frame,hk_color,text_color) 
{								
	this.items=[];
	this.frame=frame;
	this.hk_color=hk_color;
	this.text_color=text_color;
	
	this.disable=function(item)
	{
		this.items[item].enabled=false;
		this.draw();
	}
	this.enable=function(item)
	{
		this.items[item].enabled=true;
		this.draw();
	}
	this.add=function(items)
	{
		for(i=0;i<items.length;i++) {
			hotkey=get_hotkey(items[i]);
			this.items[hotkey.toUpperCase()]=new Item(items[i],hotkey,hk_color,text_color);
		}
	}
	this.draw=function()
	{
		var str="";
		for each(var i in this.items)
			if(i.enabled==true) str+=i.text + " ";
		this.frame.clear();
		this.frame.putmsg(str);
	}
	
	function Item(item,hotkey,hk_color,text_color)
	{								
		this.enabled=true;
		this.hotkey=hotkey;
		this.text=item.replace(("~" + hotkey) , hk_color + hotkey + text_color);
	}
	function get_hotkey(item)
	{
		keyindex=item.indexOf("~")+1;
		return item.charAt(keyindex);
	}	
	this.add(items);
}
/* tetris piece */
function Piece(id,fg,bg,u,d,l,r)
{
	//this.attr=fg+bg;
	this.attr=fg+BG_BLACK;
	this.ch="\xDB\xDB";
	this.id=id;
	this.direction=[u,r,d,l];

	this.x=4;
	this.y=0;
	this.o=0;
	
	for(var x=0;x<u[0].length;x++) {
		for(var y=0;y<u.length;y++) {
			var found=false;
			if(u[y][x] > 0) {
				found=true;
				break;
			}
		}
		if(found) {
			this.x-=x;
			break;
		}
	}
}
/* miniature piece */
function Mini(id,fg,bg,grid)
{
	this.grid=grid;
	this.id=id;
	this.fg=fg;
	this.bg=bg;
}
