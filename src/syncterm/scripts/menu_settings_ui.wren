import "syncterm" for FilePickerOptions, Host, Key
import "syncterm_menu" for Menu, MenuEncryption, MenuFontSlot
import "menu_ui" for MenuUi
import "ui_style" for Theme
import "menu_bbs_editor" for BbsEditor
import "ui_popup" for Alert
import "ui_help" for Help

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
    text = text +
        "Font Management\n:  Configure additional local font files\n" +
        "Program Settings\n:  Configure hardware, display, and application behavior\n" +
        "Themes\n:  Preview and select the user-interface theme\n" +
        "File Locations\n:  Display configuration and data paths\n" +
        "Build Options\n:  Display features selected at build time"
    if (!connected) {
      text = text + "\nAlt-B\n:  View scrollback of the last session"
    }
    if (!connected && Menu.encryptionAvailable) {
      text = text +
          "\nList Encryption\n:  Protect the personal directory with a password"
    }
    return text
  }

  static rows(connected) {
    var rows = [
      [0, "Web Lists"],
      [1, "Default Connection Settings"]
    ]
    if (!connected) rows.add([2, "Current Screen Mode"])
    rows.add([3, "Font Management"])
    rows.add([4, "Program Settings"])
    rows.add([5, "Themes"])
    rows.add([6, "File Locations"])
    rows.add([7, "Build Options"])
    if (!connected && Menu.encryptionAvailable) {
      rows.add([8, "List Encryption"])
    }
    return rows
  }

  static begin() {
    __passwordChanged = false
  }

  static run(app, connected) {
    begin()
    var changed = false
    var selected = 0
    while (true) {
      var picked = MenuUi.choice(app, "SyncTERM Settings",
          rows(connected), selected, helpText(connected), Fn.new {|value|
        selected = value
        if (runAction(app, value, connected)) changed = true
      })
      if (picked == null) return changed
    }
  }

  static runAction(app, picked) { runAction(app, picked, false) }

  static runAction(app, picked, connected) {
    if (picked == 0) return webLists_(app)
    if (picked == 1) {
      var changed = BbsEditor.edit(app, Menu.defaults, true, false)
      // Reloading the model would discard defaults accepted for this
      // process when safe mode suppresses their file write.
      return Host.safeMode ? false : changed
    }
    if (picked == 2) {
      screenMode_(app)
      return false
    }
    if (picked == 3) {
      if (!Host.safeMode) fonts_(app)
      return false
    }
    if (picked == 4) return program_(app, connected)
    if (picked == 5) {
      themes_(app)
      return false
    }
    if (picked == 6) {
      locations_(app)
      return false
    }
    if (picked == 7) {
      buildOptions_(app)
      return false
    }
    if (picked == 8 && !connected && Menu.encryptionAvailable) {
      return encryption_(app)
    }
    return false
  }

  static themeEntry_(entries, filename) {
    for (entry in entries) {
      if (entry[0] == filename) return entry
    }
    return null
  }

  static themeRows_(entries) {
    var counts = {}
    for (entry in entries) {
      var name = entry[1]
      counts[name] = counts.containsKey(name) ? counts[name] + 1 : 1
    }
    var rows = []
    for (entry in entries) {
      var label = entry[1]
      if (counts[label] > 1 && entry[0].count > 0) {
        label = "%(label) (%(entry[0]))"
      }
      if (entry[5] != null) label = "[Invalid] %(label)"
      rows.add([entry[0], label])
    }
    return rows
  }

  static themeHelp_(entry) {
    if (entry == null) return "# Themes\n\nThe selected theme is unavailable."
    var text = "# %(entry[1])\n\n"
    if (entry[0].count == 0) {
      text = text + "File\n:  Built-in Classic Theme\n"
    } else {
      text = text + "File\n:  `%(entry[0])`\n"
    }
    if (entry[2] != null) text = text + "Author\n:  %(entry[2])\n"
    if (entry[4] != null) text = text + "Version\n:  %(entry[4])\n"
    if (entry[3] != null) text = text + "\n%(entry[3])\n"
    if (entry[5] != null) {
      return text + "\nError\n:  %(entry[5])\n\nThis theme cannot be selected."
    }
    return text + "\nEnter\n:  Select this theme\nEsc\n:  Keep the current theme"
  }

  static themes_(app) {
    var entries = Menu.themes
    var rows = themeRows_(entries)
    var current = Menu.selectedThemeFile
    MenuUi.browserChoice(app, "Themes", rows, current,
        Fn.new {|filename|
          themeHelp_(themeEntry_(entries, filename))
        }, Fn.new {|filename|
          var entry = themeEntry_(entries, filename)
          if (entry == null || entry[5] != null) {
            Menu.cancelThemePreview()
          } else {
            var error = Menu.previewTheme(filename)
            if (error != null) Menu.cancelThemePreview()
          }
          app.theme = Theme.current
        }, Fn.new {|filename|
          var entry = themeEntry_(entries, filename)
          if (entry == null || entry[5] != null) return false
          var error = Menu.selectTheme(filename)
          if (error != null) {
            Alert.show(app, "Themes", error)
            return false
          }
          app.theme = Theme.current
          return true
        }, Fn.new {
          Menu.cancelThemePreview()
          app.theme = Theme.current
        })
  }

  static screenMode_(app) {
    var rows = []
    var names = Menu.screenModes
    for (i in 1...names.count) rows.add([i, names[i]])
    var mode = MenuUi.choice(app, "Screen Mode", rows,
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

  static audioSummary_(s) {
    var names = []
    for (row in Menu.audioModes) {
      if ((s.audioModes & row[0]) != 0) names.add(row[1])
    }
    return names.count == 0 ? "<None>" : names.join(", ")
  }

  static helpItem_(term, description) {
    return "%(term)\n:  %(description)\n"
  }

  static programHelp_(connected) {
    var text = "# Program Settings\n\n" +
        "Accepted changes are applied to the running program immediately. " +
        "The configuration file is written when you leave this screen.\n\n" +
        helpItem_("Confirm Program Exit", "Prompt before exiting SyncTERM") +
        helpItem_("Prompt to Save", "Offer to save temporary URL entries") +
        helpItem_("Startup Screen Mode", "The initial screen size and mode") +
        helpItem_("Video Output Mode", "The video or terminal output implementation") +
        helpItem_("Default Cursor Style", "The cursor's normal appearance") +
        helpItem_("Audio Output Mode", "The audio backends SyncTERM may try") +
        helpItem_("Scrollback Buffer Lines", "The number of retained scrollback rows") +
        helpItem_("Modem/Comm Device", "The modem communications device") +
        helpItem_("Modem/Comm Rate", "The modem-port DTE rate") +
        helpItem_("Modem Init String", "The command used to initialize a modem") +
        helpItem_("Modem Dial String", "The command prefix used to dial a modem") +
        helpItem_("List Path", "The path or URI of the personal directory") +
        helpItem_("TERM For Shell", "The TERM value supplied to local shells") +
        helpItem_("Scaling", "Cycle the display scaling algorithm") +
        helpItem_("Invert Mouse Wheel", "Reverse wheel-up and wheel-down") +
        helpItem_("Key Derivation Shift", "The scrypt cost used for list encryption") +
        helpItem_("Classic Theme Colours",
            "The colours used by the shipped menu and file picker")
    if (!connected) {
      text = text + helpItem_("Custom Screen Mode",
          "The dimensions, font height, and aspect ratio of the custom mode")
    }
    return text
  }

  static screenModeHelp_() {
    return "# Current Screen Mode\n\nChange the current screen size and " +
        "mode.\n\nTo change the initial screen mode, select Startup " +
        "Screen Mode under Program Settings instead."
  }

  static settingHelp_(title) {
    if (title == "Startup Screen Mode") {
      return "# Startup Screen Mode\n\nSelect the initial screen size and mode used at startup."
    }
    if (title == "Video Output Mode") return outputHelp_()
    if (title == "Default Cursor Style") {
      return "# Default Cursor Style\n\nSelect the cursor's normal appearance."
    }
    if (title == "Custom Font Height") {
      return "# Custom Font Height\n\nChoose the font height for the custom screen mode."
    }
    return null
  }

  static outputHelp_() {
    return "# Video Output Mode\n\n" +
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
    return "# List Path\n\nEnter the complete path or URI of the personal BBS directory."
  }

  static shellTermHelp_() {
    return "# TERM For Shell\n\nEnter the value assigned to the `TERM` " +
        "environment variable for local shell connections. For example, " +
        "`ansi` selects basic ANSI behavior."
  }

  static modemDeviceHelp_() {
    return "# Modem/Comm Device\n\nEnter the device used to communicate with the modem."
  }

  static modemRateHelp_() {
    return "# Modem/Comm Rate\n\nEnter the DTE rate in bits per second. Use " +
        "the highest rate supported by the communications port and modem, " +
        "such as `38400`, `57600`, or `115200`. This is sometimes " +
        "incorrectly called the baud rate. Enter `0` to use the current " +
        "or default communications-port rate."
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

  static customModeHelp_() {
    return "# Custom Screen Mode\n\nConfigure the rows, columns, font " +
        "height, and aspect ratio used by the Custom screen mode."
  }

  static customAspectHelp_(width) {
    var dimension = "height"
    var defaultValue = "3"
    if (width) {
      dimension = "width"
      defaultValue = "4"
    }
    return "# Custom Aspect %(dimension)\n\nEnter the %(dimension) part " +
        "of the aspect ratio. The default value is `%(defaultValue)`."
  }

  static colorHelp_() {
    return "# Theme Colour\n\nSelect the colour used for this part of " +
        "the shipped menu and file picker."
  }

  static colorsHelp_() {
    return "# Classic Theme Colours\n\nSelect the interface element whose colour " +
        "you want to change."
  }

  static audioHelp_() {
    return "# Audio Output Mode\n\nToggle the audio backends SyncTERM may " +
        "use. Enabled backends are attempted in the order shown."
  }

  static webListsHelp_() {
    return "# Web Lists\n\nSelect the blank final row to add a dialing " +
        "directory available through HTTP or HTTPS. Each entry needs a " +
        "unique name, used as its cache filename, and the URI from which " +
        "the directory is downloaded. Insert adds before the highlighted " +
        "row, Delete removes it, and Enter edits its URI.\n\n" +
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
    return "# Font Details\n\nSelect Name to rename the font. Select " +
        "a cell size to choose its file. Insert or the blank final row " +
        "adds a font before this one, and Delete removes this font. `[` " +
        "and `]` move to the previous or next font.\n\n" +
        "8 x 8\n:  Modes with at least 35 lines and C64/C128 modes\n" +
        "8 x 14\n:  Modes with 28 through 34 lines\n" +
        "8 x 16\n:  Modes with fewer than 28 lines or exactly 30 lines\n" +
        "12 x 20\n:  Prestel mode"
  }

  static fontNameHelp_() {
    return "# Font Name\n\nEnter the name that will identify this font in menus."
  }

  static fontNameFits_(name) { name.bytes.count <= 50 }

  static fontMask_(slot) {
    if (slot == MenuFontSlot.eightByEight) return "*.f8"
    if (slot == MenuFontSlot.eightByFourteen) return "*.f14"
    if (slot == MenuFontSlot.eightBySixteen) return "*.f16"
    return "*.f20"
  }

  static fontSizeName_(slot) {
    if (slot == MenuFontSlot.eightByEight) return "8x8"
    if (slot == MenuFontSlot.eightByFourteen) return "8x14"
    if (slot == MenuFontSlot.eightBySixteen) return "8x16"
    return "12x20"
  }

  static encryptionHelp_() {
    return "# List Encryption\n\nChange Password rewrites the directory " +
        "with a new password while retaining its current encryption. The " +
        "encryption choices reuse the current password, prompting only when " +
        "the list is not yet encrypted. Decrypt removes password protection."
  }

  static pickSetting_(app, title, rows, current) {
    return MenuUi.choice(app, title, rows, current, settingHelp_(title))
  }

  static program_(app, connected) {
    var s = Menu.settings
    var changed = false
    var selected = 1
    while (true) {
      var rows = []
      rows.add([1, settingsLine_("Confirm Program Exit", yesNo_(s.confirmClose))])
      rows.add([2, settingsLine_("Prompt to Save", yesNo_(s.promptSave))])
      rows.add([3, settingsLine_("Startup Screen Mode", Menu.screenModes[s.startupMode])])
      rows.add([4, settingsLine_("Video Output Mode", MenuUi.rowName(Menu.outputModes, s.outputMode))])
      rows.add([5, settingsLine_("Default Cursor Style", Menu.cursorStyles[s.cursorStyle])])
      rows.add([6, settingsLine_("Audio Output Mode", audioSummary_(s))])
      rows.add([7, settingsLine_("Scrollback Buffer Lines", s.scrollbackLines)])
      rows.add([8, settingsLine_("Modem/Comm Device", s.modemDevice)])
      var rate = s.modemRate == 0 ? "Current" : "%(s.modemRate)bps"
      rows.add([9, settingsLine_("Modem/Comm Rate", rate)])
      rows.add([10, settingsLine_("Modem Init String", s.modemInit)])
      rows.add([11, settingsLine_("Modem Dial String", s.modemDial)])
      rows.add([12, settingsLine_("List Path", s.listPath)])
      rows.add([13, settingsLine_("TERM For Shell", s.shellTerm)])
      rows.add([14, settingsLine_("Scaling", Menu.scalingModes[s.scalingMode])])
      rows.add([15, settingsLine_("Invert Mouse Wheel", yesNo_(s.invertWheel))])
      rows.add([16, settingsLine_("Key Derivation Shift", s.kdfShift)])
      rows.add([17, "Classic Theme Colours"])
      if (!connected) rows.add([18, "Custom Screen Mode"])

      var picked = MenuUi.choice(app, "Program Settings", rows, selected,
          programHelp_(connected), Fn.new {|value|
        selected = value
        if (value == 1) {
          s.confirmClose = !s.confirmClose
        } else if (value == 2) {
          s.promptSave = !s.promptSave
        } else if (value == 3) {
          var next = pickSetting_(app, "Startup Screen Mode",
              Menu.screenModes, s.startupMode)
          if (next != null) s.startupMode = next
        } else if (value == 4) {
          var next = pickSetting_(app, "Video Output Mode",
              Menu.outputModes, s.outputMode)
          if (next != null) s.outputMode = next
        } else if (value == 5) {
          var next = pickSetting_(app, "Default Cursor Style",
              Menu.cursorStyles, s.cursorStyle)
          if (next != null) s.cursorStyle = next
        } else if (value == 6) {
          if (audio_(app, s)) changed = true
        } else if (value == 7) {
          var next = MenuUi.integer(app, "Scrollback Lines",
              "Number of retained lines", s.scrollbackLines, 1,
              2147483647, scrollbackHelp_())
          if (next != null) s.scrollbackLines = next
        } else if (value == 8) {
          var next = MenuUi.prompt(app, "Modem/Comm Device", "Serial device",
              s.modemDevice, 1024, false, modemDeviceHelp_())
          if (next != null) s.modemDevice = next
        } else if (value == 9) {
          var next = MenuUi.integer(app, "Modem/Comm Rate",
              "Bits per second", s.modemRate, 0, 4294967295,
              modemRateHelp_())
          if (next != null) s.modemRate = next
        } else if (value == 10) {
          var next = MenuUi.prompt(app, "Modem Init String",
              "Initialization string", s.modemInit, 1023, false,
              modemInitHelp_())
          if (next != null) s.modemInit = next
        } else if (value == 11) {
          var next = MenuUi.prompt(app, "Modem Dial String", "Dial string",
              s.modemDial, 1023, false, modemDialHelp_())
          if (next != null) s.modemDial = next
        } else if (value == 12) {
          var next = MenuUi.prompt(app, "List Path", "Path or URI",
              s.listPath, Menu.maxPathLength, false, listPathHelp_())
          if (next != null) s.listPath = next
        } else if (value == 13) {
          var next = MenuUi.prompt(app, "TERM For Shell", "Terminal name",
              s.shellTerm, 30, false, shellTermHelp_())
          if (next != null) s.shellTerm = next
        } else if (value == 14) {
          s.scalingMode = (s.scalingMode + 1) % Menu.scalingModes.count
        } else if (value == 15) {
          s.invertWheel = !s.invertWheel
        } else if (value == 16) {
          var next = MenuUi.integer(app, "Key Derivation Work Factor",
              "Exponent N for scrypt-Nn", s.kdfShift, 8, 24, kdfHelp_())
          if (next != null) s.kdfShift = next
        } else if (value == 17) {
          if (colors_(app, s)) changed = true
        } else if (value == 18 && !connected) {
          if (customMode_(app, s)) changed = true
        }
        if (s.dirty && applySettings_(app, s)) changed = true
      })
      if (picked == null) {
        if (!changed) return false
        if (Host.safeMode || s.save()) return true
        Alert.show(app, "Program Settings",
            "The settings could not be saved.")
        return true
      }
    }
  }

  static applySettings_(app, s) {
    if (s.apply()) {
      app.theme = Theme.current
      return true
    }
    s.reload()
    Alert.show(app, "Program Settings",
        "The setting could not be applied.")
    return false
  }

  static colors_(app, s) {
    var changed = false
    var selected = 21
    while (true) {
      var rows = [
        [21, settingsLine_("Frame Colour", Menu.colors[s.frameColor])],
        [22, settingsLine_("Text Colour", Menu.colors[s.textColor])],
        [23, settingsLine_("Background Colour",
            Menu.backgroundColors[s.backgroundColor])],
        [24, settingsLine_("Inverse Colour",
            Menu.backgroundColors[s.inverseColor])],
        [25, settingsLine_("Lightbar Colour", Menu.colors[s.lightbarColor])],
        [26, settingsLine_("Lightbar Background",
            Menu.backgroundColors[s.lightbarBackgroundColor])]
      ]
      var picked = MenuUi.choice(app, "Classic Theme Colours", rows, selected,
          colorsHelp_(), Fn.new {|value|
        selected = value
        editColor_(app, s, value)
        if (s.dirty && applySettings_(app, s)) changed = true
      })
      if (picked == null) return changed
    }
  }

  static editColor_(app, s, which) {
    var background = which == 23 || which == 24 || which == 26
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

  static customMode_(app, s) {
    var changed = false
    var selected = 0
    while (true) {
      var rows = [
        [0, settingsLine_("Rows", s.customRows)],
        [1, settingsLine_("Columns", s.customColumns)],
        [2, settingsLine_("Font Size", "8x%(s.customFontHeight)")],
        [3, settingsLine_("Aspect Ratio Width", s.customAspectWidth)],
        [4, settingsLine_("Aspect Ratio Height", s.customAspectHeight)]
      ]
      var picked = MenuUi.choice(app, "Custom Screen Mode", rows, selected,
          customModeHelp_(), Fn.new {|value|
        selected = value
        if (value == 0) {
          var next = MenuUi.integer(app, "Custom Rows", "Rows",
              s.customRows, 14, 255, customRowsHelp_())
          if (next != null) s.customRows = next
        } else if (value == 1) {
          var next = MenuUi.integer(app, "Custom Columns", "Columns",
              s.customColumns, 40, 255, customColumnsHelp_())
          if (next != null) s.customColumns = next
        } else if (value == 2) {
          var choices = [[8, "8x8"], [14, "8x14"], [16, "8x16"]]
          var next = MenuUi.choice(app, "Font Size", choices,
              s.customFontHeight, settingHelp_("Custom Font Height"))
          if (next != null) s.customFontHeight = next
        } else if (value == 3) {
          var next = MenuUi.integer(app, "Custom Aspect Width",
              "Aspect width", s.customAspectWidth, 1, 2147483647,
              customAspectHelp_(true))
          if (next != null) s.customAspectWidth = next
        } else if (value == 4) {
          var next = MenuUi.integer(app, "Custom Aspect Height",
              "Aspect height", s.customAspectHeight, 1, 2147483647,
              customAspectHelp_(false))
          if (next != null) s.customAspectHeight = next
        }
        if (s.dirty && applySettings_(app, s)) changed = true
      })
      if (picked == null) return changed
    }
  }

  static audio_(app, s) {
    var changed = false
    var selected = null
    while (true) {
      var rows = []
      for (row in Menu.audioModes) {
        var enabled = (s.audioModes & row[0]) != 0
        rows.add([row[0], "[%(enabled ? "X" : " ")] %(row[1])"])
      }
      var bit = MenuUi.choice(app, "Audio Output Mode", rows, selected,
          audioHelp_(), Fn.new {|value|
        selected = value
        if ((s.audioModes & value) != 0) {
          s.audioModes = s.audioModes & ~value
        } else {
          s.audioModes = s.audioModes | value
        }
        if (applySettings_(app, s)) changed = true
      })
      if (bit == null) return changed
    }
  }

  static locations_(app) {
    var p = Menu.fileLocations
    var text = "`SyncTERM File Locations`\n\n" +
        "**Global Dialing Directory (Read-Only)**\n" +
        "  %(p["globalList"])\n" +
        "**Personal Dialing Directory**\n  %(p["personalList"])\n" +
        "**Configuration File**\n  %(p["configuration"])\n" +
        "**Default download Directory**\n  %(p["download"])\n" +
        "**Cache Directory**\n  %(p["cache"])\n" +
        "**SSH Keys File**\n  %(p["keys"])\n" +
        "**Wren Scripts Directory**\n  %(p["scripts"])\n" +
        "**Theme Directory**\n  %(p["themes"])"
    var viewer = Help.new("File Locations", text)
    viewer.preformatted = true
    viewer.fitToScreen(78, 21)
    app.modal(viewer)
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
    viewer.fitToScreen(60, 21)
    app.modal(viewer)
  }

  static webLists_(app) {
    var changed = false
    var selected = 0
    while (true) {
      var rows = []
      var lists = Menu.webLists
      for (i in 0...lists.count) rows.add([i, lists[i][0]])
      rows.add([-1, ""])
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      var picked = MenuUi.commandChoice(app, "Web Lists", rows, selected,
          webListsHelp_(), commands, Fn.new {|result|
        var command = result[0]
        var index = result[1]
        selected = index
        if (command == "insert" ||
            (command == "select" && index == -1)) {
          if (index == -1) index = lists.count
          var initialName = lists.count == 0 ? "SyncTERM BBS List" : ""
          var name = initialName
          while (true) {
            name = MenuUi.prompt(app, "Add Web List", "Name", name, 1024,
                false, webListsHelp_())
            if (name == null || name.count == 0) break
            if (MenuUi.namesEqual(name, "System List")) {
              Alert.show(app, "Add Web List", "Invalid web-list name.")
              continue
            }
            var duplicate = false
            for (list in lists) {
              if (MenuUi.namesEqual(name, list[0])) duplicate = true
            }
            if (!duplicate) break
            Alert.show(app, "Add Web List", "Duplicate web-list name.")
          }
          if (name == null || name.count == 0) return
          var initialUri = ""
          if (lists.count == 0) {
            initialUri = "http://syncterm.bbsdev.net/syncterm.lst"
          }
          var uri = MenuUi.prompt(app, "Add Web List", "URI", initialUri,
              1024, false, webListsHelp_())
          if (uri == null || uri.count == 0) return
          app.popStatus("Fetching web list")
          var error = Menu.addWebList(name, uri, index)
          app.popStatus(null)
          if (error == null) {
            changed = true
            selected = index
          } else {
            Alert.show(app, "Web List", error)
          }
        } else if (command == "delete") {
          if (Menu.deleteWebList(index)) {
            changed = true
            selected = index < Menu.webLists.count ? index : -1
          } else {
            Alert.show(app, "Web List", "The web list could not be deleted.")
          }
        } else if (command == "select") {
          var uri = MenuUi.prompt(app, "Web List URI", "URI", lists[index][1],
              1024, false, webListsHelp_())
          if (uri != null) {
            if (Menu.updateWebList(index, uri)) {
              changed = true
            } else {
              Alert.show(app, "Web List", "The URI could not be saved.")
            }
          }
        }
      })
      if (picked == null) {
        if (Menu.webListsDirty && !Menu.saveWebLists()) {
          Alert.show(app, "Web Lists",
              "The web-list configuration could not be saved.")
        }
        return changed
      }
    }
  }

  static fonts_(app) {
    var selected = 0
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
      var picked = MenuUi.commandChoice(app, "Font Management", rows,
          selected, fontManagementHelp_(), commands, Fn.new {|result|
        var command = result[0]
        var index = result[1]
        selected = index
        if (command == "insert" ||
            (command == "select" && index == -1)) {
          if (index == -1) index = fonts.count
          if (addFont_(app, fonts, index)) {
            selected = index
            font_(app, index)
          }
        } else if (command == "delete") {
          if (!fonts[index].delete()) {
            Alert.show(app, "Font Management",
                "The font could not be deleted.")
          } else {
            var remaining = Menu.fonts
            if (remaining != null && index >= remaining.count) selected = -1
          }
        } else if (command == "select") {
          font_(app, index)
        }
      })
      if (picked == null) {
        if (Menu.fontsDirty && !Menu.saveFonts()) {
          Alert.show(app, "Font Management",
              "The font configuration could not be saved.")
        }
        return
      }
    }
  }

  static addFont_(app, fonts, index) {
    if (!Menu.canCreateFont) {
      Alert.show(app, "Font Management",
          "All custom font slots are configured.")
      return false
    }
    var name = MenuUi.prompt(app, "Add Font", "Font name", "", 50,
        false, fontNameHelp_())
    if (name == null || name.count == 0) return false
    if (!fontNameFits_(name)) {
      Alert.show(app, "Font Management",
          "The font name is longer than 50 bytes.")
      return false
    }
    if (Menu.createFont(name, index) == null) {
      Alert.show(app, "Font Management",
          "realloc() failure, cannot add font.")
      return false
    }
    return true
  }

  static font_(app, index) {
    var slots = [
      [MenuFontSlot.eightByEight, "8 x 8"],
      [MenuFontSlot.eightByFourteen, "8 x 14"],
      [MenuFontSlot.eightBySixteen, "8 x 16"],
      [MenuFontSlot.twelveByTwenty, "12 x 20"]
    ]
    var selected = -1
    while (true) {
      var fonts = Menu.fonts
      if (fonts == null || fonts.count == 0) return
      index = ((index % fonts.count) + fonts.count) % fonts.count
      var font = fonts[index]
      var rows = [[-1, "Name: %(font.name)"]]
      for (slot in slots) {
        var path = font.path(slot[0])
        rows.add([slot[0], "%(slot[1])  %(path == null ? "<undefined>" : path)"])
      }
      rows.add([-2, ""])
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      commands[0x5B] = ["previous", true]
      commands[0x5D] = ["next", true]
      var deleted = false
      var picked = MenuUi.commandChoice(app, "Font Details", rows, selected,
          fontDetailsHelp_(), commands, Fn.new {|result|
        var command = result[0]
        var value = result[1]
        selected = value
        if (command == "previous") {
          index = index - 1
        } else if (command == "next") {
          index = index + 1
        } else if (command == "insert" ||
            (command == "select" && value == -2)) {
          if (addFont_(app, fonts, index)) selected = -1
        } else if (command == "delete") {
          if (!font.delete()) {
            Alert.show(app, "Font Management",
                "The font could not be deleted.")
          } else {
            deleted = true
          }
        } else if (value == -1) {
          var name = MenuUi.prompt(app, "Rename Font", "Font name", font.name,
              50, false, fontNameHelp_())
          if (name == null || MenuUi.namesEqual(name, font.name)) return
          if (!fontNameFits_(name)) {
            Alert.show(app, "Font Management",
                "The font name is longer than 50 bytes.")
            return
          }
          font.name = name
        } else {
          app.releaseFocus()
          var title = "%(fontSizeName_(value)) %(font.name)"
          var path = font.path(value)
          var file = Host.pickFile(path == null ? "." : path,
              fontMask_(value), FilePickerOptions.allowEntry, title)
          app.restoreFocus()
          if (file != null) font.setFile(value, file)
        }
      })
      if (picked == null || deleted) return
    }
  }

  static encryption_(app) {
    var choices = [
      [[-1, 0], "Change Password"],
      [[MenuEncryption.chacha20, 0], "Encrypt Using ChaCha20"],
      [[MenuEncryption.aes, 128], "Encrypt Using AES-128"],
      [[MenuEncryption.aes, 256], "Encrypt Using AES-256"],
      [[MenuEncryption.none, 0], "Decrypt"]
    ]
    var title = Menu.encryptionName
    if (Menu.encryptionAlgorithm != MenuEncryption.none) {
      title = "Currently %(title)"
    }
    var changed = false
    MenuUi.choice(app, title, choices, null, encryptionHelp_(), Fn.new {|picked|
      var algorithm = picked[0]
      var keySize = picked[1]
      var password = null
      if (algorithm == -1) {
        algorithm = Menu.encryptionAlgorithm
        keySize = Menu.encryptionKeySize
        password = MenuUi.prompt(app, "List Encryption", "New password", "",
            1023, true, encryptionHelp_())
        if (password == null || password.count == 0) return
      }
      if (Host.safeMode) return
      if (password == null && algorithm != MenuEncryption.none &&
          Menu.encryptionAlgorithm == MenuEncryption.none) {
        password = MenuUi.prompt(app, "List Encryption", "Password", "",
            1023, true, encryptionHelp_())
        if (password == null || password.count == 0) return
      }
      if (!Menu.setEncryption(algorithm, keySize, password)) {
        Alert.show(app, "List Encryption",
            "The personal directory could not be rewritten.")
        return
      }
      if (algorithm == MenuEncryption.none) {
        __directoryPassword = null
        __passwordChanged = true
      } else if (password != null) {
        __directoryPassword = password
        __passwordChanged = true
      }
      changed = true
    })
    return changed
  }
}
