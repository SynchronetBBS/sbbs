// Run with run_wren.sh --menu-no-cache and allocator poisoning enabled.
import "syncterm_menu" for Menu

class MenuTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== menu_web_lists self-test starting ===")
    check_(Menu.webLists.count == 0, "new configuration has no web lists")
    check_(Menu.addWebList("First", "http://example.invalid/first.lst", 0) == null,
        "add the first list from a NULL array")
    rows_(["First"])
    check_(Menu.webListsDirty, "adding the first list marks it dirty")

    check_(Menu.addWebList("Last", "http://example.invalid/last.lst", 1) == null,
        "append to an existing list")
    check_(Menu.addWebList("Head", "http://example.invalid/head.lst", 0) == null,
        "insert before the first entry")
    check_(Menu.addWebList("Middle", "http://example.invalid/middle.lst", 2) == null,
        "insert between existing entries")
    rows_(["Head", "First", "Middle", "Last"])

    check_(Menu.addWebList("first", "http://example.invalid/duplicate.lst", 0) is String,
        "reject duplicate names without changing the list")
    rows_(["Head", "First", "Middle", "Last"])
    check_(Menu.updateWebList(1, "http://example.invalid/updated.lst"),
        "update an existing URI")
    check_(Menu.webLists[1][1] == "http://example.invalid/updated.lst",
        "updated URI is readable")

    check_(Menu.deleteWebList(0), "delete the head")
    rows_(["First", "Middle", "Last"])
    check_(Menu.deleteWebList(1), "delete the middle")
    rows_(["First", "Last"])
    check_(Menu.deleteWebList(1), "delete the tail")
    check_(Menu.deleteWebList(0), "delete the final entry")
    rows_([])
    check_(Menu.addWebList("Again", "http://example.invalid/again.lst", 0) == null,
        "add after deleting every entry")
    rows_(["Again"])
    check_(Menu.deleteWebList(0), "remove the final test entry")
    rows_([])
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

  static rows_(names) {
    var rows = Menu.webLists
    check_(rows.count == names.count, "web-list count matches")
    for (i in 0...names.count) {
      check_(rows[i][0] == names[i], "web-list order matches at %(i)")
    }
  }
}
