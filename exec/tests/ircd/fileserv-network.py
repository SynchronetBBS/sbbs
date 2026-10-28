#!/usr/bin/env python3
"""Native IRC FileServ/DCC tests. Requires isolated services and file fixtures.
Workers run through a status-recording wrapper, so a post-response native crash
cannot masquerade as a passing protocol test. Never touches live BBS data.
"""
import argparse,json,os,re,resource,select,signal,socket,ssl,struct,subprocess,tempfile,time
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
    def __init__(self,nick,login=False):
        self.nick=nick;self.buf=b'';self.sock=socket.create_connection(('127.0.0.1',ircport),timeout=5);clients.append(self)
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
    return int(m[4]),int(m[5] or 0)
def download(c,command,secure=False,resume=0,partial=None):
    n,size=offer(c,command,'SSEND' if secure else 'SEND')
    if resume:
        c.send('PRIVMSG FileServ :\x01DCC RESUME fixture.txt '+str(n)+' '+str(resume)+'\x01');c.expect('DCC ACCEPT fixture.txt '+str(n)+' '+str(resume))
    s=socket.create_connection(('127.0.0.1',n),timeout=10)
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
TLS=optional
Timeout=8
JSexec={wrapper}
''')
    log=(run/'ircd.log').open('w')
    def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    process=subprocess.Popen([str(a.jsexec),'-A','-L7','-m','67108864','-c',str(root/'ctrl'),'-i',str(source/'load'),str(source/'ircd.js'),'-a','127.0.0.1','-f',str(config)],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,preexec_fn=no_core)
    end=time.monotonic()+20
    while True:
        assert process.poll() is None,'IRCd startup failed: '+str(run)
        try:s=socket.create_connection(('127.0.0.1',ircport),timeout=.2);s.close();break
        except OSError:
            assert time.monotonic()<end,'Listener not ready';time.sleep(.1)
    public=Client('Visitor');owner=Client('Owner',True)
    assert ' NOTICE ' in public.cmd('HELP GET','DCC SEND')
    public.cmd('LIST fs_public','fs_public/fixture.txt')
    public.cmd('LIST fs_private','access denied')
    owner.cmd('LIST fs_private','fs_private/fixture.txt')
    owner.cmd('LIST fs_excluded','access denied')
    owner.cmd('GET fs_restricted fixture.txt PLAIN','access denied')
    owner.cmd('GET fs_public ../fixture.txt PLAIN','without paths or control characters')
    owner.cmd('GET fs_public escape.txt PLAIN','unavailable')
    for _ in range(8):public.cmd('LIST fs_private','access denied')
    print('PASS public selection, local PASS authentication, native ARS, filters, traversal/symlink rejection and repeated terminal worker replies',flush=True)
    before=guest_metrics();payload=download(public,'GET fs_public fixture.txt PLAIN');after=guest_metrics()
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
    n,_=offer(public,'CHAT PLAIN','CHAT');chat=socket.create_connection(('127.0.0.1',n),timeout=5);chat.settimeout(10)
    assert b'virtual browser' in chat.recv(4096)
    chat.sendall(b'CD fs_public\r\n');assert b'fs_public' in chat.recv(4096)
    chat.sendall(b'DIR\r\n');assert b'fixture.txt' in chat.recv(8192)
    chat.sendall(b'CD ../\r\n');assert b'access denied' in chat.recv(4096)
    chat.sendall(b'QUIT\r\n');chat.close()
    print('PASS partial-download accounting and DCC CHAT virtual browser',flush=True)
    owner.send('JOIN #files');owner.expect(' JOIN :#files');time.sleep(.6)
    owner.send('PRIVMSG #files :!files '+bbs_qwk+' LIBS');owner.expect('fs_ : FileServ fixture library')
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
    # Leave a listener pending during DIE; shutdown must close it and its child.
    n,_=offer(owner,'CHAT PLAIN','CHAT')
    owner.send('OPER testop testpass');owner.expect(' 381 ');owner.send('DIE')
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
