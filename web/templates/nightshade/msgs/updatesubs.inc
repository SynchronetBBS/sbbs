<!-- $Id: updatesubs.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  

<!-- Main Content -->  

		<div id="contentcontainer">
			<div id="content2">
    		<div id="subslist">
<table class="sublist" cellpadding="2" cellspacing="1" border="0" width="100%">
<tr>
    <th class="sublist2lft">Sub-board</th>
    <th class="sublist2">Messages</th>
    <th class="sublist2" align="right">Latest</th>
	<th class="sublist2">Scan Setting</th>
</tr>
<<REPEAT subs>>
<tr>
    <td class="sublist2">%%subs:description%%</td>
    <td class="sublist2right">%%subs:messages%%</td>
    <td class="sublist2right" nowrap="nowrap">%%subs:lastmsg%%</td>
    <td class="sublist2" align="center">
        @@JS:if(RepeatObj.ischecked == 1)'Off'; else if(RepeatObj.ischecked == 2)'On'; else if(RepeatObj.ischecked == 3)'To You Only';@@
    </td></tr>
<<END REPEAT subs>>
<tr>
<td class="sublist2" colspan="4" align="center">
  <a href="/msgs">Back To Message Groups</a>
</td>
</tr>
</table>
	</div>
	</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

   



