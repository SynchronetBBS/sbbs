<!-- $Id$ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />


<div align="center">
@@JS:template.can_post?'<form name="PostMsg" action="post.ssjs?msg_sub=^^sub:code^^&amp;post=new" method="post"><input type="submit" value="'+template.post_button+'" /></form>':''@@
@@JS:template.show_choice==undefined ? '' : '(@@show_choice@@)<br /><br />'@@
</div>

<form name="DeleteMsg" action="management.ssjs" method="post">
<input type="hidden" name="msg_sub" value="^^sub:code^^" />
<table class="msglist" cellpadding="5" cellspacing="2" border="0" width="100%">
<tr class="msglist">
     @@JS:template.can_delete?'<th class="msglist"><input name="SelectAll" title="Select All Messages" type="checkbox" onclick="checkBoxes(this.form, this.checked)" /></th>':''@@
    <th class="msglist"><img src="/graphics/attach_black.gif" title="Attachment?" alt="Attachment?" width="10" height="13" border="0" /></th>
    <th class="msglist">Subject</th>
    <th class="msglist">From</th>
    <th class="msglist">To</th>
    <th class="msglist">Date</th>
</tr>

    <!-- %%JS:template.color=0%% -->
    <<REPEAT messages>>
    <tr class=@@JS:((template.color++)%2==0)?'"msglist1"':'"msglist2"';@@>
    @@JS:template.can_delete?'<td align=center><input name="number" value="^^messages:number^^" type="checkbox"></td>':''@@
	   <td align="center">
    		@@JS:if(@@messages:attachments@@>0) '<img alt="Attachment" src="/graphics/attach_black.gif" />'; else '&nbsp;';@@
	   </td>
	   <td>
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '<b>';@@
		  <a class="msglist" href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^messages:number^^">%%messages:subject%%</a>
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '</b>';@@
	   </td>
	   <td>
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '<b>';@@
		  %%messages:from%%
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '</b>';@@
	   </td>
	   <td>
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '<b>';@@
		  %%messages:to%%
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '</b>';@@
	   </td>
	   <td align="right" nowrap="nowrap">
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '<b>';@@
		  %%JS:system.timestr(@@messages:when_written_time@@)%%
          @@JS:if(sub=='mail' && !(@@messages:attr@@ & MSG_READ)) '</b>';@@
	   </td>
    </tr>
    <<END REPEAT messages>>
    </table>
    <br />
    @@JS:template.can_delete?'<div align="center"><input class="msg" type="submit" name="Action" value="Delete Selected Message(s)" /></div>':''@@
</form>
		
<script type="text/javascript">
<!--
	function checkBoxes (form, check) {
	for (var c = 0; c < form.elements.length; c++)
		if (form.elements[c].type == 'checkbox')
		form.elements[c].checked = check;
}
// -->
</script>

		
<div align="center">@@pagelinks@@</div>
<br />

<!-- end Main Content -->

