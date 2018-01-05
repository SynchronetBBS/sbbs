var dir=this.dir;
var status = {STARTING:0,WAITING:1,VOTING:2,FINISHED:3};
var scores = load_scores();
var interval = 5000;
var humanities={};

function save() {
	var f=new File(dir + "scores.ini");
	if(!f.open(file_exists(f.name)?"r+":"w+")) 
		return false;
	f.iniSetObject(null,scores);
	f.close();

}

function load_scores() {
	var f=new File(dir + "scores.ini");
	if(!f.open("r",true))
		return {};
	var s = f.iniGetObject();
	f.close();
	return s;
}

function main(srv,target) {	
	//log(target.toUpperCase());
	var game=humanities[target.toUpperCase()];

	/* if there is no game active, do not cycle */
	if(!game) {
		return;
	}
	
	var now = Date.now();
	if(now - game.lastActivity > interval) {
		game.lastActivity = now;
		switch(game.status) {
		case status.STARTING:
			var c = get_black_card(game);
			if(c.indexOf("(2)") >= 0) {
				game.currentSet.params = 2;
			}
			else if(c.indexOf("(draw 2, pick 3)") >= 0 || c.indexOf("(Draw 2, Pick 3)") >= 0 ) {
				game.currentSet.params = 3;
				for(var p in game.users) {
					game.deal(p,2);
					show_white_cards(srv,target,game,game.users[p]);
				}
			}
			else {
				game.currentSet.params = 1;
			}
			game.currentSet.black = c;
			game.status = status.WAITING;
			show_black_card(srv,target,game);
			break;
		case status.WAITING:
			var ready = true;
			for each(var u in game.users) {
				if(!game.currentSet.submissions[u.nick]) {
					ready = false;
					break;
				}
			}
			if(ready) {
				show_submissions(srv,target,game);
				game.status = status.VOTING;
			}
			break;
		case status.VOTING:
			var ready = true;
			for(var u in game.currentSet.submissions) {
				if(game.currentSet.votes[u] == undefined) {
					ready = false;
					break;
				}
			}
			if(ready) {
				show_results(srv,target,game);
				game.status = status.FINISHED;
			}
			break;
		case status.FINISHED:
			for(var u in game.currentSet.submissions) {
				if(game.users[u]) {
					if(game.currentSet.params == 2)
						game.deal(u,2);
					else
						game.deal(u,1);
					show_white_cards(srv,target,game,game.users[u]);
				}
			}
			game.currentSet.submissions={};
			game.currentSet.votes={};
			game.status = status.STARTING;
			break;
		}
	}
}

function Humanity(srv,target) {
	this.chan=target;
	this.users={};
	this.currentSet={
		submissions:{},
		votes:{},
		black:undefined,
		params:undefined
	};
	this.status=status.STARTING;
	this.lastActivity=Date.now()+5000;
	
	this.black_cards=load_cards(dir+'black_cards.txt');
	this.white_cards=load_cards(dir+'white_cards.txt');
	this.white_cards=this.white_cards.concat(load_cards(dir+'apple_cards.txt'));
	
	this.deal = function(nick, qty) {
		var usr = this.users[nick];
		for(var i=0;i<qty;i++) {
			var c = get_white_card(this);
			usr.cards.push(c);
		}
	}
}

function Player(nick) {
	this.nick = nick;
	this.cards=[];
	this.score = 0;
}