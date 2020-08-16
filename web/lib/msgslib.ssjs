/* $Id: msgslib.ssjs,v 1.62 2018/12/21 02:32:03 rswindell Exp $ */

load("sbbsdefs.js");
load('../web/lib/template.ssjs');
load(msgsconfig);

// Use guest user if the current user is super-duper
if(user.number==0 && this.login!=undefined)
	login("Guest");	// requires v3.12b (websrvr 1.263 or later)
if(user.number==0) {
    error("Message Group access requires login");
	exit();
}

/* Flags for clean_msg_headers() */
var CLEAN_MSG_REPLY	=	(1<<0);

/* Gets for get_message_offsets() */
var GET_ALL_MESSAGES =	0;
var GET_MY_MESSAGES	 =	1;
var GET_MY_UNREAD_MESSAGES = 2;

http_reply.header["Pragma"]="no-cache";
http_reply.header["Expires"]="0";

var grp=http_request.query.msg_grp;
if(grp=='' || grp==null)
	grp=undefined;
var sub=http_request.query.msg_sub;
if(grp=='E-Mail')
	sub='mail';
var s=-1;
if(sub != undefined)
	var msgbase = new MsgBase(sub);
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

function get_message_offsets(type)
{
	offsets=[];//new Array; // is this global or something?
//	var last_offset;
//	var idx;
//	var hdr;

//	for(last_offset=0; (idx=msgbase.get_msg_index(true,last_offset)) != null;last_offset++) {
	for (var last_offset = 0; last_offset <= msgbase.last_msg; last_offset++) {
		var idx = msgbase.get_msg_index(true, last_offset);
		if (typeof idx === 'undefined' || idx === null) continue;
		if(idx.attr&MSG_DELETE)
			continue;
		if(sub=='mail' && idx.to!=user.number)
			continue;
		if(type==GET_MY_MESSAGES || type==GET_MY_UNREAD_MESSAGES) {
			if(!idx_to_user(idx))
				continue;
		}
		if(type==GET_MY_UNREAD_MESSAGES) {
			if(idx.attr&MSG_READ)
				continue;
		}
		if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED))
			break; // pourquoi?
		// msg=new Array;
		// msg.idx=idx;
		// msg.offset=last_offset;
		// offsets.push(msg);
		offsets.push({ idx : idx, offset : last_offset });
	}
	return(offsets);
}

function msgs_done()
{
	if(msgbase!=undefined)
		msgbase.close();
}

function find_np_message(offset,next)
{
	/* "Next" actually means the one before this one as msgs reverses everything */
	var step=-1;
	var	idx;
	var last_offset;

	if(!next)
		step=1;
	for(last_offset=parseInt(offset)+step;last_offset>=0 && (idx=msgbase.get_msg_index(true,last_offset))!=null;last_offset+=step) {
		if(idx.attr&MSG_DELETE)
			continue;
		if(idx.attr&MSG_PRIVATE && !idx_to_user(idx))
			continue;
		if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED)) {
			if(step==1)
				break;
			else
				continue;
		}
		if(sub!='mail')
			return(idx.number);
		if(!idx_to_user(idx))
			continue;
		return(idx.number);
	}
	return(undefined);
}

function get_msg_offset(number)
{
	var idx;
	var last_offset;

	idx=msgbase.get_msg_index(false,number);
	if(idx!=undefined)
		return(idx.offset);
	return(undefined);
}

function can_delete(mnum)
{
	if(sub=='mail' && ((idx=msgbase.get_msg_index(false,mnum))==null || idx.to!=user.number))
		return(false);
	if(sub!='mail' && !msg_area.sub[sub].is_operator) {
		if(!(msg_area.sub[sub].settings&SUB_DEL))
			return(false);
		if(msg_area.sub[sub].settings&SUB_DELLAST) {
			if(msgbase.last_msg!=mnum)
				return(false);
		}
		if((hdr=msgbase.get_msg_header(false,mnum))==null || hdr.from_ext!=user.number)
			return(false);
	}
	return(true);
}

function clean_msg_headers(hdr,flags)
{
	if (hdr !== null) {

		if(hdr.subject=='')
			hdr.subject="-- No Subject --";
		if(hdr.attr&MSG_ANONYMOUS) {
			if((!(flags&CLEAN_MSG_REPLY))) {
				if(user.security.level>=90)
					hdr.from='Anonymous ('+hdr.from+')';
				else
					hdr.from="Anonymous";
			}
			else
				hdr.from="All";
		}

	}

	return(hdr);
}

function idx_to_user(fromidx)
{
	var istouser=false;

	if(sub=='mail') {
		if(fromidx.to==user.number)
			istouser=true;
	}
	else {
		var aliascrc=crc16_calc(user.alias.toLowerCase());
		var namecrc=crc16_calc(user.name.toLowerCase());
		var sysopcrc=crc16_calc((user.number==1?'sysop':user.name.toLowerCase()));
		if(fromidx.to==aliascrc || fromidx.to==namecrc || fromidx.to==sysopcrc) {
			/* Could be to this user */
			var fromhdr=msgbase.get_msg_header(true,fromidx.offset);
			if(!fromhdr)
				return(false);
			if(user.alias.toLowerCase()==fromhdr.to.toLowerCase())
				istouser=true;
			else if(user.name.toLowerCase()==fromhdr.to.toLowerCase())
				istouser=true;
			else if(user.number==1 && fromhdr.to.toLowerCase()=='sysop' && fromhdr.from_net_type==0)
				istouser=true;
		}
	}
	return(istouser);
}

function make_links(str) {
	str=str.replace(/(?:http|https|ftp|telnet|gopher|irc|news):\/\/[\w\-\.]+\.[a-zA-Z]+(?::[\w-+_%]*)?(?:\/(?:[\w\-._\?\,\/\\\+&;:%\$#\=~\*]*))?/gi,function(str) {
//					 | Protocol                                    |Hostname  |TLD     | Port        | Path									  |
		var text=str;
		var uri=str;
		var extra='';
		m=str.match(/^([\x00-\xff]*?)((?:&gt;|[\r\n,.\)])+)$/);
		if(m!=null) {
			text=m[1];
			uri=m[1];
			extra=m[2]+extra;
		}
		uri=uri.replace(/[\r\n]/g,'');
		var ret='<a href="'+uri+'" target="_blank">'+text+'</a>'+extra;
		return(ret);}
	);
	return(str);
}
