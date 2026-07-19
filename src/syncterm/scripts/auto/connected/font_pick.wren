// font_pick.wren -- Wren replacement for the C-side font_control()
// uifc dialog.  Driven by Hook.onKey(Key.altF) in keys_default.wren
// and the online menu's "Font Setup" entry.
//
// Two flows:
//
//   - Slot picker (default).  ListView of every populated entry in
//     the conio_fontdata table.  Selection writes CTerm.altFont,
//     which calls setfont() and stashes the new slot in cterm's
//     altfont[0] cache.
//   - Load from file (Insert key).  Pops Host.pickFile, then
//     Font.load(file) — same path-as-File consent shape as
//     Transfer.upload's filepick.
//
// Gated by ScreenSupports.fontSelection: text-mode backends (curses,
// ANSI, conio) can't change the font at runtime; pop an Alert and
// bail.

import "syncterm" for CTerm, Font, Host, Input, Key, Screen, ScreenSupports
import "ui_app"     for App
import "ui_pane"    for Pane
import "ui_list"    for ListView
import "ui_popup"   for Alert
import "ui_widget"  for Rect

class FontApp {
  static run() {
    Fiber.new {
      Screen.modalRun(Fn.new {
        if (Host.safeMode) return
        if (!ScreenSupports.fontSelection) {
          showUnsupported_()
          return
        }
        runPicker_()
      })
      Input.setupMouseEvents()
    }.call()
  }

  static showUnsupported_() {
    var app = App.new()
    var msg = "Font cannot be changed in the current video output mode."
    Alert.runStandalone(app, msg)
  }

  // Build the (slot, name) list — Font.count includes empty entries
  // (Font.name returns null), filter those out so the picker shows
  // only populated slots.
  static buildEntries_() {
    var labels = []
    var slots  = []
    var n = Font.count
    var i = 0
    while (i < n) {
      var name = Font.name(i)
      if (name != null && name != "") {
        labels.add(name)
        slots.add(i)
      }
      i = i + 1
    }
    return [labels, slots]
  }

  static runPicker_() {
    var entries = buildEntries_()
    var labels  = entries[0]
    var slots   = entries[1]
    if (labels.isEmpty) return

    // Pre-select whichever entry matches the current altFont slot.
    var current = CTerm.altFont
    var preselect = 0
    var k = 0
    while (k < slots.count) {
      if (slots[k] == current) {
        preselect = k
        break
      }
      k = k + 1
    }

    var picked = -1
    var loadInsteadOfSelect = false
    var app  = App.new()
    var pane = Pane.new()
    pane.title    = "Font Setup"
    pane.helpText =
      "# Font Setup\n" +
      "\n" +
      "Change the current font.  Font must support the current video mode:\n" +
      "\n" +
      "- **8x8**  used for screen modes with 35 or more lines and all C64/C128 modes\n" +
      "- **8x14** used for screen modes with 28 and 34 lines\n" +
      "- **8x16** used for screen modes with 30 lines or fewer than 28 lines\n" +
      "\n" +
      "Press **Insert** to load a font file from disk."
    pane.focused  = true
    pane.shadow   = true
    pane.onClose  = Fn.new { app.quit() }
    app.root.add(pane)

    var list = ListView.new()
    list.items    = labels
    list.selected = preselect
    list.onSelect = Fn.new { |i, item|
      picked = i
      app.quit()
    }
    pane.add(list)
    pane.fitContentToScreen()
    // Pin to the top row, horizontally centered.  Centering vertically
    // (the default centerOnScreen) puts the pane right over the cterm
    // content the user is presumably trying to read while changing
    // fonts; top-aligned leaves the bottom of the screen visible.
    pane.bounds = Rect.new(pane.bounds.x, 1, pane.bounds.w, pane.bounds.h)
    if (pane.children.count > 0) pane.children[0].bounds = pane.innerBounds
    app.bind(Key.escape, Fn.new { |k| app.quit() })
    if (ScreenSupports.loadableFonts) {
      app.bind(Key.insert, Fn.new { |k|
        loadInsteadOfSelect = true
        app.quit()
      })
    }
    app.run()

    if (loadInsteadOfSelect) {
      loadFromFile_()
      return
    }
    if (picked < 0) return
    CTerm.altFont = slots[picked]
  }

  static loadFromFile_() {
    var file = Host.pickFile(null, null, 1)
    if (file == null) return
    Font.load(file)
  }
}
