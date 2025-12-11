var start = system.timer;
var slept = mswait(1000);
var end = system.timer;
if (Math.abs(end - (start + slept / 1000)) > 0.0025)
	throw new Error("mswait() = "+slept+" diff of "+Math.abs(end - (start + (slept / 1000))));
