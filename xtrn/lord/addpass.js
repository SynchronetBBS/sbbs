if (argc !== 2) {
	print('Usage: addpass user pass');
	exit(1);
}
var f = new File(js.exec_dir+'bbs.cred');
if (!f.open('a')) {
	throw ('Unable to open '+f.name);
}
var sha = new CryptContext(CryptContext.ALGO.SHA2);
sha.blocksize = 64;
sha.encrypt(argv[0]+argv[1]);
f.writeln(argv[0]+':'+base64_encode(sha.hashvalue));
