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
                @@JS:if(user.number==0 || user.security.restrictions&UFLAG_G)'<a href="/login.ssjs">Login</a><a href="/newuser.ssjs">New User'; else'<a href="/members/userlist.ssjs">User Listing</a><a href="/members/info.ssjs">Information</a><a href="/members/themes.ssjs">Change Theme</a><a href="/msgs/msgs.ssjs?msg_sub=mail">E-mail</a>';@@</a>  
                @@JS:if(user.number || (this.login!=undefined && system.matchuser("Guest")))'<a href="/msgs">Message Groups</a>'@@ 
                <a href="%%ftp_url%%">File Libraries</a>
            </p>
        </div>
        
<!-- start Nodelisting -->

      @@JS:if(template.node_list.length)'Who\'s Online now ...<br /><br /><table class="main_nodelist"><tbody><<REPEAT node_list>><tr><td class="main_nodelist"><a href="mailto:@@node_list:email@@">%%node_list:name%%</a> %%node_list:action%%</td></tr><<END REPEAT node_list>></tbody></table>';@@
 
     </div>
     </td>                        
<!-- end Left Navigation -->
            
