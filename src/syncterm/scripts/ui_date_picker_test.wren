// Self-tests for ui_date_picker.

import "syncterm" for Key, KeyEvent, Mouse, MouseEvent
import "ui_date_picker" for CalendarDate, DatePicker
import "ui_widget" for Rect

class UiDatePickerTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_date_picker self-test starting ===")

    testCalendarArithmetic_()
    testCivilRoundTrips_()
    testWeekdays_()
    testPickerDefaults_()
    testSetDateClamps_()
    testOnChange_()
    testDayKeyboardNavigation_()
    testMonthKeyboardNavigation_()
    testHomeEnd_()
    testAcceptKeys_()
    testHeaderAndWheelMouse_()
    testNeighborDateMouse_()
    testDoubleClickAccepts_()
    testDraw_()
    testOutsideMouseFallsThrough_()

    var total = __pass + __fail
    System.print("=== ui_date_picker: %(total) tests, %(__pass) pass, " +
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

  static testCalendarArithmetic_() {
    check_(CalendarDate.monthNames[0] == "January" &&
        CalendarDate.monthNames[11] == "December" &&
        CalendarDate.daysInMonth(2024, 2) == 29 &&
        CalendarDate.daysInMonth(2023, 2) == 28 &&
        CalendarDate.daysInMonth(1900, 2) == 28 &&
        CalendarDate.daysInMonth(2000, 2) == 29 &&
        CalendarDate.iso(1, 2, 3) == "0001-02-03" &&
        CalendarDate.isoFromTimestamp(0) == "1970-01-01",
        "CalendarDate: names, leap years, and ISO formatting")
  }

  static testCivilRoundTrips_() {
    var dates = [
      [1, 1, 1],
      [1582, 10, 15],
      [1970, 1, 1],
      [2000, 2, 29],
      [2026, 7, 23],
      [9999, 12, 31]
    ]
    var valid = true
    for (date in dates) {
      var serial = CalendarDate.daysFromCivil(date[0], date[1], date[2])
      var result = CalendarDate.civilFromDays(serial)
      if (result[0] != date[0] || result[1] != date[1] ||
          result[2] != date[2]) valid = false
    }
    check_(valid, "CalendarDate: civil day conversion round-trips")
  }

  static testWeekdays_() {
    check_(CalendarDate.weekday(1970, 1, 1) == 4 &&
        CalendarDate.weekday(2026, 7, 23) == 4 &&
        CalendarDate.weekday(2025, 3, 1) == 6,
        "CalendarDate.weekday: Sunday-zero weekday indices")
  }

  static picker_(year, month, day) {
    var picker = DatePicker.new(year, month, day)
    picker.bounds = Rect.new(2, 5, 28, 8)
    return picker
  }

  static testPickerDefaults_() {
    var picker = DatePicker.new(2026, 7, 23)
    check_(picker.year == 2026 && picker.month == 7 && picker.day == 23 &&
        picker.date[0] == 2026 && picker.value == "2026-07-23" &&
        picker.preferredWidth == 28 && picker.preferredHeight == 8 &&
        picker.focusable && !picker.cursorVisible,
        "DatePicker: initial date, value, and size")
  }

  static testSetDateClamps_() {
    var picker = DatePicker.new(2024, 1, 31)
    picker.setDate(2023, 2, 31)
    var monthClamp = picker.value == "2023-02-28"
    picker.setDate(0, 0, 0)
    var lowClamp = picker.value == "0001-01-01"
    picker.setDate(10000, 13, 40)
    check_(monthClamp && lowClamp && picker.value == "9999-12-31",
        "DatePicker.setDate: calendar and range clamping")
  }

  static testOnChange_() {
    var picker = DatePicker.new(2026, 7, 23)
    var calls = 0
    var received = null
    picker.onChange = Fn.new {|year, month, day|
      calls = calls + 1
      received = [year, month, day]
    }
    picker.setDate(2026, 7, 23)
    picker.setDate(2026, 7, 24)
    check_(calls == 1 && received[0] == 2026 &&
        received[1] == 7 && received[2] == 24,
        "DatePicker.onChange: fires only for a changed date")
  }

  static testDayKeyboardNavigation_() {
    var picker = picker_(2025, 3, 1)
    picker.handle(KeyEvent.new(Key.left))
    var previous = picker.value == "2025-02-28"
    picker.handle(KeyEvent.new(Key.right))
    picker.handle(KeyEvent.new(Key.down))
    var nextWeek = picker.value == "2025-03-08"
    picker.handle(KeyEvent.new(Key.up))
    check_(previous && nextWeek && picker.value == "2025-03-01",
        "DatePicker: arrows move by day and week across months")
  }

  static testMonthKeyboardNavigation_() {
    var picker = picker_(2024, 2, 29)
    picker.handle(KeyEvent.new(Key.pageDown))
    var next = picker.value == "2024-03-29"
    picker.handle(KeyEvent.new(Key.pageUp))
    picker.moveMonths(12)
    check_(next && picker.value == "2025-02-28",
        "DatePicker: month movement retains or clamps day")
  }

  static testHomeEnd_() {
    var picker = picker_(2026, 4, 17)
    picker.handle(KeyEvent.new(Key.home))
    var home = picker.value == "2026-04-01"
    picker.handle(KeyEvent.new(Key.end))
    check_(home && picker.value == "2026-04-30",
        "DatePicker: Home and End select month boundaries")
  }

  static testAcceptKeys_() {
    var picker = picker_(2026, 7, 23)
    var calls = 0
    picker.onAccept = Fn.new { calls = calls + 1 }
    picker.handle(KeyEvent.new(Key.enter))
    picker.handle(KeyEvent.new(0x20))
    check_(calls == 2, "DatePicker.onAccept: Enter and Space accept")
  }

  static testHeaderAndWheelMouse_() {
    var picker = picker_(2025, 3, 1)
    // Local right header arrow begins at x=25; bounds.x=2 => screen x=27.
    picker.handle(MouseEvent.new(Mouse.button1Click, 27, 5))
    var header = picker.value == "2025-04-01"
    picker.handle(MouseEvent.new(Mouse.wheelUpPress, 10, 8))
    check_(header && picker.value == "2025-03-01",
        "DatePicker: header arrows and wheel change month")
  }

  static testNeighborDateMouse_() {
    var picker = picker_(2025, 3, 1)
    // March 2025 begins Saturday, so grid cell 0 is February 23.
    picker.handle(MouseEvent.new(Mouse.button1Click, 3, 7))
    check_(picker.value == "2025-02-23",
        "DatePicker: neighboring-month dates are selectable")
  }

  static testDoubleClickAccepts_() {
    var picker = picker_(2025, 3, 1)
    var accepted = false
    picker.onAccept = Fn.new { accepted = true }
    picker.handle(MouseEvent.new(Mouse.button1DblClick, 3, 7))
    check_(accepted && picker.value == "2025-02-23",
        "DatePicker: double-click selects and accepts")
  }

  static testDraw_() {
    var picker = picker_(2025, 3, 1)
    var surface = picker.draw()
    var title = surface.cellAt(9, 0).ch == "M"
    var weekdays = surface.cellAt(1, 1).ch == "S" &&
        surface.cellAt(25, 1).ch == "S"
    var otherStyle = surface.cellAt(0, 2).legacyAttr ==
        picker.style("list.item.disabled").legacyAttr
    var selectedStyle = surface.cellAt(24, 2).legacyAttr ==
        picker.style("list.item.focused").legacyAttr
    check_(title && weekdays && otherStyle && selectedStyle,
        "DatePicker.draw: month, weekdays, neighbor, and selection styles")
  }

  static testOutsideMouseFallsThrough_() {
    var picker = picker_(2025, 3, 1)
    var handled = picker.handle(
        MouseEvent.new(Mouse.button1Click, 1, 1))
    check_(!handled && picker.value == "2025-03-01",
        "DatePicker: outside mouse event falls through")
  }
}
