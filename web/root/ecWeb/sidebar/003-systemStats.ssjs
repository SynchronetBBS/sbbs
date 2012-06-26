/* systemStats.ssjs, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

/* A basic sidebar widget to display selected system statistics. Nothing
   special, more of an example of something you can do with the sidebar. */
   
print("<table border=0 cellpadding=0 cellspacing=0 class='standardColor standardFont'>");
print("<tr><td>Sysop:</td><td>&nbsp;" + system.operator + "</td></tr>");
print("<tr><td>Location:</td><td>&nbsp;" + system.location + "</td></tr>");
print("<tr><td>Users:</td><td>&nbsp;" + system.stats.total_users + "</td></tr>");
print("<tr><td>Nodes:</td><td>&nbsp;" + system.nodes + "</td></tr>");
print("<tr><td>Uptime:</td><td>&nbsp;" + system.secondstr(time() - system.uptime) + "</td></tr>");
print("<tr><td>Calls:</td><td>&nbsp;" + system.stats.total_logons + "</td></tr>");
print("<tr><td>Calls today:</td><td>&nbsp;" + system.stats.logons_today + "</td></tr>");
print("<tr><td>Files:</td><td>&nbsp;" + system.stats.total_files + "</td></tr>");
print("<tr><td>U/L today:</td><td>&nbsp;" + system.stats.files_uploaded_today + " (" + system.stats.bytes_uploaded_today + " bytes)</td></tr>");
print("<tr><td>D/L today:</td><td>&nbsp;" + system.stats.bytes_downloaded_today + " (" + system.stats.bytes_downloaded_today + " bytes)</td></tr>");
print("<tr><td>Messages:</td><td>&nbsp;" + system.stats.total_messages + "</td></tr>");
print("<tr><td>Posts today:</td><td>&nbsp;" + system.stats.messages_posted_today + "</td></tr>");
print("</table>");