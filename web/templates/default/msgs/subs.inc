<!-- $Id: subs.inc,v 1.8 2006/01/31 23:14:21 runemaster Exp $ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />

<table class="sublist" cellpadding="2" cellspacing="2">
<tr>
    <th class="sublistlft">Sub-board</th>
    <th class="sublist">Messages</th>
    <th class="sublist" align="right">Latest</th>
</tr>
<<REPEAT subs>>
<tr>
    <td class="sublist"><a class="msglist" href="msgs.ssjs?msg_sub=^^subs:code^^">%%subs:description%%</a></td>
    <td class="sublistright"><a class="msglistnu" href="subinfo.ssjs?msg_sub=^^subs:code^^" title="Click for Sub Info">%%subs:messages%%</a></td>
    <td class="sublistright" nowrap="nowrap">%%subs:lastmsg%%</td>
</tr>
<<END REPEAT subs>>
</table>
@@JS:template.showall_toggle==undefined ? '' : '<p class="msgcenter">['+template.showall_toggle+']</p>'@@
<br />

