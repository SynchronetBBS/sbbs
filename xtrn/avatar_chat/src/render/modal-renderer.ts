import { AppModalState, ActionBarAction, ChannelsModalState, HelpModalState, PrivateThreadsModalState, RosterEntry, RosterModalState } from "../domain/ui";
import { AvatarCache, blitAvatarToFrame, lookupAvatarBinary, resolveAvatarDimensions } from "./avatar";
import { centerText, clipText, padRight, repeatChar, wrapText } from "../util/text";

const BOX_TOP_LEFT = "\xDA";
const BOX_TOP_RIGHT = "\xBF";
const BOX_BOTTOM_LEFT = "\xC0";
const BOX_BOTTOM_RIGHT = "\xD9";
const BOX_HORIZONTAL = "\xC4";
const BOX_VERTICAL = "\xB3";
const SELECTED_POINTER = ">";

export interface ModalRenderContext {
  ownAlias: string;
  ownUserNumber: number;
  avatarLib: AvatarLibrary | null;
  avatarCache: AvatarCache;
}

function drawBorder(frame: Frame, title: string): void {
  const innerWidth = Math.max(1, frame.width - 2);
  let topLine = "";
  let bottomLine = "";

  frame.clear(BG_BLACK | LIGHTGRAY);

  topLine = BOX_TOP_LEFT + repeatChar(BOX_HORIZONTAL, innerWidth) + BOX_TOP_RIGHT;
  bottomLine = BOX_BOTTOM_LEFT + repeatChar(BOX_HORIZONTAL, innerWidth) + BOX_BOTTOM_RIGHT;
  frame.gotoxy(1, 1);
  frame.putmsg(topLine, LIGHTGRAY);

  if (frame.height > 1) {
    frame.gotoxy(1, frame.height);
    frame.putmsg(bottomLine, LIGHTGRAY);
  }

  if (frame.height > 2) {
    let row = 0;

    for (row = 2; row < frame.height; row += 1) {
      frame.gotoxy(1, row);
      frame.putmsg(BOX_VERTICAL, LIGHTGRAY);
      frame.gotoxy(frame.width, row);
      frame.putmsg(BOX_VERTICAL, LIGHTGRAY);
    }
  }

  if (title.length && frame.width > 4) {
    frame.gotoxy(3, 1);
    frame.putmsg(clipText(title, innerWidth - 2), WHITE);
  }
}

function drawHorizontalRule(frame: Frame, y: number, x: number, width: number): void {
  if (width < 1 || y < 1 || y > frame.height) {
    return;
  }

  frame.gotoxy(x, y);
  frame.putmsg(repeatChar(BOX_HORIZONTAL, width), DARKGRAY);
}

function drawVerticalRule(frame: Frame, x: number, y: number, height: number): void {
  let row = 0;

  if (height < 1 || x < 1 || x > frame.width) {
    return;
  }

  for (row = 0; row < height; row += 1) {
    if (y + row > frame.height) {
      break;
    }
    frame.gotoxy(x, y + row);
    frame.putmsg(BOX_VERTICAL, DARKGRAY);
  }
}

function fillCells(frame: Frame, x: number, y: number, width: number, attr: number): void {
  let column = 0;

  for (column = 0; column < width; column += 1) {
    frame.setData(x + column - 1, y - 1, " ", attr, false);
  }
}

function drawListRow(frame: Frame, x: number, y: number, width: number, text: string, selected: boolean, accentAttr: number): void {
  const rowWidth = Math.max(1, width);
  const prefixWidth = Math.min(2, rowWidth);
  const textWidth = Math.max(0, rowWidth - prefixWidth);
  const visibleText = clipText(text, textWidth);
  const rowAttr = selected ? (BG_CYAN | BLACK) : LIGHTGRAY;
  const markerAttr = selected ? (BG_CYAN | BLACK) : accentAttr;
  const rowText = repeatChar(" ", prefixWidth) + visibleText;

  if (selected) {
    fillCells(frame, x, y, rowWidth, rowAttr);
  }

  frame.gotoxy(x, y);
  frame.putmsg(selected ? rowText : padRight(rowText, rowWidth), rowAttr);

  if (selected) {
    frame.gotoxy(x, y);
    frame.putmsg(SELECTED_POINTER, markerAttr);
  }
}

interface StyledToken {
  text: string;
  attr: number;
}

function writeStyledToken(frame: Frame, x: number, y: number, width: number, token: StyledToken): number {
  const visible = clipText(token.text, width);

  if (x < 1 || y < 1 || width < 1 || !visible.length) {
    return 0;
  }

  frame.gotoxy(x, y);
  frame.putmsg(visible, token.attr);
  return visible.length;
}

function writeStyledTokens(frame: Frame, x: number, y: number, width: number, tokens: StyledToken[]): void {
  let writeX = x;
  let remaining = width;
  let index = 0;

  for (index = 0; index < tokens.length; index += 1) {
    const token = tokens[index];

    if (!token) {
      continue;
    }

    const written = writeStyledToken(frame, writeX, y, remaining, token);

    if (!written) {
      continue;
    }

    writeX += written;
    remaining -= written;
    if (remaining < 1) {
      break;
    }
  }
}

function extractUnreadCount(metaText: string | undefined): number {
  const match = String(metaText || "").match(/new\s+(\d+)/i);

  if (!match) {
    return 0;
  }

  return parseInt(match[1] || "0", 10) || 0;
}

function drawCommandFooter(frame: Frame, textTokens: StyledToken[]): void {
  drawHorizontalRule(frame, frame.height - 1, 2, frame.width - 2);
  writeStyledTokens(frame, 3, frame.height - 1, Math.max(1, frame.width - 4), textTokens);
}

function drawChannelLikeRow(frame: Frame, x: number, y: number, width: number, entry: { name: string; userCount: number; isCurrent: boolean; metaText?: string }, primaryLabel: string, selected: boolean): void {
  const rowWidth = Math.max(1, width);
  const contentWidth = Math.max(0, rowWidth - 1);
  const unreadCount = extractUnreadCount(entry.metaText);
  const baseBg = selected ? BG_BLUE : BG_BLACK;
  const pointerAttr = BG_BLACK | LIGHTMAGENTA;
  const nameAttr = baseBg | (selected ? LIGHTCYAN : LIGHTRED);
  const primaryLabelAttr = baseBg | (selected ? LIGHTGRAY : CYAN);
  const primaryValueAttr = baseBg | (selected ? WHITE : LIGHTCYAN);
  const secondaryLabelAttr = baseBg | (selected ? CYAN : BROWN);
  const secondaryValueAttr = baseBg | (selected ? LIGHTGREEN : YELLOW);
  const separatorAttr = baseBg | LIGHTGRAY;
  const prefixText = entry.isCurrent ? "* " : "  ";
  let primaryTokens: StyledToken[] = [];
  let secondaryTokens: StyledToken[] = [];
  let nameWidth = 0;
  let nameText = "";

  frame.gotoxy(x, y);
  frame.putmsg(selected ? SELECTED_POINTER : " ", pointerAttr);
  fillCells(frame, x + 1, y, Math.max(0, rowWidth - 1), baseBg | BLACK);

  primaryTokens = [
    { text: " ", attr: baseBg | BLACK },
    { text: primaryLabel + ": ", attr: primaryLabelAttr },
    { text: String(entry.userCount), attr: primaryValueAttr }
  ];

  if (unreadCount > 0) {
    secondaryTokens = [
      { text: " | ", attr: separatorAttr },
      { text: "New: ", attr: secondaryLabelAttr },
      { text: String(unreadCount), attr: secondaryValueAttr }
    ];
  }

  nameWidth = contentWidth
    - prefixText.length
    - primaryTokens.reduce(function (total, token) { return total + token.text.length; }, 0)
    - secondaryTokens.reduce(function (total, token) { return total + token.text.length; }, 0);

  if (secondaryTokens.length && nameWidth < 6) {
    nameWidth += secondaryTokens.reduce(function (total, token) { return total + token.text.length; }, 0);
    secondaryTokens = [];
  }

  if (nameWidth < 1) {
    nameWidth = 1;
  }

  nameText = clipText(entry.name, nameWidth);

  writeStyledTokens(
    frame,
    x + 1,
    y,
    contentWidth,
    [
      { text: prefixText, attr: nameAttr },
      { text: nameText, attr: nameAttr }
    ].concat(primaryTokens, secondaryTokens)
  );
}

function renderActionTokens(frame: Frame, actions: ActionBarAction[]): void {
  let x = 1;
  let index = 0;

  frame.clear(BG_BLUE | WHITE);

  for (index = 0; index < actions.length; index += 1) {
    const action = actions[index];
    let token = "";
    let attr = index % 2 === 0 ? (BG_CYAN | BLACK) : (BG_BLUE | WHITE);

    if (!action) {
      continue;
    }

    if (action.attr !== undefined) {
      attr = action.attr;
    }

    token = " " + action.label + " ";

    if (x + token.length - 1 > frame.width) {
      break;
    }

    frame.gotoxy(x, 1);
    frame.putmsg(token, attr);
    x += token.length + 1;
  }

  if (x <= frame.width) {
    frame.gotoxy(x, 1);
    frame.putmsg(repeatChar(" ", frame.width - x + 1), BG_BLUE | WHITE);
  }
}

function renderRosterDetails(frame: Frame, x: number, y: number, width: number, entry: RosterEntry, context: ModalRenderContext): void {
  const avatarSize = resolveAvatarDimensions(context.avatarLib);
  const avatarX = x;
  const avatarY = y;
  const detailX = width >= avatarSize.width + 18 ? (x + avatarSize.width + 2) : x;
  const detailY = width >= avatarSize.width + 18 ? y : (y + avatarSize.height + 1);
  const avatarBinary = lookupAvatarBinary(
    context.avatarLib,
    entry.nick,
    context.ownAlias,
    context.ownUserNumber,
    context.avatarCache
  );
  const roleLabel = entry.isSelf ? "You" : "User";
  const nameLabel = clipText(entry.name, Math.max(8, width - (detailX - x)));
  const bbsTitle = "BBS";
  const bbsLabel = clipText(entry.bbs, Math.max(8, width - (detailX - x)));

  if (avatarBinary) {
    blitAvatarToFrame(frame, avatarBinary, avatarSize.width, avatarSize.height, avatarX, avatarY);
  }

  frame.gotoxy(detailX, detailY);
  frame.putmsg(roleLabel, LIGHTBLUE);
  frame.gotoxy(detailX, detailY + 1);
  frame.putmsg(nameLabel, WHITE);
  frame.gotoxy(detailX, detailY + 3);
  frame.putmsg(bbsTitle, LIGHTBLUE);
  frame.gotoxy(detailX, detailY + 4);
  frame.putmsg(bbsLabel, WHITE);
}

function renderRosterModal(frame: Frame, modal: RosterModalState, context: ModalRenderContext): void {
  const contentWidth = Math.max(1, frame.width - 2);
  const contentHeight = Math.max(1, frame.height - 2);
  const listWidth = Math.min(28, Math.max(20, Math.floor((contentWidth - 3) / 2)));
  const detailX = listWidth + 4;
  const listHeight = Math.max(1, contentHeight - 3);
  const selectedIndex = modal.entries.length ? Math.max(0, Math.min(modal.selectedIndex, modal.entries.length - 1)) : 0;
  const topIndex = modal.entries.length > listHeight
    ? Math.max(0, Math.min(selectedIndex - Math.floor(listHeight / 2), modal.entries.length - listHeight))
    : 0;
  const selectedEntry = modal.entries[selectedIndex] || modal.entries[0];
  let row = 0;

  drawBorder(frame, modal.title);
  drawVerticalRule(frame, listWidth + 3, 2, Math.max(1, frame.height - 3));
  drawHorizontalRule(frame, frame.height - 1, 2, frame.width - 2);

  if (!modal.entries.length) {
    frame.gotoxy(2, Math.max(2, Math.floor(frame.height / 2)));
    frame.putmsg(centerText("Nobody is here right now.", frame.width - 2), DARKGRAY);
    frame.gotoxy(3, frame.height - 1);
    frame.putmsg(clipText("Esc close", frame.width - 4), LIGHTBLUE);
    return;
  }

  for (row = 0; row < listHeight; row += 1) {
    const entry = modal.entries[topIndex + row];
    if (!entry) {
      break;
    }
    drawListRow(
      frame,
      2,
      2 + row,
      listWidth + 1,
      entry.name,
      topIndex + row === selectedIndex,
      entry.isSelf ? LIGHTCYAN : LIGHTMAGENTA
    );
  }

  if (selectedEntry) {
    renderRosterDetails(
      frame,
      detailX,
      3,
      Math.max(10, frame.width - detailX - 1),
      selectedEntry,
      context
    );
  }

  frame.gotoxy(3, frame.height - 1);
  frame.putmsg(clipText("Up/Down move | Esc close", frame.width - 4), LIGHTBLUE);
}

function renderChannelsModal(frame: Frame, modal: ChannelsModalState): void {
  const bodyHeight = Math.max(1, frame.height - 3);
  const selectedIndex = modal.entries.length ? Math.max(0, Math.min(modal.selectedIndex, modal.entries.length - 1)) : 0;
  const topIndex = modal.entries.length > bodyHeight
    ? Math.max(0, Math.min(selectedIndex - Math.floor(bodyHeight / 2), modal.entries.length - bodyHeight))
    : 0;
  let row = 0;

  drawBorder(frame, modal.title);

  if (!modal.entries.length) {
    frame.gotoxy(2, Math.max(2, Math.floor(frame.height / 2)));
    frame.putmsg(centerText("No joined channels.", frame.width - 2), DARKGRAY);
    drawCommandFooter(frame, [
      { text: "Esc", attr: YELLOW },
      { text: " close | ", attr: LIGHTBLUE },
      { text: "/join", attr: YELLOW },
      { text: " start", attr: LIGHTBLUE }
    ]);
    return;
  }

  for (row = 0; row < bodyHeight; row += 1) {
    const entry = modal.entries[topIndex + row];

    if (!entry) {
      break;
    }

    drawChannelLikeRow(frame, 2, 2 + row, frame.width - 2, entry, "Users", topIndex + row === selectedIndex);
  }

  drawCommandFooter(frame, [
    { text: "Enter", attr: YELLOW },
    { text: " switch | ", attr: LIGHTBLUE },
    { text: "Esc", attr: YELLOW },
    { text: " close | ", attr: LIGHTBLUE },
    { text: "/join", attr: YELLOW },
    { text: " ", attr: LIGHTBLUE },
    { text: "/part", attr: YELLOW },
    { text: " manage", attr: LIGHTBLUE }
  ]);
}

function renderPrivateThreadsModal(frame: Frame, modal: PrivateThreadsModalState): void {
  const bodyHeight = Math.max(1, frame.height - 3);
  const selectedIndex = modal.entries.length ? Math.max(0, Math.min(modal.selectedIndex, modal.entries.length - 1)) : 0;
  const topIndex = modal.entries.length > bodyHeight
    ? Math.max(0, Math.min(selectedIndex - Math.floor(bodyHeight / 2), modal.entries.length - bodyHeight))
    : 0;
  let row = 0;

  drawBorder(frame, modal.title);

  if (!modal.entries.length) {
    frame.gotoxy(2, Math.max(2, Math.floor(frame.height / 2)));
    frame.putmsg(centerText("No private threads yet.", frame.width - 2), DARKGRAY);
    drawCommandFooter(frame, [
      { text: "Esc", attr: YELLOW },
      { text: " close | ", attr: LIGHTBLUE },
      { text: "/msg", attr: YELLOW },
      { text: " start", attr: LIGHTBLUE }
    ]);
    return;
  }

  for (row = 0; row < bodyHeight; row += 1) {
    const entry = modal.entries[topIndex + row];

    if (!entry) {
      break;
    }

    drawChannelLikeRow(frame, 2, 2 + row, frame.width - 2, entry, "Msgs", topIndex + row === selectedIndex);
  }

  drawCommandFooter(frame, [
    { text: "Enter", attr: YELLOW },
    { text: " switch | ", attr: LIGHTBLUE },
    { text: "Esc", attr: YELLOW },
    { text: " close | ", attr: LIGHTBLUE },
    { text: "/msg", attr: YELLOW },
    { text: " start chats", attr: LIGHTBLUE }
  ]);
}

function renderHelpModal(frame: Frame, modal: HelpModalState): void {
  const width = Math.max(1, frame.width - 4);
  let row = 0;
  let writeY = 2;

  drawBorder(frame, modal.title);
  drawHorizontalRule(frame, frame.height - 1, 2, frame.width - 2);

  for (row = 0; row < modal.lines.length; row += 1) {
    const wrapped = wrapText(modal.lines[row] || "", width);
    let innerIndex = 0;

    for (innerIndex = 0; innerIndex < wrapped.length; innerIndex += 1) {
      if (writeY >= frame.height - 1) {
        break;
      }
      frame.gotoxy(3, writeY);
      frame.putmsg(clipText(wrapped[innerIndex] || "", width), LIGHTGRAY);
      writeY += 1;
    }

    if (writeY >= frame.height - 1) {
      break;
    }
  }

  frame.gotoxy(3, frame.height - 1);
  frame.putmsg(clipText("Esc close", frame.width - 4), LIGHTBLUE);
}

export function renderActionBar(frame: Frame, actions: ActionBarAction[]): void {
  renderActionTokens(frame, actions);
}

export function renderModal(overlay: Frame, modalFrame: Frame, modal: AppModalState, context: ModalRenderContext): void {
  overlay.clear(BG_BLACK | DARKGRAY);
  modalFrame.clear(BG_BLACK | LIGHTGRAY);

  if (modal.kind === "roster") {
    renderRosterModal(modalFrame, modal, context);
    return;
  }

  if (modal.kind === "channels") {
    renderChannelsModal(modalFrame, modal);
    return;
  }

  if (modal.kind === "private") {
    renderPrivateThreadsModal(modalFrame, modal);
    return;
  }

  renderHelpModal(modalFrame, modal);
}
