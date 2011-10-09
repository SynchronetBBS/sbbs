/* $Id$ */

/* A sophisticated suite of test cases.  CVS pollution at its finest. */

var test01 = log(LOG_ERROR, "one", "two", "three");
if (test01 != "three") {
	log(LOG_ERROR, "log() test failed!");
	exit();
}

var test02 = write("one, two, three");
if (test02 != "one, two, three") {
	log(LOG_ERROR, "write() test failed!");
	exit();
}

var test03 = printf("%s, %s, %s", "one", "two", "three");
if (test03 != "one, two, three") {
	log(LOG_ERROR, "printf() test failed!");
	exit();
}

var test04 = alert("one, two, three");
if (test04 != "one, two, three") {
	log(LOG_ERROR, "alert() test failed!");
	exit();
}

var test05 = js.eval('a = "one"; b = "two"; c = "three"; a + ", " + b + ", " + c;');
if (test05 != "one, two, three") {
	log(LOG_ERROR, "js.eval() test failed!");
	exit();
}

var test06 = js.gc(true);
if (test06 != undefined) {
	log(LOG_ERROR, "js.gc() test failed!");
	exit();
}

var test07 = js.on_exit("js.report_error('js.on_exit() successful');");
if (test07 != undefined) {
	log(LOG_ERROR, "js.on_exit() test failed!");
	exit();
}

/* The following currently segfaults - so many bug like Microsoft */
/*
var test08_obj = {};
//var test08 = js.get_parent(js.get_parent);
var test08 = js.get_parent(test08_obj);
log(test08.toSource());
*/

var test09 = js.version;
if (test09 != js.version) {
	log(LOG_ERROR, "js.version test failed!");
	exit();
}

var test10 = system.username(1);
if (test10 != system.username(1)) {
	log(LOG_ERROR, "system.username(1) test failed!");
	exit();
}

var test11 = system.alias("guest");
if (test11 != system.alias("guest")) {
	log(LOG_ERROR, "system.alias('guest') test failed!");
	exit();
}

var test12 = system.matchuser("guest");
if (test12 != system.matchuser("guest")) {
	log(LOG_ERROR, "system.matchuser('guest') test failed!");
	exit();
}

var test13 = load(true, "sbbsdefs.js");
if (typeof(test13) != 'object') {
	log(LOG_ERROR, "Loading of background JS failed!");
	exit();
}

var test14 = test13.write("penis");
if (!test14) {
	log(LOG_ERROR, "Writing to queue failed!");
	exit();
}

var test15 = test13.data_waiting;
if (test13.data_waiting) {
	log(LOG_ERROR, "Data waiting on queue when there shouldn't be??");
	exit();
}

var test16 = new File("test.txt");
if (typeof(test16) != 'object') {
	log(LOG_ERROR, "Couldn't create file object?!");
	exit();
}

var test17 = test16.open('w+');
if (test17 != true) {
	log(LOG_ERROR, "Couldn't open file in test16 for writing.");
	exit();
}

var test18 = test16.writeln("shitting documentation");
if (test18 != true) {
	log(LOG_ERROR, "Couldn't write to file in test 16.");
	exit();
}

var test19 = test16.rewind();
if (test19 != true) {
	log(LOG_ERROR, "Couldn't rewind file in test 16.");
	exit();
}

var test20 = test16.position;
if (test20 != 0) {
	log(LOG_ERROR, "Rewind didn't go back to position 0?!");
	exit();
}

var test21 = test16.readAll();
if (test21 != "shitting documentation") {
	log(LOG_ERROR, "Couldn't read about the shitting documentation.");
	exit();
}

var test22 = test16.eof;
if (test22 != true) {
	log(LOG_ERROR, "Not at the end of file?!");
	exit();
}

var test22 = test16.is_open;
if (test22 != true) {
	log(LOG_ERROR, "test16 file reporting as not open?!");
	exit();
}

var test23 = test16.md5_hex;
if (test23 != "52859f60180b7416f52bcae17cb4da47") {
	log(LOG_ERROR, "MD5 sum mismatch!");
	exit();
}

var test24 = test16.md5_base64;
log(test24);
if (test24 != "UoWfYBgLdBb1K8rhfLTaR0==") {
	log(LOG_ERROR, "MD5 sum (base64) mismatch!");
//	exit();
}

var test25 = test16.close();
if (test25 != undefined) {
	log(LOG_ERROR, "Couldn't close test 16 file!");
	exit();
}

var test26 = test16.remove("test.txt");
if (test26 != true) {
	log(LOG_ERROR, "Couldn't remove test.txt");
	exit();
}

var test27 = new Socket();
if (typeof(test27) != 'object') {
	log(LOG_ERROR, "New socket isn't an object.");
	exit();
}

var test28 = (test27.non_blocking = true);
if (test28 != true) {
	log(LOG_ERROR, "Couldn't set socket to non blocking.");
	exit();
}

var test29 = test27.bind(1339);
if (test29 != true) {
	log(LOG_ERROR, "Couldn't bind to port 1337.");
	exit();
}

var test30_sock = new Socket();
var test30 = test30_sock.connect("localhost",1337);

log('-----');
log(test30);
log('-----');
exit();

var s = new Socket();
s.bind(10089);
s.close();
s = new Socket();
s.non_blocking = true;
s.connect("localhost",23);
s.send("derp");
log(s.recvline());
log(s.error);
log(s.descriptor);
s.close();

exit();

function tf(obj) {
	global_counter++;
	printf("tf iteration #%d\r\n", global_counter);
	for(var i in obj) {
		log(obj[i].toSource());
//		tf(obj[i]);
	}
	return;
}

/* [+]  global object */
//write_raw("e");
/* [+]  js object */
log(system.datestr());
log(system.name);
log(system.operator);
log(system.nodes);
log(system.data_dir);
log(system.timer);
log(system.uptime);
log(system.stats.total_messages);
log(system.node_list[0].toSource());
/* [+]  server object */
//log(server.version);
//log(server.options);
//log(server.interface_ip_address);
/* [+]  client object */
//log(client.ip_address);
//log(client.user_name);
//log(client.socket.descriptor);
/* [+]  user object */
log(user.adjust_credits(1));
log(user.compare_ars("G"));
log(user.number);
log(user.alias);
log(user.stats.total_timeon);
log(user.limits.time_per_logon);
log(user.security.level);
/* [+]  bbs object */
//log(bbs.text(1));
//log(bbs.nodesync());
//log(bbs.auto_msg());
//log(bbs.ver());
//log(bbs.whos_online());
//log(bbs.online);
//log(bbs.sys_status);
/* [+]  console object */
/*
console.write("g");
console.writeln("h");
console.putmsg("\1r\1hi");
log(console.ungetstr("j"));
log(console.inkey());
log(console.crlf());
log(console.cleartoeol());
log(console.center("k"));
log(console.strlen("lmnop"));
log(console.getxy().toSource());
log(console.status);
log(console.screen_rows);
log(console.screen_columns);
log(console.ctrlkey_passthru);
*/
/* [+]  msg_area object */
log("Msg Area:");
tf(msg_area);
/* [+]  file_area object */
log("File Area:");
tf(file_area);
/* [+]  xtrn_area object */
log("Xtrn Area:");
tf(xtrn_area);
/* [+]  MsgBase class */
var mb = new MsgBase("DOVE-GEN");
tf(mb);
log(mb.open());
log(mb.error);
log(mb.subnum);
/* [+]  File class */
/* [+]  Socket class */
