load("sbbsdefs.js");
load("text.js");
load("lockfile.js");

var doorscan_dir='.';
try { throw barfitty.barf(barf) } catch(e) { doorscan_dir=e.fileName }
doorscan_dir=doorscan_dir.replace(/[\/\\][^\/\\]*$/,'');
doorscan_dir=backslash(doorscan_dir);

function LockedOpen(filename, fmode)
{
	var f=new File(filename);
	var suffix='';
	var mode='';
	var wr=false;

	if(fmode.search(/b/)!=-1)
		suffix+='b';
	if(fmode.search(/\+/)!=-1) {
		suffix+= '+';
		wr=true;
	}

	/* TODO: Possible race condition here in mode */
	var m=fmode.match(/[rwa]/);
	if(m==null)
		throw("Unknown file mode "+fmode);
	switch(m[0]) {
		case 'r':
			mode='r'+suffix;
			if(wr) {
				if(!f.exists)
					mode='w'+suffix;
			}
			break;
		default:
			wr=true;
			mode=m[0]+suffix;
			break;
	}

	if(!Lock(f.name, system.node_num, wr, 5))
		throw("Unable to lock "+f.name);

	if(!f.open(mode, true)) {
		Unlock(f.name);
		throw("Unable to open "+f.name);
	}
	return(f);
}

function Display()
{
	this.ANSI=Display_ANSI;
	this.ASCII=Display_ASCII;
	this.ASC=Display_ASC;
	this.LORD=Display_LORD;
}

function Display_ANSI(filename)
{
	/* If the user doesn't support ANSI, return false... */
	if(!(console.term_supports(USER_ANSI)))
		return(false);
	return(this.ASCII(filename));
}

function Display_ASCII(filename)
{
	var f=LockedOpen(filename, "rb");
	var txt;

	while((txt=f.read())!=undefined) {
		txt=f.read();
		if(txt==undefined || txt=='')
			break;
		console.write(txt);
	}
	f.close();
	Unlock(f.name);
	return(true);
}

function Display_ASC(filename)
{
	console.printfile(filename);
	return(true);
}

function Display_LORD(filename)
{
	var f=LockedOpen(filename, "rb");
	var txt;
	var out='';

	while((txt=f.read())!=undefined) {
		txt=f.read();
		if(txt==undefined || txt=='')
			break;
		for(var i=0; i<txt.length; i++) {
			var ch=charAt(i);
			if(ch=='`') {
				if(out.length)
					console.write(out);
				out='';
				ch=txt.charAt(++i);
				switch(ch) {
					case '*':
						console.attributes=BLINK|BLACK|BG_RED;
						break;
					case '1':
						console.attributes=BLUE;
						break;
					case '2':
						console.attributes=GREEN;
						break;
					case '3':
						console.attributes=CYAN;
						break;
					case '4':
						console.attributes=RED;
						break;
					case '5':
						console.attributes=MAGENTA;
						break;
					case '6':
						console.attributes=BROWN;
						break;
					case '7':
						console.attributes=LIGHTGRAY;
						break;
					case '8':
						console.attributes=DARKGRAY;
						break;
					case '9':
						console.attributes=LIGHTBLUE;
						break;
					case '0':
						console.attributes=LIGHTGREEN;
						break;
					case '!':
						console.attributes=LIGHTCYAN;
						break;
					case '@':
						console.attributes=LIGHTRED;
						break;
					case '#':
						console.attributes=LIGHTMAGENTA;
						break;
					case '$':
						console.attributes=YELLOW;
						break;
					case '%':
						console.attributes=WHITE;
						break;
					case 'r':
						ch=txt.charAt(++i);
						switch(ch) {
						case '0':
							console.attributes=BG_BLACK;
							break;
						case '1':
							console.attributes=BG_BLUE;
							break;
						case '2':
							console.attributes=BG_GREEN;
							break;
						case '3':
							console.attributes=BG_CYAN;
							break;
						case '4':
							console.attributes=BG_RED;
							break;
						case '5':
							console.attributes=BG_MAGENTA;
							break;
						case '6':
							console.attributes=BG_BROWN;
							break;
						case '7':
							console.attributes=BG_LIGHTGRAY;
							break;
						}
						break;
					case 'c':
						console.clear();
						console.crlf();
						console.crlf();
						break;
					case 'l':
						console.attributes=GREEN;
						console.write('  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
						break;
					case '`':
						ch=console.getkey(K_NOCRLF);
						break;
					case 'b':
					case ')':
						console.attributes=BLINK|RED;
						break;
				}
			}
			else {
				out += ch;
			}
		}
	}
	f.close();
	Unlock(f.name);
	if(out.length)
		console.write(out);
	return(true);
}

/*
 * This is like Display, but for news files...
 * the idea is that this *could* display only news since the last time you
 * started the door or something.
 */
function NewsDisplay()
{
	this.ANSI=NewsDisplay_ANSI;
	this.ASCII=NewsDisplay_ASCII;
	this.ASC=NewsDisplay_ASC;
	this.LORD=NewsDisplay_LORD;
}

function NewsDisplay_ANSI(filename, since)
{
	new Display().ANSI(filename);
}

function NewsDisplay_ASCII(filename, since)
{
	new Display().ASCII(filename);
}

function NewsDisplay_ASC(filename, since)
{
	new Display().ASC(filename);
}

function NewsDisplay_LORD(filename, since)
{
	var now=new Date();

	/*
	 * TODO: Show yesterdays log...
	 */
	//if(now.getDate() != since.getDate()) {
	//}
	new Display().LORD(filename);
}

function DoorConfig(leaveopen)
{
	if(leaveopen==undefined)
		leaveopen=false;
	this.file=LockedOpen(doorscan_dir+"doors.ini", "rb+");
	this.save=DoorConfig_save;
	this.door=new Object();
	this.skipSection=new Object();

	var sections=this.file.iniGetAllObjects();
	if(sections==undefined)
		sections=new Array();

	for(var i in sections) {
		if(sections[i].installed != undefined)
			sections[i].installed=new Date(sections[i].installed);
		if(sections[i].lastRan != undefined)
			sections[i].lastRan=new Date(sections[i].lastRan);
		if(sections[i].lastExit != undefined)
			sections[i].lastExit=new Date(sections[i].lastExit);
		this.door[sections[i].name]=sections[i];
	}
	this.global=this.file.iniGetObject();
	if(this.global.skipSections != undefined) {
		var sections=this.global.skipSections.split(/,/);
		for(var sec in sections) {
			this.skipSection[sections[sec]]=true;
		}
		delete this.global.skipSections;
	}

	var updated=false;
	for(var door in xtrn_area.prog) {
		if(this.door[door]==undefined) {
			this.door[door]={name:door, installed:new Date(), runCount:0}
			sections.push(this.door[door]);
			updated=true;
		}
	}
	if(updated) {
		this.save(leaveopen);
	}
	else {
		if(!leaveopen) {
			this.file.close();
			Unlock(this.file.name);
		}
	}
}

function DoorConfig_save(leaveopen)
{
	if(leaveopen==undefined)
		leaveopen=false;
	if(!this.file.is_open)
		this.file=LockedOpen(this.file.name, "rb+");

	var sections=new Array();
	for(var section in this.skipSection) {
		if(this.skipSection[section])
			sections.push(section);
	}
	this.global.skipSections=sections.join(',');

	this.file.iniSetObject(null,this.global);
	delete this.global.skipSections;

	sections=new Array();

	for(var door in this.door) {
		/* Convert dates to strings */
		if(this.door[door].installed != undefined)
			this.door[door].installed=this.door[door].installed.toString();
		if(this.door[door].lastRan != undefined)
			this.door[door].lastRan=this.door[door].lastRan.toString();
		if(this.door[door].lastExit != undefined)
			this.door[door].lastExit=this.door[door].lastExit.toString();
		sections.push(this.door[door]);
	}

	this.file.iniSetAllObjects(sections);

	for(door in this.door) {
		/* Convert strings back to dates */
		if(this.door[door].installed != undefined)
			this.door[door].installed=new Date(this.door[door].installed);
		if(this.door[door].lastRan != undefined)
			this.door[door].lastRan=new Date(this.door[door].lastRan);
		if(this.door[door].lastExit != undefined)
			this.door[door].lastExit=new Date(this.door[door].lastExit);
	}

	if(!leaveopen) {
		this.file.close();
		Unlock(this.file.name);
	}
	return(true);
}

function UserConfig(unum, leaveopen)
{
	var parsefile=true;

	if(leaveopen==undefined)
		leaveopen=false;
	this.door=new Object();
	this.user_number=unum;
	this.save=UserConfig_save;
	this.addxtrn=UserConfig_addxtrn;
	this.configure=UserConfig_configure;
	this.configureSec=UserConfig_configure_sec;
	this.configureDefaults=UserConfig_configure_defaults;

	if(unum==undefined) {
		this.file=LockedOpen(doorscan_dir+"defaults.ini", "rb+");
	}
	else {
		this.file=LockedOpen(format("%suser/%04u.doorscan",system.data_dir,unum), "rb+");
		if(this.file.length==0) {
			var defaults=new UserConfig();

			this.global=eval(defaults.global.toSource());
			this.door=eval(defaults.door.toSource());
			parsefile=false;
		}
	}

	if(parsefile) {
		var sections=this.file.iniGetAllObjects();
		if(sections==undefined)
			sections=new Array();

		for(var i in sections) {
			if(sections[i].lastRan != undefined)
				sections[i].lastRan=new Date(sections[i].lastRan);
			if(sections[i].lastExit != undefined)
				sections[i].lastExit=new Date(sections[i].lastExit);
			this.door[sections[i].name]=sections[i];
		}
		this.global=this.file.iniGetObject();
	}

	if(this.global.lastScan==undefined) {
		this.global.lastScan=new Date();
	}
	else {
		this.global.lastScan=new Date(this.global.lastScan);
	}

	if(!leaveopen) {
		this.file.close();
		Unlock(this.file.name);
	}
}

function UserConfig_save()
{
	if(!this.file.is_open)
		this.file=LockedOpen(this.file.name, "rb+");
	var sections=new Array();

	for(var door in this.door) {
		/* Dates to strings */
		if(this.door[door].lastRan!=undefined)
			this.door[door].lastRan=this.door[door].lastRan.toString();
		if(this.door[door].lastExit!=undefined)
			this.door[door].lastRan=this.door[door].lastExit.toString();
		sections.push(this.door[door]);
	}

	this.global.lastScan=this.global.lastScan.toString();
	this.file.iniSetObject(null,this.global);
	this.global.lastScan=new Date(this.global.lastScan.toString());

	this.file.iniSetAllObjects(sections);

	for(var door in this.door) {
		/* Strings to Dates */
		if(this.door[door].lastRan!=undefined)
			this.door[door].lastRan=new Date(this.door[door].lastRan);
		if(this.door[door].lastExit!=undefined)
			this.door[door].lastRan=new Date(this.door[door].lastExit);
		sections.push(this.door[door]);
	}

	this.file.close();
	Unlock(this.file.name);
	return(true);
}

function UserConfig_addxtrn(xtrn)
{
	if(this.door[xtrn] == undefined) {
		this.door[xtrn]=new Object();
		this.door[xtrn].name=xtrn;
		if(this.global.defaultSkipScores)
			this.door[xtrn].skipScores=true;
		if(this.global.defaultSkipNews)
			this.door[xtrn].skipNews=true;
		if(this.global.defaultSkipRunCount)
			this.door[xtrn].skipRunCount=true;
	}
}

function UserConfig_configure()
{
	var door;
	var index=new Array();
	var n,s,r;
	var xsec;
	var dcfg=new DoorConfig();

	while(1) {
		for(sec in xtrn_area.sec) {
			if(!user.compare_ars(xtrn_area.sec[sec]))
				continue;
			if(dcfg.skipSection[sec]!=undefined && dcfg.skipSection[sec])
				continue;
			index.push(sec);
			console.uselect(index.length, "External Program Section", xtrn_area.sec[sec].name);
		}
		console.uselect(index.length+1, "External Program Section", "Global Settings");
		if(index.length==0)
			return;
		xsec=console.uselect();
		if(xsec < 1 || xsec > index.length+1) {
			this.save();
			return;
		}
		if(xsec <= index.length)
			this.configureSec(dcfg, index[xsec-1]);
		else
			this.configureDefaults();
	}
}

function UserConfig_configure_defaults()
{
	while(1) {
		console.uselect(1, "Global Setting", "Add all new externals to your scan           "+(this.global.addNew?"Yes":"No"));
		console.uselect(2, "Global Setting", "Add externals to your scan when you run them "+(this.global.noAutoScan?"No":"Yes"));
		console.uselect(3, "Global Setting", "Default to showing updated news entries      "+(this.global.defaultSkipNews?"No":"Yes"));
		console.uselect(4, "Global Setting", "Default to showing updated scores entries    "+(this.global.defaultSkipScores?"No":"Yes"));
		console.uselect(5, "Global Setting", "Default to showing run counts                "+(this.global.skipRunCount?"No":"Yes"));
		switch(console.uselect()) {
			case 1:
				this.global.addNew=!this.global.addNew;
				break;
			case 2:
				this.global.noAutoScan=!this.global.noAutoScan;
				break;
			case 3:
				this.global.defaultSkipNews=!this.global.defaultSkipNews;
				break;
			case 4:
				this.global.defaultSkipScores=!this.global.defaultSkipScores;
				break;
			case 5:
				this.global.skipRunCount=!this.global.skipRunCount;
				break;
			default:
				return;
		}
	}
}

function UserConfig_configure_sec(dcfg, sec)
{
	var door;
	var index=new Array();
	var n,s,r;
	var xprog;

	while(1) {
		for(door in xtrn_area.prog) {
			if(xtrn_area.prog[door].sec_code != sec)
				continue;
			if(dcfg.skipSection[xtrn_area.prog[door].sec_code]!=undefined && dcfg.skipSection[xtrn_area.prog[door].sec_code])
				continue;
			if(dcfg.door[door].skip != undefined && dcfg.door[door].skip)
				continue;
			index.push(door);
			if(this.door[door]==undefined) {
				n=ascii('-');
				s=ascii('-');
				r=ascii('-');
			}
			else {
				n=s=r=ascii('Y');
				if(dcfg.door[door].news==undefined)
					n=ascii('-');
				else {
					if(this.door[door].skipNews)
						n=ascii('N');
				}
				if(dcfg.door[door].score==undefined)
					s=ascii('-');
				else {
					if(this.door[door].skipScores)
						s=ascii('N');
				}
				if(this.door[door].skipRunCount)
					r=ascii('N');
			}
			console.uselect(index.length,"External                                News  Scores  Run Count"
					,format("%-40s   %c      %c         %c",xtrn_area.prog[door].name,n,s,r));
		}
		if(!index.length)
			return;
		xprog=console.uselect();
		if(xprog < 1 || xprog > index.length)
			return;
		console.crlf();
		if(this.door[index[xprog-1]]==undefined)
			this.addxtrn(index[xprog-1]);
		if(dcfg.door[index[xprog-1]].news!=undefined) {
			if(this.door[index[xprog-1]].skipNews)
				this.door[index[xprog-1]].skipNews=console.noyes("Show updated news files");
			else
				this.door[index[xprog-1]].skipNews=!console.yesno("Show updated news files");
		}
		if(dcfg.door[index[xprog-1]].score!=undefined) {
			if(this.door[index[xprog-1]].skipScore)
				this.door[index[xprog-1]].skipScore=console.noyes("Show updated score files");
			else
				this.door[index[xprog-1]].skipScore=!console.yesno("Show updated score files");

		}
		if(this.door[index[xprog-1]].skipRunCount)
			this.door[index[xprog-1]].skipRunCount=console.noyes("Show run counts");
		else
			this.door[index[xprog-1]].skipRunCount=!console.yesno("Show run counts");
		if(this.door[index[xprog-1]].skipRunCount
				&& this.door[index[xprog-1]].skipScore
				&& this.door[index[xprog-1]].skipNews)
			delete this.door[index[xprog-1]];
	}
}

function LogParser()
{
	var door;
	this.door=new Object();
	this.parsed=new Object();
	this.parseLog=LogParser_parselog;
	this.parseSince=LogParser_parsesince;
	this.usersOfSince=LogParser_usersOfSince;

	for(door in xtrn_area.prog) {
		this.door[door]=new Object();
		this.door[door].log=new Array();
	}
}

function LogParser_parselog(filename)
{
	var f=new File(filename);
	var uname=undefined;
	var dname=undefined;
	var obj=undefined;
	var line;
	var m;

	if(this.parsed[filename]!=undefined)
		return
	if(!f.exists) {
		this.parsed[filename]=true;
		return;
	}
	if(!f.open("rb", true))
		throw("Unable to open file "+filename);
	while((line=f.readln())!=null) {
		if(line=='') {
			uname=undefined;
			dname=undefined;
			obj=undefined;
		}
		else if((m=line.match(/^\+\+ \([0-9]+\)  (.{25})  Logon [0-9]+ - [0-9]+$/))!=null) {
			uname=m[1].replace(/^(.*?)\s*$/,"$1");
		}
		else if((m=line.match(/^   DOORSCAN - (.+?) starting @ (.*)$/))!=null) {
			if(uname!=undefined) {
				dname=m[1];
				if(this.door[m[1]]!=undefined) {
					obj={user:uname, date:(new Date(m[2])), elapsed:0};
					this.door[m[1]].log.push(obj);
				}
				else {
					log("Door "+m[1]+" logged but not configured");
				}
			}
			else {
				log("User name not found for line "+line);
			}
		}
		else if((m=line.match(/^   DOORSCAN - (.+?) ending @ (.*)$/))!=null) {
			if(uname!=undefined) {
				if(this.door[m[1]]!=undefined) {
					if(m[1]==dname) {
						obj.elapsed += (new Date(m[2]) - obj.date)/1000;
					}
					else {
						log("Door "+m[1]+" exited without starting");
					}
				}
				else {
					log("Door "+m[1]+" logged but not configured");
				}
				dname=undefined;
				obj=undefined;
			}
			else {
				log("User name not found for line "+line);
			}
		}
	}
	this.parsed[f.name]=true;
}

/*
 * Inclusive
 */
function LogParser_parsesince(since)
{
	var fname;
	var ldate=eval(since.toSource());
	var now=new Date();
	var m,d,y;
	var nm,nd,ny;
	var i;

	if(this.donesince!=undefined) {
		if(this.donesince <= since)
			return;
		now=this.donesince;
	}
	m=ldate.getMonth();
	d=ldate.getDate();
	y=ldate.getFullYear();
	nm=now.getMonth();
	nd=now.getDate();
	ny=now.getFullYear();

	/* Silly paranoia */
	if(y < ny - 100) {
		ldate=eval(now.toSource());
		ldate.setFullYear(ldate.getFullYear()-100);
		y=ldate.getFullYear();
	}
	if(y == ny-100) {
		while(m <= nm) {
			m++;
			if(m==12) {
				ldate.setFullYear(y+1);
				ldate.setMonth(0);
				break;
			}
			else {
				ldate.setMonth(m);
			}
		}
	}

	while(ldate <= now) {
		m=ldate.getMonth();
		d=ldate.getDate();
		y=ldate.getFullYear();

		fname=format("%slogs/%2.2d%2.2d%2.2d.log",system.data_dir,m+1,d,y%100);
		this.parseLog(fname);
		ldate.setDate(ldate.getDate()+1);
	}

	this.donesince=since;

	for(i in system.node_list) {
		this.parseLog(system.node_list[i].dir);
	}
}

function LogParser_usersOfSince(xtrn, since)
{
	this.parseSince(since);

	var ret=new Object();
	ret.user=new Object();
	ret.total=0;
	ret.total_duration=0;
	ret.users=0;
	ret.prog=xtrn;

	for(var i in this.door[xtrn].log) {
		if(this.door[xtrn].log[i].date >= since) {
			ret.total++;
			ret.total_duration+=this.door[xtrn].log[i].elapsed;
			if(ret.user[this.door[xtrn].log[i].user]==undefined) {
				ret.user[this.door[xtrn].log[i].user]=new Object();
				ret.user[this.door[xtrn].log[i].user].duration=0;
				ret.user[this.door[xtrn].log[i].user].count=0;
				ret.users++;
			}
			ret.user[this.door[xtrn].log[i].user].count++;
			ret.user[this.door[xtrn].log[i].user].duration+=this.door[xtrn].log[i].elapsed;
		}
	}
	return(ret);
}

//==============================

function sysop_config_externs(dcfg)
{
	var door;
	var index=new Array();
	var n,s,r;
	var xsec;

	while(1) {
		for(sec in xtrn_area.sec) {
			index.push(sec);
			console.uselect(index.length, "External Program Section", xtrn_area.sec[sec].name);
		}
		if(index.length==0)
			return;
		xsec=console.uselect();
		if(xsec < 1 || xsec >= index.length)
			return;
		sysop_config_externs_sec(dcfg, index[xsec-1]);
	}
}

function sysop_get_newstype(dflt)
{
	var i=0;
	var news=new NewsDisplay();
	var index=new Array();

	for(var t in news) {
		index.push(t);
		console.uselect(index.length, "News File Type", t);
	}
	i=console.uselect();
	if(i < 1 || i > index.length)
		return(dflt);
	return(index[i-1]);
	
}

function sysop_get_scorestype(dflt)
{
	var i=0;
	var news=new NewsDisplay();
	var index=new Array();

	for(var t in news) {
		index.push(t);
		console.uselect(index.length, "Scores File Type", t);
	}
	i=console.uselect();
	if(i < 1 || i > index.length)
		return(dflt);
	return(index[i-1]);
}

function sysop_config_externs_sec(dcfg, sec)
{
	var door;
	var index=new Array();
	var n,s,r;
	var xprog;

	while(1) {
		for(door in xtrn_area.prog) {
			if(xtrn_area.prog[door].sec_code != sec)
				continue;
			index.push(door);
			console.uselect(index.length, "External",xtrn_area.prog[door].name);
		}
		if(!index.length)
			return;
		xprog=console.uselect();
		if(xprog < 1 || xprog > index.length)
			return;
		sysop_config_externs_prog(dcfg, index[xprog-1]);
	}
}

function sysop_config_externs_prog(dcfg, door)
{
	var door;
	var index=new Array();
	var n,s,r;
	var xprog;

	while(1) {
		console.uselect(1, door+" Scan Config", "Skip this door "+(dcfg.door[door].skip?"Yes":"No"));
		console.uselect(2, door+" Scan Config", "Ad File: "+(dcfg.door[door].ad?dcfg.door[door].ad:''));
		console.uselect(3, door+" Scan Config", "Ad Type: "+(dcfg.door[door].adType));
		console.uselect(4, door+" Scan Config", "Score File: "+(dcfg.door[door].score?dcfg.door[door].score:''));
		console.uselect(5, door+" Scan Config", "Score Type: "+(dcfg.door[door].scoreType));
		console.uselect(6, door+" Scan Config", "News File: "+(dcfg.door[door].news?dcfg.door[door].news:''));
		console.uselect(7, door+" Scan Config", "News Type: "+(dcfg.door[door].newsType));
		switch(console.uselect()) {
			case 1:
				dcfg.door[door].skip=!dcfg.door[door].skip;
				break;
			case 2:
				if(dcfg.door[door].ad==undefined)
					dcfg.door[door].ad='';
				dcfg.door[door].ad=console.getstr(dcfg.door[door].ad, K_EDIT);
				if(dcfg.door[door].ad=='')
					delete dcfg.door[door].ad;
				break;
			case 3:
				dcfg.door[door].adType=sysop_get_scorestype(dcfg.door[door].adType==undefined?'ANSI':dcfg.door[door].adType);
				break;
			case 4:
				if(dcfg.door[door].score==undefined)
					dcfg.door[door].score='';
				dcfg.door[door].score=console.getstr(dcfg.door[door].score, K_EDIT);
				if(dcfg.door[door].score=='')
					delete dcfg.door[door].score;
				break;
			case 5:
				dcfg.door[door].scoreType=sysop_get_scorestype(dcfg.door[door].scoreType==undefined?'ANSI':dcfg.door[door].scoreType);
				break;
			case 6:
				if(dcfg.door[door].news==undefined)
					dcfg.door[door].news='';
				dcfg.door[door].news=console.getstr(dcfg.door[door].news, K_EDIT);
				if(dcfg.door[door].news=='')
					delete dcfg.door[door].news;
				break;
			case 7:
				dcfg.door[door].newsType=sysop_get_scorestype(dcfg.door[door].newsType==undefined?'ANSI':dcfg.door[door].newsType);
				break;
			default:
				return;
		}
	}
}

//==============================


function sysop_config_skip(dcfg)
{
	var door;
	var index=new Array();
	var n,s,r;
	var xsec;

	while(1) {
		for(sec in xtrn_area.sec) {
			index.push(sec);
			console.uselect(index.length, "External Program Section", 
					format("%-40s %s",xtrn_area.sec[sec].name
					,(dcfg.skipSection[sec] != undefined && dcfg.skipSection[sec])?"Skip":"Include"));
		}
		if(index.length==0)
			return;
		xsec=console.uselect();
		if(xsec < 1 || xsec > index.length)
			return;
		dcfg.skipSection[index[xsec-1]]=!dcfg.skipSection[index[xsec-1]];
	}
}

function sysop_config()
{
	var dcfg=new DoorConfig();

	while(1) {
		console.uselect(1, "DoorScan Setting", "Modify default scan settings");
		console.uselect(2, "DoorScan Setting", "Mark sections to skip");
		console.uselect(3, "DoorScan Setting", "Configure individual externals");
		switch(console.uselect()) {
			case 1:
				new UserConfig().configure();
				break;
			case 2:
				sysop_config_skip(dcfg);
				break;
			case 3:
				sysop_config_externs(dcfg);
				break;
			default:
				dcfg.save();
				return;
		}
	}
}

function runXtrn(xtrn)
{
	if(xtrn_area.prog[xtrn]==undefined)
		throw("Unknown external: "+xtrn);
	if(!xtrn_area.prog[xtrn].can_run)
		throw("User "+user.name+" is not allowed to run "+xtrn);

	var now=new Date();
	var dcfg=new DoorConfig(true);

	dcfg.door[xtrn].lastRan=now;
	dcfg.door[xtrn].runCount++;
	dcfg.save();

	var ucfg=new UserConfig(user.number, true);
	if(ucfg.door[xtrn] == undefined) {
		if(ucfg.global == undefined || (!ucfg.global.noAutoScan)) {
			ucfg.addxtrn(xtrn);
		}
	}

	if(ucfg.door[xtrn] != undefined)
		ucfg.door[xtrn].lastRan=now;

	ucfg.save();

	if(!(dcfg.door[xtrn]!=undefined && dcfg.door[xtrn].skip != undefined && dcfg.door[xtrn].skip)
			|| !(dcfg.skipSection[xtrn_area.prog[xtrn].sec_code]!=undefined && dcfg.skipSection[xtrn_area.prog[xtrn].sec_code])) {
		bbs.log_str("DOORSCAN - "+xtrn+" starting @ "+now.toString()+"\r\n");
	}

	bbs.exec_xtrn(xtrn);

	now=new Date();
	dcfg=new DoorConfig(true);

	if(!(dcfg.door[xtrn]!=undefined && dcfg.door[xtrn].skip != undefined && dcfg.door[xtrn].skip)
			&& !(dcfg.skipSection[xtrn_area.prog[xtrn].sec_code]!=undefined && dcfg.skipSection[xtrn_area.prog[xtrn].sec_code])) {
		bbs.log_str("DOORSCAN - "+xtrn+" ending @ "+now.toString()+"\r\n");
	}

	dcfg.door[xtrn].lastExit=now;
	dcfg.save();

	ucfg=new UserConfig(user.number, true);
	if(ucfg.door[xtrn] != undefined) {
		ucfg.door[xtrn].lastExit=now;
		ucfg.door[xtrn].lastRunCount=dcfg.door[xtrn].runCount;
	}

	ucfg.save();
}

function doScan()
{
	var dcfg=new DoorConfig();
	var ucfg=new UserConfig(user.number);
	var dsp=new Display();
	var door;
	var scantime;
	var tmp;
	var logdetails=new LogParser();

	/* First, look for new doors */
	for(door in dcfg.door) {
		if(xtrn_area.prog[door]==undefined) {
			log("Unknown door! "+door);
			continue;
		}
		if(dcfg.skipSection[xtrn_area.prog[door].sec_code]!=undefined && dcfg.skipSection[xtrn_area.prog[door].sec_code])
			continue;
		if(dcfg.door[door].skip != undefined && dcfg.door[door].skip)
			continue;
		if(dcfg.door[door].installed > ucfg.global.lastScan) {
			/* This door is NEW! */
			
			/* If the user can't run it, don't display it. */
			if(!xtrn_area.prog[door].can_run)
				continue;

			tmp=false;
			if(dcfg.door[door].ad != undefined) {
				/* Assume ANSI */
				if(dcfg.door[door].adType==undefined)
					tmp=dsp.ANSI(dcfg.door[door].ad);
				else {
					if(dsp[dcfg.door[door].adType] == undefined)
						log("doorscan WARNING ad type "+dcfg.door[door].adType+" for door "+door+" does not have a display method.");
					else
						tmp=dsp[dcfg.door[door].adType](dcfg.door[door].ad);
				}
			}
			if(tmp==false) {
				/* No ad... just display a message... */
				console.attributes=YELLOW;
				console.writeln("New external: "+xtrn_area.prog[door].name+" in the "+xtrn_area.sec[xtrn_area.prog[door].sec_code].name+" section.");
				console.crlf();
			}
			if(ucfg.door[door]==undefined && ucfg.global.addNew) {
				ucfg=new UserConfig(user.number, true);
				ucfg.addxtrn(door);
				ucfg.save();
			}
		}
	}

	/*
	 * Next, look for doors which have been played since either
	 * your last scan or the last time you left the game, whichever is
	 * newer
	 */
	for(door in ucfg.door) {
		if(xtrn_area.prog[door]==undefined)
			continue;
		if(dcfg.skipSection[xtrn_area.prog[door].sec_code]!=undefined && dcfg.skipSection[xtrn_area.prog[door].sec_code])
			continue;
		if(dcfg.door[door].skip != undefined && dcfg.door[door].skip)
			continue;
		var lastPlayed=false;
		scantime=ucfg.global.lastScan;
		if(ucfg.door[door].lastExit != undefined && ucfg.door[door].lastExit > scantime) {
			lastPlayed=true;
			scantime=ucfg.door[door].lastExit;
		}
		if(dcfg.door[door].lastRan != undefined && dcfg.door[door].lastRan > scantime) {
			/* Yes, this has been played... */

			/* News File */
			if(!ucfg.door[door].skipNews) {
				if(dcfg.door[door].news != undefined) {
					/*
					 * If the news file has not been updated, don't bother
					 * Some doors only update the news during maintenance
					 */
					
					if(new Date(file_date(dcfg.door[door].news)*1000) >= scantime) {
						/* Assume ANSI */
						if(dcfg.door[door].newsType==undefined)
							dsp.ANSI(dcfg.door[door].news);
						else {
							if(dsp[dcfg.door[door].newsType] == undefined)
								log("doorscan WARNING News type "+dcfg.door[door].newsType+" for door "+door+" does not have a display method.");
							else
								dsp[dcfg.door[door].newsType](dcfg.door[door].news);
						}
					}
				}
			}

			if(!ucfg.door[door].skipScores) {
				if(dcfg.door[door].score != undefined) {
					/*
					 * If the Scores file has not been updated, don't bother
					 * Some doors only update the Scores during maintenance
					 */
					
					if(new Date(file_date(dcfg.door[door].score)*1000) >= scantime) {
						/* Assume ANSI */
						if(dcfg.door[door].scoreType==undefined)
							dsp.ANSI(dcfg.door[door].score);
						else {
							if(dsp[dcfg.door[door].scoreType] == undefined)
								log("doorscan WARNING Scores type "+dcfg.door[door].scoreType+" for door "+door+" does not have a display method.");
							else
								dsp[dcfg.door[door].scoreType](dcfg.door[door].score);
						}
					}
				}
			}

			if(!ucfg.door[door].skipRunCount) {
				if(ucfg.door[door].lastRunCount != undefined) {
					var rc=logdetails.usersOfSince(door, scantime);
					console.attributes=LIGHTCYAN;
					console.writeln(xtrn_area.prog[door].name+" in the "+xtrn_area.sec[xtrn_area.prog[door].sec_code].name+" section has been ran "+rc.total+" times for "+system.secondstr(rc.total_duration)+" by "+rc.users+" users since you last "+(lastPlayed?"played":"scanned")+"\r\n");
					tmp=false;
					var str;
					var str1;
					for(var i in rc.user) {
						var col=0;
						str1=format("%s (%u for %s)", i, rc.user[i].count, system.secondstr(rc.user[i].duration));
						if(!tmp) {
							str='';
							tmp=true;
						}
						else {
							str=', ';
						}
						if(col + str.length+str1.length > 77) {
							str += '\r\n';
							console.writeln(str);
							str='';
							col=0;
						}
						str += str1;
						console.write(str);
					}
				}
			}
		}
	}
	ucfg=new UserConfig(user.number, true);
	ucfg.global.lastScan=new Date();
	ucfg.save();
}

for(i in argv) {
	switch(argv[i].toLowerCase()) {
		case 'scan':
			doScan();
			break;
		case 'run':
			if(i+1<argc)
				runXtrn(argv[++i].toLowerCase());
			else
				throw("XTRN code not included on command-line!");
			break;
		case 'config':
			new UserConfig(user.number).configure();
			break;
		case 'sysconfig':
			sysop_config();
			break;
		case 'rank':
			// TODO: Door popularity rankings
			break;
	}
}
