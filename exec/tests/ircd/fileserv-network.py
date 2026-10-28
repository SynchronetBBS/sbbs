#!/usr/bin/env python3
"""Native IRC FileServ/DCC tests. Requires isolated services and file fixtures.
Workers run through a status-recording wrapper, so a post-response native crash
cannot masquerade as a passing protocol test. Never touches live BBS data.
"""
import argparse,ipaddress,json,os,re,resource,select,signal,socket,ssl,struct,subprocess,tempfile,threading,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--jsexec',required=True,type=Path)
a=p.parse_args()
source=Path(__file__).resolve().parents[2]
root=Path('/tmp/ircd-services-bbs-fixture')
run=Path(tempfile.mkdtemp(prefix='ircd-fileserv-network-'))
# A wrapper records native worker exit statuses, including cleanup failures.
wrapper=run/'worker'
wrapper.write_text('#!/usr/bin/env python3\nimport subprocess,sys,signal\np=subprocess.Popen('+repr([str(a.jsexec),'-m','67108864'])+'+sys.argv[1:]);signal.signal(signal.SIGTERM,lambda *a:p.terminate());r=p.wait()\nwith open('+repr(str(run/'worker-status'))+',"a") as f:f.write(str(r)+"\n")\nsys.exit(r)\n'.replace('str(r)+"\n"','str(r)+"\\n"'))
wrapper.chmod(0o700)
process=None;clients=[]
class LocalDNS:
    """Exercise dns.js against local A/AAAA replies without external DNS."""
    def __init__(self):
        self.sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.sock.bind(('127.0.0.1',0));self.sock.settimeout(.1)
        self.port=self.sock.getsockname()[1];self.stopped=False;self.error=None;self.pending=[]
        self.slow_seen=threading.Event();self.timeout_seen=threading.Event();self.release=threading.Event()
        self.thread=threading.Thread(target=self.serve,daemon=True);self.thread.start()
    def serve(self):
        try:
            while not self.stopped:
                if self.release.is_set():
                    for reply,peer in self.pending:self.sock.sendto(reply,peer)
                    self.pending=[]
                try:data,peer=self.sock.recvfrom(4096)
                except socket.timeout:continue
                offset=12;labels=[]
                while data[offset]:
                    n=data[offset];offset+=1;labels.append(data[offset:offset+n].decode('ascii'));offset+=n
                offset+=1;qtype,qclass=struct.unpack('!HH',data[offset:offset+4]);question=data[12:offset+4];host='.'.join(labels)
                if host=='timeout.files.test':
                    self.timeout_seen.set();continue
                exists=host in ('files.test','slow.files.test','v4-only.files.test')
                address='127.0.0.1' if qtype==1 else '::1' if qtype==28 and host!='v4-only.files.test' else None
                answer=b''
                if exists and address:
                    packed=socket.inet_pton(socket.AF_INET if qtype==1 else socket.AF_INET6,address)
                    answer=b'\xc0\x0c'+struct.pack('!HHIH',qtype,qclass,2,len(packed))+packed
                reply=data[:2]+struct.pack('!HHHHH',0x8180 if exists else 0x8183,1,bool(answer),0,0)+question+answer
                if host=='slow.files.test' and not self.release.is_set():
                    self.pending.append((reply,peer));self.slow_seen.set()
                else:self.sock.sendto(reply,peer)
        except Exception as error:
            if not self.stopped:self.error=error
    def close(self):
        self.stopped=True;self.thread.join(timeout=2);self.sock.close()
        assert not self.error,self.error
dns=LocalDNS()
def native(script,role=''):
    r=subprocess.run([str(a.jsexec),'-n','-c',str(root/role/'ctrl'),'-i',str(source/'load'),'-r',script],capture_output=True,text=True,timeout=20)
    assert r.returncode==0,r.stdout+r.stderr
    return r.stdout.strip()
def metrics():
    return json.loads(native('var u=new User(system.matchuser("Some User",false));print(JSON.stringify({bytes:u.stats.bytes_downloaded,files:u.stats.files_downloaded,credits:u.security.credits}));'))
def guest_metrics():
    return json.loads(native('var u=new User(system.matchuser("Public Reader",false));print(JSON.stringify({bytes:u.stats.bytes_downloaded,files:u.stats.files_downloaded}));'))
def port():
    s=socket.socket();s.bind(('127.0.0.1',0));n=s.getsockname()[1];s.close();return n
class Client:
    def __init__(self,nick,login=False,host='127.0.0.1'):
        self.nick=nick;self.buf=b'';self.host=host;self.sock=socket.create_connection((host,ircport),timeout=5);clients.append(self)
        if login:self.send('PASS TESTPASS')
        self.send('NICK '+nick);self.send('USER '+('Some.User' if login else 'visitor')+' 0 * :FileServ test');self.expect(' 001 ')
    def send(self,line):
        assert process.poll() is None,'IRCd died'
        self.sock.sendall((line+'\r\n').encode())
    def expect(self,pattern,timeout=15):
        end=time.monotonic()+timeout;seen=[]
        while time.monotonic()<end:
            assert process.poll() is None,'IRCd died: '+str(process.returncode)
            if b'\n' in self.buf:
                line,self.buf=self.buf.split(b'\n',1);line=line.rstrip(b'\r').decode('latin1');seen.append(line)
                if line.startswith('PING '):self.send('PONG '+line[5:])
                if pattern in line:return line
                continue
            if select.select([self.sock],[],[],.1)[0]:
                b=self.sock.recv(65536);assert b,'IRC socket closed';self.buf+=b
        raise AssertionError('Missing '+pattern+'; saw '+repr(seen))
    def cmd(self,command,expected):
        time.sleep(.55);self.send('PRIVMSG FileServ :'+command);return self.expect(expected)
def offer(c,command,kind='SEND'):
    line=c.cmd(command,'\x01DCC '+kind+' ')
    assert ' PRIVMSG ' in line,line
    m=re.search(r'\x01DCC (\S+) (\S+) (\S+) (\d+)(?: (\d+))?\x01',line);assert m,line
    c.offer_host=str(ipaddress.IPv6Address(m[3])) if ':' in m[3] else str(ipaddress.IPv4Address(int(m[3])))
    return int(m[4]),int(m[5] or 0)
class Browser:
    def __init__(self,sock):self.sock=sock;self.buf=b''
    def expect(self,marker):
        self.sock.settimeout(15)
        while True:
            while b'\n' in self.buf:
                line,self.buf=self.buf.split(b'\n',1);line=line.decode('latin1').rstrip('\r')
                if marker in line:return line
            data=self.sock.recv(8192);assert data,'DCC browser closed waiting for '+marker
            self.buf+=data
    def command(self,text,marker,listing=False):
        self.sock.sendall((text+'\r\n').encode('latin1'));line=self.expect(marker)
        if listing:self.expect('Page ')
        return line
def download(c,command,secure=False,resume=0,partial=None):
    n,size=offer(c,command,'SSEND' if secure else 'SEND')
    if resume:
        c.send('PRIVMSG FileServ :\x01DCC RESUME fixture.txt '+str(n)+' '+str(resume)+'\x01');c.expect('DCC ACCEPT fixture.txt '+str(n)+' '+str(resume))
    s=socket.create_connection((c.offer_host,n),timeout=10)
    if secure:s=ssl._create_unverified_context().wrap_socket(s,server_hostname='localhost')
    got=b''
    try:
        while len(got)+resume<size:
            data=s.recv(min(8192,(partial or size)-len(got)));assert data,'DCC ended early';got+=data
            s.sendall(struct.pack('!I',resume+len(got)))
            if partial and len(got)>=partial:break
    finally:s.close()
    c.expect('Download finished:' if not partial else 'interrupted',timeout=20)
    return got
try:
    bbs_qwk=native("print(system.qwk_id);").upper()
    ircport=port()
    config=run/'ircd.ini'
    config.write_text(f'''[Info]
Servername={bbs_qwk.lower()}.test
[Port:{ircport}]
Default=true
[Allow]
Mask=*@*
Class=1
[Class:1]
PingFrequency=120
Maximum=100
SendQ=2000000
[Operator:testop]
Nick=testop
Mask=*@*
Password=testpass
Flags=ORD
Class=1
[BBSServices]
NickServ=false
FileServ=true
[FileServ]
PublicDirs=fs_public
ExcludeDirs=fs_excluded
Channels=#files
Address=127.0.0.1
Bind=127.0.0.1
Address4=127.0.0.1
Bind4=127.0.0.1
Address6=::1
Bind6=::1
TLS=optional
Timeout=8
JSexec={wrapper}
''')
    log=(run/'ircd.log').open('w')
    # Replace the real asynchronous DNS sockets with the isolated DNS fixture
    # after IRCd initialization, before the native callback loop starts.
    runner=run/'ircd-runner.js'
    runner.write_text('load('+json.dumps(str(source/'ircd.js'))+');\n'+'''
DNS_Resolver.sockets.forEach(function(s) { s.clearOn("read",s.cbID);s.close(); });
var test_dns_socket=new Socket(SOCK_DGRAM);
if (!test_dns_socket.bind(0,"127.0.0.1") || !test_dns_socket.connect("127.0.0.1",'''+str(dns.port)+''')) throw new Error("DNS fixture connect failed");
test_dns_socket.dnsObject=DNS_Resolver;
test_dns_socket.cbID=test_dns_socket.on("read",function() {
    var response=this.dnsObject.handle_response(this);
    if (response) response.q.callback.call(response.q.thisObj,response.ret);
});
DNS_Resolver.sockets=[test_dns_socket];
''')
    def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    process=subprocess.Popen([str(a.jsexec),'-A','-L7','-m','67108864','-c',str(root/'ctrl'),'-i',str(source/'load'),str(runner),'-a','127.0.0.1,::1','-f',str(config)],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,preexec_fn=no_core)
    end=time.monotonic()+20
    while True:
        assert process.poll() is None,'IRCd startup failed: '+str(run)
        try:s=socket.create_connection(('127.0.0.1',ircport),timeout=.2);s.close();break
        except OSError:
            assert time.monotonic()<end,'Listener not ready';time.sleep(.1)
    public=Client('Visitor');owner=Client('Owner',True)
    assert ' NOTICE ' in public.cmd('HELP GET','DCC SEND')
    public.cmd('LIST fs_public','Files/public/fixture.txt')
    public.cmd('LIST fs_private','access denied')
    owner.cmd('LIST Files/private','Files/private/fixture.txt')
    public.cmd('LIBS','Files : FileServ fixture library')
    public.cmd('DIRS Files','Files/public : Fixture public')
    owner.cmd('LIST fs_excluded','access denied')
    owner.cmd('GET fs_restricted fixture.txt PLAIN','access denied')
    owner.cmd('GET fs_public ../fixture.txt PLAIN','without paths or control characters')
    owner.cmd('GET fs_public escape.txt PLAIN','unavailable')
    for _ in range(8):public.cmd('LIST fs_private','access denied')
    print('PASS public selection, local PASS authentication, native ARS, filters, traversal/symlink rejection and repeated terminal worker replies',flush=True)
    before=guest_metrics();payload=download(public,'GET Files/public fixture.txt PLAIN');after=guest_metrics()
    assert payload==(root/'files/public/fixture.txt').read_bytes()
    assert after['bytes']-before['bytes']==len(payload),(before,after)
    assert after['files']==before['files']+1
    before=metrics();got=download(owner,'GET fs_paid fixture.txt PLAIN');after=metrics()
    assert after['bytes']-before['bytes']==len(got) and before['credits']-after['credits']==100,(before,after)
    before=metrics();got=download(owner,'GET ex_exempt fixture.txt PLAIN');after=metrics()
    assert before['credits']==after['credits'] and after['bytes']-before['bytes']==len(got),(before,after)
    before=metrics();got=download(owner,'GET fs_public fixture.txt PLAIN',resume=12345);after=metrics()
    assert got==payload[12345:] and after['bytes']-before['bytes']==len(got),(before,after)
    print('PASS DCC bytes, native credits/library exemptions and resumed-byte accounting',flush=True)
    before=metrics();got=download(owner,'GET fs_public fixture.txt',secure=True);after=metrics()
    assert got==payload and after['bytes']-before['bytes']==len(got)
    n,_=offer(public,'CHAT','SCHAT');raw=socket.create_connection(('127.0.0.1',n),timeout=8)
    encrypted=ssl._create_unverified_context().wrap_socket(raw,server_hostname='localhost')
    assert b'virtual browser' in encrypted.recv(4096);encrypted.sendall(b'HELP GET\r\n');assert b'DCC SEND' in encrypted.recv(8192)
    encrypted.sendall(b'QUIT\r\n');encrypted.close()
    print('PASS default encrypted DCC SSEND and SCHAT',flush=True)
    ipv6=Client('IPv6',True,host='::1')
    assert download(ipv6,'GET fs_private fixture.txt PLAIN')==(root/'files/private/fixture.txt').read_bytes()
    assert ipv6.offer_host=='::1'
    assert download(ipv6,'GET fs_public fixture.txt',secure=True,resume=12345)==payload[12345:]
    n,_=offer(ipv6,'CHAT','SCHAT');assert ipv6.offer_host=='::1'
    encrypted=ssl._create_unverified_context().wrap_socket(socket.create_connection((ipv6.offer_host,n),timeout=8),server_hostname='localhost')
    assert b'virtual browser' in encrypted.recv(4096);encrypted.sendall(b'QUIT\r\n');encrypted.close()
    owner.cmd('ADDRESS ::1','Expected DCC peer address: ')
    assert download(owner,'GET fs_public fixture.txt PLAIN')==payload and owner.offer_host=='::1'
    owner.cmd('ADDRESS RESET','Expected DCC peer address: 127.0.0.1')
    print('PASS IPv6 protected download, TLS, resume, chat and IPv4 IRC session with IPv6 DCC override',flush=True)
    # Certificate availability is checked on accept, never on listener creation.
    cert=root/'ctrl/ssl.cert';backup=root/'ctrl/ssl.cert.fileserv-test'
    cert.rename(backup)
    try:
        n,_=offer(public,'CHAT','SCHAT');broken=socket.create_connection(('127.0.0.1',n),timeout=8)
        time.sleep(.2);owner.send('PING :blocked-handshake-responsive');owner.expect('blocked-handshake-responsive',timeout=2)
        try:
            broken=ssl._create_unverified_context().wrap_socket(broken,server_hostname='localhost')
            raise AssertionError('TLS succeeded without a certificate')
        except OSError:pass
        finally:broken.close()
        public.expect('FileServ',timeout=25)
        public.send('PING :tls-failure-responsive');public.expect('tls-failure-responsive')
    finally:backup.rename(cert)
    print('PASS missing certificate fails at accept without plaintext fallback or stalled IRC',flush=True)
    before=metrics();download(owner,'GET fs_public fixture.txt PLAIN',partial=16384);after=metrics()
    assert after['bytes']-before['bytes']==16384,(before,after)
    n,_=offer(public,'CHAT PLAIN','CHAT');chat=socket.create_connection(('127.0.0.1',n),timeout=5);browser=Browser(chat)
    browser.expect('virtual browser')
    browser.command('PWD','/')
    browser.command('DIR','Files : FileServ fixture library',listing=True)
    browser.command('CD Files','/Files')
    browser.command('DIR','public : Fixture public',listing=True)
    browser.command('LIST public','Files/public/fixture.txt',listing=True)
    browser.command('CD public','/Files/public')
    line=browser.command('DIR','fixture.txt',listing=True);assert 'fs_public' not in line and 'Files/' not in line,line
    browser.command('INFO fixture.txt','Files/public/fixture.txt')
    browser.command('CD ..','/Files')
    browser.command('DIR pub*','public : Fixture public',listing=True)
    browser.command('CD /Files/public','/Files/public')
    browser.command('CD /Exempt','access denied')
    browser.command('PWD','/Files/public')
    browser.command('CD fs_public','/Files/public')
    browser.command('CD /','/')
    browser.command('CD ../','access denied')
    chat.sendall(b'QUIT\r\n');chat.close()
    print('PASS partial-download accounting and hierarchical DCC CHAT library/directory browsing',flush=True)
    owner.send('JOIN #files');owner.expect(' JOIN :#files');time.sleep(.6)
    owner.send('PRIVMSG #files :!files '+bbs_qwk+' LIBS');owner.expect('Files : FileServ fixture library')
    for option,kind in [('plain','CHAT'),('TLS','SCHAT')]:
        time.sleep(.6);owner.send('PRIVMSG #files :!fserve '+bbs_qwk+' '+option)
        line=owner.expect('\x01DCC '+kind+' ')
        match=re.search(r'DCC '+kind+r' chat (\S+) (\d+)\x01',line);assert match,line
        browser=socket.create_connection(('127.0.0.1',int(match[2])),timeout=8)
        if kind=='SCHAT':browser=ssl._create_unverified_context().wrap_socket(browser,server_hostname='localhost')
        assert b'virtual browser' in browser.recv(4096)
        browser.sendall(b'QUIT\r\n');browser.close()
    print('PASS channel !fserve honors explicit PLAIN and TLS with native CHAT/SCHAT connections',flush=True)
    time.sleep(.6);owner.send('PRIVMSG #files :!files '+bbs_qwk+' ADDRESS 127.0.0.2');owner.expect('Use ADDRESS privately on IRC.')
    owner.cmd('ADDRESS','Expected DCC peer address: 127.0.0.1')
    n,_=offer(owner,'GET fs_public fixture.txt PLAIN')
    owner.cmd('GET fs_public fixture.txt PLAIN','Queued ')
    waiting=owner.cmd('QUEUE',' queued ');queued_id=waiting.split(' :',1)[1].split()[0]
    active=owner.expect(' send waiting');active_id=active.split(' :',1)[1].split()[0]
    assert active_id.startswith('q') and queued_id.startswith('q') and active_id!=queued_id
    owner.cmd('CANCEL '+active_id,'cancelled');owner.expect('DCC SEND fixture.txt ')
    owner.cmd('CANCEL '+queued_id,'cancelled')
    time.sleep(1)
    # Other IRC clients must remain responsive while a worker awaits its DCC peer.
    n,_=offer(owner,'CHAT PLAIN','CHAT');public.send('PING :still-responsive');public.expect('still-responsive')
    owner.cmd('CANCEL ALL','cancelled')
    owner.cmd('ADDRESS 127.0.0.2','Expected DCC peer address: 127.0.0.2')
    n,_=offer(owner,'GET fs_public fixture.txt PLAIN')
    wrongpeer=socket.create_connection(('127.0.0.1',n),timeout=5);wrongpeer.settimeout(5)
    assert wrongpeer.recv(1)==b'', 'DCC sent data to the wrong peer IP'
    wrongpeer.close();owner.cmd('CANCEL ALL','cancelled');owner.cmd('ADDRESS RESET','Expected DCC peer address: 127.0.0.1')
    owner.send('OPER testop testpass');owner.expect(' 381 ')
    def endpoints(address,bind='files.test'):
        text=config.read_text()
        text=re.sub(r'^(?:Address[46]?|Bind[46]?)=.*\n','',text,flags=re.MULTILINE)
        text=text.replace('[FileServ]\n',f'[FileServ]\nAddress={address}\nBind={bind}\n')
        config.write_text(text);owner.send('REHASH');owner.expect(' 382 ');time.sleep(1)
    endpoints('files.test')
    assert download(owner,'GET fs_public fixture.txt PLAIN')==payload and owner.offer_host=='127.0.0.1'
    assert download(ipv6,'GET fs_public fixture.txt',secure=True)==payload and ipv6.offer_host=='::1'
    print('PASS shared Address/Bind hostname resolves A and AAAA through native asynchronous DNS',flush=True)
    endpoints('v4-only.files.test')
    ipv6.cmd('CHAT PLAIN','No IPv6 FileServ endpoint is available')
    assert download(owner,'GET fs_public fixture.txt PLAIN')==payload
    print('PASS absent AAAA refuses IPv6 offer while IPv4 hostname downloads continue',flush=True)
    endpoints('timeout.files.test')
    ipv6.send('PRIVMSG FileServ :CHAT PLAIN')
    assert dns.timeout_seen.wait(timeout=5),'FileServ did not start timeout lookup'
    public.send('PING :dns-timeout-responsive');public.expect('dns-timeout-responsive',timeout=2)
    ipv6.expect('No IPv6 FileServ endpoint is available',timeout=15)
    print('PASS unanswered DNS lookup times out without blocking IRC',flush=True)
    endpoints('slow.files.test')
    owner.send('PRIVMSG FileServ :CHAT PLAIN')
    assert dns.slow_seen.wait(timeout=5),'FileServ did not start hostname lookup'
    public.send('PING :dns-pending-responsive');public.expect('dns-pending-responsive',timeout=2)
    owner.cmd('CANCEL ALL','cancelled');dns.release.set()
    time.sleep(.3);owner.cmd('QUEUE','Your queue is empty.')
    # Leave a hostname-selected listener pending during DIE.
    n,_=offer(owner,'CHAT PLAIN','CHAT')
    print('PASS delayed DNS preserves IRC responsiveness and cancellation prevents late offers',flush=True)
    owner.send('DIE')
    assert process.wait(timeout=15)==0
    time.sleep(2)
    assert not list((root/'data').glob('ircd-fileserv-*/*')), 'Worker files leaked on shutdown'
    statuses=(run/'worker-status').read_text().splitlines()
    assert statuses and all(s=='0' for s in statuses),statuses
    print('PASS channel commands, cancellation, IRC responsiveness, native worker and IRCd exit status',flush=True)
    print('FileServ network tests passed:',run)
finally:
    for c in clients:c.sock.close()
    if process and process.poll() is None:
        process.terminate()
        try:process.wait(timeout=15)
        except subprocess.TimeoutExpired:process.kill();process.wait()
    if process:log.close()
    dns.close()
