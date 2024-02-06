if (ctrl('C') !== String.fromCharCode(3))
	throw new Error("ctrl('C') !== String.fromCharCode(3)");
if (ctrl(3) !== String.fromCharCode(3))
	throw new Error("ctrl(3) !== String.fromCharCode(3)");
