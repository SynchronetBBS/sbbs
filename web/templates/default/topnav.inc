<!-- $Id$ -->

    <!-- Top Navigation -->
    
        <table align="center" width="90%"  border="0" cellspacing="0" cellpadding="0">
  <tr>
    <td class="ltn"><img src="/graphics/tnav_left.gif" width="5" height="32" alt="" /></td>
    <td class="tn">
      @@JS:if(http_request.virtual_path=="/index.ssjs" || http_request.virtual_path=="/")'<span class="tlink">Home</span>'; else if(http_request.virtual_path!="/msgs/management.ssjs")'<a class="tlink" href="/">Home</a>';@@
      @@JS:if(http_request.virtual_path=="/members/userlist.ssjs")'<span class="tlink">User List</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/newuser.ssjs")'<span class="tlink">Create New User</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/members/info.ssjs")'<span class="tlink">Information Menu</span>'; else if(http_request.virtual_path=="/members/userstats.ssjs" || http_request.virtual_path=="/members/sysinfo.ssjs")'<a class="tlink" href="/members/info.ssjs">Information Menu</a>';@@
      @@JS:if(http_request.virtual_path=="/members/userstats.ssjs")'<span class="tlink">Your Information</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/members/sysinfo.ssjs")'<span class="tlink">System Information</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/members/themes.ssjs")'<span class="tlink">HTML Themes</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/msgs/index.ssjs" || http_request.virtual_path=="/msgs")'<span class="tlink">Message Groups</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/msgs/subs.ssjs")'<a class="tlink" href="/msgs">Message Groups</a><span class="tlink">%%group:description%%</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/msgs/subinfo.ssjs" && sub!='mail')'<a class="tlink" href="/msgs">Message Groups</a><a class="tlink" href="/msgs/subs.ssjs?msg_grp='+template.sub.name+'">%%sub:description%%</a><span class="tlink">Sub Information</span>'; else '';@@
      @@JS:if(http_request.virtual_path=="/msgs/msgs.ssjs" && sub!='mail')'<a class="tlink" href="index.ssjs">Message Groups</a><a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a><a class="tlink" href="subinfo.ssjs?msg_sub=^^sub:code^^" title="Click for Sub Info">%%sub:description%%</a>'; else if(http_request.virtual_path=="/msgs/msgs.ssjs")'<a class="tlink" href="subinfo.ssjs?msg_sub=^^sub:code^^" title="Click for Sub Info">%%sub:description%%</a>';@@
      @@JS:if(http_request.virtual_path=="/msgs/msg.ssjs" && sub!='mail')'<a class="tlink" href="index.ssjs">Message Groups</a><a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a><a class="tlink" href="msgs.ssjs?msg_sub=^^sub:code^^">%%sub:description%%</a><span class="tlink">Reading Messages</span>'; else if(http_request.virtual_path=="/msgs/msg.ssjs")'<a class="tlink" href="msgs.ssjs?msg_sub=^^sub:code^^">%%sub:description%%</a><span class="tlink">Reading E-Mail</span>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")'<a href="/msgs" class="tlink">Message Groups</a>'; else if(http_request.virtual_path=="/msgs/post.ssjs")'<a class="tlink" href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")'<a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")'<a class="tlink" href="msgs.ssjs?msg_grp='+template.sub.name+'">'+template.sub.description+'</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/post.ssjs")'<span class="tlink">Posting Message</span>'; else if(http_request.virtual_path=="/msgs/post.ssjs")'<span class="tlink">Creating E-Mail</span>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")'<a href="/msgs" class="tlink">Message Groups</a>'; else if(http_request.virtual_path=="/msgs/reply.ssjs")'<a class="tlink" href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")'<a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")'<a class="tlink" href="msgs.ssjs?msg_grp='+template.sub.name+'">'+template.sub.description+'</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/reply.ssjs")'<span class="tlink">Replying to Message</span>'; else if(http_request.virtual_path=="/msgs/reply.ssjs")'<span class="tlink">Replying to E-Mail</span>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/management.ssjs")'<span class="tlink">Deleting Message</span>'; else if(http_request.virtual_path=="/msgs/management.ssjs")'<span class="tlink" >Deleting E-Mail</span>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")'<a href="/msgs" class="tlink">Message Groups</a>'; else if(http_request.virtual_path=="/msgs/savemsg.ssjs")'<a class="tlink" href="/msgs/msgs.ssjs?msg_sub=mail">Personal E-Mail</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")'<a class="tlink" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")'<a class="tlink" href="msgs.ssjs?msg_grp='+template.sub.name+'">'+template.sub.description+'</a>';@@
      @@JS:if(sub!='mail' && http_request.virtual_path=="/msgs/savemsg.ssjs")'<span class="tlink">Posted Message</span>'; else if(http_request.virtual_path=="/msgs/savemsg.ssjs")'<span class="tlink">Sent E-Mail</span>';@@
      
      
    </td>
    <td class="rtn"><img src="/graphics/tnav_right.gif" width="5" height="32" alt="" /></td>
  </tr>
</table>
<br />

    <!-- end Top Navigation --> 
    
