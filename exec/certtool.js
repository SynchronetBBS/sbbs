require("acmev2.js", "ACMEv2");

/*
 * File names used...
 */
var sks_fname = backslash(system.ctrl_dir)+"ssl.cert";
var maincnf_fname = backslash(system.ctrl_dir)+"main.cnf";
var recycle_sem = backslash(system.ctrl_dir)+"recycle.web";
var csr_fname = backslash(system.ctrl_dir)+"csr.cert";

var rsa;
var csr;
var cert;
var domains = [system.inet_addr];
var i;
var ks;
var maincnf = new File(maincnf_fname);
var syspass;
var f;

function create_dnsnames(names) {
	var ext = '';
	var tmplen;
	var count;
	var name;

	for (name in names) {
		ext = names[name] + ext;
		ext = ACMEv2.prototype.asn1_len(names[name].length) + ext;
		ext = ascii(0x82) + ext;
	}
	ext = ACMEv2.prototype.asn1_len(ext.length) + ext;
	ext = ascii(0x30) + ext;
	return ext;
}

if (!maincnf.open("rb", true))
	throw("Unable to open "+maincnf.name);
maincnf.position = 186; // Indeed.
syspass = maincnf.read(40);
syspass = syspass.replace(/\x00/g,'');
maincnf.close();

for (i=0; i<argc; i++) {
	if (argv[i] == '--domain' && i+1 < argc) {
		if (domains.indexOf(argv[++i]) < 0)
			domains.push(argv[i]);
	}
}

if (argv.indexOf('--csr') > -1) {
	csr = new CryptCert(CryptCert.TYPE.CERTREQUEST);
	rsa = new CryptContext(CryptContext.ALGO.RSA);
	rsa.keysize=256;
	rsa.label="ssl_cert";
	rsa.generate_key();
	if (file_exists(csr_fname))
		file_remove(csr_fname);
	ks = new CryptKeyset(csr_fname, CryptKeyset.KEYOPT.CREATE);
	ks.add_private_key(rsa, syspass);
	ks.close();

	csr.subjectpublickeyinfo=rsa;
	csr.oganizationname=system.name;
	csr.commonname=system.inet_addr;
	csr.add_extension("2.5.29.17", false, create_dnsnames(domains));
	csr.sign(rsa);
	csr.check();
	print(csr.export_cert(CryptCert.FORMAT.TEXT_CERTIFICATE));
}
if (argv.indexOf('--import') > -1) {
	ks = new CryptKeyset(csr_fname, CryptKeyset.KEYOPT.READONLY);
	rsa = ks.get_private_key("ssl_cert", syspass);
	ks.close();

	i = argv.indexOf('--import') + 1;
	if (i>=argc)
		throw("No cert filename specified");
	f = new File(argv[i]);
	if (!f.open("rb"))
		throw("Unable to open "+f.name);
	cert = f.read();
	f.close();
	cert = ACMEv2.prototype.create_pkcs7(cert);
	cert = new CryptCert(cert);
	//cert.check();

	for (i=0; i < 10; i++) {
		if (file_remove(sks_fname))
			break;
		mswait(100);
	}
	if (i == 10)
		throw("Unable to delete file "+sks_fname);

	ks = new CryptKeyset(sks_fname, CryptKeyset.KEYOPT.CREATE);
	ks.add_private_key(rsa, syspass);
	ks.add_public_key(cert);
	ks.close();
	print("Certificate imported, delete "+csr_fname+" after verifying.);
	file_touch(recycle_sem);
}
