#!/usr/bin/env python3
"""Native IRCd TLS regressions. Requires Python cryptography and a private BBS
fixture (services-fixture.py); never use a production control directory.
Certificates, configurations and logs are kept in the printed /tmp directory.
"""
import argparse
from datetime import datetime, timedelta, timezone
from pathlib import Path
import select
import re
import shutil
import signal
import socket
import ssl
import subprocess
import tempfile
import threading
import time

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsexec', type=Path, required=True)
parser.add_argument('--ctrl', type=Path, default=Path('/tmp/ircd-services-bbs-fixture/ctrl'))
parser.add_argument('--routes-only', action='store_true', help='Run the three-server route and services scenarios only')
args = parser.parse_args()
source = Path(__file__).resolve().parents[2]
assert args.ctrl.resolve().is_relative_to(Path('/tmp')), 'Use a private fixture under /tmp'
run = Path(tempfile.mkdtemp(prefix='ircd-tls-network-'))
ctrl = run / 'ctrl'
shutil.copytree(args.ctrl, ctrl, ignore=shutil.ignore_patterns('ssl.cert'))
# The route authority below is a.test; use a matching private BBS QWK ID.
main = ctrl / 'main.ini'
main.write_text(re.sub(r'(?m)^qwk_id=.*$', 'qwk_id=A', main.read_text(encoding='latin1')), encoding='latin1')
print('TLS test files:', run, flush=True)


def native(script, *arguments):
    return [str(args.jsexec), '-n', '-c', str(ctrl), '-i', str(source / 'load'), str(script), *map(str, arguments)]


def certificate(name, issuer=None, ca=False, start=-1, end=2, dns='localhost', client_only=False, public_key=None, eku=True):
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, name)])
    now = datetime.now(timezone.utc)
    builder = (x509.CertificateBuilder().subject_name(subject)
               .issuer_name(issuer[0].subject if issuer else subject)
               .public_key(public_key or key.public_key()).serial_number(x509.random_serial_number())
               .not_valid_before(now + timedelta(days=start)).not_valid_after(now + timedelta(days=end))
               .add_extension(x509.BasicConstraints(ca=ca, path_length=None), critical=True)
               .add_extension(x509.SubjectKeyIdentifier.from_public_key(public_key or key.public_key()), critical=False)
               .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key((issuer[1] if issuer else key).public_key()), critical=False)
               .add_extension(x509.KeyUsage(digital_signature=True, content_commitment=False, key_encipherment=True,
                                            data_encipherment=False, key_agreement=False, key_cert_sign=ca,
                                            crl_sign=ca, encipher_only=False, decipher_only=False), critical=True))
    if not ca or client_only:
        builder = builder.add_extension(x509.SubjectAlternativeName([x509.DNSName(dns)]), critical=False)
        if eku:
            builder = builder.add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH if client_only else ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
    cert = builder.sign(issuer[1] if issuer else key, hashes.SHA256())
    (run / (name + '.pem')).write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    (run / (name + '.key')).write_bytes(key.private_bytes(serialization.Encoding.PEM,
                                                        serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    return cert, key


root = certificate('root', ca=True)
middle = certificate('intermediate', root, ca=True)
other_root = certificate('other-root', ca=True)
expired_middle = certificate('expired-middle', root, ca=True, start=-3, end=-2)
client_middle = certificate('client-middle', root, ca=True, client_only=True)
not_ca_middle = certificate('not-ca-middle', root)
for name, issuer, options in [
        ('valid', root, {}), ('intermediate-peer', middle, {}), ('untrusted', other_root, {}),
        ('no-eku', root, {'eku': False}),
        ('expired-no-eku', root, {'eku': False, 'start': -3, 'end': -2}),
        ('untrusted-no-eku', other_root, {'eku': False}),
        ('self-signed', None, {}), ('expired', root, {'start': -3, 'end': -2}),
        ('future', root, {'start': 1, 'end': 2}), ('wrong-name', root, {'dns': 'wrong.test'}),
        ('client-only', root, {'client_only': True})]:
    certificate(name, issuer, **options)
certificate('expired-middle-peer', expired_middle)
certificate('not-ca-middle-peer', not_ca_middle)
certificate('client-middle-peer', client_middle)
(run / 'intermediate-chain.pem').write_bytes((run / 'intermediate-peer.pem').read_bytes() + (run / 'intermediate.pem').read_bytes())
(run / 'untrusted-chain.pem').write_bytes((run / 'untrusted.pem').read_bytes() + (run / 'other-root.pem').read_bytes())
for prefix in ['expired-middle', 'not-ca-middle', 'client-middle']:
    (run / (prefix + '-chain.pem')).write_bytes((run / (prefix + '-peer.pem')).read_bytes() + (run / (prefix + '.pem')).read_bytes())
(run / 'not-ca.pem').write_bytes((run / 'valid.pem').read_bytes())
(run / 'empty.pem').write_text('not a certificate\n')

# Provision a CA-issued native server certificate without copying live TLS keys.
setup = run / 'certificate.js'
setup.write_text('''
var main = new File(system.ctrl_dir + "main.ini"); main.open("r");
var password = main.iniGetValue(null, "password"); main.close();
var ks, cert;
if (argv[0] == "create") {
    var key = new CryptContext(CryptContext.ALGO.RSA);
    key.keysize = 256; key.label = "ssl_cert"; key.generate_key();
    ks = new CryptKeyset(system.ctrl_dir + "ssl.cert", CryptKeyset.KEYOPT.CREATE);
    ks.add_private_key(key, password); ks.close();
    cert = new CryptCert(CryptCert.TYPE.CERTREQUEST);
    cert.subjectpublickeyinfo = key; cert.commonname = "localhost"; cert.sign(key);
    var f = new File(argv[1]); f.open("w"); f.write(cert.export_cert(CryptCert.FORMAT.TEXT_CERTIFICATE)); f.close();
} else {
    var f = new File(argv[1]); f.open("rb"); cert = new CryptCert(f.read()); f.close();
    ks = new CryptKeyset(system.ctrl_dir + "ssl.cert"); ks.add_public_key(cert); ks.close();
}
''')
subprocess.run(native(setup, 'create', run / 'native.csr'), check=True, timeout=30)
csr = x509.load_pem_x509_csr((run / 'native.csr').read_bytes())
server_cert, _ = certificate('native', root, public_key=csr.public_key())
(run / 'native.der').write_bytes(server_cert.public_bytes(serialization.Encoding.DER))
subprocess.run(native(setup, 'import', run / 'native.der'), check=True, timeout=30)


class Peer:
    def __init__(self, cert, key, tls=True, handshake_reply=False):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen()
        self.listener.settimeout(15)
        self.port = self.listener.getsockname()[1]
        self.received = b''
        self.error = None
        self.connected = threading.Event()
        self.done = threading.Event()
        self.reply = handshake_reply
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER) if tls else None
        if tls:
            self.context.load_cert_chain(run / cert, run / key)
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def serve(self):
        try:
            connection, _ = self.listener.accept()
            connection.settimeout(10)
            with (self.context.wrap_socket(connection, server_side=True) if self.context else connection) as stream:
                self.connected.set()
                replied = False
                while True:
                    data = stream.recv(16384)
                    if not data:
                        break
                    self.received += data
                    if self.reply and not replied and b'\r\nSERVER ' in self.received:
                        stream.sendall(b'PASS tls-test-password :TS\r\nCAPAB TSJOIN TSMODE\r\nSERVER peer.test 1 :TLS test peer\r\n')
                        replied = True
        except (ssl.SSLError, OSError) as error:
            self.error = str(error)
        finally:
            self.listener.close()
            self.done.set()


cases = [
    ('trusted CA', 'valid.pem', 'valid.key', 'root.pem', True),
    ('trusted CA without optional EKU extension', 'no-eku.pem', 'no-eku.key', 'root.pem', True),
    ('expired certificate without optional EKU', 'expired-no-eku.pem', 'expired-no-eku.key', 'root.pem', False),
    ('untrusted issuer without optional EKU', 'untrusted-no-eku.pem', 'untrusted-no-eku.key', 'root.pem', False),
    ('intermediate chain, root omitted by peer', 'intermediate-chain.pem', 'intermediate-peer.key', 'root.pem', True),
    ('untrusted issuer', 'untrusted.pem', 'untrusted.key', 'root.pem', False),
    ('untrusted root supplied by peer', 'untrusted-chain.pem', 'untrusted.key', 'root.pem', False),
    ('self-signed certificate', 'self-signed.pem', 'self-signed.key', 'root.pem', False),
    ('expired certificate', 'expired.pem', 'expired.key', 'root.pem', False),
    ('future certificate', 'future.pem', 'future.key', 'root.pem', False),
    ('wrong hostname', 'wrong-name.pem', 'wrong-name.key', 'root.pem', False),
    ('client-only certificate', 'client-only.pem', 'client-only.key', 'root.pem', False),
    ('expired intermediate CA', 'expired-middle-chain.pem', 'expired-middle-peer.key', 'root.pem', False),
    ('issuer without CA constraints', 'not-ca-middle-chain.pem', 'not-ca-middle-peer.key', 'root.pem', False),
    ('client-only intermediate CA', 'client-middle-chain.pem', 'client-middle-peer.key', 'root.pem', False),
    ('missing CA bundle', 'valid.pem', 'valid.key', 'missing.pem', False),
    ('empty CA bundle', 'valid.pem', 'valid.key', 'empty.pem', False),
    ('non-CA in bundle', 'valid.pem', 'valid.key', 'not-ca.pem', False),
]
for name, cert, key, ca, accept in ([] if args.routes_only else cases):
    peer = Peer(cert, key)
    result = subprocess.run(native(source / 'tests/ircd/tls-worker.sjs', run / ca, 'localhost', peer.port,
                                   'accept' if accept else 'reject'), capture_output=True, text=True, errors='replace', timeout=30)
    assert result.returncode == 0, name + ': ' + result.stdout + result.stderr
    peer.done.wait(16)
    assert peer.done.is_set(), name + ': peer thread did not finish'
    assert (b'PASS tls-test-password :TS\r\n' in peer.received) == accept, name + ': credential leak or missing handshake'
    print('PASS:', name, flush=True)


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def receive_until(stream, marker, timeout=8):
    data = b''
    end = time.monotonic() + timeout
    while marker not in data and time.monotonic() < end:
        if select.select([stream], [], [], .2)[0]:
            part = stream.recv(16384)
            if not part:
                break
            data += part
    assert marker in data, repr(marker) + ' missing from ' + repr(data)
    return data


def daemon_test(automatic):
    peer = Peer('valid.pem', 'valid.key', handshake_reply=True)
    plain, tls = free_port(), free_port()
    config = run / ('automatic.ini' if automatic else 'manual.ini')
    config.write_text(f'''[Info]
Servername=hub.test
Description=TLS regression
[TLS]
CAFile={run / 'root.pem'}
[Port:{plain}]
Default=true
[Port:{tls}]
TLS=true
[Class:1]
PingFrequency=120
ConnectFrequency=0
Maximum=100
SendQ=2000000
[Class:30]
PingFrequency=120
ConnectFrequency={1 if automatic else 0}
Maximum=100
SendQ=2000000
[Allow]
Mask=*@*
Class=1
[Operator:testop]
Nick=testop
Mask=*@*
Password=testpass
Flags=OD
Class=1
[Server:peer]
Servername=peer.test
Hostname=localhost
Password=tls-test-password
Port={peer.port}
TLS=true
Hub=true
Class=30
[Server:oldleaf]
Servername=leaf.test
Hostname=localhost
Password=leaf-test-password
Class=1
[Server:secured]
Servername=secured.test
Hostname=localhost
Password=tls-test-password
TLS=true
Class=1
''')
    logfile = run / (config.stem + '.log')
    process = None
    try:
        with logfile.open('w') as log:
            process = subprocess.Popen(native(source / 'ircd.js', '-f', config, '-a', '127.0.0.1'),
                                       stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
            end = time.monotonic() + 20
            while True:
                assert process.poll() is None, logfile.read_text()
                try:
                    client = socket.create_connection(('127.0.0.1', plain), timeout=.5)
                    break
                except OSError:
                    assert time.monotonic() < end, 'IRCd not ready: ' + logfile.read_text()
                    time.sleep(.1)
            with client:
                client.sendall(b'NICK testop\r\nUSER testop 0 * :TLS tests\r\n')
                receive_until(client, b' 376 ')
                client.sendall(b'OPER testop testpass\r\n')
                receive_until(client, b' 381 ')
                if not automatic:
                    client.sendall(f'CONNECT peer.test {peer.port}\r\n'.encode())
                end = time.monotonic() + 10
                while True:
                    client.sendall(b'LINKS\r\n')
                    links = receive_until(client, b' 365 ')
                    if b' 364 testop peer.test ' in links:
                        break
                    assert time.monotonic() < end, 'TLS link not registered: ' + logfile.read_text()
                    time.sleep(.1)
                assert b'PASS tls-test-password :TS\r\n' in peer.received
                with socket.create_connection(('127.0.0.1', plain), timeout=5) as insecure:
                    insecure.sendall(b'PASS tls-test-password :TS\r\nSERVER secured.test 1 :Downgrade\r\n')
                    receive_until(insecure, b'TLS is required for this server link.')
                # TLS-only listener: no plaintext IRC banner or credentials.
                with socket.create_connection(('127.0.0.1', tls), timeout=5) as raw:
                    raw.sendall(b'PASS tls-test-password :TS\r\nSERVER peer.test 1 :Plaintext\r\n')
                    raw.shutdown(socket.SHUT_WR)
                    try:
                        response = raw.recv(16384)
                    except ConnectionResetError:
                        response = b''
                    assert b'NOTICE ' not in response and b'SERVER ' not in response
                # Existing plaintext port stays available to legacy leaves.
                with socket.create_connection(('127.0.0.1', plain), timeout=5) as leaf:
                    leaf.sendall(b'PASS leaf-test-password :TS\r\nSERVER leaf.test 1 :Legacy leaf\r\n')
                    receive_until(leaf, b'SERVER hub.test ')
                # Native TLS server role serves the installed BBS ssl.cert.
                context = ssl.create_default_context(cafile=run / 'root.pem')
                with context.wrap_socket(socket.create_connection(('127.0.0.1', tls), timeout=5), server_hostname='localhost') as secure:
                    secure.sendall(b'NICK tlsclient\r\nUSER tlsclient 0 * :TLS client\r\n')
                    receive_until(secure, b' 001 ')
                client.sendall(b'DIE\r\n')
                assert process.wait(timeout=10) == 0, 'IRCd shutdown failed: ' + logfile.read_text()
            print('PASS:', 'automatic' if automatic else 'manual CONNECT', 'TLS link and separate listeners', flush=True)
    finally:
        if process and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                code = process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
                raise AssertionError('IRCd did not shut down: ' + logfile.read_text())
            assert code in (0, -signal.SIGTERM), 'IRCd crashed: ' + logfile.read_text()
        assert process is None or process.returncode in (0, -signal.SIGTERM), 'IRCd crashed: ' + logfile.read_text()


if not args.routes_only:
    daemon_test(True)
    daemon_test(False)


def route_test():
    """Real three-hub routes, burst replay, legacy peers, and changed links."""
    nodes = {name: {'plain': free_port(), 'tls': free_port()} for name in ['a', 'b', 'c']}
    processes = []

    def start(name, encrypted=True):
        node = nodes[name]
        config = run / f'route-{name}.ini'
        text = f'''[Info]
Servername={name}.test
Description=TLS route regression
[TLS]
CAFile={run / 'root.pem'}
[Port:{node['plain']}]
Default=true
[Port:{node['tls']}]
TLS=true
[Class:1]
PingFrequency=120
ConnectFrequency=0
Maximum=100
SendQ=2000000
[Class:30]
PingFrequency=120
ConnectFrequency=1
Maximum=100
SendQ=2000000
[Allow]
Mask=*@*
Class=1
[Services:embedded]
Servername=127.0.0.1
[BBSServices]
NickServ=true
MemoServ=true
RequireTLS=true
[EmbeddedServices]
{'Enabled=true' if name == 'a' else '; Central authority stays disabled by default'}
Host=127.0.0.1
Database={run / 'route-services.sqlite'}
RequireTLS=true
'''
        adjacent = {'a': ['b', 'old', 'leaf'], 'b': ['a', 'c'], 'c': ['b']}[name]
        for other in adjacent:
            outbound = (name, other) in [('b', 'a'), ('c', 'b')]
            port = nodes[other]['tls' if encrypted else 'plain'] if outbound else 0
            text += f'''[Server:{other}]
Servername={other}.test
Hostname=localhost
Password=route-password
Port={port}
TLS={'true' if outbound and encrypted else 'false'}
Hub={'false' if other == 'leaf' else 'true'}
Class=30
'''
        config.write_text(text)
        logfile = run / f'route-{name}-{len(processes)}.log'
        with logfile.open('w') as log:
            process = subprocess.Popen(native(source / 'ircd.js', '-f', config, '-a', '127.0.0.1'),
                                       stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
        node['process'] = process
        node['log'] = logfile
        processes.append((process, logfile))
        with viewer(name, f'ready{name}'):
            pass

    def viewer(name, nick, encrypted=False):
        end = time.monotonic() + 20
        while True:
            assert nodes[name]['process'].poll() is None, nodes[name]['log'].read_text()
            try:
                stream = socket.create_connection(('127.0.0.1', nodes[name]['tls' if encrypted else 'plain']), timeout=3)
                if encrypted:
                    context = ssl.create_default_context(cafile=run / 'root.pem')
                    stream = context.wrap_socket(stream, server_hostname='localhost')
                break
            except OSError:
                assert time.monotonic() < end, nodes[name]['log'].read_text()
                time.sleep(.1)
        stream.sendall(f'NICK {nick}\r\nUSER viewer 0 * :Route viewer\r\n'.encode())
        # Native TLS setup can occupy a callback for its 10-second timeout.
        receive_until(stream, b' 376 ', timeout=20)
        return stream

    def whois(stream, nick, secure, server=''):
        end = time.monotonic() + 20
        while True:
            stream.sendall(f'WHOIS {server + " " if server else ""}{nick}\r\n'.encode())
            data = receive_until(stream, b' 318 ', timeout=20)
            if b'TLS path:' in data and (b' 671 ' in data) == secure:
                return data
            assert time.monotonic() < end, repr(data)
            time.sleep(.2)

    def service(stream, target, command, expected):
        time.sleep(1.1)  # Local service command rate limit.
        stream.sendall(f'PRIVMSG {target} :{command}\r\n'.encode())
        return receive_until(stream, expected.encode(), timeout=20)

    def route_reply(stream, target):
        marker = f'TLSROUTE END {target}: '.encode()
        stream.sendall(f'TLSROUTE {target}\r\n'.encode())
        data = b''
        end = time.monotonic() + 8
        while time.monotonic() < end:
            if select.select([stream], [], [], .2)[0]:
                part = stream.recv(16384)
                assert part, 'Route viewer disconnected: ' + repr(data)
                data += part
                if any(marker in line or b' 402 ' in line for line in data.split(b'\r\n')[:-1]):
                    return data
        raise AssertionError('Route reply missing: ' + repr(data))

    def route(stream, target, summary, timeout=12):
        end = time.monotonic() + timeout
        marker = f'TLSROUTE END {target}: '.encode()
        while True:
            data = route_reply(stream, target)
            if marker + summary.encode() in data:
                return data
            assert time.monotonic() < end, repr(data)
            time.sleep(.3)

    def stop(name):
        process = nodes[name]['process']
        process.send_signal(signal.SIGTERM)
        assert process.wait(timeout=10) in (0, -signal.SIGTERM), nodes[name]['log'].read_text()

    def wait_gone(stream, target):
        end = time.monotonic() + 20
        while True:
            data = route_reply(stream, target)
            if b' 402 ' in data:
                return
            assert time.monotonic() < end, repr(data)
            time.sleep(.3)

    def fake(name, capabilities):
        context = ssl.create_default_context(cafile=run / 'root.pem')
        stream = context.wrap_socket(socket.create_connection(('127.0.0.1', nodes['a']['tls']), timeout=5), server_hostname='localhost')
        stream.sendall(f'PASS route-password :TS\r\nCAPAB TSJOIN {capabilities}\r\nSERVER {name}.test 1 :Test peer\r\n'.encode())
        data = receive_until(stream, b'BURST 0\r\n')
        if not capabilities:
            assert not any(line.startswith(b':') and b' LINKSEC ' in line for line in data.splitlines()), repr(data)
        return stream

    try:
        for name in nodes:
            start(name)
        with viewer('a', 'routeview') as client:
            data = route(client, 'c.test', 'all network hops TLS; certificate validation verified')
            assert b'VERIFIED by b.test' in data and b'VERIFIED by c.test' in data, repr(data)
            with viewer('c', 'reverseview') as reverse:
                route(reverse, 'a.test', 'all network hops TLS; certificate validation verified')
            print('PASS: three native TLS hubs report both directions and burst observations', flush=True)

            with viewer('a', 'secureviewer', True) as secure, viewer('c', 'securetarget', True) as target:
                whois(secure, 'securetarget', True)
                whois(secure, 'securetarget', True, 'c.test')
                whois(client, 'securetarget', False)
                whois(secure, 'routeview', False)
                whois(target, 'NickServ', True)
                service(client, 'NickServ', 'HELP', 'IDENTIFY')
                service(client, 'NickServ', 'IDENTIFY Some.User TESTPASS', 'requires TLS')
                service(target, 'NickServ@a.test', 'IDENTIFY Some.User TESTPASS', 'Identified as Some.User+A')
                service(target, 'NickServ', 'REGISTER', 'Nickname registered')
                service(client, 'NickServ', 'INFO securetarget', 'Registered network identity')
                service(target, 'MemoServ@a.test', 'STATUS', 'No telegrams')
                service(client, 'MemoServ', 'READ', 'requires TLS')
                service(target, 'NickServ', 'LOGOUT ALL', 'All BBS logins removed')
            print('PASS: WHOIS 671 and protected services work across native TLS hops; public commands remain available', flush=True)

            stop('c')
            # Wait for SQUIT before allowing reuse of the same server name.
            wait_gone(client, 'c.test')
            start('c', encrypted=False)
            route(client, 'c.test', 'plaintext present; certificate validation not applicable')
            with viewer('a', 'secureviewer', True) as secure, viewer('c', 'securetarget', True) as target:
                whois(secure, 'securetarget', False)
                service(target, 'NickServ@a.test', 'IDENTIFY Some.User TESTPASS', 'requires TLS')
            print('PASS: TLS clients cannot authenticate through a plaintext server hop', flush=True)
            stop('c')
            wait_gone(client, 'c.test')
            start('c')
            route(client, 'c.test', 'all network hops TLS; certificate validation verified')
            print('PASS: same-name native reconnect replaces TLS/plaintext/TLS observations', flush=True)

            with fake('old', '') as legacy:
                route(client, 'old.test', 'all network hops TLS; certificate validation unknown')
                legacy.sendall(f'NICK olduser 1 {int(time.time())} +i user localhost old.test 0 0 :Legacy client\r\n'.encode())
                with viewer('a', 'secureviewer', True) as secure:
                    assert b'is unknown' in whois(secure, 'olduser', False)
                legacy.sendall(b':olduser PRIVMSG NickServ :REGISTER\r\n')
                receive_until(legacy, b'requires TLS')
                legacy.sendall(b':old.test SERVER older.test 2 :Behind legacy hub\r\n')
                route(client, 'older.test', 'unknown; certificate validation unknown')
                with viewer('c', 'legacyview') as distant:
                    route(distant, 'older.test', 'unknown; certificate validation unknown')
            with fake('leaf', 'LINKSEC CLIENTTLS') as leaf:
                leaf.sendall(b':leaf.test LINKSEC a.test TLS OUT VERIFIED\r\n')
                route(client, 'leaf.test', 'all network hops TLS; certificate validation unknown')
                leaf.sendall(f'NICK leafuser 1 {int(time.time())} +i user localhost leaf.test 0 0 :Leaf client\r\n:leaf.test CLIENTTLS leafuser TLS\r\n'.encode())
                with viewer('a', 'secureviewer', True) as secure:
                    whois(secure, 'leafuser', True)
                with viewer('c', 'leafview') as distant:
                    route(distant, 'leaf.test', 'all network hops TLS; certificate validation unknown')
            print('PASS: legacy hub gaps and untrusted leaf certificate claims remain unknown', flush=True)
    finally:
        for process, logfile in processes:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                    raise AssertionError('Route IRCd did not shut down: ' + logfile.read_text())
            assert process.returncode in (0, -signal.SIGTERM), 'Route IRCd crashed: ' + logfile.read_text()


route_test()
print('All native TLS tests passed.', flush=True)
