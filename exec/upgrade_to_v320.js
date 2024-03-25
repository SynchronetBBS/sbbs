// Convert SBBS v3.1x ctrl/*.cnf to SBBS v3.2 ctrl/*.ini

print("Upgrading Synchronet v3.1x config files to v3.20");
load('sbbsdefs.js');
var cnflib = load({}, 'cnflib.js');
var node_settings;
var node_valuser;
var node_erruser;
var node_errlevel;

function upgrade_node(dir)
{
	var path = dir + "node.cnf";
	print(path + " -> node.ini");
	var cnf = cnflib.read(path);
	if(!cnf) {
		return "Error reading " + path;
	}
	var ini = new File(dir + "node.ini");
	ini.ini_section_separator = "";
	if(!ini.open("w+")) {
		return "Error " + ini.error + " opening/creating " + ini.name;
	}
	ini.iniSetObject(null, cnf);
	ini.close();
	node_settings = cnf.settings;
	node_valuser = cnf.valuser;
	node_erruser = cnf.erruser;
	node_errlevel = cnf.errlevel;
	return true;
}

//---------------------------------------------------------------------------
print("main.cnf -> main.ini");
var cnf = cnflib.read("main.cnf");
if(!cnf) {
	alert("Error reading main.cnf");
	exit(1);
}
var ini = new File("main.ini");
ini.ini_section_separator = "";
if(!ini.open("w+")) {
	alert("Error " + ini.error + " opening/creating " + ini.name);
	exit(1);
}

ini.iniSetObject("logon_event", { cmd: cnf.sys_logon });
delete cnf.sys_logon;
ini.iniSetObject("logout_event", { cmd: cnf.sys_logout });
delete cnf.sys_logout;
ini.iniSetObject("daily_event", { cmd: cnf.sys_daily });
delete cnf.sys_daily;

var sys = {};
sys.settings = format("0x%08lx", cnf.sys_settings);
delete cnf.sys_settings;
for(var i in cnf) {
	if(i.substring(0,4) == "sys_") {
		sys[i.substring(4)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject(null, sys);

var dir = {};
for(var i in cnf) {
	if(i != "node_dir" && i.slice(-4) == "_dir") {
		dir[i.slice(0, -4)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject("dir", dir);

var mod = {};
for(var i in cnf) {
	if(i.slice(-4) == "_mod") {
		mod[i.slice(0, -4)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject("module", mod);

var newuser = {};
if(cnf.command_shell[cnf.newuser_command_shell])
	cnf.newuser_command_shell = cnf.command_shell[cnf.newuser_command_shell].code;
for(var i in cnf) {
	if(i.substring(0,8) == "newuser_") {
		newuser[i.substring(8)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject("newuser", newuser);

var exp = {};
for(var i in cnf) {
	if(i.substring(0,8) == "expired_") {
		exp[i.substring(8)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject("expired", exp);

for(var i = 0; i < cnf.node_dir.length; ++i) {
	var path = backslash(cnf.node_dir[i].path.replace('\\','/'));
	ini.iniSetValue("node_dir", i + 1, path);
	var result = upgrade_node(path);
	if(result !== true) {
		alert(result);
		if(i == 0)
			exit(1);
	}
}
delete cnf.node_dir;
cnf.login = 0;
if(node_settings & NM_LOGON_R)
	cnf.login |= LOGIN_REALNAME;
if(node_settings & NM_LOGON_P)
	cnf.login |= LOGIN_PWPROMPT;
if(!(node_settings & NM_NO_NUM))
	cnf.login |= LOGIN_USERNUM;
cnf.spinning_pause_prompt = (node_settings & NM_NOPAUSESPIN) ? "false" : "true";

for(var i in cnf.validation_set)
	ini.iniSetObject("valset:" + i, cnf.validation_set[i]);
delete cnf.validation_set;
for(var i in cnf.sec_level) {
	var level = cnf.sec_level[i];
	if(level.freecdtperd)
		level.freecdtperday = level.freecdtperd;
	level.freecdtperday = format("%lld", level.freecdtperday);
	ini.iniSetObject("level:" + i, level);
}
delete cnf.sec_level;
for(var i in cnf.command_shell) {
	var key = "shell:" + cnf.command_shell[i].code;
	delete cnf.command_shell[i].code;
	ini.iniSetObject(key, cnf.command_shell[i]);
}
delete cnf.command_shell;
cnf.valuser = node_valuser;
cnf.erruser = node_erruser;
cnf.errlevel = node_errlevel;
ini.iniSetObject(null, cnf);
ini.close();

//---------------------------------------------------------------------------
print("msgs.cnf -> msgs.ini");
var cnf = cnflib.read("msgs.cnf");
ini = new File("msgs.ini");
ini.ini_section_separator = "";
if(!ini.open("w+")) {
	alert("Error " + ini.error + " opening/creating " + ini.name);
	exit(1);
}

var mail = { 
	max_crcs: cnf.mail_maxcrcs, 
	max_age: cnf.mail_maxage,
	max_spam_age: cnf.max_spamage
};
delete cnf.mail_maxcrcs;
delete cnf.mail_maxage;
delete cnf.max_spamage;
ini.iniSetObject("mail", mail);

var qwk = {};
qwk.max_msgs = cnf.max_qwkmsgs, delete cnf.max_qwkmsgs;
qwk.prepack_ars = cnf.preqwk_ars, delete cnf.preqwk_ars;
qwk.max_age = cnf.max_qwkmsgage, delete cnf.max_qwkmsgage;
qwk.default_tagline = cnf.qwknet_default_tagline, delete cnf.qwknet_default_tagline;
ini.iniSetObject("qwk", qwk);

var fido = { 
	addr_list: [] 
};
for(var i in cnf.fido_addr_list) {
	var addr = cnf.fido_addr_list[i];
	fido.addr_list.push(format("%u:%u/%u.%u", addr.zone, addr.net, addr.node, addr.point));
}
fido.addr_list = fido.addr_list.join(",");
delete cnf.fido_default_addr; // not used
delete cnf.fido_addr_list;
for(var i in cnf) {
	if(i.substring(0,5) == "fido_") {
		fido[i.substring(5)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject("fidonet", fido);

var inet = {};
inet.addr = cnf.sys_inetaddr, delete cnf.sys_inetaddr;
inet.smtp_sem = cnf.smtpmail_sem, delete cnf.smtpmail_sem;
for(var i in cnf) {
	if(i.substring(0,9) == "inetmail_") {
		inet[i.substring(1)] = cnf[i];
		delete cnf[i];
	}
}
ini.iniSetObject("internet", inet);

for(var i in cnf.grp)
	ini.iniSetObject("grp:" + cnf.grp[i].name, cnf.grp[i]);
for(var i in cnf.sub) {
	var sub = cnf.sub[i];
	if(sub.fidonet_addr) {
		var addr = sub.fidonet_addr;
		sub.fidonet_addr = format("%u:%u/%u.%u", addr.zone, addr.net, addr.node, addr.point);
	}
	var key = "sub:" + cnf.grp[sub.grp_number].name + ":" + sub.code_suffix;
	sub.settings = format("0x%lx", sub.settings);
	ini.iniSetObject(key, sub);
}
for(var i in cnf.qwknet_hub) {
	var qhub = cnf.qwknet_hub[i];
	var subs = qhub.subs;
	for(var j in subs) {
		var sub = subs[j];
		sub.sub = cnf.grp[cnf.sub[sub.sub].grp_number].code_prefix + cnf.sub[sub.sub].code_suffix;
		var key = "qhubsub:" + qhub.id + ":" + sub.conf;
		delete sub.conf;
		ini.iniSetObject(key, sub);
	}
	delete qhub.subs;
	var key = "qhub:" + qhub.id;
	delete qhub.id;
	ini.iniSetObject(key, qhub);
}
delete cnf.grp;
delete cnf.sub;
delete cnf.qwknet_hub;
cnf.settings = format("0x%lx", cnf.settings);
ini.iniSetObject(null, cnf);
ini.close();

//---------------------------------------------------------------------------
print("file.cnf -> file.ini");
var cnf = cnflib.read("file.cnf");
ini = new File("file.ini");
ini.ini_section_separator = "";
if(!ini.open("w+")) {
	alert("Error " + ini.error + " opening/creating " + ini.name);
	exit(1);
}
for(var i in cnf.lib)
	ini.iniSetObject("lib:" + cnf.lib[i].name, cnf.lib[i]);
for(var i in cnf.dir) {
	var dir = cnf.dir[i];
	var key = "dir:" + cnf.lib[dir.lib_number].name + ":" + dir.code_suffix;
	delete dir.lib_number;
	delete dir.code_suffix;
	dir.settings = format("0x%lx", dir.settings);
	ini.iniSetObject(key, dir);
}
delete cnf.lib;
delete cnf.dir;
for(var i in cnf.fextr)
	ini.iniSetObject("extractor:" + i, cnf.fextr[i]);
delete cnf.fextr;
for(var i in cnf.fcomp)
	ini.iniSetObject("compressor:" + i, cnf.fcomp[i]);
delete cnf.fcomp;
for(var i in cnf.fview)
	ini.iniSetObject("viewer:" + i, cnf.fview[i]);
delete cnf.fview;
for(var i in cnf.ftest)
	ini.iniSetObject("tester:" + i, cnf.ftest[i]);
delete cnf.ftest;
for(var i in cnf.dlevent)
	ini.iniSetObject("dlevent:" + i, cnf.dlevent[i]);
delete cnf.dlevent;
for(var i in cnf.prot)
	ini.iniSetObject("protocol:" + i, cnf.prot[i]);
delete cnf.prot;
for(var i in cnf.txtsec) {
	var key = "text:" + cnf.txtsec[i].code;
	delete cnf.txtsec[i].code;
	ini.iniSetObject(key, cnf.txtsec[i]);
}
delete cnf.txtsec;

cnf.min_dspace = format("%uK", cnf.min_dspace);
cnf.settings = format("0x%lx", cnf.settings);
ini.iniSetObject(null, cnf);
ini.close();

//---------------------------------------------------------------------------
print("xtrn.cnf -> xtrn.ini");
var cnf = cnflib.read("xtrn.cnf");
ini = new File("xtrn.ini");
ini.ini_section_separator = "";
if(!ini.open("w+")) {
	alert("Error " + ini.error + " opening/creating " + ini.name);
	exit(1);
}
for(var i in cnf.xtrn) {
	var obj = cnf.xtrn[i];
	var key = "prog:" + cnf.xtrnsec[obj.sec].code + ":" + obj.code;
	delete obj.code;
	delete obj.sec;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.xtrnsec) {
	var obj = cnf.xtrnsec[i];
	var key = "sec:" + obj.code;
	delete obj.code;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.xedit) {
	var obj = cnf.xedit[i];
	var key = "editor:" + obj.code;
	delete obj.code;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.event) {
	var obj = cnf.event[i];
	var key = "event:" + obj.code;
	delete obj.code;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.natvpgm) {
	var obj = cnf.natvpgm[i];
	var key = "native:" + obj.name;
	delete obj.name;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.hotkey) {
	var obj = cnf.hotkey[i];
	var key = "hotkey:" + obj.key;
	delete obj.key;
	ini.iniSetObject(key, obj);
}
ini.close();


//---------------------------------------------------------------------------
print("chat.cnf -> chat.ini");
var cnf = cnflib.read("chat.cnf");
ini = new File("chat.ini");
ini.ini_section_separator = "";
if(!ini.open("w+")) {
	alert("Error " + ini.error + " opening/creating " + ini.name);
	exit(1);
}
for(var i in cnf.chan) {
	var obj = cnf.chan[i];
	var key = "chan:" + obj.code;
	delete obj.code;
	obj.actions = cnf.actset[obj.actset].name;
	delete obj.actset;
	if(cnf.guru[obj.guru])
		obj.guru = cnf.guru[obj.guru].code;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.chatact) {
	var obj = cnf.chatact[i];
	cnf.actset[obj.actset][obj.cmd] = obj.out;
}
for(var i in cnf.actset) {
	var obj = cnf.actset[i];
	var key = "actions:" + obj.name;
	delete obj.name;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.guru) {
	var obj = cnf.guru[i];
	var key = "guru:" + obj.code;
	delete obj.code;
	ini.iniSetObject(key, obj);
}
for(var i in cnf.page) {
	var obj = cnf.page[i];
	var key = "pager:" + i;
	ini.iniSetObject(key, obj);
}

ini.close();
