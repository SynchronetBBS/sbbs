/*
 * Takes an RFC822 message and creates a parsed object out of it
 * The object looks like this:
{
	header:{
		"::":[Original Headers in Original Order],
		":mime:":[Original MIME Headers in Original Order],
		<name>:[<header string>...]
	},
	text:"String"
}
 */

var abnf={};

/************************/
/* Core (from RFC5234) */
/************************/
abnf.ALPHA="[\\x41-\\x5a\\x61-\\x7a]";
abnf.BIT="[01]";
abnf.CHAR="[\\x01-\\x7f]";
abnf.CR="\\x0d";
abnf.CTL="[\\x00-\\x1f\\x7f]";
abnf.DIGIT="[\\x30-\\x39]";
abnf.DQUOTE='"';
abnf.HEXDIG="[\\x30-\\x39A-F]";
abnf.HTAB="\\x09";
abnf.LF="\\x0a";
abnf.CRLF="(?:"+abnf.CR+abnf.LF+")";
abnf.LWSP="(?:(?:"+abnf.CRLF+"?[\\t ])*)";
abnf.OCTET="[\\x00-\\xff]";
abnf.SP="\\x20";
abnf.VCHAR="[\\x21-\\x7e]";
abnf.WSP="[\\t ]";

/*****************/
/* From RFC 5322 */
/*****************/
var rfc5322abnf={};

// 4.1 Miscellaneous Obsolete Tokens
rfc5322abnf.obs_NO_WS_CTL="[\\x01-\\x08\\x0b-\\x0c\\x0e-\\x1f\\x7f]";
rfc5322abnf.obs_ctext=rfc5322abnf.obs_NO_WS_CTL;
rfc5322abnf.obs_qtext=rfc5322abnf.obs_NO_WS_CTL;
rfc5322abnf.obs_utext="(?:\\x00|"+rfc5322abnf.obs_NO_WS_CTL+"|"+abnf.VCHAR+")";
rfc5322abnf.obs_qp="(?:\\\\(?:\\x00|"+rfc5322abnf.obs_NO_WS_CTL+"|"+abnf.LF+"|"+abnf.CR+"))";

// 3.2.1 Quoted characters
rfc5322abnf.quoted_pair="(?:(?:\\\\(?:"+abnf.VCHAR+"|"+abnf.WSP+"))|"+rfc5322abnf.obs_qp+")";

// 4.2 Obsolete Folding White Space
rfc5322abnf.obs_FWS="(?:"+abnf.WSP+"+(?:"+abnf.CRLF+abnf.WSP+"+)*)";

// 3.2.2 Folding White Space and Comments
rfc5322abnf.FWS="(?:(?:(?:"+abnf.WSP+"*"+abnf.CRLF+")?"+abnf.WSP+"+)|"+rfc5322abnf.obs_FWS+")";

// 4.1 Miscellaneous Obsolete Tokens
rfc5322abnf.obs_unstruct="(?:(?:(?:"+abnf.LF+"*(?:"+abnf.CR+"(?!"+abnf.LF+"))*"+"(?:"+rfc5322abnf.obs_utext+abnf.LF+"*(?:"+abnf.CR+"(?!"+abnf.LF+"))*)*)|"+rfc5322abnf.FWS+")*)";

// 3.2.2 Folding White Space and Comments
rfc5322abnf.ctext="[\\x21-\\x27\\x2a-\\x5b\\x5d-\\x7e]";
// RECURSIVE!!!
//rfc5322abnf.comment="(?:\\((?:"+rfc5322abnf.FWS+"?"+rfc5322abnf.ccontent+")*"+rfc5322abnf.FWS+"?\\))";
//rfc5322abnf.ccontent="(?:"+rfc5322abnf.ctext+"|"+rfc5322abnf.quoted_pair+"|"+rfc5322abnf.comment+")";
// To fix this, we'll do a magic comment which does not allow comments in it (otherwise a copy/paste from ccontent)
// THIS NEEDS TO BE REMOVED MULTIPLE TIMES UNTIL IT'S GONE!
rfc5322abnf.comment="(?:\\((?:"+rfc5322abnf.FWS+"?"+"(?:"+rfc5322abnf.ctext+"|"+rfc5322abnf.quoted_pair+")"+")*"+rfc5322abnf.FWS+"?\\))";
rfc5322abnf.CFWS="(?:(?:(?:"+rfc5322abnf.FWS+"?"+rfc5322abnf.comment+")+"+rfc5322abnf.FWS+"?)|"+rfc5322abnf.FWS+")";

// 3.2.3 Atom
rfc5322abnf.atext="(?:"+abnf.ALPHA+"|"+abnf.DIGIT+"|[\\!\\#\\$\\%\\&\\+\\-\\/\\=\\?\\^\\_\\`\\{\\|\\}\\~])";
rfc5322abnf.atom="(?:"+rfc5322abnf.CFWS+"?"+rfc5322abnf.atext+"+"+rfc5322abnf.CFWS+"?)";
rfc5322abnf.dot_atom_text="(?:"+rfc5322abnf.atext+"+(?:\\."+rfc5322abnf.atext+"+)*)";
rfc5322abnf.dot_atom="(?:"+rfc5322abnf.CFWS+"?"+rfc5322abnf.dot_atom_text+rfc5322abnf.CFWS+")";
rfc5322abnf.specials="(?:[\\(\\)\\<\\>\\[\\]\\:\\;\\@\\\\\\,\\.]|"+abnf.DQUOTE+")";

// 3.2.4 Quoted Strings
rfc5322abnf.qtext="[\\x21\\x23-\\x5b\\x5d-\\x7e]";
rfc5322abnf.qcontent="(?:"+rfc5322abnf.qtext+"|"+rfc5322abnf.quoted_pair+")";
rfc5322abnf.quoted_string="(?:"+rfc5322abnf.CFWS+"?"+abnf.DQUOTE+"(?:"+rfc5322abnf.FWS+"?"+rfc5322abnf.qcontent+")*"+rfc5322abnf.FWS+"?"+abnf.DQUOTE+rfc5322abnf.CFWS+"?)";

// 3.2.5 Miscellaneous Tokens
rfc5322abnf.word="(?:"+rfc5322abnf.atom+"|"+rfc5322abnf.quoted_string+")";

// 4.1 Miscellaneous Obsolete Tokens
rfc5322abnf.obs_phrase="(?:"+rfc5322abnf.word+"(?:"+rfc5322abnf.word+"|\\.|"+rfc5322abnf.CFWS+")*)";

// 3.2.5 Miscellaneous Tokens
rfc5322abnf.phrase="(?:"+rfc5322abnf.word+"+|"+rfc5322abnf.obs_phrase+")";

// 3.2.5 Miscellaneous Tokens
rfc5322abnf.unstructured="(?:(?:(?:"+rfc5322abnf.FWS+"?"+abnf.VCHAR+")*"+abnf.WSP+"*)|"+rfc5322abnf.obs_unstruct+")";

// 3.3 Date and Time Specification
rfc5322abnf.day_name="(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_day_of_week="(?:"+rfc5322abnf.CFWS+"?"+rfc5322abnf.day_name+rfc5322abnf.CFWS+"?)";

// 3.3 Date and Time Specification
rfc5322abnf.day_of_week="(?:(?:"+rfc5322abnf.FWS+"?"+rfc5322abnf.day_name+")|"+rfc5322abnf.obs_day_of_week+")";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_day="(?:"+rfc5322abnf.CFWS+"?"+abnf.DIGIT+"{1,2}"+rfc5322abnf.CFWS+"?)";

// 3.3 Date and Time Specification
rfc5322abnf.day="(?:(?:"+rfc5322abnf.FWS+"?"+abnf.DIGIT+"{1,2}"+rfc5322abnf.FWS+")|"+rfc5322abnf.obs_day+")";
rfc5322abnf.month="(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_year="(?:"+rfc5322abnf.CFWS+"?"+abnf.DIGIT+"{2,}"+rfc5322abnf.CFWS+"?)";

// 3.3 Date and Time Specification
rfc5322abnf.year="(?:(?:"+rfc5322abnf.FWS+abnf.DIGIT+"{4,}"+rfc5322abnf.FWS+")|"+rfc5322abnf.obs_year+")";
rfc5322abnf.date="(?:"+rfc5322abnf.day+rfc5322abnf.month+rfc5322abnf.year+")";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_hour="(?:"+rfc5322abnf.CFWS+"?"+abnf.DIGIT+"{2}"+rfc5322abnf.CFWS+"?)";

// 3.3 Date and Time Specification
rfc5322abnf.hour="(?:"+abnf.DIGIT+"{2}|"+rfc5322abnf.obs_hour+")";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_minute="(?:"+rfc5322abnf.CFWS+"?"+abnf.DIGIT+"{2}"+rfc5322abnf.CFWS+"?)";

// 3.3 Date and Time Specification
rfc5322abnf.minute="(?:"+abnf.DIGIT+"{2}|"+rfc5322abnf.obs_minute+")";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_second="(?:"+rfc5322abnf.CFWS+"?"+abnf.DIGIT+"{2}"+rfc5322abnf.CFWS+"?)";

// 3.3 Date and Time Specification
rfc5322abnf.second="(?:"+abnf.DIGIT+"{2}|"+rfc5322abnf.obs_second+")";

// 4.3 Obsolete Date and Time
rfc5322abnf.obs_zone="(?:UT|GMT|EST|EDT|CST|CDT|MST|MDT|PST|PDT|[\\x41-\\x49\\x4b-\\x5a\\x61-\\x69\\x6b-\\x7a])";

// 3.3 Date and Time Specification
rfc5322abnf.zone="(?:(?:"+rfc5322abnf.FWS+"[\\+\\-]"+abnf.DIGIT+"{4})|"+rfc5322abnf.obs_zone+")";
rfc5322abnf.time_of_day="(?:"+rfc5322abnf.hour+":"+rfc5322abnf.minute+"(?::"+rfc5322abnf.second+")?)";
rfc5322abnf.time="(?:"+rfc5322abnf.time_of_day+rfc5322abnf.zone+")";
rfc5322abnf.date_time="(?:(?:"+rfc5322abnf.day_of_week+",)?"+rfc5322abnf.date+rfc5322abnf.time+rfc5322abnf.CFWS+"?)";

// 4.4 Obsolete Addressing
rfc5322abnf.obs_local_part="(?:"+rfc5322abnf.word+"(?:\\."+rfc5322abnf.word+")*)";

// 3.4.1 Addr-Spec Specification
rfc5322abnf.local_part="(?:"+rfc5322abnf.dot_atom+"|"+rfc5322abnf.quoted_string+"|"+rfc5322abnf.obs_local_part+")";

// 4.4 Obsolete Addressing
rfc5322abnf.obs_dtext="(?:"+rfc5322abnf.obs_NO_WS_CTL+"|"+rfc5322abnf.quoted_pair+")";

// 3.4.1 Addr-Spec Specification
rfc5322abnf.dtext="(?:[\\x21-\\x5a\\x5e-\\x7e]|"+rfc5322abnf.obs_dtext+")";
rfc5322abnf.domain_literal="(?:"+rfc5322abnf.CFWS+"?\\[(?:"+rfc5322abnf.FWS+"?"+rfc5322abnf.dtext+")*"+rfc5322abnf.FWS+"?\\]"+rfc5322abnf.CFWS+"?)";

// 4.4 Obsolete Addressing
rfc5322abnf.obs_domain="(?:"+rfc5322abnf.atom+"(?:\\."+rfc5322abnf.atom+")*)";

// 3.4.1 Addr-Spec Specification
rfc5322abnf.domain="(?:"+rfc5322abnf.dot_atom+"|"+rfc5322abnf.domain_literal+"|"+rfc5322abnf.obs_domain+")";
rfc5322abnf.addr_spec="(?:"+rfc5322abnf.local_part+"@"+rfc5322abnf.domain+")";

// 4.4 Obsolete Addressing
rfc5322abnf.obs_domain_list="(?:(?:"+rfc5322abnf.CFWS+"|,)*@"+rfc5322abnf.domain+"(?:,"+rfc5322abnf.CFWS+"?(?:@"+rfc5322abnf.domain+")?)*)";
rfc5322abnf.obs_route="(?:"+rfc5322abnf.obs_domain_list+":)";
rfc5322abnf.obs_angle_addr="(?:"+rfc5322abnf.CFWS+"?\\<"+rfc5322abnf.obs_route+rfc5322abnf.addr_spec+"\\>"+rfc5322abnf.CFWS+"?)";

// 3.4 Address Specification
rfc5322abnf.angle_addr="(?:"+rfc5322abnf.CFWS+"?\\<"+rfc5322abnf.addr_spec+"\\>"+rfc5322abnf.CFWS+"?|"+rfc5322abnf.obs_angle_addr+")";
rfc5322abnf.display_name=rfc5322abnf.phrase;
rfc5322abnf.name_addr="(?:"+rfc5322abnf.display_name+"|"+rfc5322abnf.angle_addr+")";
rfc5322abnf.mailbox="(?:"+rfc5322abnf.name_addr+"|"+rfc5322abnf.addr_spec+")";

// 4.4 Obsolete Addressing
rfc5322abnf.obs_mbox_list="(?:(?:"+rfc5322abnf.CFWS+",)*"+rfc5322abnf.mailbox+"(?:,(?:"+rfc5322abnf.mailbox+"|"+rfc5322abnf.CFWS+")?)*)";
rfc5322abnf.obs_group_list="(?:(?:"+rfc5322abnf.CFWS+"?,)+"+rfc5322abnf.CFWS+"?)";

// 3.4 Address Specifications
rfc5322abnf.mailbox_list="(?:(?:"+rfc5322abnf.mailbox+"(?:,"+rfc5322abnf.mailbox+")*)|"+rfc5322abnf.obs_mbox_list+")";
rfc5322abnf.group_list="(?:"+rfc5322abnf.mailbox_list+"|"+rfc5322abnf.CFWS+"|"+rfc5322abnf.obs_group_list+")";
rfc5322abnf.group="(?:"+rfc5322abnf.display_name+":"+rfc5322abnf.group_list+"?;"+rfc5322abnf.CFWS+"?)";
rfc5322abnf.address="(?:"+rfc5322abnf.mailbox+"|"+rfc5322abnf.group+")";

// 4.4 Obsolete Addressing
rfc5322abnf.obs_addr_list="(?:(?:"+rfc5322abnf.CFWS+",)*"+rfc5322abnf.address+"(?:,(?:"+rfc5322abnf.address+"|"+rfc5322abnf.CFWS+")?)*)";

// 3.4 Address Specifications
rfc5322abnf.address_list="(?:(?:"+rfc5322abnf.address+"(?:,"+rfc5322abnf.address+")*)|"+rfc5322abnf.obs_addr_list+")";

// 3.6.1 The Origination Date field
rfc5322abnf.orig_date="(?:Date:"+rfc5322abnf.date_time+abnf.CRLF+")";

// 3.6.2 Originator Fields
rfc5322abnf.from="(?:From:"+rfc5322abnf.mailbox_list+abnf.CRLF+")";
rfc5322abnf.sender="(?:Sender:"+rfc5322abnf.mailbox+abnf.CRLF+")";
rfc5322abnf.reply_to="(?:Reply-To:"+rfc5322abnf.address_list+abnf.CRLF+")";

// 3.6.3 Destination Address Fields
rfc5322abnf.to="(?:To:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.cc="(?:Cc:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.bcc="(?:Bcc:(?:"+rfc5322abnf.address_list+"|"+rfc5322abnf.CFWS+")?"+abnf.CRLF+")";

// 4.5.4 Obsolete Identification Fields
rfc5322abnf.obs_id_left=rfc5322abnf.local_part;
rfc5322abnf.obs_id_right=rfc5322abnf.domain;

// 3.6.4 Identification Fields
rfc5322abnf.id_left="(?:"+rfc5322abnf.dot_atom_text+"|"+rfc5322abnf.obs_id_left+")";
rfc5322abnf.no_fold_literal="(?:\\["+rfc5322abnf.dtext+"*\\])";
rfc5322abnf.id_right="(?:"+rfc5322abnf.dot_atom_text+"|"+rfc5322abnf.no_fold_literal+"|"+rfc5322abnf.obs_id_right+")";
rfc5322abnf.msg_id="(?:"+rfc5322abnf.CFWS+"?\\<"+rfc5322abnf.id_left+"@"+rfc5322abnf.id_right+"\\>"+rfc5322abnf.CFWS+"?)";
rfc5322abnf.message_id="(?:Message-ID:"+rfc5322abnf.msg_id+abnf.CRLF+")";
rfc5322abnf.in_reply_to="(?:In-Reply-To:"+rfc5322abnf.msg_id+"+"+abnf.CRLF+")";
rfc5322abnf.references="(?:References:"+rfc5322abnf.msg_id+"+"+abnf.CRLF+")";

// 3.6.5 Informational Fields
rfc5322abnf.subject="(?:Subject:"+rfc5322abnf.unstructured+abnf.CRLF+")";
rfc5322abnf.comments="(?:Comments:"+rfc5322abnf.unstructured+abnf.CRLF+")";
rfc5322abnf.keywords="(?:Keywords:"+rfc5322abnf.phrase+"(?:,"+rfc5322abnf.phrase+")*"+abnf.CRLF+")";

// 3.6.6 Resent Fields
rfc5322abnf.resent_date="(?:Resent-Date:"+rfc5322abnf.date_time+abnf.CRLF+")";
rfc5322abnf.resent_from="(?:Resent-From:"+rfc5322abnf.mailbox_list+abnf.CRLF+")";
rfc5322abnf.resent_sender="(?:Resent-Sender:"+rfc5322abnf.mailbox+abnf.CRLF+")";
rfc5322abnf.resent_to="(?:Resent-To:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.resent_cc="(?:Resent-Cc:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.resent_bcc="(?:Resent-Bcc:(?:"+rfc5322abnf.address_list+"|"+rfc5322abnf.CFWS+")?"+abnf.CRLF+")";
rfc5322abnf.resent_msg_id="(?:Resent-Message-ID:"+rfc5322abnf.msg_id+abnf.CRLF+")";

// 3.6.7 Trace Fields
rfc5322abnf.path="(?:"+rfc5322abnf.angle_addr+"|(?:"+rfc5322abnf.CFWS+"?\\<"+rfc5322abnf.CFWS+"?\\>"+rfc5322abnf.CFWS+"?))";
rfc5322abnf.received_token="(?:"+rfc5322abnf.word+"|"+rfc5322abnf.angle_addr+"|"+rfc5322abnf.addr_spec+"|"+rfc5322abnf.domain+")";
rfc5322abnf.received="(?:Received:"+rfc5322abnf.received_token+"*;"+rfc5322abnf.date_time+abnf.CRLF+")";
rfc5322abnf.return="(?:Return-Path:"+rfc5322abnf.path+abnf.CRLF+")";
rfc5322abnf.trace="(?:"+rfc5322abnf.return+rfc5322abnf.received+"+)";

// 3.6.8 Optional Fields
rfc5322abnf.ftext="[\\x21-\\x39\\x3b-\\x7e]";
rfc5322abnf.field_name="(?:"+rfc5322abnf.ftext+"+)";
rfc5322abnf.optional_field="(?:"+rfc5322abnf.field_name+":"+rfc5322abnf.unstructured+abnf.CRLF+")";

// 3.6 Field Definitions
rfc5322abnf.fields="(?:(?:"+rfc5322abnf.trace+"|"+rfc5322abnf.optional_field+"*|(?:"+rfc5322abnf.resent_date+"|"+rfc5322abnf.resent_from+"|"+rfc5322abnf.resent_sender+"|"+rfc5322abnf.resent_to+"|"+rfc5322abnf.resent_cc+"|"+rfc5322abnf.resent_bcc+"|"+rfc5322abnf.resent_msg_id+")*)*(?:"+rfc5322abnf.orig_date+"|"+rfc5322abnf.from+"|"+rfc5322abnf.sender+"|"+rfc5322abnf.reply_to+"|"+rfc5322abnf.to+"|"+rfc5322abnf.cc+"|"+rfc5322abnf.bcc+"|"+rfc5322abnf.message_id+"|"+rfc5322abnf.in_reply_to+"|"+rfc5322abnf.references+"|"+rfc5322abnf.subject+"|"+rfc5322abnf.comments+"|"+rfc5322abnf.keywords+"|"+rfc5322abnf.optional_field+")*)";

// 3.5 Overall Message Syntax
rfc5322abnf.text="[\\x01-\\x09\\x0b-\\x0c\\x0e-\\x7f]";

// 4.1 Miscellaneous Obsolete Tokens
rfc5322abnf.obs_body="(?:(?:(?:"+abnf.LF+"*"+abnf.CR+"*"+"(?:(?:\\x00|"+rfc5322abnf.text+")"+abnf.LF+"*"+abnf.CR+"*)*)|"+abnf.CRLF+")*)";

// 4.5.1 Obsolete Origination Date field
rfc5322abnf.obs_orig_date="(?:Date"+abnf.WSP+"*:"+rfc5322abnf.date_time+abnf.CRLF+")";

// 4.5.2 Obsolete Originator Fields
rfc5322abnf.obs_from="(?:From"+abnf.WSP+"*:"+rfc5322abnf.mailbox_list+abnf.CRLF+")";
rfc5322abnf.obs_sender="(?:Sender"+abnf.WSP+"*:"+rfc5322abnf.mailbox+abnf.CRLF+")";
rfc5322abnf.obs_reply_to="(?:Reply-To"+abnf.WSP+"*:"+rfc5322abnf.address_list+abnf.CRLF+")";

// 4.5.3 Obsolete Destination Address Fields
rfc5322abnf.obs_to="(?:To"+abnf.WSP+"*:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.obs_cc="(?:Cc"+abnf.WSP+"*:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.obs_bcc="(?:Bcc"+abnf.WSP+"*:(?:"+rfc5322abnf.address_list+"|(?:(?:"+rfc5322abnf.CFWS+"?,)*"+rfc5322abnf.CFWS+"?))"+abnf.CRLF+")";

// 4.5.4 Obsolete Identification Fields
rfc5322abnf.obs_message_id="(?:Message-ID"+abnf.WSP+"*:"+rfc5322abnf.msg_id+abnf.CRLF+")";
rfc5322abnf.obs_in_reply_to="(?:In-Reply-To"+abnf.WSP+"*:(?:"+rfc5322abnf.phrase+"|"+rfc5322abnf.msg_id+")*"+abnf.CRLF+")";
rfc5322abnf.obs_references="(?:References"+abnf.WSP+"*:(?:"+rfc5322abnf.phrase+"|"+rfc5322abnf.msg_id+")*"+abnf.CRLF+")";

// 4.1 Miscellaneous Obsolete Tokens
rfc5322abnf.obs_phrase_list="(?:(?:"+rfc5322abnf.phrase+"|"+abnf.CRLF+")?(?:,"+rfc5322abnf.phrase+"|"+rfc5322abnf.CFWS+")*)";

// 4.5.5 Obsolete Informational Fields
rfc5322abnf.obs_subject="(?:Subject"+abnf.WSP+"*:"+rfc5322abnf.unstructured+abnf.CRLF+")";
rfc5322abnf.obs_comments="(?:Comments"+abnf.WSP+"*:"+rfc5322abnf.unstructured+abnf.CRLF+")";
rfc5322abnf.obs_keywords="(?:Keywords"+abnf.WSP+"*:"+rfc5322abnf.obs_phrase_list+abnf.CRLF+")";

// 4.5.6 Resent Fields
rfc5322abnf.obs_resent_date="(?:Resent-Date"+abnf.WSP+"*:"+rfc5322abnf.date_time+abnf.CRLF+")";
rfc5322abnf.obs_resent_from="(?:Resent-From"+abnf.WSP+"*:"+rfc5322abnf.mailbox_list+abnf.CRLF+")";
rfc5322abnf.obs_resent_send="(?:Resent-Sender"+abnf.WSP+"*:"+rfc5322abnf.mailbox+abnf.CRLF+")";
rfc5322abnf.obs_resent_to="(?:Resent-To"+abnf.WSP+"*:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.obs_resent_cc="(?:Resent-Cc"+abnf.WSP+"*:"+rfc5322abnf.address_list+abnf.CRLF+")";
rfc5322abnf.obs_resent_bcc="(?:Resent-Bcc"+abnf.WSP+"*:(?:"+rfc5322abnf.address_list+"|(?:(?:"+rfc5322abnf.CFWS+"?,)*"+rfc5322abnf.CFWS+"?))"+abnf.CRLF+")";
rfc5322abnf.obs_resent_mid="(?:Resent-Message-ID"+abnf.WSP+"*:"+rfc5322abnf.msg_id+abnf.CRLF+")";
rfc5322abnf.obs_resent_rply="(?:Resent-Reply-To"+abnf.WSP+"*:"+rfc5322abnf.address_list+abnf.CRLF+")";

// 4.5.7 Obsolete Trace Fields
rfc5322abnf.obs_return="(?:Return-Path"+abnf.WSP+"*:"+rfc5322abnf.path+abnf.CRLF+")";
rfc5322abnf.obs_received="(?:Received"+abnf.WSP+"*:"+rfc5322abnf.received_token+"*;"+rfc5322abnf.date_time+abnf.CRLF+")";

// 4.5.8 Optional Fields
rfc5322abnf.obs_optional="(?:"+rfc5322abnf.field_name+abnf.WSP+"*:"+rfc5322abnf.unstructured+abnf.CRLF+")";

// 4.5 Obsolete Header Fields
rfc5322abnf.obs_fields="(?:(?:"+rfc5322abnf.obs_return+"|"+rfc5322abnf.obs_received+"|"+rfc5322abnf.obs_orig_date+"|"+rfc5322abnf.obs_from+"|"+rfc5322abnf.obs_sender+"|"+rfc5322abnf.obs_reply_to+"|"+rfc5322abnf.obs_to+"|"+rfc5322abnf.obs_cc+"|"+rfc5322abnf.obs_bcc+"|"+rfc5322abnf.obs_message_id+"|"+rfc5322abnf.obs_in_reply_to+"|"+rfc5322abnf.obs_references+"|"+rfc5322abnf.obs_subject+"|"+rfc5322abnf.obs_comments+"|"+rfc5322abnf.obs_keywords+"|"+rfc5322abnf.obs_resent_date+"|"+rfc5322abnf.obs_resent_from+"|"+rfc5322abnf.obs_resent_send+"|"+rfc5322abnf.obs_resent_rply+"|"+rfc5322abnf.obs_resent_to+"|"+rfc5322abnf.obs_resent_cc+"|"+rfc5322abnf.obs_resent_bcc+"|"+rfc5322abnf.obs_resent_mid+"|"+rfc5322abnf.obs_optional+")*)";

// 3.5 Overall Message Syntax
rfc5322abnf.body="(?:(?:(?:"+rfc5322abnf.text+"{0,998}"+abnf.CRLF+")*"+rfc5322abnf.text+"{0,998})|"+rfc5322abnf.obs_body+")";
rfc5322abnf.message="(?:(?:"+rfc5322abnf.fields+"|"+rfc5322abnf.obs_fields+")(?:"+abnf.CRLF+rfc5322abnf.body+")?)";

/***************************/
/* Simplified from RFC5322 */
/***************************/
// 4.1 Miscellaneous Obsolete Tokens
abnf.obs_NO_WS_CTL="[\\x01-\\x08\\x0b-\\x0c\\x0e-\\x1f\\x7f]";
abnf.obs_ctext=abnf.obs_NO_WS_CTL;
abnf.obs_qtext=abnf.obs_NO_WS_CTL;
abnf.obs_utext="[\\x00-\\x08\\x0b-\\x0c\\x0e-\\x1f\\x21-\\x7f]";
abnf.obs_qp="(?:\\\\[\\x00-\\x08\\x0a-\\x1f\\x7f])";

// 3.2.1 Quoted characters
abnf.quoted_pair=abnf.obs_qp;

// 4.2 Obsolete Folding White Space
abnf.obs_FWS="(?:"+abnf.WSP+"+(?:"+abnf.CRLF+abnf.WSP+"+)*)";

// 3.2.2 Folding White Space and Comments
abnf.FWS=abnf.obs_FWS;

// 4.1 Miscellaneous Obsolete Tokens
//abnf.obs_unstruct="(?:(?:(?:"+abnf.LF+"*"+abnf.CR+"*"+"(?:"+abnf.obs_utext+abnf.LF+"*"+abnf.CR+"*)*)|"+abnf.FWS+")*)";
abnf.obs_unstruct="(?:(?:(?:"+abnf.LF+"*(?:"+abnf.CR+"(?!"+abnf.LF+"))*"+"(?:"+rfc5322abnf.obs_utext+abnf.LF+"*(?:"+abnf.CR+"(?!"+abnf.LF+"))*)*)|"+rfc5322abnf.FWS+")*)";

// 3.2.2 Folding White Space and Comments
abnf.ctext="[\\x21-\\x27\\x2a-\\x5b\\x5d-\\x7e]";
// RECURSIVE!!!
//abnf.comment="(?:\\((?:"+abnf.FWS+"?"+abnf.ccontent+")*"+abnf.FWS+"?\\))";
//abnf.ccontent="(?:"+abnf.ctext+"|"+abnf.quoted_pair+"|"+abnf.comment+")";
// To fix this, we'll do a magic comment which does not allow comments in it (otherwise a copy/paste from ccontent)
// THIS NEEDS TO BE REMOVED MULTIPLE TIMES UNTIL IT'S GONE!
abnf.comment="(?:\\((?:"+abnf.FWS+"?"+"(?:"+abnf.ctext+"|"+abnf.quoted_pair+")"+")*"+abnf.FWS+"?\\))";
abnf.CFWS="(?:(?:(?:"+abnf.FWS+"?"+abnf.comment+")+"+abnf.FWS+"?)|"+abnf.FWS+")";

// 3.2.3 Atom
abnf.atext="[!#-&+\\-/-9=?A-Z\\^-~]";
abnf.atom="(?:"+abnf.CFWS+"?"+abnf.atext+"+"+abnf.CFWS+"?)";
abnf.dot_atom_text="(?:"+abnf.atext+"+(?:\\."+abnf.atext+"+)*)";
abnf.dot_atom="(?:"+abnf.CFWS+"?"+abnf.dot_atom_text+abnf.CFWS+")";
abnf.specials="(?:[\\(\\)\\<\\>\\[\\]\\:\\;\\@\\\\\\,\\.]|"+abnf.DQUOTE+")";

// 3.2.4 Quoted Strings
abnf.qtext="[\\x21\\x23-\\x5b\\x5d-\\x7e]";
abnf.qcontent="(?:"+abnf.qtext+"|"+abnf.quoted_pair+")";
abnf.quoted_string="(?:"+abnf.CFWS+"?"+abnf.DQUOTE+"(?:"+abnf.FWS+"?"+abnf.qcontent+")*"+abnf.FWS+"?"+abnf.DQUOTE+abnf.CFWS+"?)";

// 3.2.5 Miscellaneous Tokens
abnf.word="(?:"+abnf.atom+"|"+abnf.quoted_string+")";

// 4.1 Miscellaneous Obsolete Tokens
abnf.obs_phrase="(?:"+abnf.word+"(?:"+abnf.word+"|\\.|"+abnf.CFWS+")*)";

// 3.2.5 Miscellaneous Tokens
abnf.phrase=abnf.obs_phrase;

// 3.2.5 Miscellaneous Tokens
abnf.unstructured=abnf.obs_unstruct;

// 3.3 Date and Time Specification
abnf.day_name="(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)";

// 4.3 Obsolete Date and Time
abnf.obs_day_of_week="(?:"+abnf.CFWS+"?"+abnf.day_name+abnf.CFWS+"?)";

// 3.3 Date and Time Specification
abnf.day_of_week=abnf.obs_day_of_week;

// 4.3 Obsolete Date and Time
abnf.obs_day="(?:"+abnf.CFWS+"?"+abnf.DIGIT+"{1,2}"+abnf.CFWS+"?)";

// 3.3 Date and Time Specification
abnf.day=abnf.obs_day;
abnf.month="(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)";

// 4.3 Obsolete Date and Time
abnf.obs_year="(?:"+abnf.CFWS+"?"+abnf.DIGIT+"{2,}"+abnf.CFWS+"?)";

// 3.3 Date and Time Specification
abnf.year=abnf.obs_year;
abnf.date="(?:"+abnf.day+abnf.month+abnf.year+")";

// 4.3 Obsolete Date and Time
abnf.obs_hour="(?:"+abnf.CFWS+"?"+abnf.DIGIT+"{2}"+abnf.CFWS+"?)";

// 3.3 Date and Time Specification
abnf.hour=abnf.obs_hour;

// 4.3 Obsolete Date and Time
abnf.obs_minute="(?:"+abnf.CFWS+"?"+abnf.DIGIT+"{2}"+abnf.CFWS+"?)";

// 3.3 Date and Time Specification
abnf.minute=abnf.obs_minute;

// 4.3 Obsolete Date and Time
abnf.obs_second="(?:"+abnf.CFWS+"?"+abnf.DIGIT+"{2}"+abnf.CFWS+"?)";

// 3.3 Date and Time Specification
abnf.second=abnf.obs_second;

// 4.3 Obsolete Date and Time
abnf.obs_zone="(?:UT|GMT|EST|EDT|CST|CDT|MST|MDT|PST|PDT|[\\x41-\\x49\\x4b-\\x5a\\x61-\\x69\\x6b-\\x7a])";

// 3.3 Date and Time Specification
abnf.zone="(?:(?:"+abnf.FWS+"[\\+\\-]"+abnf.DIGIT+"{4})|"+abnf.obs_zone+")";
abnf.time_of_day="(?:"+abnf.hour+":"+abnf.minute+"(?::"+abnf.second+")?)";
abnf.time="(?:"+abnf.time_of_day+abnf.zone+")";
abnf.date_time="(?:(?:"+abnf.day_of_week+",)?"+abnf.date+abnf.time+abnf.CFWS+"?)";

// 4.4 Obsolete Addressing
abnf.obs_local_part="(?:"+abnf.word+"(?:\\."+abnf.word+")*)";

// 3.4.1 Addr-Spec Specification
abnf.local_part=abnf.obs_local_part;

// 4.4 Obsolete Addressing
abnf.obs_dtext="(?:"+abnf.obs_NO_WS_CTL+"|"+abnf.quoted_pair+")";

// 3.4.1 Addr-Spec Specification
abnf.dtext=abnf.obs_dtext;
abnf.domain_literal="(?:"+abnf.CFWS+"?\\[(?:"+abnf.FWS+"?"+abnf.dtext+")*"+abnf.FWS+"?\\]"+abnf.CFWS+"?)";

// 4.4 Obsolete Addressing
abnf.obs_domain="(?:"+abnf.atom+"(?:\\."+abnf.atom+")*)";

// 3.4.1 Addr-Spec Specification
abnf.domain="(?:"+abnf.dot_atom+"|"+abnf.domain_literal+"|"+abnf.obs_domain+")";
abnf.addr_spec="(?:"+abnf.local_part+"@"+abnf.domain+")";

// 4.4 Obsolete Addressing
abnf.obs_domain_list="(?:(?:"+abnf.CFWS+"|,)*@"+abnf.domain+"(?:,"+abnf.CFWS+"?(?:@"+abnf.domain+")?)*)";
abnf.obs_route="(?:"+abnf.obs_domain_list+":)";
abnf.obs_angle_addr="(?:"+abnf.CFWS+"?\\<"+abnf.obs_route+abnf.addr_spec+"\\>"+abnf.CFWS+"?)";

// 3.4 Address Specification
abnf.angle_addr=abnf.obs_angle_addr;
abnf.display_name=abnf.phrase;
abnf.name_addr="(?:"+abnf.display_name+"|"+abnf.angle_addr+")";
abnf.mailbox="(?:"+abnf.name_addr+"|"+abnf.addr_spec+")";

// 4.4 Obsolete Addressing
abnf.obs_mbox_list="(?:(?:"+abnf.CFWS+",)*"+abnf.mailbox+"(?:,(?:"+abnf.mailbox+"|"+abnf.CFWS+")?)*)";

// 3.4 Address Specifications
abnf.mailbox_list=abnf.obs_mbox_list;
abnf.obs_group_list="(?:(?:"+abnf.CFWS+"?,)+"+abnf.CFWS+"?)";
abnf.group_list=abnf.obs_group_list;
abnf.group="(?:"+abnf.display_name+":"+abnf.group_list+"?;"+abnf.CFWS+"?)";

abnf.address="(?:"+abnf.mailbox+"|"+abnf.group+")";

// 4.4 Obsolete Addressing
abnf.obs_addr_list="(?:(?:"+abnf.CFWS+",)*"+abnf.address+"(?:,(?:"+abnf.address+"|"+abnf.CFWS+")?)*)";

// 3.4 Address Specifications
abnf.address_list=abnf.obs_addr_list;

// 4.4 Obsolete Addressing

// 3.4 Address Specifications
// 3.6.1 The Origination Date field
abnf.orig_date="(?:Date:"+abnf.date_time+abnf.CRLF+")";

// 3.6.2 Originator Fields
abnf.from="(?:From:"+abnf.mailbox_list+abnf.CRLF+")";
abnf.sender="(?:Sender:"+abnf.mailbox+abnf.CRLF+")";
abnf.reply_to="(?:Reply-To:"+abnf.address_list+abnf.CRLF+")";

// 3.6.3 Destination Address Fields
abnf.to="(?:To:"+abnf.address_list+abnf.CRLF+")";
abnf.cc="(?:Cc:"+abnf.address_list+abnf.CRLF+")";
abnf.bcc="(?:Bcc:(?:"+abnf.address_list+"|"+abnf.CFWS+")?"+abnf.CRLF+")";

// 4.5.4 Obsolete Identification Fields
abnf.obs_id_left=abnf.local_part;
abnf.obs_id_right=abnf.domain;

// 3.6.4 Identification Fields
abnf.id_left=abnf.obs_id_left;
abnf.no_fold_literal="(?:\\["+abnf.dtext+"*\\])";
abnf.id_right=abnf.obs_id_right;
abnf.msg_id="(?:"+abnf.CFWS+"?\\<"+abnf.id_left+"@"+abnf.id_right+"\\>"+abnf.CFWS+"?)";
abnf.message_id="(?:Message-ID:"+abnf.msg_id+abnf.CRLF+")";
abnf.in_reply_to="(?:In-Reply-To:"+abnf.msg_id+"+"+abnf.CRLF+")";
abnf.references="(?:References:"+abnf.msg_id+"+"+abnf.CRLF+")";

// 3.6.5 Informational Fields
abnf.subject="(?:Subject:"+abnf.unstructured+abnf.CRLF+")";
abnf.comments="(?:Comments:"+abnf.unstructured+abnf.CRLF+")";
abnf.keywords="(?:Keywords:"+abnf.phrase+"(?:,"+abnf.phrase+")*"+abnf.CRLF+")";

// 3.6.6 Resent Fields
abnf.resent_date="(?:Resent-Date:"+abnf.date_time+abnf.CRLF+")";
abnf.resent_from="(?:Resent-From:"+abnf.mailbox_list+abnf.CRLF+")";
abnf.resent_sender="(?:Resent-Sender:"+abnf.mailbox+abnf.CRLF+")";
abnf.resent_to="(?:Resent-To:"+abnf.address_list+abnf.CRLF+")";
abnf.resent_cc="(?:Resent-Cc:"+abnf.address_list+abnf.CRLF+")";
abnf.resent_bcc="(?:Resent-Bcc:(?:"+abnf.address_list+"|"+abnf.CFWS+")?"+abnf.CRLF+")";
abnf.resent_msg_id="(?:Resent-Message-ID:"+abnf.msg_id+abnf.CRLF+")";

// 3.6.7 Trace Fields
abnf.path="(?:"+abnf.angle_addr+"|(?:"+abnf.CFWS+"?\\<"+abnf.CFWS+"?\\>"+abnf.CFWS+"?))";
abnf.received_token="(?:"+abnf.word+"|"+abnf.angle_addr+"|"+abnf.addr_spec+"|"+abnf.domain+")";
abnf.received="(?:Received:"+abnf.received_token+"*;"+abnf.date_time+abnf.CRLF+")";
abnf.return="(?:Return-Path:"+abnf.path+abnf.CRLF+")";
abnf.trace="(?:"+abnf.return+abnf.received+"+)";

// 3.6.8 Optional Fields
abnf.ftext="[\\x21-\\x39\\x3b-\\x7e]";
abnf.field_name="(?:"+abnf.ftext+"+)";
abnf.optional_field="(?:"+abnf.field_name+":"+abnf.unstructured+abnf.CRLF+")";

// 3.6 Field Definitions
abnf.fields="(?:"+abnf.optional_field+"*)";

// 3.5 Overall Message Syntax
abnf.text="[\\x01-\\x09\\x0b-\\x0c\\x0e-\\x7f]";

// 4.1 Miscellaneous Obsolete Tokens
abnf.obs_body="(?:(?:(?:"+abnf.LF+"*"+abnf.CR+"*"+"(?:(?:\\x00|"+abnf.text+")"+abnf.LF+"*"+abnf.CR+"*)*)|"+abnf.CRLF+")*)";

// 4.5.1 Obsolete Origination Date field
abnf.obs_orig_date="(?:Date"+abnf.WSP+"*:"+abnf.date_time+abnf.CRLF+")";

// 4.5.2 Obsolete Originator Fields
abnf.obs_from="(?:From"+abnf.WSP+"*:"+abnf.mailbox_list+abnf.CRLF+")";
abnf.obs_sender="(?:Sender"+abnf.WSP+"*:"+abnf.mailbox+abnf.CRLF+")";
abnf.obs_reply_to="(?:Reply-To"+abnf.WSP+"*:"+abnf.address_list+abnf.CRLF+")";

// 4.5.3 Obsolete Destination Address Fields
abnf.obs_to="(?:To"+abnf.WSP+"*:"+abnf.address_list+abnf.CRLF+")";
abnf.obs_cc="(?:Cc"+abnf.WSP+"*:"+abnf.address_list+abnf.CRLF+")";
abnf.obs_bcc="(?:Bcc"+abnf.WSP+"*:(?:"+abnf.address_list+"|(?:(?:"+abnf.CFWS+"?,)*"+abnf.CFWS+"?))"+abnf.CRLF+")";

// 4.5.4 Obsolete Identification Fields
abnf.obs_message_id="(?:Message-ID"+abnf.WSP+"*:"+abnf.msg_id+abnf.CRLF+")";
abnf.obs_in_reply_to="(?:In-Reply-To"+abnf.WSP+"*:(?:"+abnf.phrase+"|"+abnf.msg_id+")*"+abnf.CRLF+")";
abnf.obs_references="(?:References"+abnf.WSP+"*:(?:"+abnf.phrase+"|"+abnf.msg_id+")*"+abnf.CRLF+")";

// 4.1 Miscellaneous Obsolete Tokens
abnf.obs_phrase_list="(?:(?:"+abnf.phrase+"|"+abnf.CRLF+")?(?:,"+abnf.phrase+"|"+abnf.CFWS+")*)";

// 4.5.5 Obsolete Informational Fields
abnf.obs_subject="(?:Subject"+abnf.WSP+"*:"+abnf.unstructured+abnf.CRLF+")";
abnf.obs_comments="(?:Comments"+abnf.WSP+"*:"+abnf.unstructured+abnf.CRLF+")";
abnf.obs_keywords="(?:Keywords"+abnf.WSP+"*:"+abnf.obs_phrase_list+abnf.CRLF+")";

// 4.5.6 Resent Fields
abnf.obs_resent_date="(?:Resent-Date"+abnf.WSP+"*:"+abnf.date_time+abnf.CRLF+")";
abnf.obs_resent_from="(?:Resent-From"+abnf.WSP+"*:"+abnf.mailbox_list+abnf.CRLF+")";
abnf.obs_resent_send="(?:Resent-Sender"+abnf.WSP+"*:"+abnf.mailbox+abnf.CRLF+")";
abnf.obs_resent_to="(?:Resent-To"+abnf.WSP+"*:"+abnf.address_list+abnf.CRLF+")";
abnf.obs_resent_cc="(?:Resent-Cc"+abnf.WSP+"*:"+abnf.address_list+abnf.CRLF+")";
abnf.obs_resent_bcc="(?:Resent-Bcc"+abnf.WSP+"*:(?:"+abnf.address_list+"|(?:(?:"+abnf.CFWS+"?,)*"+abnf.CFWS+"?))"+abnf.CRLF+")";
abnf.obs_resent_mid="(?:Resent-Message-ID"+abnf.WSP+"*:"+abnf.msg_id+abnf.CRLF+")";
abnf.obs_resent_rply="(?:Resent-Reply-To"+abnf.WSP+"*:"+abnf.address_list+abnf.CRLF+")";

// 4.5.7 Obsolete Trace Fields
abnf.obs_return="(?:Return-Path"+abnf.WSP+"*:"+abnf.path+abnf.CRLF+")";
abnf.obs_received="(?:Received"+abnf.WSP+"*:"+abnf.received_token+"*;"+abnf.date_time+abnf.CRLF+")";

// 4.5.8 Optional Fields
abnf.obs_optional="(?:"+abnf.field_name+abnf.WSP+"*:"+abnf.unstructured+abnf.CRLF+")";

// 4.5 Obsolete Header Fields
abnf.obs_fields="(?:"+abnf.obs_optional+"*)";

// 3.5 Overall Message Syntax
abnf.body="(?:(?:(?:"+abnf.text+"{0,998}"+abnf.CRLF+")*"+abnf.text+"{0,998})|"+abnf.obs_body+")";
abnf.message="(?:(?:"+abnf.fields+"|"+abnf.obs_fields+")(?:"+abnf.CRLF+abnf.body+")?)";

/*************************/
/* From MIME RFCs (2045) */
/*************************/
abnf.tspecials="[\\x28\\x29\\x2c\\x2f\\x3a\\x3b\\x3c\\x3d\\x3e\\x3f\\x40\\x5b\\x5c\\x5d]";
abnf.tspecial_ws="(?:"+abnf.CFWS+"?"+abnf.tspecials+abnf.CFWS+"?)";
abnf.ttext="[\\x21-\\x27\\x2a\\x2b\\x2d\\x2e\\x30-\\x39\\x41-\\x5a\\x5e-\\x7e]";
abnf.token="(?:"+abnf.CFWS+"?"+abnf.ttext+"+"+abnf.CFWS+"?)";

var field_info={
	"date":{min:1, max:1},
	"from":{min:1, max:1},
	"reply-to":{min:0, max:1},
	"to":{min:0, max:1},
	"cc":{min:0, max:1},
	"bcc":{min:0, max:1},
	"message-id":{min:0, max:1},
	"in-reply-to":{min:0, max:1},
	"references":{min:0, max:1},
	"subject":{min:0, max:1},
};

function strip_CFWS(str)
{
	var strip=new RegExp("^"+abnf.CFWS+"*(.*?)"+abnf.CFWS+"*$","i");

	str=str.replace(strip,"$1");
	return(str);
}

function parse_header(str)
{
	var hdr={};
	var m;
	var re;

	re=new RegExp("^("+abnf.field_name+")"+abnf.WSP+"*:"+rfc5322abnf.unstructured+""+abnf.CRLF,"i");
	m=re.exec(str);
	if(m==null)
		return(undefined);
	hdr.orig=m[0];
	hdr.field=m[1].toLowerCase();
	return(hdr);
}

function parse_headers(str)
{
	var hdrs={};
	var hdr;

	hdrs["::"]=[];
	hdrs[":mime:"]=[];
	while(str.length > 0) {
		hdr=parse_header(str);
		if(hdr==undefined)
			break;
		hdrs["::"].push(hdr.orig);
		if(hdrs[hdr.field]==undefined)
			hdrs[hdr.field]=[];
		hdrs[hdr.field].push(hdr.orig);
		if(hdr.field=='mime-version' || hdr.field.substr(0, 8)=='content-')
			hdrs[":mime:"].push(hdr.orig);
		if(hdr.orig.length<=0)
			str='';
		str=str.substr(hdr.orig.length);
	}
	return(hdrs);
}

function get_next_symbol(str)
{
	var m;
	var ret={};
	var sym=new RegExp("^("+abnf.tspecial_ws+"|"+abnf.quoted_string+"|"+abnf.domain_literal+"|"+abnf.comment+"|"+abnf.token+")","i");
	var is_quoted=new RegExp("^"+abnf.quoted_string+"$","i");
	var old_qp=new RegExp(abnf.obs_qp, "i");

	m=sym.exec(str);
	if(m==null)
		return(undefined);
	ret.length=m[1].length;
	ret.sym=strip_CFWS(m[1]);
	if(ret.sym.search(is_quoted)!=-1) {
		ret.sym=ret.sym.replace(/^"(.*)"$/, "$1");
		ret.sym=ret.sym.replace(old_qp, "$1".substr(1));
	}
	if(ret.sym.search(new RegExp("^"+abnf.domain_literal+"$", "i"))!=-1) {
		ret.sym=ret.sym.replace(/^\[(.*)\]$/, "$1");
	}

	return(ret);
}

function parse_mime_attrs(attrs, str)
{
	var tmp;
	var key;

	while(str.length) {
		tmp=get_next_symbol(str);
		if(tmp==undefined || tmp.sym != ";")
			break;
		str=str.substr(tmp.length);

		tmp=get_next_symbol(str);
		if(tmp==undefined)
			break;
		str=str.substr(tmp.length);
		key=tmp.sym.toLowerCase();

		tmp=get_next_symbol(str);
		if(tmp==undefined || tmp.sym != "=")
			break;
		str=str.substr(tmp.length);

		tmp=get_next_symbol(str);
		if(tmp==undefined)
			break;
		str=str.substr(tmp.length);
		attrs[key]=tmp.sym;
	}
}

function parse_mime_header(hdrstr)
{
	var hdr={};
	var tmp;
	var m;
	var p;

	hdr.orig=hdrstr;
	hdr.attrs={};
	hdr.vals=[];

	m=new RegExp("^("+abnf.field_name+")"+abnf.WSP+"*:("+abnf.unstructured+")"+abnf.CRLF+"$","i").exec(hdrstr);
	if(m==null)
		return(undefined);

	hdr.name=m[1].toLowerCase();
	hdr.val=m[2];
	if(hdr.name != "content-description") {
		/* Strip comments */
		while((tmp=hdr.val.replace(new RegExp(abnf.comment,"i"),""))!=hdr.val)
			hdr.val=tmp;
	}
	/* Unwrap */
	hdr.val=hdr.val.replace(/\x0d\x0a[\t ]/," ");
	switch(hdr.name) {
		case 'mime-version':				// RFC 2045
			/* We don't do anything with this... */
			break;
		case 'content-type':				// RFC 2045
			p=hdr.val;
			/* type */
			tmp=get_next_symbol(p);
			if(tmp==undefined)
				return(undefined);
			hdr.vals.push(tmp.sym);
			p=p.substr(tmp.length);

			/* slash */
			tmp=get_next_symbol(p);
			if(tmp==undefined || tmp.sym != '/')
				return(undefined);
			p=p.substr(tmp.length);

			/* sub-type */
			tmp=get_next_symbol(p);
			if(tmp==undefined)
				return(undefined);
			hdr.vals.push(tmp.sym);
			p=p.substr(tmp.length);

			parse_mime_attrs(hdr.attrs,p);
			break;
		case 'content-transfer-encoding':	// RFC 2045
			/* Slash */
			tmp=get_next_symbol(hdr.val);
			if(tmp==undefined)
				return(undefined);
			hdr.vals.push(tmp.sym);
			break;
		case 'content-id':					// RFC 2045
			if(hdr.val.search(new RegExp("^"+abnf.msg_id+"$","i"))==-1)
				return(undefined);
			hdr.vals.push(strip_CFWS(hdr.val));
			break;
		case 'content-description':			// RFC 2045
			hdr.vals.push(strip_CFWS(hdr.val));
			break;
		case 'content-disposition':			// RFC 2183
			p=hdr.val;
			tmp=get_next_symbol(p);
			if(tmp==undefined)
				return(undefined);
			hdr.vals.push(tmp.sym);
			p=p.substr(tmp.length);
			parse_mime_attrs(hdr.attrs,p);
			break;
	}
	return(hdr);
}

function parse_message(str)
{
	var ret={headers:{},text:''};
	var tmp,tmp2;

	tmp=str.split(/\r\n\r\n/);
	ret.headers=parse_headers(tmp.shift()+"\r\n");
	ret.text=tmp.join("\r\n\r\n");

	//if(ret.headers[":mime:"].length > 0)
		ret.mime=parse_mime(ret.headers, ret.text);
	return(ret);
}

function parse_mime(hdrs, text)
{
	var i;
	var ret={};
	var tmp;
	var re;

	ret.parsed={};
	for(i in hdrs[":mime:"]) {
		tmp=parse_mime_header(hdrs[":mime:"][i]);
		if(tmp==undefined) {
			continue;
		}
		ret.parsed[tmp.name]=tmp;
	}

	// Set up defaults...
	if(ret.parsed['content-type']==undefined)
		ret.parsed['content-type']={attrs:{charset:"us-ascii"}, vals:["text","plain"]};

	// Decode content-transfer-encoding?	RFC 2045

	// Decode message/multipart 			RFC 2046
	if(ret.parsed['content-type'].vals[0]=="multipart") {
		if(ret.parsed["content-type"].attrs.boundary==undefined)
			return({});
		re=new RegExp("\x0d\x0a--"+ret.parsed["content-type"].attrs.boundary+"(?:--)?"+abnf.CFWS+"?\x0d\x0a","i");
		tmp=text.split(re);
		if(tmp.length < 2) {
			return({});
		}
		tmp.shift();
		tmp.pop();
		ret.parts=[];
		for(i in tmp)
			ret.parts.push(parse_message(tmp[i]));
	}

	return(ret);
}
