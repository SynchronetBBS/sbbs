/* $Id$ */

var is_sysop=false;

if(http_request.header.referer!=undefined) {
var p = http_request.header.referer.indexOf("/members/");
var virtual_referer=http_request.header.referer.slice(p)
}

if(user.number==1 || user.security.level>=90)
	is_sysop=true;

template.topnav=new Array;

if(http_request.virtual_path=="/index.ssjs" || http_request.virtual_path=="/")
    template.topnav.push({html: '<span class="tlink">Home</span>'});
else
    template.topnav.push({html: '<a class="tlink" href="/">Home</a>'});

if(http_request.virtual_path=="/members/newpw.ssjs")
    template.topnav.push({html: '<span class="tlink">Changing Password</span>'});

if(http_request.virtual_path=="/members/changepw.ssjs")
    template.topnav.push({html: '<span class="tlink">Password Change Result</span>'});
    
if(http_request.virtual_path=="/nodelist.ssjs")
    template.topnav.push({html: '<span class="tlink">Who\'s Online</span>'});

if(http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer=="/members/userlist.ssjs") {
	if(is_sysop) {
		if(http_request.virtual_path=="/members/viewprofile.ssjs")
		    template.topnav.push({html: '<span class="tlink">View/Edit Profile</span>'});
		} else {
		if(http_request.virtual_path=="/members/viewprofile.ssjs")
		    template.topnav.push({html: '<span class="tlink">Viewing Profile</span>'});
		}
}	

if(http_request.virtual_path=="/members/userlist.ssjs")
    template.topnav.push({html: '<span class="tlink">User List</span>'});

if(http_request.virtual_path=="/newuser.ssjs")
    template.topnav.push({html: '<span class="tlink">Create New User</span>'});

if(http_request.virtual_path=="/members/lastcallers.ssjs")
    template.topnav.push({html: '<span class="tlink">Last Callers</span>'});

if(http_request.virtual_path=="/irc/index.ssjs")
    template.topnav.push({html: '<span class="tlink">IRC Chat</span>'});

if(http_request.virtual_path=="/members/info.ssjs")
    template.topnav.push({html: '<span class="tlink">Information Menu</span>'});
else if(http_request.virtual_path=="/members/userstats.ssjs" || http_request.virtual_path=="/members/sysinfo.ssjs" || http_request.virtual_path=="/members/editprofile.ssjs" || (http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer!="/members/userlist.ssjs"))
    template.topnav.push({html: '<a class="tlink" href="/members/info.ssjs">Information Menu</a>'});

if(is_sysop) {
if(http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer!="/members/userlist.ssjs")
    template.topnav.push({html: '<span class="tlink">View/Edit Profile</span>'});
} else {
if(http_request.virtual_path=="/members/viewprofile.ssjs" && virtual_referer!="/members/userlist.ssjs")
    template.topnav.push({html: '<span class="tlink">Viewing Profile</span>'});
}

if(http_request.virtual_path=="/members/userstats.ssjs")
    template.topnav.push({html: '<span class="tlink">Your Information</span>'});

if(http_request.virtual_path=="/members/sysinfo.ssjs")
    template.topnav.push({html: '<span class="tlink">System Information</span>'});

if(http_request.virtual_path=="/members/editprofile.ssjs")
    template.topnav.push({html: '<span class="tlink">Edit Your Profile</span>'});

if(http_request.virtual_path=="/members/themes.ssjs")
    template.topnav.push({html: '<span class="tlink">Choose HTML Theme</span>'});

if(http_request.virtual_path=="/members/picktheme.ssjs")
    template.topnav.push({html: '<span class="tlink">HTML Theme Changed</span>'});

if(http_request.virtual_path=="/msgs/index.ssjs" || http_request.virtual_path=="/msgs")
    template.topnav.push({html: '<span class="tlink">Message Groups</span>'});

if(http_request.virtual_path=="/msgs/choosegroup.ssjs")
    template.topnav.push({html: '<span class="tlink">Choose Group to Set Scan</span>'});
    
if(http_request.virtual_path=="/msgs/choosesubs.ssjs")
    template.topnav.push({html: '<span class="tlink">Selecting Subs to Display</span>'});
    
if(http_request.virtual_path=="/msgs/updatesubs.ssjs")
    template.topnav.push({html: '<span class="tlink">Subs to Display Updated</span>'});
    
if(http_request.virtual_path=="/msgs/subs.ssjs")
    template.topnav.push({html: '<a class="tlink" href="/msgs">Message Groups</a><span class="tlink">'+template.group.description+'</span>'});

if(http_request.virtual_path=="/msgs/subinfo.ssjs" && sub!='mail') {
    template.topnav.push({html: '<a class="tlink" href="/msgs">Message Groups</a><a class="tlink" href="/msgs/subs.ssjs?msg_grp='+template.sub.grp_name+'">'+template.sub.grp_name+'</a>'});
    template.topnav.push({html: '<a class="tlink" href="/msgs/msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a><span class="tlink">Sub Information</span>'});
}

if(http_request.virtual_path=="/msgs/msgs.ssjs" && sub!='mail')
    template.topnav.push({html: '<a class="tlink" href="index.ssjs">Message Groups</a><a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a><a class="tlink" href="subinfo.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'" title="Click for Sub Info">'+template.sub.description+'</a>'});
else if(http_request.virtual_path=="/msgs/msgs.ssjs")
    template.topnav.push({html: '<a class="tlink" href="subinfo.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'" title="Click for Sub Info">'+template.sub.description+'</a>'});

if(http_request.virtual_path=="/msgs/msg.ssjs" && sub!='mail')
    template.topnav.push({html: '<a class="tlink" href="index.ssjs">Message Groups</a><a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a><a class="tlink" href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a><span class="tlink">Reading Messages</span>'});
else if(http_request.virtual_path=="/msgs/msg.ssjs")
    template.topnav.push({html: '<a class="tlink" href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a><span class="tlink">Reading E-Mail</span>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a href="/msgs" class="tlink">Message Groups</a>'});
else if(http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a class="tlink" href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<a class="tlink" href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<span class="tlink">Posting Message</span>'});
else if(http_request.virtual_path=="/msgs/post.ssjs")
    template.topnav.push({html: '<span class="tlink">Creating E-Mail</span>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a href="/msgs" class="tlink">Message Groups</a>'});
else if(http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a class="tlink" href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<a class="tlink" href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<span class="tlink">Replying to Message</span>'});
else if(http_request.virtual_path=="/msgs/reply.ssjs")
    template.topnav.push({html: '<span class="tlink">Replying to E-Mail</span>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/management.ssjs")
    template.topnav.push({html: '<span class="tlink">Deleting Message</span>'});
else if(http_request.virtual_path=="/msgs/management.ssjs")
    template.topnav.push({html: '<span class="tlink" >Deleting E-Mail</span>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a href="/msgs" class="tlink">Message Groups</a>'});
else if(http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a class="tlink" href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<a class="tlink" href="msgs.ssjs?msg_sub='+encodeURIComponent(template.sub.code)+'">'+template.sub.description+'</a>'});

if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<span class="tlink">Posted Message</span>'});
else if(http_request.virtual_path=="/msgs/savemsg.ssjs")
    template.topnav.push({html: '<span class="tlink">Sent E-Mail</span>'});

write_template("topnav.inc");
