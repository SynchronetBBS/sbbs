if(user.number==0)
	login("Guest");
if(user.number==0) {
	write('<html><head><title>No Guest Access</title></head><body>Sorry, this BBS does not offer Guest access.</body></html>');
	exit(1);
}

function clean_id(id)
{
	return(id.replace(/[^A-Za-z0-9]/g,'_'));
}
