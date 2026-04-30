// SyncTERM Wren UI library — convenience aggregator.
//
// Pulls every public class out of the per-topic ui_*.wren modules so
// consumers can write a single line:
//
//   import "ui" for App, Pane, ListView, TextInput, Button,
//                  Alert, Confirm, Prompt, Help, Rect
//
// instead of chasing individual modules.  The fine-grained modules
// remain the source of truth — this file only re-exports.

import "ui_widget" for Widget, Container, Rect
import "ui_style"  for Style, Glyphs, Theme
import "ui_draw"   for Painter
import "ui_pane"   for Pane
import "ui_list"   for ListView
import "ui_input"  for TextInput
import "ui_button" for Button
import "ui_popup"  for PopStatus, Popup, Alert, Confirm, Prompt
import "ui_help"   for Help
import "ui_app"    for App
