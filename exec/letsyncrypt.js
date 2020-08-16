require("acmev2.js", "ACMEv2");

/*
 * File names used...
 */
var ks_fname = backslash(system.ctrl_dir)+"letsyncrypt.key";
var setting_fname = backslash(system.ctrl_dir)+"letsyncrypt.ini";
var sks_fname = backslash(system.ctrl_dir)+"ssl.cert";
var sbbsini_fname = backslash(system.ctrl_dir)+"sbbs.ini";
var maincnf_fname = backslash(system.ctrl_dir)+"main.cnf";
var recycle_sem = backslash(system.ctrl_dir)+"recycle.web";

function at_least_a_third()
{
	var sks;
	var now;
	var cutoff;
	var cert;

	if (!file_exists(sks_fname))
		return false;
	sks = new CryptKeyset(sks_fname, CryptKeyset.KEYOPT.READONLY);
	try {
		cert = sks.get_public_key("ssl_cert");
	}
	catch(e1) {
		sks.close();
		return false;
	}
	sks.close();
	now = new Date();
	try {
		cutoff = new Date(cert.validfrom.valueOf() + ((cert.validto.valueOf() - cert.validfrom.valueOf())/3)*2);
	}
	catch(badcert) {
		return false;
	}
	return now < cutoff;
}

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

function authorize_order(acme, order, webroots)
{
	var auth;
	var authz;
	var challenge;
	var completed=0;
	var fulfilled;
	var i;
	var tmp;
	var token;
	var tokens=[];
	var waittime;

	/*
	 * Find an http-01 authorization
	 */
	try {
		for (auth in order.authorizations) {
			fulfilled = false;
			authz = acme.get_authorization(order.authorizations[auth]);
			if (authz.status == 'valid') {
				completed++;
				continue;
			}
			for (challenge in authz.challenges) {
				if (authz.challenges[challenge].type=='http-01') {
					/*
					 * Create a place to store the challenge and store it there
					 */
					for (i in webroots) {
						if (!file_isdir(webroots[i]+".well-known/acme-challenge")) {
							if (!mkpath(webroots[i]+".well-known/acme-challenge"))
								throw("Unable to create "+webroots[i]+".well-known/acme-challenge");
							tmp = new File(webroots[i]+".well-known/acme-challenge/webctrl.ini");
							if(tmp.open("w")) {
								tmp.writeln("AccessRequirements=");
								tmp.close();
							} else
								log(LOG_ERR, "Error " + errno + " opening/creating " + tmp.name);
						}
						token = new File(webroots[i]+".well-known/acme-challenge/"+authz.challenges[challenge].token);
						if (tokens.indexOf(token.name) < 0) {
							log(LOG_DEBUG, "Creating " + token.name);
							if(token.open("w")) {
								token.write(authz.challenges[challenge].token+"."+acme.thumbprint());
								tokens.push(token.name);
								token.close();
							} else
								log(LOG_ERR, "Error " + errno + " opening/creating " + token.name);
						} else {
							log(LOG_WARNING, "Token not found: " + token.name);
						}
					}
					acme.accept_challenge(authz.challenges[challenge]);
					fulfilled = true;
				}
			}
			/*
			 * Wait for server to confirm
			 */
			waittime = 1000;
			if (fulfilled) {
				while (!acme.poll_authorization(order.authorizations[auth])) {
					if (waittime > 64000) {
						throw("Authorization timeout");
					}
					mswait(waittime);
					waittime *= 2;
				}
				completed++;
			}
		}
	}
	catch (autherr) {
		for (i in tokens)
			file_remove(tokens[i]);
		throw(autherr);
	}
	if (!completed)
		throw("No challenges fulfilled!");

	for (i in tokens)
		file_remove(tokens[i]);
}

/*
 * Variables declarations
 */
var acme;
var cert;
var csr;
var dir_path = "/directory";
var dnsnames=[];
var domain_list;
var i;
var identifiers = [];
var ks;
var key_id;
var maincnf = new File(maincnf_fname);
var new_host = "acme-v02.api.letsencrypt.org";
var new_domain_hash = '';
var old_domain_hash;
var old_host;
var oldcert;
var order;
var print_tos = false;
var rekey = false;
var renew = false;
var revoke = false;
var rsa;
var sks;
var sks_group_readable = false;
var sbbsini = new File(sbbsini_fname);
var settings = new File(setting_fname);
var syspass;
var webroot;
var webroots = {};
var usersa = true;	// TODO: Make configurable
var keysize = 256;	// TODO: Make configurable... ECC sizes are 32, 48, and 66 (66 is not supported by Let's Encrypt)
var waittime;
var TOSAgreed=false;

/*
 * Get the Web Root
 */
if (!sbbsini.open("r"))
	throw("Unable to open "+sbbsini.name);
webroot = backslash(sbbsini.iniGetValue("Web", "RootDirectory", "../web/root"));
sbbsini.close();

/*
 * Now read the settings and state.
 */
webroots[system.inet_addr] = webroot;
if (settings.open("r")) {
	domain_list = settings.iniGetObject("Domains");
	for (i in domain_list) {
		if (file_isdir(domain_list[i])) {
			webroots[i] = backslash(domain_list[i]);
		}
		else {
			log(LOG_ERR, "Web root for "+i+" is not a directory ("+domain_list[i]+")");
		}
	}
	old_domain_hash = settings.iniGetValue("State", "DomainHash", "<None>");
	old_host = settings.iniGetValue("State", "Host", "acme-v02.api.letsencrypt.org");
	new_host = settings.iniGetValue(null, "Host", new_host);
	dir_path = settings.iniGetValue(null, "Directory", dir_path);
	TOSAgreed = settings.iniGetValue(null, "TOSAgreed", TOSAgreed);
	sks_group_readable = settings.iniGetValue(null, "GroupReadableKeyFile", sks_group_readable);

	settings.close();
}

for (i in Object.keys(webroots).sort())
	new_domain_hash += i+"/";
new_domain_hash = md5_calc(new_domain_hash);

/*
 * Parse arguments
 */
if (argv !== undefined) {
	if (argv.indexOf('--new-key') > -1)
		rekey = true;
	if (argv.indexOf('--revoke') > -1) {
		revoke = true;
		renew = true;
	}
	if (argv.indexOf('--force') > -1)
		renew = true;
	if (argv.indexOf('--tos') > -1)
		print_tos = true;
}

/*
 * Renew if the config has changed
 */
if (old_host != new_host)
	renew = true;
else if (new_domain_hash != old_domain_hash)
	renew = true;
else if (!at_least_a_third())
	renew = true;

if (renew || rekey || revoke || print_tos) {
	/*
	 * Now read in the system password which must be used to encrypt the 
	 * private key.
	 * 
	 * TODO: What happens when the system password changes?
	 */
	if (!maincnf.open("rb", true))
		throw("Unable to open "+maincnf.name);
	maincnf.position = 186; // Indeed.
	syspass = maincnf.read(40);
	syspass = syspass.replace(/\x00/g,'');
	maincnf.close();

	/*
	 * Now open/create the keyset and RSA signing key for
	 * ACME.  Note that this key is not the one used for the
	 * final certificate.
	 */
	ks = new CryptKeyset(ks_fname, file_exists(ks_fname) ? CryptKeyset.KEYOPT.NONE : CryptKeyset.KEYOPT.CREATE);

	/*
	 * The ACME key uses the service hostname as the label.
	 */
	try {
		rsa = ks.get_private_key(new_host, syspass);
	}
	catch(e2) {
		if (usersa) {
			rsa = new CryptContext(CryptContext.ALGO.RSA);
			rsa.keysize=keysize;
		}
		else {
			rsa = new CryptContext(CryptContext.ALGO.ECDSA);
			rsa.keysize=keysize;
		}
		rsa.label=new_host;
		rsa.generate_key();
		ks.add_private_key(rsa, syspass);
	}

	/*
	 * We store the key ID in our ini file so we don't need an extra
	 * round-trip each session to discover it.
	 */
	settings.open(settings.exists ? "r+" : "w+");
	key_id = settings.iniGetValue("key_id", new_host, undefined);
	acme = new ACMEv2({key:rsa, key_id:key_id, host:new_host, dir_path:dir_path, user_agent:'LetSyncrypt '+("$Revision: 1.35 $".split(' ')[1])});
	if (renew || rekey || revoke) {
		if (acme.key_id === undefined) {
			if (TOSAgreed)
				acme.create_new_account({termsOfServiceAgreed:TOSAgreed,contact:["mailto:sysop@"+system.inet_addr]});
			else {
				try {
					acme.create_new_account({contact:["mailto:sysop@"+system.inet_addr]});
				}
				catch (e) {
					log(LOG_ERR, "Creating account without agreeing to ToS failed.");
					log(LOG_ERR, "Please visit "+acme.get_terms_of_service()+" and review the ToS");
					log(LOG_ERR, "Then set TOSAgreed=true in "+settings.name);
					throw(e);
				}
			}
		}
		/*
		 * After the ACMEv2 object is created, we will always have a key_id
		 * Write it to our ini if it wasn't there already.
		 */
		if (key_id === undefined) {
			settings.iniSetValue("key_id", new_host, acme.key_id);
			key_id = acme.key_id;
		}
	}
	settings.close();
}

if (print_tos)
	print("ToS: "+acme.get_terms_of_service());

if (rekey) {
	if (usersa) {
		rsa = new CryptContext(CryptContext.ALGO.RSA);
		rsa.keysize=keysize;
	}
	else {
		rsa = new CryptContext(CryptContext.ALGO.ECDSA);
		rsa.keysize=keysize;
	}
	rsa.label=new_host;
	rsa.generate_key();
	acme.change_key(rsa);
	try {
		ks.delete_key(new_host);
	}
	catch(dkerr) {}
	ks.add_private_key(rsa, syspass);
}

if (revoke) {
	sks = new CryptKeyset(sks_fname, CryptKeyset.KEYOPT.READONLY);
	oldcert = sks.get_public_key("ssl_cert");
	sks.close();
	acme.revoke(oldcert);
	renew=true;
}

if (renew) {
	/*
	 * Create the order, using system.inet_addr
	 */
	for (i in webroots)
		identifiers.push({type:"dns",value:i});
	order = acme.create_new_order({identifiers:identifiers});

	authorize_order(acme, order, webroots);

	/*
	 * Create CSR
	 */
	csr = new CryptCert(CryptCert.TYPE.CERTREQUEST);

	/*
	 * We want to use a new key since there's no reason to
	 * keep using the old one, and changing the key often
	 * is good for security.
	 */

	if (usersa) {
		rsa = new CryptContext(CryptContext.ALGO.RSA);
		rsa.keysize=keysize;
	}
	else {
		rsa = new CryptContext(CryptContext.ALGO.ECDSA);
		rsa.keysize=keysize;
	}
	rsa.label="ssl_cert";
	rsa.generate_key();

	csr.subjectpublickeyinfo=rsa;
	csr.oganizationname=system.name;
	csr.commonname=system.inet_addr;
	for (i in webroots)
		dnsnames.push(i);
	csr.add_extension("2.5.29.17", false, create_dnsnames(dnsnames));
	csr.sign(rsa);
	csr.check();
	order = acme.finalize_order(order, csr);

	waittime = 1000;
	while (order.status !== 'valid') {
		order = acme.poll_order(order);
		if (order.status == 'valid')
			break;
		else if (order.status == 'invalid')
			throw("Order "+order.Location+" invalid!");
		if (waittime > 64000) {
			log(LOG_DEBUG, JSON.stringify(order));
			throw("Timeout waiting for order to be valid");
		}
		mswait(waittime);
		waittime *= 2;
	}

	cert = acme.get_cert(order);
	cert.label = "ssl_certchain";

	/*
	 * Now delete/create the keyset with the key and cert
	 */
	for (i=0; i < 10 && file_exists(sks_fname); i++) {
		if (file_remove(sks_fname))
			break;
		mswait(100);
	}
	if (i == 10)
		throw("Unable to delete file "+sks_fname);

	sks = new CryptKeyset(sks_fname, CryptKeyset.KEYOPT.CREATE);
	sks.add_private_key(rsa, syspass);
	sks.add_public_key(cert);
	sks.close();
	if(sks_group_readable)
		file_chmod(sks_fname, 0x1a0); //0640

	/*
	 * Recycle webserver
	 */
	file_touch(recycle_sem);

	/*
	 * Save the domain hash and any other state information.
	 * If the certificate was from the staging server, note that, so when
	 * we move to non-staging, we can update automatically.
	 */
	if (settings.open(settings.exists ? "r+" : "w+")) {
		settings.iniSetValue("State", "DomainHash", new_domain_hash);
		settings.iniSetValue("State", "Host", new_host);
		settings.iniRemoveKey("State", "Staging");
		settings.close();
	}
	else {
		// SO CLOSE!
		log(LOG_ERR, "!ERROR! Unable to save state after getting certificate");
		log(LOG_ERR, "!ERROR! THIS IS VERY BAD");
		throw("Unable to open "+settings.name+" to save state information!");
	}
}
