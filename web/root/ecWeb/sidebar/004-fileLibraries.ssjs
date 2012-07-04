var ftp_url="ftp://";
if(user.number && !(user.security.restrictions&UFLAG_G))
	ftp_url += user.alias + ":" + user.security.password + "&#064;";
var host = http_request.host;
if(!host || !host.length)
	host = system.host_name;
var port = host.indexOf(':');
if(port>=0)
	host=host.slice(0,port);
ftp_url += host;
ftp_port=webIni.ftpPort;
var ftp_port;
if(ftp_port==undefined)
	ftp_port=21;
if(ftp_port!=21)
	ftp_url += ftp_port;
ftp_url +="/00index.html?$" + new Date().valueOf().toString(36);
print('<a class="link" href="'+encodeURI(ftp_url)+'">File Libraries</a>');
