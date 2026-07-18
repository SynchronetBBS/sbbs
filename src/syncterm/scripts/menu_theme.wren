import "ui_style" for Style, Theme

class ClassicTheme {
  static rgb_(index) {
    return [
      0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
      0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
      0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
      0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    ][index]
  }

  static color_(value, sentinel, fallback) {
    return value == sentinel ? fallback : value
  }

  static style_(foreground, background) {
    return Style.new(0, (background << 4) | foreground,
        rgb_(foreground), rgb_(background))
  }

  static from(settings) {
    var frame = color_(settings.frameColor, 16, 14)
    var text = color_(settings.textColor, 16, 15)
    var background = color_(settings.backgroundColor, 8, 1)
    var inverse = color_(settings.inverseColor, 8, 3)
    var lightbar = color_(settings.lightbarColor, 16, 1)
    var lightbarBackground = color_(settings.lightbarBackgroundColor, 8, 7)

    var normal = style_(text, background)
    var inactive = style_(text, inverse)
    var selected = style_(lightbar, lightbarBackground)
    var inactiveSelected = style_(frame, inverse)
    var frameStyle = style_(frame, background)
    var roles = {}
    var base = Theme.default
    for (name in base.roles.keys) roles[name] = base.roles[name]

    roles["default"] = normal
    roles["default.inactive"] = inactive
    roles["frame"] = frameStyle
    roles["frame.inactive"] = inactive
    roles["title"] = frameStyle
    roles["title.inactive"] = inactive
    roles["menu.item"] = normal
    roles["menu.item.focused"] = selected
    roles["list.item"] = normal
    roles["list.item.focused"] = selected
    roles["list.item.focused.inactive"] = inactiveSelected
    roles["input"] = selected
    roles["input.focused"] = selected
    roles["button"] = normal
    roles["button.hotkey"] = frameStyle
    roles["button.focused"] = selected
    roles["button.focused.hotkey"] = selected
    roles["checkbox"] = normal
    roles["checkbox.focused"] = selected
    roles["radio.item"] = normal
    roles["radio.item.focused"] = selected
    roles["spinbox"] = selected
    roles["spinbox.focused"] = selected
    roles["statusbar"] = style_(0, inverse)
    roles["classic.backdrop"] = style_(inverse, background)
    roles["classic.header"] = style_(background, inverse)
    roles["classic.comment"] = inactive
    roles["classic.hint"] = style_(0, inverse)
    roles["classic.hotkey"] = style_(background, inverse)
    roles["help"] = normal
    roles["help.bold"] = frameStyle
    roles["help.code"] = frameStyle
    roles["help.heading.1"] = style_(background, inverse)
    roles["help.heading.2"] = style_(background, inverse)
    roles["help.heading.3"] = style_(background, inverse)
    roles["help.bullet"] = normal
    return Theme.new(roles, base.glyphs)
  }
}
