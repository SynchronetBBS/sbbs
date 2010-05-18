poker_dir=this.dir;
poker_scores=[];
notice_interval=10; //seconds
activity_timeout=120;
poker_games=[];

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
	var active_users=false;
	for(var u in srv.users) {
		if(poker.users[u]) {
			if(time()-srv.users[u].last_spoke<activity_timeout){
				active_users=true;
				break;
			} else {
				delete poker.users[u];
				srv.o(u,"You have been idle too long. "
					+ "Type 'DEAL' here to resume playing poker.", "NOTICE");
			}
		}
	}
	if(!active_users) return;
	
	if(poker.cycle()) {
	}
}

// Game objects
function Poker_Game()
{
	this.last_update=0;
	this.turn=0;
	this.pot=0;
	this.lg_blind=10;
	this.sm_blind=5;
	this.dealer=0;
	this.min_bet=this.sm_blind;
	this.current_bet=this.min_bet;
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
	this.money=100;
	this.bet=0;
	this.active=true;
}

load_scores();
