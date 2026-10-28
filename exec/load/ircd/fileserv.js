/* Per-BBS FileServ adapter. Ordinary replies are NOTICE; PRIVMSG is used
 * only for the client-required CTCP DCC transport negotiation.
 * GPL-2.0-or-later.
 */
"use strict";
load("ircd/fileserv-catalog.js");
function IRC_files_nick(qwk) { return "FileServ_" + String(qwk).replace(/[^a-z0-9_-]/gi, "_").substr(0,8); }
function IRC_files_quote(s) { return "'" + String(s).replace(/'/g,"'\"'\"'") + "'"; }
function IRC_files_dcc_address(address) {
	var ip=IRC_files_ip(address);if (!ip || ip=="0.0.0.0" || ip=="::") throw new Error("Configure [FileServ] Address to a reachable numeric IP address.");
	if (ip.indexOf(":")>=0) return ip;
	return ip.split(".").reduce(function(n,p) { return n*256+Number(p); },0).toString();
}
function IRC_files_endpoint_settings(config,peer) {
	peer=IRC_files_ip(peer);
	if (!peer) throw new Error("Your DCC peer address is unknown. Use ADDRESS <numeric-IP> privately.");
	var ipv6=peer.indexOf(":")>=0,family=ipv6 ? "6" : "4";
	var legacy_address=IRC_files_ip(config.address),legacy_bind=IRC_files_ip(config.bind);
	if (config.bind=="0.0.0.0" || config.bind=="::") legacy_bind=config.bind;
	var bind=config["bind"+family] || (legacy_bind && (legacy_bind.indexOf(":")>=0)!=ipv6 ? "" : config.bind) || (ipv6 ? "::" : "0.0.0.0");
	var address=config["address"+family] || (legacy_address && (legacy_address.indexOf(":")>=0)!=ipv6 ? "" : config.address) || bind;
	return {address:address,bind:bind,family:family,peer:peer};
}
function IRC_files_checked_endpoint(spec,address,bind) {
	var ipv6=spec.family=="6",label="IPv"+spec.family;
	bind=bind==(ipv6 ? "::" : "0.0.0.0") ? bind : IRC_files_ip(bind);
	if (!bind || (bind.indexOf(":")>=0)!=ipv6) throw new Error("No "+label+" FileServ bind address is available. Check [FileServ] Bind/Bind"+spec.family+".");
	address=IRC_files_ip(address);
	if (!address || (address.indexOf(":")>=0)!=ipv6) throw new Error("No "+label+" FileServ endpoint is available. Check [FileServ] Address/Address"+spec.family+".");
	return {address:address,bind:bind,peer:spec.peer};
}
/* DNS callbacks use explicit context, including dns.js's timeout context. */
function IRC_files_dns_reply(response) {
	var entry=this.owner ? this : this.thisObj;
	if (!entry || !entry.pending) return;
	entry.pending=false;
	var addresses=[],ttl=300;
	if (response && response.rcode===0) (response.answers || []).forEach(function(answer) {
		if (answer.class==1 && answer.type==5) ttl=Math.min(ttl,Math.max(0,Number(answer.ttl) || 0));
		var ip=IRC_files_ip(answer.rdata);
		if (answer.type!=entry.type || answer.class!=1 || !ip || (ip.indexOf(":")>=0)!=(entry.family=="6")) return;
		addresses.push(ip);ttl=Math.min(ttl,Math.max(0,Number(answer.ttl) || 0));
	});
	entry.address=addresses[0] || null;entry.until=system.timer+(entry.address ? ttl : 5);
	var waiters=entry.waiters;entry.waiters=[];
	if (!entry.owner.stopped) waiters.forEach(function(done) { done(entry.address); });
}
var IRC_files_help = {
	HELP:"HELP [command] — Describe the command, or list commands. FileServ replies with NOTICE. Use FileServ@<server> to select a BBS; authentication is specific to that BBS.",
	LIBS:"LIBS [page] — List exported libraries you can access, identified by their internal code prefixes.",
	DIRS:"DIRS [library-code-mask] [page] — List accessible exported directory codes. Use these codes in LIST, INFO and GET.",
	LIST:"LIST <directory-code> [filename-mask] [page] — List catalog files, descriptions and sizes. Only registered BBS files are available; filesystem paths are never accepted.",
	SEARCH:"SEARCH <text> [page] — Search accessible exported catalogs by filename or description. Use one search token; wildcards apply to LIST instead.",
	INFO:"INFO <directory-code> <filename> — Show size, description and the download credit cost for your BBS account.",
	GET:"GET <directory-code> <filename> [TLS|PLAIN] — Queue active DCC SEND (PLAIN) or SSEND (TLS). TLS is the default unless disabled by the sysop. Credits and native BBS download limits apply. No silent plaintext fallback. DCC RESUME is supported before connecting; only newly acknowledged bytes are accounted.",
	CHAT:"CHAT [TLS|PLAIN] — Open an active DCC CHAT/SCHAT virtual file browser. TLS is the default unless disabled. In chat: LIBS, DIRS, CD <directory-code|/>, PWD, DIR [mask] [page], SEARCH, INFO <file>, GET <file>, QUEUE, CANCEL [id|ALL], HELP and QUIT. No OS paths or uploads.",
	ADDRESS:"ADDRESS [numeric-IP|RESET] — Privately set the expected incoming DCC peer address for new GET/CHAT requests (useful behind a bouncer). Its address family selects the BBS's IPv4 or IPv6 endpoint. RESET restores your IRC connection address. Connections from other IPs are rejected. Queued and active requests keep their original addresses. Does not change your BBS record.",
	QUEUE:"QUEUE — List your pending downloads and active DCC sessions. FileServ limits transfers to one per BBS account, with bounded per-session and server queues.",
	CANCEL:"CANCEL [id|ALL] — Cancel your matching queued or active DCC work. With no argument cancels all your downloads. Acknowledged partial downloads receive native BBS accounting; cancellation does not undo credits already used."
};
function IRC_FileServ(manager) {
	if (/^win/i.test(system.platform)) throw new Error("FileServ workers currently require a POSIX host and realpath utility.");
	this.manager=manager;this.config=IRC_services_copy(manager.config.files || IRC_files_defaults());
	this.sessions={};this.workers={};this.reaping={};this.pending=[];this.connections=[];this.serial=0;this.stopped=false;this.dns={};
	this.listener=new Socket(SOCK_STREAM,"FileServ worker control");
	if (!this.listener.bind(0,"127.0.0.1") || !this.listener.listen(16)) { this.listener.close();throw new Error("FileServ control listener unavailable"); }
	this.listener.nonblocking=true;
	this.directory=backslash(system.data_dir)+"ircd-fileserv-"+IRC_services_random().substr(0,24)+"/";
	if (!mkdir(this.directory) || !file_chmod(this.directory,parseInt("700",8))) { this.listener.close();throw new Error("Cannot create private FileServ worker directory"); }
	var self=this,name=IRC_files_nick(manager.config.qwk);
	try {
		this.bot=manager.fake_user(name,ServerName,false,function(user,text) { self.command(user,text); },"bbs+ircd+files");
		/* Set the marker before the NICK is broadcast (fake_user supports it). */
		this.bot.services_fileserv=true;
	} catch(e) { this.listener.close();rmdir(this.directory);throw e; }
}
IRC_FileServ.prototype.valid = function(s) { return !this.stopped && Users[s.user.nick.toUpperCase()]===s.user && !s.user.server && !s.user.services_internal; };
IRC_FileServ.prototype.resolve_ip = function(host,family,done) {
	var ip=IRC_files_ip(host);
	if (ip || host=="::" || host=="0.0.0.0") { done(ip || host);return; }
	/* Only configured endpoint names reach DNS; client ADDRESS stays numeric. */
	if (/^[0-9.]+$/.test(host) || !/^(?=.{1,253}\.?$)[a-z0-9_-]+(?:\.[a-z0-9_-]+)*\.?$/i.test(host)) { done(null);return; }
	var key=family+":"+host.toLowerCase(),entry=this.dns[key];
	if (entry && entry.pending) { entry.waiters.push(done);return; }
	if (entry && system.timer<entry.until) { done(entry.address);return; }
	entry=this.dns[key]={owner:this,family:family,type:family=="6" ? 28 : 1,pending:true,waiters:[done]};
	try { DNS_Resolver.query([{query:host,type:entry.type,class:1}],IRC_files_dns_reply,entry,true,5000,2); }
	catch(e) { IRC_files_dns_reply.call(entry,null); }
};
IRC_FileServ.prototype.resolve_endpoint = function(s,done) {
	var self=this,spec;
	try { spec=IRC_files_endpoint_settings(this.config,s.address || s.user.ip); }
	catch(e) { done(null,e.message);return; }
	this.resolve_ip(spec.bind,spec.family,function(bind) {
		if (!self.valid(s)) return;
		function finish(address) {
			if (!self.valid(s)) return;
			var endpoint;
			try { endpoint=IRC_files_checked_endpoint(spec,address,bind); }
			catch(e) { done(null,e.message);return; }
			done(endpoint);
		}
		if (!bind || (bind.indexOf(":")>=0)!=(spec.family=="6")) finish(null);
		else if (spec.address==spec.bind) finish(bind);
		else self.resolve_ip(spec.address,spec.family,finish);
	});
};
IRC_FileServ.prototype.session = function(user) {
	/* The services RPC cookie can be rebound after a legacy burst. DCC
	   capabilities belong to this exact IRC object across that rebinding. */
	if (!user.services_files_ref) user.services_files_ref=IRC_services_random();
	var key=user.services_files_ref,s=this.sessions[key];
	if (!s || s.user!==user) s=this.sessions[key]={id:key,user:user,address:null,nextauth:0,authorized:0,identity:null,checking:false,last:0,catalog:0};
	return s;
};
IRC_FileServ.prototype.notice = function(s,text,chat) {
	if (!this.valid(s)) return;
	if (chat) { this.write(chat,{type:"text",text:String(text)});return; }
	var bot=this.bot;
	String(text).split(/\r?\n/).forEach(function(line) {
		line=IRC_files_clean(line);for (var i=0;i<Math.max(1,line.length);i+=280) s.user.originatorout("NOTICE "+s.user.nick+" :"+line.substr(i,280),bot);
	});
};
IRC_FileServ.prototype.authorize = function(s,done) {
	var self=this,m=this.manager;
	if (!this.valid(s)) { done(null,"Session ended.");return; }
	function finish(identity,actor) {
		if (!self.valid(s)) { done(null,"Session ended.");return; }
		if (identity && (identity.home!=m.config.server || identity.qwk!=m.config.qwk)) { done(null,"Invalid FileServ authentication.");return; }
		if (identity && !s.user.local) {
			var a=actor && m.attestations[actor.ref];
			if (!a || a.user!==s.user || a.identity.id!=identity.id || a.actor.server!=actor.server || a.actor.created!=actor.created) { done(null,"Identify to this BBS again before using protected files.");return; }
		}
		try { if (identity) { IRC_files_user(identity,self.config);var tls_error=m.tls_error(s.user,m.config.bbs_require_tls);if (tls_error) throw new Error(tls_error); } } catch(e) { done(null,e.message);return; }
		if ((s.identity && s.identity.id || null)!=(identity && identity.id || null)) self.cancel(s,"ALL",true);
		s.identity=identity || null;s.auth_revision=s.user.services_auth_revision || 0;s.authorized=system.timer+5;s.nextauth=system.timer+2;
		done(s.identity);
	}
	if (s.user.local) {
		finish(m.logins(s.user).filter(function(a) { return a.home==m.config.server; })[0]);return;
	}
	var has_attestation=Object.keys(m.attestations).some(function(ref) { return m.attestations[ref].user===s.user; });
	if (!has_attestation) { finish(null);return; }
	var cp=m.central_peer();
	if (cp && this.capable!==m.peer(cp)) { finish(null);return; }
	if (!cp) { finish(null);return; }
	m.request(cp,{type:"files-access",subject:{nick:s.user.nick,server:s.user.servername.toLowerCase()}},function(r) {
		if (r.error) { finish(null);return; }
		finish(r.identity,r.actor);
	});
};
IRC_FileServ.prototype.command = function(user,text,chat,from_channel) {
	if (!user || user.server || user.services_internal || user.uline || text.length>1024) return;
	var s=this.session(user),self=this;
	if (/^\x01/.test(text)) { this.resume(s,text);return; }
	if (!chat && s.last && system.timer-s.last<0.5) { this.notice(s,"Please wait before sending another FileServ command.");return; }
	if (!chat) s.last=system.timer;
	var args;try { args=IRC_files_args(text); } catch(e) { this.notice(s,e.message,chat);return; }
	var command=(args.shift() || "HELP").toUpperCase();
	if (chat) {
		if (command=="PWD") { this.notice(s,chat.cwd || "/",chat);return; }
		if (command=="QUIT" || command=="EXIT") { this.write(chat,{type:"exit"});return; }
		if (command=="DIR" || command=="LS") { command=chat.cwd ? "LIST" : "DIRS";if (chat.cwd) args.unshift(chat.cwd); }
		if ((command=="GET" || command=="INFO") && chat.cwd) args.unshift(chat.cwd);
	}
	if (command=="HELP") { this.notice(s,args.length ? (IRC_files_help[args[0].toUpperCase()] || "Unknown command. HELP lists commands.") : "Commands: "+Object.keys(IRC_files_help).join(", ")+". Use HELP <command> for syntax and details."+(chat ? " Chat also supports CD, PWD, DIR and QUIT." : ""),chat);return; }
	if (command=="ADDRESS") {
		if (chat || from_channel) { this.notice(s,"Use ADDRESS privately on IRC.",chat);return; }
		if (args.length>1 || (args.length && args[0].toUpperCase()!="RESET" && !IRC_files_ip(args[0]))) { this.notice(s,IRC_files_help.ADDRESS);return; }
		if (args.length) s.address=args[0].toUpperCase()=="RESET" ? null : IRC_files_ip(args[0]);
		this.notice(s,"Expected DCC peer address: "+(s.address || IRC_files_ip(user.ip) || "unknown; use ADDRESS <numeric-IP>"));return;
	}
	if (command=="QUEUE") {
		var rows=this.pending.filter(function(p) { return p.session===s; }).map(function(p) { return p.id+" queued "+p.file.dir+"/"+p.file.name; });
		Object.keys(this.workers).forEach(function(id) { var w=self.workers[id];if (w.session===s && w.kind!="catalog") rows.push(id+" "+w.kind+" "+(w.connected ? "connected" : "waiting")); });
		this.notice(s,rows.join("\n") || "Your queue is empty.",chat);return;
	}
	if (command=="CANCEL") { this.cancel(s,args[0] || "ALL",false);this.notice(s,"Matching FileServ work cancelled.",chat);return; }
	if (command=="CD" && chat && args[0]=="/") { chat.cwd="";this.notice(s,"/",chat);return; }
	if (["LIBS","DIRS","LIST","SEARCH","INFO","GET","CHAT","CD"].indexOf(command)<0) { this.notice(s,"Unknown command. Use HELP.",chat);return; }
	if (s.checking || s.catalog>=2) { this.notice(s,"A FileServ request is already pending.",chat);return; }
	var tls=this.config.tls!="off";
	if (command=="GET" || command=="CHAT") {
		var option=args.length && args[args.length-1].toUpperCase();
		if (option=="TLS" || option=="PLAIN") { args.pop();tls=option=="TLS"; }
		if ((this.config.tls=="required" && !tls) || (this.config.tls=="off" && tls)) { this.notice(s,"Requested DCC encryption is disabled by this BBS's TLS policy.",chat);return; }
		if (command=="CHAT" && args.length) { this.notice(s,IRC_files_help.CHAT,chat);return; }
	}
	s.checking=true;
	function execute(endpoint,endpoint_error) {
		if (!self.valid(s)) return;
		if (endpoint_error) { s.checking=false;self.notice(s,endpoint_error,chat);return; }
		self.authorize(s,function(identity,error) {
			s.checking=false;if (!self.valid(s)) return;
			if (error) { self.notice(s,error,chat);return; }
			try {
				if (command=="CHAT") { self.start_chat(s,tls,endpoint);return; }
				if (command=="CD") {
					if (args.length!=1) throw new Error("Syntax: CD <directory-code|/>");
					IRC_files_directory(args[0],IRC_files_user(identity,self.config),self.config,!identity,false);
					chat.cwd=args[0].toLowerCase();self.notice(s,chat.cwd,chat);return;
				}
				self.spawn(s,"catalog",{command:command,args:args},function(result) {
					if (result.error) self.notice(s,result.error,chat);
					else if (result.file) self.enqueue(s,result.file,tls,chat,endpoint);
					else self.notice(s,(result.lines || []).join("\n"),chat);
				});
			} catch(e) { self.notice(s,e.message || e,chat); }
		});
	}
	if (command=="GET" || command=="CHAT") {
		var resolution=s.resolving={};
		this.resolve_endpoint(s,function(endpoint,error) {
			if (s.resolving!==resolution) return;
			s.resolving=null;execute(endpoint,error);
		});
	}
	else execute();
};
IRC_FileServ.prototype.write = function(w,message) {
	if (w && w.connection) w.connection.output+=JSON.stringify(message)+"\n";
};
IRC_FileServ.prototype.spawn = function(s,kind,extra,callback,existing_id) {
	if (Object.keys(this.workers).length+Object.keys(this.reaping).length>=this.config.max_downloads+this.config.max_chats+4) throw new Error("FileServ worker limit reached; try again later.");
	var id=existing_id || String(++this.serial),token=IRC_services_random(),path=this.directory+id+".json",exe=this.config.jsexec || system.exec_dir+"jsexec";
	if (!file_exists(exe)) throw new Error("Configure [FileServ] JSexec to the Synchronet jsexec executable.");
	var job={id:id,token:token,kind:kind,config:IRC_services_copy(this.config),identity:s.identity,control:this.listener.local_port,address:s.address || IRC_files_ip(s.user.ip)};
	Object.keys(extra || {}).forEach(function(k) { job[k]=extra[k]; });
	if (kind!="catalog") job.address=job.endpoint.peer;
	if (extra.file && !s.identity) job.config.guest=extra.file.account.number;
	var f=new File(path);if (!f.open("w")) throw new Error("Cannot create FileServ worker job.");
	try { f.write(JSON.stringify(job)); } finally { f.close(); }
	var status=this.directory+id+".status",pidpath=this.directory+id+".pid",logpath=this.directory+id+".log";
	var launch=IRC_files_quote(exe)+" -n -L0 -E0 -m 67108864 -c "+IRC_files_quote(system.ctrl_dir)+" -i "+IRC_files_quote(system.exec_dir+"load")+" "+IRC_files_quote(system.exec_dir+"load/ircd/fileserv-worker.js")+" "+IRC_files_quote(path);
	var command="( "+launch+" </dev/null >/dev/null 2>"+IRC_files_quote(logpath)+" & child=$!; printf '%s' \"$child\" >"+IRC_files_quote(pidpath)+"; wait \"$child\"; result=$?; printf '%s' \"$result\" >"+IRC_files_quote(status)+" ) </dev/null >/dev/null 2>&1 & echo $!";
	var output=system.popen(command),pid=output && Number(output[0]);
	if (!pid || !isFinite(pid) || pid%1) { file_remove(path);throw new Error("Cannot start FileServ worker."); }
	var w=this.workers[id]={id:id,pid:pid,token:token,kind:kind,session:s,identity:s.identity,callback:callback,until:system.timer+this.config.timeout+15,path:path,bytes:0,file:extra.file,tls:extra.tls,endpoint:job.endpoint,lastlease:0,accounted:false,created:system.timer,status:status,pidpath:pidpath,logpath:logpath};
	if (kind=="catalog") s.catalog++;
	return w;
};
IRC_FileServ.prototype.enqueue = function(s,file,tls,chat,endpoint) {
	if (this.pending.length>=this.config.max_queue || this.pending.filter(function(p) { return p.identity ? s.identity && p.identity.id==s.identity.id : !s.identity; }).length>=this.config.user_queue) { this.notice(s,"Download queue is full.",chat);return; }
	var p={id:"q"+(++this.serial),session:s,identity:s.identity,file:file,tls:tls,endpoint:endpoint};this.pending.push(p);
	this.notice(s,"Queued "+p.id+": "+file.dir+"/"+file.name+" ("+file.size+" bytes, "+file.cost+" credits).",chat);
};
IRC_FileServ.prototype.start_chat = function(s,tls,endpoint) {
	var self=this,chats=Object.keys(this.workers).filter(function(id) { return self.workers[id].kind=="chat"; });
	if (chats.length>=this.config.max_chats || chats.some(function(id) { return self.workers[id].session===s; })) throw new Error("DCC chat limit reached.");
	this.spawn(s,"chat",{tls:tls,endpoint:endpoint});
};
IRC_FileServ.prototype.cancel = function(s,id,quiet) {
	if (id=="ALL" && s.resolving) { s.resolving=null;s.checking=false; }
	this.pending=this.pending.filter(function(p) { return p.session!==s || (id!="ALL" && id!=p.id); });
	for (var key in this.workers) {
		var w=this.workers[key];if (w.session===s && (id=="ALL" || id==w.id)) { this.write(w,{type:"cancel"});w.cancelled=true;w.until=Math.min(w.until,system.timer+3); }
	}
};
IRC_FileServ.prototype.resume = function(s,text) {
	var m=/^\x01DCC RESUME ("[^"\r\n]+"|[^\s]+) (\d+) (\d+)\x01$/.exec(text);
	if (!m) return;
	var self=this;
	Object.keys(this.workers).some(function(id) {
		var w=self.workers[id];if (w.session!==s || w.kind!="send" || w.connected || Number(m[2])!=w.port || m[1].replace(/^"|"$/g,"")!=w.file.name || Number(m[3])>=w.file.size) return false;
		self.write(w,{type:"resume",offset:Number(m[3])});return true;
	});
};
IRC_FileServ.prototype.account = function(w) {
	if (w.accounted || w.kind!="send") return;w.accounted=true;
	if (!w.bytes && w.file.size) return;
	try {
		if (!w.account_user) return;
		var u=new User(w.account_user.number);
		/* Guest transfers are bound to the initially chosen guest, too. */
		if (w.account_user && (u.number!=w.account_user.number || u.stats.firston_date!=w.account_user.created)) throw new Error("Download account changed.");
		if (!u.downloaded_file(w.file.dir,w.file.name,w.bytes)) throw new Error("Native download accounting failed.");
	} catch(e) { log(LOG_ERR,"FileServ download accounting failed id="+w.id+": "+e);this.notice(w.session,"Download accounting failed; please contact this BBS's sysop."); }
};
IRC_FileServ.prototype.finish = function(w,error,result) {
	if (!this.workers[w.id]) return;
	delete this.workers[w.id];
	w.reap_at=system.timer+3;w.reap_attempts=0;this.reaping[w.id]=w;
	if (w.kind=="catalog") w.session.catalog--;
	else this.account(w);
	if (w.connection) { w.connection.socket.close();w.connection.closed=true; }
	if (w.callback && !w.cancelled) {
		var self=this;
		this.authorize(w.session,function(identity,denied) {
			if (denied || (w.identity ? !identity || w.identity.id!=identity.id : identity)) w.callback({error:denied || "Authentication changed; retry your request."});
			else w.callback(error ? {error:error} : result || {});
		});
	}
	else if (!w.cancelled) this.notice(w.session,error || (w.kind=="send" ? "Download finished: "+w.file.name+" ("+w.bytes+" new bytes acknowledged)." : "DCC chat closed."));
};
IRC_FileServ.prototype.frame = function(c,m) {
	if (!c.worker) {
		var w=this.workers[String(m.id)];
		if (m.type!="hello" || !w || w.connection || typeof m.proof!="string" || !IRC_services_equal(m.proof,IRC_files_mac(w.token,"worker:"+w.id))) throw new Error("Unrecognized FileServ worker");
		c.worker=w;w.connection=c;this.write(w,{type:"ready",proof:IRC_files_mac(w.token,"parent:"+w.id)});if (w.cancelled) this.write(w,{type:"cancel"});return;
	}
	var w=c.worker,s=w.session;
	if (m.type=="result") { this.finish(w,null,m.result);return; }
	if (w.cancelled && ["progress","error","complete"].indexOf(m.type)<0) { this.write(w,{type:"cancel"});return; }
	if (m.type=="error" || m.type=="complete" || m.type=="progress") {
		if (w.kind=="send" && isFinite(m.bytes) && m.bytes>=w.bytes && m.bytes<=w.file.size) { w.bytes=m.bytes;if (!w.cancelled) w.until=system.timer+this.config.timeout; }
		if (m.type!="progress") this.finish(w,m.type=="error" ? m.error : null);return;
	}
	if (m.type=="offer") {
		w.until=system.timer+this.config.timeout;
		w.port=Number(m.port);if (!w.port || w.port>65535 || w.port%1) throw new Error("Invalid DCC offer port");
		var address=IRC_files_dcc_address(w.endpoint.address),payload;
		var filename=w.file && (w.file.name.indexOf(" ")>=0 ? '"'+w.file.name+'"' : w.file.name);
		if (w.kind=="send") payload="DCC "+(w.tls ? "SSEND" : "SEND")+" "+filename+" "+address+" "+w.port+" "+w.file.size;
		else payload="DCC "+(w.tls ? "SCHAT" : "CHAT")+" chat "+address+" "+w.port;
		s.user.originatorout("PRIVMSG "+s.user.nick+" :\x01"+payload+"\x01",this.bot);
	} else if (m.type=="resumed") s.user.originatorout("PRIVMSG "+s.user.nick+" :\x01DCC ACCEPT "+(w.file.name.indexOf(" ")>=0 ? '"'+w.file.name+'"' : w.file.name)+" "+w.port+" "+m.offset+"\x01",this.bot);
	else if (m.type=="connected") { w.connected=true;w.until=system.timer+this.config.timeout*(w.kind=="chat" ? 5 : 1); }
	else if (m.type=="command" && w.kind=="chat" && typeof m.text=="string") { w.until=system.timer+this.config.timeout*5;this.command(s.user,m.text,w); }
};
IRC_FileServ.prototype.tick = function() {
	if (this.stopped) return;
	if (!this.bot || Users[this.bot.nick.toUpperCase()]!==this.bot) { this.stop();this.manager.fileserv=null;log(LOG_WARNING,"FileServ stopped after its service nickname disappeared.");return; }
	var self=this,now=system.timer,cp=this.manager.central_peer();
	if (!cp) { this.cap_peer=this.cap_service=this.capable=null; }
	if (cp && (this.cap_peer!==this.manager.peer(cp) || this.cap_service!==Servers[this.manager.config.host])) {
		this.cap_peer=this.manager.peer(cp);this.cap_service=Servers[this.manager.config.host];this.capable=null;
		var expected=this.cap_peer,expected_service=this.cap_service;
		this.manager.request(cp,{type:"files-capabilities"},function(r) { if (self.cap_peer===expected && self.cap_service===expected_service && r.files_access==1) self.capable=expected; });
	}
	for (var accepted=0;accepted<8 && this.listener.poll(0);accepted++) {
		var sock=this.listener.accept();if (!sock) break;
		sock.nonblocking=true;
		if (this.connections.length>=32) sock.close();else this.connections.push({socket:sock,input:"",output:"",until:now+5});
	}
	this.connections.forEach(function(c) {
		if (c.closed) return;
		try {
			if (c.output && c.socket.poll(0,true)) { var n=c.socket.send(c.output);if (n>0) c.output=c.output.substr(n); }
			if (c.output.length>65536) throw new Error("Worker output overflow");
			/* data_waiting/is_connected can report HUP before queued bytes have
						been consumed. A raw readiness poll followed by recv drains them. */
			var readable=c.socket.poll(0);
			if (readable<0) throw new Error("Worker socket failed");
			if (readable>0) {
				var data=c.socket.recv(32768,0);
				if (data) c.input+=data;
				else if (!c.socket.is_connected) c.eof=true;
			}
			if (c.input.length>131072) throw new Error("Worker input overflow");
			var nl,count=0;
			while (!c.closed && (nl=c.input.indexOf("\n"))>=0 && count++<64) { var frame=JSON.parse(c.input.substr(0,nl));c.input=c.input.substr(nl+1);self.frame(c,frame); }
			if (!c.closed && ((c.eof && c.input.indexOf("\n")<0) || (!c.worker && now>c.until))) throw new Error("Worker disconnected");
		} catch(e) { if (c.worker) self.finish(c.worker,"FileServ worker ended: "+e.message);c.socket.close();c.closed=true; }
	});
	this.connections=this.connections.filter(function(c) { return !c.closed; });
	Object.keys(this.sessions).forEach(function(id) {
		var s=self.sessions[id];
		if (!self.valid(s)) { self.cancel(s,"ALL",true);delete self.sessions[id];return; }
		var active=self.pending.some(function(p) { return p.session===s; }) || Object.keys(self.workers).some(function(k) { return self.workers[k].session===s; });
		if (active && (now>=s.nextauth || s.auth_revision!=(s.user.services_auth_revision || 0)) && !s.checking) {
			s.checking=true;s.nextauth=now+2;
			self.authorize(s,function(identity,error) { s.checking=false;if (error) { s.authorized=0;self.cancel(s,"ALL",true); } });
		}
	});
	Object.keys(this.workers).forEach(function(id) {
		var w=self.workers[id],s=w.session;
		if (!w.connection && now-w.created>2 && file_exists(w.status)) { self.finish(w,"FileServ worker exited before connecting.");return; }
		if (!self.valid(s) || now>w.until) { self.write(w,{type:"cancel"});self.kill(w,"TERM");self.finish(w,"FileServ worker timed out.");return; }
		if (!w.cancelled && s.authorized>now && now-w.lastlease>=1) { self.write(w,{type:"lease"});w.lastlease=now; }
	});
	this.reap();
	var active=[],accounts={};
	Object.keys(this.workers).forEach(function(id) { var w=self.workers[id];if (w.kind=="send") { active.push(w);accounts[w.identity ? w.identity.id : "guest"]=true; } });
	Object.keys(this.reaping).forEach(function(id) { var w=self.reaping[id];if (w.kind=="send") { active.push(w);accounts[w.identity ? w.identity.id : "guest"]=true; } });
	for (var i=0;i<this.pending.length && active.length<this.config.max_downloads;i++) {
		var p=this.pending[i],s=p.session,key=p.identity ? p.identity.id : "guest";
		if (accounts[key] || s.checking || s.authorized<=now) continue;
		this.pending.splice(i--,1);
		if ((p.identity && (!s.identity || p.identity.id!=s.identity.id)) || (!p.identity && s.identity)) { this.notice(s,"Authentication changed; request the file again.");continue; }
		try {
			var user=s.identity ? IRC_files_user(s.identity,this.config) : new User(p.file.account.number);
			if (!IRC_files_active(user) || (!s.identity && !(user.security.restrictions & UFLAG_G))) throw new Error("Guest access revoked.");
			if (p.file.account && (user.number!=p.file.account.number || user.stats.firston_date!=p.file.account.created)) throw new Error("Download account changed; request the file again.");
			if (!IRC_files_allowed(file_area.dir[p.file.dir],user,this.config,!s.identity,true)) throw new Error("Download access revoked.");
			var w=this.spawn(s,"send",{file:p.file,tls:p.tls,endpoint:p.endpoint},undefined,p.id);w.account_user={number:user.number,created:user.stats.firston_date};accounts[key]=true;active.push(w);
		} catch(e) { this.notice(s,e.message || e); }
	}
};
IRC_FileServ.prototype.channel = function(user,chan,text) {
	if (!this.bot || !this.bot.channels[chan.nam.toUpperCase()] || !chan.users[this.bot.id]) return;
	var a=text.trim().split(/\s+/),verb=a.shift().toLowerCase();
	var browser=verb=="!fserve";
	if (verb=="!list") {
		var s=this.session(user);if (s.last && system.timer-s.last<0.5) return;s.last=system.timer;
		this.notice(s,"Files from "+this.manager.config.qwk+": /msg FileServ@"+this.manager.config.server+" HELP; !fserve "+this.manager.config.qwk+" [TLS|PLAIN] opens the DCC browser.");
	}
	else if ((verb=="!files" || browser) && (a.shift() || "").toUpperCase()==this.manager.config.qwk)
		this.command(user,(browser ? "CHAT " : "")+a.join(" "),null,true);
};
IRC_FileServ.prototype.join_channels = function() {
	var self=this;
	String(this.config.channels).split(/[ ,]+/).forEach(function(name) {
		if (/^#[^\x00-\x20,:\x7f]{1,99}$/.test(name) && !self.bot.channels[name.toUpperCase()]) self.bot.do_join(name);
	});
};
IRC_FileServ.prototype.kill = function(w,signal) {
	if (file_exists(w.status)) return;
	var f=new File(w.pidpath),pid=0;
	if (f.open("r",true)) { try { pid=Number(f.read()); } finally { f.close(); } }
	if (pid>0 && isFinite(pid) && pid%1==0) system.popen("kill -"+signal+" "+pid+" 2>/dev/null");
};
IRC_FileServ.prototype.reap = function() {
	var self=this,now=system.timer;
	Object.keys(this.reaping).forEach(function(id) {
		var w=self.reaping[id],f=new File(w.status),ended=false;
		if (f.open("r",true)) {
			var status;try { status=f.read(); } finally { f.close(); }
			if (/^\d+$/.test(status)) {
				ended=true;
				if (Number(status) && !w.cancelled) {
					var native_log=new File(w.logpath),detail="";
					if (native_log.open("r",true)) { try { detail=IRC_files_clean(native_log.read(1024)); } finally { native_log.close(); } }
					log(LOG_ERR,"FileServ worker exit="+status+" id="+w.id+" kind="+w.kind+(detail ? " detail="+detail : ""));
				}
			}
		}
		if (!ended && now>w.reap_at) {
			self.kill(w,w.term_sent ? "KILL" : "TERM");w.term_sent=true;w.reap_at=now+3;
			if (++w.reap_attempts>3) { ended=true;log(LOG_ERR,"FileServ worker exit status unavailable id="+w.id); }
		}
		if (ended) { [w.path,w.pidpath,w.status,w.logpath].forEach(function(p) { file_remove(p); });delete self.reaping[id]; }
	});
};
IRC_FileServ.prototype.stop = function(except) {
	this.stopped=true;this.pending=[];
	var self=this;
	Object.keys(this.workers).forEach(function(id) {
		var w=self.workers[id];w.cancelled=true;
		self.finish(w,"FileServ stopped.");
	});
	this.connections.forEach(function(c) { c.socket.close(); });this.listener.close();
	var until=system.timer+1;
	while (Object.keys(this.reaping).length && system.timer<until) { this.reap();mswait(20); }
	Object.keys(this.reaping).forEach(function(id) { self.kill(self.reaping[id],"TERM");self.reaping[id].reap_at=system.timer+0.2; });
	until=system.timer+1;
	while (Object.keys(this.reaping).length && system.timer<until) { this.reap();mswait(20); }
	Object.keys(this.reaping).forEach(function(id) { var w=self.reaping[id];self.kill(w,"KILL");[w.path,w.pidpath,w.status,w.logpath].forEach(function(p) { file_remove(p); }); });
	if (this.bot && Users[this.bot.nick.toUpperCase()]===this.bot) {
		if (except) {
			for (var key in Local_Servers) if (Local_Servers[key]!==except && Local_Servers[key].admitted)
				Local_Servers[key].rawout(":"+this.bot.nick+" QUIT :FileServ nickname unavailable");
			this.bot.quit("FileServ nickname unavailable",true);
		} else this.bot.quit("FileServ stopped");
	}
	rmdir(this.directory);
};
