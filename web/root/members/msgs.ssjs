// Configuration
var max_subs=10;
var max_groups=10;
var max_messages=10;
/* Remove trailing filename from path and add '/templates' to the end */
var template_dir=http_request.real_path;
var last=template_dir.lastIndexOf("/");
if(template_dir.lastIndexOf("\\") > last) {
	last=template_dir.lastIndexOf("\\");
}
if(last>0) {
	template_dir=template_dir.substr(0,last+1);
}
template_dir += './templates';

// Do not change below this line
load("sbbsdefs.js");
http_reply.header["Pragma"]="no-cache";
http_reply.header["Expires"]="0";

var grp=http_request.query.msg_grp;
if(grp=='' || grp==null)
	grp=undefined;
var g=parseInt(http_request.query.msg_grp);
// if(g==0)
//	g=undefined;
var sub=http_request.query.msg_sub;
if(grp=='E-Mail')
	sub='mail';
var s=-1;
if(sub != undefined)  {
	if(msg_area.grp_list[g] != undefined) {
		for(stmp in msg_area.grp_list[g].sub_list)  {
			if(msg_area.grp_list[g].sub_list[stmp].code == sub)  {
				s=stmp;
			}
		}
	}
	var msgbase = new MsgBase(sub);
}
var message=http_request.query.message;
var m=parseInt(http_request.query.message);
var path=http_request.virtual_path;
var offset=parseInt(http_request.query.offset);
if (http_request.query.offset == undefined)  {
	offset=0;
}
var hdr=null;
var title=null;
var body=null;
var err=null;

if(sub != undefined && ((grp != undefined) || (sub=='mail')) && message == undefined &&
		http_request.query.to != undefined &&
		http_request.query.subject != undefined &&
		http_request.query.body != undefined)  {
	var hdrs = new Object;
	if(sub!='mail')  {
		if(! msg_area.grp_list[g].sub_list[s].can_post)  {
			error("You don't have sufficient rights to post in this sub");
		}
	}
	else {
		hdrs.to_net_type=netaddr_type(http_request.query.to);
		if(hdrs.to_net_type!=NET_NONE)
			hdrs.to_net_addr=http_request.query.to;
	}
	hdrs.from=user.alias;
	hdrs.to=http_request.query.to;
	hdrs.subject=http_request.query.subject;
	if(http_request.query.reply_to != undefined)  {
		hdrs.thread_orig=parseInt(http_request.query.reply_to);
	}
	if(msgbase.open!=undefined && msgbase.open()==false) {
		error(msgbase.last_error);
	}
	if(!msgbase.save_msg(hdrs,http_request.query.body))  {
		error(msgbase.last_error);
	}
	msgbase.close();
	http_reply.status="201 Created";
	title="Message posted";
	write_template("header.inc");
	write_template("posted.inc");
	write_template("footer.inc");
}
else if(grp != undefined &&
		sub == undefined)  {
	var next=null;
	var prev=null;
	if(offset > 0)  {
		if(offset<max_subs)  {
			offset=max_subs;
		}
		prev=path+'?msg_grp='+g+'&offset='+(offset-max_subs);
	}
	if(sub!='mail') {
		if(offset+max_subs<msg_area.grp_list[g].sub_list.length)  {
			next=path+'?msg_grp='+g+'&offset='+(offset+max_subs);
		}
		title="Message Subs in "+msg_area.grp_list[g].description;
	}
	write_template("header.inc");
	write_template("subhead.inc");
	for(s=offset;s<offset+max_subs && s<msg_area.grp_list[g].sub_list.length;s++)  {
		if(msg_area.grp_list[g].sub_list[s].can_read)
			write_template("sub.inc");
	}
	write_template("subfoot.inc");
	write_template("footer.inc");
}
else if(sub != undefined && ((sub=='mail') || (grp != undefined)) && http_request.query.reply_to != undefined)  {
	if(sub!='mail') {
		if(! msg_area.grp_list[g].sub_list[s].can_post)  {
			error("You don't have sufficient rights to post in this sub");
		}
	}
	if(msgbase.open!=undefined && msgbase.open()==false) {
		error(msgbase.last_error);
	}
	hdr=msgbase.get_msg_header(false,parseInt(http_request.query.reply_to));
	body=msgbase.get_msg_body(false,parseInt(http_request.query.reply_to),true,true)
	if(this.word_wrap != undefined)  {
		body=quote_msg(word_wrap(body,79),79);
	}
	else  {
		body=body.replace(/^(.)/mg,"> $1");
	}
	msgbase.close();
	title="Reply to message";
	write_template("header.inc");
	write_template("reply.inc");
	write_template("footer.inc");
}
else if(sub != undefined && ((sub=='mail') || (grp != undefined)) && http_request.query.post != undefined)  {
	if(sub!='mail') {
		if(! msg_area.grp_list[g].sub_list[s].can_post)  {
			error("You don't have sufficient rights to post in this sub");
		}
	}
	title="Post a message";
	write_template("header.inc");
	write_template("post.inc");
	write_template("footer.inc");
}
else if(sub != undefined && ((sub=='mail') || (grp != undefined)) && message==undefined)  {
	if(msgbase.open!=undefined && msgbase.open()==false) {
		error(msgbase.last_error);
	}
	next=null;
	prev=null;
	if(offset > 0)  {
		if(offset<max_messages)  {
			offset=max_messages;
		}
		prev=path+'?msg_grp='+g+'&msg_sub='+encodeURIComponent(sub)+'&offset='+(offset-max_messages);
	}
	hdr=msgbase.get_msg_header(true,msgbase.total_msgs-1-offset-max_messages);
	if(hdr!=null)  {
		next=path+'?msg_grp='+g+'&msg_sub='+encodeURIComponent(sub)+'&offset='+(offset+max_messages);
	}
	if(sub=='mail')
		title="Messages in E-Mail";
	else
		title="Messages in "+msg_area.grp_list[g].sub_list[s].description;
	if(sub!='mail')  {
		if(! msg_area.grp_list[g].sub_list[s].can_read)  {
			error("You don't have sufficient rights to read this sub");
		}
	}

	write_template("header.inc");
	write_template("msgshead.inc");
	last_offset=msgbase.total_msgs-1-offset;
	var displayed=0;
	for(;displayed<max_messages && (hdr=msgbase.get_msg_header(true,last_offset)) != null;last_offset--) {
		if(hdr==null)
			continue;
		if(sub=='mail' && hdr.to!=user.alias && hdr.to!=user.name && hdr.to !=user.netmail)
			continue;
		write_template("msgs.inc");
		displayed++;
	}
	write_template("msgsfoot.inc");
	write_template("footer.inc");
	msgbase.close();
}
else if(sub != undefined && ((sub=='mail') || (grp != undefined)) && message != undefined)  {
	var msgbase = new MsgBase(sub);
	if(sub!='mail')  {
		if(! msg_area.grp_list[g].sub_list[s].can_read)  {
			error("You don't have sufficient rights to read this sub");
		}
	}
	if(msgbase.open!=undefined && msgbase.open()==false) {
		error(msgbase.last_error);
	}
	hdr=msgbase.get_msg_header(false,m);
	body=msgbase.get_msg_body(false,m,true,true)
	if(hdr != null)  {
		title="Message: "+hdr.subject;
	}
	write_template("header.inc");
	write_template("msg.inc");
	write_template("footer.inc");
	msgbase.close();
}
else {
	var next=null;
	var prev=null;
	if(offset > 0)  {
		if(offset<max_groups)  {
			offset=max_max_groups;
		}
		prev=path+'?offset='+(offset-max_groups);
	}
	if(offset+max_groups<msg_area.grp_list.length)  {
		next=path+'?offset='+(offset+max_groups);
	}
	title="Message Groups";
	write_template("header.inc");
	write_template("grouphead.inc");
	for(g=offset;g<offset+max_groups&&g<msg_area.grp_list.length;g++) {
		write_template("group.inc");
	}
	write_template("groupfoot.inc");
	write_template("footer.inc");
}

function write_template(filename)  {
	var inc=new File(template_dir+'/'+filename);
	if(!inc.open("r",true,1024)) {
		horrible_error("Cannot open template file "+template_dir+'/'+filename+"!");
	}
	var file='';
	while(! inc.eof)  {
		file=file += inc.read(1024);
	}
	inc.close();
	file=file.replace(/%%TITLE%%/g,html_encode(title,true,false,false));
	file=file.replace(/%%PATH%%/g,path);
	file=file.replace(/%%NEXT%%/g,next);
	file=file.replace(/%%PREV%%/g,prev);
	file=file.replace(/%%ERROR%%/g,err);
	file=file.replace(/%%REPLY_TO%%/g,http_request.query.reply_to);
	if(next == null)  {
		file=file.replace(/\+\+NEXT%%[^%]*%%NEXT\+\+/gm,'');
	}
	else  {
		file=file.replace(/--NEXT%%[^%]*%%NEXT--/gm,'');
	}
	if(prev == null)  {
		file=file.replace(/\+\+PREV%%.[^%]*%%PREV\+\+/gm,'');
	}
	else  {
		file=file.replace(/--PREV%%[^%]*%%PREV--/gm,'');
	}
	file=file.replace(/%%GRP_([a-zA-Z_][A-Za-z0-9_]*)%%/g,
		function (matched,prop) {
			if(sub=='mail')
				return html_encode("E-Mail",true,false,false);
			else
				return html_encode(msg_area.grp_list[g][prop].toString(),true,false,false);
		});
	file=file.replace(/%%SUB_([a-zA-Z_][A-Za-z0-9_]*)%%/g,
		function (matched,prop) {
			if(sub=='mail')
				return html_encode("mail",true,false,false);
			else
				return html_encode(msg_area.grp_list[g].sub_list[s][prop].toString(),true,false,false);
		});
	if(hdr != null)  {
		if(hdr.attr&MSG_DELETE)  {
			file=file.replace(/%%HDRDATE%%/g,"&lt;Deleted&gt;");
			file=file.replace(/%%HDR_([a-zA-Z_][A-Za-z0-9_]*)%%/g,"&lt;Deleted&gt;");
		}
		else  {
			date = system.timestr(hdr.when_written_time);
			file=file.replace(/%%HDRDATE%%/g,html_encode(date,true,false,false));
			file=file.replace(/%%HDR_([a-zA-Z_][A-Za-z0-9_]*)%%/g,
				function (matched,prop) {
					return html_encode(hdr[prop].toString(),true,false,false);
				});
		}
	}
	else  {
		file=file.replace(/%%HDRDATE%%/g,"&lt;Deleted&gt;");
		file=file.replace(/%%HDR_([a-zA-Z_][A-Za-z0-9_]*)%%/g,"&lt;Deleted&gt;");
	}
	file=file.replace(/!!URI(.*?)!!/g,
		function (matched,uri)  {
			return encodeURIComponent(html_decode(uri));
		});
	if(body==null)
		file=file.replace(/%%BODY%%/g,"&lt;Deleted&gt;");
	else  {
		if(body.indexOf('\x1b[')>=0 || body.indexOf('\x01')>=0)
			var body2=html_encode(body,true,false,true,true);
		else  {
			body=word_wrap(body,79);
			var body2=html_encode(body,true,false,false,false);
		}
		file=file.replace(/%%BODY%%/g,body2);
	}
	file.replace(/%%JS_(.*?)%%/g,
		function (matched, exp)  {
			return eval(exp);
		});
	write(file);
}

function error(message)  {
	title="Error";
	err=message;
	write_template("header.inc");
	write_template("error.inc");
	write_template("footer.inc");
	exit();
}

function horrible_error(message) {
	write("<HTML><HEAD><TITLE>ERROR</TITLE></HEAD><BODY><p>"+message+"</p></BODY></HTML>");
}
