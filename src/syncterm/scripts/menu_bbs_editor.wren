import "syncterm" for Key, KeyEvent, Screen
import "syncterm_menu" for Menu
import "menu_ui" for MenuUi
import "ui_app" for App
import "ui_widget" for Rect
import "ui_pane" for Pane
import "ui_list" for ListView
import "ui_popup" for Alert, Confirm

class EditorPane is Pane {
  construct new() { super() }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      triggerClose_()
      return true
    }
    return super.handle(event)
  }
}

class BbsEditor {
  static edit(app, bbs, isDefaults, isNew) {
    var editor = build_(app, bbs, isDefaults, false)
    app.modal(editor[0])
    return finish_(editor[1], bbs, isNew)
  }

  static editStandalone(bbs, isDefaults, isNew) {
    var app = App.new()
    var editor = build_(app, bbs, isDefaults, true)
    app.pushModal(editor[0])
    app.runSync()
    return finish_(editor[1], bbs, isNew)
  }

  static finish_(state, bbs, isNew) {
    if (!state["saved"] && isNew) bbs.delete()
    return state["saved"]
  }

  static build_(app, bbs, isDefaults, standalone) {
    var draft = draft_(bbs)
    var state = {"saved": false}
    var pane = EditorPane.new()
    pane.title = "Edit Directory Entry"
    if (isDefaults) pane.title = "Default Connection Settings"
    pane.helpText = editorHelp_(draft, isDefaults)
    pane.focused = true
    var size = Screen.size
    pane.bounds = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    var dismiss = Fn.new {
      app.popModal()
      if (standalone) app.quit()
    }
    pane.onClose = Fn.new {
      if (apply_(app, bbs, draft, isDefaults)) {
        state["saved"] = true
        dismiss.call()
      }
    }

    var list = ListView.new()
    list.bounds = pane.innerBounds
    pane.add(list)

    var rebuild = null
    rebuild = Fn.new {
      var selected = list.selected
      if (selected == null) selected = 0
      var rows = rows_(app, draft, isDefaults)
      pane.helpText = editorHelp_(draft, isDefaults)
      list.items = rows.map {|row| row[0] }.toList
      list.selected = selected.min(rows.count - 1).max(0)
      list.onSelect = Fn.new {|i, item|
        rows[i][1].call()
        rebuild.call()
      }
    }
    rebuild.call()
    return [pane, state]
  }

  static draft_(b) {
    return {
      "name": b.name,
      "addr": b.addr,
      "port": b.port,
      "connType": b.connType,
      "user": b.user,
      "password": b.password,
      "syspass": b.syspass,
      "addressFamily": b.addressFamily,
      "screenMode": b.screenMode,
      "termName": b.termName,
      "font": b.font,
      "music": b.music,
      "rip": b.rip,
      "comment": b.comment,
      "bpsRate": b.bpsRate,
      "noStatus": b.noStatus,
      "hidePopups": b.hidePopups,
      "yellowIsYellow": b.yellowIsYellow,
      "forceLcf": b.forceLcf,
      "lfExpand": b.lfExpand,
      "dlDir": b.dlDir,
      "ulDir": b.ulDir,
      "logFile": b.logFile,
      "appendLogFile": b.appendLogFile,
      "xferLogLevel": b.xferLogLevel,
      "telnetLogLevel": b.telnetLogLevel,
      "stopBits": b.stopBits,
      "dataBits": b.dataBits,
      "parity": b.parity,
      "flowControl": b.flowControl,
      "telnetNoBinary": b.telnetNoBinary,
      "deferTelnetNegotiation": b.deferTelnetNegotiation,
      "ghostProgram": b.ghostProgram,
      "sftpPublicKey": b.sftpPublicKey,
      "sshAllowAes128Cbc": b.sshAllowAes128Cbc,
      "sshAcceptEarlyData": b.sshAcceptEarlyData,
      "palette": b.palette,
      "sortOrder": b.sortOrder
    }
  }

  static line_(label, value) {
    var text = label
    while (text.count < 20) text = text + " "
    return text + value.toString
  }

  static yesNo_(value) { value ? "Yes" : "No" }

  static row_(label, value, action) {
    return [line_(label, value), action]
  }

  static textRow_(app, d, key, label, maxLen, masked) {
    return row_(label, masked && d[key].count > 0 ? "********" : d[key],
        Fn.new {
      var value = MenuUi.prompt(app, label, label, d[key], maxLen, masked,
          fieldHelp_(key, d))
      if (value != null) d[key] = value
    })
  }

  static boolRow_(d, key, label) {
    return row_(label, yesNo_(d[key]), Fn.new { d[key] = !d[key] })
  }

  static choiceRow_(app, d, key, label, choices) {
    return row_(label, MenuUi.rowName(choices, d[key]), Fn.new {
      var value = MenuUi.choice(app, label, choices, d[key],
          fieldHelp_(key, d))
      if (value != null) d[key] = value
    })
  }

  static network_(type) {
    return type == 1 || type == 2 || type == 3 || type == 4 ||
        type == 5 || type == 6 || type == 11 || type == 12 || type == 13
  }

  static serial_(type) { type == 7 || type == 8 || type == 9 }
  static ssh_(type) { type == 5 || type == 6 }
  static telnet_(type) { type == 3 || type == 12 }

  static ansi_(mode) {
    return !(mode == 17 || mode == 18 || mode == 19 || mode == 20 ||
        mode == 21 || mode == 25 || mode == 26 || mode == 27 ||
        mode == 28 || mode == 29)
  }

  static helpItem_(term, description) {
    return "%(term)\n:  %(description)\n"
  }

  static editorHelp_(d, defaults) {
    var heading = "# Edit Default Connection\n\n"
    if (!defaults) heading = "# Edit Directory Entry\n\n"
    var text = heading + "Select an item to edit. Changes are stored " +
        "when you leave the editor.\n\n"
    if (!defaults) {
      text = text + helpItem_("Name", "The name shown in the directory")
      var address = "The network address to connect to"
      if (d["connType"] == 7) address = "The phone number to dial"
      if (d["connType"] == 8 || d["connType"] == 9) {
        address = "The communications-port device to open"
      }
      if (d["connType"] == 10) address = "The local command to run"
      text = text + helpItem_("Address", address)
    }
    text = text + helpItem_("Connection Type", "The connection protocol")
    if (network_(d["connType"])) {
      text = text + helpItem_("Address Family", "Use DNS, IPv4, or IPv6")
    }
    if (serial_(d["connType"])) {
      text = text + helpItem_("Flow Control", "The flow control to use") +
          helpItem_("Comm Rate", "The serial-port speed") +
          helpItem_("Stop Bits", "The number of stop bits per byte") +
          helpItem_("Data Bits", "The number of data bits per byte") +
          helpItem_("Parity", "The parity applied to each byte")
    } else if (d["connType"] != 10) {
      text = text + helpItem_("TCP Port", "The remote TCP port")
    }
    if (d["connType"] == 6) {
      text = text +
          helpItem_("SSH Username", "The username used by SSH") +
          helpItem_("BBS Username", "The username sent by auto-login") +
          helpItem_("BBS Password",
          "The password sent by auto-login; protected only by list encryption")
    } else if (d["connType"] == 11) {
      text = text +
          helpItem_("Username", "The username sent by GHost") +
          helpItem_("GHost Program", "The requested GHost program") +
          helpItem_("System Password",
          "The password sent by auto-login; protected only by list encryption")
    } else {
      text = text +
          helpItem_("Username", "The username sent by auto-login") +
          helpItem_("Password",
          "The password sent by auto-login; protected only by list encryption") +
          helpItem_("System Password",
          "The third auto-login response; protected only by list encryption")
    }
    if (ssh_(d["connType"])) {
      text = text + helpItem_("SFTP Public Key",
          "Open SFTP and transfer the public key to `.ssh/authorized_keys`") +
          helpItem_("Allow AES128-CBC",
          "Offer legacy `aes128-cbc` for old servers while preferring `aes256-ctr`") +
          helpItem_("Accept Early Data",
          "Accept data before PTY and shell requests finish; a workaround for " +
          "servers that otherwise drop their welcome banner")
    }
    if (telnet_(d["connType"])) {
      text = text + helpItem_("Binmode Broken",
          "Do not enable Telnet binary mode") +
          helpItem_("Defer Negotiate",
          "Wait for a remote Telnet command before negotiating, for systems " +
          "whose initial mailer ignores or rejects Telnet commands")
    }
    text = text + helpItem_("Screen Mode", "The display mode to use")
    if (ssh_(d["connType"]) || telnet_(d["connType"]) ||
        d["connType"] == 1 || d["connType"] == 2 ||
        d["connType"] == 10) {
      text = text + helpItem_("Terminal Type",
          "The terminal type advertised to the remote")
    }
    text = text + helpItem_("LF Expand",
        "Treat each LF as though CR and LF were received") +
        helpItem_("Font", "The font used for this entry")
    if (ansi_(d["screenMode"])) {
      text = text + helpItem_("ANSI Music", "The ANSI music dialect") +
          helpItem_("RIP", "Enable or disable a RIP mode") +
          helpItem_("Force LCF Mode",
          "Force VT-style last-column-flag behavior") +
          helpItem_("Yellow is Yellow",
          "Use yellow instead of IBM CGA brown")
    } else if (d["screenMode"] == 19) {
      text = text + helpItem_("Yellow is Yellow",
          "Use yellow instead of IBM CGA brown")
    }
    text = text + helpItem_("Hide Status Line",
        "Use the status row as an additional display row") +
        helpItem_("Download Path", "The default destination for downloads") +
        helpItem_("Upload Path", "The default directory for uploads") +
        helpItem_("Log File", "The optional session log filename") +
        helpItem_("Append Log File", "Append instead of replacing the log") +
        helpItem_("Transfer Log Level", "File-transfer log verbosity") +
        helpItem_("Telnet Log Level", "Telnet-command log verbosity")
    if (!serial_(d["connType"])) {
      text = text + helpItem_("Fake Comm Rate",
          "Throttle displayed characters to the selected rate")
    }
    text = text + helpItem_("Hide Popups",
        "Hide connection and disconnection popups") +
        helpItem_("Palette", "The color palette for this entry")
    if (!defaults) {
      text = text + helpItem_("Explicit Sort Order",
          "A manual numeric override for directory sorting") +
          helpItem_("Comment", "Text shown below the directory")
    }
    return text
  }

  static fieldHelp_(key, d) {
    if (key == "name") return nameHelp_()
    if (key == "addr") return addressHelp_()
    if (key == "addressFamily") return addressFamilyHelp_()
    if (key == "flowControl") return flowControlHelp_()
    if (key == "bpsRate") return commRateHelp_()
    if (key == "parity") return parityHelp_()
    if (key == "user") {
      if (d["connType"] == 6) {
        return "# SSH Username\n\nEnter the username used for " +
            "passwordless SSH authentication."
      }
      return "# Username\n\nEnter the username used for auto-login. " +
          "For SSH, this must be the SSH username."
    }
    if (key == "password") {
      if (d["connType"] == 11) {
        return "# GHost Program\n\nEnter the program name to request."
      }
      if (d["connType"] == 6) {
        return "# BBS Username\n\nEnter the username sent by auto-login."
      }
      return "# Password\n\nEnter the password used for auto-login. " +
          "For SSH, this must be the SSH password when one is required. " +
          "It is protected at rest only when list encryption is enabled."
    }
    if (key == "syspass") {
      if (d["connType"] == 6) {
        return "# BBS Password\n\nEnter the password sent by auto-login. " +
            "It is protected at rest only when list encryption is enabled."
      }
      return "# System Password\n\nThis value is sent after the username " +
          "and password during auto-login. It can also serve as a third " +
          "response for non-Synchronet systems. It is protected at rest " +
          "only when list encryption is enabled."
    }
    if (key == "termName") return terminalTypeHelp_()
    if (key == "font") return fontHelp_()
    if (key == "music") return ansiMusicHelp_()
    if (key == "dlDir") return "# Download Path\n\nEnter the directory where downloads will be placed."
    if (key == "ulDir") return "# Upload Path\n\nEnter the directory initially browsed for uploads."
    if (key == "logFile") return logFileHelp_()
    if (key == "xferLogLevel") return logLevelHelp_("File Transfer")
    if (key == "telnetLogLevel") return logLevelHelp_("Telnet Command")
    if (key == "comment") return "# Comment\n\nEnter the text shown below this directory entry."
    return null
  }

  static nameHelp_() {
    return "# Directory Entry Name\n\n" +
        "Enter the unique name shown in the directory."
  }

  static addressHelp_() {
    return "# Address, Phone Number, Device, or Command\n\n" +
        "Enter the hostname, IP address, phone number, or serial device " +
        "for the system. For example: `nix.synchro.net`.\n\n" +
        "For a Shell connection, enter the local command to run. Shell " +
        "connections are available on Unix-like systems."
  }

  static connectionTypeHelp_() {
    return "# Connection Type\n\n" +
        "RLogin\n:  Auto-login using the RLogin protocol\n" +
        "RLogin Reversed\n:  RLogin with reversed username and password parameters\n" +
        "Telnet\n:  Connect using Telnet\n" +
        "Raw\n:  Open an unprocessed TCP connection\n" +
        "SSH\n:  Connect using Secure Shell\n" +
        "SSH (no auth)\n:  SSH without sending a password or public key\n" +
        "Modem\n:  Dial using a modem\n" +
        "Serial / 3-wire\n:  Open a serial communications port\n" +
        "Shell\n:  Run a local command in a PTY\n" +
        "MBBS GHost\n:  Use the Major BBS GHost protocol\n" +
        "TelnetS\n:  Run Telnet over TLS\n" +
        "SBBS MQTT Spy\n:  Observe a Synchronet node through MQTT"
  }

  static addressFamilyHelp_() {
    return "# Address Family\n\n" +
        "As per DNS\n:  Use the addresses returned by DNS\n" +
        "IPv4 only\n:  Resolve and connect only to IPv4 addresses\n" +
        "IPv6 only\n:  Resolve and connect only to IPv6 addresses"
  }

  static flowControlHelp_() {
    return "# Flow Control\n\nSelect the desired serial flow control. " +
        "This should normally remain **RTS/CTS**."
  }

  static parityHelp_() {
    return "# Parity\n\nSelect the parity setting for the serial connection."
  }

  static tcpPortHelp_() {
    return "# TCP Port\n\nEnter the TCP port on which the remote server " +
        "is listening. Common defaults are Telnet `23`, RLogin `513`, " +
        "and SSH `22`."
  }

  static screenModeHelp_() {
    return "# Screen Mode\n\nSelect the display size and emulation for this connection."
  }

  static terminalTypeHelp_() {
    return "# Terminal Type\n\nThis value is sent to the remote to identify " +
        "the supported terminal emulation. Leave it blank to select the " +
        "correct value from the screen mode."
  }

  static fontHelp_() {
    return "# Font\n\nSelect the font for this connection. Some fonts " +
        "are incompatible with some modes; when necessary, SyncTERM " +
        "selects an appropriate mode."
  }

  static commRateHelp_() {
    return "# Comm Rate\n\n" +
        "## TCP Connections\n\n" +
        "Select the rate at which received characters are displayed. " +
        "This lets animated ANSI and some games run as intended.\n\n" +
        "## Serial Connections\n\nSelect the DTE rate for the port."
  }

  static ansiMusicHelp_() {
    return "# ANSI Music\n\n" +
        "ESC [ | only\n:  Enable SyncTERM's `CSI |` music sequence\n" +
        "BANSI Style\n:  Also enable BANSI `CSI N` music\n" +
        "All ANSI Music\n:  Enable both `CSI M` and `CSI N`; Delete Line is disabled\n\n" +
        "So-called ANSI music historically reused the ANSI Delete Line " +
        "sequence, which full-screen editors may need. BananaCom later " +
        "used `ESC [ N`, which is actually the Erase Field sequence. " +
        "SyncTERM's `ESC [ |` form uses a legal private extension."
  }

  static ripHelp_() {
    return "# RIP Version\n\nRIP v1 requires EGA mode. RIP v3 works in any screen mode."
  }

  static logFileHelp_() {
    return "# Log File\n\nEnter the optional session log filename."
  }

  static logLevelHelp_(kind) {
    return "# %(kind) Log Level\n\nSelect the logging verbosity. " +
        "Each level includes all messages from the less verbose levels above it."
  }

  static paletteHelp_() {
    return "# Edit Palette\n\nSelect an existing color to edit it. " +
        "Use **Add color** to extend palettes with fewer than sixteen " +
        "entries, or **Use default palette** to remove the override. " +
        "Short palettes repeat to fill all sixteen entries."
  }

  static paletteColorHelp_() {
    return "# Palette Color\n\nEnter a 24-bit RGB value from `0` through " +
        "`16777215` (`0x000000` through `0xFFFFFF`)."
  }

  static explicitSortHelp_() {
    return "# Explicit Sort Order\n\nThis number manually overrides normal " +
        "directory sorting and is not used for any other purpose. The " +
        "default is `0`."
  }

  static changeConnection_(d, type) {
    var old = d["connType"]
    if (old == 6 && type != 6) {
      d["user"] = d["password"]
      d["password"] = d["syspass"]
      d["syspass"] = ""
    } else if (old != 6 && type == 6) {
      d["syspass"] = d["password"]
      d["password"] = d["user"]
      d["user"] = ""
    }
    d["connType"] = type
    if (!serial_(type) && type != 10) d["port"] = Menu.defaultPort(type)
  }

  static changeScreen_(d, mode) {
    var old = d["screenMode"]
    var fonts = Menu.fontsCatalog
    d["screenMode"] = mode
    if (d["rip"] == 1 && mode != 23 && mode != 5) d["rip"] = 2
    if (mode == 17 && fonts.count > 33) {
      d["font"] = fonts[33][1]
      d["noStatus"] = true
    } else if ((mode == 18 || mode == 19) && fonts.count > 35) {
      d["font"] = fonts[35][1]
      d["noStatus"] = true
    } else if ((mode == 20 || mode == 21) && fonts.count > 36) {
      d["font"] = fonts[36][1]
      d["noStatus"] = true
    } else if ((mode == 25 || mode == 26) && fonts.count > 43) {
      d["font"] = fonts[43][1]
    } else if (mode >= 27 && mode <= 29 && fonts.count > 44) {
      d["font"] = fonts[44][1]
    } else if (old >= 17 && old <= 29 && fonts.count > 0) {
      d["font"] = fonts[0][1]
      if (old >= 17 && old <= 21) d["noStatus"] = false
    }
  }

  static editPalette_(app, d) {
    while (true) {
      var choices = [[-2, "Done"], [-1, "Use default palette"]]
      var colors = d["palette"]
      for (i in 0...colors.count) {
        choices.add([i, "Color %(i): %(colors[i])"])
      }
      if (colors.count < 16) choices.add([colors.count, "Add color"])
      var picked = MenuUi.choice(app, "Palette", choices, -2,
          paletteHelp_())
      if (picked == null || picked == -2) return
      if (picked == -1) {
        d["palette"] = []
      } else {
        var initial = picked < colors.count ? colors[picked] : 0
        var value = MenuUi.integer(app, "Palette Color",
            "24-bit RGB value (0 through 16777215)", initial,
            0, 16777215, paletteColorHelp_())
        if (value != null) {
          if (picked < colors.count) {
            colors[picked] = value
          } else {
            colors.add(value)
          }
          d["palette"] = colors
        }
      }
    }
  }

  static rows_(app, d, defaults) {
    var rows = []
    if (!defaults) rows.add(textRow_(app, d, "name", "Name", 30, false))
    var addrLabel = "Address"
    if (d["connType"] == 7) {
      addrLabel = "Phone Number"
    } else if (d["connType"] == 8 || d["connType"] == 9) {
      addrLabel = "Device Name"
    } else if (d["connType"] == 10) {
      addrLabel = "Command"
    }
    if (!defaults) rows.add(textRow_(app, d, "addr", addrLabel, 64, false))

    rows.add(row_("Connection Type",
        MenuUi.rowName(Menu.connectionTypes, d["connType"]), Fn.new {
      var value = MenuUi.choice(app, "Connection Type",
          Menu.connectionTypes, d["connType"], connectionTypeHelp_())
      if (value != null) changeConnection_(d, value)
    }))
    if (network_(d["connType"])) {
      rows.add(choiceRow_(app, d, "addressFamily", "Address Family",
          Menu.addressFamilies))
    }
    if (serial_(d["connType"])) {
      rows.add(choiceRow_(app, d, "flowControl", "Flow Control",
          Menu.flowControls))
      rows.add(choiceRow_(app, d, "bpsRate", "Comm Rate", Menu.rates))
      rows.add(row_("Stop Bits", d["stopBits"], Fn.new {
        d["stopBits"] = d["stopBits"] == 1 ? 2 : 1
      }))
      rows.add(row_("Data Bits", d["dataBits"], Fn.new {
        d["dataBits"] = d["dataBits"] == 8 ? 7 : 8
      }))
      rows.add(choiceRow_(app, d, "parity", "Parity", Menu.parities))
    } else if (d["connType"] != 10) {
      rows.add(row_("TCP Port", d["port"], Fn.new {
        var value = MenuUi.integer(app, "TCP Port", "TCP port",
            d["port"], 1, 65535, tcpPortHelp_())
        if (value != null) d["port"] = value
      }))
    }

    var userLabel = d["connType"] == 6 ? "SSH Username" : "Username"
    var passLabel = "Password"
    if (d["connType"] == 11) {
      passLabel = "GHost Program"
    } else if (d["connType"] == 6) {
      passLabel = "BBS Username"
    }
    var sysLabel = d["connType"] == 6 ? "BBS Password" : "System Password"
    rows.add(textRow_(app, d, "user", userLabel, 30, false))
    rows.add(textRow_(app, d, "password", passLabel, 128,
        d["connType"] != 6 && d["connType"] != 11))
    rows.add(textRow_(app, d, "syspass", sysLabel, 128, true))
    if (ssh_(d["connType"])) {
      rows.add(boolRow_(d, "sftpPublicKey", "SFTP Public Key"))
      rows.add(boolRow_(d, "sshAllowAes128Cbc", "Allow AES128-CBC"))
      rows.add(boolRow_(d, "sshAcceptEarlyData", "Accept Early Data"))
    }
    if (telnet_(d["connType"])) {
      rows.add(boolRow_(d, "telnetNoBinary", "Binmode Broken"))
      rows.add(boolRow_(d, "deferTelnetNegotiation", "Defer Negotiate"))
    }

    rows.add(row_("Screen Mode", Menu.screenModes[d["screenMode"]],
        Fn.new {
      var value = MenuUi.choice(app, "Screen Mode", Menu.screenModes,
          d["screenMode"], screenModeHelp_())
      if (value != null) changeScreen_(d, value)
    }))
    if (ssh_(d["connType"]) || telnet_(d["connType"]) ||
        d["connType"] == 1 || d["connType"] == 2 ||
        d["connType"] == 10) {
      rows.add(textRow_(app, d, "termName", "Terminal Type", 31, false))
    }
    rows.add(boolRow_(d, "lfExpand", "LF Expand"))
    var fontChoices = Menu.fontsCatalog.map {|row| [row[1], row[1]] }.toList
    rows.add(choiceRow_(app, d, "font", "Font", fontChoices))
    if (ansi_(d["screenMode"])) {
      rows.add(choiceRow_(app, d, "music", "ANSI Music", Menu.musicModes))
      rows.add(row_("RIP", MenuUi.rowName(Menu.ripModes, d["rip"]),
          Fn.new {
        var value = MenuUi.choice(app, "RIP", Menu.ripModes, d["rip"],
            ripHelp_())
        if (value != null) {
          d["rip"] = value
          if (value == 1) {
            d["screenMode"] = 5
            var fonts = Menu.fontsCatalog
            if (fonts.count > 45) d["font"] = fonts[45][1]
          }
        }
      }))
      rows.add(boolRow_(d, "forceLcf", "Force LCF Mode"))
      rows.add(boolRow_(d, "yellowIsYellow", "Yellow is Yellow"))
    } else if (d["screenMode"] == 19) {
      rows.add(boolRow_(d, "yellowIsYellow", "Yellow is Yellow"))
    }
    rows.add(boolRow_(d, "noStatus", "Hide Status Line"))
    rows.add(textRow_(app, d, "dlDir", "Download Path", 1024, false))
    rows.add(textRow_(app, d, "ulDir", "Upload Path", 1024, false))
    rows.add(textRow_(app, d, "logFile", "Log File", 1024, false))
    rows.add(boolRow_(d, "appendLogFile", "Append Log File"))
    rows.add(choiceRow_(app, d, "xferLogLevel", "Transfer Log Level",
        Menu.logLevels))
    rows.add(choiceRow_(app, d, "telnetLogLevel", "Telnet Log Level",
        Menu.logLevels))
    if (!serial_(d["connType"])) {
      rows.add(choiceRow_(app, d, "bpsRate", "Fake Comm Rate", Menu.rates))
    }
    rows.add(boolRow_(d, "hidePopups", "Hide Popups"))
    var paletteCount = d["palette"].count
    var paletteLabel = "Default"
    if (paletteCount != 0) paletteLabel = "%(paletteCount) custom colors"
    rows.add(row_("Palette", paletteLabel, Fn.new {
      editPalette_(app, d)
    }))
    if (!defaults) {
      rows.add(row_("Explicit Sort Order", d["sortOrder"], Fn.new {
        var value = MenuUi.integer(app, "Explicit Sort Order",
            "Signed sort index", d["sortOrder"], -2147483648, 2147483647,
            explicitSortHelp_())
        if (value != null) d["sortOrder"] = value
      }))
      rows.add(textRow_(app, d, "comment", "Comment", 1023, false))
    }
    return rows
  }

  static apply_(app, b, d, defaults) {
    if (!defaults && d["name"] != b.name && !b.rename(d["name"])) {
      Alert.show(app, "Save Failed", "The entry name is invalid or already in use.")
      return false
    }
    b.addr = d["addr"]
    b.port = d["port"]
    b.connType = d["connType"]
    b.user = d["user"]
    b.password = d["password"]
    b.syspass = d["syspass"]
    b.addressFamily = d["addressFamily"]
    b.screenMode = d["screenMode"]
    b.termName = d["termName"]
    b.font = d["font"]
    b.music = d["music"]
    b.rip = d["rip"]
    b.comment = d["comment"]
    b.bpsRate = d["bpsRate"]
    b.noStatus = d["noStatus"]
    b.hidePopups = d["hidePopups"]
    b.yellowIsYellow = d["yellowIsYellow"]
    b.forceLcf = d["forceLcf"]
    b.lfExpand = d["lfExpand"]
    b.dlDir = d["dlDir"]
    b.ulDir = d["ulDir"]
    b.logFile = d["logFile"]
    b.appendLogFile = d["appendLogFile"]
    b.xferLogLevel = d["xferLogLevel"]
    b.telnetLogLevel = d["telnetLogLevel"]
    b.stopBits = d["stopBits"]
    b.dataBits = d["dataBits"]
    b.parity = d["parity"]
    b.flowControl = d["flowControl"]
    b.telnetNoBinary = d["telnetNoBinary"]
    b.deferTelnetNegotiation = d["deferTelnetNegotiation"]
    b.ghostProgram = d["ghostProgram"]
    b.sftpPublicKey = d["sftpPublicKey"]
    b.sshAllowAes128Cbc = d["sshAllowAes128Cbc"]
    b.sshAcceptEarlyData = d["sshAcceptEarlyData"]
    b.palette = d["palette"]
    b.sortOrder = d["sortOrder"]
    if (b.save()) return true
    Alert.show(app, "Save Failed", "The directory entry could not be written.")
    return false
  }
}
