<!-- $Id$ -->

<td class="main" valign="top"><br />
    <br />
	<h3 align='center'>External Program Section</h3>
	<p style="text-align: center;"><a href="/members/externals.ssjs">Back to External Programs</a></p>

<!-- fTelnet --> 

	<p style="text-align: center;"><span id="ClientContainer"></span></p>
	
	<script type="text/javascript" src="/ftelnet/ClientVars.js"></script>
	<script type="text/javascript" src="/ftelnet/ClientFuncs.js"></script>
	<script type="text/javascript" src="/ftelnet/swfobject.js"></script>
	<script type="text/javascript">
		ClientVars.RLogin = 1;
		ClientVars.RLoginClientUserName = "@@ClientUserName@@";
		ClientVars.RLoginHostName = "@@HostName@@";
		ClientVars.RLoginPort = @@Port@@;
		ClientVars.RLoginServerUserName = "@@ServerUserName@@";
		ClientVars.RLoginTerminalType = "@@TerminalType@@";
		ClientVars.ServerName = "@@ServerName@@";
		ClientVars.SocketPolicyPort = @@SocketPolicyPort@@;
		swfobject.embedSWF(
			"/ftelnet/fTelnet.swf",
			"ClientContainer",
			"100%",
			"100%",
			"10.0.0",
			"/ftelnet/playerProductInstall.swf",
			ClientVars,
			{ allowfullscreen: "true", allowscriptaccess: "sameDomain", bgcolor: "#ffffff", quality: "high" },
			{ align: "middle", id: "fTelnet", name: "fTelnet", swliveconnect: "true" },
			function (callbackObj) {
				if (!callbackObj.success) {
					// Failed to use fTelnet, and this method has no fall-back
					alert("Sorry, I wasn't able to load fTelnet\n\nTry again with Flash 10+ installed");
				}
			}
		);
	</script>

<!-- end fTelnet -->

	<p style="text-align: center;"><a href="/members/externals.ssjs">Back to External Programs</a></p>