load("html_inc/msgslib.ssjs");

if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

/* Ensure that offset is an even multiple of max_messages */
offset-=offset%max_messages;

if(offset<0)
	offset=0;

if(offset > 0)  {
	if(offset<max_messages)  {
		offset=max_messages;
	}
}
var currpage=Math.floor(offset/max_messages);
var total_pages=Math.floor(msgbase.total_msgs/max_messages);
var firstpage=0;
var lastpage=firstpage+max_pages-1;
if(lastpage>total_pages)
	lastpage=total_pages;
/* Ensure currpage is inside first/last */
var lcount=0;
while(currpage>lastpage) {
	lcount++;
	firstpage++;
	lastpage=firstpage+max_pages-1;
	if(lastpage>total_pages)
		lastpage=total_pages;
	if(lcount>5000)
		currpage=lastpage;
}

/* Try adjust so currpage is in the middle of firstpage and lastpage */
lcount=0;
while(currpage>firstpage+(max_pages/2) && lastpage<total_pages) {
	lcount++;
	firstpage++;
	lastpage=firstpage+max_pages-1;
	if(lastpage>total_pages)
		lastpage=total_pages;
	if(lcount>5000)
		break;
}

/* Build the links now */
template.pagelinks='';
for(var page=firstpage;page<=lastpage;page++) {
	if(currpage==page)
		template.pagelinks += '<b>'+(page+1)+'</b> ';
	else
		template.pagelinks += "<a href=\""+path+'?msg_grp='+g+'&msg_sub='+encodeURIComponent(sub)+'&offset='+(page*max_messages)+'">'+(page+1)+'</a> ';
}


hdr=msgbase.get_msg_header(true,msgbase.total_msgs-1-offset-max_messages);
if(hdr!=null)  {
	template.pagelinks+='<a href="'+path+'?msg_grp='+g+'&msg_sub='+encodeURIComponent(sub)+'&offset='+(offset+max_messages)+'">NEXT</a>';
}

if(offset>0) {
	template.pagelinks='<a href="'+path+'?msg_grp='+g+'&msg_sub='+encodeURIComponent(sub)+'&offset='+(offset-max_messages)+'">PREV</a> '+template.pagelinks;
}

if(sub=='mail') {
	template.title="Messages in E-Mail";
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.title="Messages in "+msg_area.grp_list[g].sub_list[s].description;
	template.sub=msg_area.grp_list[g].sub_list[s];
}

if(sub!='mail')  {
	if(! msg_area.grp_list[g].sub_list[s].can_read)  {
		error("You don't have sufficient rights to read this sub");
	}
}

write_template("header.inc");
last_offset=msgbase.total_msgs-1-offset;
var displayed=0;
template.messages=new Array;
template.group=msg_area.grp_list[g];

for(;displayed<max_messages && (hdr=msgbase.get_msg_header(true,last_offset)) != null;last_offset--) {
	if(hdr==null)
		continue;
	if(sub=='mail' && hdr.to!=user.alias && hdr.to!=user.name && hdr.to !=user.netmail)
		continue;
	template.messages[displayed.toString()]=hdr;
	displayed++;
}
write_template("msgs/msgs.inc");
write_template("footer.inc");
msgbase.close();
