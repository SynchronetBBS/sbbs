<!-- $Id: choosesubs.inc,v 1.5 2011/12/02 03:17:33 deuce Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

<form action="/msgs/updatesubs.ssjs" method="post">
<input type="hidden" value="%%group:name%%" name="msg_grp" />
<table class="sublist" cellpadding="2" cellspacing="2">
<tr>
    <th class="sublistlft">Sub-board</th>
    <th class="sublist">Messages</th>
    <th class="sublist" align="right">Latest</th>
    <th class="sublist"><input name="SelectAll" type="radio" onclick="checkBoxes(this.form, this.checked, 1)" /> Off</th>
    <th class="sublist"><input name="SelectAll" type="radio" onclick="checkBoxes(this.form, this.checked, 2)" /> On</th>
    <th class="sublist"><input name="SelectAll" type="radio" onclick="checkBoxes(this.form, this.checked, 3)" /> To You Only</th>
</tr>
<<REPEAT subs>>
<tr>
    <td class="sublist">%%subs:description%%</td>
    <td class="sublistright">%%subs:messages%%</td>
    <td class="sublistright" nowrap="nowrap">%%subs:lastmsg%%</td>
    <td class="sublist">
      <input name="sub-%%subs:code%%" type="radio" value="1" @@JS:if(RepeatObj.ischecked==1)' checked="checked"';@@ />
    </td>
    <td class="sublist">
      <input name="sub-%%subs:code%%" type="radio" value="2" @@JS:if(RepeatObj.ischecked==2)' checked="checked"';@@ />
    </td>
    <td class="sublist">  
      <input name="sub-%%subs:code%%" type="radio" value="3" @@JS:if(RepeatObj.ischecked==3)' checked="checked"';@@ />
    </td>
</tr>
<<END REPEAT subs>>
<tr>
  <td colspan="6" class="sublist" align="center"><input type="image" src="@@image_dir@@/update_new_scan.gif" value="Update New Scan" /></td>
</tr>
</table>
</form>
<br />

<script type="text/javascript">
<!--
    function checkBoxes (form, check, value) {
    for (var c = 0; c < form.elements.length; c++)
        if (form.elements[c].type == 'radio'  && form.elements[c].value == value)
        form.elements[c].checked = check;
}
// -->
</script>

