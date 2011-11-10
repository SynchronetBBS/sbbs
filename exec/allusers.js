/* 
 *	ALLUSERS - Bulk User Editor for Synchronet User Database
 * 
 *	usage: jsexec allusers.js [[ARS] [...]] [[/modify] [...]]
 * 
 *	where ARS is any valid ARS string 
 *		(see http://www.synchro.net/docs/security.html)
 *	
 *	where modify is one of:
 *		L#                  			change security level to #
 *		F#[+|-]<flags>      			add or remove flags from flag set #
 *		E[+|-]<flags>       			add or remove exemption flags
 *		R[+|-]<flags>       			add or remove restriction flags
 * 
 *	examples:
 *
 *	jsexec allusers.js "L30" /FA  		add 'A' to flag set #1 for all level 30+ users
 *	jsexec allusers.js /F3-G      		remove 'G' from flag set #3 for all users
 *	jsexec allusers.js F2B /E-P 		remove 'P' exemption for all users with FLAG '2B'
 *	jsexec allusers.js "60$XA" /R+W 	add 'W' restriction for all level 60+ users 
 *										with exemption 'A'
 *
 *	NOTE: 	multi-word ARS strings or ARS strings with special characters
 *			must be enclosed in "quotes"
 */

(function() {
	
	/* required values for user matching */	 
	var match_rules = [];

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

	/* apply argument to edit rule */
	function setEditRule(str) {
		
		str = str.substr(1);
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
		
		if(level && level < 1 || level > 99)
			throw("invalid security level: " + level);
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

	/* return a list of users that fit the match rule */
	function matchUsers() {
		var matches = [];
		user_loop:
		for(var u = 1; u < system.lastuser; u++) {
			var usr = new User(u);
			for each(var m in match_rules) {
				if(!usr.compare_ars(m))
					continue user_loop;
			}
			matches.push(usr);
		}
		return matches;
	}

	/* edit them bitches */
	function editUsers(matches, rule) {
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
		if(arg[0] == "/") 
			setEditRule(arg);
		else
			match_rules.push(arg);
	}

	/* search user list and return matches */
	var matches = matchUsers();
	
	/* edit matched users */
	editUsers(matches, edit_rule);
		
})();


