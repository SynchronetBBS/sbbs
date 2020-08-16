/* 	
	Javascript CVS client for Synchronet (2011)
	-	code by mcmlxxix
	-	awesome by Deuce
*/

CVS = new (function () {
	this.VERSION = "$Revision: 1.18 $".split(' ')[1];
	this.socket = undefined;
	
////////////////////////////////// SETTINGS

	/* connection defaults */
	this.CVSSERV = "cvs.synchro.net";
	this.CVSROOT = "/cvsroot/sbbs";
	this.CVSUSER = "anonymous";
	this.CVSPASS = "";
	this.CVSAUTH = "pserver";

	/* Files which have been recieved via commands */
	this.files={};

	/* default accepted responses  */
	this.validResponses = {
		'Checked-in':			true,	// Handled
		'Checksum':				false,
		'Clear-rename':			false,
		'Clear-static-directory':false,
		'Clear-sticky':			false,
		'Copy-file':			false,
		'Created':				false,
		'E':					true,	// Handled
		'EntriesExtra':			false,
		'F':					true,	// Handled
		'M':					true,	// Handled
		'MT':					true,	// Handled
		'Mbinary':				false,
		'Merged':				true,	// Handled
		'Mod-time':				false,
		'Mode':					false,
		'Module-expansion':		false,
		'New-entry':			false,
		'Notified':				false,
		'Patched':				false,
		'Rcs-diff':				false,
		'Remove-entry':			false,
		'Removed':				true,	// Handled
		'Renamed':				false,
		'Set-checkin-prog':		false,
		'Set-static-directory':	false,
		'Set-sticky':			false,
		'Set-update-prog':		false,
		'Template':				false,
		'Update-existing':		false,
		'Updated':				true,	// Handled
		'Valid-requests':		true,	// Handled
		'Wrapper-rcsOption':	false,
		'error':				true,	// Handled
		'ok':					true,	// Handled
	};
	
	/* populated by response to valid-requests  */
	this.validRequests = {
		'valid-requests':		true,	// Better be.
		'Valid-responses':		true,	// Better be.
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
		if (this.socket.connect(this.CVSSERV,2401))
			this.authenticate();
		else
			this.socket.close();
		this.init();
	}
	
	/* initialize protocol */
	this.init = function() {
		this.protocol["Valid-responses"]();
		this.protocol["valid-requests"]();
		this.protocol.UseUnchanged();
		this.protocol.Root(this.CVSROOT);
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
		default:
			throw("Unhandled AUTH scheme.");
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

	this.check_update = function(file, dir, curr_ver, srv, root, user, pw) {
		// Boilerplate...
		if(srv)
			this.CVSSERV = srv;
		if(root)
			this.CVSROOT = root;
		if(user)
			this.CVSUSER = user;
		if(pw)
			this.CVSPASS = pw;

		this.verifyConnection();
		while(dir.charAt(0)=='/')
			dir=dir.substr(1);
		while(dir.charAt(dir.length-1)=='/')
			dir=dir.substr(0,dir.length-1);
		this.protocol.Directory('.', this.CVSROOT+(dir.length?'/'+dir:''));
		this.protocol.Entry('/'+file+'/'+curr_ver+'///');
		this.protocol.Unchanged(file);
		this.protocol.Argument(file);
		this.protocol.update();
		this.disconnect();
		if(this.files[(dir.length?dir+'/':'')+file]) {
			return this.files[(dir.length?dir+'/':'')+file].data;
		}
		return null;
	}

	this.log = function(dir, files, srv, root, user, pw) {
		// Boilerplate...
		if(srv)
			this.CVSSERV = srv;
		if(root)
			this.CVSROOT = root;
		if(user)
			this.CVSUSER = user;
		if(pw)
			this.CVSPASS = pw;

		this.verifyConnection();
		while(dir.charAt(0)=='/')
			dir=dir.substr(1);
		while(dir.charAt(dir.length-1)=='/')
			dir=dir.substr(0,dir.length-1);
		this.protocol.Directory('.', this.CVSROOT+(dir.length?'/'+dir:''));
		this.protocol.Entry('/'+file+'/'+curr_ver+'///');
		this.protocol.Unchanged(file);
		this.protocol.Argument(file);
		this.protocol.update();
		this.disconnect();
		if(this.files[(dir.length?dir+'/':'')+file]) {
			return this.files[(dir.length?dir+'/':'')+file].data;
		}
		return null;
	}

	/* verify that socket is connected to the server */
	this.verifyConnection = function() {
		if(!this.socket || !this.socket.is_connected) {
			this.connect();
			if(!this.socket.is_connected) {
				throw("error connecting to server");
			}
		}
	}

////////////////////////////////// Stuff that should be overwritten

	this.write = function(str) {
		if(js.global.write)
			write(str);
		else if(js.global.print)
			print(str);
		else
			this.writeln(str);
	}

	this.writeln = function(str) {
		if(js.global.writeln)
			writeln(str);
		if(js.global.print)
			print(str+"\n");
		else
			log(LOG_INFO, str);
	}

	this.errstr = function(str) {
		log(LOG_INFO, str);
		if(js.global.alert)
			alert(str);
		else if(js.global.writeln)
			writeln(str);
		else if(js.global.print)
			print(str+"\n");
	}

	this.flush = function() {
	}

////////////////////////////////// MAGIC PROPERTIES

	/*
	 * check for a response from the cvs server 
	 * This will handle whatever it can, and return
	 * only unhandled responses.
	 * 
	 * While it "handles" error, it still returns it.
	 */
	
	this.__defineGetter__("response", function() {
		function split_cmd(str, count) {
			if(count==undefined)
				count=2;

			var ret=[];

			while(--count) {
				var space=str.indexOf(' ');
				if(space==-1)
					break;
				ret.push(str.substr(0, space));
				str=str.substr(space+1);
			}
			ret.push(str);
			return ret;
		}
		
		function recv_file(socket) {
			var length=parseInt(socket.recvline());
			var ret='';

			while(length) {
				var str=socket.recv(length);
				if(str) {
					ret += str;
					length -= str.length;
				}
				else
					throw("Error on recv()");
			}
			return ret;
		}

		for(;;) {
			this.verifyConnection();
			if(!this.socket.poll(1))
				return undefined;
			var response=this.socket.recvline(65535,10);
			var cmd=split_cmd(response,2);
			if(cmd.length < 2) {
				if(cmd[0] != 'ok')
					log(LOG_ERR, "Response with no arg: "+response);
				return response;
			}
			switch(cmd[0]) {
				case 'Checked-in':
					var directory=cmd[1];
					var repofile=this.socket.recvline(65535,10);
					var entries=this.socket.recvline(65535,10);
					break;
				case 'Merged':
				case 'Updated':
					var directory=cmd[1];
					var repofile=this.socket.recvline(65535,10);
					var entries=this.socket.recvline(65535,10);
					var mode=this.socket.recvline(65535,10);
					var filedata=recv_file(this.socket);

					this.files[repofile]={};
					this.files[repofile].entries=entries;
					this.files[repofile].mode=mode;
					this.files[repofile].data=filedata;
					this.files[repofile].status=cmd[0];
					break;
				case 'Removed':
					var directory=cmd[1];
					var repofile=this.socket.recvline(65535,10);

					this.files[repofile]={};
					this.files[repofile].status=cmd[0];
					break;
				case 'M':
					this.writeln(cmd[1]);
					break;
				case 'MT':
					var m=split_cmd(cmd[1],2);
					switch(m[0]) {
						case 'newline':
							this.writeln('');
							break;
						case 'text':
							this.write(m[1]);
							break;
						default:
							if(m.length > 1)
								this.write(m[1]);
							break;
					}
					break;
				case 'E':
					this.errstr(cmd[1]);
					break;
				case 'F':
					this.flush();
					break;
				case 'Valid-requests':
					var m=cmd[1].split(' ');
					for each(var r in m) {
						this.validRequests[r] = true;
					}
					break;
				case 'error':
					var m=cmd[1].split(' ',2);
					if(m[0].length > 0) {
						this.errstr("ERROR "+m[0]+" - "+m[1]);
					}
					else {
						if(m[1].length > 0)
							this.errstr("ERROR - "+m[1]);
					}
					return response;
				default:
					return response;
			}
		}
	});

////////////////////////////////// PROTOCOL API

	/* cvs protocol commands */
	this.protocol = {

		//TODO ---> http://www.cvsnt.org/cvsclient/Requests.html
	
		rcmd:function(cmd, arg) {
			if(!this.parent.validRequests[cmd])
				throw(cmd+' command not supported by remote!');
			this.parent.request(cmd+(arg?" "+arg:''));
			var response=this.parent.response;
			if(response != 'ok') {
				log(LOG_ERR, "Unhandled response: '"+response+"'");
			}
		},

		init:function(root) { this.rcmd('init', root); },
		'valid-requests':function()	{ this.rcmd('valid-requests'); },
		'expand-modules':function()	{ this.rcmd('expand-modules'); },
		ci:function() { this.rcmd('ci'); },
		diff:function() { this.rcmd('diff'); },
		tag:function() { this.rcmd('tag'); },
		status:function() { this.rcmd('status'); },
		admin:function() { this.rcmd('admin'); },
		history:function() { this.rcmd('history'); },
		watchers:function() { this.rcmd('watchers'); },
		editors:function() { this.rcmd('editors'); },
		annotate:function() { this.rcmd('annotate'); },
		log:function() { this.rcmd('log'); },
		co:function() { this.rcmd('co'); },
		export:function() { this.rcmd('export'); },
		rannotate:function() { this.rcmd('rannotate'); },
		rdiff:function() { this.rcmd('rdiff'); },
		rlog:function() { this.rcmd('rlog'); },
		rtag:function() { this.rcmd('rtag'); },
		update:function() { this.rcmd('update'); },
		import:function() { this.rcmd('import'); },
		add:function() { this.rcmd('add'); },
		remove:function() { this.rcmd('remove'); },
		'watch-on':function() { this.rcmd('watch-on'); },
		'watch-off':function() { this.rcmd('watch-off'); },
		'watch-add':function() { this.rcmd('watch-add'); },
		'watch-remove':function() { this.rcmd('watch-remove'); },
		release:function() { this.rcmd('release'); },
		noop:function() { this.rcmd('noop'); },
		'update-patches':function() { this.rcmd('update-patches'); },
		'gzip-file-contents':function(level) { this.rcmd('gzip-file-contents', level); },
		'wrapper-sendme-rcsOptions':function() { this.rcmd('wrapper-sendme-rcsOptions'); },
		version:function() { this.rcmd('version'); },

		cmd:function(cmd, arg1, arg2) {
			if(!this.parent.validRequests[cmd])
				throw(cmd+' command not supported by remote!');
			this.parent.request(cmd+(arg1?" "+arg1:'')+(arg2?"\n"+arg2:''));
		},

		/* tell server what responses we can handle */
		'Valid-responses':function() {
			var str="";
			for(var r in this.parent.validResponses) {
				if(this.parent.validResponses[r] == true)
					str+=" "+r;
			}
			this.cmd('Valid-responses', str);
		},
		Root:function(pathname) { this.cmd('Root', pathname); },
		Directory:function(dir,repository) { this.cmd('Directory',dir,repository); },
		Entry:function(str) { this.cmd("Entry", str); },
		Unchanged:function(filename) { this.cmd("Unchanged", filename); },
		Argument:function(str) { this.cmd("Argument", str); },
		Argumentx:function(str) { this.cmd("Argumentx", str); },
		Notify:function(filename) { throw("Notify is not implemented!"); },
		Questionable:function(filename) { this.cmd("Questionable", filename); },
		Case:function() { this.cmd("Case"); },
		Utf8:function() { this.cmd("Utf8"); },
		Global_option:function(option) { this.cmd("Global_option ", option); },
		'Max-dotdot':function(level) { this.cmd('Max-dotdot', level); },
		'Static-directory':function() { this.cmd('Static-directory'); },
		Sticky:function(tagspec) { this.cmd('Sticky', tagspec); },
		Kopt:function(option) { this.cmd('Kopt', option); },
		'Checkin-time':function(time) { this.cmd('Checkin-time', time); },
		Modified:function(filename, mode, filedata) { throw("Modified is not implemented!"); },
		'Is-modified':function(filename) { this.cmd('Is-modified', filename); },
		UseUnchanged:function() { this.cmd('UseUnchanged'); },
		'Empty-conflicts':function() { this.cmd('Empty-conflicts'); },
		'Gzip-stream':function(level) { this.cmd('Gzip-stream', level); },
		'Kerberos-encrypt':function() { this.cmd('Kerberos-encrypt'); },
		'Gssapi-encrypt':function() { this.cmd('Gssapi-encrypt'); },
		'Gssapi-authenticate':function() { this.cmd('Gssapi-authenticate'); },
		Set:function(varValPair) { this.cmd('Set', varValPair); },
	};
	this.protocol.parent=this;
	
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
