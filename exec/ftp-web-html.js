// JavaScript HTML Index for Synchronet FTP Server
// $Id: ftp-web-html.js,v 1.11 2015/04/14 01:42:29 rswindell Exp $

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
if(client.socket.local_port!=21)
    port=":" + client.socket.local_port;
else
    port="";

var http_port = 80;
var file = new File(file_cfgname(system.ctrl_dir, "sbbs.ini"));
if(file.open("r")) {
	http_port = file.iniGetValue("web","port",80);
	file.close();
}
    
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
writeln('<link rel="stylesheet" type="text/css" href="http://' + format("%s:%u",system.host_name, http_port) + '/default.css">');

writeln("</head>");
writeln('<body style="background-image: url(http://' + format("%s:%u",system.host_name, http_port) +'/images/default/bg_grad.jpg)";>');
//writeln(font_face);

writeln('<table class="welcome_main" width="95%"><tr><td>');

/* Go To Select Box */
writeln("<table width=100%>");
writeln("<td>");
writeln('<h1 class="ftp_title">' + title + '</h1>');
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

var hdr_font="<font color=silver>";
var dat_font="<font color=#CCCCCC>";

if(!(user.security.restrictions&UFLAG_G)) { /* !Guest or Anonymous */
    if(system.matchuser("Guest")) {
        /* Logout button */
        writeln("<table align=right>");
        writeln("<form>");
        writeln("<input type=button value=Logout onclick='location=\"ftp://" 
            + format("%s/%s%s",system.host_name + port,html_index_file,time_stamp_only)
            + "\";'>");
        writeln("</form>");
        writeln("</table><br /><br />");
    }

	writeln("<table nowrap class=\"ftp_stats\"><tr><td>");

    /* User Info */
    writeln("<table nowrap align=left>");
    writeln("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "User:<th class=\"ftp_stats\" align=left>" +dat_font+ user.alias);
    writeln("<tr><th class=\"ftp_stats\" align=right>"+ hdr_font 
        + "Address:<th class=\"ftp_stats\" align=left width=150>"+dat_font+user.ip_address);
    write("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "Credits:<th class=\"ftp_stats\" align=left>"+dat_font);
    if(user.security.exemptions&UFLAG_D)
        writeln("Exempt");
    else
        writeln(kbytes(user.security.credits+user.security.free_credits));
    write("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "Time left:<th class=\"ftp_stats\" align=left>"+dat_font);
    if(user.security.exemptions&UFLAG_T)
        writeln("Exempt");
    else
        writeln(secstr(ftp.time_left));
    writeln("</table>");

    /* User Stats */

    writeln("<table nowrap>");
    writeln("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "Logons:<th class=\"ftp_stats\" align=left>" + dat_font + user.stats.total_logons);
    writeln("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "Last on:<th class=\"ftp_stats\" align=left>"+ dat_font 
        + strftime("%B %d, %Y, %H:%M" ,user.stats.laston_date));
    writeln("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "Uploaded:<th class=\"ftp_stats\" align=left>"+dat_font);
    writeln(format("%s bytes in %u files"
        ,kbytes(user.stats.bytes_uploaded),user.stats.files_uploaded));
    writeln("<tr><th class=\"ftp_stats\" align=right>" + hdr_font 
        + "Downloaded:<th class=\"ftp_stats\" align=left>"+dat_font);
    writeln(format("%s bytes in %u files",kbytes(user.stats.bytes_downloaded)
        ,user.stats.files_downloaded));
    writeln("</table>");

	writeln("</tr></td></table>");  
    
    writeln("<br>");
} else if(ftp.curlib.name==undefined) { /* Login */
    writeln("<table align=right>");
    writeln("<td><input type=button value='New User' onClick='location=\"telnet://" 
        + system.host_name + port + "\";'>");
    writeln("</table>");

    writeln("<form name='login'>");
    writeln("<table border=1 frame=box rules=none cellpadding=3>");
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

writeln('<table align="center" width="100%"  border="0" cellspacing="0" cellpadding="0">');
    writeln('<tr>');
    writeln('<td style="background-image: url(http://' + format("%s:%u",system.host_name, http_port) + '/images/default/tnav_bg.gif); background-repeat: repeat-x; text-align: left;" width="1%"><img src="http://' + format("%s:%u"
        ,system.host_name
        ,http_port) 
        + '/images/default/tnav_left.gif" width="5" height="32" alt="" /></td>');
    writeln('<td style="background-image: url(http://' 
        + format("%s:%u",system.host_name, http_port) 
        + '/images/default/tnav_bg.gif); background-repeat: repeat-x;">');
        if(ftp.curlib.name==undefined) 
            writeln('<span class="tlink">FTP Server Root</span>');
        else
    writeln('<a class="tlink" href="' + format("/%s%s",html_index_file,time_stamp_only) 
        + '">FTP Server Root</a>');
    if(ftp.curlib.name!=undefined) {
        if(ftp.curdir.name==undefined)
            writeln('<span class="tlink">' + ftp.curlib.description + '</span>');
        else
            writeln('<a class="tlink" href="' + format("/%s/%s%s",ftp.curlib.name,html_index_file,time_stamp_only) + '">' 
            + ftp.curlib.description + '</a>');
}       
if(ftp.curdir.name!=undefined) 
    writeln('<span class="tlink">' + ftp.curdir.description + '</span>');
if(ftp.curdir.settings!=undefined && ftp.curdir.settings&DIR_FREE)
    write(" - FREE");
    writeln('</td>');
    writeln('<td style="background-image: url(http://' + format("%s:%u",system.host_name, http_port) + '/images/default/tnav_bg.gif); background-repeat: repeat-x; text-align: right;" width="1%"><img src="http://' + format("%s:%u",system.host_name, http_port) 
        + '/images/default/tnav_right.gif" width="5" height="32" alt="" /></td>');
    writeln('</tr>');
writeln('</table>');
writeln('<br />');



/* Directory Listing */
if(ftp.dir_list.length) {

    writeln('<table class="ftp_dirlist" cellspacing="2" width="33%">');

    /* header */
    writeln("<thead>");
    writeln('<tr class="ftp_dirlist_hdr">');
    writeln('<th class="ftp_dirlist_hdr">Directory</th>');
    if(ftp.curlib.name!=undefined) 
        writeln('<th class="ftp_dirlist_hdr">Files</th>');
    writeln("</thead>");

    /* body */
    writeln("<tbody>");
    for(i in ftp.dir_list) {
        writeln("<tr>");
    
        /* filename */
        writeln('<th class="ftp_dirlist" nowrap align="left"><a class="ftp_dirlist" href="' 
            + ftp.dir_list[i].link + time_stamp_only + '">' + ftp.dir_list[i].description + '</a></th>');

        if(ftp.curlib.name!=undefined) {
            writeln('<td class="ftp_dirlist" align="right">' + ftp.dir_list[i].size);
            writeln((ftp.dir_list[i].settings&DIR_FREE ? "FREE</th>":"</th>"));
        }
    }   
    writeln('</tr><tr><th class="ftp_dirlist_hdr">&nbsp;</th>');
    if(ftp.curlib.name!=undefined) 
        writeln('<th class="ftp_dirlist_hdr_rt">-</th></tr></tbody></table>');
    else
        writeln('</table>');
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

    writeln('<table class="ftp_dirlist" cellspacing="2" width="100%">');
    
    /* header */
    writeln("<thead>");
    writeln('<tr>');

    /* File */
    writeln(format('<th class="ftp_dirlist_hdr"><a class="ftp_dirlist_hdr" href=%s%%3Fsort=name%s%s>File</a>'
        ,html_index_file
        ,(ftp.sort=="name" && !ftp.reverse) ? '&reverse':'', time_stamp) + '</th>');

    /* Credits or Size */
    if(ftp.curdir.settings!=undefined && !(ftp.curdir.settings&DIR_FREE))
        writeln(format('<th class="ftp_dirlist_hdr"><a class="ftp_dirlist_hdr" href=%s%%3Fsort=credits%s%s>Credits</a>'
            ,html_index_file
            ,(ftp.sort=="credits" && !ftp.reverse) ? '&reverse' : '', time_stamp) + '</th>');
    else
        writeln(format('<th class="ftp_dirlist_hdr"><a class="ftp_dirlist_hdr" href=%s%%3Fsort=size%s%s>Size</a>'
            ,html_index_file
            ,(ftp.sort=="size" && !ftp.reverse) ? '&reverse' : '', time_stamp) + '</th>');

    /* Description */
    write('<th class="ftp_dirlist_hdr">Description');
    if(ftp.extended_descriptions)
        writeln(' [<a class="ftp_dirlist_hdr" href="' 
        + format("%s%%3Fext=off%s",html_index_file, time_stamp) + '">short</a>]</th>');
    else
        writeln(' [<a class="ftp_dirlist_hdr" href="' 
        + format("%s%%3Fext=on%s",html_index_file, time_stamp) + '">extended</a>]</th>');

    /* Date/Time */
    writeln(format('<th class="ftp_dirlist_hdr"><a class="ftp_dirlist_hdr" href=%s%%3Fsort=time%s%s>Date/Time</a>'
        ,html_index_file
        ,(ftp.sort=="time" && !ftp.reverse) ? '&reverse' : '', time_stamp) + '</th>');

    /* Uploader and Hits (downloads) */
    if(ftp.curdir.name!=undefined) {    /* not valid for aliased files in root */
        writeln(format('<th class="ftp_dirlist_hdr"><a class="ftp_dirlist_hdr" href=%s%%3Fsort=uploader%s%s>Uploader</a>'
            ,html_index_file
            ,(ftp.sort=="uploader" && !ftp.reverse) ? '&reverse' : '', time_stamp) + '</th>');
        writeln(format('<th class="ftp_dirlist_hdr"><a class="ftp_dirlist_hdr" href=%s%%3Fsort=hits%s%s>Hits</a>'
            ,html_index_file
            ,(ftp.sort=="hits" && !ftp.reverse) ? '&reverse' : '', time_stamp) + '</th>');
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
            write('<td class="ftp_dirlist" align="left">');
            writeln('<a class="ftp_dirlist" href="javascript:alert(\"Sorry, you do not have enough credits to download this file.\"); ">' 
            + ftp.file_list[i].name + '</a><td>');
        } else {
            write('<th class="ftp_dirlist" align=left>');
            writeln('<a class="ftp_dirlist" href="' + ftp.file_list[i].link + '">' 
            + ftp.file_list[i].name + "</a></th>");
        }

        /* size */
        write('<td class="ftp_dirlist" align="right">');
        if(ftp.curdir.settings!=undefined && !(ftp.curdir.settings&DIR_FREE)) {
            if(!ftp.file_list[i].credits)
                writeln('FREE</td>');
            else
                writeln(kbytes(ftp.file_list[i].credits)+ "</td>"); 
            total_bytes+=ftp.file_list[i].credits;
        } else {
            writeln(kbytes(ftp.file_list[i].size) + "</td>"); 
            total_bytes+=ftp.file_list[i].size;
        }

        /* description */
        write("<td>");
        if (show_ext_desc) {
            if(ftp.file_list[i].settings&FILE_EXTDESC)
                writeln("<pre class=\"ftp_desc\">" + ftp.file_list[i].extended_description + "</pre></td>");
            else
                writeln("<tt class=\"ftp_dirlist\">" + ftp.file_list[i].description + "</tt></td>");
        } else
            writeln(ftp.file_list[i].description + "</td>");

        /* date/time */
        writeln('<td align="left" class="ftp_dirlist" nowrap="nowrap">' 
            + strftime("%b %d, %Y %H:%M" ,ftp.file_list[i].time) + "</td>");
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
            writeln('<td class="ftp_dirlist" nowrap="nowrap">' + uploader + '</td>');

            /* download count */
            writeln('<td class="ftp_dirlist" align=right>' 
                + ftp.file_list[i].times_downloaded + '</td>');
        }
    }

    /* Footer (with totals) */
    writeln("<tfoot>");
    writeln(format('<tr><th class="ftp_dirlist_ftr">%lu files</th><th class="ftp_dirlist_ftr" align="right">%s</th><th class="ftp_dirlist_ftr">-</th><th class="ftp_dirlist_ftr">%s'
        ,ftp.file_list.length
        ,kbytes(total_bytes)
        ,strftime("%b %d, %Y %H:%M" ,most_recent)
        ));

    if(ftp.curdir.name!=undefined)  /* not valid for aliased files in root */
        writeln(format('<th class="ftp_dirlist_ftr">-</th><th class="ftp_dirlist_ftr" align=right>%lu'
            ,total_downloads
            ) + '</th>');

    writeln('</tr></tfoot></table>');
}

if(!ftp.file_list.length && !ftp.dir_list.length)
    writeln("<br><b>No Files.</b><br>");

/* Footer */
write(format('<div class="ftp_ftr"><br>Problems? Ask '));
write(format("<a href=mailto:sysop@%s>%s</a>.",system.inetaddr,system.operator));

write(format("<br>Dynamically generated "));
write(format("in %lu milliseconds ", new Date().valueOf()-start.valueOf()));
write("by <a href=http://www.synchro.net>" + server.version + "</a>");
writeln("<br>" + Date() + "</font>");
writeln('</div></td></tr></table><br>');
writeln("</body>");
writeln("</html>");

/* End of ftp-html.js */
