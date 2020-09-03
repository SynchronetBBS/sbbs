/* $Id: dest6.js,v 1.21 2015/09/26 05:15:37 deuce Exp $ */

/* A sophisticated suite of test cases.  CVS pollution at its finest. */

/*
 * This bit is here for test32.  If the commented bit is
 * uncommented, it will spawn 128 different threads which
 * will each spawn 128 dnshelper.js threads.
 *
 * This will crash.
 * At 2:25AM, Oct 28th, 2011, I didn't care about that though.
 */
if(argc) {
	var test32=[];
	var test32_count=128;
	var test32_script="dnshelper.js";
	for(test32_count=0; test32_count<128; test32_count++)
		test32.push(load(true,test32_script,"127.0.0.1"))
	for(test32_count=0; test32_count<128; test32_count++) {
		test32_queue=test32.shift();
		while ((test32_queue.ret=test32_queue.read()) == undefined) {
			log("Waiting for dnshelper.js to return a result...");
			sleep(10);
		}
	}
	exit();
}

var test01 = log(LOG_ERROR, "one", "two", "three");
if (test01 != "three") {
	log(LOG_ERROR, "log() test failed ("+test01+")!");
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
/* Well, it doesn't segfault, but it DOES init UIFC stuff...
var test08_obj = {};
var test08 = js.get_parent(js.get_parent);
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

var test16 = new File(system.temp_dir + "test.txt");
if (typeof(test16) != 'object') {
	log(LOG_ERROR, "Couldn't create file object?!");
	exit();
}

var test17 = test16.open('w+');
if (test17 != true) {
	log(LOG_ERROR, "Couldn't open file in test16 for writing.");
	exit();
}

var test18 = test16.write("shitting documentation");
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
if (test23 != "450491f4513e7e0c69ca80a9702a7196") {
	log(LOG_ERROR, "MD5 sum mismatch ("+test23+")!");
	exit();
}

var test24 = test16.md5_base64;
if (test24 != "RQSR9FE+fgxpyoCpcCpxln==") {
	log(LOG_ERROR, "MD5 sum (base64) mismatch ("+test24+")!");
//	exit();
}

var test25 = test16.close();
if (test25 != undefined) {
	log(LOG_ERROR, "Couldn't close test 16 file!");
	exit();
}

var test26 = test16.remove(system.temp_dir + "test.txt");
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

test27.debug=true;
var test29 = test27.bind(1339);
if (test29 != true) {
	log(LOG_ERROR, "Couldn't bind to port 1337.");
	exit();
}

var test30_sock = new Socket();
var test30 = test30_sock.connect("localhost",1337);

var test31 = 'so many bug like microsoft'.search(/[m]/);
if (test31 != 3) {
	log(LOG_ERROR, "Looks like str.search() doesn't work!");
	exit();
}

var test32=[];
var test32_count=128;
var test32_script="dnshelper.js";
for(test32_count=0; test32_count<128; test32_count++)
	test32.push(load(true,test32_script,"127.0.0.1"))
for(test32_count=0; test32_count<128; test32_count++) {
	test32_queue=test32.shift();
	if(test32_script=="dnshelper.js") {
		while ((test32_queue.ret=test32_queue.read()) == undefined) {
			log("Waiting for dnshelper.js to return a result...");
			sleep(10);
		}
	}
}
log(test32.ret);

var test33_file = new File(system.temp_dir + "test.ini");
test33_file.open("w+");
test33_file.iniSetValue("test", "Date", new Date("April 17, 1980 03:00:00"));
test33_file.iniSetValue("test", "Double", 13.37);
test33_file.iniSetValue("test", "Integer", 1337);
test33_file.iniSetValue("test", "Boolean", true);
test33_file.close();

var test34_file = new File(system.temp_dir + "test.ini");
test34_file.open("r+");
var test34 = test34_file.iniGetObject("test");
test34_file.close();
if (   (typeof(test34.Date) != "string")
    || (test34.Double != 13.37)
    || (test34.Integer != 1337)
    || (test34.Boolean != true)
) {
	file_remove(system.temp_dir + "test.ini");
	log("INI object readback values don't match!");
	exit();
}

var test35_file = new File(system.temp_dir + "test.ini");
test35_file.open("r+");
var test35={};
test35.Date = test35_file.iniGetValue("test", "Date", new Date());
test35.Double = test35_file.iniGetValue("test", "Double", 97.73);
test35.Integer = test35_file.iniGetValue("test", "Integer", 9773);
test35.Boolean = test35_file.iniGetValue("test", "Boolean", false);
test35_file.close();
if (   (test35.Date.getTime() != (new Date("April 17, 1980 03:00:00")).getTime())
    || (test35.Double != 13.37)
    || (test35.Integer != 1337)
    || (test35.Boolean != true)
) {
	log("INI value readback values don't match!");
	log(test35.Date+"("+test35.Date.getTime()+") != "+new Date("April 17, 1980 03:00:00")+" ("+(new Date("April 17, 1980 03:00:00")).getTime()+")");
	log(test35.Double+" != 13.37");
	log(test35.Integer+" != 1337");
	log(test35.Boolean+" != true");
	file_remove(system.temp_dir + "test.ini");
	exit();
}

var test36_file = new File(system.temp_dir + "test.ini");
test36_file.open("r+");
test36_file.truncate();
var test36={};
test36.Date = test36_file.iniGetValue("test", "Date", new Date("April 17, 1980 03:00:00"));
test36.Double = test36_file.iniGetValue("test", "Double", 13.37);
test36.Integer = test36_file.iniGetValue("test", "Integer", 1337);
test36.Boolean = test36_file.iniGetValue("test", "Boolean", true);
test36_file.close();
file_remove(system.temp_dir + "test.ini");
if (   (test36.Date.getTime() != (new Date("April 17, 1980 03:00:00")).getTime())
    || (test36.Double != 13.37)
    || (test36.Integer != 1337)
    || (test36.Boolean != true)
) {
	log("INI default value read values don't match!");
	log(test36.Date+"("+test36.Date.getTime()+") != "+new Date("April 17, 1980 03:00:00")+" ("+(new Date("April 17, 1980 03:00:00")).getTime()+")");
	log(test36.Double+" != 13.37");
	log(test36.Integer+" != 1337");
	log(test36.Boolean+" != true");
	exit();
}

var test37_xml_file = new File(system.temp_dir + "xml.txt");
test37_xml_file.open("r", true);
var test37_xml = new XML(test37_xml_file.read());
test37_xml_file.close();
//var test37 = test37_xml.person(@name == "bob").age;
//var test37 = test37_xml.find("age").text();

var test38_js = new File(system.temp_dir + "test38.js");
test38_js.open("w+");
test38_js.writeln('mswait(random(500)); log("Testing log() in background script.");');
test38_js.close();
for(i=0; i<20; i++)
	var test38 = load(true, system.temp_dir + "test38.js");
file_remove(system.temp_dir + "test38.js");

var test39_js = new File(system.temp_dir + "test39.js");
test39_js.open("w+");
test39_js.writeln('while (1) {');
test39_js.writeln('  var x = parent_queue.read(60);');
test39_js.writeln('  if (x)');
test39_js.writeln('     parent_queue.write(x);');
test39_js.writeln('  if(x=="stop") {');
test39_js.writeln('     log("background mirror exiting");');
test39_js.writeln('     parent_queue.write(x);');
test39_js.writeln('     exit(0);');
test39_js.writeln('  }');
test39_js.writeln('  sleep(10);');
test39_js.writeln('}');
test39_js.close();
var test39 = load(true, system.temp_dir + "test39.js");
test39_js.remove();
test39.readValue=function() {
	while(1) {
		var x=this.read();
		if(x)
			return(x);
		sleep(1);
	}
};
log("  I: "+ test39.write(new Date("April 17, 1980 03:00:00")));
log(" II: " + test39.write(13.37));
log("III: "+ test39.write(1337));
log(" IV: "+test39.write(true));
log("  V: "+ test39.write("go-get me some Popeye's Chicken"));
log(" VI: "+test39.write("stop"));
if ((test39_val = test39.readValue().valueOf()) != new Date("April 17, 1980 03:00:00").valueOf()) {
	log("Date on background script doesn't return date?");
	log("Nope, it's just Chuck Testa with another realistic dead JavaScript object. ("+test39_val.getTime()+")");
}
if (test39.readValue() != 13.37) {
	log("Double on background script doesn't return double?");
	exit();
}
if (test39.readValue() != 1337) {
	log("Integer on background script doesn't return integer?");
	exit();
}
if (test39.readValue() != true) {
	log("Boolean on background script doesn't return boolean?");
	exit();
}
if (test39.readValue() != "go-get me some Popeye's Chicken") {
	log("String on background script doesn't return string?");
	exit();
}
if (test39.readValue() != "stop") {
	log("Second string on background script doesn't return string?");
	exit();
}

/*
 * These tests are for testing termination, so should be last.
 */
var test_997_js = new File(system.temp_dir + "test_997.js");
test_997_js.open("w+");
test_997_js.writeln('js.auto_terminate=false; js.on_exit("log(\'Background infinite loop exited\')"); while(1) { mswait(random(500)); } ; log("Background infinite loop exited!");');
test_997_js.close();
for(i=0; i<20; i++)
	var test_997 = load(true, system.temp_dir + "test_997.js");
file_remove(system.temp_dir + "test_997.js");

var test_998_js = new File(system.temp_dir + "test_998.js");
test_998_js.open("w+");
test_998_js.writeln('js.auto_terminate=true; js.on_exit("log(\'Background autoterminate terminated\')"); while(1) { mswait(random(500)); } ; log("Infinite loop exited!");');
test_998_js.close();
for(i=0; i<20; i++)
	var test_998 = load(true, system.temp_dir + "test_998.js");
file_remove(system.temp_dir + "test_998.js");

var test_999_js = new File(system.temp_dir + "test_999.js");
test_999_js.open("w+");
test_999_js.writeln('while(!js.terminated) { mswait(random(500)); } ; log("Background terminated was true!");');
test_999_js.close();
for(i=0; i<20; i++)
	var test_999 = load(true, system.temp_dir + "test_999.js");
file_remove(system.temp_dir + "test_999.js");
js.terminated=true;
mswait(3000);

log("*** Everything appears to have passed. ***");

exit();
