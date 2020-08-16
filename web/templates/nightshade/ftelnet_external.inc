<!-- $Id: ftelnet_external.inc,v 1.5 2018/03/18 20:30:45 ree Exp $ -->

<div id="contentcontainer">
	<div id="content2">
		<h3 align='center'>External Program Section</h3>
		<p style="text-align: center;"><a href="/members/externals.ssjs">Back to External Programs</a></p>

<!-- fTelnet --> 

		<div id="fTelnetContainer" class="fTelnetContainer"></div>
		<script>document.write('<script src="//embed-v2.ftelnet.ca/js/ftelnet-loader.norip.noxfer.js?v=' + (new Date()).getTime() + '"><\/script>');</script>
		<script>
            var WSS = (location.protocol.toLowerCase() == 'https:');
			var Options = new fTelnetOptions();
			Options.BareLFtoCRLF = false;
			Options.BitsPerSecond = 57600;
			Options.ConnectionType = 'rlogin';
			Options.Emulation = 'ansi-bbs';
			Options.Enter = '\r';
			Options.Font = 'CP437';
			Options.ForceWss = false;
			Options.Hostname = '@@HostName@@';
			Options.LocalEcho = false;
            Options.Port = (WSS ? @@WSSPort@@ : @@WSPort@@);
			Options.RLoginClientUsername = '@@ClientUserName@@';
			Options.RLoginServerUsername = '@@ServerUserName@@';
			Options.RLoginTerminalType = '@@TerminalType@@';
			Options.ScreenColumns = 80;
			Options.ScreenRows = 25;
            Options.WebSocketUrlPath = '/?Port=@@RLoginPort@@';
			var fTelnet = new fTelnetClient('fTelnetContainer', Options);
			fTelnet.Connect();
		</script>

<!-- end fTelnet -->

		<p style="text-align: center;"><a href="/members/externals.ssjs">Back to External Programs</a></p>
	</div>
</div>