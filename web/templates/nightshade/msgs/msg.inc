<!-- $Id: msg.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer2">
	<div id="content2">
	<div id="msg">
<table class="msgtableA" width="100%" cellspacing="1" cellpadding="3" border="0">
	<tr>
		<th class="msgtableB" align="left" width="150" height="26" nowrap="nowrap"><img src="@@image_dir@@/profile.gif" alt="Profile" />&nbsp;Author</th>
		<th class="msgtableB" align="left" nowrap="nowrap"><img src="@@image_dir@@/msg.gif" alt="Message" />&nbsp;Message Subject: %%hdr:subject%%</th>
	</tr>
	<tr>
		<td valign="top" class="msgtableC" rowspan="4">
			<strong>%%hdr:from%%</strong>@@JS:if(!display_info)'<br /><br />';@@@@author_ismod@@@@JS:if(!display_info)'<!-- ';@@<img class="msgtableD" src="%%author_avatar%%" alt="Author's Avatar" border="0" height="67" width="74" /><br /><br />Joined: @@author_firston@@<br />Posts:  @@author_posts@@<br /> 
			<br /><a href="/members/viewprofile.ssjs?showuser=@@u_num@@"><img src="@@image_dir@@/icon_profile.gif" alt="View user's profile" title="View user's profile" border="0" /></a><br /><br /><span style="font-size:10px;">@@author_ip@@</span>@@JS:if(!display_info)'-->';@@
		</td>
		<td class="msgtableB" height="28" valign="top">
			<table class="msgtableB" width="100%" border="0" cellspacing="0" cellpadding="0">		
				<tr>
					<td class="msgtableB" width="100%">
						<img src="@@image_dir@@/icon_minipost.gif" width="12" height="9" alt="" title="" border="0" /><strong>Posted: </strong>%%JS:system.timestr(template.hdr.when_written_time)%%&nbsp; &nbsp; &nbsp;<img src="@@image_dir@@/icon_minipost.gif" width="12" height="9" alt="" title="" border="0" /><strong>Posted To: </strong>%%hdr:to%%
				</td>		
				</tr>
				<tr>
					<td class="msgtableB" colspan="2"><hr /></td>
				</tr>
				<tr>
    				@@JS:template.txtbodybgwht?'<td class="txtbodywht" colspan="4"><pre class="leftalignwht">':'<td width="14%" colspan="4" class="txtbody"><pre class="leftalign">'@@@@body@@</pre></td>
				</tr>
			</table>
		</td>
	</tr>
	@@JS:if(template.replyto!=undefined && sub!='mail') '<tr class="msgtableB"><td class="msgtableB">In Reply To: <a href="msg.ssjs?msg_sub='+sub+'&amp;message='+template.replyto.number+'">'+template.replyto.subject+'</a> ('+template.replyto.from+')</td></tr>'; else '';@@
	@@JS:if(template.replies==undefined || template.replies.length<=0) '<!--'; else '';@@
	<tr class="msgtableB"><td class="msgtableB">Replies: <<REPEAT replies>><a href="msg.ssjs?msg_sub=^^sub:code^^&amp;message=^^replies:number^^">%%replies:subject%%</a> (%%replies:from%%)<<END REPEAT replies>></td></tr>
	@@JS:if(template.replies==undefined || template.replies.length<=0) '-->'; else '';@@
	@@JS:if(template.attachments==undefined) '<!--'; else '';@@
	<tr class="msgtableB">Attachments: <br />
	<<REPEAT attachments>>
		<a href="attachments.ssjs/^^sub:code^^/^^hdr:number^^/^^attachments:name^^">%%attachments:name%%</a><br />
	<<END REPEAT attachments>>
	</td>
	</tr>
	@@JS:if(template.attachments==undefined) '-->'; else '';@@
	<tr align="center">
		<td class="msgtableB" colspan="2" height="28">
			@@prevlink@@ @@JS:template.can_post?'<a href="reply.ssjs?msg_sub='+template.sub.code+'&amp;reply_to='+template.hdr.number+'"><img src="' + template.image_dir + '/reply.gif" alt="Reply" border="0" title="Reply" /></a>':'&nbsp;'@@ @@JS:template.can_delete?'<a href="management.ssjs?Action=Delete&amp;msg_sub='+template.sub.code+'&amp;number='+template.hdr.number+'"><img src="' + template.image_dir + '/topic_delete.gif" alt="Delete" border="0" title="Delete" /></a>':'&nbsp;'@@ @@JS:template.can_post?'<a href="post.ssjs?msg_sub='+template.sub.code+'&amp;post=new"><img src="' + template.image_dir + '/msg_newpost.gif" alt="Post New" border="0" title="Post New" /></a>':'&nbsp;'@@ @@nextlink@@
		</td>
	</tr>
</table>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content -->

