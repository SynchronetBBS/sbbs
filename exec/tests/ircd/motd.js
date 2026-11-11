/* MOTD delivery must end when the file cannot be read. */
"use strict";
load(js.exec_dir + "lib.sjs");
load(js.exec_dir + "state-fixture.sjs");

/* A null in lines is a read that fails: every later read fails too, and eof
   stays false, as it does for a real stream with its error flag set. */
function motd_state(lines) {
	var s = new_state(), next = 0, failed = false;
	s.reads = 0;
	s.closed = false;
	s.sent = [];
	s.system.text_dir = "";
	s.File = function(name) {
		this.name = name;
		this.exists = true;
		this.eof = false;
		this.date = 0;
		this.open = function() { return true; };
		this.readln = function() {
			if (++s.reads > lines.length + 5)
				throw new Error("MOTD read loop did not end");
			if (failed)
				return null;
			if (next >= lines.length) {
				this.eof = true;
				return null;
			}
			failed = lines[next] === null;
			return lines[next++];
		};
		this.close = function() { s.closed = true; };
	};
	s.user = new_user(s, "Local");
	s.user.numeric = function(num, str) { s.sent.push(num + " " + str); };
	return s;
}

function motd_lines(s) {
	return s.sent.filter(function(line) { return line.indexOf("372 ") == 0; }).slice(1);
}

run_tests({
	"MOTD sends every line, blank ones included, up to end of file": function() {
		var s = motd_state(["one", "", "three"]);
		s.user.motd();
		check(motd_lines(s).join("|") == "372 :- one|372 :- |372 :- three", "Sent: " + s.sent.join("|"));
		check(s.closed && s.sent[s.sent.length - 1].indexOf("376 ") == 0);
	},
	"MOTD ends when a read fails without reaching end of file": function() {
		var s = motd_state(["one", null]);
		s.user.motd();
		check(s.reads == 2, "Read " + s.reads + " times after the failure");
		check(motd_lines(s).join("|") == "372 :- one" && s.closed);
		check(s.sent[s.sent.length - 1].indexOf("376 ") == 0, "MOTD was not terminated");
	}
});
