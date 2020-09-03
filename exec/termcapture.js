// $Id: termcapture.js,v 1.3 2015/10/28 07:48:57 rswindell Exp $

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

var result=capture.capture();
if(result==false) {
    alert("Capture failure, error: " + capture.error);
    exit();
}
alert("Capture success, stopped due to: " + result.stopcause);
print("hello: ");
for(i in result.hello)
    print("'" + result.hello[i] + "'");

var file = new File("capture.raw");
if(!file.open("wb")) {
    alert("Error opening " + file.name);
    exit();
}
file.write(result.preview.join("\r\n"));
file.close();
print(file.name + " written");

load("graphic.js");
var graphic=new Graphic(80, capture.rows);
graphic.ANSI = result.preview.join("\r\n");
graphic.save("capture.bin");

var file = new File("capture.ans");
if(!file.open("wb")) {
    alert("Error opening " + file.name);
    exit();
}
graphic.width--;
file.write(graphic.ANSI);
file.close();
print(file.name + " written");
