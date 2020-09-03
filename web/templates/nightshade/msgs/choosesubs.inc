<!-- $Id: choosesubs.inc,v 1.2 2011/12/02 03:17:47 deuce Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer">
	<div id="content2">
	<div id="subslist">
	<div align="center">

<form action="/msgs/updatesubs.ssjs" method="post">
<input type="hidden" value="%%group:name%%" name="msg_grp" />
<table class="sublist" cellpadding="2" cellspacing="1">
<tr>
    <th class="sublist2lft">Sub-board</th>
    <th class="sublist2">Messages</th>
    <th class="sublist2" align="right">Latest</th>
    <th class="sublist2"><input name="SelectAll" type="radio" onclick="checkBoxes(this.form, this.checked, 1)" /> Off</th>
    <th class="sublist2"><input name="SelectAll" type="radio" onclick="checkBoxes(this.form, this.checked, 2)" /> On</th>
    <th class="sublist2"><input name="SelectAll" type="radio" onclick="checkBoxes(this.form, this.checked, 3)" /> To You Only</th>
</tr>
<<REPEAT subs>>
<tr>
    <td class="sublist2">%%subs:description%%</td>
    <td class="sublist2right">%%subs:messages%%</td>
    <td class="sublist2right" nowrap="nowrap">%%subs:lastmsg%%</td>
    <td class="sublist2">
      <input name="sub-%%subs:code%%" type="radio" value="1" @@JS:if(RepeatObj.ischecked==1)' checked="checked"';@@ />
    </td>
    <td class="sublist2">
      <input name="sub-%%subs:code%%" type="radio" value="2" @@JS:if(RepeatObj.ischecked==2)' checked="checked"';@@ />
    </td>
    <td class="sublist2">  
      <input name="sub-%%subs:code%%" type="radio" value="3" @@JS:if(RepeatObj.ischecked==3)' checked="checked"';@@ />
    </td>
</tr>
<<END REPEAT subs>>
<tr>
  <td colspan="6" class="sublist2center" align="center"><input type="submit" value="Update New Scan" /></td>
</tr>
</table>
</form>
		</div>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<script type="text/javascript">
<!--
    function checkBoxes (form, check, value) {
    for (var c = 0; c < form.elements.length; c++)
        if (form.elements[c].type == 'radio'  && form.elements[c].value == value)
        form.elements[c].checked = check;
}
// -->
</script>

