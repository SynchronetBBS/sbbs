/* $Id$ */

template.leftnav=new Array;

if(user.number==0 || user.security.restrictions&UFLAG_G)
    template.leftnav.push({html: '<a href="/login.ssjs">Login</a><a href="/newuser.ssjs">New User</a>' });
else
    template.leftnav.push({html: '<a href="/members/userlist.ssjs">User Listing</a><a href="/members/info.ssjs">Information</a><a href="/members/themes.ssjs">Change Theme</a><a href="/members/newpw.ssjs">Change Password</a><a href="/msgs/msgs.ssjs?msg_sub=mail">E-mail</a>' });  

if(user.number || (this.login!=undefined && system.matchuser("Guest")))
    template.leftnav.push({html: '<a href="/msgs">Message Groups</a>' });

if(user.number==0 || user.security.restrictions&UFLAG_G) {
    }
else
    template.leftnav.push({html: '<a href="/msgs/choosegroup.ssjs">Set Message Scan</a>' });
if(user.number==0 || user.security.restrictions&UFLAG_G) {
    }
else
    template.leftnav.push({ html: '<a href="' + template.ftp_url + template.ftpqwk + '">Download QWK Packet</a>' });
        
write_template("leftnav.inc");