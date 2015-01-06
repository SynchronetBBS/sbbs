if(!js.global || js.global.HTTPRequest==undefined)
	js.global.load("http.js");
if(!js.global || js.global.USCallsign==undefined)
	js.global.load("callsign.js");

// Comment these...
//Bot_Commands={};
//function Bot_Command(x,y,z){}
//js.global.config_filename='hambot.ini';

var last_update=0;
var update_interval=2;
var last_wflength=-1;
var last_solar_update=0;
var solar_x;
var contests={};
var ctydat={};
var last_contest_update=0;
var rig_index={};
var last_rig_index=0;

function main(srv,target)
{
	if((time() - last_update) < update_interval) return;

	var config = new File(system.ctrl_dir + js.global.config_filename);
	if(!config.open('r')) {
		log("Unable to open config!");
		return;
	}
	CallSign.Lookup.MagicQRZuri=config.iniGetValue("module_Ham", 'MagicQRZuri');
	var wfname=config.iniGetValue("module_Ham", 'watchfile');
	config.close();
	if(wfname != undefined) {
		var wf=new File(wfname);
		var len=wf.length;
		// Ignore contents at start up.
		if(last_wflength==-1)
			last_wflength=len;
		// If the file is truncated, start over
		if(last_wflength > len)
			last_wflength=0;
		if(len > last_wflength) {
			if(wf.open("r")) {
				var l;
				wf.position=last_wflength;
				while(l=wf.readln()) {
					srv.o(target, l);
				}
				last_wflength=wf.position;
				wf.close();
			}
		}
	}
	last_update=time();
}

function get_mfg_uris(uri)
{
	var req=new HTTPRequest();
	var list=req.Get(uri);
	var base = uri;
	base = base.replace(/[^\/]*$/,'');
	list=list.replace(/^[\x00-\xff]*?<\/table>/i,'');
	var URLexp = /a href="([^"]*)"[^>]*>([^<]*)<\/a>/ig;
	var m;
	var uri;

	while((m=URLexp.exec(list)) !== null) {
		uri = m[1];
		if(uri.substr(0,1) != '/' && uri.substr(0,7) != 'http://')
			uri = base+m[1];
		rig_index[m[2].toUpperCase()]={'uri':uri,name:m[2]};
	}
}

function update_rig_index()
{
	// Only parse indexes once per week...
	if((time() - last_rig_index) < 7*24*60*60) return;
	last_rig_index=time();
	log("Updating rig index");
	var req=new HTTPRequest();
	var root=req.Get('http://www.rigpix.com/');
	var base = 'http://www.rigpix.com/';
	root = root.replace(/^[\x00-\xff]*?font size="1">([\x00-\xff]*)<br><br>[\x00-\xff]*$/,"$1");
	var URLexp = /href="([^"]*)"/gi;
	var m;

	while((m=URLexp.exec(root)) !== null) {
		uri = m[1];
		if(uri.substr(0,1) != '/' && uri.substr(0,7) != 'http://')
			uri = base+m[1];
		if(uri == 'http://www.rigpix.com/iss/iss.htm')
			return;
		if(uri.search(/\/\/.*\/.*\//)==-1)
			continue;
		log("Getting URIs from "+uri);
		get_mfg_uris(uri);
	}
}

function update_rig_specs(rig)
{
	var req=new HTTPRequest();
	var specs=req.Get(rig_index[rig].uri);
	var base = rig_index[rig].uri;
	base = base.replace(/[^\/]*$/,'');
	specs=specs.replace(/^[\x00-\xff]*?SPECIFICATIONS/i,'');
	var t=specs.match(/<table[^>]*>([\x00-\xff]*)<\/table>/i);
	if(t==null) {
		log("Specification table missing! "+specs);
		return;
	}
	var table = t[1];
	if(table==null)
		return;
	var row=/<tr[^>]*>([\x00-\xff]*?)<\/tr>/ig;
	var m;
	var s;
	var i;
	rig_index[rig].specs = [];

	while((m=row.exec(table)) !== null) {
		var r=m[1]
		r=r.match(/<td[^>]*>([\x00-\xff]*?)<\/td>(?:[^<])*<td[^>]*>([\x00-\xff]*?)<\/td>/i);
		if(r != null) {
			if(r[2]=='')
				continue;
			if(r[1].indexOf('Options')!=-1)
				continue;
			if(r[1].indexOf('Accessories')!=-1)
				continue;
			s = r[1]+' '+r[2];
			s = s.replace(/\s*<br>\s*/ig,'<br>    ');
			s = s.replace(/<a\s+href="([^"]*)"[^>]*>([^<]*)<\/a>/i,function(m, uri, desc) {
				if(uri.substr(0,1) != '/' && uri.substr(0,7) != 'http://')
					uri = base+uri;
				return(desc+' '+uri);
			});
			s = s.split(/<br>/i);
			for(i=0; i<s.length; i++) {
				s[i]=s[i].replace(/  <STRONG>/ig,'');
				s[i]=s[i].replace(/<[^>]*>/g,'');
				s[i]=s[i].replace(/[\x00-\x1F]/g,'');
				if(s[i].search(/^\s*$/) == -1)
					rig_index[rig].specs.push(s[i]);
			}
		}
	}
}

Bot_Commands["SPECS"] = new Bot_Command(0, false, false);
Bot_Commands["SPECS"].usage = get_cmd_prefix() + "SPECS <model>";
Bot_Commands["SPECS"].help = "Fetches the specs for the <model> rig from RigPix.com and sends them as a NOTICE";
Bot_Commands["SPECS"].command = function (target, onick, ouh, srv, lbl, cmd) {
	var i;

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length >= 2) {
		var rigname = cmd.slice(1).join(' ');
		var rig = rigname.toUpperCase();
		if(rig_index[rig] == undefined) {
			update_rig_index();
			if(rig_index[rig] == undefined) {
				srv.o(onick, "Unable to locate rig "+rigname, "NOTICE");
				var suggestions=[];
				for(i in rig_index) {
					if(rig_index[i].name == undefined)
						continue;
					if(i.toUpperCase().indexOf(rig.toUpperCase())!=-1)
						suggestions.push(rig_index[i].name);
				}
				if(suggestions.length == 0)
					return true;
				if(suggestions.length > 1) {
					srv.o(onick,"Suggestions: "+suggestions.join(', '),"NOTICE");
					return true;
				}
				if(rig_index[suggestions[0].toUpperCase()] == undefined)
					return true;
				rig = suggestions[0].toUpperCase();
				rigname = suggestions[0];
				srv.o(onick,"Showing specs for "+rigname,"NOTICE");
			}
		}
		if(rig_index[rig].specs == undefined) {
			update_rig_specs(rig);
			if(rig_index[rig].specs == undefined) {
				srv.o(onick, "Unable to locate specs for rig "+rigname,"NOTICE");
				return true;
			}
		}
		for(i in rig_index[rig].specs)
			srv.o(onick, rig_index[rig].specs[i],"NOTICE");
		srv.o(onick, "Provided by rigpix.com","NOTICE");
	}

	return true;
}

TimeZoneConversion = {
	zones:{
		ACDT: [{name:"Australian Central Daylight Savings Time", offset:+10*60+30}],
		ACST: [{name:"Australian Central Standard Time", offset:+9*60+30}],
		ACT: [{name:"ASEAN Common Time", offset:+8*60}],
		ADT: [{name:"Atlantic Daylight Time", offset:-3*60}],
		AEDT: [{name:"Australian Eastern Daylight Savings Time", offset:+11*60}],
		AEST: [{name:"Australian Eastern Standard Time", offset:+10*60}],
		AFT: [{name:"Afghanistan Time", offset:+4*60+30}],
		AKDT: [{name:"Alaska Daylight Time", offset:-8*60}],
		AKST: [{name:"Alaska Standard Time", offset:-9*60}],
		AMST: [{name:"Amazon Summer Time (Brazil)", offset:-3*60},
			{name:"Armenia Summer Time", offset:+5*60}],
		AMT: [{name:"Amazon Time (Brazil)", offset:-4*60},
			{name:"Armenia Time", offset:+4*60}],
		ART: [{name:"Argentina Time", offset:-3*60}],
		AST: [{name:"Arabia Standard Time", offset:+3*60},
			{name:"Atlantic Standard Time", offset:-4*60}],
		AWDT: [{name:"Australian Western Daylight Time", offset:+9*60}],
		AWST: [{name:"Australian Western Standard Time", offset:+8*60}],
		AZOST: [{name:"Azores Standard Time", offset:-1*60}],
		AZT: [{name:"Azerbaijan Time", offset:+4*60}],
		BDT: [{name:"Brunei Time", offset:+8*60}],
		BIOT: [{name:"British Indian Ocean Time", offset:+6*60}],
		BIT: [{name:"Baker Island Time", offset:-12*60}],
		BOT: [{name:"Bolivia Time", offset:-4*60}],
		BRT: [{name:"Brasilia Time", offset:-3*60}],
		BST: [{name:"Bangladesh Standard Time", offset:+6*60},
			{name:"British Summer Time (British Standard Time from Feb 1968 to Oct 1971)", offset:+1*60}],
		BTT: [{name:"Bhutan Time", offset:+6*60}],
		CAT: [{name:"Central Africa Time", offset:+2*60}],
		CCT: [{name:"Cocos Islands Time", offset:+6*60+30}],
		CDT: [{name:"Central Daylight Time (North America)", offset:-5*60},
			{name:"Cuba Daylight Time", offset:-4*60}],
		CEDT: [{name:"Central European Daylight Time", offset:+2*60}],
		CEST: [{name:"Central European Summer Time (Cf. HAEC)", offset:+2*60}],
		CET: [{name:"Central European Time", offset:+1*60}],
		CHADT: [{name:"Chatham Daylight Time", offset:+13*60+45}],
		CHAST: [{name:"Chatham Standard Time", offset:+12*60+45}],
		CHOT: [{name:"Choibalsan", offset:+8*60}],
		CHST: [{name:"Chamorro Standard Time", offset:+10*60}],
		CHUT: [{name:"Chuuk Time", offset:+10*60}],
		CIST: [{name:"Clipperton Island Standard Time", offset:-8*60}],
		CIT: [{name:"Central Indonesia Time", offset:+8*60}],
		CKT: [{name:"Cook Island Time", offset:-10*60}],
		CLST: [{name:"Chile Summer Time", offset:-3*60}],
		CLT: [{name:"Chile Standard Time", offset:-4*60}],
		COST: [{name:"Colombia Summer Time", offset:-4*60}],
		COT: [{name:"Colombia Time", offset:-5*60}],
		CST: [{name:"Central Standard Time (North America)", offset:-6*60},
			{name:"China Standard Time", offset:+8*60},
			{name:"Cuba Standard Time", offset:-5*60}],
		CT: [{name:"China time", offset:+8*60}],
		CVT: [{name:"Cape Verde Time", offset:-1*60}],
		CWST: [{name:"Central Western Standard Time (Australia) unofficial", offset:+8*60+45}],
		CXT: [{name:"Christmas Island Time", offset:+7*60}],
		DAVT: [{name:"Davis Time", offset:+7*60}],
		DDUT: [{name:"Dumont d'Urville Time", offset:+10*60}],
		DFT: [{name:"AIX specific equivalent of Central European Time", offset:+1*60}],
		EASST: [{name:"Easter Island Standard Summer Time", offset:-5*60}],
		EAST: [{name:"Easter Island Standard Time", offset:-6*60}],
		EAT: [{name:"East Africa Time", offset:+3*60}],
		ECT: [{name:"Eastern Caribbean Time (does not recognise DST)", offset:-4*60},
			{name:"Ecuador Time", offset:-5*60}],
		EDT: [{name:"Eastern Daylight Time (North America)", offset:-4*60}],
		EEDT: [{name:"Eastern European Daylight Time", offset:+3*60}],
		EEST: [{name:"Eastern European Summer Time", offset:+3*60}],
		EET: [{name:"Eastern European Time", offset:+2*60}],
		EGST: [{name:"Eastern Greenland Summer Time", offset:+0*60}],
		EGT: [{name:"Eastern Greenland Time", offset:-1*60}],
		EIT: [{name:"Eastern Indonesian Time", offset:+9*60}],
		EST: [{name:"Eastern Standard Time (North America)", offset:-5*60}],
		FET: [{name:"Further-eastern European Time", offset:+3*60}],
		FJT: [{name:"Fiji Time", offset:+12*60}],
		FKST: [{name:"Falkland Islands Standard Time", offset:-3*60},
			{name:"Falkland Islands Summer Time", offset:-3*60}],
		FKT: [{name:"Falkland Islands Time", offset:-4*60}],
		FNT: [{name:"Fernando de Noronha Time", offset:-2*60}],
		GALT: [{name:"Galapagos Time", offset:-6*60}],
		GAMT: [{name:"Gambier Islands", offset:-9*60}],
		GET: [{name:"Georgia Standard Time", offset:+4*60}],
		GFT: [{name:"French Guiana Time", offset:-3*60}],
		GILT: [{name:"Gilbert Island Time", offset:+12*60}],
		GIT: [{name:"Gambier Island Time", offset:-9*60}],
		GMT: [{name:"Greenwich Mean Time", offset:0}],
		GST: [{name:"South Georgia and the South Sandwich Islands", offset:-2*60},
			{name:"Gulf Standard Time", offset:+4*60}],
		GYT: [{name:"Guyana Time", offset:-4*60}],
		HADT: [{name:"Hawaii-Aleutian Daylight Time", offset:-9*60}],
		HAEC: [{name:"Heure Avancée d'Europe Centrale francised name for CEST", offset:+2*60}],
		HAST: [{name:"Hawaii-Aleutian Standard Time", offset:-10*60}],
		HKT: [{name:"Hong Kong Time", offset:+8*60}],
		HMT: [{name:"Heard and McDonald Islands Time", offset:+5*60}],
		HOVT: [{name:"Khovd Time", offset:+7*60}],
		HST: [{name:"Hawaii Standard Time", offset:-10*60}],
		ICT: [{name:"Indochina Time", offset:+7*60}],
		IDT: [{name:"Israel Daylight Time", offset:+3*60}],
		IOT: [{name:"Indian Ocean Time", offset:+3*60}],
		IRDT: [{name:"Iran Daylight Time", offset:+4*60+30}],
		IRKT: [{name:"Irkutsk Time", offset:+9*60}],
		IRST: [{name:"Iran Standard Time", offset:+3*60+30}],
		IST: [{name:"Indian Standard Time", offset:+5*60+30},
			{name:"Irish Standard Time", offset:+1*60},
			{name:"Israel Standard Time", offset:+2*60}],
		JST: [{name:"Japan Standard Time", offset:+9*60}],
		KGT: [{name:"Kyrgyzstan time", offset:+6*60}],
		KOST: [{name:"Kosrae Time", offset:+11*60}],
		KRAT: [{name:"Krasnoyarsk Time", offset:+7*60}],
		KST: [{name:"Korea Standard Time", offset:+9*60}],
		LHST: [{name:"Lord Howe Standard Time", offset:+10*60+30},
			{name:"Lord Howe Summer Time", offset:+11*60}],
		LINT: [{name:"Line Islands Time", offset:+14*60}],
		MAGT: [{name:"Magadan Time", offset:+12*60}],
		MART: [{name:"Marquesas Islands Time", offset:-9*60-30}],
		MAWT: [{name:"Mawson Station Time", offset:+5*60}],
		MDT: [{name:"Mountain Daylight Time (North America)", offset:-6*60}],
		MET: [{name:"Middle European Time Same zone as CET", offset:+1*60}],
		MEST: [{name:"Middle European Saving Time Same zone as CEST", offset:+2*60}],
		MHT: [{name:"Marshall Islands", offset:+12*60}],
		MIST: [{name:"Macquarie Island Station Time", offset:+11*60}],
		MIT: [{name:"Marquesas Islands Time", offset:-9*60-30}],
		MMT: [{name:"Myanmar Time", offset:+6*60+30}],
		MSK: [{name:"Moscow Time", offset:+4*60}],
		MST: [{name:"Malaysia Standard Time", offset:+8*60},
			{name:"Mountain Standard Time (North America)", offset:-7*60},
			{name:"Myanmar Standard Time", offset:+6*60+30}],
		MUT: [{name:"Mauritius Time", offset:+4*60}],
		MVT: [{name:"Maldives Time", offset:+5*60}],
		MYT: [{name:"Malaysia Time", offset:+8*60}],
		NCT: [{name:"New Caledonia Time", offset:+11*60}],
		NDT: [{name:"Newfoundland Daylight Time", offset:-2*60-30}],
		NFT: [{name:"Norfolk Time", offset:+11*60+30}],
		NPT: [{name:"Nepal Time", offset:+5*60+45}],
		NST: [{name:"Newfoundland Standard Time", offset:-3*60-30}],
		NT: [{name:"Newfoundland Time", offset:-3*60-30}],
		NUT: [{name:"Niue Time", offset:-11*60}],
		NZDT: [{name:"New Zealand Daylight Time", offset:+13*60}],
		NZST: [{name:"New Zealand Standard Time", offset:+12*60}],
		OMST: [{name:"Omsk Time", offset:+7*60}],
		ORAT: [{name:"Oral Time", offset:+5*60}],
		PDT: [{name:"Pacific Daylight Time (North America)", offset:-7*60}],
		PET: [{name:"Peru Time", offset:-5*60}],
		PETT: [{name:"Kamchatka Time", offset:+12*60}],
		PGT: [{name:"Papua New Guinea Time", offset:+10*60}],
		PHOT: [{name:"Phoenix Island Time", offset:+13*60}],
		PKT: [{name:"Pakistan Standard Time", offset:+5*60}],
		PMDT: [{name:"Saint Pierre and Miquelon Daylight time", offset:-2*60}],
		PMST: [{name:"Saint Pierre and Miquelon Standard Time", offset:-3*60}],
		PONT: [{name:"Pohnpei Standard Time", offset:+11*60}],
		PST: [{name:"Philippine Standard Time", offset:+8*60},
			{name:"Pacific Standard Time (North America)", offset:-8*60}],
		PYST: [{name:"Paraguay Summer Time (South America)", offset:-3*60}],
		PYT: [{name:"Paraguay Time (South America)", offset:-4*60}],
		RET: [{name:"Réunion Time", offset:+4*60}],
		ROTT: [{name:"Rothera Research Station Time", offset:-3*60}],
		SAKT: [{name:"Sakhalin Island time", offset:+11*60}],
		SAMT: [{name:"Samara Time", offset:+4*60}],
		SAST: [{name:"South African Standard Time", offset:+2*60}],
		SBT: [{name:"Solomon Islands Time", offset:+11*60}],
		SCT: [{name:"Seychelles Time", offset:+4*60}],
		SGT: [{name:"Singapore Time", offset:+8*60}],
		SLST: [{name:"Sri Lanka Time", offset:+5*60+30}],
		SRT: [{name:"Suriname Time", offset:-3*60}],
		SST: [{name:"Samoa Standard Time", offset:-11*60},
			{name:"Singapore Standard Time", offset:+8*60}],
		SYOT: [{name:"Showa Station Time", offset:+3*60}],
		TAHT: [{name:"Tahiti Time", offset:-10*60}],
		THA: [{name:"Thailand Standard Time", offset:+7*60}],
		TFT: [{name:"Indian/Kerguelen", offset:+5*60}],
		TJT: [{name:"Tajikistan Time", offset:+5*60}],
		TKT: [{name:"Tokelau Time", offset:+13*60}],
		TLT: [{name:"Timor Leste Time", offset:+9*60}],
		TMT: [{name:"Turkmenistan Time", offset:+5*60}],
		TOT: [{name:"Tonga Time", offset:+13*60}],
		TVT: [{name:"Tuvalu Time", offset:+12*60}],
		UCT: [{name:"Coordinated Universal Time", offset:0}],
		ULAT: [{name:"Ulaanbaatar Time", offset:+8*60}],
		UTC: [{name:"Coordinated Universal Time", offset:0}],
		UYST: [{name:"Uruguay Summer Time", offset:-2*60}],
		UYT: [{name:"Uruguay Standard Time", offset:-3*60}],
		UZT: [{name:"Uzbekistan Time", offset:+5*60}],
		VET: [{name:"Venezuelan Standard Time", offset:-4*60-30}],
		VLAT: [{name:"Vladivostok Time", offset:+10*60}],
		VOLT: [{name:"Volgograd Time", offset:+4*60}],
		VOST: [{name:"Vostok Station Time", offset:+6*60}],
		VUT: [{name:"Vanuatu Time", offset:+11*60}],
		WAKT: [{name:"Wake Island Time", offset:+12*60}],
		WAST: [{name:"West Africa Summer Time", offset:+2*60}],
		WAT: [{name:"West Africa Time", offset:+1*60}],
		WEDT: [{name:"Western European Daylight Time", offset:+1*60}],
		WEST: [{name:"Western European Summer Time", offset:+1*60}],
		WET: [{name:"Western European Time", offset:0}],
		WIT: [{name:"Western Indonesian Time", offset:+7*60}],
		WST: [{name:"Western Standard Time", offset:+8*60}],
		YAKT: [{name:"Yakutsk Time", offset:+10*60}],
		YEKT: [{name:"Yekaterinburg Time", offset:+6*60}],
		Z: [{name:"Zulu Time (Coordinated Universal Time)", offset:0}]
	}
};

for (var ham_ircbot_zone in TimeZoneConversion.zones) {
	Bot_Commands[ham_ircbot_zone] = new Bot_Command(0, false, false);
	Bot_Commands[ham_ircbot_zone].usage = get_cmd_prefix() + ham_ircbot_zone;
	Bot_Commands[ham_ircbot_zone].help = "Displays the current ";
	for (var onezone = 0; onezone < TimeZoneConversion.zones[ham_ircbot_zone].length; onezone++) {
		if (TimeZoneConversion.zones[ham_ircbot_zone].length > 1 && (onezone + 1 == TimeZoneConversion.zones[ham_ircbot_zone].length))
			Bot_Commands[ham_ircbot_zone].help += ', and ';
		else if(TimeZoneConversion.zones[ham_ircbot_zone].length > 1 && onezone > 0)
			Bot_Commands[ham_ircbot_zone].help += ', ';
		Bot_Commands[ham_ircbot_zone].help += TimeZoneConversion.zones[ham_ircbot_zone][onezone].name;
	}
	if (ham_ircbot_zone != 'Z')
		Bot_Commands[ham_ircbot_zone].no_help = true;
	Bot_Commands[ham_ircbot_zone].command = eval("function (target, onick, ouh, srv, lbl, cmd) {\n"+
	"var i;\n"+
	"var d;\n"+
	"// Remove empty cmd args\n"+
	"for(i=1; i<cmd.length; i++) {\n"+
	"	if(cmd[i].search(/^\s*$/)==0) {\n"+
	"		cmd.splice(i,1);\n"+
	"		i--;\n"+
	"	}\n"+
	"}\n"+
	"\n"+
	"if(cmd.length == 1) {\n"+
	"	for (i=0; i<TimeZoneConversion.zones['"+ham_ircbot_zone+"'].length; i++) {"+
	"		d=new Date(time()*1000+(TimeZoneConversion.zones['"+ham_ircbot_zone+"'][i].offset*60*1000));\n"+
	"		srv.o(target,d.toGMTString().replace(/[A-Z]+$/, '"+ham_ircbot_zone+"')+' - '+TimeZoneConversion.zones['"+ham_ircbot_zone+"'][i].name);\n"+
	"	}\n"+
	"}\n"+
	"\n"+
	"return true;\n"+
	"\n"+
	"}\n");
}

function update_contests()
{
	// Only update once per day.
	if((time() - last_contest_update) < 60*60*24) return;
	contests={};
	contest_order=[];

	function parse_contest(cont) {
		var m;
		var c;

		function parse_time(t) {
			var se = t.split(/ to /);
			var ret={};

			if(se.length==1) {
				se=t.match(/([0-9]{4})([^-]*?)-([0-9]{4})([^-]*?), ([A-Za-z]{3}) ([0-9]+)/)
				if(se == null) {
					log("Error parsing range "+t+"!");
					return;
				}
				ret.startTime=se[1];
				ret.zone=se[2].replace(/^\s*(.*?)\s*$/, "$1");
				ret.endTime=se[3];
				ret.startMonth=se[5];
				ret.endMonth=se[5];
				ret.startDay=se[6];
				ret.endDay=se[6];
			}
			else {
				var s=se[0].match(/([0-9]{4})([^-]*?), ([A-Za-z]{3}) ([0-9]+)/);
				if(s == null) {
					log("Error parsing start "+se[0]+"!");
					return;
				}
				ret.startTime=s[1];
				ret.zone=s[2].replace(/^\s*(.*?)\s*$/, "$1");
				ret.startMonth=s[3];
				ret.startDay=s[4];
				s=se[1].match(/([0-9]{4})([^-]*?), ([A-Za-z]{3}) ([0-9]+)/);
				if(s == null) {
					log("Error parsing end "+se[1]+"!");
					return;
				}
				ret.endTime=s[1];
				ret.endMonth=s[3];
				ret.endDay=s[4];
			}
			return ret;
		}

		m=cont.match(/<tr><td colspan="3" class="bgray"><strong><a[^>]*>(.[^<]*)<\/a>[\s:]*([^<]*)<\/strong>/)
		if(m != null) {
			c=m[1];
			c += '<!-- '+(contest_order.length+1)+' -->';
			var times=m[2].split(/ and /);

			contest_order.push(c);
			contests[c]={};
			contests[c].timeStr=m[2];
			contests[c].times=[];
			for(timeidx in times) {
				contests[c].times.push(parse_time(times[timeidx]));
			}
			var re=/<tr><td [^>]*>&nbsp;<\/td><td class="blightblue twhite">(.*?):<\/td><td>(.*?)<\/td><\/tr>/g;
			var prop;
			while((prop=re.exec(cont))!=null) {
				if(prop[1]=='Bands') {
					prop[2] = prop[2].replace(/ Only$/i,'').replace(/([0-9])m$/,'$1').replace(/VHF\/UHF/,'VHF, UHF');
					contests[c][prop[1]]=prop[2].split(/,\s*/);
				}
				else if(prop[1]=='Mode') {
					contests[c][prop[1]]=prop[2].split(/,\s*/);
				}
				else if(prop[1]=='Find rules at') {
					contests[c][prop[1]]=prop[2].replace(/^.*href="(.*?)">.*$/,"$1");
				}
				else {
					contests[c][prop[1]]=prop[2].split(/<br>/);
				}
			}
		}
	}

	// Fetch current contest cal (next 8 days)
	var req=new HTTPRequest();
	var currcal=req.Get('http://www.hornucopia.com/contestcal/weeklycont.php');
	var matches=currcal.match(/<tr><td colspan="3" class="bgray"><strong>[\x00-\xff]*?<tr><td>&nbsp;<\/td><td class="blightblue">&nbsp;<\/td><td>&nbsp;<\/td><\/tr>/g);
	for(match in matches) {
		parse_contest(matches[match]);
	}
	var currcal=req.Get('http://www.hornucopia.com/contestcal/weeklycont.php?mode=custom&week=next');
	var matches=currcal.match(/<tr><td colspan="3" class="bgray"><strong>[\x00-\xff]*?<tr><td>&nbsp;<\/td><td class="blightblue">&nbsp;<\/td><td>&nbsp;<\/td><\/tr>/g);
	for(match in matches) {
		parse_contest(matches[match]);
	}
}

function update_solar()
{
	// MUST NOT request more than every hour or you will get banned!
	if((time() - last_solar_update) < 60*60) return;
	last_solar_update=time();
	var req=new HTTPRequest();
	var newxml=req.Get('http://www.hamqsl.com/solarxml.php');
	newxml=newxml.replace(/<\?[^?]*\?>/g,'');
	solar_x=new XML(newxml).solardata;
}

function trim(val)
{
	var ret=val.toString().replace(/^\s*/,"");
	ret=ret.replace(/\s*$/,"");
	return(ret);
}

Bot_Commands["GEO"] = new Bot_Command(0, false, false);
Bot_Commands["GEO"].usage = get_cmd_prefix() + "GEO";
Bot_Commands["GEO"].help = "Gives you a summary of current geomagneting and solar conditions from N0NBH - Updated every four hours";
Bot_Commands["GEO"].command = function (target, onick, ouh, srv, lbl, cmd) {
	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length == 1) {
		update_solar();
		srv.o(target,"Solar conditions from "+trim(solar_x.source)+" as of "+trim(solar_x.updated)+":");
		srv.o(target,"Sun Spots: "+trim(solar_x.sunspots)+", Solar Flux: "+trim(solar_x.solarflux)+", A-Index: "+trim(solar_x.aindex)+", K-Index: "+trim(solar_x.kindex)+"/"+trim(solar_x.kindexnt));
		srv.o(target,"X-Ray: "+trim(solar_x.xray)+", Helium Line: "+trim(solar_x.heliumline)+", Proton Flux: "+trim(solar_x.protonflux)+", Electron Flux: "+trim(solar_x.electonflux));
		srv.o(target,"Aurora Activity: "+trim(solar_x.aurora)+"/n="+trim(solar_x.normalization)+", Aurora Latitude: "+trim(solar_x.latdegree));
		srv.o(target,"Solar Wind: "+trim(solar_x.solarwind)+", Magnetic Field: "+trim(solar_x.magneticfield)+", Geomagnetic Field: "+trim(solar_x.geomagfield)+", Noise Level: "+trim(solar_x.signalnoise)+", F2 Critical Frequency: "+trim(solar_x.fof2));
	}

	return true;
}

Bot_Commands["VHF"] = new Bot_Command(0, false, false);
Bot_Commands["VHF"].usage = get_cmd_prefix() + "VHF";
Bot_Commands["VHF"].help = "Displays the current VHF propagation predictions for VHF";
Bot_Commands["VHF"].command = function (target, onick, ouh, srv, lbl, cmd) {
	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length == 1) {
		update_solar();
		srv.o(target,"VHF conditions from "+trim(solar_x.source)+" as of "+trim(solar_x.updated)+":");
		srv.o(target,"Aurora Latitude: "+trim(solar_x.latdegree)+" ("+trim(solar_x.calculatedvhfconditions.phenomenon.(@name=='vhf-aurora'))+")");
		srv.o(target,"E-Skip North America (2m): "+trim(solar_x.calculatedvhfconditions.phenomenon.(@name=='E-Skip').(@location=='north_america')));
		srv.o(target,"E-Skip Europe (2m): "+trim(solar_x.calculatedvhfconditions.phenomenon.(@name=='E-Skip').(@location=='europe')));
		srv.o(target,"E-Skip Europe (4m): "+trim(solar_x.calculatedvhfconditions.phenomenon.(@name=='E-Skip').(@location=='europe_4m')));
		srv.o(target,"E-Skip Europe (6m): "+trim(solar_x.calculatedvhfconditions.phenomenon.(@name=='E-Skip').(@location=='europe_6m')));
	}
	return true;
}

Bot_Commands["HF"] = new Bot_Command(0, false, false);
Bot_Commands["HF"].usage = get_cmd_prefix() + "HF";
Bot_Commands["HF"].help = "Displays the current HF propagation predictions";
Bot_Commands["HF"].command = function (target, onick, ouh, srv, lbl, cmd) {
	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length == 1) {
		update_solar();
		srv.o(target,"HF conditions from "+trim(solar_x.source)+" as of "+trim(solar_x.updated)+":");
		srv.o(target,"Band    | Day    | Night");
		srv.o(target,format("80m-40m | %-7.7s| %s",solar_x.calculatedconditions.band.(@name == '80m-40m').(@time == 'day'),solar_x.calculatedconditions.band.(@name == '80m-40m').(@time == 'night')));
		srv.o(target,format("30m-20m | %-7.7s| %s",solar_x.calculatedconditions.band.(@name == '30m-20m').(@time == 'day'),solar_x.calculatedconditions.band.(@name == '30m-20m').(@time == 'night')));
		srv.o(target,format("17m-15m | %-7.7s| %s",solar_x.calculatedconditions.band.(@name == '17m-15m').(@time == 'day'),solar_x.calculatedconditions.band.(@name == '17m-15m').(@time == 'night')));
		srv.o(target,format("12m-10m | %-7.7s| %s",solar_x.calculatedconditions.band.(@name == '12m-10m').(@time == 'day'),solar_x.calculatedconditions.band.(@name == '12m-10m').(@time == 'night')));
		srv.o(target,"Signal Noise Level: "+trim(solar_x.signalnoise));
	}
	return true;
}

Bot_Commands["COUNTRY"] = new Bot_Command(0,false,false);
Bot_Commands["COUNTRY"].usage = get_cmd_prefix() + "COUNTRY <call>";
Bot_Commands["COUNTRY"].help = "Displays the details for the specified call/prefix if available";
Bot_Commands["COUNTRY"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var callsign;
	var i;
	var config = new File(system.ctrl_dir + js.global.config_filename);
	if(!config.open('r')) {
		log("Unable to open config!");
		return;
	}
	var ctyfname=config.iniGetValue("module_Ham", 'ctydat');
	config.close();
	if(ctyfname!= undefined && ctyfname.length > 0) {
		// Remove empty cmd args
		for(i=1; i<cmd.length; i++) {
			if(cmd[i].search(/^\s*$/)==0) {
				cmd.splice(i,1);
				i--;
			}
		}

		if(cmd.length==2)
			callsign=cmd[1].toUpperCase();
		else {
			return true;
		}

		var ret=CallSign.CTYDAT(callsign, ctydat, ctyfname);
		if(ret == undefined) {
			srv.o(target, "Unable to match prefix!");
		}
		else {
			srv.o(target, 'Prefix "'+ret.matched+'" is from '+ret.name+' ('+ret.continent+') ITU:'+ret.itu+' CQ:'+ret.cq+(ret.WAEDC?' DARC WAEDC list only':''));
		}
	}
}

Bot_Commands["CALLSIGN"] = new Bot_Command(0,false,false);
Bot_Commands["CALLSIGN"].usage = get_cmd_prefix() + "CALLSIGN <call>";
Bot_Commands["CALLSIGN"].help = "Displays the details for the specified call if available";
Bot_Commands["CALLSIGN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var callsign;
	var i;

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length==2)
		callsign=cmd[1].toUpperCase();
	else {
		return true;
	}

	try {
		var matched=CallSign.Lookup.Any(callsign);
		if(matched.string != undefined)
			srv.o(target, matched.string);
		else {
			var str=matched.callsign+": ";
			if(matched.name != undefined)
				str += matched.name;
			if(matched.address != undefined)
				str += ", "+matched.address;
			if(matched.city != undefined)
				str += ", "+matched.city;
			if(matched.provstate != undefined)
				str += ", "+matched.provstate;
			if(matched.postalzip != undefined)
				str += " "+matched.postalzip;
			if(matched.country != undefined)
				str += ", "+matched.country;
			if(matched.qualifications != undefined)
				str += ". "+matched.qualifications;
			if(matched.status != undefined)
				str += " ("+matched.status+")";
			if(matched.note != undefined)
				str += " ("+matched.note+")";
			str=str.replace(/ +([\.\,])/g,'$1');
			str=str.replace(/([,.])[,.]+/g,'$1');
			str=str.replace(/  +/g,' ');
			srv.o(target, str);
		}
	}
	catch(e) {
		srv.o(target,e);
	}
	return true;
}

Bot_Commands["CONTESTS"] = new Bot_Command(0,false,false);
Bot_Commands["CONTESTS"].usage = get_cmd_prefix() + "CONTESTS [arg1 [agr2 [...]]]";
Bot_Commands["CONTESTS"].help = "Lists the current and upcoming contests thanks to WA7BNM "
		+ "Multiple filters are supported: hf vhf uhf ssb cw phone sstv rttv psk digital data feld hell am fm l#"
Bot_Commands["CONTESTS"].command = function (target, onick, ouh, srv, lvl, cmd) {
	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	update_contests();

	function match_contests(rules)
	{
		var ret=[];
		var c;
		var rules;
		var matches;
		var invalid=false;
		var limit=100;
		var r;
		var m;

		for(rule in rules) {
			if((m=rules[rule].match(/^L([0-9]+)$/i))!=null)
				limit=m[1];
		}
		for(contest in contest_order) {
			c=contests[contest_order[contest]];
			matches=0;
			for(rule in rules) {
				r=rules[rule].toLowerCase();
				if(r.search(/^L([0-9]+)$/i)==0)
					matches++;
				else if(r == 'hf' || r.search(/^[0-9]+$/)!= -1 || r == 'vhf'
						|| r == 'uhf') {
					for(band in c.Bands) {
						var b=c.Bands[band].toLowerCase();
						var bn=parseFloat(b);

						if(b==r) {
							matches++;
							break;
						}
						else if(b=='all' || b=='any') {
							matches++;
							break;
						}
						else if(r=='hf' && bn >= 10 && bn <= 160) {
							matches++;
							break;
						}
						else if(r=='vhf' && ((bn <=6 && bn >= 1.25) || (bn >= 219 && bn <= 225))) {
							matches++;
							break;
						}
						else if(r=='uhf' && ((bn > 1.25 && bn < 10) || (bn >= 420))) {
							matches++;
							break;
						}
					}
				}
				else if(r == 'ssb' || r == 'cw' || r == 'phone' || r == 'sstv'
						|| r == 'rtty' || r == 'psk' || r == 'digital'
						|| r == 'data' || r == 'feld hell' || r == 'am' || r=='fm') {
					for(mode in c.Mode) {
						var m=c.Mode[mode].toLowerCase();

						if(m==r) {
							matches++;
							break;
						}
						else if(r=='phone' && (m=='ssb' || m=='am' || m=='fm')) {
							matches++;
							break;
						}
						else if((r=='data' || r=='digital') && (m=='cw' || m=='rtty' || m=='psk' || m == 'feld hell')) {
							matches++;
							break;
						}
					}
				}
				else {
					invalid=true;
					matches++;
				}
			}
			if(matches==rules.length) {
				ret.push(contest_order[contest]);
				if(ret.length>=limit)
					break;
			}
		}
		if(invalid)
			return undefined;
		return ret;
	}

	var cl=match_contests(cmd.slice(1));
	if(cl===undefined)
		return;
	var displayed=0;
	for(c in cl) {
		var t=contests[cl[c]];
		srv.o(onick, cl[c].replace(/<!--[^-]*-->/,'')+': '+t.timeStr+' '+t['Find rules at'], "NOTICE");
		displayed++;
//		if(displayed > 8) {
//			srv.o(onick, "--- Aborting after 8 entries", "NOTICE");
//			break;
//		}
	}
	if(displayed) {
		srv.o(onick,'Provided by WA7BNM Contest Calendar', "NOTICE");
	}
	else {
		srv.o(onick,'No contests found', "NOTICE");
	}
}

//var dumb={o:function(x,y) {log(y);}};
//Bot_Commands["GEO"].command(undefined, undefined, undefined,dumb,undefined,['GEO']);
//Bot_Commands["HF"].command(undefined, undefined, undefined,dumb,undefined,['GEO']);
//Bot_Commands["VHF"].command(undefined, undefined, undefined,dumb,undefined,['GEO']);
//Bot_Commands["CALLSIGN"].command(undefined, undefined, undefined,dumb,undefined,['asdf','n0l']);
//Bot_Commands["CALLSIGN"].command(undefined, undefined, undefined,dumb,undefined,['asdf','kj6pxy']);
//Bot_Commands["CALLSIGN"].command(undefined, undefined, undefined,dumb,undefined,['asdf','va6rrx']);
//Bot_Commands["CALLSIGN"].command(undefined, undefined, undefined,dumb,undefined,['asdf','g1xkz']);
