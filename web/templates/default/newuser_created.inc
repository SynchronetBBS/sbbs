<!-- $Id$ -->

    <!-- Top Navigation -->
    
        <table align="center" width="90%"  border="0" cellspacing="0" cellpadding="0">
  <tr>
    <td class="ltn"><img src="/graphics/gblnav_left.gif" width="5" height="32"/></td>
    <td class="tn">
      <span class="tlink">Home</span>
    </td>
    <td class="rtn"><img src="/graphics/glbnav_right.gif" width="5" height="32"/></td>
  </tr>
</table>
<br />

    <!-- end Top Navigation --> 
    
     
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
     </div>
     <br />
           
                   
<!-- end Left Navigation -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

Your account has been created and the password has been mailed to: %%posted:netmail%%<br />
You can Login using your Username and the supplied password by using the Login button on the left.

<!-- end Main Content --> 