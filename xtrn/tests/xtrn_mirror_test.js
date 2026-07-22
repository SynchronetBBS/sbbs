// Unit test for exec/load/xtrn_mirror.js -- pure, no network.
//
//     jsexec /home/rswindell/sbbs/xtrn/tests/xtrn_mirror_test.js
//
// Deliberately NOT under exec/tests/: that tree is walked and run wholesale by
// exec/tests/test.js, and this module can reach the network. The download path
// is exercised end-to-end against the live mirror instead.

load("xtrn_mirror.js");

var fail = 0;
function ok(cond, msg) { if (!cond) { fail++; print("FAIL: " + msg); } else print("ok: " + msg); }

ok(XTRN_MIRROR_BASE === "http://web.synchro.net/files/main/games/",
   "mirror base URL");

ok(xtrn_mirror_name("https://downloads.scummvm.org/frs/extras/Beneath%20a%20Steel%20Sky/bass-cd-1.2.zip")
   === "bass-cd-1.2.zip", "basename of an encoded path");
ok(xtrn_mirror_name("http://archive.org/download/cc_gold/c%26c_gold.zip")
   === "c&c_gold.zip", "percent-decoding of %26");
ok(xtrn_mirror_name("https://example.com/a/b/file.zip?x=1&y=2")
   === "file.zip", "query string stripped");
ok(xtrn_mirror_name("https://example.com/a/file.zip#frag")
   === "file.zip", "fragment stripped");
ok(xtrn_mirror_name("plain.zip") === "plain.zip", "bare filename");

ok(typeof xtrn_mirror_download === "function", "xtrn_mirror_download is defined");

print(fail ? ("\n" + fail + " FAILURES") : "\nALL PASS");
exit(fail ? 1 : 0);
