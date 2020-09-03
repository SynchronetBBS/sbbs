<!-- $Id: msgs.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer">
	<div id="content2">
	<div id="msgs">

<div align="center">
@@JS:template.can_post?'<form name="PostMsg" action="post.ssjs?msg_sub='+template.sub.code+'&amp;post=new" method="post"><input type="submit"  value="New Message" /></form>':''@@
@@JS:template.show_choice==undefined ? '' : '(' + template.show_choice + ')<br /><br />'@@
</div>

<form name="DeleteMsg" action="management.ssjs?Action=Delete&amp;msg_sub=^^sub:code^^" method="post">
<!-- <input type="hidden" name="msg_sub" value="^^sub:code^^" /> -->
<table class="msglistA" cellpadding="2" cellspacing="1" border="0" width="100%">
<tr class="msglistB">
     @@JS:template.can_delete?'<th class="msglist"><input name="SelectAll" title="Select All Messages" type="checkbox" onclick="checkBoxes(this.form, this.checked)" /></th>':''@@
    <th class="msglistB"><img src="@@image_dir@@/attach_black.gif" title="Attachment?" alt="Attachment?" width="10" height="13" border="0" /></th>
    <th class="msglistB"><a href="msgs.ssjs?msg_sub=^^sub:code^^&amp;sort=subject">Subject</a></th>
    <th class="msglistB"><a href="msgs.ssjs?msg_sub=^^sub:code^^&amp;sort=from">From</a></th>
    <th class="msglistB"><a href="msgs.ssjs?msg_sub=^^sub:code^^&amp;sort=to">To</a></th>
    <th class="msglistB">@@sort_date@@</th>
</tr>

    <!-- %%JS:template.color=0%% -->
    <<REPEAT messages>>
    <tr class=@@JS:((template.color++)%2==0)?'"msglist1"':'"msglist2"';@@>
    @@JS:template.can_delete?'<td align=center><input name="number" value="'+RepeatObj.number+'" type="checkbox" /></td>':''@@
       <td align="center">
            @@JS:if(RepeatObj.attachments>0) '<img alt="Attachment" src="' + template.image_dir + '/attach_black.gif" />'; else '&nbsp;';@@
       </td>
       <td>
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '<b>';@@
          <a href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^messages:number^^">%%messages:subject%%</a>
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '</b>';@@
       </td>
       <td>
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '<b>';@@
          %%messages:from%%
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '</b>';@@
       </td>
       <td>
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '<b>';@@
          %%messages:to%%
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '</b>';@@
       </td>
       <td align="right" nowrap="nowrap">
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '<b>';@@
          %%JS:if(system.settings&SYS_EURODATE) strftime("%d %b %Y, %H:%M",(RepeatObj.when_written_time)); else strftime("%b %d %Y, %H:%M",(RepeatObj.when_written_time));%%
          @@JS:if(sub=='mail' && !(RepeatObj.attr & MSG_READ)) '</b>';@@
       </td>
    </tr>
    <<END REPEAT messages>>
    </table>
	<br />
    @@JS:template.can_delete?'<div align="center"><input class="msg" type="submit" value="Delete Message(s)" /></div>':''@@
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
		
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content -->

