// mailproc_util.js

// Utility functions for Synchronet external mail processors

// $Id: mailproc_util.js,v 1.13 2019/05/02 21:23:04 rswindell Exp $

require("sbbsdefs.js", 'NET_INTERNET');
require("smbdefs.js", 'RFC822HEADER');
require("mailutil.js", 'mail_get_name');

// Parses raw RFC822-formatted messages for use with SMTP Mail Processors
// Returns an array of header fields parsed from the msgtxt
// msgtxt is an array of lines from the source (RFC822) message text
function parse_msg_header(msgtxt)
{
	var last_field;
	var hdr={};

	for(i in msgtxt) {
		if(msgtxt[i].length==0)	// Header terminator
			break;
		var match = msgtxt[i].match(/^(\S+)\s*:\s*(.*)/);
		if(match)
			hdr[last_field=match[1].toLowerCase()]=match[2];
		else if(last_field)		// Folded header field
			hdr[last_field]+=msgtxt[i];
	}	
		
	return(hdr);
}

// Convert a parsed RFC822 message header field array into a Synchronet-compatible header object
function convert_msg_header(hdr_array)
{
	var hdr = new Object;

	for(f in hdr_array) {
		var data = hdr_array[f];
		switch(f) {
			case "to":
			case "apparently-to":
			case "x-comment-to":
				hdr.to = mail_get_name(data);
				hdr.to_net_addr = mail_get_address(data);
				hdr.to_net_type		= NET_INTERNET;
				break;
			case "from":
				hdr.from = mail_get_name(data);
				hdr.from_net_addr = mail_get_address(data);
				hdr.from_net_type	= NET_INTERNET;
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
						data: f + ": " + data 
					}
				);
				break;
		}
	}

	return(hdr);
}

// Parses the body portion of the complete message text into an array
function get_msg_body(msgtxt)
{
	var body = new Array();
	var hdr = true;

	for(i in msgtxt) {
		if(hdr && msgtxt[i].length==0)	{ // Header terminator
			hdr = false;
			continue;
		}
		if(!hdr)
			body.push(msgtxt[i]);
	}
	return(body);
}
		
