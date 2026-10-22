#!/usr/bin/env python3
"""Prepare private, isolated Synchronet config/user data for services tests."""
import argparse
from pathlib import Path
import os
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--template-ctrl', required=True, type=Path)
parser.add_argument('--jsexec', required=True, type=Path)
parser.add_argument('--exec-dir', default=Path(__file__).resolve().parents[2], type=Path)
args = parser.parse_args()
root = Path('/tmp/ircd-services-bbs-fixture')
root.mkdir(mode=0o700, exist_ok=True)
os.chmod(root, 0o700)
for role in ['', 'hub', 'home', 'edge']:
    dest = root / role
    for directory in ['ctrl', 'data/user', 'data/msgs', 'node1', 'text', 'mods']:
        (dest / directory).mkdir(parents=True, exist_ok=True)
    for filename in ['main.ini', 'msgs.ini', 'file.ini', 'xtrn.ini', 'chat.ini', 'attr.cfg', 'text.dat']:
        shutil.copyfile(args.template_ctrl / filename, dest / 'ctrl' / filename)
    shutil.copyfile(args.template_ctrl.parent / "node1/node.ini", dest / "node1/node.ini")
    main = dest / 'ctrl/main.ini'
    text = main.read_text(encoding='latin1')
    if role:
        import re
        text = re.sub(r'(?m)^qwk_id=.*$', 'qwk_id=' + role.upper(), text)
    first, last = text.index('[node_dir]'), text.index('[daily_event]')
    text = text[:first] + '[node_dir]\n1=' + str(dest / 'node1') + '/\n\n[dir]\n' + ''.join(
        name + '=' + str(path) + '/\n' for name, path in [
            ('data', dest / 'data'), ('exec', args.exec_dir), ('mods', dest / 'mods'),
            ('logs', dest / 'data'), ('text', dest / 'text')]) + '\n' + text[last:]
    main.write_text(text, encoding='latin1')
    script = '''
if (system.data_dir.indexOf("/tmp/ircd-services-bbs-fixture/") !== 0) throw new Error("Unsafe fixture directory");
load("sbbsdefs.js");
var number = system.matchuser("Some User", false);
var u = number ? new User(number) : system.new_user("Some User");
if (!u || !u.number) throw new Error("Could not create fixture account");
u.settings &= ~(USER_DELETED | USER_INACTIVE);
u.security.restrictions = 0;
u.security.password = "TESTPASS";
'''
    subprocess.run([str(args.jsexec), '-n', '-c', str(dest / 'ctrl'), '-i', str(args.exec_dir / 'load'), '-r', script], check=True)
print('Prepared isolated BBS services fixtures:', root)
