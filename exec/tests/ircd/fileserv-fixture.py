#!/usr/bin/env python3
"""Add private FileServ catalogs to the isolated services BBS fixture."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--jsexec',required=True,type=Path)
a=p.parse_args()
source=Path(__file__).resolve().parents[2]
root=Path('/tmp/ircd-services-bbs-fixture')
for role in ['', 'hub','home','edge']:
    bbs=root/role
    assert str(bbs.resolve()).startswith('/tmp/ircd-services-bbs-fixture')
    for d in ['data/dirs','files/public','files/paid','files/private','files/restricted','files/excluded','files/exempt']:
        (bbs/d).mkdir(parents=True,exist_ok=True)
    ini='''settings=0
[lib:Files]
description=FileServ fixture library
code_prefix=fs_
exempt_ars=
[lib:Exempt]
description=Library exemption fixture
code_prefix=ex_
exempt_ars=LEVEL 50
'''
    for code,ars,dl,settings,lib in [('public','','',16,'Files'),('paid','','',0,'Files'),('private','LEVEL 50','',0,'Files'),('restricted','','LEVEL 90',16,'Files'),('excluded','','',16,'Files'),('exempt','','',0,'Exempt')]:
        ini+=f'''[dir:{lib}:{code}]
name={code}
description=Fixture {code}
path={bbs}/files/{code}/
data_dir={bbs}/data/dirs/
ars={ars}
download_ars={dl}
settings={settings | (1<<18)}
max_files=100
'''
        payload=(b'FileServ fixture payload\r\n'*12000) if code=='public' else b'Protected FileServ fixture\r\n'
        (bbs/'files'/code/'fixture.txt').write_bytes(payload)
    # Explicit catalog symlink escape. The file is registered but must not be sent.
    escape=bbs/'files/public/escape.txt'
    if escape.is_symlink():escape.unlink()
    escape.symlink_to(bbs/'files/private/fixture.txt')
    (bbs/'ctrl/file.ini').write_text(ini)
    script='''load("sbbsdefs.js");
if (system.data_dir.indexOf("/tmp/ircd-services-bbs-fixture/")!==0) throw new Error("Unsafe fixture");
var n=system.matchuser("Some User",false),u=n?new User(n):system.new_user("Some User");
u.settings &= ~(USER_DELETED|USER_INACTIVE);u.security.level=60;u.security.restrictions=0;u.security.exemptions=0;
u.security.password="TESTPASS";u.security.credits=100000;u.security.free_credits=0;
var gn=system.matchuser("Public Reader",false),g=gn?new User(gn):system.new_user("Public Reader");
g.settings &= ~(USER_DELETED|USER_INACTIVE);g.security.level=10;g.security.restrictions=UFLAG_G;g.security.exemptions=0;g.security.credits=0;g.security.free_credits=0;
Object.keys(file_area.dir).forEach(function(code) {var b=new FileBase(code);if(!b.open())throw new Error(b.error);
try{if(!b.get("fixture.txt"))if(!b.add({name:"fixture.txt",desc:"Native fixture payload",cost:100},false))throw new Error(b.error);
if(code=="fs_public"&&!b.get("escape.txt"))if(!b.add({name:"escape.txt",desc:"Symlink escape fixture",cost:0},false))throw new Error(b.error);
}finally{b.close();}});
'''
    subprocess.run([str(a.jsexec),'-n','-c',str(bbs/'ctrl'),'-i',str(source/'load'),'-r',script],check=True)
print('Prepared isolated FileServ catalogs and G-restricted guest accounts')

# Create a private native TLS key/certificate for the local DCC tests only.
from datetime import datetime,timedelta,timezone
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID,ExtendedKeyUsageOID
certpath=root/'ctrl/ssl.cert'
if not certpath.exists():
    setup=root/'fileserv-certificate.js'
    setup.write_text('''var f=new File(system.ctrl_dir+"main.ini");if(!f.open("r"))throw new Error("main");var pass=f.iniGetValue(null,"password");f.close();
if(argv[0]=="create") { var key=new CryptContext(CryptContext.ALGO.RSA);key.keysize=256;key.label="ssl_cert";key.generate_key();
var ks=new CryptKeyset(system.ctrl_dir+"ssl.cert",CryptKeyset.KEYOPT.CREATE);ks.add_private_key(key,pass);ks.close();
var c=new CryptCert(CryptCert.TYPE.CERTREQUEST);c.subjectpublickeyinfo=key;c.commonname="localhost";c.sign(key);
f=new File(argv[1]);f.open("w");f.write(c.export_cert(CryptCert.FORMAT.TEXT_CERTIFICATE));f.close();
} else { f=new File(argv[1]);f.open("rb");var c=new CryptCert(f.read());f.close();var ks=new CryptKeyset(system.ctrl_dir+"ssl.cert");ks.add_public_key(c);ks.close(); }''')
    def command(*args):
        subprocess.run([str(a.jsexec),'-n','-c',str(root/'ctrl'),str(setup),*map(str,args)],check=True,timeout=30)
    command('create',root/'fileserv.csr')
    csr=x509.load_pem_x509_csr((root/'fileserv.csr').read_bytes())
    key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
    name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'FileServ fixture CA')]);now=datetime.now(timezone.utc)
    cert=(x509.CertificateBuilder().subject_name(csr.subject).issuer_name(name).public_key(csr.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now-timedelta(days=1)).not_valid_after(now+timedelta(days=30))
        .add_extension(x509.BasicConstraints(ca=False,path_length=None),True)
        .add_extension(x509.KeyUsage(digital_signature=True,content_commitment=False,key_encipherment=True,data_encipherment=False,key_agreement=False,key_cert_sign=False,crl_sign=False,encipher_only=False,decipher_only=False),True)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),False).sign(key,hashes.SHA256()))
    (root/'fileserv.der').write_bytes(cert.public_bytes(serialization.Encoding.DER))
    command('import',root/'fileserv.der')
print('Prepared isolated FileServ TLS certificate')
