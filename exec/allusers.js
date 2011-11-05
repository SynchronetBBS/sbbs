// ALLUSERS - Bulk User Editor for Synchronet User Database

// usage: jsexec allusers.js [[-require] [...]] /modify [[/modify] [...]]

// where require is one of:
       // L#                  			set minimum level to # (default=0)
       // M#                  			set maximum level to # (default=99)
       // F#[+|-]<flags>           		set required flags from flag set #
       // E[+|-]<flags>           		set required exemptions
       // R[+|-]<flags>            		set required restrictions

// where modify is one of:
       // L#                  			change security level to #
       // F#[+|-]<flags>      			add or remove flags from flag set #
       // E[+|-]<flags>       			add or remove exemption flags
       // R[+|-]<flags>       			add or remove restriction flags

// Examples:
       // jsexec allusers.js -L30 /FA   add 'A' to flag set #1 for all level 30+ users
       // jsexec allusers.js /F3-G      remove 'G' from flag set #3 for all users
       // jsexec allusers.js -F2B /E-P  remove 'P' exemption for all users with FLAG '2B'
       // jsexec allusers.js /R+W       add 'W' restriction for all users

(function() {
	
	/* required values for user matching */	 
	var match_rule = {
		flags:{
			1:undefined,
			2:undefined,
			3:undefined,
			4:undefined},
		min_level:undefined,
		max_level:undefined,
		restrictions:undefined,
		exemptions:undefined
	};

	/* items to edit in matched users */
	var edit_rule = {
		flags:{
			1:undefined,
			2:undefined,
			3:undefined,
			4:undefined},
		level:undefined,
		restrictions:undefined,
		exemptions:undefined
	}

	/* apply argument to match rule */
	function setMatchRule(str) {
		var min_level = 0;
		var max_level = 99;
		var flag_set = 1;
		var flags = undefined;
		var exemptions = undefined;
		var restrictions = undefined;
		
		switch(str[0].toUpperCase()) {
			case "L":
				min_level = str.substr(1);
				break;
			case "M":
				max_level = str.substr(1);
				break;
			case "F":
				if(isNaN(str[1])) {
					flags = str;
				}
				else {
					flag_set = str[1];
					flags = str.substr(2);
				}
				break;
			case "E":
				exemptions = str.substr(1);
				break;
			case "R":
				restrictions = str.substr(1);
				break;
			default:
				throw("invalid match rule: " + str);
				break;
		}
		
		if(flag_set < 1 || flag_set > 4)
			throw("invalid flag set: " + flag_set);
			
		if(min_level && !match_rule.min_level)
			match_rule.min_level = min_level;
		if(max_level && !match_rule.max_level)
			match_rule.max_level = max_level;
		if(flags && !match_rule.flags[flag_set])
			match_rule.flags[flag_set] = parseFlags(flags);
		if(restrictions && !match_rule.restrictions)
			match_rule.restrictions = parseFlags(restrictions);
		if(exemptions && !match_rule.exemptions)
			match_rule.exemptions = parseFlags(exemptions);
	}
	
	/* apply argument to edit rule */
	function setEditRule(str) {
		var level = undefined;
		var flag_set = 1;
		var flags = undefined;
		var exemptions = undefined;
		var restrictions = undefined;

		switch(str[0].toUpperCase()) {
			case "L":
				level = str.substr(1);
				break;
			case "F":
				if(isNaN(str[1])) {
					flags = str;
				}
				else {
					flag_set = str[1];
					flags = str.substr(2);
				}
				break;
			case "E":
				exemptions = str.substr(1);
				break;
			case "R":
				restrictions = str.substr(1);
				break;
			default:
				throw("invalid edit rule: " + str);
				break;
		}
		
		if(flag_set < 1 || flag_set > 4)
			throw("invalid flag set: " + flag_set);
			
		if(level && !edit_rule.level)
			edit_rule.level = level;
		if(flags && !edit_rule.flags[flag_set])
			edit_rule.flags[flag_set] = parseFlags(flags);
		if(restrictions && !edit_rule.restrictions)
			edit_rule.restrictions = parseFlags(restrictions);
		if(exemptions && !edit_rule.exemptions)
			edit_rule.exemptions = parseFlags(exemptions);
	}

	/* parse a +/- flag string and return a flag set/unset object */
	function parseFlags(str) {
		var flags = {
			set:undefined,
			unset:undefined
		};
		var set = true;
		
		str = str.split("");
		for each(var f in str) {
			if(f == "+")
				set = true;
			else if(f == "-")
				set = false;
			else if(flags_str(f) > 0) {
				if(set) 
					flags.set |= flags_str(f);
				else
					flags.unset |= flags_str(f);
			}
			else {
				throw("invalid flag: " + f);
			}
		}
		
		return flags;
	}

	/* validate rule parameters */
	function validateRule(rule) {
		if(rule.level && rule.level < 1 || rule.level > 99)
			throw("invalid security level: " + rule.level);
		if(rule.min_level && isNaN(rule.min_level) || rule.min_level < 1 || rule.min_level > 99)
			throw("invalid minimum security level: " + rule.min_level);
		if(rule.max_level && isNaN(rule.max_level) || rule.max_level < 1 || rule.max_level > 99)
			throw("invalid maximum security level: " + rule.max_level);
		if(rule.min_level && rule.min_level > rule.max_level)
			throw("maximum security level must be higher than minimum security level");
	}

	/* return a list of users that fit the match rule */
	function matchUsers(rule) {
		var matches = [];
		
		user_loop:
		for(var u = 1; u < system.lastuser; u++) {
			var usr = new User(u);
			
			if(usr.security.level < rule.min_level)
				continue user_loop;
				
			if(usr.security.level > rule.max_level)
				continue user_loop;
				
			flag_loop:
			for(var s in rule.flag_set) {
				if(!rule.flag_set[s])
					continue flag_loop; 
				if(usr.security["flags" + s] & rule.flags.set == 0)
					continue user_loop;
				if(usr.security["flags" + s] & rule.flags.unset > 0)
					continue user_loop;
			}
			
			if(rule.exemptions) {
				if(usr.security.exemptions & rule.exemptions.set == 0)
					continue user_loop;
				if(usr.security.exemptions & rule.exemptions.unset > 0)
					continue user_loop;
			}

			if(rule.restrictions) {
				if(usr.security.restrictions & rule.restrictions.set == 0)
					continue user_loop;
				if(usr.security.restrictions & rule.restrictions.unset > 0)
					continue user_loop;
			}
			
			matches.push(usr);
		}
		
		return matches;
	}

	/* edit them bitches */
	function editUsers(matches, rule) {
		log("rule: " + rule.toSource());
		for each(var m in matches) {
			if(rule.level) 
				m.security.level = Number(rule.level);
				
			flag_loop:
			for(var s in rule.flags) {
				if(!rule.flags[s])
					continue flag_loop;
				m.security["flags" + s] |= rule.flags[s].set;
				m.security["flags" + s] &= ~rule.flags[s].unset;
			}
			
			if(rule.exemptions) {
				m.security.exemptions |= rule.exemptions.set;
				m.security.exemptions &= ~rule.exemptions.unset;
			}

			if(rule.restrictions) {
				m.security.restrictions |= rule.restrictions.set;
				m.security.restrictions &= ~rule.restrictions.unset;
			}
			
			writeln("Modified user record #" + m.number);
		}
		writeln("Modified " + matches.length + " record(s)");
	}

	/* parse command arguments */
	while(argv.length > 0) {
		var arg = argv.shift();
		switch(arg[0]) {
			case "-":
				setMatchRule(arg.substr(1));
				break;
			case "/":
				setEditRule(arg.substr(1));
				break;
			default:
				throw("invalid argument: " + arg);
				break;
		}
	}

	/* validate matching and editing rules */
	validateRule(match_rule);
	validateRule(edit_rule);

	/* search user list and return matches */
	var matches = matchUsers(match_rule);
	
	/* edit matched users */
	if(matches.length > 0)
		editUsers(matches, edit_rule);
		
})();


