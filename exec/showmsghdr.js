// $Id$
// This can be loaded from text/menu/msghdr.asc via @EXEC:SHOWMSGHDR@

load("smbdefs.js");
load("text.js");
var   USER_ANSI         =(1<<1);

// ported from sbbs_t::show_msgattr():
function show_msgattr(msg_attr, msg_auxattr)
{
	var attr = msg_attr;
	var poll = attr&MSG_POLL_VOTE_MASK;
	var auxattr = msg_auxattr;
	var nulstr = "";

	printf(bbs.text(MsgAttr)
		,attr&MSG_PRIVATE	? "Private  "   :nulstr
		,attr&MSG_SPAM		? "SPAM  "      :nulstr
		,attr&MSG_READ		? "Read  "      :nulstr
		,attr&MSG_DELETE	? "Deleted  "   :nulstr
		,attr&MSG_KILLREAD	? "Kill  "      :nulstr
		,attr&MSG_ANONYMOUS ? "Anonymous  " :nulstr
		,attr&MSG_LOCKED	? "Locked  "    :nulstr
		,attr&MSG_PERMANENT ? "Permanent  " :nulstr
		,attr&MSG_MODERATED ? "Moderated  " :nulstr
		,attr&MSG_VALIDATED ? "Validated  " :nulstr
		,attr&MSG_REPLIED	? "Replied  "	:nulstr
		,attr&MSG_NOREPLY	? "NoReply  "	:nulstr
		,poll == MSG_POLL	? "Poll  "		:nulstr
		,poll == MSG_POLL && auxattr&POLL_CLOSED ? "(Closed)  "	:nulstr
		,nulstr
		,nulstr
		,nulstr
		);
}

// ported from sbbs_t::show_msghdr():
function show_msghdr()
{
	printf(bbs.text(MsgSubj), bbs.msg_subject);
	if(bbs.msg_attr)
		show_msgattr(bbs.msg_attr /* ??? */);
	if(bbs.msg_to && bbs.msg_to.length) {
		printf(bbs.text(MsgTo), bbs.msg_to);
		if(bbs.msg_to_net)
			printf(bbs.text(MsgToNet), bbs.msg_to_net);
		if(bbs.msg_to_ext)
			printf(bbs.text(MsgToExt), bbs.msg_to_ext);
	}
	if(!(bbs.msg_attr&MSG_ANONYMOUS) || user.is_sysop) {
		printf(bbs.text(MsgFrom),bbs.msg_from);
		if(bbs.msg_from_ext)
			printf(bbs.text(MsgFromExt), bbs.msg_from_ext);
		if(bbs.msg_from_net)
			printf(bbs.text(MsgFromNet), bbs.msg_from_net); 
	}
	// the bbs.msg_up/downvotes properties don't actually exist (yet)
	if(!(bbs.msg_attr&MSG_POLL) && (bbs.atcode("MSG_UPVOTES")!='0' || bbs.atcode("MSG_DOWNVOTES")!='0'))
		printf(bbs.text(MsgVotes)
			,bbs.atcode("MSG_UPVOTES"), bbs.atcode("MSG_UPVOTED")
			,bbs.atcode("MSG_DOWNVOTES"), bbs.atcode("MSG_DOWNVOTED")
			,bbs.atcode("MSG_SCORE"));
	printf(bbs.text(MsgDate), system.timestr(bbs.msg_date), system.zonestr(bbs.msg_timezone), bbs.atcode("MSG_AGE"));

	console.crlf();
}

show_msghdr();

// Avatar support here:
if(!(bbs.msg_attr&MSG_ANONYMOUS) && console.term_supports(USER_ANSI)) {
	var Avatar = load({}, 'avatar_lib.js');
	Avatar.draw(bbs.msg_from_ext, bbs.msg_from, bbs.msg_from_net, /* above: */true, /* right-justified: */true);
//	console.attributes = -1;	// Can't trust the saved 'current attribute' value
}