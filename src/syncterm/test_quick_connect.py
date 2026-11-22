"""Headless quick-connect lifecycle regressions, using isolated XDG roots.

SYNCTERM selects the executable (default: build/syncterm).
SYNCTERM_SSH_TEST_SERVER optionally selects DeuceSSH's example server for
an actual host-key handshake without contacting an external host.
"""
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parent
BINARY = Path(os.environ.get("SYNCTERM", ROOT / "build/syncterm")).resolve()


class QuickConnectTest(unittest.TestCase):
    def run_sessions(self, urls, save=True, prompt=True, fingerprint=False,
                     existing=False):
        with tempfile.TemporaryDirectory(prefix="quick-connect-", dir="/tmp") as tmp:
            root = Path(tmp)
            scripts = root / "data/syncterm/scripts"
            (scripts / "auto/menu").mkdir(parents=True)
            (scripts / "auto/connected").mkdir(parents=True)
            config = root / "config/syncterm"
            config.mkdir(parents=True)
            (config / "syncterm.ini").write_text(
                "[SyncTERM]\nAudioModes=0\nPromptSave=" + str(prompt).lower() + "\n")
            # Supply a real file: the original bug only writes to an existing list.
            (config / "syncterm.lst").write_text("; isolated quick-connect test\n")
            if existing:
                endpoint = urls[0].rsplit("@", 1)[1]
                address, port = endpoint.rsplit(":", 1)
                (config / "syncterm.lst").write_text(
                    f"[Saved 1]\nAddress={address}\nPort={port}\n"
                    "ConnectionType=SSH\nUserName=Guest\n")
            quoted = ", ".join('"' + url + '"' for url in urls)
            expected_offers = len(urls) if prompt and not existing else 0
            expected_entries = 1 if existing else (len(urls) if prompt and save else 0)
            main = '''
import "syncterm_menu" for Menu, MenuReadStatus
class MainMenu {
  static prepare() { Menu.load(null) == MenuReadStatus.ok }
  static run(current, connected) {
    if (!prepare()) Fiber.abort("directory reload failed")
    if (__index == null) {
      __index = 0
      __offers = 0
      __addresses = []
      __ports = []
    }
    var urls = [URLS]
    if (__index < urls.count) {
      var b = EXISTING ? Menu.entries[0] : Menu.quickConnect(urls[__index])
      __index = __index + 1
      __address = b.addr
      __port = b.port
      __addresses.add(b.addr)
      __ports.add(b.port)
      return b
    }
    if (__offers != OFFERS) Fiber.abort("wrong number of save offers: %(__offers)")
    if (Menu.entries.count != ENTRIES) Fiber.abort("unexpected directory entries")
    for (i in 0...Menu.entries.count) {
      var b = Menu.entries[i]
      if (b.addr != __addresses[i] || b.port != __ports[i]) {
        Fiber.abort("reload lost the endpoint")
      }
      if (CHECK_FP && b.sshFingerprint.bytes.count != 32) Fiber.abort("reload lost the host key")
    }
    System.print("QUICK CONNECT PASS")
    Menu.quitApplication()
    return null
  }
  static offerSave(source) {
    __offers = __offers + 1
    if (source.addr != __address || source.port != __port) {
      Fiber.abort("save offer lost the endpoint")
    }
    if (Menu.entries.count != __offers - 1) Fiber.abort("phantom directory entry")
    if (CHECK_FP && source.sshFingerprint.bytes.count != 32) Fiber.abort("missing host key")
    if (!SAVE) return false
    var b = Menu.copy(source, "Saved %(__offers)")
    if (b == null || b.addr != __address || b.port != __port) {
      Fiber.abort("copy lost the endpoint")
    }
    if (!b.save()) Fiber.abort("save failed")
    return true
  }
}
'''
            main = main.replace("URLS", quoted).replace("OFFERS", str(expected_offers))
            main = main.replace("ENTRIES", str(expected_entries))
            main = main.replace("CHECK_FP", str(fingerprint).lower())
            main = main.replace("SAVE", str(save).lower())
            main = main.replace("EXISTING", str(existing).lower())
            (scripts / "auto/menu/main_menu.wren").write_text(main)
            # Auth may fail or succeed against the example server; both paths
            # have already completed host-key verification. Neither opens UI.
            (scripts / "auto/menu/menu_host_ui.wren").write_text('''
class MenuHostUI {
  static alert(title, message) {}
  static alertWithHelp(title, message, help) {}
  static help(title, body) {}
  static confirm(title, message) { false }
  static prompt(title, message, initial, maxLen, masked) { null }
  static choice(title, message, options, current) { 0 }
  static status(title, lines) {}
  static statusClear() {}
}
''')
            (scripts / "auto/connected/disconnect.wren").write_text(
                'import "syncterm" for Conn\nConn.endSession(false)\n')
            env = dict(os.environ, XDG_CONFIG_HOME=str(root / "config"),
                       XDG_DATA_HOME=str(root / "data"),
                       XDG_CACHE_HOME=str(root / "cache"),
                       SDL_VIDEODRIVER="offscreen", SDL_RENDER_DRIVER="software",
                       SDL_VIDEO_EGL_DRIVER="none")
            result = subprocess.run([str(BINARY), "-iS"], env=env,
                                    capture_output=True, text=True, timeout=30)
            output = result.stdout + result.stderr
            self.assertIn("QUICK CONNECT PASS", output, output)
            self.assertNotIn("[wren-menu]", output, output)
            if expected_entries:
                saved = (config / "syncterm.lst").read_text()
                for address in (url.split(":", 1)[1] for url in urls if url.startswith("shell:")):
                    self.assertIn(address, saved)
                if fingerprint:
                    self.assertIn("SSHFingerprint", saved)

    def test_each_disconnect_offers_save(self):
        self.run_sessions(["shell:/bin/cat", "shell:/bin/sh"])

    def test_declining_does_not_save(self):
        self.run_sessions(["shell:/bin/cat"], save=False)

    def test_prompt_disabled(self):
        self.run_sessions(["shell:/bin/cat"], prompt=False)

    def test_ssh_fingerprint_does_not_create_entry(self):
        server_binary = os.environ.get("SYNCTERM_SSH_TEST_SERVER")
        if not server_binary:
            self.skipTest("set SYNCTERM_SSH_TEST_SERVER to test an SSH handshake")
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        with tempfile.TemporaryFile() as log:
            server = subprocess.Popen([server_binary, str(port)], stdout=log,
                                      stderr=log, start_new_session=True)
            try:
                for _ in range(100):
                    if server.poll() is not None:
                        self.fail("SSH fixture exited before listening")
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                            break
                    except OSError:
                        time.sleep(0.02)
                else:
                    self.fail("SSH fixture did not start")
                url = f"ssh://Guest@127.0.0.1:{port}"
                self.run_sessions([url], prompt=False, fingerprint=True)
                self.run_sessions([url], save=False, fingerprint=True)
                self.run_sessions([url], fingerprint=True)
                self.run_sessions([url], fingerprint=True, existing=True)
            finally:
                os.killpg(server.pid, signal.SIGTERM)
                server.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
