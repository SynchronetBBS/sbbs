/*******************************************************************************
This work is copyright Michael J. Ryan (c) 2006

It is licenced under the GNU General Public License (GPL, version 2)
http://www.gnu.org/licenses/gpl.html
--------------------------------------------------------------------------------

FILE NAME	:	user_info_to_sysop.js

CVS ID		:	$Id: user_info_to_sysop.js,v 1.2 2010/07/09 21:09:47 deuce Exp $

CREATED		:	2006-04-20
BY			:	Michael J. Ryan <tracker1(at)theroughnecks(dot)net>

TABSTOPS	:	4

REQUIRES	:	sbbsdefs.js	- must be loaded before this file

MIN VERSION	:	Synchronet 3.12b

--------------------------------------------------------------------------------

Utility function for sending the details of a user to the sysop,
It's primary purpose would be to send information for a new user.

*******************************************************************************/

function sendUserInfoToSysop(u, subject, note) {
	var hdr = {
		to: system.operator.toLowerCase(),
		to_ext: '1',
		from: u.alias,
		replyto: u.alias,
		subject: subject
	};
	if (u.netmail != '')
		hdr.replyto_net_addr = u.netmail

	var msg = "" +
		"Alias         : " + u.alias+"  ("+u.security.password+")\r\n" +
		"Real name     : " + u.name+"\r\n" +
		"Chat handle   : " + u.handle+"\r\n" +
		"Location      : " + u.location+"\r\n" +
		"Gender        : " + ((u.gender=="F")?"Female":"Male")+"\r\n" +
		"Date of birth : " + u.birthdate+"\r\n" +
		"Email         : " + u.netmail +
		(((u.settings & USER_NETMAIL) != 0)?" (forwarded)":"") + "\r\n" +
		"---------------------------------------------------------------------------\r\n" +
		note + "\r\n";

	var mail = new MsgBase("mail");
	if(mail.open != undefined && mail.open() == false) {

		var err_msg = "!ERROR " + mail.error;
		log(err_msg);

		throw err_msg;
	}
	mail.save_msg(hdr,msg);
	mail.close();

}
