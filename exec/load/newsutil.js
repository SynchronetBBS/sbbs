// newsutil.js

// Generates and parses USENET news headers 
// for use with newslink.js and nntpservice.js

// $Id$

RFC822HEADER = 0xb0	// from smbdefs.h

function write_news_header(hdr,writeln)
{
	/* Required header fields */
	writeln("To: " + hdr.to);
	writeln("Subject: " + hdr.subject);
	writeln("Message-ID: " + hdr.id);
	writeln("Date: " + hdr.date);

	/* Optional header fields */
	writeln("X-Comment-To: " + hdr.to);
	if(hdr.path!=undefined)
		writeln("Path: " + system.inetaddr + "!" + hdr.path);
	if(hdr.from_org!=undefined)
		writeln("Organization: " + hdr.from_org);
	if(hdr.newsgroups!=undefined)
		writeln("Newsgroups: " + hdr.newsgroups);
	if(hdr.replyto!=undefined)
		writeln("Reply-To: " + hdr.replyto);
	if(hdr.reply_id!=undefined)
		writeln("In-Reply-To: " + hdr.reply_id);
	if(hdr.references!=undefined)
		writeln("References: " + hdr.references);
	else if(hdr.reply_id!=undefined)
		writeln("References: " + hdr.reply_id);

	/* FidoNet header fields */
	if(hdr.ftn_area!=undefined)
		writeln("X-FTN-AREA: " + hdr.ftn_area);
	if(hdr.ftn_pid!=undefined)
		writeln("X-FTN-PID: " + hdr.ftn_pid);
	if(hdr.ftn_tid!=undefined)
		writeln("X-FTN-TID: " + hdr.ftn_tid);
	if(hdr.ftn_flags!=undefined)
		writeln("X-FTN-FLAGS: " + hdr.ftn_flags);
	if(hdr.ftn_msgid!=undefined)
		writeln("X-FTN-MSGID: " + hdr.ftn_msgid);
	if(hdr.ftn_reply!=undefined)
		writeln("X-FTN-REPLY: " + hdr.ftn_reply);

	var content_type;

	if(hdr.field_list!=undefined) {
		for(i in hdr.field_list) 
			if(hdr.field_list[i].type==RFC822HEADER) {
				if(hdr.field_list[i].data.toLowerCase().indexOf("content-type:")==0)
					content_type = hdr.field_list[i].data;
				writeln(hdr.field_list[i].data);
			}
	}
	if(content_type==undefined) {
		/* No content-type specified, so assume IBM code-page 437 (full ex-ASCII) */
		writeln("Content-Type: text/plain; charset=IBM437");
		writeln("Content-Transfer-Encoding: 8bit");
	}

}

function parse_news_header(hdr, line)
{
	/* Parse header lines */
	if((line.charAt(0)==' ' || line.charAt(0)=='\t') && hdr.field_list) {
		/* "folded" header field */
		hdr.field_list.push(
			{	type: RFC822HEADER, 
				data: line
			}
		);
		return;
	}

	if((sp=line.indexOf(':'))==-1)
		return;

	data=line.slice(sp+1);
	while(data.charAt(0)==' '	// trim prepended spaces
		|| data.charAt(0)=='\t')	
		data=data.slice(1);
	data=truncsp(data);			// trim trailing spaces

	line=line.substr(0,sp);
	line=truncsp(line);			// trim trailing spaces (support "field : data" syntax)

	switch(line.toLowerCase()) {
		case "to":
		case "apparently-to":
		case "x-comment-to":
			hdr.to = getName(data);
			hdr.to_net_addr = getEmail(data);
			break;
		case "newsgroups":
			hdr.newsgroups=data;
			break;
		case "path":
			hdr.path=data;
			break;
		case "from":
			hdr.from = getName(data);
			hdr.from_net_addr = getEmail(data);
			break;
		case "organization":
			hdr.from_org=data;
			break;
		case "reply-to":
			hdr.replyto_net_type=NET_INTERNET;
			hdr.replyto=data;
			break;
		case "in-reply-to":
			hdr.reply_id=data;
			break;
		case "date":
			hdr.date=data;
			break;
		case "subject":
			hdr.subject=data;
			break;
		case "message-id":
			hdr.id=data;
			break;
		case "references":
			hdr.references=data;
			if(!hdr.reply_id && data.length)
				hdr.reply_id=data.match(/(?:\S+\s)*(\S+)$/)[1];
			break;
		case "x-gateway":
			hdr.gateway=data;
			break;

		/* FidoNet header fields */
		case "x-ftn-pid":
			hdr.ftn_pid=data;
			break;
		case "x-ftn-tid":
			hdr.ftn_tid=data;
			break;
		case "x-ftn-area":
			hdr.ftn_area=data;
			break;
		case "x-ftn-flags":
			hdr.ftn_flags=data;
			break;
		case "x-ftn-msgid":
			hdr.ftn_msgid=data;
			break;
		case "x-ftn-reply":
			hdr.ftn_reply=data;
			break;

		default:
			if(hdr.field_list==undefined)
				hdr.field_list=new Array();
			hdr.field_list.push(
				{	type: RFC822HEADER, 
					data: line + ": " + data 
				}
			);
			break;
	}
}


//Michael J. Ryan - 2004-04-16 - tracker1(at)theroughnecks.net
// gets the name portion for the "to/from"
function getName(strIn) {
	var reName1 = /[^\"]*\"([^\"]*)\".*/	//quoted name
	var reName2 = /(\S[^<]+)\s+<.*/			//unquoted name
	var reName3 = /[^<]*<([^@>]).*/			//first part of email address
	var strName = strIn;
	if (reName1.test(strIn)) strName = strIn.replace(reName1,"$1");
	else if (reName2.test(strIn)) strName = strIn.replace(reName2,"$1");
	else if (reName3.test(strIn)) strName = strIn.replace(reName3,"$1");
	return strName; //original string
}

//Michael J. Ryan - 2004-04-16 - tracker1(at)theroughnecks.net
// gets the email portion for the "to/from"
function getEmail(strIn) {
	var reEmail1 = /[^<]*<([^>]+)>.*/
	var strEmail = strIn;
	log("getEmail(" + strEmail + ")");
	if (strIn.match(reEmail1)) return strIn.replace(reEmail1,"$1");
	return null;
}