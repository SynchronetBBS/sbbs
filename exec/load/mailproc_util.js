// mailproc_util.js

// Utility functions for Synchronet external mail processors

// $Id$

// Parses raw RFC822-formatted messages for use with SMTP Mail Processors
// Returns an array of header fields parsed from the msgtxt
// msgtxt is an array of lines from the source (RFC822) message text
function parse_msg_header(msgtxt)
{
	var hdr={};

	for(i in msgtxt) {
		if(msgtxt[i].length==0)	/* Header delimiter */
			break;
		var match = msgtxt[i].match(/\s*(\S+)\s*:\s*(.*)/);
		hdr[match[0]]=match[1];
	}	
		
	return(hdr);
}