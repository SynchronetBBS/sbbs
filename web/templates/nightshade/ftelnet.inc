<!-- $Id$ -->

<div id="contentcontainer">
	<div id="content2">
		<h3 align='center'>fTelnet (Flash Telnet)</h3>

<!-- fTelnet --> 

	<p style="text-align: center;"><span id="ClientContainer"></span></p>
	<p style="text-align: center;">Keyboard Shortcuts: CTRL+UP ARROW = Scrollback, CTRL+PAGE UP = Upload, CTRL+PAGE DOWN = Download</p>
	
	<script type="text/javascript" src="/ftelnet/ClientVars.js"></script>
	<script type="text/javascript" src="/ftelnet/ClientFuncs.js"></script>
	<script type="text/javascript" src="/ftelnet/swfobject.js"></script>
	<script type="text/javascript">
		ClientVars.TelnetHostName = "@@HostName@@";
		ClientVars.TelnetPort = @@Port@@;
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

	</div>
</div>