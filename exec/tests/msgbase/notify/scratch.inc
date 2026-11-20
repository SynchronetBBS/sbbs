// Used by rcpt_list.js (not a test itself: the runner only runs *.js
// files): a scratch copy of the configuration below system.temp_dir, with
// a new, empty data directory, and a child jsexec run against it.
//
// var sc = load({}, js.exec_dir + "scratch.inc").scratch_ctrl("name");
// sc.setup();                     // ctrl copy, empty data/node/temp dirs
// var r = sc.run(script, [args]); // child's JSON result (argv[0] is the output path)
// sc.remove();

// Path of the jsexec binary running this test (Linux: /proc)
function running_jsexec()
{
	var f = new File("/proc/self/stat");
	if (!f.open("r"))
		throw new Error("Error opening " + f.name);
	var pid = parseInt(f.readln(), 10);
	f.close();
	var p = new File("readlink /proc/" + pid + "/exe");
	if (!p.popen("r"))
		throw new Error("Error running readlink");
	var path = p.readln();
	p.close();
	if (!path)
		throw new Error("readlink returned nothing");
	path = path.replace(/ \(deleted\)$/, "");
	if (!file_exists(path))
		throw new Error("No such file: " + path);
	return path;
}

function is_absolute(path)
{
	return /^([\/\\]|[A-Za-z]:)/.test(path);
}

function remove_tree(dir)
{
	directory(dir + "*").forEach(function (path) {
		var c = path.charAt(path.length - 1);
		if (c == '/' || c == '\\')
			remove_tree(path);
		else
			file_remove(path);
	});
	rmdir(dir);
}

function ini_edit(path, fn)
{
	var f = new File(path);
	if (!f.open("r+"))
		throw new Error("Error " + f.error + " opening " + f.name);
	fn(f);
	f.close();
}

function scratch_ctrl(name)
{
	var base = system.temp_dir + format("msgbase_%s_%u_%u/", name, time(), random(1000000));
	var sc = {
		base: base,
		ctrl: base + "ctrl/",
		data: base + "data/",
		node: base + "node1/",
		temp: base + "temp/"
	};

	sc.setup = function () {
		[sc.ctrl, sc.data + "user/", sc.node, sc.temp].forEach(function (dir) {
			if (!mkpath(dir))
				throw new Error("Error creating " + dir);
		});
		directory(system.ctrl_dir + "*").forEach(function (path) {
			if (file_isdir(path))
				return;
			if (!file_copy(path, sc.ctrl + file_getname(path)))
				throw new Error("Error copying " + path);
		});
		var node_ini;
		ini_edit(sc.ctrl + "main.ini", function (f) {
			var node1 = f.iniGetValue("node_dir", "1", "../node1/");
			node_ini = (is_absolute(node1) ? node1 : system.ctrl_dir + node1);
			node_ini = backslash(node_ini) + "node.ini";
			f.iniRemoveSection("node_dir");
			// relative to the ctrl directory: a node directory path is cut
			// at 100 characters, which a deep temp directory exceeds
			f.iniSetValue("node_dir", "1", "../node1/");
			f.iniSetValue("dir", "data", sc.data);
			f.iniSetValue("dir", "logs", sc.data);
			f.iniSetValue("dir", "exec", system.exec_dir);
			f.iniSetValue("dir", "mods", system.mods_dir);
		});
		// A tree without node directories (the CI test tree has only ctrl
		// and exec): an empty node.ini, filled in below
		if (file_exists(node_ini)) {
			if (!file_copy(node_ini, sc.node + "node.ini"))
				throw new Error("Error copying " + node_ini);
		} else {
			var nf = new File(sc.node + "node.ini");
			if (!nf.open("w"))
				throw new Error("Error " + nf.error + " creating " + nf.name);
			nf.close();
		}
		ini_edit(sc.node + "node.ini", function (f) {
			f.iniSetValue(null, "text_dir", system.text_dir);
			f.iniSetValue(null, "temp_dir", sc.temp);
		});
	};

	// SBBSCTRL as this jsexec started with (env is a read-only copy of the
	// environment taken at startup); there is no way to unset a variable,
	// so if it was not set it is put back as this jsexec's ctrl directory
	var saved_sbbsctrl = (env.SBBSCTRL !== undefined) ? env.SBBSCTRL : system.ctrl_dir;
	var runs = 0;
	sc.run = function (script, args) {
		var output = sc.base + "result" + (++runs) + ".json";
		var cmd = format('"%s" -c "%s" -n "%s" "%s"', running_jsexec(), sc.ctrl, script, output);
		(args || []).forEach(function (arg) {
			cmd += ' "' + arg + '"';
		});
		putenv("SBBSCTRL=" + sc.ctrl);
		var rc;
		try {
			rc = system.exec(cmd);
		} finally {
			putenv("SBBSCTRL=" + saved_sbbsctrl);
		}
		var f = new File(output);
		if (!f.open("r"))
			throw new Error("No result from the child jsexec (exit code " + rc + "): " + cmd);
		var r = JSON.parse(f.read());
		f.close();
		r.exit_code = rc;
		return r;
	};

	sc.remove = function () {
		if (file_isdir(sc.base))
			remove_tree(sc.base);
		return !file_isdir(sc.base);
	};
	return sc;
}

this;
