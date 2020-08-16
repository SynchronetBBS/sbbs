// dumpobjs.ssjs

// $Id: dumpobjs.ssjs,v 1.3 2005/02/17 01:30:02 rswindell Exp $

// Used for debugging (and possibly documenting) the Synchronet web server's JS objects

writeln("<html><body>");
function dump(obj, name)
{
	var i;

	for(i in obj) {
		if(obj.length!=undefined)
			write(name +'['+ i +'] = ');
		else
			write(name +'.'+ i +' = ');
		write(obj[i] + "<br>");
		if(typeof(obj[i])=="object")
			dump(obj[i], name +'.'+ i);
	}
}

dump(server, "server"),             writeln("<br>");
dump(client, "client"),             writeln("<br>");
dump(http_request,"http_request"),  writeln("<br>");
dump(http_reply,"http_reply"),      writeln("<br>");

writeln("</body></html>");
