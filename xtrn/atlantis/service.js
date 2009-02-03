var script_dir='.';
try { throw barfitty.barf(barf) } catch(e) { script_dir=e.fileName }
script_dir=script_dir.replace(/[\/\\][^\/\\]*$/,'');
script_dir=backslash(script_dir);

load("sockdefs.js");
load(script_dir+"gamedata.js");

function authenticated(password)
{
	var f;

	for(f in factions) {
		if(factions[f].addr==user && factions[f].password==password)
			return(true);
	}
}

function main()
{
	var allgames;
	var game=undefined;
	var user;
	var password;
	var f;
	var i;
	var tmp;
	var fa;

	f=new File(script_dir+"games.ini");
	f.open("r");
	allgames=f.iniGetAllObjects();
	f.close();

	do {
		switch(readln().toLowerCase()) {
			case 'help':
				writeln("Commands available:");
				writeln("quit        user        password    list_games  select_game get_summary");
				writeln("get_report  get_orders  new_player  send_orders");
				writeln("");
				writeln("OK");
				break;
			case 'quit':
				return;
			case 'user':
				write("User: ");
				user=readln();
				writeln("OK");
				break;
			case 'password':
				write("Pass: ");
				password=readln();
				writeln("OK");
				break;
			case 'list_games':
				for(i in allgames) {
					writeln(allgames[i].title);
				}
				writeln('');
				writeln('OK');
				break;
			case 'select_game':
				write("Game: ");
				game=undefined;
				game_dir=undefined;
				tmp=readln();
				for(i in allgames) {
					if(allgames[i].title==tmp)
						game=allgames[i];
				}
				if(game==undefined)
					writeln("Game not found!");
				else {
					game_dir=backslash(script_dir+game.dir);
					readgame();
					writeln('OK');
				}
				break;
			case 'get_summary':
				if(game==undefined) {
					writeln("No game selected!");
					break;
				}
				client.socket.sendfile(game_dir+"summary");
				writeln("END");
				writeln("OK");
				break;
			case 'new_player':
				if(game==undefined) {
					writeln("No game selected!");
					break;
				}
				if(factionbyaddr(user)!=null) {
					writeln("Already playing!");
					break;
				}
				write("Desired Faction Name: ");
				tmp=readln();
				for(i=0; ;i++) {
					f=new File(game_dir+"newplayers/"+i);

					if(!f.exists) {
						f.open("w");
						f.writeln("var addr="+user.toSource());
						f.writeln("var name="+tmp.toSource());
						f.writeln("var password="+password.toSource());
						f.close();
						break;
					}
				}
				writeln("OK");
				break;
			case 'get_report':
				if(game==undefined) {
					writeln("No game selected!");
					break;
				}
				if(!authenticated(password)) {
					writeln("Not authenticated!");
					break;
				}
				write("Turn: ");
				tmp=parseInt(readln());
				if(isNaN(tmp)) {
					writeln("Invalid turn!");
					break;
				}
				if(tmp > getturn()) {
					writeln("Please wait for the future to arrive.");
					break;
				}
				if(!client.socket.sendfile(game_dir+'reports/'+fa.no+'.'+(tmp))) {
					writeln("END");
					writeln("Unable to read report!");
					break;
				}
				writeln("END");
				writeln("OK");
				break;
			case 'get_orders':
				if(game==undefined) {
					writeln("No game selected!");
					break;
				}
				if(!authenticated(password)) {
					writeln("Not authenticated!");
					break;
				}
				write("Turn: ");
				tmp=parseInt(readln());
				if(isNaN(tmp)) {
					writeln("Invalid turn!");
					break;
				}
				if(tmp > getturn()) {
					writeln("Please wait for the future to arrive.");
					break;
				}
				if(!client.socket.sendfile(game_dir+'orders/'+fa.no+'.'+(tmp))) {
					writeln("END");
					writeln("Unable to read report!");
					break;
				}
				writeln("END");
				writeln("OK");
				break;
			case 'send_orders':
				if(game==undefined) {
					writeln("No game selected!");
					break;
				}
				if(!authenticated(password)) {
					writeln("Not authenticated!");
					break;
				}
				fa=factionbyaddr(user);
				f=new File(game_dir+'orders/'+fa.no+'.'+(turn+1));
				writeln("Send orders now... end with \"END\" on a line by itself");
				while((tmp=readln())!="END")
					f.writeln(tmp);
				f.close();
				writeln("OK");
				break;
			default:
				writeln("Unknown command!");
				break;
		}
	} while(1);
}

main();
