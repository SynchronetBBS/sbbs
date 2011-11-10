writeln("\nMAKEUSER v2.0 - Adds User to Synchronet User Database\n");

(function() {
	
	/* functions */
	function openCan(filename) {
		var list = [];
		if(file_exists(filename)) {
			var f = new File(filename);
			if(f.open('r',true)) {
				list = f.readAll();
				f.close();
			}
		}
		return list;
	}
	
	function scanCan(can,text) {
		for each(var i in can) {
			pattern = i;
			if(i.match(/~$/) != null)
				pattern = "*" + i + "*";
			if(wildmatch(text,i))
				return true;
		}
		return false;
	}

	function usage() {
		writeln('usage: jsexec.exe makeuser.js name [-param value] [...]');
		writeln('');
		writeln('params:');
		writeln('');
		writeln(' -P      Password');
		writeln(' -R      Real name');
		writeln(' -H      Chat handle');
		writeln(' -G      Gender (M or F)');
		writeln(' -B      Birth date (in MM/DD/YY or DD/MM/YY format)');
		writeln(' -T      Telephone number');
		writeln(' -N      Netmail (Internet e-mail) address');
		writeln(' -A      Street address');
		writeln(' -L      Location (city, state)');
		writeln(' -Z      Zip/Postal code');
		writeln(' -S      Security level');
		writeln(' -F#     Flag set #');
		writeln(' -FE     Exemption flags');
		writeln(' -FR     Restriction flags');
		writeln(' -E      Expiration days');
		writeln(' -C      Comment');
		writeln('');
		writeln('NOTE: multi-word user name and param values must be enclosed in "quotes"');
	}
	
	/* stuff */
	var error = false;
	var badnames = openCan(system.text_dir + "name.can");
	var badpws = openCan(system.text_dir + "password.can");
	
	/* properties */
	var alias = argv.shift();
	var pass = undefined;
	var name = undefined;
	var gender = undefined;
	var birthdate = undefined;
	var telephone = undefined;
	var email = undefined;
	var address = undefined;
	var location = undefined;
	var zip = undefined;
	var level = undefined;
	var flags1 = undefined;
	var flags2 = undefined;
	var flags3 = undefined;
	var flags4 = undefined;
	var exemptions = undefined;
	var restrictions = undefined;
	var expire = undefined;
	var comment = undefined;

	if(alias == undefined) {
		usage();
		return false;
	}
	if(system.matchuser(alias) > 0) {
		writeln("* A user by that name already exists");
		return false;
	}
	if(scanCan(badnames,alias)) {
		writeln("* Illegal user alias per " + system.text_dir + "name.can");
		return false;
	}

	/* process arguments */
	while(argv.length > 0) {
		var arg = argv.shift();
		switch(arg[1].toUpperCase()) {
		case "P":
			password = argv.shift();
			if(scanCan(badpws,password)) {
				writeln("* Illegal password per " + system.text_dir + "name.can");
				error = true;
			}
			break;
		case "R":
			name = argv.shift();
			if(scanCan(badnames,name)) {
				writeln("* Illegal user name per " + system.text_dir + "name.can");
				return false;
			}
			break;
		case "H":
			handle = argv.shift().substr(0,8);
			if(scanCan(badnames,handle)) {
				writeln("* Invalid user handle per " + system.text_dir + "name.can");
				return false;
			}
			break;
		case "G":
			gender = argv.shift();
			if(gender.match(/(m|f)/i) == null) {
				writeln("* gender must be 'M' or 'F'");
				error = true;
			}
			break;
		case "B":
			birthdate = new Date(argv.shift());
			if(birthdate == "Invalid Date") {
				writeln("* Invalid birth date");
				error = true;
			}
			break;
		case "T":
			telephone = argv.shift();
			if(!telephone.match(/\d{3}-?\d{3}-?\d{4}/)) {
				writeln("* Invalid telephone number");
				error = true;
			}
			break;
		case "N":
			email = argv.shift();
			if(!wildmatch(email,"*@*.*")) {
				writeln("* Invalid e-mail address");
				error = true;
			}
			break;
		case "A":
			address = argv.shift();
			break;
		case "L":
			location = argv.shift();
			break;
		case "Z":
			zip = argv.shift();
			if(!zip.match(/\d+/)) {
				writeln("* Invalid zip code");
				error = true;
			}
			break;
		case "S":
			level = Number(argv.shift());
			if(isNaN(level)) {
				writeln("* Invalid security level");
				error = true;
			}
			break;
		case "F":
			switch(arg[2].toUpperCase()) {
			case "1":
				flags1 = flags_str(argv.shift());
				break;
			case "2":
				flags2 = flags_str(argv.shift());
				break;
			case "3":
				flags3 = flags_str(argv.shift());
				break;
			case "4":
				flags4 = flags_str(argv.shift());
				break;
			case "E":
				exemptions = flags_str(argv.shift());
				break;
			case "R":
				restrictions = flags_str(argv.shift());
				break;
			default:
				writeln("* Invalid flag set");
				error = true;
				break;
			}
			break;
		case "E":
			expire = argv.shift();
			if(isNaN(expire)) {
				writeln("* Invalid expiration days");
				error = true;
			}
			break;
		case "C":
			comment = argv.shift();
			break;
		default:
			writeln("* Invalid argument " + arg);
			error = true;
			break;
		}
	}
	
	/* if there was an error parsing arguments, exit */
	if(error) {
		writeln("\nUser record creation unsuccessful.");
		return false;
	}
	
	/* create user and assign properties */
	var newuser = system.new_user(alias);

	/* assign user properties */
	if(password)
		newuser.security.password = password;
	if(name)
		newuser.name = name;
	if(gender)
		newuser.gender = gender;
	if(handle)
		newuser.handle = handle;
	if(birthdate)
		newuser.birthdate = birthdate;
	if(telephone)
		newuser.telephone = telephone;
	if(email)
		newuser.netmail = email;
	if(address)
		newuser.address = address;
	if(location)
		newuser.location = location;
	if(zip)
		newuser.zipcode = zip;
	if(level)
		newuser.security.level = level;
	if(flags1)
		newuser.security.flags1 = flags1;
	if(flags2)
		newuser.security.flags2 = flags2;
	if(flags3)
		newuser.security.flags3 = flags3;
	if(flags4)
		newuser.security.flags4 = flags4;
	if(exemptions)
		newuser.security.exemptions = exemptions;
	if(restrictions)
		newuser.security.restrictions = restrictions;
	if(expire)
		newuser.security.expiration_date = time() + (expire*24*60*60);
	if(comment)
		newuser.comment = comment;
	
	writeln("User record #" + newuser.number + " " + newuser.alias + " created successfully.");
})();

writeln();