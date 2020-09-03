/* $Id: topnav_html.ssjs,v 1.20 2011/07/02 18:28:12 ree Exp $ */

var is_sysop=false;

if(http_request.header.referer!=undefined) {
var p = http_request.header.referer.indexOf("/members/");
var virtual_referer=http_request.header.referer.slice(p)
}

if(user.number==1 || user.security.level>=90)
	is_sysop=true;

template.topnav=new Array;

if(http_request.virtual_path=="/index.ssjs" || http_request.virtual_path=="/")
    template.topnav.push({html: 'Home'});
else
    template.topnav.push({html: '<a href="/">Home</a>'});

if(http_request.virtual_path=="/members/newpw.ssjs")
    template.topnav.push({html: 'Changing Password'});

if(http_request.virtual_path=="/members/changepw.ssjs")
    template.topnav.push({html: 'Password Change Result'});
    
if(http_request.virtual_path=="/nodelist.ssjs")
    template.topnav.push({html: 'Who\'s Online'});

if(http_request.virtual_path=="/ftelnet.ssjs")
    template.topnav.push({html: 'Flash Telnet'});
	
if(http_request.virtual_path=="/members/externals.ssjs")
    template.topnav.push({html: 'External Program Section'});

if(http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer=="/members/userlist.ssjs") {
	if(is_sysop) {
		if(http_request.virtual_path=="/members/viewprofile.ssjs")
		    template.topnav.push({html: 'View/Edit Profile'});
		} else {
		if(http_request.virtual_path=="/members/viewprofile.ssjs")
		    template.topnav.push({html: 'Viewing Profile'});
		}
}	

if(http_request.virtual_path=="/members/userlist.ssjs")
    template.topnav.push({html: 'User List'});

if(http_request.virtual_path=="/newuser.ssjs")
    template.topnav.push({html: 'Create New User'});

if(http_request.virtual_path=="/members/lastcallers.ssjs")
    template.topnav.push({html: 'Last Callers'});

if(http_request.virtual_path=="/irc/index.ssjs")
    template.topnav.push({html: 'IRC Chat'});

if(http_request.virtual_path=="/members/info.ssjs")
    template.topnav.push({html: 'Information Menu'});
else if(http_request.virtual_path=="/members/userstats.ssjs" || http_request.virtual_path=="/members/sysinfo.ssjs" || http_request.virtual_path=="/members/editprofile.ssjs" || (http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer!="/members/userlist.ssjs"))
    template.topnav.push({html: '<a href="/members/info.ssjs">Information Menu</a>'});

if(is_sysop) {
if(http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer!="/members/userlist.ssjs")
    template.topnav.push({html: 'View/Edit Profile'});
} else {
if(http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer!="/members/userlist.ssjs")
    template.topnav.push({html: 'Viewing Profile'});
}

if(http_request.virtual_path=="/members/userstats.ssjs")
    template.topnav.push({html: 'Your Information'});

if(http_request.virtual_path=="/members/sysinfo.ssjs")
    template.topnav.push({html: 'System Information'});

if(http_request.virtual_path=="/members/editprofile.ssjs")
    template.topnav.push({html: 'Edit Your Profile'});

if(http_request.virtual_path=="/members/themes.ssjs")
    template.topnav.push({html: 'Choose HTML Theme'});

if(http_request.virtual_path=="/members/picktheme.ssjs")
    template.topnav.push({html: 'HTML Theme Changed'});

if(http_request.virtual_path=="/msgs/index.ssjs" || http_request.virtual_path=="/msgs")
    template.topnav.push({html: 'Message Groups'});

if(http_request.virtual_path=="/msgs/choosegroup.ssjs")
    template.topnav.push({html: 'Choose Group to Set Scan'});
    
if(http_request.virtual_path=="/msgs/choosesubs.ssjs")
    template.topnav.push({html: 'Selecting Subs to Display'});
    
if(http_request.virtual_path=="/msgs/updatesubs.ssjs")
    template.topnav.push({html: 'Subs to Display Updated'});
    
if(http_request.virtual_path=="/msgs/subs.ssjs")
    template.topnav.push({html: '<a href="/msgs">Message Groups</a>'+template.group.description});

if(http_request.virtual_path=="/msgs/subinfo.ssjs" && sub!='mail') {
    template.topnav.push({html: '<a href="/msgs">Message Groups</a><a href="/msgs/subs.ssjs?msg_grp='+template.sub.grp_name+'">'+template.sub.grp_name+'</a>'});
    template.topnav.push({html: '<a href="/msgs/msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>Sub Information'});
}

if(http_request.virtual_path=="/msgs/msgs.ssjs" && sub!='mail')
    template.topnav.push({html: '<a href="index.ssjs">Message Groups</a><a href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a><a href="subinfo.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'" title="Click for Sub Info">'+template.sub.description+'</a>'});
else if(http_request.virtual_path=="/msgs/msgs.ssjs")
    template.topnav.push({html: '<a href="subinfo.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'" title="Click for Sub Info">'+template.sub.description+'</a>'});

if(http_request.virtual_path=="/msgs/msg.ssjs" && sub!='mail')
    template.topnav.push({html: '<a href="index.ssjs">Message Groups</a><a href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a><a href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>Reading Messages'});
else if(http_request.virtual_path=="/msgs/msg.ssjs")
    template.topnav.push({html: '<a href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>Reading E-Mail'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a href="/msgs">Message Groups</a>'});
else if(http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: 'Posting Message'});
else if(http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: 'Creating E-Mail'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a href="/msgs">Message Groups</a>'});
else if(http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: 'Replying to Message'});
else if(http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: 'Replying to E-Mail'});

if(sub!='mail' && http_request.virtual_path=="/msgs/management.ssjs")
    template.topnav.push({html: 'Deleting Message'});
else if(http_request.virtual_path=="/msgs/management.ssjs")
    template.topnav.push({html: 'Deleting E-Mail'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a href="/msgs">Message Groups</a>'});
else if(http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: 'Posted Message'});
else if(http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: 'Sent E-Mail'});

write_template("topnav.inc");
