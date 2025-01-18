/*
 * Ignore all the chicken/egg issues here...
 *
 * backslash()
 * directory()
 * format()
 * js.exec()
 * writeln()
 * write()
 */

var testroot = js.exec_dir;
var testdirs = {};

function depth_first(root, parent)
{
	var entries;

	if (file_exists(root+'skipif')) {
		try {
			if (load(root+'skipif')) {
				stdout.writeln("Skipping "+root);
				return;
			}
		}
		catch(e) {}
	}
	parent[root] = {tests:[]};
	entries = directory(root+'*');

	entries.forEach(function(entry) {
		var last_ch;

		if (entry === './' || entry === '../' || entry === 'skipif')
			return;
		if (entry.length < 1)
			return;

		last_ch = entry[entry.length - 1];
		if (last_ch !== '/' && last_ch !== '\\') {
			if (entry.substr(-3) === '.js')
				this.tests.push(entry);
			return;
		}

		depth_first(entry, this);
	}, parent[root]);
}

depth_first(testroot, testdirs);
// Do not run tests in top-level
testdirs[testroot].tests = [];

var passed = 0;
var failed = 0;

if (this.stdout === undefined)
	stdout = console;

function run_tests(location, obj)
{
	if (testroot.length > location.substr)
		stdout.writeln("Running tests in "+location.substr(testroot.length));
	else
		stdout.writeln("Running tests in "+location);

	if (obj.tests !== undefined) {
		obj.tests.forEach(function(testscript) {
			var tfailed = false;
			var fail_msg = '';

			try {
				var dir = testscript;
				stdout.write(format("%-70.70s ", testscript.substr(location.length)+'......................................................................'));
				var result = js.exec(testscript, location, new function(){});
				if (result instanceof Error) {
					tfailed = true;
					fail_msg = result.toString();
					log(file_getname(result.fileName) + " line " + result.lineNumber + " " + result);
				}
			}
			catch(e) {
				tfailed = true;
				fail_msg = e.toString();
				log("Caught: "+fail_msg);
			}
			if (tfailed) {
				stdout.writeln("FAILED!");
				stdout.writeln(fail_msg);
				stdout.writeln("");
				failed++;
			}
			else {
				stdout.writeln("passed");
				passed++;
			}
		});
	}
	stdout.writeln('');
	Object.keys(obj).forEach(function(subname) {
		if (subname === 'tests')
			return;
		run_tests(subname, obj[subname]);
	});
	stdout.writeln('');
}

run_tests(testroot, testdirs[testroot]);
if (failed > 0)
	exit(1);
exit(0);
