<!-- $Id: ftelnet.inc,v 1.4 2018/03/18 20:30:45 ree Exp $ -->

<div id="contentcontainer">
	<div id="content2">

<!-- fTelnet --> 

	<div id="fTelnetContainer" class="fTelnetContainer"></div>
	<script>document.write('<script src="//embed-v2.ftelnet.ca/js/ftelnet-loader.norip.xfer.js?v=' + (new Date()).getTime() + '"><\/script>');</script>
	<script>
		var WSS = (location.protocol.toLowerCase() == 'https:');
		var Options = new fTelnetOptions();
		Options.BareLFtoCRLF = false;
		Options.BitsPerSecond = 57600;
		Options.ConnectionType = 'telnet';
		Options.Emulation = 'ansi-bbs';
		Options.Enter = '\r';
		Options.Font = 'CP437';
		Options.ForceWss = false;
		Options.Hostname = '@@HostName@@';
		Options.LocalEcho = false;
		Options.Port = (WSS ? @@WSSPort@@ : @@WSPort@@);
		Options.ScreenColumns = 80;
		Options.ScreenRows = 25;
		Options.WebSocketUrlPath = '/?Port=@@TelnetPort@@';
		var fTelnet = new fTelnetClient('fTelnetContainer', Options);
	</script>

<!-- end fTelnet -->

	</div>
</div>