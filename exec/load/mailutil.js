// mailutil.js

// Parses Internet mail and USENET article header fields 
// for use with newsutil.js and mailproc_util.js

// $Id$

//Michael J. Ryan - 2004-04-16 - tracker1(at)theroughnecks.net
// gets the name portion for the "to/from"
function mail_get_name(strIn) {
	var reName1 = /[^\"]*\"([^\"]*)\".*/	//quoted name
	var reName2 = /(\S[^<]+)\s+<.*/			//unquoted name
	var reName3 = /[^<]*<([^@>]+).*/		//first part of <email address>
	var reName4 = /([^@]+)*@.*/				//first part of email address
	if (reName1.test(strIn)) return strIn.replace(reName1,"$1");
	if (reName2.test(strIn)) return strIn.replace(reName2,"$1");
	if (reName3.test(strIn)) return strIn.replace(reName3,"$1");
	if (reName4.test(strIn)) return strIn.replace(reName4,"$1");
	return strIn; //original string
}

//Michael J. Ryan - 2004-04-16 - tracker1(at)theroughnecks.net
// gets the address portion for the "to/from"
function mail_get_address(strIn) {
	var reEmail1 = /[^<]*<([^>]+)>.*/
	var reEmail2 = /([^@]+@.*)/
	if (strIn.match(reEmail1)) return strIn.replace(reEmail1,"$1");
	if (strIn.match(reEmail2)) return strIn.replace(reEmail2,"$1");
	return null;
}