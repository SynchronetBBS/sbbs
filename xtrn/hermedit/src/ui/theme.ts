/** Chrome colors, kept in one place. All values are CGA attribute bytes. */

import { makeAttr, BLACK, BLUE, GREEN, CYAN, RED, BROWN, LIGHTGRAY, MAGENTA } from '../core/attr';

export var theme = {
  /** Title bar. */
  title: makeAttr(LIGHTGRAY, BLUE, true),
  titleDim: makeAttr(CYAN, BLUE, false),
  /** Title-bar values (e.g. the recipient): light cyan. */
  titleValue: makeAttr(CYAN, BLUE, true),
  /** Button bar background. */
  bar: makeAttr(BLACK, CYAN, false),
  /** Button body: label text. */
  button: makeAttr(BLACK, LIGHTGRAY, false),
  /** Button key hint (the part in brackets). */
  buttonKey: makeAttr(RED, LIGHTGRAY, false),
  /** Directional arrows inside buttons (distinct from the key hint color). */
  buttonArrow: makeAttr(BLUE, LIGHTGRAY, false),
  /** Focused/active button. */
  buttonActive: makeAttr(LIGHTGRAY, BLUE, true),
  buttonActiveKey: makeAttr(BROWN, BLUE, true),
  /** Divider lines between chrome and canvas. */
  divider: makeAttr(CYAN, BLACK, false),
  /** Right-edge safe-width boundary marker. */
  boundary: makeAttr(BLACK, BLACK, true),
  /** Status bar: black background; dark-gray labels, white values. */
  status: makeAttr(LIGHTGRAY, BLACK, false),
  statusLabel: makeAttr(BLACK, BLACK, true),      // dark gray
  statusValue: makeAttr(LIGHTGRAY, BLACK, true),  // bright white
  statusHi: makeAttr(LIGHTGRAY, BLACK, true),
  /** Canvas default. */
  canvas: makeAttr(LIGHTGRAY, BLACK, false),
  /** Modal window frame/body. */
  modalFrame: makeAttr(LIGHTGRAY, BLUE, true),
  modalBody: makeAttr(LIGHTGRAY, BLUE, false),
  modalTitle: makeAttr(BROWN, BLUE, true),
  modalSel: makeAttr(BLUE, LIGHTGRAY, false),
  /** Character-set bar (above the status bar). */
  charsetBar: makeAttr(LIGHTGRAY, MAGENTA, false),
  charsetKey: makeAttr(LIGHTGRAY, MAGENTA, true),
  /** Keyboard hints that live outside buttons (status row, panel rows). */
  keyHint: makeAttr(BROWN, BLACK, true), // yellow
  /** Side panel. */
  panel: makeAttr(LIGHTGRAY, BLACK, false),
  panelTitle: makeAttr(CYAN, BLACK, true),
  panelSel: makeAttr(LIGHTGRAY, MAGENTA, true),
  /** Quote picker line colors. */
  quote: makeAttr(GREEN, BLACK, false),
  quoteSel: makeAttr(LIGHTGRAY, BLUE, true)
};
