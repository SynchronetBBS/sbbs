/* $Id: sbbslist_html.js,v 1.12 2020/08/01 22:07:25 rswindell Exp $ */
// vi: tabstop=4

var REVISION = "$Revision: 1.12 $".split(' ')[1];

var start=time();

var query_string;

var lib = argv[0];
var list = argv[1];
var query_string=argv[2];

if(lib == null || typeof lib != "object")
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

list=list.filter(function(obj) { return (obj.software && obj.software.substr(0,10).toLowerCase() == "synchronet") ||
	(obj.entry && obj.entry.autoverify && obj.entry.autoverify.success && obj.entry.autoverify.last_success.result.substr(0,14) == "Synchronet BBS"); });

writeln('<h1 style="text-align: center;"><i>' + 'Synchronet'.link('http://www.synchro.net') + ' BBS List</i></h1>');

if(false) {
	var synch_ansi="";
	var f=new File(system.text_dir + "synch.ans");
	if(f.open("rb")) {
    		synch_ansi=f.read();
    		f.close();
	}

	writeln('<table align="center"><tr><td><pre style="background-color:black;">'
        	+html_encode(synch_ansi, true, false, true, true) //.replace(/background-color: black;/g, 'background-color: lightgrey')
        	+'</pre></table>');
}

/**
 * Convert a string to HTML entities
 */
String.prototype.toHtmlEntities = function() {
    return this.replace(/./gm, function(s) {
        return "&#" + s.charCodeAt(0) + ";";
    });
};

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
	if(!service.address || !service.protocol)
		return "";
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
	var desc = service.description;
	if(!desc)
		desc = '';
	var prot = service.protocol;
	if(!prot)
		prot = '';
	else
		prot = " ("+encode_text(prot)+") ";
    return encode_text(service.address).link(encodeURI(uri)) + prot + desc;
}

function bbs_sysop(sysop)
{
    if(sysop.email && sysop.email.length)
        return format('<a href="&#109;&#97;&#105;&#108;&#116;&#111;&#58;%s">%s</a>', sysop.email.toHtmlEntities(), encode_text(sysop.name));
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
	var bin = lib.decode_preview(bbs.preview);
	if(!bin || !bin.length)
		return false;
	graphic.BIN = bin;
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
    writeln(format('<tr class="row"><td class="bbsName"><a name="%s"/></a>'
		, encodeURI(bbs.name.toLowerCase())));
    var uri = bbs.web_site;
    if(uri && uri.length) {
        if(uri.indexOf('://')<1)
            uri = "http://" + bbs.web_site;
        writeln(encode_text(bbs.name).link(encodeURI(uri)));
    } else
        writeln(encode_text(bbs.name));
    writeln('<tr><td>' + encode_text(bbs.description.join(" ")));
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
writeln('<p>');
writeln(format("Download a %s compatible list file %s"
	,"SyncTERM".link("http://syncterm.net")
	,"here".link("http://synchro.net/syncterm.lst")));
writeln("or " + ("Jump to statistics".link("#stats")));
writeln('<p>');
writeln('</caption>');
writeln('<thead>');
writeln('<tr>');
//writeln('<th style="width:25%;">BBS Name and Description</th>');
writeln('<th>BBS</th>');
writeln('<th>Since</th>');
writeln('<th>Operators</th>');
writeln('<th>Location</th>');
writeln('<th>Terminal Services</th>');
writeln('<th>Networks</th>');
writeln('<th style="width:1%;">Verification Results</th>');
writeln('</thead>');
writeln('<tbody>');

var stats = {
    version: {},
    os: {},
	network: {}
};

var i;
for(i in list) {
	var bbs = list[i];
	if (bbs.entry
		&& bbs.entry.autoverify
		&& bbs.entry.autoverify.last_success
		&& bbs.entry.autoverify.last_success.result) {
		ver = bbs.entry.autoverify.last_success.result;
		var x = ver.indexOf("Version ");
		if (x > 0) {
			var num = ver.substring(x + 8);
			stats.version[num] = (stats.version[num] || 0) + 1;
		}
		const prefix = "Synchronet BBS for ";
		x = ver.indexOf(prefix);
		if (x == 0) {
			var os = ver.substring(prefix.length);
			x = os.indexOf(' ');
			if (x > 0) {
				os = os.slice(0, x);
				stats.os[os] = (stats.os[os] || 0) + 1;
			}
		}
	}
	for (var n = 0; n < bbs.network.length; ++n) {
		var net = bbs.network[n].name.replace('-','').replace('_','').toUpperCase();
		stats.network[net] = (stats.network[net] || 0) + 1;
	}
    bbs_table_entry(i, bbs);
}

function getProcessedStats(dataObj, percentThreshold) {
    var total = 0;
    var rawArray = [];

    // Calculate total and convert to array for sorting
    for (var key in dataObj) {
        total += dataObj[key];
        rawArray.push({ label: key, value: dataObj[key] });
    }

    // Sort descending by value
    rawArray.sort(function (a, b) { return b.value - a.value });

    var processed = { labels: [], values: [] };
    var otherCount = 0;

    rawArray.forEach(function(item) {
        if ((item.value / total) < percentThreshold) {
            otherCount += item.value;
        } else {
            processed.labels.push(item.label);
            processed.values.push(item.value);
        }
    });

    if (otherCount > 0) {
        processed.labels.push("Other");
        processed.values.push(otherCount);
    }

    return processed;
}

stats.version = getProcessedStats(stats.version, 0.00);
stats.os = getProcessedStats(stats.os, 0.00);
stats.network = getProcessedStats(stats.network, 0.01);

writeln('</tbody>');
writeln('</table>');
writeln('<hr>');
writeln('<h1 id="stats" align="center">Statistics</h1>');

writeln('<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>');
writeln('<div style="display: flex; flex-wrap: wrap; justify-content: space-around;">');
writeln('  <div style="width: 400px;"><div align="center"><h2><i>Synchronet Versions</i></div><canvas id="verChart"></canvas></div>');
writeln('  <div style="width: 400px;"><div align="center"><h2><i>Operating Systems</i></div><canvas id="osChart"></canvas></div>');
writeln('  <div style="width: 400px;"><div align="center"><h2><i>Message Networks</i></div><canvas id="netChart"></canvas></div>');
writeln('</div>');
writeln('<script>');
writeln('const vData = ' + JSON.stringify(stats.version) + ';');
writeln('const oData = ' + JSON.stringify(stats.os) + ';');
writeln('const nData = ' + JSON.stringify(stats.network) + ';');

// Helper to render pie charts
writeln('function renderPie(id, title, processed) {');
writeln('    const ctx = document.getElementById(id).getContext("2d");');
writeln('    const colors = processed.labels.map((l, i) => ');
writeln('        l === "Other" ? "#999999" : "hsl(" + (i * (360 / processed.labels.length)) + ", 70%, 60%)"');
writeln('    );');
writeln('    new Chart(ctx, {');
writeln('        type: "doughnut",');
writeln('        data: {');
writeln('            labels: processed.labels,');
writeln('            datasets: [{');
writeln('                data: processed.values,');
writeln('                backgroundColor: colors');
writeln('            }]');
writeln('        },');
writeln('        options: {');
writeln('            responsive: true,');
writeln('            plugins: {');
//writeln('                title: { display: true, text: title },');
writeln('                tooltip: {');
writeln('                    callbacks: {');
writeln('                        label: function(ctx) {');
writeln('                            let val = ctx.raw;');
writeln('                            let sum = ctx.dataset.data.reduce((a, b) => a + b, 0);');
writeln('                            let perc = ((val / sum) * 100).toFixed(1) + "%";');
writeln('                            return title.split(" ").pop() + ": " + val + " (" + perc + ")";');
writeln('                        }');
writeln('                    }');
writeln('                }');
writeln('            }');
writeln('        }');
writeln('    });');
writeln('}');

writeln('renderPie("verChart", "Synchronet Versions", vData);');
writeln('renderPie("osChart", "Operating Systems", oData);');
writeln('renderPie("netChart", "Message Network Nodes", nData);');

writeln('</script>');

writeln('<hr>');
writeln('<p style="font-size:smaller;">Generated in ' + system.secondstr(time()-start) + ' by ' + system.version_notice);
if(this.jsexec_revision_detail != undefined)
    writeln(this.jsexec_revision_detail);
writeln('sbbslist_html.js' + ' ' + REVISION);

writeln('</body></html>');
