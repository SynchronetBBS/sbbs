#!/usr/bin/env python3
"""Exercise real dns.js UDP lookups and IRC admission with a controlled DNS
server. Uses the private services-fixture.py BBS under /tmp, never live data.
"""
import argparse
from pathlib import Path
import queue
import re
import select
import signal
import socket
import struct
import subprocess
import tempfile
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsexec', required=True, type=Path)
parser.add_argument('--ctrl', type=Path, default=Path('/tmp/ircd-services-bbs-fixture/ctrl'))
args = parser.parse_args()
assert args.ctrl.resolve().is_relative_to(Path('/tmp'))
source = Path(__file__).resolve().parents[2]
run = Path(tempfile.mkdtemp(prefix='ircd-rbl-network-'))
print('RBL test files:', run, flush=True)
clients = []


class DNS:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', 0))
        self.sock.settimeout(.2)
        self.port = self.sock.getsockname()[1]
        self.hold = set()
        self.waiting = queue.Queue()
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def respond(self, request, result=None):
        packet, address, end, _, _ = request
        question = packet[12:end]
        flags = 0x8180 if result else 0x8183
        response = packet[:2] + struct.pack('!HHHHH', flags, 1, int(bool(result)), 0, 0) + question
        if result:
            response += b'\xc0\x0c' + struct.pack('!HHIH', 1, 1, 60, 4) + socket.inet_aton(result)
        self.sock.sendto(response, address)

    def serve(self):
        while not self.stop.is_set():
            try:
                packet, address = self.sock.recvfrom(4096)
            except socket.timeout:
                continue
            labels, pos = [], 12
            while packet[pos]:
                size = packet[pos]
                labels.append(packet[pos+1:pos+1+size].decode('ascii'))
                pos += size + 1
            pos += 1
            kind, cls = struct.unpack('!HH', packet[pos:pos+4])
            request = (packet, address, pos+4, '.'.join(labels), kind)
            assert cls == 1
            if request[3] in self.hold:
                self.waiting.put(request)
            else:
                self.respond(request)

    def query(self, name, timeout=5):
        request = self.waiting.get(timeout=timeout)
        assert request[3] == name and request[4] == 1, request[3:]
        return request


class Client:
    def __init__(self):
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=5)
        self.buffer = b''
        clients.append(self)

    def send(self, text):
        self.sock.sendall((text + '\r\n').encode())

    def read(self, timeout):
        end = time.monotonic() + timeout
        while True:
            if b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                return line.rstrip(b'\r').decode('latin1')
            remaining = end - time.monotonic()
            if remaining <= 0 or not select.select([self.sock], [], [], remaining)[0]:
                return None
            data = self.sock.recv(16384)
            if not data:
                return ''
            self.buffer += data

    def expect(self, match, timeout=5):
        seen, end = [], time.monotonic() + timeout
        while time.monotonic() < end:
            line = self.read(end - time.monotonic())
            if line is None or line == '':
                break
            seen.append(line)
            if match in line:
                return line
        raise AssertionError('Expected ' + match + '; saw ' + repr(seen))

    def register(self, nick):
        self.send('NICK ' + nick + '\r\nUSER user 0 * :RBL regression')

    def held(self):
        end = time.monotonic() + .3
        while time.monotonic() < end:
            line = self.read(end - time.monotonic())
            assert line != '', 'Pending connection closed'
            assert not line or (' 001 ' not in line and 'SERVER ' not in line), line


with socket.socket() as listener:
    listener.bind(('127.0.0.1', 0))
    port = listener.getsockname()[1]
dns = DNS()
config = run / 'ircd.ini'
config.write_text('''[Info]
Servername=rbl.test
Description=RBL regression
[Port:%d]
Default=true
[Class:1]
PingFrequency=90
ConnectFrequency=0
Maximum=100
SendQ=2000000
[Allow]
Mask=*@*
Class=1
[RBL:1]
Hostname=blocked.test
[WebIRC:1]
Hostname=127.0.0.1
Password=gateway
[Server:peer.test]
Servername=peer.test
Hostname=127.0.0.1
Password=linktest
Port=0
Hub=false
Class=1
''' % port)
# Give the normal dns.js resolver a socket on our unprivileged local DNS port.
# Override loopback exemption solely to exercise initial admission.
worker = run / 'worker.js'
worker.write_text('''load(%s);
DNS_Resolver.sockets.forEach(function(sock) {
    sock.clearOn("read", sock.cbID); sock.close();
});
var test_dns_socket = new Socket(SOCK_DGRAM);
test_dns_socket.array_buffer = false;
if (!test_dns_socket.bind(0, "127.0.0.1") || !test_dns_socket.connect("127.0.0.1", %d))
    throw new Error("DNS test socket failed");
test_dns_socket.dnsObject = DNS_Resolver;
test_dns_socket.cbID = test_dns_socket.on("read", function() {
    var response = this.dnsObject.handle_response(this);
    if (response) response.q.callback.call(response.q.thisObj, response.ret);
});
DNS_Resolver.sockets = [test_dns_socket];
dnsbl_exempt = function() { return false; };
''' % ('"' + str(source / 'ircd.js') + '"', dns.port))
logpath = run / 'ircd.log'
with logpath.open('w') as logfile:
    process = subprocess.Popen([str(args.jsexec), '-n', '-L7', '-c', str(args.ctrl),
                                '-i', str(source / 'load'), str(worker),
                                '-a', '127.0.0.1', '-f', str(config)],
                               stdin=subprocess.DEVNULL, stdout=logfile, stderr=subprocess.STDOUT)
try:
    end = time.monotonic() + 30
    while True:
        assert process.poll() is None, logpath.read_text()
        try:
            healthy = Client()
            healthy.register('Healthy')
            healthy.expect(' 376 ')
            break
        except ConnectionRefusedError:
            assert time.monotonic() < end, logpath.read_text()
            time.sleep(.1)

    initial = '1.0.0.127.blocked.test'
    dns.hold.add(initial)
    pending = Client()
    pending.register('Waiting')
    request = dns.query(initial)
    pending.held()
    healthy.send('PING :while-rbl-pending')
    # Allow the client's normal two-second command throttle, while staying
    # well below dns.js's 15-second timeout for the held query.
    healthy.expect('while-rbl-pending', timeout=3)
    dns.respond(request)
    pending.expect(' 001 ')
    print('PASS: initial admission waits for real DNS while another client remains responsive', flush=True)

    listed = Client()
    listed.register('Listed')
    request = dns.query(initial)
    listed.held()
    dns.respond(request, '127.0.0.2')
    listed.expect(' 463 ')
    listed.expect('ERROR :Your IP address is on an RBL')
    assert listed.read(3) == ''
    dns.hold.remove(initial)
    print('PASS: listed initial connection is denied before registration', flush=True)

    gateway = Client()
    gateway.expect('Performing RBL check')
    name = '9.100.51.198.blocked.test'
    dns.hold.add(name)
    gateway.send('WEBIRC gateway web client.example 198.51.100.9')
    gateway.register('WebWaiting')
    request = dns.query(name)
    gateway.held()
    healthy.send('PING :while-webirc-pending')
    healthy.expect('while-webirc-pending', timeout=3)
    dns.respond(request)
    gateway.expect(' 001 ')
    gateway.send('WHOIS WebWaiting')
    assert 'client.example' in gateway.expect(' 311 ')
    print('PASS: WEBIRC checks the claimed IP asynchronously before registration', flush=True)

    dns.hold.add(initial)
    peer = Client()
    peer.send('PASS linktest\r\nSERVER peer.test 1 :RBL handshake regression')
    request = dns.query(initial)
    peer.held()
    healthy.send('PING :while-server-pending')
    healthy.expect('while-server-pending', timeout=3)
    dns.respond(request)
    peer.expect('SERVER rbl.test 1')
    peer.sock.close()
    print('PASS: inbound server handshake also waits for RBL admission', flush=True)

    abandoned = Client()
    abandoned.register('Abandoned')
    request = dns.query(initial)
    abandoned.sock.close()
    time.sleep(.2)
    dns.respond(request, '127.0.0.2')
    healthy.send('PING :after-stale-reply')
    healthy.expect('after-stale-reply', timeout=3)
    print('PASS: late listing for a disconnected client leaves other connections intact', flush=True)

    silent = Client()
    silent.register('Timeout')
    dns.query(initial)
    silent.held()
    silent.expect(' 001 ', timeout=20)
    dns.query(initial)
    dns.query(initial)
    healthy.send('PING :after-dns-timeout')
    healthy.expect('after-dns-timeout', timeout=3)
    print('PASS: unanswered DNS retries and times out without blocking the event loop', flush=True)
    assert process.poll() is None, logpath.read_text()
finally:
    for client in clients:
        client.sock.close()
    was_running = process.poll() is None
    if was_running:
        process.send_signal(signal.SIGTERM)
    try:
        status = process.wait(timeout=15)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
        raise AssertionError('IRCd did not stop')
    dns.stop.set()
    dns.thread.join(timeout=2)
    dns.sock.close()
    print('IRCd exit:', status, 'log:', logpath, flush=True)
    assert was_running and status in (0, -signal.SIGTERM), logpath.read_text()
    assert not re.search(r'TypeError|ReferenceError|SyntaxError|Segmentation', logpath.read_text()), logpath.read_text()
