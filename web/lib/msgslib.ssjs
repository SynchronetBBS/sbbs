load("sbbsdefs.js");
load("html_inc/template.ssjs");
load("html_inc/msgsconfig.ssjs");

http_reply.header["Pragma"]="no-cache";
http_reply.header["Expires"]="0";

var grp=http_request.query.msg_grp;
if(grp=='' || grp==null)
	grp=undefined;
var g=parseInt(http_request.query.msg_grp);
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
