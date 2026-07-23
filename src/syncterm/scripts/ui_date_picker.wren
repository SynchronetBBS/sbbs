// SyncTERM Wren UI library — DatePicker.
//
// Embeddable Sunday-through-Saturday month calendar. The caller owns
// dialog buttons and supplies the initial date:
//
//   var picker = DatePicker.new(2026, 7, 23)
//   picker.bounds = Rect.new(x, y, picker.preferredWidth,
//       picker.preferredHeight)
//   picker.onChange = Fn.new {|year, month, day| /* preview */ }
//   picker.onAccept = Fn.new { /* commit picker.value */ }
//
// Arrow keys move by a day or week. Page Up / Page Down changes month;
// Home / End chooses the first / last day. Enter or Space accepts.
// Mouse clicks select any visible date, including neighboring-month
// dates; double-click also accepts. Header arrows and the wheel change
// month.
//
// Theme roles consulted:
//   button
//   list.item
//   list.item.focused
//   list.item.disabled

import "syncterm" for Key, KeyEvent, Mouse, MouseEvent
import "ui_draw" for Painter
import "ui_widget" for Rect, Widget

class CalendarDate {
  static monthNames {
    return ["January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"]
  }

  static daysInMonth(year, month) {
    var days = 31
    if ([4, 6, 9, 11].contains(month)) days = 30
    if (month == 2) {
      var leap = year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)
      days = leap ? 29 : 28
    }
    return days
  }

  static iso(year, month, day) {
    var yearText = year.toString
    while (yearText.count < 4) yearText = "0" + yearText
    var monthText = month < 10 ? "0%(month)" : month.toString
    var dayText = day < 10 ? "0%(day)" : day.toString
    return "%(yearText)-%(monthText)-%(dayText)"
  }

  static fromTimestamp(timestamp) {
    return civilFromDays((timestamp / 86400).floor)
  }

  static isoFromTimestamp(timestamp) {
    var date = fromTimestamp(timestamp)
    return iso(date[0], date[1], date[2])
  }

  // Howard Hinnant's civil-calendar conversion, adapted for Wren's
  // floor-based arithmetic. The result is days relative to 1970-01-01.
  static daysFromCivil(year, month, day) {
    var adjustedYear = year
    if (month <= 2) adjustedYear = adjustedYear - 1
    var era = (adjustedYear / 400).floor
    var yearOfEra = adjustedYear - era * 400
    var monthPrime = month > 2 ? month - 3 : month + 9
    var dayOfYear = ((153 * monthPrime + 2) / 5).floor + day - 1
    var dayOfEra = yearOfEra * 365 + (yearOfEra / 4).floor -
        (yearOfEra / 100).floor + dayOfYear
    return era * 146097 + dayOfEra - 719468
  }

  static civilFromDays(epochDays) {
    var days = epochDays + 719468
    var era = (days / 146097).floor
    var dayOfEra = days - era * 146097
    var yearOfEra = (dayOfEra - (dayOfEra / 1460).floor +
        (dayOfEra / 36524).floor - (dayOfEra / 146096).floor) / 365
    yearOfEra = yearOfEra.floor
    var year = yearOfEra + era * 400
    var dayOfYear = dayOfEra - 365 * yearOfEra -
        (yearOfEra / 4).floor + (yearOfEra / 100).floor
    var monthPrime = ((5 * dayOfYear + 2) / 153).floor
    var day = dayOfYear - ((153 * monthPrime + 2) / 5).floor + 1
    var month = monthPrime < 10 ? monthPrime + 3 : monthPrime - 9
    if (month <= 2) year = year + 1
    return [year, month, day]
  }

  static weekday(year, month, day) {
    var weekday = (daysFromCivil(year, month, day) + 4) % 7
    return weekday < 0 ? weekday + 7 : weekday
  }
}

class DatePicker is Widget {
  construct new(year, month, day) {
    super()
    _year = 1
    _month = 1
    _day = 1
    _onChange = null
    _onAccept = null
    setDate(year, month, day)
    helpText = "# Month Calendar\n\nArrow keys move by a day or week. " +
        "Page Up / Page Down changes month. Home / End selects the first " +
        "or last day of the month. Enter chooses the highlighted date."
    keyHints = [
      ["Arrows", "Move date"],
      ["PgUp/PgDn", "Change month"],
      ["Enter", "Choose"]
    ]
  }

  preferredWidth { 28 }
  preferredHeight { 8 }
  cursorVisible { false }

  year { _year }
  month { _month }
  day { _day }
  date { [_year, _month, _day] }
  value { CalendarDate.iso(_year, _month, _day) }
  onChange=(fn) { _onChange = fn }
  onAccept=(fn) { _onAccept = fn }

  setDate(year, month, day) {
    var nextYear = year.max(1).min(9999)
    var nextMonth = month.max(1).min(12)
    var nextDay = day.max(1).min(
        CalendarDate.daysInMonth(nextYear, nextMonth))
    if (nextYear == _year && nextMonth == _month && nextDay == _day) return
    _year = nextYear
    _month = nextMonth
    _day = nextDay
    markDirty()
    if (_onChange != null) _onChange.call(_year, _month, _day)
  }

  moveDays(offset) {
    var serial = CalendarDate.daysFromCivil(_year, _month, _day) + offset
    var minimum = CalendarDate.daysFromCivil(1, 1, 1)
    var maximum = CalendarDate.daysFromCivil(9999, 12, 31)
    serial = serial.max(minimum).min(maximum)
    var next = CalendarDate.civilFromDays(serial)
    setDate(next[0], next[1], next[2])
  }

  moveMonths(offset) {
    var total = (_year - 1) * 12 + _month - 1 + offset
    total = total.max(0).min(9999 * 12 - 1)
    var year = (total / 12).floor + 1
    var month = total % 12 + 1
    setDate(year, month, _day)
  }

  accept_() {
    if (_onAccept != null) _onAccept.call()
  }

  gridX_ {
    return ((bounds.w - preferredWidth) / 2).floor.max(0)
  }

  gridStart_ {
    var first = CalendarDate.daysFromCivil(_year, _month, 1)
    return first - CalendarDate.weekday(_year, _month, 1)
  }

  onPaint_() {
    var sf = surface
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))

    var gridX = gridX_
    var buttonStyle = style("button")
    Painter.text(sf, gridX, 0, "[<]", buttonStyle, 3)
    Painter.text(sf, gridX + 25, 0, "[>]", buttonStyle, 3)
    var title = "%(CalendarDate.monthNames[_month - 1]) %(_year)"
    var titleX = ((bounds.w - title.count) / 2).floor.max(0)
    Painter.text(sf, titleX, 0, title, style("default"), bounds.w - titleX)

    var weekdays = ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]
    var column = 0
    while (column < 7) {
      Painter.text(sf, gridX + column * 4, 1,
          " %(weekdays[column]) ", style("default"), 4)
      column = column + 1
    }

    var start = gridStart_
    var index = 0
    while (index < 42) {
      var date = CalendarDate.civilFromDays(start + index)
      var row = (index / 7).floor
      column = index % 7
      var x = gridX + column * 4
      var y = 2 + row
      var selected = date[0] == _year && date[1] == _month &&
          date[2] == _day
      var otherMonth = date[1] != _month
      var role = otherMonth ? "list.item.disabled" : "list.item"
      if (selected) role = "list.item.focused"
      var cellStyle = style(role)
      Painter.fill(sf, Rect.new(x, y, 4, 1), " ", cellStyle)
      var label = date[2] < 10 ? "  %(date[2]) " : " %(date[2]) "
      Painter.text(sf, x, y, label, cellStyle, 4)
      index = index + 1
    }
  }

  handle(event) {
    if (event is KeyEvent) return handleKey_(event)
    if (event is MouseEvent) return handleMouse_(event)
    return false
  }

  handleKey_(event) {
    var code = event.code
    if (code == Key.left) {
      moveDays(-1)
      return true
    }
    if (code == Key.right) {
      moveDays(1)
      return true
    }
    if (code == Key.up) {
      moveDays(-7)
      return true
    }
    if (code == Key.down) {
      moveDays(7)
      return true
    }
    if (code == Key.pageUp) {
      moveMonths(-1)
      return true
    }
    if (code == Key.pageDown) {
      moveMonths(1)
      return true
    }
    if (code == Key.home) {
      setDate(_year, _month, 1)
      return true
    }
    if (code == Key.end) {
      setDate(_year, _month,
          CalendarDate.daysInMonth(_year, _month))
      return true
    }
    if (code == Key.enter || event.codepoint == 0x20) {
      accept_()
      return true
    }
    return false
  }

  handleMouse_(event) {
    if (bounds == null || !bounds.contains(event.startX, event.startY)) {
      return false
    }
    if (event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick) {
      moveMonths(-1)
      return true
    }
    if (event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick) {
      moveMonths(1)
      return true
    }
    if (event.event != Mouse.button1Click &&
        event.event != Mouse.button1DblClick) return false

    var x = event.startX - bounds.x
    var y = event.startY - bounds.y
    var gridX = gridX_
    if (y == 0 && x >= gridX && x < gridX + 3) {
      moveMonths(-1)
      return true
    }
    if (y == 0 && x >= gridX + 25 && x < gridX + 28) {
      moveMonths(1)
      return true
    }
    if (y < 2 || y > 7 || x < gridX || x >= gridX + 28) return false

    var column = ((x - gridX) / 4).floor
    var row = y - 2
    var date = CalendarDate.civilFromDays(gridStart_ + row * 7 + column)
    setDate(date[0], date[1], date[2])
    if (event.event == Mouse.button1DblClick) accept_()
    return true
  }
}
