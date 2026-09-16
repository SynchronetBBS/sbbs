require("acmev2.js", "ACMEv2");

/*
 * File names used...
 */
var ks_fname = backslash(system.ctrl_dir)+"letsyncrypt.key";
var setting_fname = backslash(system.ctrl_dir)+"letsyncrypt.ini";
var sks_fname = backslash(system.ctrl_dir)+"ssl.cert";
var sks_new_fname = sks_fname+".new";
var sks_old_fname = sks_fname+".old";
var main_ini_fname = backslash(system.ctrl_dir)+"main.ini";
var recycle_sem = backslash(system.ctrl_dir)+"recycle";

function at_least_a_third()
{
	var sks;
	var now;
	var cutoff;
	var cert;

	if (!file_exists(sks_fname))
		return false;
	/*
	 * An unreadable keyset (truncated, corrupt, wrong system password) must
	 * mean "renew it", not an uncaught exception - otherwise a damaged
	 * ssl.cert wedges every future run of this script.
	 */
	try {
		sks = new CryptKeyset(sks_fname, CryptKeyset.KEYOPT.READONLY);
	}
	catch(e0) {
		log(LOG_WARNING, "Unable to read "+sks_fname+": "+e0);
		return false;
	}
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

function retry_rename(from, to)
{
	var i;

	for (i = 0; i < 10; i++) {
		if (file_rename(from, to))
			return true;
		mswait(100);
	}
	return false;
}

function authorize_order(acme, order, webroots)
{
	var auth;
	var authz;
	var challenge;
	var completed=0;
	var failures=[];
	var fulfilled;
	var i;
	var name;
	var timed_out;
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
			name = (authz.identifier === undefined) ? order.authorizations[auth] : authz.identifier.value;
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
						}
					}
					acme.accept_challenge(authz.challenges[challenge]);
					fulfilled = true;
				}
			}
			if (!fulfilled) {
				failures.push("No http-01 challenge offered for "+name);
				continue;
			}
			/*
			 * Wait for server to confirm
			 */
			waittime = 1000;
			timed_out = false;
			try {
				while (!acme.poll_authorization(order.authorizations[auth])) {
					if (waittime > 64000) {
						timed_out = true;
						throw("Authorization timeout for "+name);
					}
					mswait(waittime);
					waittime *= 2;
				}
				completed++;
			}
			catch (e) {
				failures.push((e.message === undefined) ? e : e.message);
				/* A slow responder will just be slow again - stop here. */
				if (timed_out)
					break;
			}
		}
	}
	catch (autherr) {
		for (i in tokens)
			file_remove(tokens[i]);
		throw(autherr);
	}

	for (i in tokens)
		file_remove(tokens[i]);

	/*
	 * A single invalid authorization invalidates the whole order, so there is
	 * nothing to finalize.  Name every identifier that failed and why, rather
	 * than letting finalize_order() report an opaque 403 later on.
	 */
	if (failures.length > 0) {
		for (i in failures)
			log(LOG_ERR, failures[i]);
		throw(failures.join("; "));
	}
	if (!completed)
		throw("No challenges fulfilled!");
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
var main_ini = new File(main_ini_fname);
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
var settings = new File(setting_fname);
var syspass;
var webroot;
var webroots = {};
var usersa = true;	// TODO: Make configurable
var keysize = 256;	// TODO: Make configurable... ECC sizes are 32, 48, and 66 (66 is not supported by Let's Encrypt)
var waittime;
var TOSAgreed=false;
var sbbsini = load("sbbsini.js");
var sysop_email = "sysop@" + system.inet_addr;

/*
 * Now read the settings and state.
 */
webroots[sbbsini.web.host_name] = backslash(sbbsini.web.root_dir);
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
	sysop_email = settings.iniGetValue(null, "SysopEmail", sysop_email);

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
if (old_host != new_host) {
	// If we change hosts, delete the old private key.
	file_remove(ks_fname);
	renew = true;
}
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
	if (!main_ini.open("r", true))
		throw("Unable to open "+main_ini.name);
	syspass = main_ini.iniGetValue(null, "password");
	main_ini.close();

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
	acme = new ACMEv2({key:rsa, key_id:key_id, host:new_host, dir_path:dir_path, user_agent:'LetSyncrypt 1.36'});
	if (renew || rekey || revoke) {
		if (acme.key_id === undefined) {
			if (TOSAgreed)
				acme.create_new_account({termsOfServiceAgreed:TOSAgreed,contact:["mailto:"+sysop_email]});
			else {
				try {
					acme.create_new_account({contact:["mailto:"+sysop_email]});
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
	 * Create the order, using sbbsini.web.host_name
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
	csr.commonname=sbbsini.web.host_name;
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
	 * Build the keyset in a temporary file and rename it into place.  Failing
	 * part-way through must leave the working certificate alone: it holds the
	 * only copy of its private key, so destroying it before the replacement
	 * exists takes TLS down at the next recycle with no way back.
	 */
	file_remove(sks_new_fname);
	sks = new CryptKeyset(sks_new_fname, CryptKeyset.KEYOPT.CREATE);
	sks.add_private_key(rsa, syspass);
	sks.add_public_key(cert);
	sks.close();
	if(sks_group_readable)
		file_chmod(sks_new_fname, 0x1a0); //0640

	/*
	 * rename() does not replace an existing file on Windows, so move the old
	 * certificate aside instead of deleting it - if the second rename fails it
	 * can be put back.
	 */
	file_remove(sks_old_fname);
	if (file_exists(sks_fname) && !retry_rename(sks_fname, sks_old_fname)) {
		file_remove(sks_new_fname);
		throw("Unable to rename "+sks_fname+" to "+sks_old_fname);
	}
	if (!retry_rename(sks_new_fname, sks_fname)) {
		retry_rename(sks_old_fname, sks_fname);
		throw("Unable to rename "+sks_new_fname+" to "+sks_fname);
	}
	file_remove(sks_old_fname);

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
