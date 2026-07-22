// Menu-VM self-tests for per-entry Wren script configuration.

import "syncterm" for Key, KeyEvent
import "syncterm_menu" for Menu, MenuReadStatus
import "menu_bbs_editor" for ScriptListPane

class FakeApp {
  construct new() { _popped = 0 }
  popModal() { _popped = _popped + 1 }
  popped { _popped }
}

class MenuTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== menu_bbs_editor self-test starting ===")

    testMoveCommit_()
    testMoveCancel_()
    testDelete_()
    testDiscovery_()
    testDefaultsRejectScripts_()
    testPersistence_()

    var total = __pass + __fail
    System.print("=== menu_bbs_editor: %(total) tests, %(__pass) pass, " +
        "%(__fail) fail ===")
    return [__pass, __fail]
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  static same_(left, right) {
    if (left.count != right.count) return false
    for (i in 0...left.count) {
      if (left[i] != right[i]) return false
    }
    return true
  }

  static pane_(scripts, changes) {
    return ScriptListPane.new(FakeApp.new(), scripts,
        Fn.new { changes[0] = changes[0] + 1 })
  }

  static testMoveCommit_() {
    var scripts = ["alpha", "beta", "gamma"]
    var changes = [0]
    var pane = pane_(scripts, changes)
    pane.selected = 1
    pane.handle(KeyEvent.new(Key.enter))
    var tagged = pane.moving && pane.tagged.count == 1 &&
        pane.tagged[0] == 1
    pane.handle(KeyEvent.new(Key.down))
    pane.handle(KeyEvent.new(Key.enter))
    check_(tagged && !pane.moving &&
           same_(scripts, ["alpha", "gamma", "beta"]) &&
           changes[0] == 1,
           "ScriptListPane: Enter tags and arrows move in stored order")
  }

  static testMoveCancel_() {
    var scripts = ["alpha", "beta", "gamma"]
    var changes = [0]
    var pane = pane_(scripts, changes)
    pane.selected = 1
    pane.handle(KeyEvent.new(Key.enter))
    pane.handle(KeyEvent.new(Key.up))
    pane.handle(KeyEvent.new(Key.escape))
    check_(!pane.moving && pane.selected == 1 &&
           same_(scripts, ["alpha", "beta", "gamma"]) && changes[0] == 0,
           "ScriptListPane: Escape restores the pre-move order")
  }

  static testDelete_() {
    var scripts = ["alpha", "beta"]
    var changes = [0]
    var pane = pane_(scripts, changes)
    var oldHeight = pane.bounds.h
    pane.selected = 0
    pane.handle(KeyEvent.new(Key.delete))
    check_(same_(scripts, ["beta"]) && changes[0] == 1 &&
           pane.bounds.h == oldHeight - 1,
           "ScriptListPane: Delete removes the script and refits the pane")
  }

  static testDiscovery_() {
    var modules = Menu.scriptModules
    var available = pane_([], [0]).available_()
    check_(modules.indexOf("menu_bbs_editor") >= 0 &&
           modules.indexOf("syncterm_menu") < 0 &&
           modules.indexOf("syncterm_picker") < 0 &&
           modules.indexOf("picker_bootstrap") < 0 &&
           Menu.wrenScriptsMaxLength == 1023 && available.count > 0,
           "Menu.scriptModules: lists root modules except reserved VMs")
  }

  static testDefaultsRejectScripts_() {
    var loaded = Menu.load(null) == MenuReadStatus.ok
    var defaults = loaded ? Menu.defaults : null
    var fiber = Fiber.new {
      defaults.wrenScripts = ["ui_list"]
    }
    if (defaults != null) fiber.try()
    check_(defaults != null && fiber.error != null &&
           defaults.wrenScripts.count == 0,
           "BBS.wrenScripts: defaults cannot activate connection scripts")
  }

  static testPersistence_() {
    var loaded = Menu.load(null) == MenuReadStatus.ok
    var entry = loaded ? Menu.create("Wren Script Test") : null
    var created = entry != null
    var assigned = false
    var saved = false
    if (created) {
      entry.wrenScripts = ["ui_list", "menu_bbs_editor", "ui_list"]
      assigned = same_(entry.wrenScripts, ["ui_list", "menu_bbs_editor"])
      saved = entry.save()
    }

    var reloaded = created && assigned && saved &&
        Menu.load(null) == MenuReadStatus.ok
    var found = null
    if (reloaded) {
      for (candidate in Menu.entries) {
        if (candidate.name == "Wren Script Test") found = candidate
      }
    }
    var retained = found != null && same_(found.wrenScripts,
        ["ui_list", "menu_bbs_editor"])
    if (!(loaded && created && assigned && saved && reloaded && retained)) {
      System.print("  persistence state: loaded=%(loaded), " +
          "created=%(created), assigned=%(assigned), saved=%(saved), " +
          "reloaded=%(reloaded), found=%(found != null), " +
          "scripts=%(found == null ? [] : found.wrenScripts)")
    }
    if (found != null) found.delete()
    check_(loaded && created && assigned && saved && reloaded && retained,
           "BBS.wrenScripts: ordered modules survive save and reload")
  }
}
