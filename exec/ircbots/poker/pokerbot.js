var poker_dir=this.dir;
var poker_scores=[];
var notice_interval=10; //seconds
var activity_timeout=120;
var poker_games=[];

// Game functions
function save()
{
	var s_file=new File(poker_dir + "scores.ini");
	if(!s_file.open(file_exists(s_file.name)?"r+":"w+")) return false;
	writeln("writing scores to file: " + s_file.name);
	for(s in poker_scores) {
		s_file.iniSetValue(null,s,poker_scores[s]);
	}
	s_file.close();
}
function main(srv,target)
{	
	var poker=poker_games[target];
	if(!poker || poker.paused) return;
	for(var u in srv.users) {
		if(poker.users[u] && poker.users[u].active) {
			if(time()-srv.users[u].last_spoke>=activity_timeout){
				poker_fold_player(target,srv,u);
				srv.o(u,"You have been idle too long. "
					+ "Type 'DEAL' here to resume playing poker.", "NOTICE");
			}
		}
	}
}

// Game objects
function Poker_Game()
{
	this.last_update=0;
	this.turn=0;
	this.pot=0;
	this.dealer=0;
	this.min_bet=10;
	this.current_bet=this.min_bet;
	this.lg_blind=this.min_bet;
	this.sm_blind=this.min_bet/2;
	this.round=-1;
	this.deck=new Deck();
	this.community_cards=new Array();
	this.paused=false;
	this.users=[];
	this.users_map=[];

	this.cycle=function()
	{
		if(time()-this.last_update>=notice_interval) {
			this.last_update=time();
			return true;
		}
		return false
	}
	this.deck.shuffle();
}
function Poker_Player()
{
	this.cards=[];
	this.money=1000;
	this.bet=0;
	this.has_bet=false;
	this.active=true;
}

load_scores();
