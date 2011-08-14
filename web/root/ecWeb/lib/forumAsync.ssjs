/* forumAsync.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* Handlers for asynchronous posting of messages and updating of message scan
   pointers. */

load('webInit.ssjs');

// With a bit of tweaking, this could be used outside of the forum for posting of messages via HTTP
if(http_request.query.hasOwnProperty('action') && http_request.query.action.toString() == 'postMessage') {
	var msgBase = new MsgBase(http_request.query.subBoardCode);
	if(!msgBase.open()) exit();
	if(http_request.query.subBoardCode.toString() != 'mail' && !user.compare_ars(msgBase.cfg.post_ars)) exit();
	var body = html_decode(http_request.query.body).replace(/\n/g, "\r\n");
	var header = { subject : http_request.query.subject, to : http_request.query.to, from: http_request.query.from, from_ext : user.number, from_ip_addr : client.ip_address, replyto : http_request.query.from, replyto_ext : user.number }
	if(http_request.query.irtMessage != 'none') header.thread_back = http_request.query.irtMessage;
	if(http_request.query.subBoardCode.toString() == 'mail') header.to_net_addr = http_request.query.to;
	header.to_net_type = netaddr_type(header.to);
	header.from_net_type = netaddr_type(header.to);
	msgBase.save_msg(header, body_text=word_wrap(body));
	print("<div class='headingFont standardBorder standardPadding'>Your message has been posted</div>");
	msgBase.close();
}

if(http_request.query.hasOwnProperty('action') && http_request.query.action.toString() == 'updatePointer' && user.alias != webIni.guestUser) {
	var msgBase = new MsgBase(http_request.query.subBoardCode);
	if(!msgBase.open() || http_request.query.subBoardCode == 'mail') exit();
	msg_area.grp_list[http_request.query.mg].sub_list[http_request.query.sb].scan_ptr = msgBase.last_msg;
	msgBase.close();
}
