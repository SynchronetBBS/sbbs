<!-- $Id: updatesubs.inc,v 1.3 2006/01/31 23:14:21 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

<table class="sublist" cellpadding="2" cellspacing="2">
<tr>
    <th class="sublistlft">Sub-board</th>
    <th class="sublist">Messages</th>
    <th class="sublist" align="right">Latest</th>
    <th class="sublist">Scan Setting</th>
    </tr>
<<REPEAT subs>>
<tr>
    <td class="sublist">%%subs:description%%</td>
    <td class="sublistright">%%subs:messages%%</td>
    <td class="sublistright" nowrap="nowrap">%%subs:lastmsg%%</td>
    <td class="sublist" align="center">
        @@JS:if(RepeatObj.ischecked == 1)'Off'; else if(RepeatObj.ischecked == 2)'On'; else if(RepeatObj.ischecked == 3)'To You Only';@@
    </td>
</tr>
<<END REPEAT subs>>
<tr>
<td class="sublist" colspan="4" align="center">
  <a href="/msgs">Back To Message Groups</a>
</td>
</tr>
</table>
<br />


