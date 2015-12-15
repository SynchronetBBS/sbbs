var f = new File(file_cfgname(system.ctrl_dir, "services.ini"));
if (!f.open('r')) exit();
var webSocket = f.iniGetObject('WebSocket');
var webSocketRLogin = f.iniGetObject('WebSocketRLogin');
f.close();

function getSplash() {
	var f = new File(settings.ftelnet_splash);
	f.open('rb');
	var splash = base64_encode(f.read());
	f.close();
	return splash;
}