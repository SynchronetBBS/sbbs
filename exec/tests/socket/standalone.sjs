/* Return a runner so load() does not export helpers into the outer suite. */
(function(worker) {
	var windows = system.platform == "Win32";
	var program = typeof jsdoor_revision == "undefined" ? "jsexec" : "jsdoor";
	var revision = program == "jsdoor" ? jsdoor_revision : jsexec_revision;
	var filename = program + (windows ? ".exe" : "");
	var dir = worker.substr(0, worker.length - file_getname(worker).length);
	var root = dir + "../../../";
	var candidates = [system.exec_dir + filename, dir + "../../" + filename];
	var builds = directory(root + "src/sbbs3/*.exe*/" + filename);
	builds = builds.filter(function(path) {
		return path.toLowerCase().indexOf("." + system.platform.toLowerCase() + ".") >= 0;
	});
	/* CI extracts its runtime into the platform's release build directory. */
	candidates = candidates.concat(builds.filter(function(path) {
		return /[.]exe[.]release[/\\]/i.test(path);
	}), builds);
	function quote(path) {
		return windows ? '"' + path + '"' : "'" + path.replace(/'/g, "'\\''") + "'";
	}
	var runtime;
	for (var i = 0; i < candidates.length; i++) {
		if (!file_exists(candidates[i])) continue;
		/* An installed binary can predate the build running this suite. */
		var version = system.popen(quote(candidates[i]) + " -S -v 2>" + (windows ? "NUL" : "/dev/null"));
		if (version.join("\n").indexOf(revision) >= 0) {
			runtime = candidates[i];
			break;
		}
	}
	if (!runtime)
		throw new Error("Cannot locate " + filename + " " + revision + " for standalone socket test");
	var command = quote(runtime) + " -A -S -f ";
	if (program == "jsexec")
		command += "-C " + quote("-c" + system.ctrl_dir) + " ";
	var status = system.exec(command + quote(worker));
	if (status != 0)
		throw new Error(file_getname(worker) + " failed with exit status " + status);
});
