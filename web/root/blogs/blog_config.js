// User editable settings...

// Internal code of the message base
var msg_code="GENERAL";

// Number of posts to show per page.
var max_posts=16;

// Minimum number of characters in an exerpt
var min_chars=400;

//
// Don't change stuff down here.
//

// User name of the blogger
var parameters=http_request.path_info.substr(1).split("/");
var poster=parameters[0];
var year=parseInt(parameters[1],10);
var month=parseInt(parameters[2],10);
var day=parseInt(parameters[3],10);
var msgid=parseInt(parameters[4],10);
var subject=parameters[5];
if(poster.indexOf("/")>=0) {
	poster=poster.substr(0,poster.indexOf("/"));
}

//var pnum=system.matchuser(poster);
//if(pnum==0) {
//	write("<html><head><title>Error</title></head><body>Error getting UserID for "+poster+"!</body></html>");
//	exit(1);
//}

var msgbase = new MsgBase(msg_code);
if(!msgbase.open()) {
	write("<html><head><title>Error</title></head><body>Error opening "+msg_code+"!</body></html>");
	exit(1);
}

if(!isNaN(msgid)) {
	if(subject==undefined || subject=="") {
		var hdr=msgbase.get_msg_header(parseInt(msgid));
		http_reply.status="301 Moved Permanently";
		http_reply.header["Location"]="http://"+http_request.host+http_request.virtual_path+poster+"/"+format("%04d",year)+"/"+format("%02d",month)+"/"+format("%02d",day)+"/"+msgid+"/"+clean_subject(hdr.subject);
		exit(0);
	}
}

function clean_subject(sub)
{
	sub=sub.replace(/[^a-zA-Z0-9]/g,"_");
	return(sub);
}
