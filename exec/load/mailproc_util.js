// mailproc_util.js

// Utility functions for Synchronet external mail processors

// $Id$

load("sbbsdefs.js");
load("mailutil.js");	// mail_get_name() and mail_get_address()

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
		var match = msgtxt[i].match(/(\S+)\s*:\s*(.*)/);
		if(match)
			hdr[last_field=match[1].toLowerCase()]=match[2];
		else if(last_field)		// Folded header field
			hdr[last_field]+=msgtxt[i];
	}	
		
	return(hdr);
}

// Convert a parsed message header into Synchronet-compatible
function convert_msg_header(hdr)
{
	hdr.id				= hdr["message-id"]; 

	hdr.from_net_type	= NET_INTERNET;
	hdr.from_net_addr	= mail_get_address(hdr.from);
	hdr.from			= mail_get_name(hdr.from);

	hdr.to_net_type		= NET_INTERNET;
	hdr.to_net_addr		= mail_get_address(hdr.to);
	hdr.to				= mail_get_name(hdr.to);

	return(hdr);
}

// Parses the body portion of the complete message text into an array
function get_msg_body(msgtxt)
{
	var body = new Array();
	var hdr = true;

	for(i in msgtxt) {
		if(msgtxt[i].length==0)	{ // Header terminator
			hdr = false;
			continue;
		}
		if(!hdr)
			body.push(msgtxt[i]);
	}
	return(body);
}
		