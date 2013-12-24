/* game status constants */
var status = {
	NEW:-1,
	PLAYING:0,
	FINISHED:1
};

/* Dicewarz II class definitions */
function Char(ch,fg,bg)
{
	this.ch=ch;
	this.fg=fg;
	this.bg=bg;
	// this.draw=function() {
		// console.attributes=this.fg+this.bg;
		// console.putmsg(this.ch,P_SAVEATR);
	// }
}
function Map(gameNumber)
{
	this.gameNumber = gameNumber;
	this.width=22;
	this.height=10;
	this.grid=createGrid(this.width,this.height);
	this.tiles=[];
}
function Game(gameNumber,hidden_names)
{
	this.players=[];
	this.status=status.NEW;
	this.gameNumber=gameNumber;
	this.hidden_names=hidden_names;
	this.single_player=false;
	this.winner=false;
	this.turn=0;
	this.round=0;
	this.last_turn=Date.now();
}
function Tile(id)
{
	this.id=id;
	this.coords=[];
	this.home;
	this.owner;
	this.dice=0;
}
function Coords(x,y,z)
{
	this.x=x;
	this.y=y;
	this.z=z;
}
function Data()
{
	this.games={};
	this.scores={};
	
	/* return the last valid game number */
	this.__defineGetter__("lastGameNumber",function() {
		var gnum=0;
		for each(var n in this.games) {
			if(n.gameNumber > gnum)
				gnum = n.gameNumber;
		}
		return gnum;
	});
	
	/* load scores and game list */
	this.init=function() {
		this.loadGames();
		this.loadScores();
	}

	/* count games */
	this.count=function() {
		var count=0;
		for(var g in this.games) 
			count++;
		return count;
	}

	/* game data */
	this.loadGames=function() {
		this.games = client.read(game_id,"games",1);
		if(!this.games)
			this.games={};
	}
	this.loadGame=function(gameNumber) {	
		var location = "games." + gameNumber;
		this.games[gameNumber] = client.read(game_id,location,1);
		return this.games[gameNumber];
	}

	this.savePlayer=function(game,playerNumber) {
		var location="games." + game.gameNumber + ".players." + playerNumber;
		client.write(game_id,location,game.players[playerNumber],2);
	}
	this.removePlayer=function(map,n) {
		var location="games." + map.gameNumber + ".players." + n;
		client.remove(game_id,location,2);
	}	
	this.saveGame=function(game) {
		var location = "games." + game.gameNumber;
		client.write(game_id,location,game,2);
		this.games[game.gameNumber]=game;
	}
	this.saveWinner=function(game) {
		var location="games." + game.gameNumber + ".winner";
		client.write(game_id,location,game.winner,2);
	}
	this.saveStatus=function(game) {
		var location="games." + game.gameNumber + ".status";
		client.write(game_id,location,game.status,2);
	}
	this.saveTurn=function(game) {
		var location="games." + game.gameNumber;
		client.write(game_id,location + ".last_turn",Date.now(),2);
		client.write(game_id,location + ".turn",game.turn,2);
	}
	this.saveRound=function(game) {
		var location="games." + game.gameNumber;
		client.write(game_id,location + ".round",game.round,2);
	}
	this.saveMap=function(map) {
		var location = "maps." + map.gameNumber;
		client.write(game_id,location,map,2);
	}
	this.saveTile=function(map,tile) {
		var location="maps." + map.gameNumber + ".tiles." + tile.id;
		client.write(game_id,location,tile,2);
	}
	this.saveActivity=function(game,activity) {
		var location="metadata." + game.gameNumber + ".activity";
		client.write(game_id,location,activity,2);
	}
	this.assignTile=function(map,tile,owner,dice) {
		tile.owner=owner;
		tile.dice=dice;
		this.saveTile(map,tile);
	}
	
	/* score data */
	this.loadScores=function() {
		this.scores = client.read(game_id,"scores",1);
		if(!this.scores)
			this.scores={};
	}
	this.scoreKiller=function(player) {
		client.lock(game_id,"scores." + player.name,2);
		
		var score=client.read(game_id,"scores." + player.name);
		if(!score)
			score=new Score(player.name,player.system);
			
		score.kills++;
		score.points+=Number(settings.point_set.kill);
		this.scores[player.name]=score;
		
		client.write(game_id,"scores." + player.name,score);
		client.unlock(game_id,"scores." + player.name);
	}
	this.scoreWinner=function(game) {
		var winner = game.players[game.winner];
		client.lock(game_id,"scores." + winner.name,2);
		
		var score=client.read(game_id,"scores." + winner.name);
		if(!score)
			score=new Score(winner.name,winner.system);
			
		score.wins++;
		score.points=Number(score.points) + Number(settings.point_set.win);
		this.scores[winner.name]=score;
		
		client.write(game_id,"scores." + winner.name,score);
		client.unlock(game_id,"scores." + winner.name);
	}
	this.scoreLoser=function(player) {
		client.lock(game_id,"scores." + player.name,2);
		
		var score=client.read(game_id,"scores." + player.name);
		if(!score)
			score=new Score(player.name,player.system);
			
		score.losses++;
		score.points=Number(score.points) + Number(settings.point_set.loss);
		if(score.points < settings.min_points)
			score.points = Number(settings.min_points);
		this.scores[player.name]=score;
		
		client.write(game_id,"scores." + player.name,score);
		client.unlock(game_id,"scores." + player.name);
	}
	this.scoreForfeit=function(player) {
		client.lock(game_id,"scores." + player.name,2);
		
		var score=client.read(game_id,"scores." + player.name);
		if(!score)
			score=new Score(player.name,player.system);
			
		score.losses++;
		score.points=Number(score.points) + Number(settings.point_set.forfeit);
		if(score.points < settings.min_points)
			score.points = Number(settings.min_points);
		this.scores[player.name]=score;
		
		client.write(game_id,"scores." + player.name,score);
		client.unlock(game_id,"scores." + player.name);
	}
	
	this.init();
}
function Player(name,sys_name,vote)
{
	this.name=name;
	this.system=sys_name;
	this.vote=vote;
	this.reserve=0;
	this.active=true;
	this.AI=false;
}
function AI(sort,check,qty)
{
	this.sort=sort
	this.check=check;
	this.qty=qty;
	this.turns=0;
	this.moves=0;
}
function Score(name,sys_name)
{
	this.name=name;
	this.system=sys_name;
	this.kills=0;
	this.wins=0;
	this.losses=0;
	this.points=0;
}
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
	this.enable=function(item) {
		this.items[item].enabled=true;
		this.draw();
	}
	this.add=function(items) {
		for(i=0;i<items.length;i++) {
			hotkey=getHotkey(items[i]);
			this.items[hotkey.toUpperCase()]=new Item(items[i],hotkey,hk_color,text_color);
		}
	}
	this.draw=function() {
		var str="";
		for each(var i in this.items)
			if(i.enabled==true) str+=i.text + " ";
		this.frame.clear();
		this.frame.putmsg(str);
	}
	
	function Item(item,hotkey,hk_color,text_color) {								
		this.enabled=true;
		this.hotkey=hotkey;
		this.text=item.replace(("~" + hotkey) , hk_color + hotkey + text_color);
	}
	function getHotkey(item) {
		keyindex=item.indexOf("~")+1;
		return item.charAt(keyindex);
	}	
	this.add(items);
}
function Roll(pnum) 
{
	this.pnum=pnum;
	this.rolls=[];
	this.total=0;
	this.roll=function(num)	{
		this.rolls.push(num);
		this.total+=num;
	}
}
function Die(number) 
{				
	var dot = "\xFE";
	
	this.number=number;
	this.line1;
	this.line2;
	this.line3;

	switch(this.number) {
	case 1:
		this.line2=" "+dot+" ";
		break;
	case 2:
		this.line1=dot+"  ";
		this.line3="  "+dot;
		break;
	case 3:
		this.line1=dot+"  ";
		this.line2=" "+dot+" ";
		this.line3="  "+dot;
		break;
	case 4:
		this.line1=dot+" "+dot;
		this.line3=dot+" "+dot;
		break;
	case 5:
		this.line1=dot+" "+dot;
		this.line2=" "+dot+" ";
		this.line3=dot+" "+dot;
		break;
	case 6:
		this.line1=dot+" "+dot;
		this.line2=dot+" "+dot;
		this.line3=dot+" "+dot;
		break;
	}
	
	this.draw=function() {
		console.putmsg(this.line1,P_SAVEATR);
		console.down();
		console.left(3);
		console.putmsg(this.line2,P_SAVEATR);
		console.down();
		console.left(3);
		console.putmsg(this.line3,P_SAVEATR);
	}
}
