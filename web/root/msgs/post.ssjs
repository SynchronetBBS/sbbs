load("html_inc/msgslib.ssjs");

template.group=msg_area.grp_list[g];

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.grp_list[g].sub_list[s];
}

if(sub!='mail') {
	if(! msg_area.grp_list[g].sub_list[s].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}
template.title="Post a message";
write_template("header.inc");
write_template("msgs/post.inc");
write_template("footer.inc");

msgs_done();
