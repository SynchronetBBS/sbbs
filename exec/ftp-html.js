// JavaScript HTML Index for Synchronet FTP Server
// $id$

load("sbbsdefs.js");

var title=system.name + " FTP Server";

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
write("<body bgcolor=gray text=white link=yellow vlink=lime alink=white>\r\n");
write("<font face=Arial,Helvetica,sans-serif>\r\n");
write("<H1><I><FONT COLOR=lime>" + title + "</FONT></I></H1>\r\n");

write("<h4>");
var prevdir;

if(curlib.name!=undefined) {
	write("<font color=silver>Library: </font>"+curlib.description+"<BR>\r\n");
	prevdir="/";
}
if(curdir.name!=undefined) {
	write("<font color=silver>Directory: </font>"+curdir.description);
	if(curdir.misc&DIR_FREE)
		write("<font color=silver> - FREE Downloads</font>");
	write("<BR>\r\n");
	prevdir="/"+curlib.name+"/";
}
if(prevdir!=undefined) {
	write("<BR>\r\n");
	write(format("[<a href=%s%s>prev</a>] ",prevdir,html_index_file));
	write(format("[<a href=/%s>root</a>]",html_index_file));
}
write("</h4>");		

/* Table Attributes */
var hdr_background="white";
var hdr_font="<font size=-2><font color=black>";
var dat_font="<font size=-1>";
var cell_spacing=""; //"cellspacing=2 cellpadding=2";

/* Directory Listing */
if(dir.length) {

	write("<table border=0 nowrap " + cell_spacing + " width=50%>\r\n");

	/* header */
	write("<thead>");
	write("<tr bgcolor=" + hdr_background + ">");
	write("<th>" + hdr_font + "Directory");
	write("<th>" + hdr_font + "Description");
	write("</thead>");

	/* body */
	write("\r\n<tbody>\r\n");
	for(i in dir) {
		write("<TR>\r\n");

		/* filename */
		write("<TD>" + dat_font + format("<A HREF=%s", dir[i].link) + ">" 
			+ dir[i].name +"</A>");

		write("<TD>" + dat_font + dir[i].description);
	}	
	write("</TABLE>\r\n");
	write("<BR>\r\n");
}


function date(time)
{
	var mon=["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
	var d=new Date(time*1000);

	return(format("<TT>%s %02d %d %02d:%02d"
			,mon[d.getUTCMonth()]
			,d.getUTCDate()
			,d.getUTCFullYear()
			,d.getUTCHours()
			,d.getUTCMinutes()));
}

function bytes(nbytes)
{
	return(Math.round(nbytes/1024)+"k");
}

/* File Listing */
if(file.length) {

	var show_ext_desc=true;	/* show extended descriptions */
	var total_bytes=0;
	var total_downloads=0;
	var most_recent=0;

	if (curdir.name==undefined)
		show_ext_desc=false;	/* aliased files have no ext desc */

	write("<table border=0 nowrap " + cell_spacing + " width=100%>\r\n");

	/* header */
	write("<thead>");
	write("<tr bgcolor=" + hdr_background + ">");
	write("<th>" + hdr_font + "File");
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

		total_bytes+=file[i].size;
		total_downloads+=file[i].times_downloaded;
		if(file[i].time>most_recent)
			most_recent=file[i].time;

		write("<TR>\r\n");

		/* filename */
		write("<TD valign=top>" + dat_font + "<A HREF=" + file[i].name + ">" 
			+ file[i].name +"</A>");

		/* size */
		write("<TD valign=top align=right>" + dat_font + "<font color=black>" + bytes(file[i].size)); 

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
			+ date(file[i].time));

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
		"<TH align=right>%s%s<TH>%s-<TH>%s<font color=black>%s"
		,hdr_background
		,hdr_font, file.length
		,hdr_font, bytes(total_bytes)
		,hdr_font
		,dat_font, date(most_recent)
		));

	if(curdir.name!=undefined) 	/* not valid for aliased files in root */
		write(format("<TH>%s-<TH align=right>%s%lu"
			,hdr_font
			,hdr_font, total_downloads
			));

	write("</TABLE>\r\n");
	write("<BR>\r\n");
}

if(!file.length && !dir.length)
	write("No Files.<br>");

/* Footer */
write("<BR><font size='-2'>Dynamically generated by ");
write("<A HREF=http://www.synchro.net>" + system.version + "</A>");
write("<BR>" + Date() + "</font>\r\n");
write("</BODY>\r\n");
write("</HTML>\r\n");

/* End of ftp-html.js */