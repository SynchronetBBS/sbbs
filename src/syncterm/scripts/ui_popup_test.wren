// Self-tests for ui_popup.  Use a fake "app" that records modal
// pushes and pops without running drainOnce_ — gives us synchronous
// observation of popup state transitions without spinning the event
// loop.

import "ui_widget" for Rect, Container
import "ui_popup"  for Alert, Confirm, Prompt, Popup
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

class UiPopupTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_popup self-test starting ===")

    testAlertConstruction_()
    testAlertTitledConstruction_()
    testAlertEscDismisses_()
    testConfirmYes_()
    testConfirmNo_()
    testConfirmEnterIsYes_()
    testConfirmEscIsNo_()
    testConfirmUnrecognizedKeyDoesNotDismiss_()
    testPromptConstruction_()
    testPromptEnterReturnsValue_()
    testPromptEscReturnsNull_()
    testPromptForwardsTypingToInput_()

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
    check_(p.message == "Name?" && p.title == "Prompt" && p.result == null,
           "Prompt: message + title + null result on construct")
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
    check_(p.result == "abC",
           "Prompt: typed char forwarded to TextInput; Enter returns updated value")
  }
}
