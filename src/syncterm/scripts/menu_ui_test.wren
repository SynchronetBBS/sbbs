// Self-tests for menu_ui.

import "ui_widget" for Rect
import "menu_ui" for ChoiceViewState, MenuUi

class MenuUiTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== menu_ui self-test starting ===")

    testChoiceViewState_()
    testBrowserMaximumHeight_()

    var total = __pass + __fail
    System.print("=== menu_ui: %(total) tests, %(__pass) pass, " +
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

  static testChoiceViewState_() {
    var state = ChoiceViewState.new()
    var first = MenuUi.choiceList_(state)
    first.bounds = Rect.new(1, 1, 20, 5)
    first.items = (0...20).map {|i| "old %(i)" }.toList
    first.selected = 10
    first.scrollTop = 8

    var second = MenuUi.choiceList_(state)
    second.items = (0...20).map {|i| "new %(i)" }.toList
    var fresh = MenuUi.choiceList_(null)
    check_(first == second && fresh != first && second.selected == 10 &&
           second.scrollTop == 8,
           "ChoiceViewState: repeated choices retain ListView viewport")
  }

  static testBrowserMaximumHeight_() {
    check_(MenuUi.browserMaximumHeight_(25, 0) == 21 &&
           MenuUi.browserMaximumHeight_(25, 1) == 20,
           "MenuUi browser bottom clearance reserves the shadow row")
  }
}
