<p class="navigation">
    <a class="navigation" href="http://^^JS:system.inet_addr^^/">Home</a> : <a class="navigation" href="/members/">Members</a> : 
    <a class="navigation" href="index.ssjs">Groups</a> : @@JS:if(template.group.name!="E-Mail") '<a class="navigation" href="subs.ssjs?msg_grp='+template.group.name+'">'+template.group.description+'</a> :'; else '';@@
    <a class="navigation" href="subinfo.ssjs?msg_sub=^^sub:code^^" title="Click for Sub Info">%%sub:description%%</a> -&#062;
    <a class="navigation" href="post.ssjs?msg_sub=^^sub:code^^&amp;post=new">Post New Message</a> : (@@show_choice@@)</p>
<form action="management.ssjs" method="post">
<input type="hidden" name="msg_sub" value="^^sub:code^^" />
<table class="msglist" cellpadding="5" cellspacing="2" border="0" width="100%">
<tr class="msglist">
     @@JS:template.can_delete?'<th class="msglist">X</th>':''@@
    <th class="msglist"><img src="/graphics/attach_black.gif" title="Attachment?" alt="Attachment?" width="10" height="13" border="0" /></th>
    <th class="msglist">Subject</th>
    <th class="msglist">From</th>
    <th class="msglist">To</th>
    <th class="msglist">Date</th>
</tr>
    <!-- %%JS:template.color=0%% -->
    <<REPEAT messages>>
    <tr class=@@JS:((template.color++)%2==0)?'"msglist1"':'"msglist2"';@@>
    @@JS:template.can_delete?'<td><input name="number" value="^^messages:number^^" type="checkbox"></td>':''@@
	   <td>
    		@@JS:if(@@messages:attachments@@>0) '<img alt="Attachment" src="/graphics/attach_black.gif" />'; else '&nbsp;';@@
	   </td>
	   <td>
		  <a class="msglist" href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^messages:number^^">%%messages:subject%%</a>
	   </td>
	   <td>
		  %%messages:from%%
	   </td>
	   <td>
		  %%messages:to%%
	   </td>
	   <td nowrap="nowrap">
		  %%JS:system.timestr(@@messages:when_written_time@@)%%
	   </td>
    </tr>
    <<END REPEAT messages>>
    </table>
    <br />
    <div align="center"><input class="msg" type="submit" name="Action" value="Delete Message(s)" /></div>
</form>
<div align="center">@@pagelinks@@</div>
<br />

