/* $Id: mime_decode.ssjs,v 1.22 2009/05/06 21:54:05 deuce Exp $ */

function count_attachments(hdr, body)
{
	var Message=new Array;
	var CT;
	var TE;
	var attach=0;

	if(hdr==undefined || body==undefined || hdr.field_list==undefined)
		return(0);
	for(head in hdr.field_list) {
		if(hdr.field_list[head].data.search(/content-type:/i)!=-1) {
			CT=hdr.field_list[head].data;
		}
		else if(hdr.field_list[head].data.search(/content-transfer-encoding:/i)!=-1) {
			TE=hdr.field_list[head].data;
		}
	}

	if(CT==undefined)
		return(0);

	if(CT.search(/multipart\/[^\s;]*/i)!=-1) {
		var bound=CT.match(/;[\s\r\n]*boundary="{0,1}([^";\r\n]*)"{0,1}/i);
		if(bound==undefined)
			return(attach);
		bound[1]=regex_escape(bound[1]);
		re=new RegExp ("--"+bound[1]+"-{0,2}","");
		msgbits=body.split(re);
		/* Search for attachments/inlined */
		for(bit in msgbits) {
			var pieces=msgbits[bit].split(/\r?\n\r?\n/);
			var disp=pieces[0].match(/content-disposition:\s+(?:attachment|inline)[;\s]*filename="?([^";\r\n]*)"?/i);
			if(disp!=undefined) {
				attach++;
			}
		}
	}

	return(attach);
}

function mime_decode(hdr, body)
{
	var Message=new Array;
	var CT;
	var TE;
	var undef;

	if(hdr==undefined || body==undefined || hdr.field_list==undefined) {
		Message.type="plain";
		Message.body=decode_body(TE,undef,body);
		return(Message);
	}
	for(head in hdr.field_list) {
		if(hdr.field_list[head].data.search(/content-type:/i)!=-1) {
			CT=hdr.field_list[head].data;
		}
		else if(hdr.field_list[head].data.search(/content-transfer-encoding:/i)!=-1) {
			TE=hdr.field_list[head].data;
		}
	}
	if(CT==undefined) {
		Message.type="plain";
		Message.body=decode_body(TE,undef,body);
		return(Message);
	}
	if(CT.search(/multipart\/[^\s;]*/i)!=-1) {
		var bound=CT.match(/;[\s\r\n]*boundary="{0,1}([^";\r\n]*)"{0,1}/i);
		if(bound!=undefined) {
			bound[1]=regex_escape(bound[1]);
			re=new RegExp ("--"+bound[1]+"-{0,2}");
			msgbits=body.split(re);
			/* Search for attachments/inlined */
			for(bit in msgbits) {
				var pieces=msgbits[bit].split(/\r?\n\r?\n/);
				var disp=pieces[0].match(/content-disposition:\s+(?:attachment|inline)[;\s]*filename="?([^";\r\n]*)"?/i);
				if(disp!=undefined) {
					/* Attachment */
					if(Message.attachments==undefined)
						Message.attachments=new Array;
					Message.attachments.push(disp[1]);
				}
				disp=pieces[0].match(/content-id:\s+\<?([^\<\>;\r\n]*)\>?/i);
				if(disp!=undefined) {
					/* Inline Attachment */
					if(Message.inlines==undefined)
						Message.inlines=new Array;
					Message.inlines.push(disp[1]);
				}
			}
			/* Search for HTML encoded bit */
			for(bit in msgbits) {
				var pieces=msgbits[bit].split(/\r?\n\r?\n/);
				var pheads=pieces[0];
				if(pheads==undefined)
					continue;
				var content=pieces.slice(1).join('');
				if(content==undefined)
					continue;
				if(pheads.search(/content-type: text\/html/i)!=-1) {
					Message.body=decode_body(TE,pheads,content);
					if(Message.inlines!=undefined) {
						for(il in Message.inlines) {
							var path=http_request.virtual_path;
							var basepath=path.match(/^(.*\/)[^\/]*$/);
							re=new RegExp("cid:("+regex_escape(Message.inlines[il])+")","ig");
							Message.body=Message.body.replace(re,basepath[1]+"inline.ssjs/"+template.group.number+"/"+template.sub.code+"/"+hdr.number+"/$1");
						}
					}
					Message.type="html";
					return(Message);
				}
			}
			/* Search for plaintext bit */
			for(bit in msgbits) {
				var pieces=msgbits[bit].split(/\r?\n\r?\n/);
				var pheads=pieces[0];
				var content=pieces.slice(1).join('');
				if(content==undefined)
					continue;
				if(pheads.search(/content-type: text\/plain/i)!=-1) {
					Message.body=decode_body(TE,pheads,content);
					Message.type="plain";
					return(Message);
				}
			}
		}
	}

	if(CT.search(/text\/html/i)!=-1) {
		Message.type="html";
		Message.body=decode_body(TE,undef,body);
		return(Message);
	}

	Message.type="plain";
	Message.body=body;
	return(Message);
}

function decode_body(TE, heads, body)
{
	var tmp;

	if(heads!=undefined && heads != "") {
		tmp=heads.match(/content-transfer-encoding: ([^;\r\n]*)/i);
		if(tmp!=undefined)
			tmp=tmp[1];
		else {
			if(TE!=undefined) {
				tmp=TE.match(/content-transfer-encoding: ([^;\r\n]*)/i);
				if(tmp!=undefined)
					tmp=tmp[1];
			}
		}
	}
	else {
		if(TE!=undefined) {
			tmp=TE.match(/content-transfer-encoding: ([^;\r\n]*)/i);
			if(tmp!=undefined)
				tmp=tmp[1];
		}
		else
			tmp="";
	}
	
	if(tmp==undefined)
		tmp="";
	if(tmp.search(/quoted-printable/i)!=-1) {
		body=body.replace(/\=(\r{0,1}\n)/g,"$1");
		body=body.replace(/\=([A-F0-9]{2})/ig,function (str,p1,offset,s)
			{
				var i=parseInt(p1,16);
				if(i==NaN || i==undefined)
					return('='+p1);
				return ascii(i);
			}
		);
		return body;
	}
	if(tmp.search(/base64/i)!=-1) {
		body=body.replace(/[^A-Za-z0-9\+\/\=]/g,'');
		return base64_decode(body);
	}

	return body;
}

function mime_get_attach(hdr, body, filename)
{
	var Message=new Array;
	var CT;
	var TE;
	var undef;

	for(head in hdr.field_list) {
		if(hdr.field_list[head].data.search(/content-type:/i)!=-1) {
			CT=hdr.field_list[head].data;
		}
		else if(hdr.field_list[head].data.search(/content-transfer-encoding:/i)!=-1) {
			TE=hdr.field_list[head].data;
		}
	}
	if(CT==undefined) {
		return(undefined);
	}
	if(CT.search(/multipart\/[^\s;]*/i)!=-1) {
		var bound=CT.match(/;[\s\r\n]*boundary="{0,1}([^";\r\n]*)"{0,1}/i);
		if(bound==undefined)
			return(undefined);
		bound[1]=regex_escape(bound[1]);
		re=new RegExp ("--"+bound[1]+"-{0,2}");
		msgbits=body.split(re);
		/* Search for attachments */
		for(bit in msgbits) {
			var pieces=msgbits[bit].split(/\r+\n\r+\n/);
			var disp=pieces[0].match(/content-disposition:\s+(?:attachment|inline)[;\s]*filename="?([^";\r\n]*)"?/i);
			if(disp==undefined)
				continue;
			if(disp[1]==filename) {
				var contyp=pieces[0].match(/content-type:\s*([^\r\n]*)/i);
				var content=pieces.slice(1).join('');
				if(contyp!=undefined && contyp[0]!=undefined)
					Message.content_type=contyp[1];
				Message.body=decode_body(undefined,pieces[0],content);
				return(Message);
			}
		}
	}
	return(undefined);
}

function mime_get_cid_attach(hdr, body, cid)
{
	var Message=new Array;
	var CT;
	var TE;
	var undef;

	for(head in hdr.field_list) {
		if(hdr.field_list[head].data.search(/content-type:/i)!=-1) {
			CT=hdr.field_list[head].data;
		}
		else if(hdr.field_list[head].data.search(/content-transfer-encoding:/i)!=-1) {
			TE=hdr.field_list[head].data;
		}
	}
	if(CT==undefined) {
		return(undefined);
	}
	if(CT.search(/multipart\/[^\s;]*/i)!=-1) {
		var bound=CT.match(/;[\s\r\n]*boundary="{0,1}([^";\r\n]*)"{0,1}/i);
		if(bound==undefined)
			return(undefined);
		bound[1]=regex_escape(bound[1]);
		re=new RegExp ("--"+bound[1]+"-{0,2}");
		msgbits=body.split(re);
		/* Search for attachments */
		for(bit in msgbits) {
			var pieces=msgbits[bit].split(/\r?\n\r?\n/);
			var disp=pieces[0].match(/content-id:\s+<?([^\<\>;\r\n]*)>?/i);
			if(disp==undefined)
				continue;
			if(disp[1]==cid) {
				var contyp=pieces[0].match(/content-type:\s*([^\r\n]*)/i);
				var content=pieces.slice(1).join('');
				if(contyp!=undefined && contyp[0]!=undefined)
					Message.content_type=contyp[1];
				Message.body=decode_body(undefined,pieces[0],content);
				return(Message);
			}
		}
	}
	return(undefined);
}
