<!-- $Id: subs.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  

		<div id="contentcontainer">
			<div id="content2">
    		<div id="subslist">
<table class="sublist" cellpadding="2" cellspacing="1" border="0" width="100%">
<tr>
    <th class="sublist2lft">Sub-board</th>
    <th class="sublist2">Messages</th>
    <th class="sublist2" align="right">Latest</th>
</tr>
<<REPEAT subs>>
<tr>
    <td class="sublist2"><a class="msglist" href="msgs.ssjs?msg_sub=^^subs:code^^">%%subs:description%%</a></td>
    <td class="sublist2right"><a class="msglistnu" href="subinfo.ssjs?msg_sub=^^subs:code^^" title="Click for Sub Info">%%subs:messages%%</a></td>
    <td class="sublist2right" nowrap="nowrap">%%subs:lastmsg%%</td>
</tr>
<<END REPEAT subs>>
</table>
@@JS:template.showall_toggle==undefined ? '' : '<p class="msgcenter">['+template.showall_toggle+']</p>'@@
	</div>
	</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>
