load("sbbsdefs.js");
load("html_inc/template.ssjs");
load("html_inc/msgsconfig.ssjs");

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

function get_my_message_offsets()
{
	offsets=new Array;
	var last_offset;
	var idx;
	var hdr;

	for(last_offset=0; (hdr=msgbase.get_msg_header(true,last_offset)) != null;last_offset++) {
		if(hdr.attr&MSG_DELETE)
			continue;
		if((idx=msgbase.get_msg_index(true,last_offset))==null || idx.to!=user.number)
			continue;
		msg=new Array;
		msg["hdr"]=hdr;
		msg.idx=idx;
		msg.offset=last_offset;
		offsets.push(msg);
	}
	return(offsets);
}

function get_all_message_offsets()
{
	offsets=new Array;
	var last_offset;
	var hdr;

	for(last_offset=0; (hdr=msgbase.get_msg_header(true,last_offset)) != null;last_offset++) {
		if(hdr.attr&MSG_DELETE)
			continue;
		idx=msgbase.get_msg_index(true,last_offset);
		msg=new Object;
		msg.hdr=hdr;
		msg.idx=idx;
		msg.offset=last_offset;
		offsets.push(msg);
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
	var hdr=null;
	var	idx;

	if(!next)
		step=1;

	for(last_offset=offset+step;(hdr=msgbase.get_msg_header(true,last_offset))!=null;last_offset+=step) {
		if(hdr.attr&MSG_DELETE)
			continue;
		if(sub!=mail)
			return(last_offset);
		if((idx=msgbase.get_msg_index(true,last_offset))==null || idx.to!=user.number)
			continue;
		return(hdr.number);
	}
	return(undefined);
}
