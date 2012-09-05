load("sbbsdefs.js");
load(js.exec_dir + "client.js");
if(!http)
	exit;

console.clear();
console.putmsg("\1H\1W\1ILoading a random ANSI from bbs-scene.org...\1N");
var ansi = http.Get("http://bbs-scene.org/api/grab.php?ansirandom");
console.clear();
console.putmsg(ansi, P_NOPAUSE);