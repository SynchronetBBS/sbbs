import "syncterm" for Host, Screen
import "syncterm_menu" for Menu, MenuEncryption, MenuFontSlot
import "menu_ui" for MenuUi
import "menu_bbs_editor" for BbsEditor
import "ui_popup" for Alert, Confirm
import "ui_help" for Help
import "ui_widget" for Rect

class SettingsMenu {
  static directoryPassword { __directoryPassword }
  static passwordChanged { __passwordChanged }

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
          rows(connected), null)
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
        Menu.currentScreenMode)
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

  static pickSetting_(app, title, rows, current) {
    return MenuUi.choice(app, title, rows, current)
  }

  static program_(app) {
    var s = Menu.settings
    while (true) {
      var rows = [[0, "Save Changes"]]
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

      var picked = MenuUi.choice(app, "Program Settings", rows, null)
      if (picked == null) {
        s.reload()
        return false
      }
      if (picked == 0) {
        if (s.save()) return true
        Alert.show(app, "Program Settings", "The settings could not be validated or saved.")
      } else if (picked == 1) {
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
            s.scrollbackLines, 1, 10000000)
        if (value != null) s.scrollbackLines = value
      } else if (picked == 8) {
        var value = pickSetting_(app, "Scaling Mode", Menu.scalingModes, s.scalingMode)
        if (value != null) s.scalingMode = value
      } else if (picked == 9) {
        var value = MenuUi.prompt(app, "Personal List", "Path or URI", s.listPath, 1024, false)
        if (value != null) s.listPath = value
      } else if (picked == 10) {
        var value = MenuUi.prompt(app, "Shell TERM", "Terminal name", s.shellTerm, 255, false)
        if (value != null) s.shellTerm = value
      } else if (picked == 11) {
        var value = MenuUi.prompt(app, "Modem Device", "Serial device", s.modemDevice, 255, false)
        if (value != null) s.modemDevice = value
      } else if (picked == 12) {
        var value = MenuUi.prompt(app, "Modem Init", "Initialization string", s.modemInit, 255, false)
        if (value != null) s.modemInit = value
      } else if (picked == 13) {
        var value = MenuUi.prompt(app, "Modem Dial", "Dial string", s.modemDial, 255, false)
        if (value != null) s.modemDial = value
      } else if (picked == 14) {
        var value = MenuUi.integer(app, "Modem Rate", "Bits per second", s.modemRate, 1, 2147483647)
        if (value != null) s.modemRate = value
      } else if (picked == 15) {
        var value = MenuUi.integer(app, "KDF Work Factor", "Exponent N for scrypt-Nn", s.kdfShift, 8, 24)
        if (value != null) s.kdfShift = value
      } else if (picked == 16) {
        var value = MenuUi.integer(app, "Custom Columns", "Columns", s.customColumns, 40, 255)
        if (value != null) s.customColumns = value
      } else if (picked == 17) {
        var value = MenuUi.integer(app, "Custom Rows", "Rows", s.customRows, 14, 255)
        if (value != null) s.customRows = value
      } else if (picked == 18) {
        var choices = [[8, "8"], [14, "14"], [16, "16"]]
        var value = pickSetting_(app, "Custom Font Height", choices, s.customFontHeight)
        if (value != null) s.customFontHeight = value
      } else if (picked == 19) {
        var value = MenuUi.integer(app, "Custom Aspect Width", "Aspect width", s.customAspectWidth, 1, 255)
        if (value != null) s.customAspectWidth = value
      } else if (picked == 20) {
        var value = MenuUi.integer(app, "Custom Aspect Height", "Aspect height", s.customAspectHeight, 1, 255)
        if (value != null) s.customAspectHeight = value
      } else if (picked >= 21 && picked <= 26) {
        editColor_(app, s, picked)
      } else if (picked == 27) {
        audio_(app, s)
      }
    }
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
    var value = MenuUi.choice(app, "Color", rows, current)
    if (value == null) return
    if (which == 21) s.frameColor = value
    if (which == 22) s.textColor = value
    if (which == 23) s.backgroundColor = value
    if (which == 24) s.inverseColor = value
    if (which == 25) s.lightbarColor = value
    if (which == 26) s.lightbarBackgroundColor = value
  }

  static audio_(app, s) {
    while (true) {
      var rows = []
      for (row in Menu.audioModes) {
        var enabled = (s.audioModes & row[0]) != 0
        rows.add([row[0], "[%(enabled ? "X" : " ")] %(row[1])"])
      }
      var bit = MenuUi.choice(app, "Audio Output", rows, null)
      if (bit == null) return
      if ((s.audioModes & bit) != 0) {
        s.audioModes = s.audioModes & ~bit
      } else {
        s.audioModes = s.audioModes | bit
      }
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
      var rows = [[-1, "Add Web List"]]
      var lists = Menu.webLists
      for (i in 0...lists.count) rows.add([i, lists[i][0]])
      var picked = MenuUi.choice(app, "Web Lists", rows, null)
      if (picked == null) return changed
      if (picked == -1) {
        var name = MenuUi.prompt(app, "Add Web List", "Name", "", 255, false)
        if (name == null) continue
        var uri = MenuUi.prompt(app, "Add Web List", "URI", "", 1024, false)
        if (uri == null) continue
        app.popStatus("Fetching web list")
        var error = Menu.addWebList(name, uri, lists.count)
        app.popStatus(null)
        if (error == null) {
          changed = true
        } else {
          Alert.show(app, "Web List", error)
        }
      } else {
        if (webList_(app, picked, lists[picked])) changed = true
      }
    }
  }

  static webList_(app, index, row) {
    var action = MenuUi.choice(app, row[0],
        [[0, "Edit URI"], [1, "Refresh Cache"], [2, "Delete"]], null)
    if (action == null) return false
    if (action == 0) {
      var uri = MenuUi.prompt(app, "Web List URI", "URI", row[1], 1024, false)
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
      var rows = [[-2, "Save Changes"], [-1, "Add Font"]]
      for (i in 0...fonts.count) rows.add([i, fonts[i].name])
      var picked = MenuUi.choice(app, "Font Management", rows, null)
      if (picked == null) {
        if (Menu.fontsDirty && Confirm.show(app, "Discard unsaved font changes?")) Menu.reloadFonts()
        return
      }
      if (picked == -2) {
        if (!Menu.saveFonts()) Alert.show(app, "Font Management", "The font configuration could not be saved.")
      } else if (picked == -1) {
        var name = MenuUi.prompt(app, "Add Font", "Font name", "", 79, false)
        if (name != null && Menu.createFont(name, fonts.count) == null) {
          Alert.show(app, "Font Management", "The font could not be added.")
        }
      } else {
        font_(app, fonts[picked])
      }
    }
  }

  static font_(app, font) {
    var slots = [
      [MenuFontSlot.eightByEight, "8 x 8"],
      [MenuFontSlot.eightByFourteen, "8 x 14"],
      [MenuFontSlot.eightBySixteen, "8 x 16"],
      [MenuFontSlot.twelveByTwenty, "12 x 20"]
    ]
    while (true) {
      var rows = [[-2, "Rename"], [-1, "Delete Font"]]
      for (slot in slots) {
        var path = font.path(slot[0])
        rows.add([slot[0], "%(slot[1])  %(path == null ? "<none>" : path)"])
      }
      var picked = MenuUi.choice(app, font.name, rows, null)
      if (picked == null) return
      if (picked == -2) {
        var name = MenuUi.prompt(app, "Rename Font", "Font name", font.name, 79, false)
        if (name != null) font.name = name
      } else if (picked == -1) {
        if (Confirm.show(app, "Delete %(font.name)?")) {
          font.delete()
          return
        }
      } else {
        var action = MenuUi.choice(app, "Font File",
            [[0, "Choose File"], [1, "Clear File"]], null)
        if (action == 0) {
          app.releaseFocus()
          var file = Host.pickFile(null, null, 1)
          app.restoreFocus()
          if (file != null && !font.setFile(picked, file)) {
            Alert.show(app, "Font File", "The selected file has the wrong size.")
          }
        } else if (action == 1) {
          font.clearFile(picked)
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
    var picked = MenuUi.choice(app, "List Encryption", choices, null)
    if (picked == null) return false
    var password = null
    if (picked[0] != MenuEncryption.none) {
      password = MenuUi.prompt(app, "List Encryption", "New password", "", 1023, true)
      if (password == null || password.count == 0) return false
      var verify = MenuUi.prompt(app, "List Encryption", "Confirm password", "", 1023, true)
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
