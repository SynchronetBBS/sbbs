// JavaScript HTML Index for Synchronet FTP Server
// $id$

var start=new Date();
var time_stamp=start.valueOf().toString(36);	// Used to defeat caching browsers

load("sbbsdefs.js");	// Synchronet constants

/* Utility Functions */

function writeln(str)
{
	write(str + "\r\n");
}

function date(time)
{
	var mon=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
	var d=new Date(time*1000);

	return(format("%s %02d %d %02d:%02d"
			,mon[d.getUTCMonth()]
			,d.getUTCDate()
			,d.getUTCFullYear()
			,d.getUTCHours()
			,d.getUTCMinutes()));
}

function kbytes(nbytes)
{
	return(Math.round(nbytes/1024)+"k");
}

function secstr(sec)
{
	return(format("%02u:%02u",sec/60,sec%60));
}

var title = system.name + " BBS - FTP Server";
var font_face = "<font face=Arial,Helvetica,sans-serif>";
var font_size = 2;	// Change base font size here

writeln('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">');
writeln("<html>");
writeln("<head>");
writeln("<title>");
writeln(title);
if (curdir.name!=undefined)
	write(" - " + curdir.name);
else if(curlib.description!=undefined)
	write(" - " + curlib.description);
writeln("</title>");

/* META TAGS */
writeln("<meta name='GENERATOR' content='" + system.version + "'>");
// The following line is necessary for IBM extended-ASCII in descriptions 
writeln("<meta http-equiv='Content-Type' content='text/html; charset=IBM437'>");

writeln("</head>");
writeln("<body bgcolor=teal text=white link=yellow vlink=lime alink=white>");
writeln(font_face);

/* Go To Select Box */
writeln("<table width=100%>");
writeln("<td>");
writeln("<h1>" + font_face + "<font color=lime>" + title.italics() + "</font></h1>");
writeln("<td align=right>");
writeln("<form>");	// Netscape requires this to be in a form <sigh>
writeln(format(
	"<select " +
	"onChange='if(selectedIndex>0) location=options[selectedIndex].value + \"%s\";'>"
	,"?$" + time_stamp));
writeln("<option>Go To...</option>");
writeln(format("<option value=/%s>Root</option>",html_index_file));
for(l in file_area.lib_list) {
	writeln(format("<optgroup label=%s>",file_area.lib_list[l].name));
	writeln(format("<option value=%s> [%s]"
		,file_area.lib_list[l].link
		,file_area.lib_list[l].name));
	for(d in file_area.lib_list[l].dir_list) {
		writeln(format("<option value=%s>&nbsp&nbsp&nbsp&nbsp %s"
			,file_area.lib_list[l].dir_list[d].link
			,file_area.lib_list[l].dir_list[d].name));
	}
	writeln("</optgroup>");
}
writeln("</select>");
writeln("</form>");
writeln("</table>");

var prevdir;

var hdr_font="<font color=silver>";
var dat_font="<font color=white>";

if(!(user.security.restrictions&UFLAG_G)) {	/* !Guest or Anonymous */
	/* Logout button */
	writeln("<table align=right>");
	writeln("<form>");
	writeln("<input type=button value=Logout onclick='location=\"ftp://" 
		+ format("%s/%s?$%s",system.inetaddr,html_index_file,time_stamp)
		+ "\";'>");
	writeln("</form>");
	writeln("</table>");

	/* User Info */
	writeln("<table nowrap align=left>");
	writeln(font_face);
	writeln("<tr><th align=right>"+hdr_font+"User:<th align=left>"+dat_font+user.alias);
	writeln("<tr><th align=right>"+hdr_font+"Address:<th align=left width=150>"+dat_font+user.ip_address);
	write("<tr><th align=right>"+hdr_font+"Credits:<th align=left>"+dat_font);
	if(user.security.exemptions&UFLAG_D)
		writeln("Exempt");
	else
		writeln(kbytes(user.security.credits+user.security.free_credits));
	write("<tr><th align=right>"+hdr_font+"Time left:<th align=left>"+dat_font);
	if(user.security.exemptions&UFLAG_T)
		writeln("Exempt");
	else
		writeln(secstr(user.time_left));
	writeln("</table>");

	/* User Stats */

	writeln("<table nowrap>");
	writeln(font_face);
	writeln("<tr><th align=right>"+hdr_font+"Logons:<th align=left>"+dat_font+user.stats.total_logons);
	writeln("<tr><th align=right>"+hdr_font+"Last on:<th align=left>"+date(user.stats.laston_date));
 	writeln("<tr><th align=right>"+hdr_font+"Uploaded:<th align=left>"+dat_font);
	writeln(format("%s bytes in %u files"
		,kbytes(user.stats.bytes_uploaded),user.stats.files_uploaded));
	writeln("<tr><th align=right>"+hdr_font+"Downloaded:<th align=left>"+dat_font);
	writeln(format("%s bytes in %u files",kbytes(user.stats.bytes_downloaded)
		,user.stats.files_downloaded));
	writeln("</table>");
	writeln("<br>");
} else if(curlib.name==undefined) {	/* Login */
	writeln("<table align=right>");
	writeln("<td><input type=button value='New User' onClick='location=\"telnet://" 
		+ system.inetaddr + "\";'>");
	writeln("</table>");

	writeln("<form name='login'>");
	writeln("<table border=1 frame=box rules=none cellpadding=3>");
	writeln(font_face);
	writeln("<tr><th valign=top align=left>"+hdr_font+"Name");
	writeln("<td colspan=2><input type=text name='username' size=25 maxlength=25>");
	writeln("<tr><th valign=top align=left>"+hdr_font+"Password");
	writeln("<td><input type=password name='password' size=10 maxlength=25>");
	writeln("<td align=right><input type=button name='LoginButton' value='Login' onClick='login_event();'>");

	/* Client-Side Script */
	writeln("<SCRIPT language='JavaScript'>");
		writeln("<!--");
		writeln("function login_event() {");
		write("var url='ftp://'"); 
		write("+ escape(document.login.username.value) + ':'");
		write("+ escape(document.login.password.value) + '@'");
		write(format("+ '%s/%s?$%s'\r\n",system.inetaddr,html_index_file,time_stamp));
//		writeln("alert(url);");
		writeln("location = url;");
		writeln("}");
		writeln("// -->");
		writeln("</script>");

	writeln("</table>");
	writeln("</form>");
}

/* Virtual Path */
writeln("<h3>" + hdr_font + "Path: ");
if(curlib.name==undefined) 
	writeln(dat_font + "Root");
else
	writeln("Root".link(format("/%s?$%s",html_index_file,time_stamp)));
if(curlib.name!=undefined) {
	if(curdir.name==undefined)
		writeln(" / " + dat_font + curlib.description);
	else
		writeln(" / " + curlib.description.link(format("/%s/%s?$%s"
			,curlib.name,html_index_file,time_stamp)));
}		
if(curdir.name!=undefined) 
	writeln(" / " + dat_font + curdir.description);
if(curdir.settings!=undefined && curdir.settings&DIR_FREE)
	write(hdr_font+" - FREE");
writeln("</h3>");


/* Table Attributes */
var hdr_background="white";
var hdr_font=format("<font size=%d color=black>",font_size-1);
var dat_font=format("<font size=%d>",font_size);
var cell_spacing=""; //"cellspacing=2 cellpadding=2";

/* Directory Listing */
if(dir_list.length) {

	writeln("<table " + cell_spacing + " width=33%>");
	writeln(font_face);

	/* header */
	writeln("<thead>");
	writeln("<tr bgcolor=" + hdr_background + ">");
	writeln("<th>" + hdr_font + "Directory");
	if(curlib.name!=undefined) 
		writeln("<th>" + hdr_font + "Files");
	writeln("</thead>");

	/* body */
	writeln("<tbody>");
	for(i in dir_list) {
		writeln("<tr>");
	
		/* filename */
		writeln("<th nowrap align=left>" + dat_font 
			+ dir_list[i].description.link(dir_list[i].link + "?$" + time_stamp));

		if(curlib.name!=undefined) {
			writeln("<td align=right><font color=black>" + dat_font + dir_list[i].size);
			writeln("<th>" + dat_font + (dir_list[i].settings&DIR_FREE ? "FREE":""));
		}
	}	
	writeln("</table>");
	if(file_list.length)
		writeln("<br>"); 
}

/* File Listing */
if(file_list.length) {

	/* Sort the list? */
	switch(ftp_sort) {
		case "uploader":
			file_list.sort(function(a,b) 
				{ 	if(a.uploader>b.uploader)
						return(1);
					if(a.uploader<b.uploader)
						return(-1);
					return(0); }
				);
			break;
		case "size":
			file_list.sort(function(a,b) 
				{ return(a.size-b.size); }
				);
			break;
		case "credits":
			file_list.sort(function(a,b) 
				{ return(a.credits-b.credits); }
				);
			break;
		case "time":
			file_list.sort(function(a,b) 
				{ return(a.time.valueOf()-b.time.valueOf()); }
				);
			break;
		case "hits":
			file_list.sort(function(a,b) 
				{ return(a.times_downloaded-b.times_downloaded); }
				);
			break;
	}
	if(ftp_reverse)
		file_list.reverse();

	var show_ext_desc;			/* show extended descriptions */
	var total_bytes=0;
	var total_downloads=0;
	var most_recent=0;

	if (curdir.name==undefined)
		show_ext_desc=false;	/* aliased files have no ext desc */
	else
		show_ext_desc=user.settings&USER_EXTDESC;

	writeln("<table " + cell_spacing + " width=100%>");
	writeln(font_face);
	
	/* header */
	writeln("<thead>");
	writeln("<tr bgcolor=" + hdr_background + ">");

	/* File */
	writeln(format("<th><a href=%s?sort=name%s$%s>%sFile</a>"
		,html_index_file
		,(ftp_sort=="name" && !ftp_reverse) ? "&reverse":"", time_stamp, hdr_font));

	/* Credits or Size */
	if(curdir.settings!=undefined && !(curdir.settings&DIR_FREE))
		writeln(format("<th><a href=%s?sort=credits%s$%s>%sCredits</a>"
			,html_index_file
			,(ftp_sort=="credits" && !ftp_reverse) ? "&reverse" : "", time_stamp, hdr_font));
	else
		writeln(format("<th><a href=%s?sort=size%s$%s>%sSize</a>"
			,html_index_file
			,(ftp_sort=="size" && !ftp_reverse) ? "&reverse" : "", time_stamp, hdr_font));

	/* Description */
	write("<th>" + hdr_font + "Description");
	if(!(user.security.restrictions&UFLAG_G) && curdir.settings!=undefined) {
		if(user.settings&USER_EXTDESC)
			writeln(format(" [%s]"
				,(hdr_font+"short").link(format("%s?ext=off$%s",html_index_file, time_stamp))));
		else
			writeln(format(" [%s]"
				,(hdr_font+"extended").link(format("%s?ext=on$%s",html_index_file, time_stamp))));
	}

	/* Date/Time */
	writeln(format("<th><a href=%s?sort=time%s$%s>%sDate/Time</a>"
		,html_index_file
		,(ftp_sort=="time" && !ftp_reverse) ? "&reverse" : "", time_stamp, hdr_font));

	/* Uploader and Hits (downloads) */
	if(curdir.name!=undefined) {	/* not valid for aliased files in root */
		writeln(format("<th><a href=%s?sort=uploader%s$%s>%sUploader</a>"
			,html_index_file
			,(ftp_sort=="uploader" && !ftp_reverse) ? "&reverse" : "", time_stamp, hdr_font));
		writeln(format("<th><a href=%s?sort=hits%s$%s>%sHits</a>"
			,html_index_file
			,(ftp_sort=="hits" && !ftp_reverse) ? "&reverse" : "", time_stamp, hdr_font));
	}
	writeln("</thead>");

	/* body */
	writeln("<tbody>");
	for(i in file_list) {

		total_downloads+=file_list[i].times_downloaded;
		if(file_list[i].time>most_recent)
			most_recent=file_list[i].time;

		writeln("<tr valign=top>");

		/* filename */
		if(user.security.restrictions&UFLAG_D
			|| (curdir.settings!=undefined 
				&& !(curdir.settings&DIR_FREE)
				&& !(user.security.exemptions&UFLAG_D)
				&& file_list[i].credits > (user.security.credits+user.security.free_credits))
			) {
			write("<td align=left>" + dat_font);
			writeln(file_list[i].name.link(
				"javascript:alert('Sorry, you do not have enough credits to download this file.');"));
		} else {
			write("<th align=left>" + dat_font);
			writeln(file_list[i].name.link(file_list[i].link));
		}

		/* size */
		write("<td align=right>" + dat_font + "<font color=black>");
		if(curdir.settings!=undefined && !(curdir.settings&DIR_FREE)) {
			if(!file_list[i].credits)
				writeln("<font color=white><b>FREE");
			else
				writeln(kbytes(file_list[i].credits)); 
			total_bytes+=file_list[i].credits;
		} else {
			writeln(kbytes(file_list[i].size)); 
			total_bytes+=file_list[i].size;
		}

		/* description */
		write("<td>");
		if (show_ext_desc) {
			if(file_list[i].settings&FILE_EXTDESC)
				writeln("<pre>" + file_list[i].extended_description);
			else
				writeln("<tt>" + file_list[i].description);
		} else
			writeln(dat_font + file_list[i].description);

		/* date/time */
		writeln("<td align=center nowrap>" + dat_font + "<font color=black>" 
			+ "<tt>"+ date(file_list[i].time));

		if(curdir.name!=undefined) {	/* not valid for aliased files in root */
			/* uploader */
			var uploader=file_list[i].uploader;
			if (file_list[i].settings&FILE_ANON)
				uploader="Anonymous";
			else if (uploader == "-> ADDFILES <-")
				uploader="Sysop".link("mailto:sysop@"+system.inetaddr);
			else if (!(user.security.restrictions&UFLAG_G))	/* ! Guest/Anonymous */
				uploader=uploader.link("mailto:" + uploader + "@" + system.inetaddr);
			writeln("<td nowrap>" + dat_font + uploader);

			/* download count */
			writeln("<td align=right>" + dat_font + "<font color=black>" 
				+ file_list[i].times_downloaded);
		}
	}

	/* Footer (with totals) */
	writeln("<tfoot>");
	writeln(format("<tr bgcolor=%s><th>%s%lu files" +
		"<th align=right>%s%s<th>%s-<th>%s<font color=black><tt>%s"
		,hdr_background
		,hdr_font, file_list.length
		,hdr_font, kbytes(total_bytes)
		,hdr_font
		,dat_font, date(most_recent)
		));

	if(curdir.name!=undefined) 	/* not valid for aliased files in root */
		writeln(format("<th>%s-<th align=right>%s%lu"
			,hdr_font
			,hdr_font, total_downloads
			));

	writeln("</table>");
}

if(!file_list.length && !dir_list.length)
	writeln("<br><b>No Files.</b><br>");

/* Footer */
write(format("<br><font size=%d color=silver>Problems? Ask ",font_size-1));
write(format("<a href=mailto:sysop@%s>%s</a>.",system.inetaddr,system.operator));

write(format("<br><font size=%d>Dynamically generated ",font_size-1));
write(format("in %lu milliseconds ", new Date().valueOf()-start.valueOf()));
write("by <a href=http://www.synchro.net>" + server.version + "</a>");
writeln("<br>" + Date() + "</font>");
writeln("</body>");
writeln("</html>");

/* End of ftp-html.js */