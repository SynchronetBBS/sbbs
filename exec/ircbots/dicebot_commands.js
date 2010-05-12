this.Bot_Commands["ROLL"] = new Bot_Command(0,false,false);
this.Bot_Commands["ROLL"].usage = 
	get_cmd_prefix() + "ROLL <num_dice>d<num_sides>";
this.Bot_Commands["ROLL"].help = 
	"This is the command used to roll dice.";
this.Bot_Commands["ROLL"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	var num_dice;
	var sides_per_die;
	
	if(!cmd[0]) {
		/* If no arguments are supplied, assume a roll of two six-sided dice.	*/
		num_dice=2;
		sides_per_die=6;
	} else {
		var args=cmd[0].toUpperCase().split("D");
		num_dice=args[0];
		sides_per_die=args[1];
	}
	if(!num_dice>0 || !sides_per_die>0) {
		srv.o(target,"Invalid arguments.");
		return;
	}
	
	var total=roll_them_dice(num_dice,sides_per_die);
	srv.o(target,onick + " rolled: " + total);
	return;
}

this.Bot_Commands["DICE"] = new Bot_Command(0,false,false);
this.Bot_Commands["DICE"].help = 
	"To roll some dice, type '" + get_cmd_prefix() + "ROLL <num_dice>d<num_sides>'. " +
	"For a full list of commands, type '" + get_cmd_prefix() + "HELP'.";
this.Bot_Commands["DICE"].command = function (target,onick,ouh,srv,lvl,cmd) {
	srv.o(target,"Help: " + this.help);
	return;
}


