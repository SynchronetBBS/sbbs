// JavaScript HTML Index for Synchronet FTP Server
// $Id: ftp-html.js,v 1.23 2015/04/14 01:45:31 rswindell Exp $

load("sbbsdefs.js");    // Synchronet constants

var start=new Date();
var time_stamp = '';
var time_stamp_only = '';
var new_time_stamp='$'+start.valueOf().toString(36);    // Used to defeat caching browsers
if (!(user.security.restrictions&UFLAG_G)) {
	time_stamp=new_time_stamp;
	time_stamp_only = '%3F' + time_stamp;
}

/* Utility Functions */

function writeln(str)
{
    write(str + "\r\n");
}

function kbytes(nbytes)
{
    return(Math.round(nbytes/1024)+"k");
}

function secstr(sec)
{
    return(format("%02u:%02u",sec/60,sec%60));
}

var title = system.name + " - FTP Server";
var font_face = "<font face=Arial,Helvetica,sans-serif>";
var font_size = 2;  // Change base font size here
if(client.socket.local_port!=21)
    port=":" + client.socket.local_port;
else
    port="";

writeln('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">');
writeln("<html>");
writeln("<head>");
writeln("<title>");
writeln(title);
if (ftp.curdir.name!=undefined)
    write(" - " + ftp.curdir.name);
else if(ftp.curlib.description!=undefined)
    write(" - " + ftp.curlib.description);
writeln("</title>");

/* META TAGS */
writeln("<meta name='GENERATOR' content='" + system.version + "'>");
// The following line is necessary for IBM extended-ASCII in descriptions 
writeln("<meta http-equiv='Content-Type' content='text/html; charset=IBM437'>");

writeln("</head>");
writeln("<body bgcolor=#aaaaaa text=black link=white vlink=#334455 alink=red>");
writeln(font_face);

/* Go To Select Box */
writeln("<table width=100%>");
writeln("<td>");
writeln("<h1>" + font_face + "<font color=white>" + title.italics() + "</font></h1>");
writeln("<td align=right>");
writeln("<form>");  // Netscape requires this to be in a form <sigh>
writeln(format(
    "<select " +
    "onChange='if(selectedIndex>0) location=options[selectedIndex].value + \"%s\";'>"
    ,time_stamp_only));
writeln("<option>Go To...</option>");
writeln(format("<option value=/%s>Root</option>",html_index_file));
for(l in file_area.lib_list) {
    writeln(format("<optgroup label=%s>",file_area.lib_list[l].name));
    writeln(format("<option value=%s>[%s]"
        ,file_area.lib_list[l].link
        ,file_area.lib_list[l].name));
    for(d in file_area.lib_list[l].dir_list) {
        writeln(format("<option value=%s>%s"
            ,file_area.lib_list[l].dir_list[d].link
            ,file_area.lib_list[l].dir_list[d].name));
    }
    writeln("</optgroup>");
}
writeln("</select>");
writeln("</form>");
writeln("</table>");

var prevdir;

var hdr_font="<font color=black>";
var dat_font="<font color=#112233>";

if(!(user.security.restrictions&UFLAG_G)) { /* !Guest or Anonymous */
    if(system.matchuser("Guest")) {
        /* Logout button */
        writeln("<table align=right>");
        writeln("<form>");
        writeln("<input type=button value=Logout onclick='location=\"ftp://" 
            + format("%s/%s%s",system.host_name + port,html_index_file,time_stamp_only)
            + "\";'>");
        writeln("</form>");
        writeln("</table>");
        writeln("<br><br>");
    }

    /* User Info */
    writeln("<table border=0 nowrap align=left>");
    writeln(font_face);
    writeln("<tr><th bgcolor=#888888 align=right>"+hdr_font+"User:<th bgcolor=#888888 align=left>"+dat_font+user.alias);
    writeln("<tr><th bgcolor=#888888 align=right>"+hdr_font+"Address:<th bgcolor=#888888 align=left width=150>"+dat_font+user.ip_address);
    write("<tr><th bgcolor=#888888 align=right>"+hdr_font+"Credits:<th bgcolor=#888888 align=left>"+dat_font);
    if(user.security.exemptions&UFLAG_D)
        writeln("Exempt");
    else
        writeln(kbytes(user.security.credits+user.security.free_credits));
    write("<tr><th bgcolor=#888888 align=right>"+hdr_font+"Time left:<th bgcolor=#888888 align=left>"+dat_font);
    if(user.security.exemptions&UFLAG_T)
        writeln("Exempt");
    else
        writeln(secstr(ftp.time_left));
    writeln("</table>");

    /* User Stats */

    writeln("<table border=0 nowrap>");
    writeln(font_face);
    writeln("<tr><th align=right bgcolor=#888888>"+hdr_font+"Logons:<th bgcolor=#888888 align=left>"+dat_font+user.stats.total_logons);
    writeln("<tr><th bgcolor=#888888 align=right>"+hdr_font+"Last on:<th bgcolor=#888888 align=left>"+strftime("%B %d, %Y %H:%M" ,user.stats.laston_date));
    writeln("<tr><th bgcolor=#888888 align=right>"+hdr_font+"Uploaded:<th bgcolor=#888888 align=left>"+dat_font);
    writeln(format("%s bytes in %u files"
        ,kbytes(user.stats.bytes_uploaded),user.stats.files_uploaded));
    writeln("<tr><th bgcolor=#888888 align=right>"+hdr_font+"Downloaded:<th bgcolor=#888888 align=left>"+dat_font);
    writeln(format("%s bytes in %u files",kbytes(user.stats.bytes_downloaded)
        ,user.stats.files_downloaded));
    writeln("</table>");
    writeln("<br>");
} else if(ftp.curlib.name==undefined) { /* Login */
    writeln("<table align=right>");
    writeln("<td><input type=button value='New User' onClick='location=\"telnet://" 
        + system.host_name + port + "\";'>");
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
        write(format("+ '%s/%s%%3F%s'\r\n"
            ,system.host_name + port,html_index_file,new_time_stamp));
//      writeln("alert(url);");
        writeln("location = url;");
        writeln("}");
        writeln("// -->");
        writeln("</script>");

    writeln("</table>");
    writeln("</form>");
}

/* Virtual Path */
writeln("<h3>" + hdr_font + "Path: ");
if(ftp.curlib.name==undefined) 
    writeln(dat_font + "Root");
else
    writeln("Root".link(format("/%s%s",html_index_file,time_stamp_only)));
if(ftp.curlib.name!=undefined) {
    if(ftp.curdir.name==undefined)
        writeln(" / " + dat_font + ftp.curlib.description);
    else
        writeln(" / " + ftp.curlib.description.link(format("/%s/%s%s"
            ,ftp.curlib.name,html_index_file,time_stamp_only)));
}       
if(ftp.curdir.name!=undefined) 
    writeln(" / " + dat_font + ftp.curdir.description);
if(ftp.curdir.settings!=undefined && ftp.curdir.settings&DIR_FREE)
    write(hdr_font+" - FREE");
writeln("</h3>");


/* Table Attributes */
var hdr_background="#8899aa";
var hdr_font=format("<font size=%d color=black>",font_size-1);
var dat_font=format("<font size=%d>",font_size);
var cell_spacing=""; //"cellspacing=2 cellpadding=2";

/* Directory Listing */
if(ftp.dir_list.length) {

    writeln("<table " + cell_spacing + " width=33%>");
    writeln(font_face);

    /* header */
    writeln("<thead>");
    writeln("<tr bgcolor=" + hdr_background + ">");
    writeln("<th>" + hdr_font + "Directory");
    if(ftp.curlib.name!=undefined) 
        writeln("<th>" + hdr_font + "Files");
    writeln("</thead>");

    /* body */
    writeln("<tbody>");
    for(i in ftp.dir_list) {
        writeln("<tr>");
    
        /* filename */
        writeln("<th nowrap align=left>" + dat_font 
            + ftp.dir_list[i].description.link(ftp.dir_list[i].link + time_stamp_only));

        if(ftp.curlib.name!=undefined) {
            writeln("<td align=right><font color=black>" + dat_font + ftp.dir_list[i].size);
            writeln("<th>" + dat_font + (ftp.dir_list[i].settings&DIR_FREE ? "FREE":""));
        }
    }   
    writeln("</table>");
    if(ftp.file_list.length)
        writeln("<br>"); 
}

/* File Listing */
if(ftp.file_list.length) {

    /* Sort the list? */
    switch(ftp.sort) {
        case "uploader":
            ftp.file_list.sort(function(a,b) 
                {   if(a.uploader>b.uploader)
                        return(1);
                    if(a.uploader<b.uploader)
                        return(-1);
                    return(0); }
                );
            break;
        case "size":
            ftp.file_list.sort(function(a,b) 
                { return(a.size-b.size); }
                );
            break;
        case "credits":
            ftp.file_list.sort(function(a,b) 
                { return(a.credits-b.credits); }
                );
            break;
        case "time":
            ftp.file_list.sort(function(a,b) 
                { return(a.time.valueOf()-b.time.valueOf()); }
                );
            break;
        case "hits":
            ftp.file_list.sort(function(a,b) 
                { return(a.times_downloaded-b.times_downloaded); }
                );
            break;
    }
    if(ftp.reverse)
        ftp.file_list.reverse();

    var show_ext_desc;          /* show extended descriptions */
    var total_bytes=0;
    var total_downloads=0;
    var most_recent=0;

    if (ftp.curdir.name==undefined)
        show_ext_desc=false;    /* aliased files have no ext desc */
    else
        show_ext_desc=ftp.extended_descriptions;

    writeln("<table " + cell_spacing + " width=100%>");
    writeln(font_face);
    
    /* header */
    writeln("<thead>");
    writeln("<tr bgcolor=" + hdr_background + ">");

    /* File */
    writeln(format("<th><a href=%s%%3Fsort=name%s%s>%sFile</a>"
        ,html_index_file
        ,(ftp.sort=="name" && !ftp.reverse) ? "&reverse":"", time_stamp, hdr_font));

    /* Credits or Size */
    if(ftp.curdir.settings!=undefined && !(ftp.curdir.settings&DIR_FREE))
        writeln(format("<th><a href=%s%%3Fsort=credits%s%s>%sCredits</a>"
            ,html_index_file
            ,(ftp.sort=="credits" && !ftp.reverse) ? "&reverse" : "", time_stamp, hdr_font));
    else
        writeln(format("<th><a href=%s%%3Fsort=size%s%s>%sSize</a>"
            ,html_index_file
            ,(ftp.sort=="size" && !ftp.reverse) ? "&reverse" : "", time_stamp, hdr_font));

    /* Description */
    write("<th>" + hdr_font + "Description");
    if(ftp.extended_descriptions)
        writeln(format(" [%s]"
            ,(hdr_font+"short").link(format("%s%%3Fext=off%s",html_index_file, time_stamp))));
    else
        writeln(format(" [%s]"
            ,(hdr_font+"extended").link(format("%s%%3Fext=on%s",html_index_file, time_stamp))));

    /* Date/Time */
    writeln(format("<th><a href=%s%%3Fsort=time%s%s>%sDate/Time</a>"
        ,html_index_file
        ,(ftp.sort=="time" && !ftp.reverse) ? "&reverse" : "", time_stamp, hdr_font));

    /* Uploader and Hits (downloads) */
    if(ftp.curdir.name!=undefined) {    /* not valid for aliased files in root */
        writeln(format("<th><a href=%s%%3Fsort=uploader%s%s>%sUploader</a>"
            ,html_index_file
            ,(ftp.sort=="uploader" && !ftp.reverse) ? "&reverse" : "", time_stamp, hdr_font));
        writeln(format("<th><a href=%s%%3Fsort=hits%s%s>%sHits</a>"
            ,html_index_file
            ,(ftp.sort=="hits" && !ftp.reverse) ? "&reverse" : "", time_stamp, hdr_font));
    }
    writeln("</thead>");

    /* body */
    writeln("<tbody>");
    for(i in ftp.file_list) {

        total_downloads+=ftp.file_list[i].times_downloaded;
        if(ftp.file_list[i].time>most_recent)
            most_recent=ftp.file_list[i].time;

        writeln("<tr valign=top>");

        /* filename */
        if(user.security.restrictions&UFLAG_D
            || (ftp.curdir.settings!=undefined 
                && !(ftp.curdir.settings&DIR_FREE)
                && !(user.security.exemptions&UFLAG_D)
                && ftp.file_list[i].credits > (user.security.credits+user.security.free_credits))
            ) {
            write("<td align=left>" + dat_font);
            writeln(ftp.file_list[i].name.link(
                "javascript:alert('Sorry, you do not have enough credits to download this file.');"));
        } else {
            write("<th align=left>" + dat_font);
            writeln(ftp.file_list[i].name.link(ftp.file_list[i].link));
        }

        /* size */
        write("<td align=right>" + dat_font + "<font color=black>");
        if(ftp.curdir.settings!=undefined && !(ftp.curdir.settings&DIR_FREE)) {
            if(!ftp.file_list[i].credits)
                writeln("<font color=white><b>FREE");
            else
                writeln(kbytes(ftp.file_list[i].credits)); 
            total_bytes+=ftp.file_list[i].credits;
        } else {
            writeln(kbytes(ftp.file_list[i].size)); 
            total_bytes+=ftp.file_list[i].size;
        }

        /* description */
        write("<td>" + dat_font);
        if (show_ext_desc) {
            if(ftp.file_list[i].settings&FILE_EXTDESC)
                writeln("<pre>" + ftp.file_list[i].extended_description);
            else
                writeln("<tt>" + ftp.file_list[i].description);
        } else
            writeln(dat_font + ftp.file_list[i].description);

        /* date/time */
        writeln("<td align=center nowrap>" + dat_font + "<font color=black>" 
            + "<tt>"+ strftime("%b %d, %Y %H:%M" ,ftp.file_list[i].time));

        if(ftp.curdir.name!=undefined) {    /* not valid for aliased files in root */
            /* uploader */
            var uploader=ftp.file_list[i].uploader;
            if(ftp.file_list[i].settings&FILE_ANON)
                uploader="Anonymous";
			else {
				if(uploader == "-> ADDFILES <-")
					uploader=system.operator;
				if(!(user.security.restrictions&UFLAG_G)) /* ! Guest/Anonymous */
					uploader=uploader.link("mailto:" + uploader + "@" + system.inetaddr);
			}

            writeln("<td nowrap>" + dat_font + uploader);

            /* download count */
            writeln("<td align=right>" + dat_font + "<font color=black>" 
                + ftp.file_list[i].times_downloaded);
        }
    }

    /* Footer (with totals) */
    writeln("<tfoot>");
    writeln(format("<tr bgcolor=%s><th>%s%lu files" +
        "<th align=right>%s%s<th>%s-<th>%s<font color=black><tt>%s"
        ,hdr_background
        ,hdr_font, ftp.file_list.length
        ,hdr_font, kbytes(total_bytes)
        ,hdr_font
        ,dat_font, strftime("%b %d, %Y %H:%M" ,most_recent)
        ));

    if(ftp.curdir.name!=undefined)  /* not valid for aliased files in root */
        writeln(format("<th>%s-<th align=right>%s%lu"
            ,hdr_font
            ,hdr_font, total_downloads
            ));

    writeln("</table>");
}

if(!ftp.file_list.length && !ftp.dir_list.length)
    writeln("<br><b>No Files.</b><br>");

/* Footer */
write(format("<br><font size=%d color=black>Problems? Ask ",font_size-1));
write(format("<a href=mailto:sysop@%s>%s</a>.",system.inetaddr,system.operator));

write(format("<br><font size=%d>Dynamically generated ",font_size-1));
write(format("in %lu milliseconds ", new Date().valueOf()-start.valueOf()));
write("by <a href=http://www.synchro.net>" + server.version + "</a>");
writeln("<br>" + Date() + "</font>");
writeln("</body>");
writeln("</html>");

/* End of ftp-html.js */
