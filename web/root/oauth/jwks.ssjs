var ks_fname = backslash(system.ctrl_dir)+"oid.key";
var maincnf_fname = backslash(system.ctrl_dir)+"main.cnf";
var rsa;
var syspass;
var maincnf = new File(maincnf_fname);
var keysize = 256;	// TODO: Make configurable... ECC sizes are 32, 48, and 66

if (!maincnf.open("rb", true))
	throw("Unable to open "+maincnf.name);
maincnf.position = 186; // Indeed.
syspass = maincnf.read(40);
syspass = syspass.replace(/\x00/g,'');
maincnf.close();

ks = new CryptKeyset(ks_fname, file_exists(ks_fname) ? CryptKeyset.KEYOPT.NONE : CryptKeyset.KEYOPT.CREATE);

try {
	rsa = ks.get_private_key(system.host_name, syspass);
}
catch(e2) {
	rsa = new CryptContext(CryptContext.ALGO.RSA);
	rsa.keysize=keysize;
	rsa.label=system.host_name;
	rsa.generate_key();
	ks.add_private_key(rsa, syspass);
}

var ret = {};
ret.keys = [];
var k = get_jwk(rsa);
k.jwk.kid='1';
k.jwk.alg = k.alg;
ret.keys.push(k.jwk);
http_reply.header['Content-Type'] = 'application/jwk-set+json';
write(JSON.stringify(ret));

function get_jwk(key)
{
	var ret = {};

	if (key === undefined)
		throw("change_key() requires a new key.");
	/* Create the inner object signed with old key */
	switch(key.algo) {
		case CryptContext.ALGO.RSA:
			ret.alg = 'RS256';	// Always use SHA-256 with RSA.
			ret.jwk = {e:key.public_key.e, kty:'RSA', n:key.public_key.n};
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
