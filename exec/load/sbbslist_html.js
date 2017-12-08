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
    list.sort(lib.verify_compare);
}

writeln('<!DOCTYPE html>');
writeln('<html>');
writeln('<head>');
writeln("<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>");
writeln('<style media="screen" type="text/css">');
writeln('body { font-family:arial, sans-serif; }');
writeln('th { color:#FFFFFF; background-color:#000000;}');
writeln('td { vertical-align:top; font-size:10pt; }');
writeln('table { width:100%; }');
writeln('a:link { text-decoration:none; font-weight:bold; }');
writeln('a:visited { text-decoration:none; }');
writeln('a:hover { text-decoration:underline; }');
writeln('a:active { text-decoration:underline; }');
writeln('.row { background-color:#eeeeee; }');
writeln('.bbsName { text-align:center; font-weight:bold; font-size:larger; }');
writeln('pre { cursor:zoom-in; color: #a8a8a8; background-color:black; font-family:Courier,Prestige,monospace; font-size:5pt }');
writeln('.zoomedIn { font-size:large; cursor:zoom-out; }');
writeln('.zoomedOut { font-size:5pt; cusor:zoom-in; }');
//writeln('.overlay { position:relative; left:0px; top:0px; width:100%; height;100%; background-color:rgba(0,0,0,0.5); }');
writeln('</style>');
writeln('<script>');
writeln('function onClick(obj) {');
	writeln('if(obj.className=="zoomedIn") obj.className="zoomedOut", obj.title="Click to Zoom-In";');
	writeln('else obj.className="zoomedIn", obj.title="Click to Zoom-Out";');
	writeln('scrollBy(1000,0);');
writeln('}');
writeln('</script>');
writeln('<title>Synchronet BBS List</title>');
writeln('</head>');

load("portdefs.js");
load("graphic.js");

//writeln('<body><font face="Arial" size="-1">');

list=list.filter(function(obj) { return obj.software && obj.software.substr(0,10).toLowerCase() == "synchronet"; });


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
    return encode_text(service.address).link(encodeURI(uri)) + " ("+encode_text(service.protocol)+")";
}

function bbs_sysop(sysop)
{
    if(sysop.email && sysop.email.length)
        return format('<a href="&#109;&#97;&#105;&#108;&#116;&#111;&#58;%s">%s</a>', encodeURI(sysop.email), encode_text(sysop.name));
    return(encode_text(sysop.name));
}

function bbs_preview(num, bbs)
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
    write('<pre title="Click to Zoom-In" onclick="onClick(this)">'); // onmouseout="this.className=\'zoomOut\'">');
	var html = graphic.HTML;
	/* HTML Optimization: */
	/* Remove black background color (black is the default */
	html = html.replace(/background-color: black;/g, '');
	/* Remove grey foreground color (grey is the default) */
	html = html.replace(/\"color: #a8a8a8;/g, '"');
	/* Remove empty style attributes */
	html = html.replace(/\ style=\" \"/g, '');
	/* Remoe empty span tags */
	html = html.replace(/<span>([^<]*)<\/span>/g,'$1');
	write(html);
    writeln('</pre>');
}

function localDateStr(date)
{
	var date=new Date(date);

	return format("%u-%02u-%02u", date.getFullYear(), date.getMonth()+1, date.getDate());
}

function other_service(service, address)
{
    switch(service) {
        case "ftp":
        case "gopher":
            return service.link(service + "://" + address);
        case "nntp":
            return service.link("news://" + address);
    }
    return service;
}

function bbs_table_entry(num, bbs)
{
    var i;

    writeln('<tr><td><table>');
    /* Name: */
    writeln('<tr class="row"><td class="bbsName">');
    var uri = bbs.web_site;
    if(uri && uri.length) {
        if(uri.indexOf('://')<1)
            uri = "http://" + bbs.web_site;
        writeln(encode_text(bbs.name).link(encodeURI(uri)));
    } else
        writeln(encode_text(bbs.name));
    writeln('</b>');
    writeln('<tr><td>' + encode_text(bbs.description.join("\r\n")));
    writeln('</table>');

    /* Since: */
    writeln('<td ><table><tr class="row">');
    writeln('<td style="text-align:center;">');
    if(bbs.first_online)
        writeln(encode_text(bbs.first_online.substring(0,4)));
    writeln('</table>');

    /* Operators: */
    writeln('<td><table>');
    for(i in bbs.sysop)
        writeln('<tr class="row"><td>' + bbs_sysop(bbs.sysop[i]));
    if(!bbs.sysop.length)
        writeln('<tr class="row"><td>');
    writeln('</table>');

    /* Location: */
    writeln('<td><table>');
    writeln('<tr class="row"><td>' + encode_text(bbs.location));
    writeln('</table>');

    /* Services */
    writeln('<td><table>');
    for(i=0; i<bbs.service.length; i++) {
        if(i && JSON.stringify(bbs.service[i]).toLowerCase() == JSON.stringify(bbs.service[i-1]).toLowerCase())
            continue;
        writeln('<tr class="row"><td>' + bbs_service(bbs.service[i]));
    }
    writeln('</table>');

    /* Networks */
    writeln('<td><table>');
    for(i in bbs.network) {
        writeln('<tr class="row"><td>');
		if(bbs.network[i].address && bbs.network[i].address.length)
			writeln(format('<div title="%s address: %s">'
				,encode_text(bbs.network[i].name), encode_text(bbs.network[i].address)));
		else
			writeln('<div>');
		writeln(encode_text(bbs.network[i].name) + '</div>');
	}
    if(!bbs.network.length)
        writeln('<tr class="row"><td>');
    writeln('</table>');

    /* Verification results */
    writeln('<td><table><tr class="row"><td>');
    if(bbs.entry.autoverify) {
        if(!bbs.entry.autoverify.success) {
            if(bbs.entry.autoverify.last_failure) {
                writeln(localDateStr(bbs.entry.autoverify.last_failure.on));
                writeln(encode_text(bbs.entry.autoverify.last_failure.result));
            } else
                writeln("N/A");
        } else if(bbs.entry.autoverify.last_success) {
            writeln(format("<div title='IP address: %s'>", bbs.entry.autoverify.last_success.ip_address));
            writeln(localDateStr(bbs.entry.autoverify.last_success.on));
            writeln(encode_text(bbs.entry.autoverify.last_success.result));
            writeln("</div>");
        } else
            writeln("Success");
    } else
        writeln("No auto-verification possible");

//        writeln('<table style="width:100%;">');
//        while(addl_services.length)
//            writeln('<tr><td style="font-size:100%;">' + bbs_service(addl_services.shift()));
//        writeln('</table>');


//    if(bbs.service.length)
 //       writeln(bbs_service(bbs.service[0]));
//    if(bbs.capture)
//        writeln(format('<td rowspan="%u">', bbs.service.length));
//    else
   
	if(!bbs.entry.autoverify || !bbs.entry.autoverify.success) {
        write('<tr><td>');
		if(bbs.entry.verified) 
			writeln("Last verified on " + localDateStr(bbs.entry.verified.on) + " by " + encode_text(bbs.entry.verified.by));
	}
	else {
        if(bbs.preview) {
            write('<tr><td>'); // style="border-style:outset;">');
      //     	writeln('<div>'); // onclick="this.className=\'overlay\'">'); 
			bbs_preview(num, bbs);
	//		writeln('</div>');
        }
        if(bbs.entry.autoverify.last_success.other_services 
            && bbs.entry.autoverify.last_success.other_services.tcp
            && bbs.entry.autoverify.last_success.other_services.tcp.length) {
            write('<tr><td>Also: ');
            for(i in bbs.entry.autoverify.last_success.other_services.tcp)
                writeln('[' + other_service(bbs.entry.autoverify.last_success.other_services.tcp[i], bbs.entry.autoverify.last_success.service.address) + ']');
        }
    }
    writeln('</table>');
}

/* GENERATE SUMMARY TABLE */
writeln('<table>');
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
writeln('<th>Networks</th>');
writeln('<th style="width:1%;">Verification Results</th>');
writeln('</thead>');
writeln('<tbody>');
var i;
for(i in list)
    bbs_table_entry(i, list[i]);
writeln('</tbody>');
writeln('</table>');

writeln('<hr>');
writeln('<p style="font-size:smaller;">Generated in ' + system.secondstr(time()-start) + ' by ' + system.version_notice);
if(this.jsexec_revision_detail != undefined)
    writeln(this.jsexec_revision_detail);
writeln('sbbslist_html.js' + ' ' + REVISION);

writeln('</body></html>');
