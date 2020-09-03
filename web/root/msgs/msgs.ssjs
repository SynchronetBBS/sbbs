/* $Id: msgs.ssjs,v 1.44 2018/06/30 19:41:49 rswindell Exp $ */

load("../web/lib/msgslib.ssjs");
load("../web/lib/mime_decode.ssjs");

if(msgbase.open!=undefined && msgbase.open()==false) {
    error(msgbase.last_error);
}

var ShowMsgs=GET_ALL_MESSAGES;
var MAX_NAME_LEN=25;
var MAX_SUBJ_LEN=70;

if(file_exists(prefs_dir +format("%04d.html_prefs",user.number))) {
  prefsfile=new File(prefs_dir +format("%04d.html_prefs",user.number));
  if(prefsfile.open("r",false)) {
  SortDate=prefsfile.iniGetValue('Messaging', 'SortDate', 'descending');
  prefsfile.close();
  }
} else {
  SortDate="descending";
}

if(SortDate=="ascending")
  DateDescending=false;
else
  DateDescending=true;

if(http_request.query.show_messages != undefined) {
    switch(http_request.query.show_messages[0]) {
        case 'All':
            ShowMsgs=GET_ALL_MESSAGES;
            break;
        case 'Yours':
            ShowMsgs=GET_MY_MESSAGES;
            break;
        case 'YourUnread':
            ShowMsgs=GET_MY_UNREAD_MESSAGES;
            break;
    }
}

var new_query='';
for(key in http_request.query) {
    if(key != 'show_messages') {
        if(new_query.length>0)
            new_query+='&amp;';
        new_query+=encodeURIComponent(key);
        new_query+='=';
        new_query+=encodeURIComponent(http_request.query[key]);
    }
}
if(user.security.restrictions&UFLAG_G)
    ShowMsg=GET_ALL_MESSAGES;
else {
    if(new_query.length>0)
        new_query+='&amp;';
    new_query+='show_messages=';
    if(sub=='mail') {
        switch(ShowMsgs) {
            case GET_ALL_MESSAGES:
            case GET_MY_MESSAGES:
                template.show_choice='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'YourUnread">'+show_messages_your_unread_html+'</a>';
                break;
            case GET_MY_UNREAD_MESSAGES:
                template.show_choice='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'All">'+show_messages_all_html+'</a>';
                break;
        }
    } else {
        switch(ShowMsgs) {
            case GET_ALL_MESSAGES:
                template.show_choice='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'Yours">'+show_messages_yours_html+'</a>';
                template.show_choice+=show_messages_spacer_html;
                template.show_choice+='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'YourUnread">'+show_messages_your_unread_html+'</a>';
                break;
            case GET_MY_MESSAGES:
                template.show_choice='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'All">'+show_messages_all_html+'</a>';
                template.show_choice+=show_messages_spacer_html;
                template.show_choice+='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'YourUnread">'+show_messages_your_unread_html+'</a>';
                break;
            case GET_MY_UNREAD_MESSAGES:
                template.show_choice='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'All">'+show_messages_all_html+'</a>';
                template.show_choice+=show_messages_spacer_html;
                template.show_choice+='<a class="tlink2" href="'+http_request.virtual_path+'?'+new_query+'Yours">'+show_messages_yours_html+'</a>';
                break;
        }
    }
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
msgarray=get_message_offsets(ShowMsgs);

if(sub=='mail')
    template.can_delete=true;
else
    template.can_delete=msg_area.sub[sub].is_operator;
var total_pages=Math.floor(msgarray.length/max_messages+(msgarray.length%max_messages?1:0));
if(total_pages==0)
    total_pages=1;
var firstpage=0;
var lastpage=firstpage+max_pages-1;
if(lastpage>=total_pages)
    lastpage=total_pages-1;
/* Ensure currpage is inside first/last */
var lcount=0;
while(currpage>lastpage) {
    lcount++;
    firstpage++;
    lastpage=firstpage+max_pages-1;
    if(lastpage>=total_pages)
        lastpage=total_pages-1;
    if(lcount>5000)
        currpage=lastpage;
}

/* Try adjust so currpage is in the middle of firstpage and lastpage */
lcount=0;
while(currpage>firstpage+(max_pages/2) && lastpage<(total_pages-1)) {
    lcount++;
    firstpage++;
    lastpage=firstpage+max_pages-1;
    if(lastpage>=total_pages)
        lastpage=total_pages-1;
    if(lcount>5000)
        break;
}

/* I could not get this to work with:           */
/* file.open(file_exists(fname) ? "r+":"w+");   */

if(http_request.query.sort_date != undefined) {
  switch(http_request.query.sort_date[0]) {
    case "ascending":
      DateDescending=false;
      SortDate="ascending"
      prefsfile=new File(prefs_dir + format("%04d.html_prefs",user.number));
      if(prefsfile.open("r+",false)) {
          prefsfile.iniSetValue('Messaging', 'SortDate', 'ascending');
          prefsfile.close();
      } else {
      if(prefsfile.open("w+",false)) {
          prefsfile.iniSetValue('Messaging', 'SortDate', 'ascending');
          prefsfile.close();
        }
      }
      break;
    case "descending":
      DateDescending=true;
      SortDate="descending"
      prefsfile=new File(prefs_dir + format("%04d.html_prefs",user.number));
      if(prefsfile.open("r+",false)) {
          prefsfile.iniSetValue('Messaging', 'SortDate', 'descending');
          prefsfile.close();
        } else {
      if(prefsfile.open("w+",false)) {
          prefsfile.iniSetValue('Messaging', 'SortDate', 'descending');
          prefsfile.close();
        }
      }
      break;
  }
}

if(DateDescending)
  template.sort_date='<a href="' + path + '?msg_sub=' + encodeURIComponent(sub) + '&amp;offset=' + (offset) + '&amp;sort_date=ascending">Date</a>';
else
  template.sort_date='<a href="' + path + '?msg_sub=' + encodeURIComponent(sub) + '&amp;offset=' + (offset) + '&amp;sort_date=descending">Date</a>';

/* Build the links now */

template.pagelinks='';
if(total_pages>1) {
    for(var page=firstpage;page<=lastpage;page++) {
        if(currpage==page)
            template.pagelinks += '<b>'+(page+1)+'</b> ';
        else
            template.pagelinks += "<a href=\""+path+'?msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(page*max_messages)+'&amp;sort_date=' + SortDate +'">'+(page+1)+'</a> ';
    }
    if(DateDescending) {
    if(offset+max_messages < msgarray.length)  {
        template.pagelinks+='<a href="'+path+'?msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(offset+max_messages)+'&amp;sort_date=' + SortDate +'">'+next_page_html+'</a>';
    }
    if(offset>0) {
        template.pagelinks='<a href="'+path+'?msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(offset-max_messages)+'&amp;sort_date=' + SortDate +'">'+prev_page_html+'</a> '+template.pagelinks;
    }
} else {
    if(offset+max_messages < msgarray.length)  {
        template.pagelinks+='<a href="'+path+'?msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(offset+max_messages)+'&amp;sort_date=' + SortDate +'">'+next_page_html+'</a>';
    }
    if(offset>0) {
        template.pagelinks='<a href="'+path+'?msg_sub='+encodeURIComponent(sub)+'&amp;offset='+(offset-max_messages)+'&amp;sort_date=' + SortDate +'">'+prev_page_html+'</a> '+template.pagelinks;
    }
  }
}

if(sub=='mail') {
    template.title="Messages in E-Mail";
    template.sub=new Object;
    template.sub.description="Personal E-Mail";
    template.sub.code="mail";
}
else {
    template.title="Messages in "+msg_area.sub[sub].description;
    template.sub=msg_area.sub[sub];
}

if(sub!='mail')  {
    if(! msg_area.sub[sub].can_read)  {
        error("You don't have sufficient rights to read this sub");
    }
}

last_offset=msgarray.length-1-offset;
first_offset = offset;
max_first_offset= offset+msgarray.length-1-offset;
max_last_offset = last_offset;


template.messages=new Array;
if(sub=='mail') {
    template.group=new Object;
    template.group.name="E-Mail";
    template.group.description="E-Mail";
}
else {
    template.group=msg_area.grp[msg_area.sub[sub].grp_name];
}

if(DateDescending) {

  for(displayed=0;displayed<max_messages && last_offset >= 0;last_offset--) {
     var hdr=clean_msg_headers(msgbase.get_msg_header(true,msgarray[last_offset].offset),0);
     if(hdr==null)
          continue;
     template.messages[displayed.toString()]=hdr;
	 template.messages[displayed.toString()].to=template.messages[displayed.toString()].to.substr(0,MAX_NAME_LEN);
	 template.messages[displayed.toString()].from=template.messages[displayed.toString()].from.substr(0,MAX_NAME_LEN);
	 template.messages[displayed.toString()].subject=template.messages[displayed.toString()].subject.substr(0,MAX_SUBJ_LEN);
     template.messages[displayed.toString()].attachments=count_attachments(hdr,msgbase.get_msg_body(true,msgarray[last_offset].offset,hdr));
     template.messages[displayed.toString()].offset=msgarray[last_offset].offset;
     displayed++;
  }
} else {

  for(displayed = 0;displayed < max_messages && first_offset <= max_first_offset; first_offset++) {
      var hdr=clean_msg_headers(msgbase.get_msg_header(true,msgarray[first_offset].offset),0);
      if(hdr==null)
          continue;
	  template.messages[displayed.toString()]=hdr;
	  template.messages[displayed.toString()].to=template.messages[displayed.toString()].to.substr(0,MAX_NAME_LEN);
	  template.messages[displayed.toString()].from=template.messages[displayed.toString()].from.substr(0,MAX_NAME_LEN);
	  template.messages[displayed.toString()].subject=template.messages[displayed.toString()].subject.substr(0,MAX_SUBJ_LEN);
	  template.messages[displayed.toString()].attachments=count_attachments(hdr,msgbase.get_msg_body(true,msgarray[first_offset].offset,hdr));
      template.messages[displayed.toString()].offset=msgarray[first_offset].offset;
      displayed++;
  }
}

if(sub=='mail') {
    template.can_post=!(user.security.restrictions&UFLAG_E);
    template.post_button_image="new_e-mail.gif";
} else {
    template.can_post=msg_area.sub[sub].can_post;
    template.post_button_image="new_message.gif";
}           

if(http_request.query["sort"]!=undefined)
	template.messages.sort(alphasort);

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
	load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("msgs/msgs.inc");
if(do_footer)
	write_template("footer.inc");
msgs_done();

function alphasort (a,b)
{
	if(http_request.query["sort"]==undefined)
		return(0);
	var sortby=http_request.query["sort"]
	var au;
	var bu;
	if(a[sortby].toUpperCase!=undefined)
		au=a[sortby].toUpperCase();
	else
		au=a[sortby];
	if(b[sortby].toUpperCase!=undefined)
		bu=b[sortby].toUpperCase();
	else
		bu=b[sortby];
	if(au<bu)
		return -1;
	if(bu>au)
		return 1;
	return 0;
}
