// File.popen(): reading a command's output, writing a command's input, and
// the wait-for-exit that close() performs.
//
// The command line is run by the platform's shell, so 'echo' and 'sort' are
// spelled the same on Windows and *nix.

mkpath(system.temp_dir);

function check(what, got, want)
{
	if (got !== want)
		throw new Error(what + " = '" + got + "', expected '" + want + "'");
}

// mode "r": read what the command writes.
var f = new File("echo popen_test");
if (!f.popen("r"))
	throw new Error("popen('r') failed: " + f.error);
try {
	check("popen('r').readln()", f.readln(), "popen_test");
} finally {
	f.close();
}

// The default mode is bidirectional.  Only its reading half is exercised
// here: nothing can half-close a single stream to give the command an EOF.
f = new File("echo popen_test");
if (!f.popen())
	throw new Error("popen() failed: " + f.error);
try {
	check("popen().readln()", f.readln(), "popen_test");
} finally {
	f.close();
}

// mode "w": write what the command reads.  close() waits for the command to
// exit, so its output file is complete by the time close() returns.
var path = system.temp_dir + "test_file_popen_" + Date.now() + ".txt";
f = new File('sort > "' + path + '"');
if (!f.popen("w"))
	throw new Error("popen('w') failed: " + f.error);
f.writeln("charlie");
f.writeln("alpha");
f.writeln("bravo");
f.close();
try {
	var out = new File(path);
	if (!out.open("r"))
		throw new Error("could not open " + path + ": " + out.error);
	var lines = out.readAll();
	out.close();
	check("sorted output", lines.join(","), "alpha,bravo,charlie");
} finally {
	file_remove(path);
}

// An unusable mode fails rather than opening a stream that cannot work.
f = new File("echo popen_test");
if (f.popen("x"))
	throw new Error("popen('x') succeeded on an invalid mode");
