<!-- $Id$ -->

    <!-- Left Navigation (Includes Main table start) -->
        
<table align="center" bgcolor="#EEEEEE" width="90%"  border="0" cellspacing="0" cellpadding="0">
  <tr>
    <td width="1%" valign="top" align="left"><img alt="" src="/graphics/tl_curve_grey.gif" height="6" width="6" /></td>
    <td>
    <table width="100%"  border="0" cellspacing="0" cellpadding="0">
  <tr>
  
    <td valign="top" width="1%">
    <div id="pageNav"> 
        <div id="sectionLinks"> 
        	<p>
           		<a href="/nodelist.ssjs">Who's Online</a>
           		@@JS:if(user.number==0 || user.security.restrictions&UFLAG_G)'<a href="/login.ssjs">Login</a><a href="/newuser.ssjs">New User'; else'<a href="/members/userlist.ssjs">User Listing</a><a href="/members/info.ssjs">Information Menu</a><a href="/members/themes.ssjs">Change Your HTML Theme</a><a href="/msgs/msgs.ssjs?msg_sub=mail">E-mail</a>';@@</a>  
           		@@JS:if(user.number || (this.login!=undefined && system.matchuser("Guest")))'<a href="/msgs">Message Groups</a>'@@ 
           		<a href="%%ftp_url%%">File Libraries</a>
        	</p>
		</div> 

<!-- start Nodelisting -->

      @@JS:if(template.show_nodelist)'Who\'s Online now ...<br /><br /><table class="main_nodelist"><tbody><tr><td class="main_nodelist"><a href="mailto:'+template.user_email+'">'+template.user_name+'</a> '+template.node_action+'</td></tr></tbody></table></div>';@@
                        
<!-- end Left Navigation -->
            
