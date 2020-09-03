<!-- $Id: userlist.inc,v 1.1 2006/02/22 02:16:17 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer">
	<div id="content2">
	<div id="usrlist">
	<div align="center">
<table class="userlist" cellpadding="2" cellspacing="1">
<tr>
    <th class="userlist2"><a class="userlist2" href="userlist.ssjs?sort=alias">Alias</a></th>
    <th class="userlist2"><a class="userlist2" href="userlist.ssjs?sort=location">Location</a></th>
    <th class="userlist2"><a class="userlist2" href="userlist.ssjs?sort=connection">Connection</a></th>
    <th class="userlist2"><a class="userlist2" href="userlist.ssjs?sort=laston">Last On</a></th>
</tr>
<<REPEAT users>>
<tr>
  <td class="userlist2">@@users:alias@@</td>
  <td class="userlist2">@@users:location@@</td>
  <td class="userlist2right">@@users:connection@@</td>
  <td class="userlist2right">@@users:logon@@</td>
</tr>
<<END REPEAT users>>
</table>
		</div>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>
