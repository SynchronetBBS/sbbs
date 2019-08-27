load("dorkit.js");

function getkeyw()
{
	var timeout = time() + 60 * 5;
	var tl;
	var now;

	do {
	} while(!dk.console.waitkey(1000));
	return dk.console.getkey();
}

dk.console.print('Press a key');
getkeyw();
exit(0);
