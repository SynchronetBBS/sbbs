js.global.require_module_file = argv[1];
if (eval('typeof('+argv[2]+') !== "undefined" ? true : false'))
	js.global.require_module_file = 'dummy.js';
load(js.global.require_module_file);
if (eval('typeof('+argv[2]+') == "undefined"'))
	throw("ERROR: load()ing "+argv[1]+" didn't define symbol \""+argv[2]+"\"");
if (argv[0] === 'undefined') {
	Object.defineProperties(this, {
		"argv": {
			value: undefined,
			writable: false
		},
		"argc": {
			value: undefined,
			writeable: false
		}
	});
}
else {
	Object.defineProperties(this, {
		"argv": {
			value: argv[0],
			writable: false
		},
		"argc": {
			value: argv[0].length,
			writeable: false
		}
	});
}
