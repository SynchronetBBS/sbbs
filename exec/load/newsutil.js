// newsutil.js

// Generates and parses USENET news headers 
// for use with newslink.js and nntpservice.js

// $Id: newsutil.js,v 1.34 2020/06/08 03:20:37 rswindell Exp $

require("mailutil.js", 'mail_get_name');
require("smbdefs.js", 'RFC822HEADER');

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
	if(hdr.ftn_charset!=undefined)
		writeln("X-FTN-CHRS: " + hdr.ftn_charset);

	var content_type;

	if(hdr.field_list!=undefined) {
		for(i in hdr.field_list) {
			if(hdr.field_list[i].type==RFC822HEADER) {
				if(hdr.field_list[i].data.toLowerCase().indexOf("content-type:")==0)
					content_type = hdr.field_list[i].data;
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
		case "x-ftn-chrs":
			hdr.ftn_charset=data;
			break;

		/* NNTP control messages */
		case "control":
			hdr.control=data;
			break;

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
 * Returns TRUE if hdr and/or body are modified
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
