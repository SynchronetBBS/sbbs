Bot_Commands["HUMANITY"] = new Bot_Command(0,false,false);
Bot_Commands["HUMANITY"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var g = target.toUpperCase();
	if(!humanities[g]) {
		srv.o(target,"<Cards Against Humanity> game started");
		humanities[g]=new Humanity(srv,g);
	}
	if(!humanities[g].users[onick]) {
		srv.o(target,"<Cards Against Humanity> " + onick + " joined");
		srv.writeout("NOTICE " + onick + " :To submit cards, type '!submit <card>' in " + target) ;
		srv.writeout("NOTICE " + onick + " :For multiple submissions, space separate cards (!submit <card> <card> etc..)");
		humanities[g].users[onick] = new Player(onick);
		humanities[g].deal(onick,10);
		show_white_cards(srv,target,humanities[g],humanities[g].users[onick]);
	}
	else {
		srv.o(target,"<Cards Against Humanity> " + onick + " left");
		delete humanities[g].users[onick];
		if(countMembers(humanities[g].users) == 0) {
			srv.o(target,"<Cards Against Humanity> game stopped");
			delete humanities[g];
		}
	}
	return;
}

Bot_Commands["SUBMIT"] = new Bot_Command(0,false,false);
Bot_Commands["SUBMIT"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var g = target.toUpperCase();
	if(humanities[g] && humanities[g].users[onick]) {
		var game = humanities[g];
		if(game.currentSet.submissions[onick] || game.status != status.WAITING) {
			srv.o(target,"That is false, " + onick);
		}
		else {
			var picks = [];
			game.currentSet.submissions[onick] = "";
			cmd.shift();
			for(var c=0;c<game.currentSet.params;c++) {
				var cnum = Number(cmd.shift()) - 1;
				var card = game.users[onick].cards[cnum];
				if(!card) {
					srv.o(target,"That is false, " + onick);
					return;
				}
				picks.push(cnum);
			}
			for(var p=0;p<picks.length;p++) {
				var card = picks[p];
				game.currentSet.submissions[onick] +=  ", " + game.users[onick].cards[card];
				game.users[onick].cards[card] = undefined;
			}
			for(var c=0;c<game.users[onick].cards.length;c++) {
				if(game.users[onick].cards[c] == undefined)
					game.users[onick].cards.splice(c--,1);
			}
		}
	}
}

Bot_Commands["HUMANITY!"] = new Bot_Command(0,false,false);
Bot_Commands["HUMANITY!"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var g = target.toUpperCase();
	if(humanities[g] && humanities[g].users[onick]) {
		humanities[g].status = status.FINISHED;
	}
}

Bot_Commands["PICK"] = new Bot_Command(0,false,false);
Bot_Commands["PICK"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var g = target.toUpperCase();
	if(humanities[g] && humanities[g].users[onick]) {
		cmd.shift();
		var game = humanities[g];
		if(game.status != status.VOTING) {
			srv.o(target,"That is false, " + onick);
		}
		else {
			var vote = Number(cmd.shift()) - 1;
			game.currentSet.votes[onick] = vote;
//			srv.o(target,onick + " voted for submission #" + (vote+1));
		}
	}
}

Bot_Commands["HUMANITY?"] = new Bot_Command(0,false,false);
Bot_Commands["HUMANITY?"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var g = target.toUpperCase();
	if(humanities[g]) {
		var game = humanities[g];
		var str = "";
		for(var u in game.users) {
			str+=", " + u;
		}
		srv.o(target,"Users Against Humanity: " + str.substr(2));
		show_black_card(srv,target,game);
		show_white_cards(srv,target,game,game.users[onick]);
	}
}

Bot_Commands["SCORE"] = new Bot_Command(0,false,false);
Bot_Commands["SCORE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	show_scores(srv,target);
}