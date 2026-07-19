// Wren-side handlers for keys that fire during an active connection.
// Alt+L (0x2600) sends the BBS-list-stored login credentials (user,
// password, and sysop password).
//
// Send-order rules:
//   - rlogin / ssh:        skip user + password (the protocol handles auth);
//                          still send syspass if set
//   - sshNoAuth (SSHNA):   skip user (negotiated); send password + syspass
//   - everything else:     send user, password, then syspass — each followed
//                          by Enter
//
// Loaded into its own "connected" module; pulls bindings from the
// foundational "syncterm" module via import.

import "syncterm" for Hook, Conn, BBS, CTerm, ConnType, Emulation

class Connected {
  // Enter byte differs in ATASCII (CR is 0x9B there, not 0x0D).
  static enter_ {
    return CTerm.emulation == Emulation.atascii ? "\x9b" : "\r"
  }

  static sendLogin() {
    var ct      = BBS.connType
    var sendUser = ct != ConnType.rlogin &&
                   ct != ConnType.rloginReversed &&
                   ct != ConnType.ssh &&
                   ct != ConnType.sshNoAuth
    var sendPass = ct != ConnType.rlogin &&
                   ct != ConnType.rloginReversed &&
                   ct != ConnType.ssh

    var e = enter_

    if (sendUser && BBS.user.count > 0)     Conn.send(BBS.user + e)
    if (sendPass && BBS.password.count > 0) Conn.send(BBS.password + e)
    if (BBS.syspass.count > 0)              Conn.send(BBS.syspass + e)
  }
}

Hook.onKey(0x2600) { |k|  // Alt+L
  Connected.sendLogin()
  return true
}
