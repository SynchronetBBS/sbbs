<!-- $Id: rightnav.inc,v 1.1 2006/02/22 02:16:17 runemaster Exp $ -->

<!-- Begin Right Nav -->
<div id="more_invisible">
      @@JS:if(!template.node_list.length || http_request.virtual_path=="/nodelist.ssjs")'<!--';@@
	<div id="right_nodelistcontainer">
		<div id="right_nodelist">
			<table class="r_nodelist" cellpadding="0" cellspacing="1" border="0" width="100%">
				<tr>
				<th class="r_nodelist3">Who's Online</th>
				</tr>
				<tr>
					<td>
						<table class="r_nodelist2" cellpadding="3" cellspacing="3">
							<tbody>
							<<REPEAT node_list>>
							<tr>
								<td class="r_nodelist2"><a href="mailto:@@node_list:email@@">%%node_list:name%%</a> %%node_list:action%%
								</td>
							</tr>
							<<END REPEAT node_list>>
						</tbody>
					</table>
				</td>
			</tr>
		</table>

		</div>
	</div>

@@JS:if(!template.node_list.length || http_request.virtual_path=="/nodelist.ssjs")'-->';@@
	<div id="morecontainer">
		<div id="more">
			<table class="stats" border="0" cellpadding="0" cellspacing="1">
				<tr>
				<th class="stats3" colspan="2">Today's Stats</th>
				</tr>
				<tr>
				<td class="stats2">Uptime</td><td class="stats2">@@uptime@@</td>
				</tr>
				<tr>
				<td class="stats2">Time</td><td class="stats2">@@timeon_today@@ minutes</td>
				</tr>
				<tr>
				<td class="stats2">Logons</td><td class="stats2">@@logons_today@@</td>
				</tr>
				<tr>
				<td class="stats2">New Users</td><td class="stats2">@@new_users_today@@</td>
				</tr>
				<tr>
				<td class="stats2">Posts</td><td class="stats2">@@posted_today@@</td>
				</tr>
				<tr>
				<td class="stats2">E-Mails</td><td class="stats2">@@email_sent_today@@</td>
				</tr>
				<tr>
				<td class="stats2">Uploads</td><td class="stats2">@@uploaded_today@@</td>
				</tr>
				<tr>
				<td class="stats2">Downloads</td><td class="stats2">@@downloaded_today@@</td>
				</tr>
				<tr>
				<th class="stats3" colspan="2">System Totals</th>
				</tr>
				<tr>
				<td class="stats2">Time</td><td class="stats2">@@total_timeon@@ minutes</td>
				</tr>
				<tr>
				<td class="stats2">Logons</td><td class="stats2">@@total_logons@@</td>
				</tr>
				<tr>
				<td class="stats2">Messages</td><td class="stats2">@@total_messages@@</td>
				</tr>
				<tr>
				<td class="stats2">Users</td><td class="stats2">@@total_users@@</td>
				</tr>
				<tr>
				<td class="stats2">E-Mails</td><td class="stats2">@@total_email@@</td>
				</tr>
				<tr>
				<td class="stats2">Files</td><td class="stats2">@@total_files@@</td>
				</tr>
			</table>
		</div>
	</div>
</div>

<!-- End Right Nav -->

