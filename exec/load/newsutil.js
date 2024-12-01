// Generates and parses USENET news headers 
// for use with newslink.js and nntpservice.js

require("mailutil.js", 'mail_get_name');
require("smbdefs.js", 'RFC822HEADER');

const mimehdr = load("mimehdr.js");

function get_news_subject(hdr)
{
	if(hdr.field_list !== undefined)
		for(var i in hdr.field_list) {
			if(hdr.field_list[i].type == RFC822SUBJECT)
				return hdr.field_list[i].data;
		}
	return mimehdr.encode(hdr.subject);
}

function get_news_from(hdr)
{
	if(!hdr.from_net_type || !hdr.from_net_addr)	/* local message */
		return format("\"%s\" <%s@%s>"
			,mimehdr.encode(hdr.from)
			,hdr.from.replace(/ /g,".").toLowerCase()
			,system.inetaddr);
	if(!hdr.from_net_addr.length)
		return mimehdr.encode(hdr.from);
	if(hdr.from_net_addr.indexOf('@')!=-1) {
		if(hdr.from == hdr.from_net_addr)
			return format("<%s>", hdr.from_net_addr);
		return format("\"%s\" <%s>"
			,mimehdr.encode(hdr.from)
			,hdr.from_net_addr);
	}
	if(hdr.from_net_type == NET_FIDO)
		return format("\"%s\" (%s) <%s>"
			,mimehdr.encode(hdr.from)
			,hdr.from_net_addr
			,fidoaddr_to_emailaddr(hdr.from, hdr.from_net_addr));
	if(hdr.from_net_type == NET_QWK)
		return format("\"%s\" (%s) <%s!%s@%s>"
			,mimehdr.encode(hdr.from)
			,hdr.from_net_addr
			,hdr.from_net_addr
			,hdr.from.replace(/ /g,".")
			,system.inetaddr);
	return format("\"%s\" <%s@%s>"
			,mimehdr.encode(hdr.from)
			,hdr.from.replace(/ /g,".").toLowerCase()
			,hdr.from_net_addr);
}

function write_news_header(hdr,writeln)
{
	/* Required header fields */
	writeln("To: " + mimehdr.encode(hdr.to));
	writeln("Subject: " + get_news_subject(hdr));
	writeln("Message-ID: " + hdr.id);
	writeln("Date: " + hdr.date);

	/* Optional header fields */
	writeln("X-Comment-To: " + mimehdr.encode(hdr.to));
	if(hdr.path!=undefined)
		writeln("Path: " + system.inetaddr + "!" + hdr.path);
	if(hdr.from_org!=undefined)
		writeln("Organization: " + hdr.from_org);
	if(hdr.newsgroups!=undefined)
		writeln("Newsgroups: " + hdr.newsgroups);
	if(hdr.replyto!=undefined)
		writeln("Reply-To: " + hdr.replyto);

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
	if(hdr.ftn_charset!=undefined)
		writeln("X-FTN-CHRS: " + hdr.ftn_charset);

	var content_type;
	var user_agent;
	var references;

	if(hdr.field_list!=undefined) {
		for(var i in hdr.field_list) {
			if(hdr.field_list[i].type==RFC822HEADER) {
				if(hdr.field_list[i].data.toLowerCase().indexOf("content-type:")==0)
					content_type = hdr.field_list[i].data;
				if(hdr.field_list[i].data.toLowerCase().indexOf("user-agent:")==0)
					user_agent = hdr.field_list[i].data;
				if(hdr.field_list[i].data.toLowerCase().indexOf("references:")==0)
					references = hdr.field_list[i].data;
				writeln(hdr.field_list[i].data);
			} else if(hdr.field_list[i].type==FIDOCTRL) {
				writeln("X-FTN-Kludge: " + hdr.field_list[i].data);
			} else if(hdr.field_list[i].type==FIDOSEENBY) {
				writeln("X-FTN-SEEN-BY: " + hdr.field_list[i].data);
			} else if(hdr.field_list[i].type==FIDOPATH) {
				writeln("X-FTN-PATH: " + hdr.field_list[i].data);
			}
		}
	}

	if(references === undefined && hdr.reply_id !== undefined)
		writeln("References: " + hdr.reply_id);
	if(user_agent === undefined && hdr.editor !== undefined)
		writeln("User-Agent: " + hdr.editor);

	if(content_type==undefined) {
		var charset = hdr.text_charset;
		if(!charset) {
			if(hdr.is_utf8)
				charset = "UTF-8";
			else switch(hdr.ftn_charset) {
				case "ASCII 1":
					charset = "US-ASCII";
					break;
				case "CP866 2":
					charset = "KOI8-R";
					break;
				case "LATIN-1 2":
					charset = "ISO-8859-1";
					break;
				default:
					charset = "IBM437";
					break;
			}
		}
		writeln("Content-Type: text/plain; charset=" + charset);
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
			hdr.to = mail_get_name(data);
			if((hdr.to_net_addr = mail_get_address(data)) != null)
				hdr.to_net_type = NET_INTERNET;
			break;
		case "newsgroups":
			hdr.newsgroups=data;
			break;
		case "path":
			hdr.path=data;
			break;
		case "from":
			hdr.from = mail_get_name(data);
			if((hdr.from_net_addr = mail_get_address(data)) != null)
				hdr.from_net_type = NET_INTERNET;
			break;
		case "organization":
			hdr.from_org=data;
			break;
		case "reply-to":
			hdr.replyto_net_type=NET_INTERNET;
			hdr.replyto=data;
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
		case "user-agent":
			hdr.editor=data;
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
		case "x-ftn-chrs":
			hdr.ftn_charset=data;
			break;

		/* NNTP control messages */
		case "control":
			hdr.control=data;
			break;

		case "references":
			if(!hdr.reply_id && data.length)
				hdr.reply_id = data.match(/<[^\<]*>$/);
			// fall-through
		default:
			if(hdr.field_list==undefined)
				hdr.field_list=[];
			if(hdr.extra_headers==undefined)
				hdr.extra_headers={};
			hdr.extra_headers[line.toLowerCase()]=
				{	type: RFC822HEADER, 
					get data() { return(this.hdr_name+': '+this.hdr_data); },
					set data(v) { 
						var m=v.split(/:\s*/,2);
						if(m.length==1)
							this.hdr_data=v;
						else {
							this.hdr_name=m[0];
							this.hdr_data=m[1];
						}
					},
					hdr_name: line,
					hdr_data: data,
					toString: function() { return this.hdr_data; }
				};
			hdr.field_list.push(hdr.extra_headers[line.toLowerCase()]);
			break;
	}
}

/*
 * Returns decoded body text
 */
function decode_news_body(hdr, body)
{
	if(hdr.extra_headers && hdr.extra_headers["content-transfer-encoding"]) {
		switch(hdr.extra_headers["content-transfer-encoding"].hdr_data.toLowerCase()) {
			case '7bit':
			case '8bit':
			case 'binary':
				break;
			case 'quoted-printable':
				/* Remove trailing whitespace from lines */
				body=body.replace(/\s+\x0d\x0a/g,'\x0d\x0a');

				/* Remove "soft" line breaks */
				body=body.replace(/=\x0d\x0a/g,'');

				/* Collapse =XX encoded bytes */
				body=body.replace(/=([0-9A-F][0-9A-F])/g,function(str, p1) { return(ascii(parseInt(p1,16))); });
				hdr.extra_headers["content-transfer-encoding"].hdr_data='8bit';
				break;
			case 'base64':
				/* Remove non-base64 bytes */
				body=body.replace(/[^ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+\/=]/g,'');
				body=base64_decode(body);
				hdr.extra_headers["content-transfer-encoding"].hdr_data='8bit';
				break;
		}
	}
	// No content-type specified? Auto-detect UTF-8 and set FTN charset field accordingly
	if((!hdr.extra_headers || !hdr.extra_headers["content-type"]) && !str_is_ascii(body) && str_is_utf8(body))
		hdr.ftn_charset = "UTF-8 4";
	return(body);
}
