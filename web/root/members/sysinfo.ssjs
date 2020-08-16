/* $Id: sysinfo.ssjs,v 1.16 2006/02/25 21:40:35 runemaster Exp $ */

load("../web/lib/template.ssjs");

var sub="";

template.title= system.name+ " - System Information";

if(do_extra)
	do_rightnav = false;

template.sysinfo="";
f=new File(system.text_dir+"system.msg");
if(f.open("rb",true)) {
	template.sysinfo=f.read();
	template.sysinfo=template.sysinfo.replace(/@([^@]*)@/g,
		function(matched, code) {
			var fmt="%s";
			ma=new Array();
			if((ma=code.match(/^(.*)-L.*$/))!=undefined) {
				fmt="%-"+(code.length)+"s";
				code=ma[1];
			}
			if((ma=code.match(/^(.*)-R.*$/))!=undefined) {
				fmt="%"+(code.length)+"s";
				code=ma[1];
			}
			switch(code.toUpperCase()) {
				case 'BBS':
					return(format(fmt,system.name.toString()));
				case 'LOCATION':
					return(format(fmt,system.location.toString()));
				case 'SYSOP':
					return(format(fmt,system.operator.toString()));
				case 'HOSTNAME':
					return(format(fmt,system.host_name.toString()));
				case 'OS_VER':
					return(format(fmt,system.os_version.toString()));
				case 'UPTIME':
					var days=0;
					var hours=0;
					var min=0;
					var seconds=0;
					var ut=time()-system.uptime;
					days=(ut/86400);
					ut%=86400;
					hours=(ut/3600);
					ut%=3600;
					mins=(ut/60);
					secsonds=parseInt(ut%60);
					if(parseInt(days)!=0)
						ut=format("%d days %d:%02d",days,hours,mins);
					else
						ut=format("%d:%02d",hours,mins);
					return(format(fmt,ut.toString()));
				case 'TUSER':
					return(format(fmt,system.stats.total_users.toString()));
				case 'STATS.NUSERS':
					return(format(fmt,system.stats.new_users_today.toString()));
				case 'STATS.LOGONS':
					return(format(fmt,system.stats.total_logons.toString()));
				case 'STATS.LTODAY':
					return(format(fmt,system.stats.logons_today.toString()));
				case 'STATS.TIMEON':
					return(format(fmt,system.stats.total_timeon.toString()));
				case 'STATS.TTODAY':
					return(format(fmt,system.stats.timeon_today.toString()));
				case 'TMSG':
					return(format(fmt,system.stats.total_messages.toString()));
				case 'STATS.PTODAY':
					return(format(fmt,system.stats.messages_posted_today.toString()));
				case 'MAILW:0':
					return(format(fmt,system.stats.total_email.toString()));
				case 'STATS.ETODAY':
					return(format(fmt,system.stats.email_sent_today.toString()));
				case 'MAILW:1':
					return(format(fmt,system.stats.total_feedback.toString()));
				case 'STATS.FTODAY':
					return(format(fmt,system.stats.feedback_sent_today.toString()));
				case 'TFILE':
					return(format(fmt,system.stats.total_files.toString()));
				case 'STATS.ULS':
					return(format(fmt,system.stats.files_uploaded_today.toString()));
				case 'STATS.DLS':
					return(format(fmt,system.stats.files_downloaded_today.toString()));
				case 'STATS.DLB':
					return(format(fmt,system.stats.bytes_downloaded_today.toString()));
				default:
					return('@'+code+'@');
			}
		});
	template.sysinfo=lfexpand(template.sysinfo);
	template.sysinfo=html_encode(template.sysinfo,true,false,true,true);
}

template.logon="";
f=new File(system.text_dir+"menu/logon.ans");
if(f.open("rb",true))
	template.logon=f.read();
else {
	f=new File(system.text_dir+"menu/logon.asc");
	if(f.open("rb",true))
		template.logon=f.read();
}
template.logon=lfexpand(template.logon);
template.logon=html_encode(template.logon,true,false,true,true);

template.fidoaddrs=new Array;
for(addr in system.fido_addr_list) {
	template.fidoaddrs[addr]=new Object;
	template.fidoaddrs[addr].address=system.fido_addr_list[addr];
}

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("sysinfo.inc");
if(do_footer)
	write_template("footer.inc");
