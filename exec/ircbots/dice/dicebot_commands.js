Bot_Commands["ROLL"] = new Bot_Command(0,false,false);
Bot_Commands["ROLL"].usage = 
	get_cmd_prefix() + "ROLL <num_dice>d<num_sides>";
Bot_Commands["ROLL"].help = 
	"This is the command used to roll dice.";
Bot_Commands["ROLL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var num_dice;
	var sides_per_die;
	var totals=[];
	var die_max=100000;
	var roll_max=10;
	
	if(!cmd[0]) {
		/* If no arguments are supplied, assume a roll of two six-sided dice.	*/
		num_dice=2;
		sides_per_die=6;
		totals.push(roll_them_dice(num_dice,sides_per_die));
	} else {
		var rolls=0;
		while(cmd.length && rolls<roll_max){
			var args=cmd.shift().toUpperCase().split("D");
			num_dice=js.eval(args[0]);
			sides_per_die=js.eval(args[1]);
			if(!num_dice>0 || !sides_per_die>0) {
				srv.o(target,"Invalid arguments.");
				return;
			}
			if(num_dice>die_max || sides_per_die>die_max) {
				srv.o(target,"I think not, sir.");
				return;
			}
			totals.push(roll_them_dice(num_dice,sides_per_die));
			rolls++;
		}
	}
	srv.o(target,onick + " rolled: " + totals.join(", "));
	return;
}

Bot_Commands["DICE"] = new Bot_Command(0,false,false);
Bot_Commands["DICE"].help = 
	"To roll some dice, type '" + get_cmd_prefix() + "ROLL <num_dice>d<num_sides>'. " +
	"For a full list of commands, type '" + get_cmd_prefix() + "HELP'.";
Bot_Commands["DICE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	srv.o(target,"Help: " + this.help);
	return;
}


