// JavaScript HTML Index for Synchronet FTP Server
// $id$

load("sbbsdefs.js");	// Synchronet constants

/* Utility Functions */
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

var title=system.name /* + " FTP Server" */ ;
if (curdir.name!=undefined)
	title += " - " + curdir.name;
else if(curlib.name!=undefined)
	title += " - " + curlib.name;

write('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">\r\n');
write("<html>\r\n");
write("<head>\r\n");
write("<title>");
write(title);
write("</title>\r\n");
write("<META NAME='GENERATOR' content='" + system.version + "'>\r\n");
write("</head>\r\n");
write("<body bgcolor=teal text=white link=yellow vlink=lime alink=white>\r\n");
write("<font face=Arial,Helvetica,sans-serif>\r\n");
write("<H1><I><FONT COLOR=lime>" + title + "</FONT></I></H1>\r\n");

var prevdir;

var hdr_font="<font color=silver>";
var dat_font="<font color=white>";

/* User Info */
write("<table border=0 nowrap>\r\n");
write("<tr><th align=right>"+hdr_font+"User:<th align=left>"+dat_font+user.alias);
/* Library Info */
if(curlib.name!=undefined) {
	write("<th align=right>"+hdr_font+"Library:<th align=left>"+dat_font+curlib.description);
	prevdir="/";
} else
	write("<th align=right>"+hdr_font+"Logons:<th align=left>"+dat_font+user.total_logons);
write("<tr><th align=right>"+hdr_font+"Address:<th align=left width=150>"+dat_font+user.note);
if(curdir.name!=undefined) {
	write("<th align=right>"+hdr_font+"Directory:<th align=left>"+dat_font+curdir.description);
	if(curdir.misc&DIR_FREE)
		write(hdr_font+" - FREE Downloads</font>");
	prevdir="/"+curlib.name+"/";
} else if(curlib.name==undefined) 
	write("<th align=right>"+hdr_font+"Last on:<th align=left>"+date(user.laston_date));

write("<tr><th align=right>"+hdr_font+"Credits:<th align=left>"+dat_font);
if(user.exemptions&UFLAG_D)
	write("Exempt");
else
	write(kbytes(user.credits+user.free_credits));
if(curdir.misc!=undefined) {
	write("<th align=right>"+hdr_font+"Descriptions:<th align=left>");
	if(user.settings&EXTDESC)
		write(format("[<a href=%s?ext=off>short</a>]",html_index_file));
	else
		write(format("[<a href=%s?ext=on>extended</a>]",html_index_file));
} else if(curlib.name==undefined && user.files_uploaded) {
	write("<th align=right>"+hdr_font+"Uploaded:<th align=left>"+dat_font);
	write(format("%s bytes in %u files",kbytes(user.bytes_uploaded),user.files_uploaded));
}

write("<tr><th align=right>"+hdr_font+"Time left:<th align=left>"+dat_font);
if(user.exemptions&UFLAG_T)
	write("Exempt");
else
	write(secstr(time_left));
if(prevdir!=undefined) {
	write("<th align=right>"+hdr_font+"Navigate:<th align=left>");
	write(format("[<a href=%s%s>prev</a>] ",prevdir,html_index_file));
	write(format("[<a href=/%s>root</a>] ",html_index_file));
} else if(curlib.name==undefined && user.files_downloaded) {
	write("<th align=right>"+hdr_font+"Downloaded:<th align=left>"+dat_font);
	write(format("%s bytes in %u files",kbytes(user.bytes_downloaded),user.files_downloaded));
}

write("\r\n");
write("</table>\r\n");

/* Table Attributes */
var hdr_background="white";
var hdr_font="<font size=-2><font color=black>";
var dat_font="<font size=-1>";
var cell_spacing=""; //"cellspacing=2 cellpadding=2";

/* Directory Listing */
if(dir.length) {

	write("<br><table border=0 " + cell_spacing + " width=50%>\r\n");

	/* header */
	write("<thead>");
	write("<tr bgcolor=" + hdr_background + ">");
	write("<th>" + hdr_font + "Directory");
	write("<th>" + hdr_font + "Description");
	if(curlib.name!=undefined) 
		write("<th>" + hdr_font + "Files");
	write("</thead>");

	/* body */
	write("\r\n<tbody>\r\n");
	for(i in dir) {
		write("<tr>\r\n");

		/* filename */
		write("<th nowrap align=left>" + dat_font + format("<A HREF=%s", dir[i].link) + ">" 
			+ dir[i].name +"</A>");

		write("<td nowrap>" + dat_font + dir[i].description);

		if(curlib.name!=undefined) {
			write("<td align=right><font color=black>" + dat_font + dir[i].size);
			write("<th>" + dat_font + (dir[i].misc&DIR_FREE ? "FREE":""));
		}
	}	
	write("</table>\r\n");
}

/* File Listing */
if(file.length) {

	var show_ext_desc;			/* show extended descriptions */
	var total_bytes=0;
	var total_downloads=0;
	var most_recent=0;

	if (curdir.name==undefined)
		show_ext_desc=false;	/* aliased files have no ext desc */
	else
		show_ext_desc=user.settings&EXTDESC;

	write("<br><table border=0 nowrap " + cell_spacing + " width=100%>\r\n");

	/* header */
	write("<thead>");
	write("<tr bgcolor=" + hdr_background + ">");
	write("<th>" + hdr_font + "File");
	if(curdir.misc!=undefined && !(curdir.misc&DIR_FREE))
		write("<th>" + hdr_font + "Credits");
	else
		write("<th>" + hdr_font + "Size");
	write("<th>" + hdr_font + "Description");
	write("<th>" + hdr_font + "Date/Time");
	if(curdir.name!=undefined) {	/* not valid for aliased files in root */
		write("<th>" + hdr_font + "Uploader");
		write("<th>" + hdr_font + "Hits");
	}
	write("</thead>");

	/* body */
	write("\r\n<tbody>\r\n");
	for(i in file) {

		total_downloads+=file[i].times_downloaded;
		if(file[i].time>most_recent)
			most_recent=file[i].time;

		write("<TR>\r\n");

		/* filename */
		write("<th valign=top align=left>" + dat_font);
		if(curdir.misc!=undefined 
			&& !(curdir.misc&DIR_FREE)
			&& !(user.exemptions&UFLAG_D)
			&& file[i].credits > (user.credits+user.free_credits)
			)
			write(file[i].name);
		else
			write("<A HREF=" + file[i].name + ">" + file[i].name +"</A>");

		/* size */
		write("<TD valign=top align=right>" + dat_font + "<font color=black>");
		if(curdir.misc!=undefined && !(curdir.misc&DIR_FREE)) {
			write(kbytes(file[i].credits)); 
			total_bytes+=file[i].credits;
		} else {
			write(kbytes(file[i].size)); 
			total_bytes+=file[i].size;
		}

		/* description */
		write("<TD valign=top>");
		if (show_ext_desc) {
			if(file[i].misc&FM_EXTDESC)
				write("<PRE>" + file[i].extended_description);
			else
				write("<TT>" + file[i].description);
		} else
			write(dat_font + file[i].description);

		/* date/time */
		write("<TD valign=top align=center nowrap>" + dat_font + "<font color=black>" 
			+ "<TT>"+ date(file[i].time));

		if(curdir.name!=undefined) {	/* not valid for aliased files in root */
			/* uploader */
			var uploader=file[i].uploader;
			if (file[i].misc&FM_ANON)
				uploader="Anonymous";
			else if (uploader == "-> ADDFILES <-")
				uploader="Sysop";
			write("<TD valign=top nowrap>" + dat_font + uploader);

			/* download count */
			write("<TD valign=top align=right>" + dat_font + "<font color=black>" 
				+ file[i].times_downloaded);
		}
		write("\r\n");
	}

	/* Footer (with totals) */
	write("<tfoot>\r\n");
	write(format("<TR bgcolor=%s><TH>%s%lu files" +
		"<TH align=right>%s%s<TH>%s-<TH>%s<font color=black><TT>%s"
		,hdr_background
		,hdr_font, file.length
		,hdr_font, kbytes(total_bytes)
		,hdr_font
		,dat_font, date(most_recent)
		));

	if(curdir.name!=undefined) 	/* not valid for aliased files in root */
		write(format("<TH>%s-<TH align=right>%s%lu"
			,hdr_font
			,hdr_font, total_downloads
			));

	write("</TABLE>\r\n");
}

if(!file.length && !dir.length)
	write("<br><b>No Files.</b><br>");

/* Footer */
write("<BR><font size='-2'>Problems? Ask ");
write(format("<a href=mailto:sysop@%s>%s</a>.",system.inetaddr,system.operator));

write("<BR><font size='-2'>Dynamically generated by ");
write("<A HREF=http://www.synchro.net>" + system.version + "</A>");
write("<BR>" + Date() + "</font>\r\n");
write("</BODY>\r\n");
write("</HTML>\r\n");

/* End of ftp-html.js */