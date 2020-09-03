<!-- $Id: userlist.inc,v 1.7 2006/01/31 23:13:43 runemaster Exp $ -->

<!-- Main Content -->  
    
<td class="main" valign="top"><br />

<table class="userlist" cellpadding="2" cellspacing="2">
<tr>
    <th class="userlist"><a class="userlist" href="userlist.ssjs?sort=alias">Alias</a></th>
    <th class="userlist"><a class="userlist" href="userlist.ssjs?sort=location">Location</a></th>
    <th class="userlist"><a class="userlist" href="userlist.ssjs?sort=connection">Connection</a></th>
    <th class="userlist"><a class="userlist" href="userlist.ssjs?sort=laston">Last On</a></th>
</tr>
<<REPEAT users>>
<tr>
  <td class="userlist">@@users:alias@@</td>
  <td class="userlist">@@users:location@@</td>
  <td class="userlistright">@@users:connection@@</td>
  <td class="userlistright">@@users:logon@@</td>
</tr>
<<END REPEAT users>>
</table>
<br />
