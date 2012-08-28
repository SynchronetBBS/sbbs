if(user.alias.toUpperCase() != webIni.WebGuest.toUpperCase()) {
	print("You are logged in as <b>" + user.alias + "</b><br />");
	print("<script language='javascript' type='text/javascript'>");
	print("document.write('<a class=link href=./?logout=true&callback=' + location.pathname + location.search + '>Log out</a>');");
	print("</script><br />");
} else {
	print("<form action=./ method=post>");
	print("Username:<br /><input class='border' type='text' name='username' /><br /><br />");
	print("Password:<br /><input class='border' type='password' name='password' /><br /><br />");
	print("<script language='javascript' type='text/javascript'>");
	print("document.write('<input type=hidden name=callback value=' + window.location + ' />');");
	print("</script>");
	print("<input class='border' type='submit' value='Log in' />");
	print("</form>");
	print("<a class='link' href='./index.xjs?page=999-newUser.ssjs'>Register</a>");
	if(http_request.query.hasOwnProperty('loginfail'))
		print("<br /><i>Invalid username or password</i>");
}