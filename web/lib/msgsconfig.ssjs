/* $Id: msgsconfig.ssjs,v 1.12 2006/01/31 23:19:29 runemaster Exp $ */

max_messages=20;
max_pages=20;
next_msg_html='<img src="' + template.image_dir + '/next1.gif" alt="Next Message" title="Next Message" border="0" />';
no_next_msg_html='<img src="' + template.image_dir + '/next1_light.gif" alt="No More Messages" title="No More Messages" />';
prev_msg_html='<img src="' + template.image_dir + '/prev1.gif" alt="Previous Message" title="Previous Message" border="0" />';
no_prev_msg_html='<img src="' + template.image_dir + '/prev1_light.gif" alt="No More Messages" title="No More Messages" />';
next_page_html="NEXT";
prev_page_html="PREV";
showall_subs_enable_html="Show all subs";
showall_subs_disable_html="Show subs in new scan only";
show_messages_all_html="Show all messages";
show_messages_yours_html="Show messages to you only";
show_messages_your_unread_html="Show unread messages to you only";
show_messages_spacer_html="&nbsp;<b>|</b>&nbsp;";
anon_only_message="Message will be posted anonymously";
anon_allowed_message='<input type="checkbox" name="anonymous" value="Yes" /> Post message anonymously';
anon_reply_message='<input type="checkbox" name="anonymous" value="Yes" checked /> Post message anonymously';
private_only_message="Message will be marked private";
private_allowed_message='<input type="checkbox" name="private" value="Yes" /> Mark message as private';
private_reply_message='<input type="checkbox" name="private" value="Yes" checked /> Mark message as private';
