if(user.number==0) {
	http_reply.status='401 Premission Denied';
	http_reply.header["WWW-Authenticate"]='Basic realm="'+system.name+'"';
}
writeln('<html>');
writeln('<head>');
writeln('<meta http-equiv="Pragma" content="no-cache">');
writeln('<meta http-equiv="expires" content="0">');
writeln('<meta http-equiv="refresh" content="0; URL=../index.ssjs?login=' 
		+ new Date().valueOf().toString(36) + '">');
writeln('</head>');
writeln('<body>Logging in...</body>');
writeln('</html>');

