<div align="center">
<table>
  <tr>
	<td>
      System:
	</td>
	<td>
      %%JS:system.name%%
	</td>
  </tr>
  <tr>
	<td>
      QWK ID:
	</td>
	<td>
      %%JS:system.qwk_id%%
	</td>
  </tr>
<<REPEAT fidoaddrs>>
  <tr>
	<td>
      FidoNet Address:
	</td>
	<td>
      %%fidoaddrs:address%%
	</td>
  </tr>
<<END REPEAT fidoaddrs>>
  <tr>
	<td>
      Location:
	</td>
	<td>
      %%JS:system.location%%
	</td>
  </tr>
  <tr>
	<td>
      Operator:
	</td>
	<td>
      %%JS:system.operator%%
	</td>
  </tr>
  <tr>
	<td>
      Nodes:
	</td>
	<td>
	  %%JS:system.nodes%%
	</td>
  </tr>
  <tr>
	<td>
      Node Phone:
	</td>
	<td>
	  Telnet and RLogin Only
	</td>
  </tr>
  <tr>
	<td>
      Total Logons:
	</td>
	<td>
	  %%JS:system.stats.total_logons%%
	</td>
  </tr>
  <tr>
	<td>
      Logons Today:
	</td>
	<td>
	  %%JS:system.stats.logons_today%%
	</td>
  </tr>
  <tr>
	<td>
      Total Time:
	</td>
	<td>
	  %%JS:system.stats.total_timeon%%
	</td>
  </tr>
  <tr>
	<td>
      Time Today:
	</td>
	<td>
	  %%JS:system.stats.timeon_today%%
	</td>
  </tr>
</table>
<p>Synchronet BBS for %%JS:system.platform%%
Version %%JS:system.full_version%%</p>
<p>Revision %%JS:system.revision%%
%%JS:system.beta_version%%
%%JS:system.compiled_when%%
%%JS:system.msgbase_lib%%
%%JS:system.compiled_with%%</p>
<p>%%JS:system.copyright%% - <a href="http://www.synchro.net/">http://www.synchro.net</a></p>
<p>%%JS:system.js_version%%</p>
<p>%%JS:system.socket_lib%%</P>
<p>%%JS:system.os_version%%</P>
<table>
<tr><td>
<pre class="leftalign">@@sysinfo@@</pre>
</td></tr>
<tr><td>
&nbsp;
</td></tr>
<tr><td>
<pre class="leftalign">@@logon@@</pre>
</td></tr>
</table>
</div>
