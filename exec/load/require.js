js.global.require_module_file = argv[0];
if (eval('typeof('+argv[1]+') !== "undefined" ? true : false'))
	js.global.require_module_file = 'dummy.js';
load(js.global.require_module_file);
