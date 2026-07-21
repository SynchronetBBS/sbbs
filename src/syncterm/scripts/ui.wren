// SyncTERM Wren UI library — convenience aggregator.
//
// Pulls every public class out of the per-topic ui_*.wren modules so
// consumers can write a single line:
//
//   import "ui" for App, Pane, ListView, TextInput, SelectOnFocusInput,
//                  Button, Alert, Confirm, Prompt, LinePrompt, Find,
//                  Help, Rect
//
// instead of chasing individual modules.  The fine-grained modules
// remain the source of truth — this file only re-exports.

import "ui_widget"    for Widget, Container, Rect
import "ui_style"     for Style, Glyphs, Theme
import "ui_draw"      for Painter
import "ui_pane"      for Pane
import "ui_list"      for ListView
import "ui_picker"    for ListPicker
import "ui_input"     for TextInput, SelectOnFocusInput
import "ui_button"    for Button
import "ui_checkbox"  for Checkbox
import "ui_radio"     for RadioGroup
import "ui_spinbox"   for SpinBox
import "ui_color_picker" for ColorPicker
import "ui_statusbar" for StatusBar
import "ui_menubar"   for MenuBar
import "ui_form"      for Form
import "ui_popup"     for PopStatus, Popup, Alert, Confirm, Prompt, LinePrompt, Find
import "ui_help"      for Help
import "ui_app"       for App
