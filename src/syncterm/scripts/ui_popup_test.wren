// Self-tests for ui_popup.  Use a fake "app" that records modal
// pushes and pops without running drainOnce_ — gives us synchronous
// observation of popup state transitions without spinning the event
// loop.

import "ui_widget" for Rect, Container
import "ui_popup"  for Alert, Confirm, Prompt, Find, Popup
import "menu_ui" for MenuUi, ModalPane, StandaloneChoice
import "syncterm"  for KeyEvent, Key

class FakeApp {
  construct new() {
    _stack = []
    _theme = null
  }
  modalStack { _stack }
  effectiveTheme { _theme }
  markDirty() {}
  pushModal(w) {
    w.parent = this
    _stack.add(w)
    return w
  }
  popModal() {
    if (_stack.count == 0) return null
    var w = _stack.removeAt(-1)
    w.parent = null
    return w
  }
  // Synchronous "modal": push, let caller drive handle() directly,
  // pop is widget-driven.  Returns the widget so tests can inspect it.
  modal(w) {
    pushModal(w)
    return w
  }
}

class EscapeFakeApp is FakeApp {
  construct new() {
    super()
    _last = null
  }

  last { _last }

  modal(w) {
    _last = w
    pushModal(w)
    w.handle(KeyEvent.new(Key.escape))
    return w
  }
}

class CommandFakeApp is FakeApp {
  construct new(key, selected) {
    super()
    _key = key
    _selected = selected
  }

  modal(w) {
    pushModal(w)
    w.children[0].selected = _selected
    var event = KeyEvent.new(_key)
    w.handle(event)
    return w
  }
}

class UiPopupTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_popup self-test starting ===")

    testAlertConstruction_()
    testAlertTitledConstruction_()
    testAlertShowCarriesHelp_()
    testAlertEscDismisses_()
    testConfirmYes_()
    testConfirmNo_()
    testConfirmEnterIsYes_()
    testConfirmEscIsNo_()
    testConfirmShowCarriesHelp_()
    testConfirmUnrecognizedKeyDoesNotDismiss_()
    testPromptConstruction_()
    testPromptLongInitialShowsEnd_()
    testPromptEnterReturnsValue_()
    testPromptEscReturnsNull_()
    testPromptForwardsTypingToInput_()
    testPromptMovementPreservesInitial_()
    testFindReplacesPreviousQuery_()
    testMenuChoiceEscReturnsNull_()
    testMenuChoiceCarriesHelp_()
    testMenuChoiceRunsCallbackBeforeDismiss_()
    testMenuCommandChoiceReturnsCommandAndRow_()
    testMenuCommandChoicePreemptsTypeahead_()
    testMenuCommandChoiceProtectsBlankRow_()
    testMenuCommandChoiceCompatibilityAliases_()
    testMenuPromptCarriesHelp_()
    testStandaloneChoiceEscReturnsNull_()
    testModalPaneEscDismisses_()

    var total = __pass + __fail
    System.print("=== ui_popup: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  // ----- Alert ----------------------------------------------------

  static testAlertConstruction_() {
    var a = Alert.new("hi")
    check_(a.message == "hi" && a.title == "Alert" && a.result == null,
           "Alert: message + title + null result on construct")
  }

  static testAlertTitledConstruction_() {
    var a = Alert.new("Notice", "hi")
    check_(a.message == "hi" && a.title == "Notice" && a.result == null,
           "Alert: explicit title on construct")
  }

  static testAlertShowCarriesHelp_() {
    var app = FakeApp.new()
    Alert.show(app, "Notice", "hi", "# Notice Help")
    check_(app.modalStack[-1].helpText == "# Notice Help",
           "Alert: optional help reaches the popup")
  }

  static testAlertEscDismisses_() {
    // Alert dismisses on Esc directly, or via Enter / Space / mouse
    // click on the OK button (handled by Container's focus dispatch
    // to the focused Button child).  Random printable keys fall
    // through and don't dismiss — the OK button is meant to feel like
    // an actual confirmation step, not a "press any key" trap.
    var app = FakeApp.new()
    var a   = Alert.new("hi")
    a.bounds = Rect.new(1, 1, 20, 5)
    app.modal(a)
    check_(app.modalStack.count == 1, "Alert: pushed onto modal stack")
    a.handle(KeyEvent.new(0x41))                // 'A' — falls through
    check_(app.modalStack.count == 1,
           "Alert: random key does not dismiss")
    a.handle(KeyEvent.new(Key.escape))
    check_(app.modalStack.count == 0,
           "Alert: Esc pops the modal")
  }

  // ----- Confirm --------------------------------------------------

  static testConfirmYes_() {
    var app = FakeApp.new()
    var c   = Confirm.new("Delete?")
    c.bounds = Rect.new(1, 1, 20, 5)
    app.modal(c)
    c.handle(KeyEvent.new(0x59))   // 'Y'
    check_(c.result == true && app.modalStack.count == 0,
           "Confirm: Y → result true, dismissed")
  }

  static testConfirmNo_() {
    var app = FakeApp.new()
    var c   = Confirm.new("Delete?")
    c.bounds = Rect.new(1, 1, 20, 5)
    app.modal(c)
    c.handle(KeyEvent.new(0x6E))   // 'n' lowercase
    check_(c.result == false && app.modalStack.count == 0,
           "Confirm: n → result false, dismissed")
  }

  static testConfirmEnterIsYes_() {
    var app = FakeApp.new()
    var c   = Confirm.new("Delete?")
    c.bounds = Rect.new(1, 1, 20, 5)
    app.modal(c)
    c.handle(KeyEvent.new(Key.enter))
    check_(c.result == true,
           "Confirm: Enter → result true")
  }

  static testConfirmEscIsNo_() {
    var app = FakeApp.new()
    var c   = Confirm.new("Delete?")
    c.bounds = Rect.new(1, 1, 20, 5)
    app.modal(c)
    c.handle(KeyEvent.new(Key.escape))
    check_(c.result == false,
           "Confirm: Esc → result false")
  }

  static testConfirmShowCarriesHelp_() {
    var app = FakeApp.new()
    Confirm.show(app, "Delete?", "# Delete Help")
    check_(app.modalStack[-1].helpText == "# Delete Help",
           "Confirm: optional help reaches the popup")
  }

  static testConfirmUnrecognizedKeyDoesNotDismiss_() {
    var app = FakeApp.new()
    var c   = Confirm.new("Delete?")
    c.bounds = Rect.new(1, 1, 20, 5)
    app.modal(c)
    c.handle(KeyEvent.new(0x41))   // 'A'
    check_(app.modalStack.count == 1 && c.result == null,
           "Confirm: unrecognized key doesn't dismiss")
  }

  // ----- Prompt ---------------------------------------------------

  static testPromptConstruction_() {
    var p = Prompt.new("Name?", "default")
    p.bounds = Rect.new(1, 1, 30, 6)
    check_(p.message == "Name?" && p.title == "Prompt" &&
           p.result == null && p.input.allSelected,
           "Prompt: initial value starts selected")
  }

  static testPromptLongInitialShowsEnd_() {
    var p = Prompt.new("URI", "https://example.com/a/long/web-list/path")
    p.bounds = Rect.new(1, 1, 24, 7)
    var input = p.input
    var cursor = input.cursorPos
    var expected = input.count - input.bounds.w + 1
    check_(input.scrollOff == expected && cursor[0] == input.bounds.right,
           "Prompt: long initial value shows its end and cursor")
  }

  static testPromptEnterReturnsValue_() {
    var app = FakeApp.new()
    var p   = Prompt.new("Name?", "abc")
    p.bounds = Rect.new(1, 1, 30, 6)
    app.modal(p)
    p.handle(KeyEvent.new(Key.enter))
    check_(p.result == "abc" && app.modalStack.count == 0,
           "Prompt: Enter → result is input.value, dismissed")
  }

  static testPromptEscReturnsNull_() {
    var app = FakeApp.new()
    var p   = Prompt.new("Name?", "abc")
    p.bounds = Rect.new(1, 1, 30, 6)
    app.modal(p)
    p.handle(KeyEvent.new(Key.escape))
    check_(p.result == null && app.modalStack.count == 0,
           "Prompt: Esc → result null, dismissed")
  }

  static testPromptForwardsTypingToInput_() {
    var app = FakeApp.new()
    var p   = Prompt.new("Name?", "ab")
    p.bounds = Rect.new(1, 1, 30, 6)
    app.modal(p)
    p.handle(KeyEvent.new(0x43))      // 'C'
    p.handle(KeyEvent.new(Key.enter))
    check_(p.result == "C",
           "Prompt: typing replaces the selected initial value")
  }

  static testPromptMovementPreservesInitial_() {
    var app = FakeApp.new()
    var p = Prompt.new("Name?", "ab")
    p.bounds = Rect.new(1, 1, 30, 6)
    app.modal(p)
    p.handle(KeyEvent.new(Key.left))
    p.handle(KeyEvent.new(0x43))
    p.handle(KeyEvent.new(Key.enter))
    check_(p.result == "aCb",
           "Prompt: cursor movement preserves initial value")
  }

  static testFindReplacesPreviousQuery_() {
    var app = FakeApp.new()
    var p = Find.new("Find", "previous")
    p.bounds = Rect.new(1, 1, 30, 3)
    app.modal(p)
    p.handle(KeyEvent.new(0x4E))
    p.handle(KeyEvent.new(Key.enter))
    check_(p.result == "N",
           "Find: typing replaces the previous query")
  }

  static testMenuChoiceEscReturnsNull_() {
    var app = EscapeFakeApp.new()
    var result = MenuUi.choice(app, "Choice", ["One", "Two"], 0)
    check_(result == null && app.modalStack.count == 0,
           "MenuUi.choice: Esc returns null and dismisses")
  }

  static testMenuChoiceCarriesHelp_() {
    var app = EscapeFakeApp.new()
    MenuUi.choice(app, "Choice", ["One", "Two"], 0, "# Choice Help")
    check_(app.last.helpText == "# Choice Help",
           "MenuUi.choice: optional help reaches the modal pane")
  }

  static testMenuChoiceRunsCallbackBeforeDismiss_() {
    var app = CommandFakeApp.new(Key.enter, 1)
    var resident = false
    var result = MenuUi.choice(app, "Choice", ["One", "Two"], 0, null,
        Fn.new {|picked|
      var child = ModalPane.new(Fn.new {})
      app.pushModal(child)
      resident = picked == 1 && app.modalStack.count == 2
      app.popModal()
    })
    check_(resident && result == 1 && app.modalStack.count == 0,
           "MenuUi.choice: callback runs while parent remains modal")
  }

  static testMenuCommandChoiceReturnsCommandAndRow_() {
    var app = CommandFakeApp.new(Key.insert, 1)
    var commands = {}
    commands[Key.insert] = ["insert", true]
    var result = MenuUi.commandChoice(app, "Choice",
        [[10, "One"], [20, "Two"], [-1, ""]], 10, null,
        commands)
    check_(result[0] == "insert" && result[1] == 20,
           "MenuUi.commandChoice: returns command and mapped row")
  }

  static testMenuCommandChoicePreemptsTypeahead_() {
    var app = CommandFakeApp.new(0x5B, 0)
    var commands = {}
    commands[0x5B] = ["previous", true]
    var result = MenuUi.commandChoice(app, "Choice",
        ["One", "Two"], 0, null, commands)
    check_(result[0] == "previous" && result[1] == 0,
           "MenuUi.commandChoice: printable command preempts typeahead")
  }

  static testMenuCommandChoiceProtectsBlankRow_() {
    var app = CommandFakeApp.new(Key.delete, 2)
    var commands = {}
    commands[Key.delete] = ["delete", false]
    var result = MenuUi.commandChoice(app, "Choice",
        [[10, "One"], [20, "Two"], [-1, ""]], 10, null,
        commands)
    check_(result == null && app.modalStack.count == 1,
           "MenuUi.commandChoice: disallowed command leaves blank row open")
    app.popModal()
  }

  static testMenuCommandChoiceCompatibilityAliases_() {
    var app = CommandFakeApp.new(0x2B, 1)
    var commands = {}
    commands[Key.insert] = ["insert", true]
    var result = MenuUi.commandChoice(app, "Choice",
        ["One", "Two"], 0, null, commands)
    check_(result[0] == "insert" && result[1] == 1,
           "MenuUi.commandChoice: + aliases Insert")
  }

  static testMenuPromptCarriesHelp_() {
    var app = EscapeFakeApp.new()
    MenuUi.prompt(app, "Name", "Name", "", 20, false, "# Name Help")
    check_(app.last.helpText == "# Name Help",
           "MenuUi.prompt: optional help reaches the prompt")
  }

  static testStandaloneChoiceEscReturnsNull_() {
    var app = FakeApp.new()
    var popup = StandaloneChoice.new("", ["One", "Two"])
    popup.bounds = Rect.new(1, 1, 24, 7)
    app.pushModal(popup)
    var consumed = popup.handle(KeyEvent.new(Key.escape))
    check_(consumed && popup.result == null && app.modalStack.count == 0,
           "StandaloneChoice: Esc returns null and dismisses")
  }

  static testModalPaneEscDismisses_() {
    var app = FakeApp.new()
    var pane = ModalPane.new(Fn.new { app.popModal() })
    pane.bounds = Rect.new(1, 1, 24, 7)
    app.pushModal(pane)
    var consumed = pane.handle(KeyEvent.new(Key.escape))
    check_(pane.shadow && consumed && app.modalStack.count == 0,
           "ModalPane: casts a shadow and Esc invokes dismissal callback")
  }

}
