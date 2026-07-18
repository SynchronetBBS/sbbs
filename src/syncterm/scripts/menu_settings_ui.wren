import "syncterm" for Host, Key, Screen
import "syncterm_menu" for Menu, MenuEncryption, MenuFontSlot
import "menu_ui" for MenuUi
import "menu_bbs_editor" for BbsEditor
import "ui_popup" for Alert, Confirm
import "ui_help" for Help
import "ui_widget" for Rect

class SettingsMenu {
  static directoryPassword { __directoryPassword }
  static passwordChanged { __passwordChanged }

  static helpText(connected) {
    var text = "# SyncTERM Settings\n\n" +
        "Web Lists\n:  Configure dialing directories downloaded from the web\n" +
        "Default Connection Settings\n:  Set initial values for new entries\n"
    if (!connected) {
      text = text + "Current Screen Mode\n:  Change the current display size and mode\n"
    }
    return text +
        "Font Management\n:  Configure additional local font files\n" +
        "Program Settings\n:  Configure hardware, display, and application behavior\n" +
        "File Locations\n:  Display configuration and data paths\n" +
        "Build Options\n:  Display features selected at build time\n" +
        "List Encryption\n:  Protect the personal directory with a password"
  }

  static rows(connected) {
    var rows = [
      [0, "Web Lists"],
      [1, "Default Connection Settings"]
    ]
    if (!connected) rows.add([2, "Current Screen Mode"])
    rows.add([3, "Font Management"])
    rows.add([4, "Program Settings"])
    rows.add([5, "File Locations"])
    rows.add([6, "Build Options"])
    rows.add([7, "List Encryption"])
    return rows
  }

  static begin() {
    __passwordChanged = false
  }

  static run(app, connected) {
    begin()
    var changed = false
    while (true) {
      var picked = MenuUi.choice(app, "SyncTERM Settings",
          rows(connected), null, helpText(connected))
      if (picked == null) return changed
      if (runAction(app, picked)) changed = true
    }
  }

  static runAction(app, picked) {
    if (picked == 0) return webLists_(app)
    if (picked == 1) return BbsEditor.edit(app, Menu.defaults, true, false)
    if (picked == 2) {
      screenMode_(app)
      return false
    }
    if (picked == 3) {
      fonts_(app)
      return false
    }
    if (picked == 4) return program_(app)
    if (picked == 5) {
      locations_(app)
      return false
    }
    if (picked == 6) {
      buildOptions_(app)
      return false
    }
    if (picked == 7) return encryption_(app)
    return false
  }

  static screenMode_(app) {
    var rows = []
    var names = Menu.screenModes
    for (i in 1...names.count) rows.add([i, names[i]])
    var mode = MenuUi.choice(app, "Current Screen Mode", rows,
        Menu.currentScreenMode, screenModeHelp_())
    if (mode != null && !Menu.setScreenMode(mode)) {
      Alert.show(app, "Screen Mode", "The requested screen mode could not be applied.")
    }
  }

  static settingsLine_(label, value) {
    var text = label
    while (text.count < 24) text = text + " "
    return text + value.toString
  }

  static yesNo_(value) { value ? "Yes" : "No" }

  static helpItem_(term, description) {
    return "%(term)\n:  %(description)\n"
  }

  static programHelp_() {
    return "# Program Settings\n\n" +
        "Accepted changes are applied to the running program immediately. " +
        "The configuration file is written when you leave this screen.\n\n" +
        helpItem_("Confirm Close", "Prompt before exiting SyncTERM") +
        helpItem_("Prompt to Save URL", "Offer to save temporary URL entries") +
        helpItem_("Invert Mouse Wheel", "Reverse wheel-up and wheel-down") +
        helpItem_("Startup Screen", "The initial screen size and mode") +
        helpItem_("Output Backend", "The video or terminal output implementation") +
        helpItem_("Cursor Style", "The cursor's normal appearance") +
        helpItem_("Scrollback Lines", "The number of retained scrollback rows") +
        helpItem_("Scaling Mode", "The algorithm used when scaling the display") +
        helpItem_("Personal List", "The path or URI of the personal directory") +
        helpItem_("Shell TERM", "The TERM value supplied to local shells") +
        helpItem_("Modem Device", "The modem communications device") +
        helpItem_("Modem Init", "The command used to initialize a modem") +
        helpItem_("Modem Dial", "The command prefix used to dial a modem") +
        helpItem_("Modem Rate", "The modem-port DTE rate") +
        helpItem_("KDF Work Factor", "The scrypt cost used for list encryption") +
        helpItem_("Custom Columns / Rows", "The custom screen dimensions") +
        helpItem_("Custom Font Height", "The custom mode's font height") +
        helpItem_("Custom Aspect Width / Height", "The custom mode's aspect ratio") +
        helpItem_("UI Colors", "The colors used by menus and the native picker") +
        helpItem_("Audio Output", "The audio backends SyncTERM may try")
  }

  static screenModeHelp_() {
    return "# Current Screen Mode\n\nSelect the screen size and mode to use now."
  }

  static settingHelp_(title) {
    if (title == "Startup Screen") {
      return "# Startup Screen Mode\n\nSelect the initial screen size and mode used at startup."
    }
    if (title == "Output Backend") return outputHelp_()
    if (title == "Cursor Style") {
      return "# Default Cursor Style\n\nSelect the cursor's normal appearance."
    }
    if (title == "Scaling Mode") {
      return "# Scaling Mode\n\nSelect the algorithm used when the terminal display is scaled."
    }
    if (title == "Custom Font Height") {
      return "# Custom Font Height\n\nChoose the font height for the custom screen mode."
    }
    return null
  }

  static outputHelp_() {
    return "# Output Backend\n\n" +
        "Autodetect\n:  Try the available backends in preferred order\n" +
        "Curses\n:  Text output through the Curses library\n" +
        "Curses CP437\n:  Curses with a terminal configured for Code Page 437\n" +
        "ANSI\n:  Write CP437 ANSI to standard output; useful for BBS-door operation\n" +
        "X11\n:  Graphical output through Xlib\n" +
        "SDL\n:  Graphical output through SDL\n" +
        "GDI\n:  Native Windows graphical output\n" +
        "Console\n:  Native text-console output\n\n" +
        "Fullscreen variants start the corresponding graphical backend " +
        "in full-screen mode. Only backends present in this build are listed."
  }

  static scrollbackHelp_() {
    return "# Scrollback Buffer Lines\n\nEnter the number of retained " +
        "scrollback rows. The value must be greater than zero."
  }

  static listPathHelp_() {
    return "# Personal List\n\nEnter the complete path or URI of the personal BBS directory."
  }

  static shellTermHelp_() {
    return "# TERM For Shell\n\nEnter the value assigned to the `TERM` " +
        "environment variable for local shell connections. For example, " +
        "`ansi` selects basic ANSI behavior."
  }

  static modemDeviceHelp_() {
    return "# Modem Device\n\nEnter the device used to communicate with the modem."
  }

  static modemRateHelp_() {
    return "# Modem Rate\n\nEnter the DTE rate in bits per second. Use " +
        "the highest rate supported by the communications port and modem, " +
        "such as `38400`, `57600`, or `115200`. This is sometimes " +
        "incorrectly called the baud rate."
  }

  static modemInitHelp_() {
    return "# Modem Initialization\n\nEnter the command used to initialize " +
        "the modem. `AT&F` loads a Hayes-compatible modem's factory " +
        "defaults.\n\n" +
        "## Expected Hayes Settings\n\n" +
        "Echo on\n:  `E1`\n" +
        "Verbal result codes\n:  `Q0V1`\n" +
        "Normal carrier detect\n:  `&C1`\n" +
        "Normal DTR\n:  `&D2`\n\n" +
        "## Expected USRobotics Settings\n\n" +
        "Include connection speed\n:  `&X4`\n" +
        "Locked speed\n:  `&B1`\n" +
        "CTS/RTS flow control\n:  `&H1&R2`\n" +
        "Disable software flow control\n:  `&I0`"
  }

  static modemDialHelp_() {
    return "# Modem Dial String\n\nEnter the modem command used before " +
        "the phone number. For example, `ATDT` selects touch-tone dialing " +
        "on a Hayes-compatible modem."
  }

  static kdfHelp_() {
    return "# Key Derivation Work Factor\n\nThis is the base-2 logarithm " +
        "of scrypt's N parameter: `N = 2^value`. Higher values make " +
        "offline dictionary attacks more expensive, but also increase " +
        "the time and memory needed to unlock the list.\n\n" +
        "The default `15` uses N=32768 and about 16 MiB. The supported " +
        "range is 8 through 24; each step doubles CPU and memory cost."
  }

  static customColumnsHelp_() {
    return "# Custom Columns\n\nEnter 40 through 255 columns. Values " +
        "other than 40, 80, and 132 may not be supported by every backend."
  }

  static customRowsHelp_() {
    return "# Custom Rows\n\nEnter 14 through 255 rows for the custom screen mode."
  }

  static customAspectHelp_(width) {
    var dimension = "height"
    var historical = "3"
    if (width) {
      dimension = "width"
      historical = "4"
    }
    return "# Custom Aspect %(dimension)\n\nEnter the %(dimension) part " +
        "of the aspect ratio. The historical value is `%(historical)`."
  }

  static colorHelp_() {
    return "# UI Color\n\nSelect the color used for this part of the " +
        "menu interface and native file picker."
  }

  static audioHelp_() {
    return "# Audio Output\n\nToggle the audio backends SyncTERM may " +
        "use. Enabled backends are attempted in the order shown."
  }

  static webListsHelp_() {
    return "# Web Lists\n\nSelect the blank final row to add a dialing " +
        "directory available through HTTP or HTTPS. Each entry needs a " +
        "unique name, used as its cache filename, and the URI from which " +
        "the directory is downloaded. Insert adds before the highlighted " +
        "row, Delete removes it, and Enter opens its actions.\n\n" +
        "The SyncTERM project provides these lists:\n\n" +
        "- `http://syncterm.bbsdev.net/syncterm.lst`\n" +
        "- `http://syncterm.bbsdev.net/telnetbbsguide.lst`"
  }

  static fontManagementHelp_() {
    return "# Font Management\n\nSelect the blank final row to add a font " +
        "set used by connection profiles. Insert adds before the highlight, " +
        "Delete removes it, and Enter opens its details. Changes are stored " +
        "when you leave this screen.\n\n" +
        "8 x 8\n:  Modes with at least 35 lines and C64/C128 modes\n" +
        "8 x 14\n:  Modes with 28 through 34 lines\n" +
        "8 x 16\n:  Modes with fewer than 28 lines or exactly 30 lines\n" +
        "12 x 20\n:  Prestel mode"
  }

  static fontDetailsHelp_() {
    return "# Font Details\n\nRename changes the font's menu name. " +
        "Select a cell size to choose or clear its file. `[` and `]` move " +
        "to the previous or next font.\n\n" +
        "8 x 8\n:  Modes with at least 35 lines and C64/C128 modes\n" +
        "8 x 14\n:  Modes with 28 through 34 lines\n" +
        "8 x 16\n:  Modes with fewer than 28 lines or exactly 30 lines\n" +
        "12 x 20\n:  Prestel mode"
  }

  static fontNameHelp_() {
    return "# Font Name\n\nEnter the name that will identify this font in menus."
  }

  static fontMask_(slot) {
    if (slot == MenuFontSlot.eightByEight) return "*.f8"
    if (slot == MenuFontSlot.eightByFourteen) return "*.f14"
    if (slot == MenuFontSlot.eightBySixteen) return "*.f16"
    return "*.f20"
  }

  static encryptionHelp_() {
    return "# List Encryption\n\nChoose the encryption used for the " +
        "personal directory. Selecting an encrypted format prompts for a " +
        "new password and rewrites the list. **Not Encrypted** stores the " +
        "directory without password protection."
  }

  static pickSetting_(app, title, rows, current) {
    return MenuUi.choice(app, title, rows, current, settingHelp_(title))
  }

  static program_(app) {
    var s = Menu.settings
    var changed = false
    while (true) {
      var rows = []
      rows.add([1, settingsLine_("Confirm Close", yesNo_(s.confirmClose))])
      rows.add([2, settingsLine_("Prompt to Save URL", yesNo_(s.promptSave))])
      rows.add([3, settingsLine_("Invert Mouse Wheel", yesNo_(s.invertWheel))])
      rows.add([4, settingsLine_("Startup Screen", Menu.screenModes[s.startupMode])])
      rows.add([5, settingsLine_("Output Backend", MenuUi.rowName(Menu.outputModes, s.outputMode))])
      rows.add([6, settingsLine_("Cursor Style", Menu.cursorStyles[s.cursorStyle])])
      rows.add([7, settingsLine_("Scrollback Lines", s.scrollbackLines)])
      rows.add([8, settingsLine_("Scaling Mode", Menu.scalingModes[s.scalingMode])])
      rows.add([9, settingsLine_("Personal List", s.listPath)])
      rows.add([10, settingsLine_("Shell TERM", s.shellTerm)])
      rows.add([11, settingsLine_("Modem Device", s.modemDevice)])
      rows.add([12, settingsLine_("Modem Init", s.modemInit)])
      rows.add([13, settingsLine_("Modem Dial", s.modemDial)])
      rows.add([14, settingsLine_("Modem Rate", s.modemRate)])
      rows.add([15, settingsLine_("KDF Work Factor", "2^%(s.kdfShift)")])
      rows.add([16, settingsLine_("Custom Columns", s.customColumns)])
      rows.add([17, settingsLine_("Custom Rows", s.customRows)])
      rows.add([18, settingsLine_("Custom Font Height", s.customFontHeight)])
      rows.add([19, settingsLine_("Custom Aspect Width", s.customAspectWidth)])
      rows.add([20, settingsLine_("Custom Aspect Height", s.customAspectHeight)])
      rows.add([21, settingsLine_("Frame Color", Menu.colors[s.frameColor])])
      rows.add([22, settingsLine_("Text Color", Menu.colors[s.textColor])])
      rows.add([23, settingsLine_("Background Color", Menu.backgroundColors[s.backgroundColor])])
      rows.add([24, settingsLine_("Inverse Color", Menu.colors[s.inverseColor])])
      rows.add([25, settingsLine_("Lightbar Color", Menu.colors[s.lightbarColor])])
      rows.add([26, settingsLine_("Lightbar Background", Menu.backgroundColors[s.lightbarBackgroundColor])])
      rows.add([27, "Audio Output"])

      var picked = MenuUi.choice(app, "Program Settings", rows, null,
          programHelp_())
      if (picked == null) {
        if (!changed) return false
        if (Host.safeMode || s.save()) return true
        Alert.show(app, "Program Settings",
            "The settings could not be saved.")
        return true
      }
      if (picked == 1) {
        s.confirmClose = !s.confirmClose
      } else if (picked == 2) {
        s.promptSave = !s.promptSave
      } else if (picked == 3) {
        s.invertWheel = !s.invertWheel
      } else if (picked == 4) {
        var value = pickSetting_(app, "Startup Screen", Menu.screenModes, s.startupMode)
        if (value != null) s.startupMode = value
      } else if (picked == 5) {
        var value = pickSetting_(app, "Output Backend", Menu.outputModes, s.outputMode)
        if (value != null) s.outputMode = value
      } else if (picked == 6) {
        var value = pickSetting_(app, "Cursor Style", Menu.cursorStyles, s.cursorStyle)
        if (value != null) s.cursorStyle = value
      } else if (picked == 7) {
        var value = MenuUi.integer(app, "Scrollback Lines", "Number of retained lines",
            s.scrollbackLines, 1, 10000000, scrollbackHelp_())
        if (value != null) s.scrollbackLines = value
      } else if (picked == 8) {
        var value = pickSetting_(app, "Scaling Mode", Menu.scalingModes, s.scalingMode)
        if (value != null) s.scalingMode = value
      } else if (picked == 9) {
        var value = MenuUi.prompt(app, "Personal List", "Path or URI",
            s.listPath, 1024, false, listPathHelp_())
        if (value != null) s.listPath = value
      } else if (picked == 10) {
        var value = MenuUi.prompt(app, "Shell TERM", "Terminal name",
            s.shellTerm, 255, false, shellTermHelp_())
        if (value != null) s.shellTerm = value
      } else if (picked == 11) {
        var value = MenuUi.prompt(app, "Modem Device", "Serial device",
            s.modemDevice, 255, false, modemDeviceHelp_())
        if (value != null) s.modemDevice = value
      } else if (picked == 12) {
        var value = MenuUi.prompt(app, "Modem Init", "Initialization string",
            s.modemInit, 255, false, modemInitHelp_())
        if (value != null) s.modemInit = value
      } else if (picked == 13) {
        var value = MenuUi.prompt(app, "Modem Dial", "Dial string",
            s.modemDial, 255, false, modemDialHelp_())
        if (value != null) s.modemDial = value
      } else if (picked == 14) {
        var value = MenuUi.integer(app, "Modem Rate", "Bits per second",
            s.modemRate, 1, 2147483647, modemRateHelp_())
        if (value != null) s.modemRate = value
      } else if (picked == 15) {
        var value = MenuUi.integer(app, "KDF Work Factor",
            "Exponent N for scrypt-Nn", s.kdfShift, 8, 24, kdfHelp_())
        if (value != null) s.kdfShift = value
      } else if (picked == 16) {
        var value = MenuUi.integer(app, "Custom Columns", "Columns",
            s.customColumns, 40, 255, customColumnsHelp_())
        if (value != null) s.customColumns = value
      } else if (picked == 17) {
        var value = MenuUi.integer(app, "Custom Rows", "Rows",
            s.customRows, 14, 255, customRowsHelp_())
        if (value != null) s.customRows = value
      } else if (picked == 18) {
        var choices = [[8, "8"], [14, "14"], [16, "16"]]
        var value = pickSetting_(app, "Custom Font Height", choices, s.customFontHeight)
        if (value != null) s.customFontHeight = value
      } else if (picked == 19) {
        var value = MenuUi.integer(app, "Custom Aspect Width", "Aspect width",
            s.customAspectWidth, 1, 255, customAspectHelp_(true))
        if (value != null) s.customAspectWidth = value
      } else if (picked == 20) {
        var value = MenuUi.integer(app, "Custom Aspect Height", "Aspect height",
            s.customAspectHeight, 1, 255, customAspectHelp_(false))
        if (value != null) s.customAspectHeight = value
      } else if (picked >= 21 && picked <= 26) {
        editColor_(app, s, picked)
      } else if (picked == 27) {
        if (audio_(app, s)) changed = true
      }
      if (s.dirty && applySettings_(app, s)) changed = true
    }
  }

  static applySettings_(app, s) {
    if (s.apply()) return true
    s.reload()
    Alert.show(app, "Program Settings",
        "The setting could not be applied.")
    return false
  }

  static editColor_(app, s, which) {
    var background = which == 23 || which == 26
    var rows = background ? Menu.backgroundColors : Menu.colors
    var current = s.lightbarBackgroundColor
    if (which == 21) current = s.frameColor
    if (which == 22) current = s.textColor
    if (which == 23) current = s.backgroundColor
    if (which == 24) current = s.inverseColor
    if (which == 25) current = s.lightbarColor
    var value = MenuUi.choice(app, "Color", rows, current, colorHelp_())
    if (value == null) return
    if (which == 21) s.frameColor = value
    if (which == 22) s.textColor = value
    if (which == 23) s.backgroundColor = value
    if (which == 24) s.inverseColor = value
    if (which == 25) s.lightbarColor = value
    if (which == 26) s.lightbarBackgroundColor = value
  }

  static audio_(app, s) {
    var changed = false
    while (true) {
      var rows = []
      for (row in Menu.audioModes) {
        var enabled = (s.audioModes & row[0]) != 0
        rows.add([row[0], "[%(enabled ? "X" : " ")] %(row[1])"])
      }
      var bit = MenuUi.choice(app, "Audio Output", rows, null,
          audioHelp_())
      if (bit == null) return changed
      if ((s.audioModes & bit) != 0) {
        s.audioModes = s.audioModes & ~bit
      } else {
        s.audioModes = s.audioModes | bit
      }
      if (applySettings_(app, s)) changed = true
    }
  }

  static locations_(app) {
    var p = Menu.fileLocations
    var text = "Global directory:\n  %(p["globalList"])\n\n" +
        "Personal directory:\n  %(p["personalList"])\n\n" +
        "Configuration:\n  %(p["configuration"])\n\n" +
        "Downloads:\n  %(p["download"])\n\n" +
        "Cache:\n  %(p["cache"])\n\n" +
        "SSH keys:\n  %(p["keys"])\n\n" +
        "Wren scripts:\n  %(p["scripts"])"
    MenuUi.text(app, "File Locations", text)
  }

  static buildOptions_(app) {
    var lines = ["`SyncTERM Build Options`", ""]
    var last = null
    for (row in Menu.buildOptions) {
      if (row[0] != last) {
        if (last != null) lines.add("")
        if (row[0] != "Terminal") lines.add(row[0])
        last = row[0]
      }
      var indent = "    "
      if (row[0] == "Terminal") indent = ""
      if (row[1] == "Xinerama" || row[1] == "XRandR" ||
          row[1] == "XRender") indent = "        "
      var mark = row[2] ? "[`√`]" : "[ ]"
      lines.add("%(indent)%(mark) %(row[1])")
    }
    var body = lines.join("\n")
    var viewer = Help.new("Build Options", body)
    viewer.preformatted = true
    var size = Screen.size
    var width = 60.min(size[0])
    var height = 21.min(size[1])
    var left = ((size[0] - width + 2) / 2).floor.max(1)
    var top = ((size[1] - height + 1) / 2).floor + 1
    viewer.bounds = Rect.new(left, top, width, height)
    app.modal(viewer)
  }

  static webLists_(app) {
    var changed = false
    while (true) {
      var rows = []
      var lists = Menu.webLists
      for (i in 0...lists.count) rows.add([i, lists[i][0]])
      rows.add([-1, ""])
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      var picked = MenuUi.commandChoice(app, "Web Lists", rows, null,
          webListsHelp_(), commands)
      if (picked == null) return changed
      var command = picked[0]
      var index = picked[1]
      if (command == "insert" ||
          (command == "select" && index == -1)) {
        if (index == -1) index = lists.count
        var initialName = lists.count == 0 ? "SyncTERM BBS List" : ""
        var name = MenuUi.prompt(app, "Add Web List", "Name", initialName, 255,
            false, webListsHelp_())
        if (name == null) continue
        var initialUri = ""
        if (lists.count == 0) {
          initialUri = "http://syncterm.bbsdev.net/syncterm.lst"
        }
        var uri = MenuUi.prompt(app, "Add Web List", "URI", initialUri, 1024,
            false, webListsHelp_())
        if (uri == null) continue
        app.popStatus("Fetching web list")
        var error = Menu.addWebList(name, uri, index)
        app.popStatus(null)
        if (error == null) {
          changed = true
        } else {
          Alert.show(app, "Web List", error)
        }
      } else if (command == "delete") {
        if (Menu.deleteWebList(index)) {
          changed = true
        } else {
          Alert.show(app, "Web List", "The web list could not be deleted.")
        }
      } else if (command == "select") {
        if (webList_(app, index, lists[index])) changed = true
      }
    }
  }

  static webList_(app, index, row) {
    var action = MenuUi.choice(app, row[0],
        [[0, "Edit URI"], [1, "Refresh Cache"], [2, "Delete"]], null,
        webListsHelp_())
    if (action == null) return false
    if (action == 0) {
      var uri = MenuUi.prompt(app, "Web List URI", "URI", row[1], 1024,
          false, webListsHelp_())
      if (uri == null) return false
      if (Menu.updateWebList(index, uri)) return true
      Alert.show(app, "Web List", "The URI could not be saved.")
    } else if (action == 1) {
      app.popStatus("Fetching web list")
      var error = Menu.refreshWebList(index)
      app.popStatus(null)
      if (error == null) return true
      Alert.show(app, "Web List", error)
    } else if (action == 2 && Confirm.show(app, "Delete %(row[0])?")) {
      if (Menu.deleteWebList(index)) return true
      Alert.show(app, "Web List", "The web list could not be deleted.")
    }
    return false
  }

  static fonts_(app) {
    while (true) {
      var fonts = Menu.fonts
      if (fonts == null) {
        Alert.show(app, "Font Management", "The font configuration could not be read.")
        return
      }
      var rows = []
      for (i in 0...fonts.count) rows.add([i, fonts[i].name])
      rows.add([-1, ""])
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      var picked = MenuUi.commandChoice(app, "Font Management", rows, null,
          fontManagementHelp_(), commands)
      if (picked == null) {
        if (Menu.fontsDirty && !Menu.saveFonts()) {
          Alert.show(app, "Font Management",
              "The font configuration could not be saved.")
        }
        return
      }
      var command = picked[0]
      var index = picked[1]
      if (command == "insert" ||
          (command == "select" && index == -1)) {
        if (index == -1) index = fonts.count
        var name = MenuUi.prompt(app, "Add Font", "Font name", "", 50,
            false, fontNameHelp_())
        if (name != null && Menu.createFont(name, index) == null) {
          Alert.show(app, "Font Management", "The font could not be added.")
        }
      } else if (command == "delete") {
        if (!fonts[index].delete()) {
          Alert.show(app, "Font Management", "The font could not be deleted.")
        }
      } else if (command == "select") {
        font_(app, index)
      }
    }
  }

  static font_(app, index) {
    var slots = [
      [MenuFontSlot.eightByEight, "8 x 8"],
      [MenuFontSlot.eightByFourteen, "8 x 14"],
      [MenuFontSlot.eightBySixteen, "8 x 16"],
      [MenuFontSlot.twelveByTwenty, "12 x 20"]
    ]
    while (true) {
      var fonts = Menu.fonts
      if (fonts == null || fonts.count == 0) return
      index = ((index % fonts.count) + fonts.count) % fonts.count
      var font = fonts[index]
      var rows = [[-2, "Rename"], [-1, "Delete Font"]]
      for (slot in slots) {
        var path = font.path(slot[0])
        rows.add([slot[0], "%(slot[1])  %(path == null ? "<none>" : path)"])
      }
      var commands = {}
      commands[0x5B] = ["previous", true]
      commands[0x5D] = ["next", true]
      var picked = MenuUi.commandChoice(app, font.name, rows, null,
          fontDetailsHelp_(), commands)
      if (picked == null) return
      var command = picked[0]
      var value = picked[1]
      if (command == "previous") {
        index = index - 1
      } else if (command == "next") {
        index = index + 1
      } else if (value == -2) {
        var name = MenuUi.prompt(app, "Rename Font", "Font name", font.name,
            50, false, fontNameHelp_())
        if (name != null) font.name = name
      } else if (value == -1) {
        if (Confirm.show(app, "Delete %(font.name)?")) {
          font.delete()
          return
        }
      } else {
        var action = MenuUi.choice(app, "Font File",
            [[0, "Choose File"], [1, "Clear File"]], null,
            fontDetailsHelp_())
        if (action == 0) {
          app.releaseFocus()
          var file = Host.pickFile(".", fontMask_(value), 1)
          app.restoreFocus()
          if (file != null && !font.setFile(value, file)) {
            Alert.show(app, "Font File",
                "The selected file has the wrong size.")
          }
        } else if (action == 1) {
          font.clearFile(value)
        }
      }
    }
  }

  static encryption_(app) {
    var choices = [
      [[MenuEncryption.none, 0], "Not Encrypted"],
      [[MenuEncryption.aes, 128], "AES (128-bit)"],
      [[MenuEncryption.aes, 256], "AES (256-bit)"],
      [[MenuEncryption.chacha20, 0], "ChaCha20"]
    ]
    var picked = MenuUi.choice(app, "List Encryption", choices, null,
        encryptionHelp_())
    if (picked == null) return false
    var password = null
    if (picked[0] != MenuEncryption.none) {
      password = MenuUi.prompt(app, "List Encryption", "New password", "",
          1023, true, encryptionHelp_())
      if (password == null || password.count == 0) return false
      var verify = MenuUi.prompt(app, "List Encryption", "Confirm password",
          "", 1023, true, encryptionHelp_())
      if (verify != password) {
        Alert.show(app, "List Encryption", "The passwords do not match.")
        return false
      }
    }
    if (!Menu.setEncryption(picked[0], picked[1], password)) {
      Alert.show(app, "List Encryption", "The personal directory could not be rewritten.")
      return false
    }
    __directoryPassword = password
    __passwordChanged = true
    return true
  }
}
