// $Id$

var capture=load(new Object, "termcapture_lib.js");

if(argc < 2) {
    alert("usage: " + js.exec_file + " <protocol> <address> [port] [rows]");
    exit();
}
capture.protocol = argv[0];
capture.address = argv[1];
capture.port = argv[2];
capture.rows = argv[3];
capture.timeout = 15;

var lines=capture.capture();
if(lines==false) {
    alert("Capture failure");
    exit();
}
print("test");
print("hello: " + capture.hello.join("\n"));

var file = new File("capture.ans");
if(!file.open("wb")) {
    alert("Error opening " + file.name);
    exit();
}
file.write(lines.join("\n"));
file.close();
print(file.name + " written");

load("graphic.js");
var graphic=new Graphic(80, capture.rows);
graphic.parseANSI(lines);
graphic.write("capture.bin");
