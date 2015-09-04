/* $Id$ */

var REVISION = "$Revision$".split(' ')[1];

var start=time();

var query_string;

var lib = argv[0];
var list = argv[1];
var query_string=argv[2];

if(lib==undefined)
    lib = load(new Object, "sbbslist_lib.js");

if(list == undefined)
    list=lib.read_list();

//list=lib.remove_dupes(list);

if(this.query_string && query_string.length) {
    lib.sort_property=query_string;
    list.sort(lib.compare);
}

writeln('<!DOCTYPE html>');
writeln('<html>');
writeln('<head>');
writeln("<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>");
writeln('<style media="screen" type="text/css">');
writeln('body { font-family:arial, sans-serif; }');
writeln('th { color:#FFFFFF; background-color:#000000;}');
writeln('td { vertical-align:top; font-size:80%; }');
writeln('tr { background-color:#eeeeee; }');
writeln('pre { background-color:black; font-family:Courier,Prestige,monospace; font-size:5pt }');
writeln('</style>');
writeln('<title>Synchronet BBS List</title>');
writeln('</head>');

load("portdefs.js");
load("graphic.js");

//writeln('<body><font face="Arial" size="-1">');

list=list.filter(function(obj) { return obj.software.bbs.substr(0,10).toLowerCase() == "synchronet"; });


writeln('<h1 style="text-align: center;"><i>' + 'Synchronet'.link('http://www.synchro.net') + ' BBS List</i></h1>');

if(0) {
	var synch_ansi="";
	var f=new File(system.text_dir + "menu/logon.asc");
	if(f.open("rb")) {
    		synch_ansi=f.read();
    		f.close();
	}

	writeln('<table><tr><td><pre style="background-color:lightgrey;">'
        	+html_encode(synch_ansi, true, false, true, true).replace(/background-color: black;/g, 'background-color: lightgrey')
        	+'</pre></table>');
}

function encode_text(str)
{
	return html_encode(str
		, /* ex-ascii: */	true
		, /* white-space: */	false
		, /* ansi: */		false
		, /* ctrl-a: */		false);
}

function bbs_service(service)
{
    var uri = service.address;
	if(uri.length==0)
		return "";
    if(uri.indexOf(':') < 0) {
        var port = "";
        if(service.port)
            port = ":" + service.port;
        var sep="//";
        var protocol = service.protocol.toLowerCase();
        switch(protocol) {
            case "modem":
                protocol="tel"
                break;
			default:	
                if(service.port == standard_service_port[protocol])
                    port="";
                break;
        }

        if(protocol=="tel")
            sep="";

        uri=format('%s:%s%s%s', protocol, sep, service.address, port);
    }
    return encode_text(service.address + port).link(encodeURI(uri)) + " ("+encode_text(service.protocol)+")";
}

function bbs_sysop(sysop)
{
    if(sysop.email && sysop.email.length)
        return format('<a href="&#109;&#97;&#105;&#108;&#116;&#111;&#58;%s">%s</a>', encodeURI(sysop.email), encode_text(sysop.name));
    return(encode_text(sysop.name));
}

function bbs_preview(bbs)
{
    /***
    var cap=[];
    if(!bbs.preview)
        return;
    for(var i in bbs.preview) {
        if(bbs.preview[i].length)
            cap.push(base64_decode(bbs.preview[i]));
        else
            cap.push("");
    }
    **/
//    log(LOG_DEBUG,bbs.preview.join("\r\n"));
    var graphic=new Graphic();
    graphic.base64_decode(bbs.preview);
    write('<pre>');
    write(graphic.HTML.replace(/background-color: black;/g, ''));
    writeln('</pre>');
}

function localDateStr(date)
{
	var date=new Date(date);

	return format("%u-%02u-%02u", date.getFullYear(), date.getMonth()+1, date.getDate());
}

function bbs_table_entry(bbs)
{
    var i;

    /* Name: */
    writeln('<tr><td style="text-align:center;"><b>');
    var uri = bbs.web_site;
    if(uri.length) {
        if(uri.indexOf('://')<1)
            uri = "http://" + bbs.web_site;
        writeln(encode_text(bbs.name).link(encodeURI(uri)));
    } else
        writeln(encode_text(bbs.name));
    writeln('</b>');

    /* Since: */
    writeln('<td style="text-align:center;">');
    if(bbs.first_online)
        writeln(encode_text(bbs.first_online.substring(0,4)));

    /* Operators: */
    writeln('<td>');
    for(i=0; i<bbs.sysop.length; i++)
        writeln((i ? ', ':'') + bbs_sysop(bbs.sysop[i]));
    writeln('<td>' + encode_text(bbs.location));
    writeln('<td>');
	writeln(bbs_service(bbs.service[0]));

    /* Verification results */
	writeln('<td>');
    if(bbs.entry.autoverify) {
        if(!bbs.entry.autoverify.success) {
            if(bbs.entry.autoverify.last_failure) {
                writeln(localDateStr(bbs.entry.autoverify.last_failure.on));
                writeln(encode_text(bbs.entry.autoverify.last_failure.result));
            } else
                writeln("N/A");
        } else if(bbs.entry.autoverify.last_success) {
            writeln(localDateStr(bbs.entry.autoverify.last_success.on));
            writeln(encode_text(bbs.entry.autoverify.last_success.result));
        } else
            writeln("Success");
    } else
        writeln("No auto-verification possible");

    writeln('<tr style="background-color:#ffffff;"><td>' + encode_text(bbs.description.join("\r\n")));
    writeln('<td><td><td>');
	var addl_services=[];
    for(i=1; i<bbs.service.length; i++) {
        if(i && JSON.stringify(bbs.service[i]).toLowerCase() == JSON.stringify(bbs.service[i-1]).toLowerCase())
            continue;
        addl_services.push(bbs.service[i]);
    }
    writeln('<td>'); // style="border-collapse: collapse;">');
	if(addl_services.length) {
        writeln('<table style="width:100%;">');
        while(addl_services.length)
            writeln('<tr><td style="font-size:100%;">' + bbs_service(addl_services.shift()));
        writeln('</table>');
    }


//    if(bbs.service.length)
 //       writeln(bbs_service(bbs.service[0]));
//    if(bbs.capture)
//        writeln(format('<td rowspan="%u">', bbs.service.length));
//    else
   
	if(!bbs.entry.autoverify || !bbs.entry.autoverify.success) {
        write('<td>');
		if(bbs.entry.verified) 
			writeln("Last verified on " + localDateStr(bbs.entry.verified.on) + " by " + encode_text(bbs.entry.verified.by));
	}
	else if(bbs.preview) {
        write('<td>'); // style="text-align:left; border-collapse:collapse; border-style:none;">');
        bbs_preview(bbs);
    }
}

/* GENERATE SUMMARY TABLE */
writeln('<table style="width:100%;">');
writeln('<caption>');
writeln(format("List of Synchronet BBSes (%u systems) exported from ", list.length) + system.name.link("http://" + system.inet_addr) + " on " + Date());
writeln('<p></caption>');
writeln('<thead>');
writeln('<tr>');
writeln('<th style="width:25%;">BBS Name and Description</th>');
writeln('<th>Since</th>');
writeln('<th>Operators</th>');
writeln('<th>Location</th>');
writeln('<th>Terminal Services</th>');
writeln('<th style="width:1%;">Verification Results</th>');
writeln('</thead>');
writeln('<tbody>');
var i;
for(i in list)
    bbs_table_entry(list[i]);
writeln('</tbody>');
writeln('</table>');

writeln('<hr>');
writeln('<p style="font-size:smaller;">Generated in ' + system.secondstr(time()-start) + ' by ' + system.version_notice);
if(this.jsexec_revision_detail != undefined)
    writeln(this.jsexec_revision_detail);
writeln('sbbslist_html.js' + ' ' + REVISION);

writeln('</body></html>');
