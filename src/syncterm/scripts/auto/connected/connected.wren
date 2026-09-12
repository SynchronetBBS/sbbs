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

import "syncterm" for Hook, Key, Conn, BBS, CTerm, ConnType

class Connected {
  static sendComponent_(value) {
    if (value.count == 0) return
    Conn.send(value)
    CTerm.sendKey(Key.enter)
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

    if (sendUser) sendComponent_(BBS.user)
    if (sendPass) sendComponent_(BBS.password)
    sendComponent_(BBS.syspass)
  }
}

Hook.onKey(0x2600) { |k|  // Alt+L
  Connected.sendLogin()
  return true
}
