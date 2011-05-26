/* 	
	Javascript CVS client for Synchronet (2011)
	-	code by mcmlxxix
*/

CVS = new (function () {
	this.VERSION = "$Revision$".split(' ')[1];
	this.socket = undefined;
	
////////////////////////////////// SETTINGS

	/* connection defaults */
	this.CVSHOME = argv[0];
	this.CVSSERV = "cvs.synchro.net";
	this.CVSROOT = "/cvsroot/sbbs";
	this.CVSUSER = "anonymous";
	this.CVSPASS = "";
	this.CVSAUTH = "pserver";
	
	/* default global options */
	this.globalOptions = {
		'z':					-1,	// compression level (0 - 9)
		'x':					-1,	// encryption
		'r':					-1,	// read-only on new files
		'w':					-1	// read/write on new files
	}
	
	/* default common command options */
	this.commonOptions = {
		'P':					true, 	// prune empty directories 
		'R':					true	// recursive directory searching
	}
	
	/* default file filters */
	this.filter = {
		'.':					true,
		'..':					true,
		'core':					true,
		'RCSLOG':				true,
		'tags':					true,
		'TAGS':					true,
		'RCS':					true,
		'.make.state':			true,
		'.nse_depinfo':			true,
		'#*':					true,
		'.#*':					true,
		'cvslog.*':				true,
		',*':					true,
		'CVS':					true,
		'CVS.adm':				true,
		'.del-*':				true,
		'*.a':					true,
		'*.olb':				true,
		'*.o':					true,
		'*.obj':				true,
		'*.so':					true,
		'*.Z':					true,
		'*~':					true,
		'*.old':				true,
		'*.elc':				true,
		'*.ln':					true,
		'*.bak':				true,
		'*.BAK':				true,
		'*.orig':				true,
		'*.rej':				true,
		'*.exe':				true,
		'*.dll':				true,
		'*.pdb':				true,
		'*.lib':				true,
		'*.exp':				true,
		'_$*':					true,
		'*$':					true
	}
	
	/* default accepted responses  */
	this.validResponses = {
		'Checked-in':			true,
		'Checksum':				false,
		'Clear-rename':			false,
		'Clear-static-directory':false,
		'Clear-sticky':			false,
		'Copy-file':			true,
		'Created':				false,
		'E':					true,
		'EntriesExtra':			false,
		'F':					true,
		'M':					true,
		'MT':					true,
		'Mbinary':				false,
		'Merged':				true,
		'Mod-time':				true,
		'Mode':					false,
		'Module-expansion':		false,
		'New-entry':			false,
		'Notified':				false,
		'Patched':				false,
		'Rcs-diff':				false,
		'Remove-entry':			true,
		'Removed':				true,
		'Renamed':				false,
		'Set-checkin-prog':		false,
		'Set-static-directory':	false,
		'Set-sticky':			false,
		'Set-update-prog':		false,
		'Template':				false,
		'Update-existing':		true,
		'Updated':				true,
		'Valid-requests':		true,
		'Wrapper-rcsOption':	false,
		'error':				true,
		'ok':					true,
	};
	
	/* populated by response to valid-requests  */
	this.validRequests = {
	};

////////////////////////////////// METHODS
	
	/* connect to server and authenticate */
	this.connect = function(srv,root,usr,pw) {
	
		if(srv)
			this.CVSSERV = srv;
		if(root)
			this.CVSROOT = root;
		if(usr)
			this.CVSUSER = usr;
		if(pw)
			this.CVSPASS = pw;
	
		this.socket=new Socket();
		this.socket.connect(this.CVSSERV,2401);
		this.authenticate();
		this.init();
	}
	
	/* initialize protocol */
	this.init = function() {
		this.protocol["Valid-responses"]();
		this.protocol["valid-requests"]();
		this.protocol["Root"](this.CVSROOT);
	}
	
	/* authenticate */
	this.authenticate = function() {
		/* authenticate client */
		switch(this.CVSAUTH.toUpperCase()) {
		case "PSERVER":
			/* begin auth */
			this.request("BEGIN AUTH REQUEST");
			/* send cvs root */
			this.request(this.CVSROOT);
			/* send cvs username */
			this.request(this.CVSUSER);
			/* send cvs password */
			this.socket.send("A");
			for(var c=0;c<this.CVSPASS.length;c++) 
				this.socket.sendBin(encode_char(this.CVSPASS[c]),1);
			this.socket.send("\n");
			this.request("END AUTH REQUEST");
			/* end auth  */
			break;
		case "GSSAPI":
			// todo??
			break;
		}
		
		var response=this.response;
		if(response == "I LOVE YOU") {
			log(LOG_DEBUG,"authenticated");
			return true;
		}
		if(response == "I HATE YOU") {
			log(LOG_ERROR,"authentication failed");
			return false;
		}
		else {
			log(LOG_ERROR,response);
			return false;
		}
	}

	/* close server socket connection */
	this.disconnect = function() {
		this.socket.close();
	}
	
	/* send a request or command to the cvs server */
	this.request = function(cmdstr) {
		this.verifyConnection();
		this.socket.send(cmdstr + "\n");
	}
	
	/*
	 * check for a response from the cvs server 
	 * This will handle whatever it can, and return
	 * only unhandled responses.
	 * 
	 * While it "handles" error, it still returns it.
	 */
	
	this.response getter = function() {
		for(;;) {
			this.verifyConnection();
			if(!this.socket.poll(1))
				return undefined;
			var response=this.socket.recvline(4096,10);
			var space=response.indexOf(' ');
			if(space < 1)
				return response;
			var cmd=response.slice(0, space-1)
			switch(cmd) {
				case 'M':
					log(LOG_INFO, response.substr(space+1));
					if(js.global.writeln)
						writeln(response.substr(space+1));
					break;
				case 'MT':
					var m=response.split(' ',3);
					switch(m[1]) {
						case 'newline':
							if(js.global.writeln)
								writeln('');
							break;
						case 'text':
							log(LOG_INFO, m[2]);
							if(js.global.write)
								write(m[2]);
							break;
						default:
							if(m.length > 2) {
								log(LOG_INFO, m[2]);
								if(js.global.write)
									write(m[2]);
							}
							break;
					}
					break;
				case 'E':
					log(LOG_ERR, response.substr(space+1));
					if(js.global.writeln)
						writeln(response.substr(space+1));
					break;
				case 'F':
					break;
				case 'error':
					var m=response.split(' ',3);
					if(m[1].length > 0) {
						log(LOG_ERR, "ERROR "+m[1]+" - "+m[2]);
					}
					else {
						log(LOG_ERR, "ERROR - "+m[2]);
					}
					return response;
				default:
					return response;
			}
		}
	}

	/* verify that socket is connected to the server */
	this.verifyConnection = function() {
		if(!this.socket || !this.socket.is_connected) {
			this.connect();
			this.init();
			if(!this.socket.is_connected) {
				throw("error connecting to server");
			}
		}
	}

////////////////////////////////// COMMAND LINE API

	/* parse a standard cvs command line string */
	this.command = function(str) {
		//  cvs [ cvs_options ] cvs_command [ command_options ] [ command_args ]
		var cmd=str.split(" ");
		
		/* if cvs has been passed as part of the command, strip it */
		if(cmd[0].toUpperCase() == "CVS")
			cmd.shift();
		
		
	}

////////////////////////////////// PROTOCOL API

	/* cvs protocol commands */
	this.protocol = {

		//TODO ---> http://www.cvsnt.org/cvsclient/Requests.html
	
		/* request a list of valid server requests */
		'valid-requests':function() {
			this.parent.request('valid-requests');
			var requests=this.parent.response.split(" ");
			for each(var r in requests) {
				this.parent.validRequests[r] = true;
			}
		},
		
		/* tell server what responses we can handle */
		'Valid-responses':function() {
			var str="Valid-responses";
			for(var r in this.parent.validResponses) {
				if(this.parent.validResponses[r] == true)
					str+=" "+r;
			}
			this.parent.request(str);
		},
		
		/* specify CVSROOT on server */
		'Root':function(pathname) {
			this.parent.request("Root " + pathname);
		},
		
		/* specify directory on server */
		'Directory':function(dir,repository) {
			this.parent.request("Directory " + local-directory);
			/*
				repository \n
			*/
		},
		
		/* tell server local file revision */
		'Entry':function(str) {
			
		},
		
		/* tell server that a file has not been modified since last update or checkout */
		'Unchanged':function(filename) {
			this.parent.request("Unchanged " + filename);
		},
		
		/* send a pre-command-processing argument */
		'Argument':function(str) {
			this.parent.request("Argument " + str);
		},

		/* append a pre-command-processing argument */
		'Argumentx':function(str) {
			this.parent.request("Argumentx " + str);
		},
		
		/* notify server that an edit/unedit command took place */
		'Notify':function(filename) {
			this.parent.request("Notify " + filename);
			/*
				notification-type \t time \t clienthost \t
				working-dir \t watches \n
			*/
		},
		
		'Questionable':function(filename) {
			this.parent.request("Questionable " + filename);
		},
		
		'Case':function() {
			this.parent.request("Case");
		},
		
		'Utf8':function() {
			this.parent.request("Utf8");
		},
		
		'Global_option':function(option) {
			this.parent.request("Global_option " + option);
		},
	};
	this.protocol.parent=this;
	
////////////////////////////////// CLIENT API

	/* cvs client commands */
	this.commands = {
	
		//TODO ---> http://ximbiot.com/cvs/manual/cvs-1.11.23/cvs_16.html
		
		/* retrieve file updates */
		'update':function(dir,files,options) {
		
		},
		/* checkout files */
		'checkout':function(repository,options) {
		
		},
		/* add files */
		'add':function(dir,files,message,options) {
		
		},
		/* administration */
		'admin':function(dir,files,options) {
		
		},
		/* display which revision modified each line of a file */
		'annotate':function(dir,filename,options) {
		
		},
		/* commit changes to repository */
		'commit':function(dir,files,message,options) {
		
		},
		/* compare one file revision to another */
		'diff':function(dir,filename,options) {
		
		},
		/* export source */
		'export':function(repository,options) {
		
		},
		/* import source into cvs */
		'import':function(repository,dir,options) {
		
		},
		/* remove files from repository */
		'remove':function(dir,files,options) {
		
		}
	}

////////////////////////////////// INTERNAL FUNCTIONS

	/* cvs password encryption */
	function encode_char(ch) {
		var values=[];
		values["!"]=120;
		values['"']=53;
		values["%"]=109;
		values["&"]=72;
		values["'"]=108;
		values["("]=70;
		values[")"]=64;
		values["*"]=76;
		values["+"]=67;
		values[","]=116;
		values["-"]=74;
		values["."]=68;
		values["/"]=87;
		values["0"]=111;
		values["1"]=52;
		values["2"]=75;
		values["3"]=119;
		values["4"]=49;
		values["5"]=34;
		values["6"]=82;
		values["7"]=81;
		values["8"]=95;
		values["9"]=65;
		values[":"]=112;
		values[";"]=86;
		values["<"]=118;
		values["="]=110;
		values[">"]=122;
		values["?"]=105;
		values["A"]=57;
		values["B"]=83;
		values["C"]=43;
		values["D"]=46;
		values["E"]=102;
		values["F"]=40;
		values["G"]=89;
		values["H"]=38;
		values["I"]=103;
		values["J"]=45;
		values["K"]=50;
		values["L"]=42;
		values["M"]=123;
		values["N"]=91;
		values["O"]=35;
		values["P"]=125;
		values["Q"]=55;
		values["R"]=54;
		values["S"]=66;
		values["T"]=124;
		values["U"]=126;
		values["V"]=59;
		values["W"]=47;
		values["X"]=92;
		values["Y"]=71;
		values["Z"]=115;
		values["_"]=56;
		values["a"]=121;
		values["b"]=117;
		values["c"]=104;
		values["d"]=101;
		values["e"]=100;
		values["f"]=69;
		values["g"]=73;
		values["h"]=99;
		values["i"]=63;
		values["j"]=94;
		values["k"]=93;
		values["l"]=39;
		values["m"]=37;
		values["n"]=61;
		values["o"]=48;
		values["p"]=58;
		values["q"]=113;
		values["r"]=32;
		values["s"]=90;
		values["t"]=44;
		values["u"]=98;
		values["v"]=60;
		values["w"]=51;
		values["x"]=33;
		values["y"]=97;
		values["z"]=62;
		return values[ch];
	}
})();
