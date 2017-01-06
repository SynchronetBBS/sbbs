var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
f.write(JSON.stringify(http_request.header));
//f.write(JSON.stringify(http_request));
f.close();