function load_cards(fileName) {
	var file=new File(fileName);
	file.open('r',true);
	var c=file.readAll();
	file.close();
	return parse_cards(c);
}

function parse_cards(card_array) {
	var cards = [];
	for(var c=0;c<card_array.length;c++) {
		var str = card_array[c].split(';');
		cards.push(str.shift());
	}
	return cards;
}

function get_white_card(game) {
	var rand = random(game.white_cards.length);
	return game.white_cards.splice(rand,1);
}

function get_black_card(game) {
	var rand = random(game.black_cards.length);
	return game.black_cards.splice(rand,1)[0];
}

function get_privmsg_channel(srv,cmd) {
	return srv.channel[cmd[0].toUpperCase()];
}

function show_black_card(srv,target,game) {
	if(game.currentSet.black) {
		srv.o(target,"<Black Card>");
		srv.o(target,game.currentSet.black);
	}
}

function show_white_cards(srv,target,game,player) {
	if(player && player.cards.length > 0) {
		var str="<white cards> ";
		for(var c=0;c<player.cards.length;c++) {
			str += (c+1) + ": " + player.cards[c] + " ";
		}
		srv.writeout("NOTICE " + player.nick + " :" + str);
	}
}

function show_submissions(srv,target,game) {
	var sub = [];
	for(var s in game.currentSet.submissions) 
		sub.push(s);
	game.shuffled = shuffle(sub);
	srv.o(target,"<Submissions>");
	for(var s=0;s<sub.length;s++) {
		var uname = sub[s];
		var ss = game.currentSet.submissions[uname];
		srv.o(target,(s+1) + ": " + ss.substr(2));
	}
}

function show_results(srv,target,game) {
	var votes = {};
	for each(var v in game.currentSet.votes) {
		if(!votes[v])
			votes[v] = 0;
		votes[v]++;
	}
	srv.o(target,"<Results>");
	for(var s=0;s<game.shuffled.length;s++) {
		var uname = game.shuffled[s];
		var ss = game.currentSet.submissions[uname];
		if(!votes[s])
			votes[s] = 0;
		srv.o(target,uname + ": " + ss.substr(2) + " (" + votes[s] + ")");
		if(!scores[uname])
			scores[uname] = 0;
		scores[uname] += votes[s];
	}
}

function show_scores(srv,target) {
	srv.writeout("NOTICE " + target + " :<Cards Against Humanity> score list");
	for(var s in scores) {
		srv.o(target,format("%-25s " + scores[s],s));
	}
}