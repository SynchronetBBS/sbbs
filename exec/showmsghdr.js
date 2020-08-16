// $Id: showmsghdr.js,v 1.6 2019/06/15 01:28:28 rswindell Exp $

// This can be loaded from text/menu/msghdr.asc via @EXEC:SHOWMSGHDR@
// Don't forget to include or exclude the blank line after if do
// (or don't) want a blank line separating message headers and body text

require("text.js", 'MsgAttr');
require("smbdefs.js", 'MSG_ANONYMOUS');
require("userdefs.js", 'USER_ANSI');

// ported from sbbs_t::show_msgattr():
function show_msgattr(msg_attr, msg_auxattr)
{
	var attr = msg_attr;
	var poll = attr&MSG_POLL_VOTE_MASK;
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
		,poll == MSG_POLL && msg_auxattr&POLL_CLOSED ? "(Closed)  "	:nulstr
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
		show_msgattr(bbs.msg_attr, bbs.msg_auxattr);
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
	// the bbs.msg_up/down-votes properties don't actually exist (yet), so get their values via @-codes
	var msg_upvotes = parseInt(bbs.atcode("MSG_UPVOTES"));
	var msg_downvotes = parseInt(bbs.atcode("MSG_DOWNVOTES"));
	if(!(bbs.msg_attr&MSG_POLL) && (msg_upvotes || msg_downvotes))
		printf(bbs.text(MsgVotes)
			,msg_upvotes, bbs.atcode("MSG_UPVOTED")
			,msg_downvotes, bbs.atcode("MSG_DOWNVOTED")
			,bbs.atcode("MSG_SCORE"));
	printf(bbs.text(MsgDate), system.timestr(bbs.msg_date), system.zonestr(bbs.msg_timezone), bbs.atcode("MSG_AGE"));

	console.crlf();
}

show_msghdr();

// Avatar support here:
if(!(bbs.msg_attr&MSG_ANONYMOUS) && console.term_supports(USER_ANSI)) {
	if(!bbs.mods.avatar_lib)
		bbs.mods.avatar_lib = load({}, 'avatar_lib.js');
	bbs.mods.avatar_lib.draw(bbs.msg_from_ext, bbs.msg_from, bbs.msg_from_net, /* above: */true, /* right-justified: */true);
	console.attributes = 7;	// Clear the background attribute as the next line might scroll, filling with BG attribute
}