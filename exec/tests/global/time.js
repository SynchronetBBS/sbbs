var d = Date()/1000;
var t = time();
if (Math.abs(t - d) > 1)
	throw new Error("new Date() and time() differ by over a second");
