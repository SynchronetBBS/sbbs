var x = '/opt/sbbs/fido/outbound.00a/00010001.flo';
if (x.replace(/\.[^\.]*?$/, '.bsy') === x) {
	throw new Error("Ungreedy match failed!");
}
