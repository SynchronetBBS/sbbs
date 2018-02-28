// Inspired by p5-Net-ACME2 - https://github.com/FGasper/p5-Net-ACME2/

/*
 * var acme = new ACMEv2({key:CryptContext, key_id:undefined})
 * 
 * // For a new account:
 * {
 * 	var terms_url = acme.get_terms_of_service();
 * 
 * 	acme->create_new_account({termsOfServiceAgreed:1});
 * }
 * 
 * // Save the key_id somewhere so it can be reused.
 * 
 * var order = acme.create_new_order({identifiers:[{type:"dns",value:"example.com"}]});
 * var authz = acme.get_authorization(order.authorizations[0]);
 * var challenges = authz.challenges;
 * 
 * // Pick a challenge, and satisfy it.
 * 
 * acme.accept_challenge(challenge);
 * 
 * while (!acme->poll_authorization(order.authorizations[0]))
 * 	mswait(1000);
 * 
 * // Make a key and CSR for *.example.com
 * 
 * order = acme.finalize_order(order, csr);
 * 
 * while (order.status !== 'valid') {
 * 	mswait(1000);
 * 	order = acme.poll_order(order);
 * }
 * 
 * var cert = acme.get_cert(order);
 */

require("http.js", "HTTPRequest");

function ACMEv2(opts)
{
	if (opts.key === undefined)
		throw('Need "key"!');

	this.key = opts.key;
	this.key_id = opts.key_id;
	if (opts.host !== undefined)
		this.host = opts.host;
	if (opts.dir_path !== undefined)
		this.dir_path = opts.dir_path;
	this.ua = new HTTPRequest();
	if (opts.user_agent !== undefined)
		this.ua.user_agent = opts.user_agent;
	try {
		this.get_key_id();
	}
	catch(e) {}
}

ACMEv2.prototype.get_key_id = function()
{
	if (this.key_id === undefined)
		return(this.create_new_account({onlyReturnExisting:true}));
};

ACMEv2.prototype.host = "acme-staging-v02.api.letsencrypt.org";
ACMEv2.prototype.dir_path = "/directory";

ACMEv2.prototype.get_terms_of_service = function()
{
	var dir = this.get_directory();
	if (dir.meta === undefined)
		throw('No "meta" in directory!');
	if (dir.meta.termsOfService === undefined)
		throw('No "termsOfService" in directory metadata');
	return dir.meta.termsOfService;
};

ACMEv2.prototype.get_directory = function()
{
	if (this.directory === undefined) {
		log(LOG_DEBUG, "Getting directory.");
		var ret = this.ua.Get("https://"+this.host+this.dir_path);
		this.log_headers();
		if (this.ua.response_code != 200) {
			log(LOG_DEBUG, ret);
			throw("Error fetching directory");
		}
		this.update_nonce();
		this.directory = JSON.parse(ret);
	}
	return this.directory;
};

ACMEv2.prototype.create_new_account = function(opts)
{
	var ret = this.post('newAccount', opts);

	if (this.ua.response_code != 201 && this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("newAccount returned "+this.ua.response_code+", not a 200 or 201 status!");
	}

	if (this.ua.response_headers_parsed.Location === undefined) {
		log(LOG_DEBUG, this.ua.response_headers.join("\n"));
		throw("No Location header in newAccount response.");
	}
	this.key_id = this.ua.response_headers_parsed.Location[0];
	return JSON.parse(ret);
};

ACMEv2.prototype.update_account = function(opts)
{
	this.get_key_id();
	log(LOG_DEBUG, "Updating account.");
	var ret = this.post_url(this.key_id, opts);

	if (this.ua.response_code != 201 && this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("update_account returned "+this.ua.response_code+", not a 200 or 201 status!");
	}
	return JSON.parse(ret);
};

ACMEv2.prototype.get_account = function()
{
	return this.update_account({});
};

ACMEv2.prototype.create_new_order = function(opts)
{
	var ret;

	if (opts.identifiers === undefined)
		throw("create_new_order() requires an identifier in opts");
	ret = this.post('newOrder', opts);
	if (this.ua.response_code != 201) {
		log(LOG_DEBUG, ret);
		throw("newOrder responded with "+this.ua.response_code+" not 201");
	}
	ret = JSON.parse(ret);

	if (this.ua.response_headers_parsed.Location === undefined) {
		log(LOG_DEBUG, this.ua.response_headers.join("\n"));
		throw("No Location header in 201 response.");
	}
	ret.Location=this.ua.response_headers_parsed.Location[0];

	return ret;
};

ACMEv2.prototype.accept_challenge = function(challenge)
{
	var opts={keyAuthorization:challenge.token+"."+this.thumbprint()};
	log(LOG_DEBUG, "Accepting challenge.");
	var ret = this.post_url(challenge.url, opts);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("accept_challenge did not return 200");
	}
	return JSON.parse(ret);
};

ACMEv2.prototype.poll_authorization = function(auth)
{
	log(LOG_DEBUG, "Polling authorization.");
	var ret = JSON.parse(this.ua.Get(auth));

	this.log_headers();
	if (this.ua.response_code != 200)
		return false;
	this.update_nonce();

	for (var challenge in ret.challenges) {
		if (ret.challenges[challenge].status == 'valid')
			return true;
		if (ret.challenges[challenge].status == 'invalid') {
			log(LOG_DEBUG, JSON.stringify(ret.challenges[challenge]));
			throw ("Authorization failed... "+auth);
		}
	}
	return false;
};

ACMEv2.prototype.finalize_order = function(order, csr)
{
	var opts = {};

	if (order === undefined)
		throw("Missing order");
	if (csr === undefined)
		throw("Missing csr");
	if (typeof(csr) != 'object' || csr.export_cert === undefined)
		throw("Invalid csr");
	opts.csr = this.base64url(csr.export_cert(CryptCert.FORMAT.CERTIFICATE));

	log(LOG_DEBUG, "Finalizing order.");
	var ret = this.post_url(order.finalize, opts);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("finalize_order did not return 200");
	}

	return JSON.parse(ret);
};

ACMEv2.prototype.poll_order = function(order)
{
	var loc = order.Location;
	if (loc === undefined)
		throw("No order location!");
	log(LOG_DEBUG, "Polling oder.");
	var ret = this.ua.Get(loc);
	this.log_headers();
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("order poll did not return 200");
	}
	this.update_nonce();

	ret = JSON.parse(ret);
	ret.Location = loc;
	return ret;
};

ACMEv2.prototype.asn1_len = function (len) {
	var ret = '';
	var tmplen;

	if (len < 128)
		return ascii(len);
	tmplen = len;
	var count = 0;
	while (tmplen) {
		ret = ascii(tmplen & 0xff)+ret;
		tmplen >>= 8;
		count++;
	}
	ret = ascii(0x80 | count) + ret;
	return ret;
};

ACMEv2.prototype.get_jwk = function(key)
{
	var ret = {};

	if (key === undefined)
		throw("change_key() requires a new key.");
	/* Create the inner object signed with old key */
	switch(key.algo) {
		case CryptContext.ALGO.RSA:
			ret.alg = 'RS256';	// Always use SHA-256 with RSA.
			ret.jwk = {e:key.public_key.e, kty:'RSA', n:key.public_key.n};
			ret.shalen = 256;
			break;
		case CryptContext.ALGO.ECDSA:
			switch(key.keysize) {
				case 32:
					ret.alg = 'ES256';
					ret.jwk = {crv:'P-256',kty:'EC'};
					ret.shalen = 256;
					break;
				case 48:
					ret.alg = 'ES384';
					ret.jwk = {crv:'P-384',kty:'EC'};
					ret.shalen = 384;
					break;
				case 66:
					ret.alg = 'ES512';
					ret.jwk = {crv:'P-521',kty:'EC'};
					ret.shalen = 512;
					break;
				default:
					throw("Unhandled ECC curve size "+key.keysize);
			}
			ret.jwk.x = key.public_key.x;
			ret.jwk.y = key.public_key.y;
			break;
		default:
			throw("Unknown algorithm in new key");
	}
	return ret;
};

ACMEv2.prototype.change_key = function(new_key)
{
	var inner = {protected:{}, payload:{}};
	var old_key = this.key;
	var ret;
	var jwk;

	if (new_key === undefined)
		throw("change_key() requires a new key.");
	/* Create the inner object signed with old key */
	jwk = this.get_jwk(new_key);
	inner.protected.alg = jwk.alg;
	inner.protected.jwk = jwk.jwk;
	inner.protected.url = this.get_directory().keyChange;
	inner.payload.account = this.key_id;
	inner.payload.newKey = inner.protected.jwk;
	inner.protected = this.base64url(JSON.stringify(inner.protected));
	inner.payload = this.base64url(JSON.stringify(inner.payload));
	inner.signature = this.base64url(this.hash_thing(inner.protected + "." + inner.payload, new_key));
	ret = this.post('keyChange', inner);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("keyChange did not return 200");
	}
	this.key = new_key;
	return JSON.parse(ret);
};

ACMEv2.prototype.revoke = function(cert, reason)
{
	var opts = {};
	var ret;

	if (reason === undefined)
		reason = 0;
	if (typeof(reason) === 'string') {
		switch(reason) {
			case 'unspecified':
				reason = 0;
				break;
			case 'keyCompromise':
				reason = 1;
				break;
			case 'cACompromise':
				reason = 2;
				break;
			case 'affiliationChanged':
				reason = 3;
				break;
			case 'superseded':
				reason = 4;
				break;
			case 'cessationOfOperation':
				reason = 5;
				break;
			case 'certificateHold':
				reason = 6;
				break;
			case 'removeFromCRL':
				reason = 8;
				break;
			case 'privilegeWithdrawn':
				reason = 9;
				break;
			case 'aACompromise':
				reason = 10;
				break;
		}
	}
	reason = parseInt(reason);
	opts.certificate = this.base64url(cert.export_cert(CryptCert.FORMAT.CERTIFICATE));
	opts.reason = reason;
	ret = this.post('revokeCert', opts);
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("revokeCert did not return 200");
	}
	return;
};

ACMEv2.prototype.create_pkcs7 = function(cert)
{
	var ret = '';
	var certs = [];
	var m;
	var onecert;
	var rex = /(-+BEGIN[^\-]+-+[^\-]+-+END[^\-]+-+\n)/g;

	while ((m = rex.exec(cert)) != null) {
		onecert = new CryptCert(m[1]);
		ret = ret + onecert.export_cert(CryptCert.FORMAT.CERTIFICATE);
	}
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0xa0) + ret;
	ret = ret + ascii(0x31) + ascii(0x00);
	ret = ascii(0x02) + ascii(0x01) + ascii(0x01) + ascii(0x31) + ascii(0x00) + ascii(0x30) + ascii(0x0B) + ascii(0x06) + ascii(0x09) + ascii(0x2A) + ascii(0x86) + ascii(0x48) + ascii(0x86) + ascii(0xF7) + ascii(0x0D) + ascii(0x01) + ascii(0x07) + ascii(0x01) + ret;
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0x30) + ret;
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0xa0) + ret;
	ret = ascii(0x06) + ascii(0x09) + ascii(0x2A) + ascii(0x86) + ascii(0x48) + ascii(0x86) + ascii(0xF7) + ascii(0x0D) + ascii(0x01) + ascii(0x07) + ascii(0x02) + ret;
	ret = this.asn1_len(ret.length) + ret;
	ret = ascii(0x30) + ret;

	return ret;
};

ACMEv2.prototype.get_cert = function(order)
{
	if (order.certificate === undefined)
		throw("Order has no certificate!");
	log(LOG_DEBUG, "Getting certificate.");
	var cert = this.ua.Get(order.certificate);
	this.log_headers();
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, cert);
		throw("get_cert request did not return 200");
	}
	this.update_nonce();

	return new CryptCert(this.create_pkcs7(cert));
};

ACMEv2.prototype.FULL_JWT_METHODS = [
	'newAccount',
];
ACMEv2.prototype.post = function(link, data)
{
	var post_method;
	var url;

	if (this.FULL_JWT_METHODS.indexOf(link) > -1)
		post_method = 'post_full_jwt';
	url = this.get_directory()[link];
	if (url === undefined)
		throw('Unknown link name: "'+link+'"');
	log(LOG_DEBUG, "Calling "+link+".");
	return this.post_url(url, data, post_method);
};

ACMEv2.prototype.get_nonce = function()
{
	var ret;

	if (this.last_nonce === undefined) {
		ret = this.ua.Head(this.get_directory().newNonce);
		this.last_nonce = ret['Replay-Nonce'][0];
	}

	ret = this.last_nonce;
	this.last_nonce = undefined;
	return ret;
};

ACMEv2.prototype.get_authorization = function(url)
{
	log(LOG_DEBUG, "Getting authorization.");
	var ret = this.ua.Get(url);

	this.log_headers();
	if (this.ua.response_code != 200) {
		log(LOG_DEBUG, ret);
		throw("get_authorization request did not return 200");
	}
	this.update_nonce();

	return JSON.parse(ret);
};

ACMEv2.prototype.base64url = function(string)
{
	string = base64_encode(string);
	string = string.replace(/\=/g,'');
	string = string.replace(/\+/g,'-');
	string = string.replace(/\//g,'_');
	return string;
};

ACMEv2.prototype.hash_thing = function(data, key)
{
	var shactx;
	var jwk;

	function rsa_hash(shactx, key) {
		var MD = shactx.hashvalue;
		var D = '';

		D = ascii(0x30) + ascii(0x31) + ascii(0x30) + ascii(0x0d) + ascii(0x06) + ascii(0x09) +
			ascii(0x60) + ascii(0x86) + ascii(0x48) + ascii(0x01) + ascii(0x65) + ascii(0x03) +
			ascii(0x04) + ascii(0x02) + ascii(0x01) + ascii(0x05) + ascii(0x00) + ascii(0x04) +
			ascii(0x20) + MD;
		D = ascii(0) + D;
		while(D.length < key.keysize-2)
			D = ascii(255)+D;
		D = ascii(0x00) + ascii(0x01) + D;
		return key.decrypt(D);
	}

	function ecc_hash(shactx, key) {
		var offset = 0;
		var t;
		var len;
		var llen;
		var i;
		var ints = [];
		var sig = shactx.create_signature(key);

		while (offset < sig.length && offset > -1) {
			t = ascii(sig.substr(offset, 1));
			if ((t & 0x1f) == 0x1f) {
				for (offset++; offset < sig.length; offset++) {
					if ((ascii(sig.substr(offset,1)) & 0x80) == 0)
						break;
				}
				t = -1;
			}
			else
				offset++;
			len = ascii(sig.substr(offset, 1));
			offset++;
			if (len > 127) {
				llen = len & 0x7f;
				len = 0;
				for (i = 0; i<llen; i++) {
					len <<= 8;
					len |= ascii(sig.substr(offset,1));
					offset++;
				}
			}
			if (t == 2) {
				ints.push(sig.substr(offset, len).replace(/^\x00/,''));
				offset += len;
			}
			else if (t == 0x30 || t == 4) {
			}
			else
				offset += len;
		}

		return(ints[1] + ints[2]);
	}

	jwk = this.get_jwk(key);
	shactx = new CryptContext(CryptContext.ALGO.SHA2);
	shactx.blocksize = jwk.shalen/8;
	shactx.encrypt(data);
	shactx.encrypt('');
	var tmp;
	var i;

	if (jwk.alg == 'RS256')
		return rsa_hash(shactx, key);
	else {
		return ecc_hash(shactx, key);
	}
};

ACMEv2.prototype.update_nonce = function()
{
	if (this.ua.response_headers_parsed['Replay-Nonce'] !== undefined)
		this.last_nonce = this.ua.response_headers_parsed['Replay-Nonce'][0];
};

ACMEv2.prototype.thumbprint = function()
{
	var jwk = this.get_jwk(this.key);
	var shactx;

	shactx = new CryptContext(CryptContext.ALGO.SHA2);
	shactx.encrypt(JSON.stringify(jwk.jwk));
	shactx.encrypt('');
	var MD = shactx.hashvalue;
	return this.base64url(MD);
};

ACMEv2.prototype.log_headers = function()
{
	var i;

	if (this.ua.response_headers_parsed.Location !== undefined)
		for (i in this.ua.response_headers_parsed.Location)
			log(LOG_DEBUG, "Link: "+this.ua.response_headers_parsed.Location[i]);
	if (this.ua.response_headers_parsed.Link !== undefined)
		for (i in this.ua.response_headers_parsed.Link)
			log(LOG_DEBUG, "Link: "+this.ua.response_headers_parsed.Link[i]);

}

ACMEv2.prototype.post_url = function(url, data, post_method)
{
	var protected = {};
	var ret;
	var msg = {};
	var jwk;

	if (post_method === undefined)
		post_method = 'post_key_id';

	jwk = this.get_jwk(this.key);
	switch(post_method) {
		case 'post_key_id':
			protected.nonce = this.get_nonce();
			protected.url = url;
			protected.alg = jwk.alg;
			protected.kid = this.key_id;
			if (protected.kid === undefined)
				throw("No key_id available!");
			break;
		case 'post_full_jwt':
			protected.nonce = this.get_nonce();
			protected.url = url;
			protected.alg = jwk.alg;
			protected.jwk = jwk.jwk;
			break;
	}
	msg.protected = this.base64url(JSON.stringify(protected));
	msg.payload = this.base64url(JSON.stringify(data));
	msg.signature = this.base64url(this.hash_thing(msg.protected + "." + msg.payload, this.key));
	var body = JSON.stringify(msg);
	body = body.replace(/:"/g, ': "');
	body = body.replace(/:\{/g, ': {');
	body = body.replace(/,"/g, ', "');
	ret = this.ua.Post(url, body, undefined, undefined, "application/jose+json");
	this.log_headers();
	/* We leave error handling to the caller */
	if (this.ua.response_code == 200 || this.ua.response_code == 201)
		this.update_nonce();
	return ret;
};

// TODO: Deactivate an Authorization
