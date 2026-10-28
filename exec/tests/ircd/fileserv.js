/* FileServ authorization/protocol boundaries. Native cases require only the
 * private fixtures created by fileserv-fixture.py. No live accounts are used.
 */
"use strict";
load(js.exec_dir+"lib.sjs");load(js.exec_dir+"state-fixture.sjs"); load("sbbsdefs.js"); load("ircd/services-common.js");
load("ircd/config.js");load("ircd/services.js");load("ircd/fileserv.js");
function check(ok,msg) { if (!ok) throw new Error(msg || "Assertion failed"); }
function rejects(fn,text) { var caught;try { fn(); } catch(e) { caught=e; }check(caught && (!text || String(caught).indexOf(text)>=0),"Expected refusal: "+text); }
function policy() {
	var scope={file_area:{lib:{Files:{name:"Files",code_prefix:"id_",description:"Pretty Library"}},dir:{},user_dir:{code:"user"}}};
	load(scope,"ircd/fileserv-catalog.js");
	var c=IRC_files_defaults(),d={number:0,code:"id_public",name:"Public Area",lib_name:"Files",description:"Pretty Area"},calls=[];
	var u={can_access_dir:function(number,type) { check(typeof number=="number");calls.push(type || "access");return true; }};
	scope.file_area.dir.id_public=d;
	return {scope:scope,c:c,d:d,u:u,calls:calls};
}
function adapter() {
	var scope={Users:{},system:{timer:100},ServerName:"here.test"};load(scope,"ircd/services-common.js");load(scope,"ircd/fileserv.js");
	var a=Object.create(scope.IRC_FileServ.prototype),u={nick:"Reader",local:true,servername:"here.test",ip:"127.0.0.1"};scope.Users.READER=u;
	a.manager={ref:function() { return "session"; },config:{qwk:"HERE"}};a.config=IRC_files_defaults();a.config.address="127.0.0.1";a.sessions={};a.workers={};a.reaping={};a.pending=[];a.serial=0;a.output=[];a.dns={};
	a.notice=function(s,text) { a.output.push(text); };a.authorize=function(s,callback) { s.authorized=200;callback(null); };
	return {scope:scope,a:a,u:u};
}
run_tests({
	"FileServ and public exports default off, while enabled DCC defaults encrypted":function() {
		check(!EmbeddedServices_Config.fileserv);var c=IRC_files_defaults();check(!c.public_libs && !c.public_dirs && c.tls=="optional");
		var t=adapter(),secure;t.a.start_chat=function(s,tls) { secure=tls; };t.a.command(t.u,"CHAT");check(secure===true);
	},
	"export masks match library names and internal directory codes with literal regex characters":function() {
		check(IRC_files_masks("id_*, other?","ID_public"));check(!IRC_files_masks("id.+","idZZ"));check(IRC_files_masks("id.+","ID.+"));
		var t=policy();t.c.include_libs="Pretty*";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,false));
		t.c.include_libs="Files";t.c.include_dirs="Pretty*";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,false));
		t.c.include_dirs="ID_*";check(t.scope.IRC_files_allowed(t.d,t.u,t.c,false,false));
		t.c.include_libs="id_*";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,false));
	},
	"Main library with an empty prefix uses its ordinary name for include, public and exclude masks":function() {
		var t=policy();delete t.scope.file_area.lib.Files;t.d.lib_name="Main";
		t.scope.file_area.lib.Main={name:"Main",code_prefix:"",description:"Main File Library"};
		t.c.include_libs="Main";t.c.public_libs="Main";t.c.public_dirs="";
		check(t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
		t.c.public_libs="Other";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
		t.c.public_libs="mAiN";t.c.exclude_libs="MAIN";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
		t.c.exclude_libs="";t.c.include_libs="Other";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
	},
	"comma-separated export masks preserve spaces in names and ignore surrounding whitespace":function() {
		check(IRC_files_masks("Main, BBS Software , Games*","bbs software"));
		check(!IRC_files_masks("BBS Software","Software") && !IRC_files_masks("BBS Software","BBS"));
		check(!IRC_files_masks(", ,","") && IRC_files_masks("Games?, Main","games1"));
		var t=policy();delete t.scope.file_area.lib.Files;t.d.lib_name="BBS Software";
		t.scope.file_area.lib[t.d.lib_name]={name:t.d.lib_name,code_prefix:"sw_"};
		t.c.include_libs="Main, BBS Software";t.c.public_libs="BBS Software";
		check(t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
		t.c.exclude_libs="Private, BBS Software";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
	},
	"exclusions win over includes and public selections":function() {
		var t=policy();t.c.public_libs="*";t.c.exclude_dirs="ID_pub*";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
		t.c.exclude_dirs="";t.c.exclude_libs="Fi*";check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
	},
	"anonymous public selection cannot bypass native account ARS":function() {
		var t=policy();check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));t.c.public_dirs="id_public";
		check(t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));t.u.can_access_dir=function() { return false; };
		check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,true,false));
	},
	"download checks require both directory visibility and native download authorization":function() {
		var t=policy();check(t.scope.IRC_files_allowed(t.d,t.u,t.c,false,true));check(t.calls.join(",")=="access,download");
		t.u.can_access_dir=function(code,type) { return type!="download"; };check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,true));
	},
	"special upload, sysop, user-transfer and offline directories never export":function() {
		var t=policy();["is_upload","is_sysop","is_offline"].forEach(function(flag) { t.d[flag]=true;check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,true));delete t.d[flag]; });
		t.scope.file_area.user_dir=t.d;check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,true));
	},
	"absent special-directory configuration is resolved once without caching authorization":function() {
		var t=policy(),reads=0;delete t.scope.file_area.user_dir;
		Object.defineProperty(t.scope.file_area,"user_dir",{get:function() { reads++;return undefined; }});
		check(t.scope.IRC_files_allowed(t.d,t.u,t.c,false,false));
		check(t.scope.IRC_files_allowed(t.d,t.u,t.c,false,true));check(reads==1);
		t.u.can_access_dir=function() { return false; };check(!t.scope.IRC_files_allowed(t.d,t.u,t.c,false,false));
	},
	"library listings stop at an accessible directory and directory filters precede ARS":function() {
		var t=policy(),checked=[];t.scope.IRC_files_user=function() { return t.u; };
		t.scope.file_area.dir.id_second={number:1,code:"id_second",name:"Second",lib_name:"Files"};
		t.scope.file_area.lib.Other={name:"Other",code_prefix:"other_"};
		t.scope.file_area.dir.other={number:2,code:"other",name:"Other",lib_name:"Other"};
		t.u.can_access_dir=function(number) { checked.push(number);return number!=0; };
		check(t.scope.IRC_files_catalog(t.c,{},"LIBS",[]).lines.length==3);check(checked.join(",")=="0,1,2");
		checked=[];t.u.can_access_dir=function(number) { checked.push(number);return true; };
		t.scope.IRC_files_catalog(t.c,{},"LIBS",[]);check(checked.join(",")=="0,2");
		checked=[];t.scope.IRC_files_catalog(t.c,{},"DIRS",["Files"]);check(checked.join(",")=="0,1");
		checked=[];t.scope.IRC_files_catalog(t.c,{},"DIRS",["Second"],{library:"Files",browse:true});check(checked.join(",")=="1");
	},
	"LIBS uses library names without collapsing shared empty prefixes":function() {
		var t=policy();t.scope.IRC_files_user=function() { return t.u; };
		t.d.lib_name="Main Files";t.scope.file_area.lib["Main Files"]={name:"Main Files",code_prefix:"",description:"First library"};
		t.scope.file_area.lib.Other={name:"Other",code_prefix:"",description:"Second library"};
		t.scope.file_area.dir.other_public={number:1,code:"other_public",name:"Public Area",lib_name:"Other",description:"Other area"};
		var lines=t.scope.IRC_files_catalog(t.c,{id:"test"},"LIBS",[]).lines;
		check(lines[0]=='"Main Files" : First library' && lines[1]=="Other : Second library" && lines[2].indexOf("2 result(s)")>=0);
		lines=t.scope.IRC_files_catalog(t.c,{id:"test"},"DIRS",["Main Files"]).lines;
		check(lines[0]=='"Main Files/Public Area" : Pretty Area');
		lines=t.scope.IRC_files_catalog(t.c,{id:"test"},"DIRS",[],{library:"Main Files",browse:true}).lines;
		check(lines[0]=='"Public Area" : Pretty Area');
	},
	"virtual paths and contextual directory names resolve to native codes without bypassing exports":function() {
		var t=policy();t.scope.file_area.lib.Other={name:"Other",code_prefix:"other_",description:"Other"};
		t.scope.file_area.dir.other_public={number:1,code:"other_public",name:"Public Area",lib_name:"Other"};
		rejects(function() { t.scope.IRC_files_directory("Public Area",t.u,t.c,false,false); },"ambiguous");
		check(t.scope.IRC_files_directory("/files/public area",t.u,t.c,false,false)===t.d);
		check(t.scope.IRC_files_directory("Public Area",t.u,t.c,false,false,"Files")===t.d);
		t.scope.IRC_files_user=function() { return t.u; };
		t.scope.FileBase=function(code) { check(code=="id_public");this.open=function() { return true; };this.close=function() {};this.get_list=function() { return [{name:"test.zip",size:1,desc:"Test"}]; }; };
		check(t.scope.IRC_files_catalog(t.c,{id:"test"},"LIST",["Public Area"],{library:"Files"}).lines[0].indexOf("Files/Public Area/test.zip")>=0);
		check(t.scope.IRC_files_directory("ID_PUBLIC",t.u,t.c,false,false)===t.d);
		var location=t.scope.IRC_files_cd("Files",t.u,t.c,false);check(location.library=="Files" && !location.dir);
		location=t.scope.IRC_files_cd("Public Area",t.u,t.c,false,"Files");check(location.library=="Files" && location.dir=="id_public");
		t.c.exclude_dirs="id_*";
		rejects(function() { t.scope.IRC_files_cd("Files",t.u,t.c,false); },"access denied");
		rejects(function() { t.scope.IRC_files_directory("Files/Public Area",t.u,t.c,false,false); },"access denied");
		check(t.scope.IRC_files_directory("Public Area",t.u,t.c,false,false).code=="other_public");
	},
	"virtual components round-trip separator and quote characters while masks preserve spaces":function() {
		var t=policy();delete t.scope.file_area.lib.Files;t.d.lib_name='Main/"Files%';t.d.name='Public\\Area';
		t.scope.file_area.lib[t.d.lib_name]={name:t.d.lib_name,code_prefix:"id_"};
		var label=t.scope.IRC_files_label(t.d);check(label=='Main%2F%22Files%25/Public%5CArea');
		check(t.scope.IRC_files_directory(label,t.u,t.c,false,false)===t.d);
		check(t.scope.IRC_files_match("Main F*","Main Files") && t.scope.IRC_files_masks("Main Files","Main Files"));
	},
	"DCC navigation enters library then directory and lists each current level":function() {
		var t=adapter(),p=policy(),s=t.a.session(t.u),chat={},requests=[],replies=[];
		t.a.config.public_dirs="id_*";
		t.scope.file_area=p.scope.file_area;t.scope.IRC_files_user=function() { return p.u; };
		t.a.notice=function(session,text,where) { check(where===chat);replies.push(text); };
		t.a.spawn=function(session,kind,extra) { requests.push(extra); };
		t.a.command(t.u,"DIR",chat);check(requests[0].command=="LIBS");
		t.a.command(t.u,"CD Files",chat);check(chat.library=="Files" && !chat.cwd && replies.pop()=="/Files");
		t.a.command(t.u,"DIR Pub*",chat);check(requests[1].command=="DIRS" && requests[1].context.library=="Files" && requests[1].args[0]=="Pub*");
		t.a.command(t.u,'CD "Public Area"',chat);check(chat.cwd=="id_public" && replies.pop()=="/Files/Public Area");
		t.a.command(t.u,"PWD",chat);check(replies.pop()=="/Files/Public Area");
		t.a.command(t.u,"DIR *.zip",chat);check(requests[2].command=="LIST" && requests[2].args.join("|")=="id_public|*.zip");
		t.a.command(t.u,"GET test.zip PLAIN",chat);check(requests[3].command=="GET" && requests[3].args.join("|")=="id_public|test.zip");
		t.a.command(t.u,'INFO "Files/Public Area" test.zip',chat);check(requests[4].args.length==2);
		t.a.command(t.u,"CD ..",chat);check(chat.library=="Files" && !chat.cwd && replies.pop()=="/Files");
		t.a.command(t.u,"CD ..",chat);check(!chat.library && !chat.cwd && replies.pop()=="/");
		t.a.command(t.u,"CD id_public",chat);check(chat.cwd=="id_public" && chat.library=="Files");
		t.a.command(t.u,"CD /",chat);check(!chat.library && !chat.cwd);
		t.a.command(t.u,"CD ../",chat);check(replies.pop().indexOf("access denied")>=0 && !chat.cwd);
	},
	"DCC LIST and LIST dot browse the current level while explicit listings keep their paths":function() {
		var t=adapter(),chat={},requests=[];
		t.a.spawn=function(session,kind,extra) { requests.push(extra); };
		function listing(text,command,args) {
			t.a.command(t.u,text,chat);var request=requests.pop();
			check(request.command==command && request.args.join("|")==args && request.context.browse);
		}
		listing("LIST","LIBS","");listing("LIST .","LIBS","");
		chat.library="Files";listing("LIST","DIRS","");listing("LIST . Pub* 2","DIRS","Pub*|2");
		chat.cwd="id_public";listing("LIST","LIST","id_public");listing("LIST . *.zip 2","LIST","id_public|*.zip|2");
		t.a.command(t.u,'LIST "Other/Downloads" *.txt',chat);var request=requests.pop();
		check(request.command=="LIST" && request.args.join("|")=="Other/Downloads|*.txt" && !request.context.browse);
		/* IRC queries never inherit the DCC browser's current directory. */
		t.a.command(t.u,"LIST");request=requests.pop();check(request.command=="LIST" && !request.args.length && !request.context.browse);
		t.scope.system.timer++;t.a.command(t.u,"PWD");check(t.a.output.pop().indexOf("Unknown command")>=0);
	},
	"DCC accepts numeric addresses without DNS and rejects invalid IPv4 octets":function() {
		check(IRC_files_ip("::ffff:127.0.0.1")=="127.0.0.1");check(!IRC_files_ip("256.1.2.3") && !IRC_files_ip("example.com"));
		check(IRC_files_ip("::1")==IRC_files_ip("0:0:0:0:0:0:0:1"));check(!IRC_files_ip(":::1") && !IRC_files_ip("1:2:3") && !IRC_files_ip("12345::1"));
		check(IRC_files_dcc_address("127.0.0.1")=="2130706433");rejects(function() { IRC_files_dcc_address("0.0.0.0"); });
	},
	"separate endpoints follow the peer family and preserve legacy numeric configuration":function() {
		var t=adapter(),s=t.a.session(t.u),result,error;
		function resolve() { result=error=null;t.a.resolve_endpoint(s,function(e,err) { result=e;error=err; }); }
		resolve();check(result.address=="127.0.0.1" && result.bind=="0.0.0.0");
		t.u.ip="::1";resolve();check(!result && error.indexOf("IPv6")>=0);
		t.a.config.address6="2001:db8::1";t.a.config.bind6="::1";resolve();check(result.address==IRC_files_ip("2001:db8::1") && result.bind==IRC_files_ip("::1"));
		t.a.config.address4="192.0.2.1";t.a.config.bind4="127.0.0.1";s.address="::ffff:127.0.0.1";resolve();check(result.address=="192.0.2.1" && result.bind=="127.0.0.1");
		t.a.config.address6="";t.a.config.bind6="";t.a.config.address="2001:db8::2";t.a.config.bind="::1";s.address=null;resolve();check(result.address==IRC_files_ip("2001:db8::2") && result.bind==IRC_files_ip("::1"));
		t.a.config.address="";resolve();check(result.address==IRC_files_ip("::1"));
	},
	"dual-stack hostname lookups are asynchronous, shared, family-specific and TTL bounded":function() {
		var t=adapter(),s=t.a.session(t.u),queries=[],results=[];
		t.scope.DNS_Resolver={query:function(q,callback,ctx) { queries.push({q:q[0],reply:function(address) { callback.call(ctx,{rcode:0,answers:[{type:q[0].type,class:1,rdata:address,ttl:2}]}); }}); }};
		t.a.config.address=t.a.config.bind="files.test";
		t.a.resolve_endpoint(s,function(e) { results.push(e); });t.a.resolve_endpoint(s,function(e) { results.push(e); });
		check(!results.length && queries.length==1 && queries[0].q.type==1);
		queries[0].reply("127.0.0.1");check(results.length==2 && results[0].bind=="127.0.0.1");
		t.a.resolve_endpoint(s,function(e) { results.push(e); });check(queries.length==1 && results.length==3);
		t.u.ip="::1";t.a.resolve_endpoint(s,function(e) { results.push(e); });check(queries.length==2 && queries[1].q.type==28 && results.length==3);
		queries[1].reply("::1");check(results[3].address==IRC_files_ip("::1"));
		t.scope.system.timer+=3;t.a.resolve_endpoint(s,function(e) { results.push(e); });check(queries.length==3 && results.length==4);
	},
	"missing DNS families and timeout callbacks fail closed without starting DCC":function() {
		[false,true].forEach(function(timeout) {
			var t=adapter(),callback,ctx,started=false;t.u.ip="::1";t.a.config.address=t.a.config.bind="files.test";
			t.scope.DNS_Resolver={query:function(q,fn,context) { callback=fn;ctx=context; }};
			t.a.start_chat=function() { started=true; };t.a.command(t.u,"CHAT");check(!started && t.a.session(t.u).checking);
			if (timeout) callback.call({thisObj:ctx});else callback.call(ctx,{rcode:0,answers:[]});
			check(!started && !t.a.session(t.u).checking && t.a.output[0].indexOf("IPv6")>=0);
		});
	},
	"wrong-family endpoints are rejected and unspecified advertised addresses cannot reach workers":function() {
		var t=adapter(),started=false;t.a.start_chat=function() { started=true; };
		t.a.config.bind4="::1";t.a.command(t.u,"CHAT");check(!started && t.a.output[0].indexOf("bind address")>=0);
		t.scope.system.timer++;t.a.config.bind4="";t.a.config.address4="::1";t.a.command(t.u,"CHAT");check(!started && t.a.output[1].indexOf("endpoint")>=0);
		t.scope.system.timer++;t.a.config.address4="";t.a.config.address="";t.a.command(t.u,"CHAT");check(!started && t.a.output[2].indexOf("endpoint")>=0);
	},
	"CANCEL during DNS resolution prevents late offers and permits the next command":function() {
		var t=adapter(),callback,ctx,started=false;t.a.config.address=t.a.config.bind="files.test";
		t.scope.DNS_Resolver={query:function(q,fn,context) { callback=fn;ctx=context; }};t.a.start_chat=function() { started=true; };
		t.a.command(t.u,"CHAT");t.scope.system.timer++;t.a.command(t.u,"CANCEL ALL");check(!t.a.session(t.u).checking);
		callback.call(ctx,{rcode:0,answers:[{type:1,class:1,rdata:"127.0.0.1",ttl:1}]});check(!started);
		t.scope.system.timer++;t.a.config.address=t.a.config.bind="127.0.0.1";t.a.command(t.u,"CHAT");check(started);
	},
	"queued transfers and offers retain the selected endpoint after ADDRESS changes":function() {
		var t=adapter(),s=t.a.session(t.u),endpoint={address:"2001:db8::1",bind:"::",peer:"::1"},lines=[];
		t.a.enqueue(s,{dir:"id_files",name:"a.zip",size:1,cost:0},true,null,endpoint);check(t.a.pending[0].endpoint===endpoint);
		t.u.originatorout=function(line) { lines.push(line); };t.a.config.address6="2001:db8::2";s.address="127.0.0.1";
		t.a.frame({worker:{kind:"chat",session:s,endpoint:endpoint,tls:false}},{type:"offer",port:40000});
		check(lines[0].indexOf("DCC CHAT chat "+IRC_files_ip("2001:db8::1")+" 40000")>=0);
	},
	"quoted filenames stay single arguments and cannot inject filesystem paths":function() {
		var a=IRC_files_args('GET id_files "two words.zip" TLS');check(a.length==4 && a[2]=="two words.zip");
		check(IRC_files_name(a[2]));["../secret","a/b","a\\b","a\r\nb",".",".."].forEach(function(n) { check(!IRC_files_name(n)); });
		rejects(function() { IRC_files_args('GET id_files "unfinished'); });
	},
	"TLS required refuses explicit plaintext before authorizing or starting a worker":function() {
		var t=adapter();t.a.config.tls="required";t.a.authorize=function() { throw new Error("Should not authorize plaintext"); };
		t.a.command(t.u,"CHAT PLAIN");check(t.a.output[0].indexOf("TLS policy")>=0);
	},
	"TLS off chooses plaintext and rejects an encrypted override":function() {
		var t=adapter(),secure;t.a.config.tls="off";t.a.start_chat=function(s,tls) { secure=tls; };t.a.command(t.u,"CHAT");check(secure===false);
		t.scope.system.timer+=1;t.a.command(t.u,"CHAT TLS");check(t.a.output[t.a.output.length-1].indexOf("TLS policy")>=0);
	},
	"worker authentication rejects wrong tokens and reuse of a claimed worker":function() {
		var t=adapter(),w={id:"1",token:"0123456789abcdef0123456789abcdef"};t.a.workers[1]=w;
		rejects(function() { t.a.frame({}, {type:"hello",id:"1",token:"wrong"}); });
		var c={output:""};t.a.frame(c,{type:"hello",id:"1",proof:IRC_files_mac("0123456789abcdef0123456789abcdef","worker:1")});check(c.worker===w);
		rejects(function() { t.a.frame({}, {type:"hello",id:"1",proof:IRC_files_mac("0123456789abcdef0123456789abcdef","worker:1")}); });
	},

	"cancellation survives worker startup and prevents cancelled catalog results":function() {
		var t=adapter(),s=t.a.session(t.u),called=false,w={id:"1",kind:"catalog",token:"0123456789abcdef0123456789abcdef",session:s,cancelled:true,callback:function() { called=true; }};
		t.a.workers[1]=w;s.catalog=1;var c={output:"",socket:{close:function() {}}};
		t.a.frame(c,{type:"hello",id:"1",proof:IRC_files_mac(w.token,"worker:1")});check(c.output.indexOf("cancel")>=0);
		t.a.frame(c,{type:"result",result:{file:{name:"should-not-queue"}}});check(!called && !t.a.workers[1] && s.catalog==0);
	},
	"late progress after CANCEL cannot extend the worker shutdown deadline":function() {
		var t=adapter(),s=t.a.session(t.u),w={kind:"send",session:s,cancelled:true,bytes:0,file:{size:100},until:103};
		t.a.frame({worker:w},{type:"progress",bytes:42});check(w.bytes==42 && w.until==103);
	},
	"a replacement connection with the same nickname never inherits FileServ work":function() {
		var t=adapter(),s=t.a.session(t.u);check(t.a.valid(s));t.a.manager.ref=function() { return "rebound-cookie"; };check(t.a.session(t.u)===s);t.scope.Users.READER={nick:"Reader"};check(!t.a.valid(s));
	},
	"channel requests cannot change a private DCC peer address":function() {
		var t=adapter();t.a.command(t.u,"ADDRESS 192.0.2.5",null,true);
		check(t.a.session(t.u).address===null && t.a.output[0].indexOf("privately")>=0);
	},
	"channel browser commands honor PLAIN, TLS and the default transport":function() {
		["!fserve","!files"].forEach(function(verb) {
			["plain","TLS",""].forEach(function(option) {
				var t=adapter(),chosen;
				t.a.bot={id:"bot",channels:{"#FILES":true}};t.a.start_chat=function(s,tls) { chosen=tls; };
				t.a.channel(t.u,{nam:"#files",users:{bot:true}},verb+" here "+(verb=="!files" ? "CHAT " : "")+option);
				check(chosen===(option!="plain"),verb+" ignored "+option);
			});
		});
	},
	"channel browser options obey TLS policy and reject extra arguments":function() {
		[ {policy:"required",args:"PLAIN",error:"TLS policy"}, {policy:"off",args:"TLS",error:"TLS policy"}, {policy:"optional",args:"extra PLAIN",error:"CHAT [TLS|PLAIN]"} ].forEach(function(test) {
			var t=adapter(),started=false;t.a.config.tls=test.policy;t.a.bot={id:"bot",channels:{"#FILES":true}};
			t.a.start_chat=function() { started=true; };
			t.a.channel(t.u,{nam:"#files",users:{bot:true}},"!fserve HERE "+test.args);
			check(!started && t.a.output[0].indexOf(test.error)>=0);
		});
	},
	"DCC resume belongs to the same session, filename and listening port":function() {
		var t=adapter(),s=t.a.session(t.u),sent=[];t.a.write=function(w,m) { sent.push(m); };
		t.a.workers[1]={kind:"send",session:s,port:1234,file:{name:"a.txt",size:100}};
		t.a.resume({},"\x01DCC RESUME a.txt 1234 20\x01");t.a.resume(s,"\x01DCC RESUME b.txt 1234 20\x01");t.a.resume(s,"\x01DCC RESUME a.txt 1234 100\x01");check(!sent.length);
		t.a.resume(s,"\x01DCC RESUME a.txt 1234 20\x01");check(sent.length==1 && sent[0].offset==20);
	},
	"FileServ queues are bounded per BBS account across different IRC sessions":function() {
		var t=adapter(),s=t.a.session(t.u);s.identity={id:"same"};t.a.config.user_queue=1;t.a.pending=[{session:{},identity:{id:"same"}}];
		t.a.enqueue(s,{dir:"id_files",name:"a.zip",size:1,cost:0},true);check(t.a.pending.length==1 && t.a.output[0].indexOf("full")>=0);
	},
	"TLS policy and resource configuration rejects invalid values":function() {
		var old=EmbeddedServices_Config.files;
		try {
			EmbeddedServices_Config.files=IRC_files_defaults();ini_FileServ("",{TLS:"required",PublicDirs:"pub_*",PortFirst:40000,PortLast:40004});check(EmbeddedServices_Config.files.port_last==40004);
			[ {TLS:"maybe"},{MaxDownloads:0},{MaxChats:999},{PortFirst:70000},{PortFirst:1,PortLast:2000} ].forEach(function(cfg) { EmbeddedServices_Config.files=IRC_files_defaults();rejects(function() { ini_FileServ("",cfg); }); });
		} finally { EmbeddedServices_Config.files=old; }
	},

	"linked identity presentation never substitutes for a serving BBS attestation":function() {
		var t=adapter(),s=t.a.session(t.u);t.a.authorize=t.scope.IRC_FileServ.prototype.authorize;t.u.local=false;t.u.servername="remote.test";
		t.a.manager.config.server="here.test";t.a.manager.attestations={};
		t.u.services_identity={id:"linked",home:"here.test"};var answer="unset";
		t.a.authorize(s,function(identity,error) { check(!error);answer=identity; });check(answer===null);
	},
	"authority replies must match the serving BBS's independent session proof":function() {
		var t=adapter(),s=t.a.session(t.u),peer={},error;t.a.authorize=t.scope.IRC_FileServ.prototype.authorize;
		t.u.local=false;t.u.servername="remote.test";t.a.manager.config.server="here.test";
		t.a.manager.attestations={proven:{user:t.u,identity:{id:"HERE:1:10"},actor:{server:"remote.test",created:10}}};
		t.a.manager.central_peer=function() { return "central"; };t.a.manager.peer=function() { return peer; };t.a.capable=peer;
		t.a.manager.request=function(p,m,callback) { callback({identity:{id:"HERE:1:10",home:"here.test",qwk:"HERE"},actor:{ref:"forged",server:"remote.test",created:10}}); };
		t.a.authorize(s,function(identity,e) { error=e; });check(error && error.indexOf("Identify")>=0);
	},
	"loss of the authority cancels protected work and retains only guest eligibility":function() {
		var t=adapter(),s=t.a.session(t.u);t.a.authorize=t.scope.IRC_FileServ.prototype.authorize;t.u.local=false;t.u.servername="remote.test";
		s.identity={id:"HERE:1:10"};t.a.manager.config.server="here.test";
		t.a.manager.attestations={proven:{user:t.u}};t.a.manager.central_peer=function() { return null; };
		t.a.pending=[{session:s,identity:s.identity}];var result;
		t.a.authorize(s,function(identity,error) { check(!error);result=identity; });check(result===null && !t.a.pending.length && !s.identity);
	},
	"only the configured authority can query a connection server's actual BBS logins":function() {
		var scope={ServerName:"connection.test",Users:{},Servers:{},system:{timer:100},log:function() {}};
		load(scope,"ircd/services-common.js");load(scope,"ircd/services.js");
		var m=Object.create(scope.IRC_Services_Manager.prototype),replies=[],u={local:true};
		m.peer=function(name) { return {nick:name,servername:"authority.test"}; };m.central_peer=function() { return "authority"; };
		m.rpc_log=function() {};m.find=function() { return u; };m.logins=function() { return [{home:"here.test",id:"actual"}]; };
		m.reply=function(peer,id,r) { replies.push(r); };
		m.receive_message("untrusted",{type:"files-check",id:"1",actor:{},home:"here.test"});check(!replies.length);
		m.receive_message("authority",{type:"files-check",id:"2",actor:{},home:"here.test"});check(replies.length==1 && replies[0].identity.id=="actual");
		m.receive_message("authority",{type:"files-check",id:"3",actor:{},home:"linked.test"});check(replies[1].identity===null);
	},

	"a locally owned FileServ JOIN claims its timestamp before sending to an upgraded hub":function() {
		var s=new_state(),hub=new_peer(s,"uplink.test",true),bot=new_user(s,"FileServ_TEST");
		hub.clock_enforce=false;hub.their_capab.push("TSMAP");bot.local=false;bot.services_fileserv=true;delete s.Local_Users[bot.id];
		bot.do_join("#files");
		check(hub.sent.length>=2 && hub.sent[0].indexOf("TSMAP CLAIM ")>=0 && hub.sent[1].indexOf(" JOIN ")>=0,"FileServ JOIN could split a TSMAP hub link");
	},

	"a network nickname collision stops FileServ without killing the existing remote user":function() {
		var s=new_state(),peer=new_peer(s,"uplink.test",true),bot=new_user(s,"FileServ_TEST"),stopped=false;
		bot.local=false;bot.services_fileserv=true;delete s.Local_Users[bot.id];
		s.IRC_Services={fileserv:{bot:bot,stop:function(except) { check(except===peer);stopped=true;bot.quit("collision",true); }}};
		peer.work(":uplink.test NICK FileServ_TEST 1 1800000000 +i ordinary remote.test uplink.test 0 2130706433 :Existing remote user");
		check(stopped && !s.IRC_Services.fileserv && s.Users.FILESERV_TEST.parent==peer.nick);
		check(!peer.sent.some(function(line) { return / KILL | QUIT /.test(line); }),"collision affected the existing user");
	},

	"PASS records a verified BBS login with FileServ enabled and NickServ disabled":function() {
		var s=new_state();load(s,"ircd/unregistered.js");
		s.EmbeddedServices_Config.nickserv=false;s.EmbeddedServices_Config.fileserv=true;
		s.system={matchuser:function() {},qwk_id:"TEST"};s.User=function() { return {}; };
		s.IRC_services_authenticate=function() { return {id:"TEST:1:10",number:1,username:"Reader+TEST"}; };
		var u={socket:{ssl_session:false},password:"fixture",uprefix:"Reader",nick:"Reader",hostname:"here.test",ircout:function() {},welcome:function() { this.welcomed=true; }};
		s.Unregistered_Check_User_Registration.call(u);check(u.welcomed && u.services_identity && u.uprefix=="Reader+TEST");
	},
	"FileServ private commands and worker capabilities never appear in wire debug logs":function() {
		check(IRC_services_redact("PRIVMSG FileServ@bbs.test :ADDRESS 192.0.2.5").indexOf("192.0.2.5")<0);
		check(IRC_services_redact("PRIVMSG FileServ_BBS :GET protected.zip").indexOf("protected.zip")<0);
		var summary=IRC_services_log_rpc({type:"files-access",subject:{nick:"Reader",server:"remote.test"},token:"SECRET"});
		check(summary.indexOf("SECRET")<0 && summary.indexOf("nick=Reader")>=0 && summary.indexOf("connection=remote.test")>=0);
	}
});
if (argv.indexOf("--bbs-fixture")>=0) {
	if (system.ctrl_dir!="/tmp/ircd-services-bbs-fixture/ctrl/") throw new Error("Unsafe native fixture");
	var c=IRC_files_defaults();c.public_dirs="fs_public";
	var u=new User(system.matchuser("Some User",false)),identity=IRC_services_identity(u,system.qwk_id,"syncnix.test");
	run_tests({
		"native library filters select short names independently of code prefixes and retain guest ARS":function() {
			var config=IRC_files_defaults();config.include_libs="Files";config.public_libs="Files";
			check(IRC_files_catalog(config,null,"LIBS",[]).lines[0]=="Files : FileServ fixture library");
			check(IRC_files_catalog(config,null,"LIST",["fs_public"]).lines.some(function(line) { return line.indexOf("fixture.txt")>=0; }));
			rejects(function() { IRC_files_catalog(config,null,"LIST",["fs_private"]); },"denied");
			config.exclude_libs="Files";check(IRC_files_catalog(config,identity,"LIBS",[]).lines.length==1);
			config.exclude_libs="";config.include_libs="fs_*";check(IRC_files_catalog(config,identity,"LIBS",[]).lines.length==1);
		},
		"native special-directory checks leave the materialized directory table intact":function() {
			Object.keys(file_area.dir).forEach(function(code) { IRC_files_special(file_area.dir[code]); });
			var dirs=file_area.dir;
			Object.keys(dirs).forEach(function(code) {
				var d=dirs[code];IRC_files_special(d);
				check(u.can_access_dir(d.code)==u.can_access_dir(d.number));
				check(u.can_access_dir(d.code,"download")==u.can_access_dir(d.number,"download"));
			});
			check(file_area.dir===dirs);
		},
		"guest selection uses the lowest active G-restricted account rather than its alias":function() { var g=IRC_files_guest(c);check(g.alias=="Public Reader" && (g.security.restrictions & UFLAG_G));c.guest=u.number;check(!IRC_files_guest(c));c.guest=0; },
		"native catalog hides protected directories from guests and grants eligible owners":function() { rejects(function() { IRC_files_catalog(c,null,"LIST",["fs_private"]); },"denied");check(IRC_files_catalog(c,identity,"LIST",["fs_private"]).lines[0].indexOf("fixture.txt")>=0);check(IRC_files_catalog(c,identity,"SEARCH",["2026"]).lines.length>=1); },
		"native downloads reject symlink escapes and stale account identities":function() { rejects(function() { IRC_files_catalog(c,identity,"GET",["fs_public","escape.txt"]); },"unavailable");var stale=IRC_services_copy(identity);stale.created--;rejects(function() { IRC_files_user(stale,c); }); },
		"native credit checks enforce costs and library exemptions":function() {
			var credits=u.security.credits;try { u.security.credits=0;rejects(function() { IRC_files_catalog(c,identity,"GET",["fs_paid","fixture.txt"]); },"credits");check(IRC_files_catalog(c,identity,"GET",["ex_exempt","fixture.txt"]).file.cost==0); } finally { u.security.credits=credits; }
		}
	});
}
