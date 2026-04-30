// Self-tests for ui_form.

import "ui_widget"   for Rect
import "ui_form"     for Form
import "ui_input"    for TextInput
import "ui_checkbox" for Checkbox
import "syncterm"    for KeyEvent, Key

class UiFormTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_form self-test starting ===")

    testEmptyDefaults_()
    testAddFieldAddsAsChild_()
    testLabelColumnWidthIsMaxLabel_()
    testFieldYIncrementsByRowH_()
    testCustomRowHPerField_()
    testRowGapInsertsBlankRows_()
    testOnSubmitCreatesOkButton_()
    testOnCancelCreatesCancelButton_()
    testEscFiresOnCancel_()
    testButtonRowBelowLastField_()
    testClearFieldsRemovesChildren_()
    testLabelPaintedRightAligned_()

    var total = __pass + __fail
    System.print("=== ui_form: %(total) tests, %(__pass) pass, %(__fail) fail ===")
    return [__pass, __fail]
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
      System.print("  PASS %(label)")
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  static testEmptyDefaults_() {
    var f = Form.new()
    check_(f.fields.count == 0 && f.children.count == 0,
           "Form: empty by default")
  }

  static testAddFieldAddsAsChild_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    var t = TextInput.new()
    f.addField("Name:", t)
    check_(f.fields.count == 1 && f.children.count == 1 &&
           f.children[0] == t,
           "Form.addField: queues field and adds widget as child")
  }

  static testLabelColumnWidthIsMaxLabel_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("X:",         TextInput.new())
    f.addField("LongLabel:", TextInput.new())
    // Label col width = 10 ("LongLabel:".count); widgets start at x+10+2.
    var w2 = f.children[1]
    check_(w2.bounds.x == 1 + 10 + 2,
           "Form.layout: widgets start past max label + 2-cell gap")
  }

  static testFieldYIncrementsByRowH_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    f.addField("b:", TextInput.new())
    var y0 = f.children[0].bounds.y
    var y1 = f.children[1].bounds.y
    check_(y1 == y0 + 1,
           "Form.layout: default rowH=1, second field one row below first")
  }

  static testCustomRowHPerField_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addFieldH("a:", TextInput.new(), 3)
    f.addField("b:", TextInput.new())
    var y0 = f.children[0].bounds.y
    var y1 = f.children[1].bounds.y
    check_(y1 == y0 + 3,
           "Form.layout: per-field rowH bumps next field's y by that amount")
  }

  static testRowGapInsertsBlankRows_() {
    var f = Form.new()
    f.rowGap = 1
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    f.addField("b:", TextInput.new())
    var y0 = f.children[0].bounds.y
    var y1 = f.children[1].bounds.y
    check_(y1 == y0 + 2,
           "Form.layout: rowGap=1 puts a blank row between fields")
  }

  static testOnSubmitCreatesOkButton_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    f.onSubmit = Fn.new { }
    check_(f.children.count == 2,
           "Form.onSubmit=: adds OK button as child")
  }

  static testOnCancelCreatesCancelButton_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    f.onSubmit = Fn.new { }
    f.onCancel = Fn.new { }
    check_(f.children.count == 3,
           "Form.onCancel=: adds Cancel button on top of OK")
  }

  static testEscFiresOnCancel_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    var fired = [false]
    f.onCancel = Fn.new { fired[0] = true }
    var consumed = f.handle(KeyEvent.new(Key.escape))
    check_(consumed && fired[0],
           "Form: Esc fires onCancel")
  }

  static testButtonRowBelowLastField_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    f.onSubmit = Fn.new { }
    var fy = f.children[0].bounds.y          // field row
    var by = f.children[1].bounds.y          // OK button row
    check_(by == fy + 2,
           "Form.layout: button row sits one blank row below last field")
  }

  static testClearFieldsRemovesChildren_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 30, 10)
    f.addField("a:", TextInput.new())
    f.addField("b:", Checkbox.new("X"))
    f.clearFields()
    check_(f.fields.count == 0 && f.children.count == 0,
           "Form.clearFields: drops fields and removes children")
  }

  static testLabelPaintedRightAligned_() {
    var f = Form.new()
    f.bounds = Rect.new(1, 1, 20, 4)
    f.addField("X:", TextInput.new())
    f.addField("Long:", TextInput.new())
    var sf = f.draw()
    // Label col width = 5 ("Long:"), so "X:" is right-padded — its
    // last char ':' lands at col 4, first char 'X' at col 3.
    check_(sf.cellAt(3, 0).ch == "X" && sf.cellAt(4, 0).ch == ":",
           "Form.draw: short labels right-aligned in the label column")
  }
}
