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

	for(last_offset=0; (idx=msgbase.get_msg_index(true,last_offset)) != null;last_offset++) {
		if(idx.attr&MSG_DELETE)
			continue;
		if(idx.to!=user.number)
			continue;
		msg=new Array;
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

	for(last_offset=0; (idx=msgbase.get_msg_index(true,last_offset)) != null;last_offset++) {
		if(idx.attr&MSG_DELETE)
			continue;
		if(idx.attr&MSG_PRIVATE && idx.to!=user.number)
			continue;
		msg=new Object;
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
	var	idx;
	var last_offset;

	if(!next)
		step=1;
	for(last_offset=parseInt(offset)+step;last_offset>0 && (idx=msgbase.get_msg_index(true,last_offset))!=null;last_offset+=step) {
		if(idx.attr&MSG_DELETE)
			continue;
		if(idx.attr&MSG_PRIVATE && idx.to!=user.number)
			continue;
		if(sub!='mail')
			return(idx.number);
		if(idx.to!=user.number)
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
		if(!msg_area.sub[sub].settings&SUB_DEL)
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
