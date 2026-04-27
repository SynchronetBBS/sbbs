// SPDX-License-Identifier: GPL-2.0-or-later
//
// Wren-side handlers for keys that fire during an active connection.
// Currently:  Alt+L (0x2600) sends the BBS-list-stored login credentials
// (user, password, sysop password).  Replaces the C-side `case 0x2600`
// in term.c that called send_login(bbs).
//
// The send-order rules mirror the original C implementation:
//   - rlogin / ssh:        skip user + password (the protocol handles auth);
//                          still send syspass if set
//   - sshNoAuth (SSHNA):   skip user (negotiated); send password + syspass
//   - everything else:     send user, password, then syspass — each followed
//                          by Enter

// This script is loaded into the "syncterm" module alongside the
// foreign-class declarations, so Hook/Conn/CTerm/BBS/ConnType/Emulation
// are already in scope — no imports needed.

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
