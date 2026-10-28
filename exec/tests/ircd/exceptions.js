/* Channel exceptions, list synchronization, and legacy argument framing. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

function scenario() {
	var s = new_state(); s.IRC_Services = null;
	var owner = new_user(s,"Owner"), bot = new_user(s,"Bot"), listener = new_user(s,"Listener");
	owner.uprefix = "owner"; owner.hostname = "example.net";
	bot.uprefix = "bot"; bot.hostname = "example.net";
	var c = join_user(s,owner,"#test"); join_user(s,listener,c.nam);
	c.modelist[s.CHANMODE_OP][owner.id] = owner;
	c.mode |= s.CHANMODE_NOOUTSIDE | s.CHANMODE_MODERATED;
	return {s:s, owner:owner, bot:bot, listener:listener, c:c};
}
function mode(t, text) { t.owner.work("MODE " + t.c.nam + " " + text); }
function entries(t) { return Object.keys(t.c.modelist[t.s.CHANMODE_EXCEPT]).map(function(i) { return t.c.modelist[t.s.CHANMODE_EXCEPT][i]; }); }
function sends(t, type) {
	t.listener.received = []; t.bot.received = [];
	t.bot.do_msg(t.c.nam,type || "PRIVMSG","exception test");
	return t.listener.received.some(function(l) { return l.indexOf((type || "PRIVMSG") + " #test :exception test") >= 0; });
}
run_tests({
	"embedded services have no physical route and cannot crash channel forwarding": function() {
		var t=scenario(), synthetic=new_user(t.s,"ChanServ"); synthetic.local=false; synthetic.parent=0;
		join_user(t.s,synthetic,t.c.nam); mode(t,"+ee ~m:external:Bot ~m:moderated:Bot");
		check(sends(t));
	},
	"external and moderated grants are independent and do not grant membership": function() {
		var t = scenario(); check(!sends(t));
		mode(t,"+e ~msgbypass:external:Bot!*@*"); check(!sends(t),"external bypassed moderation");
		mode(t,"+e ~m:moderated:Bot!*@*");
		check(sends(t) && sends(t,"NOTICE")); check(!t.bot.channels["#TEST"] && !t.c.users[t.bot.id]);
		t.bot.received = []; t.owner.do_msg(t.c.nam,"PRIVMSG","not a subscriber");
		check(!t.bot.received.some(function(l) { return l.indexOf("not a subscriber") >= 0; }));
		mode(t,"-e ~m:external:Bot!*@*"); check(!sends(t));
	},
	"moderated bypass does not bypass external restriction or bans": function() {
		var t = scenario(); mode(t,"+e ~m:moderated:Bot!*@*"); check(!sends(t));
		mode(t,"+e ~m:external:Bot!*@*"); mode(t,"+b Bot!*@*"); check(!sends(t));
		mode(t,"+e Bot!*@*"); check(sends(t));
		mode(t,"-e ~m:moderated:Bot!*@*"); check(!sends(t));
	},
	"ordinary ban exceptions affect joins and bans without bypassing other modes": function() {
		var t = scenario(); mode(t,"+b Bot!*@*"); check(!t.bot.do_join("#test"));
		mode(t,"+e Bot"); check(!t.c.isbanned(t.bot.nuh));
		check(!sends(t)); check(t.bot.do_join("#test")); check(!sends(t));
		mode(t,"+e ~m:moderated:Bot"); check(sends(t));
		mode(t,"-e Bot"); check(!sends(t),"moderated exception bypassed ban");
	},
	"exception parsing preserves full services identities and normalizes aliases": function() {
		var s = new_state(), p = s.IRC_exception_parse;
		check(p("~M:MODERATED:Bot").value == "~msgbypass:moderated:Bot!*@*");
		check(p("Some%2B.User+VERT@host").value == "*!Some%2B.User+VERT@host");
		check(p("2001:db8::1").value == "*!*@2001:db8::1");
		check(p("~m:external:*!~user@2001:db8::1").mask == "*!~user@2001:db8::1");
		["~msgbypass:censor:*!*@*","~m:color:*!*@*","~m:notice:*!*@*","~account:Alice","~m:external:~account:Alice","a,b","a b","a!b@c@d",new Array(240).join("a")].forEach(function(v) { check(!p(v),"accepted " + v); });
	},
	"full BBS usernames match without truncation and grants do not reveal secret channel names": function() {
		var t=scenario(); t.bot.uprefix="Some%2B.User+VERT";
		mode(t,"+ee ~m:external:*!Some%2B.User+VERT@example.net ~m:moderated:*!Some%2B.User+VERT@example.net");
		check(sends(t)); t.bot.uprefix="Some%2B.User+OTHER"; check(!sends(t));
		t.bot.uprefix="Some%2B.User+VERT"; mode(t,"+s"); check(sends(t));
		t.bot.received=[]; t.bot.work("NAMES #test");
		check(!t.bot.received.some(function(l) { return l.indexOf(" 353 ")>=0; }),"grant exposed secret membership");
	},
	"list queries, permissions, canonical duplicates and metadata behave like bans": function() {
		var t = scenario(); t.listener.work("MODE #test +e Bot"); check(!entries(t).length);
		mode(t,"+e ~m:external:Bot"); var stamp=t.c.excepttime[0], creator=t.c.exceptcreator[0];
		t.s.now++; mode(t,"+e ~msgbypass:external:bot!*@*"); check(entries(t).length == 1);
		check(t.c.excepttime[0] == stamp && t.c.exceptcreator[0] == creator);
		t.listener.received=[]; t.listener.work("MODE #test +e");
		check(t.listener.received.some(function(l) { return l.indexOf(" 348 Listener #test ~msgbypass:external:Bot!*@*") >= 0; }));
		check(t.listener.received.some(function(l) { return l.indexOf(" 349 ") >= 0; }));
		mode(t,"+e ~m:censor:Bot"); check(entries(t).length == 1 && t.owner.received.some(function(l) { return l.indexOf(" 696 ") >= 0; }));
	},
	"local exception capacity is independent of the ban list": function() {
		var t=scenario();
		for(var i=0;i<25;i++) { mode(t,"+e User"+i); mode(t,"+b Ban"+i); }
		mode(t,"+e Full"); check(entries(t).length == 25 && t.c.count_modelist(t.s.CHANMODE_BAN) == 25);
		mode(t,"-e User0"); mode(t,"+e Replacement"); check(entries(t).length == 25);
	},
	"mixed MODE arguments stay aligned for capable and legacy peers": function() {
		var t=scenario(), fresh=new_peer(t.s,"new.test",true), old=new_peer(t.s,"old.test",true);
		fresh.their_capab.push("CHANEXCEPT"); old.their_capab=[];
		t.s.IRC_send_chanmode(old,t.owner,t.c,"+eklov-e key!u@h secret 5 Listener Bot key!u@h",t.c.created);
		var line=old.sent.join("\n"); check(line.indexOf("+klov secret 5 Listener Bot") >= 0 && line.indexOf("key!u@h") < 0,line);
		old.sent=[]; t.s.IRC_send_chanmode(old,t.owner,t.c,"-le+v Bot!*@* Listener",0);
		check(old.sent[0].indexOf(" -l") >= 0 && old.sent[1].indexOf("+v Listener") >= 0,old.sent.join("\n"));
		mode(t,"+eev ~m:external:Bot ~m:moderated:Bot Listener");
		check(fresh.sent.some(function(l) { return l.indexOf("~msgbypass:external:Bot!*@*") >= 0; }));
		check(!old.sent.some(function(l) { return l.indexOf("~msgbypass:") >= 0; }));
	},
	"capable bursts carry exceptions and legacy bursts omit them": function() {
		var t=scenario(), fresh=new_peer(t.s,"new.test",true), old=new_peer(t.s,"old.test",true);
		fresh.their_capab.push("CHANEXCEPT"); mode(t,"+e ~m:external:Bot"); fresh.sent=[]; old.sent=[];
		fresh.server_chan_info(t.c); old.server_chan_info(t.c);
		check(fresh.sent.some(function(l) { return l.indexOf(" +e ~msgbypass:external:Bot!*@*") >= 0; }));
		check(!old.sent.some(function(l) { return l.indexOf("~msgbypass:") >= 0; }));
	},
	"authoritative snapshots reconcile exceptions without changing retained metadata": function() {
		var t=scenario(), hub=new_peer(t.s,"hub2.test",true); hub.their_capab.push("CHANEXCEPT");
		mode(t,"+ee Keep Gone"); var stamp=t.c.excepttime[0], setter=t.c.exceptcreator[0]; t.listener.received=[];
		hub.work("CSYNC "+t.c.created+" #test BEGIN +mn"); hub.work("CSYNC "+t.c.created+" #test MORE :@Owner Listener");
		hub.work("CSYNC "+t.c.created+" #test EXCEPTS :keep!*@* Added!*@*"); hub.work("CSYNC "+t.c.created+" #test END");
		check(entries(t).sort().join(" ")=="Added!*@* Keep!*@*",entries(t).join(" "));
		check(t.c.excepttime[0]==stamp && t.c.exceptcreator[0]==setter);
		t.listener.received=[]; hub.work("CSYNC "+t.c.created+" #test BEGIN +mn"); hub.work("CSYNC "+t.c.created+" #test MORE :@Owner Listener");
		hub.work("CSYNC "+t.c.created+" #test EXCEPTS :keep!*@* added!*@*"); hub.work("CSYNC "+t.c.created+" #test END");
		check(!t.listener.received.some(function(l) { return l.indexOf(" MODE ")>=0; }),"identical snapshot churned");
	},
	"legacy snapshots cannot erase exceptions and capable snapshots can clear the list": function() {
		var t=scenario(), hub=new_peer(t.s,"hub2.test",true); mode(t,"+e Keep");
		channel_sync(hub,t.c.created,"#test","+mn","@Owner Listener",""); check(entries(t).length==1);
		hub.their_capab.push("CHANEXCEPT"); channel_sync(hub,t.c.created,"#test","+mn","@Owner Listener",""); check(!entries(t).length);
	},
	"untrusted leaves cannot forge exceptions or clear the hub's list": function() {
		var t=scenario(), leaf=new_peer(t.s,"leaf.test",false); leaf.their_capab.push("CHANEXCEPT"); mode(t,"+e Keep");
		leaf.work(":leaf.test MODE #test "+t.c.created+" +e Intruder");
		check(entries(t).length==1 && entries(t)[0]=="Keep!*@*");
		leaf.work("CSYNC "+t.c.created+" #test BEGIN +"); leaf.work("CSYNC "+t.c.created+" #test END");
		check(entries(t).length==1);
	},
	"exceptions survive timestamp resets like bans": function() {
		var t=scenario(); mode(t,"+e Keep"); t.s.Channel_reset_timestamp(t.c,t.c.created-1);
		check(entries(t).length==1);
	},
	"long exception MODEs and snapshots fit the wire without losing masks": function() {
		var t=scenario(), peer=new_peer(t.s,"leaf.test",false); peer.their_capab.push("CHANEXCEPT");
		for(var i=0;i<10;i++) mode(t,"+e ~m:external:User"+i+"!"+new Array(81).join("u")+"@"+new Array(70).join("h"));
		check(entries(t).length==10);
		peer.sent=[]; peer.server_chan_info(t.c); t.s.IRC_resync_leaf(peer,t.c);
		check(peer.sent.every(function(l) { return l.length<=510; }),"oversized burst");
		var modes=t.s.IRC_chanmode_batches(["+eeeeee"].concat(entries(t).slice(0,6)),":"+new Array(101).join("s")+" MODE #test ",6);
		check(modes.length>1 && modes.every(function(m) { return m.length+118<=510; }));
	}
});
