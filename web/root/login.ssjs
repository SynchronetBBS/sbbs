if(user.number==0 || user.number==system.matchuser('Guest')) {
	http_reply.status='401 Permission Denied';
	http_reply.header["WWW-Authenticate"]='Basic realm="'+system.name+'"';
}
else {
// Note: A 302 here would mean the index would be displayed as "/login.ssjs"
// That is to say, it would display the index AS login.ssjs.
	// http_reply.status='302 Found';
	http_reply.status='307 Temporary Redirect';
}
http_reply.header.location='/index.ssjs';
http_reply.header.pragma='no-cache';
http_reply.header.expires='0';
http_reply.header['cache-control']='must-revalidate';

writeln('<html>');
writeln('<head>');
writeln('</head>');
writeln('<body>Logging in to <a href="/">'+system.name+'</a></body>');
writeln('</html>');
