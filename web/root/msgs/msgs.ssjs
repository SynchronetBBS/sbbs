load("html_inc/msgslib.ssjs");
load("html_inc/mime_decode.ssjs");

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
var msgcount=0;
var msgarray;
if(sub=='mail') {
	msgarray=get_my_message_offsets();
}
else {
	msgarray=get_all_message_offsets();
}
var total_pages=Math.floor(msgarray.length/max_messages);
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
		template.pagelinks += "<a href=\""+path+'?msg_grp='+g+'&amp;msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(page*max_messages)+'">'+(page+1)+'</a> ';
}

if(offset+max_messages < msgarray.length)  {
	template.pagelinks+='<a href="'+path+'?msg_grp='+g+'&amp;msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(offset+max_messages)+'">NEXT</a>';
}

if(offset>0) {
	template.pagelinks='<a href="'+path+'?msg_grp='+g+'&amp;msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(offset-max_messages)+'">PREV</a> '+template.pagelinks;
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
last_offset=msgarray.length-1-offset;

template.messages=new Array;
template.group=msg_area.grp_list[g];

for(displayed=0;displayed<max_messages && last_offset >= 0 && msgarray[last_offset].hdr != null;last_offset--) {
	if(msgarray[last_offset].hdr.subject=='')
		msgarray[last_offset].hdr.subject="-- No Subject --";
	template.messages[displayed.toString()]=msgarray[last_offset].hdr;
	template.messages[displayed.toString()].attachments=count_attachments(msgarray[last_offset].hdr,msgbase.get_msg_body(true,msgarray[last_offset].offset,true,true));
	template.messages[displayed.toString()].offset=msgarray[last_offset].offset;
	displayed++;
}

write_template("msgs/msgs.inc");
write_template("footer.inc");
msgbase.close();
