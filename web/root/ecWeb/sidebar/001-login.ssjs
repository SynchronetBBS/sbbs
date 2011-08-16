/* login.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* A sidebar widget to generate either a login form (with a signup link) or a
   list of functions for logged-in users. */

if(user.alias.toUpperCase() != webIni.guestUser.toUpperCase()) {
	print("You are logged in as <b>" + user.alias + "</b><br />");
	print("<script language=javascript type=text/javascript>document.write('<a class=link href=./?logout=true&callback=' + location.pathname + location.search + '>Log out</a>');</script><br />");
	print("<br /><a class=link href=./pages.ssjs?page=" + webIni.forumPage + "&action=newMessageScan>Scan for new messages</a>");
	print("<br /><a class=link href=./pages.ssjs?page=" + webIni.forumPage + "&action=viewSubBoard&subBoard=mail>Check email</a>");
} else {
	print("<form action=./ method=post>");
	print("Username:<br /><input type=text name=username class=standardFont /><br /><br />");
	print("Password:<br /><input type=password name=password class=standardFont /><br /><br />");
	print("<script language=javascript type=text/javascript>document.write('<input type=hidden name=callback value=' + window.location + ' />');</script>");
	print("<input type=submit value='Log in' />");
	print("</form>");
	print("<a class=link href=./newUser.ssjs>Register</a>");
	if(http_request.query.hasOwnProperty('loginfail')) print("<br /><i>Invalid username or password</i>");
}

