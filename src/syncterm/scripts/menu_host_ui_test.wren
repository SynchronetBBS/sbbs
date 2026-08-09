// Menu-VM self-tests for the C-hosted status presentation.

import "menu_host_ui" for MenuHostUI

class MenuTest {
  static run() {
    var pass = 0
    var fail = 0

    var checks = [
      [MenuHostUI.statusWidth_(80, "Status", ["Connecting..."]) == 17,
       "single status fits its text"],
      [MenuHostUI.statusWidth_(80, "Longer title", ["Short"]) == 16,
       "title can determine width"],
      [MenuHostUI.statusWidth_(80, "Status",
          ["Short", "12345678901234567890"]) == 24,
       "longest row determines width"],
      [MenuHostUI.statusWidth_(40, "Status",
          ["1234567890123456789012345678901234567890"]) == 36,
       "width retains screen margins"]
    ]

    for (check in checks) {
      if (check[0]) {
        pass = pass + 1
      } else {
        fail = fail + 1
        System.print("  FAIL %(check[1])")
      }
    }
    return [pass, fail]
  }
}
