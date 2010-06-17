var player_file=this.dir + "players.ini";

function handler(data)
{
	switch(data.type.toUpperCase()) {
		case "TILE":
			var file=new File(data.filename);
			var tile=data.tile;
			file.open('r+',true);
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
			file.close();
			break;
		case "MAP":
			break;
		case "PLAYER":
			var file=new File(data.filename);
			var p=data.player;
			var n=data.pnum;
			file.open('r+',true);
			file.iniSetValue("p"+n,"name",p.name);
			file.iniSetValue("p"+n,"reserve",p.reserve);
			file.iniSetValue("p"+n,"vote",p.vote);
			file.iniSetValue("p"+n,"AI",p.AI?true:false);
			file.close();
			break;
		case "TURN":
			var file=new File(data.filename);
			file.open('r+',true);
			file.iniSetValue(null,"turn",data.turn);
			file.close();
			break;
		case "SCORE":
			var score=data.score;
			if(!player_file.is_open) {
				player_file.open(file_exists(player_file.name)?'r+':'w+');
			}
			player_file.iniSetObject(score.name,score);
			player_file.close();
			break;
	}
}