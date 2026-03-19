import { groupMessages, MessageGroup } from "../domain/chat-model";
import { blitAvatarToFrame, lookupAvatarBinary, AvatarCache, resolveAvatarDimensions } from "./avatar";
import { centerText, formatClockTime, padRight, wrapText } from "../util/text";

interface BubbleLayout {
  kind: "bubble";
  side: "left" | "right";
  speakerName: string;
  timestamp: string;
  rows: BubbleRow[];
  width: number;
  height: number;
  avatarBinary: string | null;
}

interface BubbleRow {
  text: string;
  isBubble: boolean;
}

interface NoticeLayout {
  kind: "notice";
  lines: string[];
  height: number;
}

type LayoutBlock = BubbleLayout | NoticeLayout;

export interface TranscriptRenderOptions {
  ownAlias: string;
  ownUserNumber: number;
  avatarLib: AvatarLibrary | null;
  avatarCache: AvatarCache;
  emptyText: string;
  scrollOffsetBlocks?: number;
}

export interface TranscriptRenderState {
  visibleBlockCount: number;
  totalBlockCount: number;
  maxScrollOffsetBlocks: number;
  actualScrollOffsetBlocks: number;
}

function measureBubbleWidth(rows: BubbleRow[]): number {
  let width = 0;
  let index = 0;

  for (index = 0; index < rows.length; index += 1) {
    const row = rows[index];
    const line = row && row.isBubble ? (row.text || "") : "";
    if (line.length > width) {
      width = line.length;
    }
  }

  return width;
}

function buildLayouts(
  messages: ChatMessage[],
  frameWidth: number,
  options: TranscriptRenderOptions
): LayoutBlock[] {
  const groups = groupMessages(messages, options.ownAlias);
  const avatarSize = resolveAvatarDimensions(options.avatarLib);
  const blocks: LayoutBlock[] = [];
  const maxBubbleWidth = Math.max(14, Math.min(frameWidth - avatarSize.width - 4, 54));
  let index = 0;

  for (index = 0; index < groups.length; index += 1) {
    const group = groups[index];

    if (!group) {
      continue;
    }

    if (group.kind === "notice") {
      const noticeText = group.messages[0] ? group.messages[0].text : "";
      const noticeLines = wrapText(noticeText, Math.max(10, frameWidth - 4));

      blocks.push({
        kind: "notice",
        lines: noticeLines,
        height: noticeLines.length
      });
      continue;
    }

    blocks.push(buildBubbleLayout(group, maxBubbleWidth, avatarSize.height, options));
  }

  return blocks;
}

function buildBubbleLayout(
  group: MessageGroup,
  maxBubbleWidth: number,
  avatarHeight: number,
  options: TranscriptRenderOptions
): BubbleLayout {
  const rows: BubbleRow[] = [];
  const lastMessage = group.messages.length ? group.messages[group.messages.length - 1] : null;
  const timestamp = lastMessage ? formatClockTime(lastMessage.time) : "";
  let index = 0;
  let messageIndex = 0;
  let width = 0;
  let avatarBinary = null;

  for (messageIndex = 0; messageIndex < group.messages.length; messageIndex += 1) {
    const message = group.messages[messageIndex];
    const messageLines = wrapText(message ? message.text : "", Math.max(8, maxBubbleWidth - 2));

    for (index = 0; index < messageLines.length; index += 1) {
      rows.push({
        text: messageLines[index] || "",
        isBubble: true
      });
    }

    if (messageIndex < group.messages.length - 1) {
      rows.push({
        text: "",
        isBubble: false
      });
    }
  }

  if (!rows.length) {
    rows.push({
      text: "",
      isBubble: true
    });
  }

  width = measureBubbleWidth(rows) + 2;
  if (width < 8) {
    width = 8;
  }

  avatarBinary = lookupAvatarBinary(
    options.avatarLib,
    group.nick,
    options.ownAlias,
    options.ownUserNumber,
    options.avatarCache
  );

  return {
    kind: "bubble",
    side: group.side,
    speakerName: group.speakerName,
    timestamp: timestamp,
    rows: rows,
    width: width,
    height: Math.max(avatarHeight, rows.length + 1),
    avatarBinary: avatarBinary
  };
}

function pickVisibleBlocks(blocks: LayoutBlock[], frameHeight: number, scrollOffsetBlocks: number): {
  blocks: LayoutBlock[];
  actualScrollOffsetBlocks: number;
} {
  const selected: LayoutBlock[] = [];
  const bottomIndex = Math.max(0, blocks.length - 1 - Math.max(0, scrollOffsetBlocks));
  let used = 0;
  let index = bottomIndex;

  while (index >= 0) {
    const block = blocks[index];

    if (!block) {
      index -= 1;
      continue;
    }

    const gap = selected.length > 0 ? 1 : 0;
    const nextUsed = used + gap + block.height;

    if (nextUsed > frameHeight && selected.length > 0) {
      break;
    }

    selected.unshift(block);
    used = nextUsed;

    if (used >= frameHeight) {
      break;
    }

    index -= 1;
  }

  return {
    blocks: selected,
    actualScrollOffsetBlocks: blocks.length ? (blocks.length - 1 - bottomIndex) : 0
  };
}

function usedHeight(blocks: LayoutBlock[]): number {
  let total = 0;
  let index = 0;

  for (index = 0; index < blocks.length; index += 1) {
    const block = blocks[index];
    if (!block) {
      continue;
    }
    total += block.height;
    if (index < blocks.length - 1) {
      total += 1;
    }
  }

  return total;
}

function renderNotice(frame: Frame, block: NoticeLayout, startRow: number): number {
  let index = 0;

  for (index = 0; index < block.lines.length; index += 1) {
    frame.gotoxy(1, startRow + index);
    frame.putmsg(centerText(block.lines[index] || "", frame.width), DARKGRAY);
  }

  return startRow + block.lines.length;
}

function renderBubble(frame: Frame, block: BubbleLayout, startRow: number, avatarLib: AvatarLibrary | null): number {
  const avatarSize = resolveAvatarDimensions(avatarLib);
  const avatarX = block.side === "left" ? 1 : frame.width - avatarSize.width + 1;
  const bubbleX = block.side === "left"
    ? avatarSize.width + 3
    : Math.max(1, avatarX - 2 - block.width);
  const headerText = block.speakerName + " " + block.timestamp;
  const headerX = block.side === "left"
    ? bubbleX
    : Math.max(1, avatarX - 2 - headerText.length);
  const bubbleAttr = block.side === "left" ? (BG_CYAN | BLACK) : (BG_BLUE | WHITE);
  const speakerAttr = block.side === "left" ? LIGHTMAGENTA : LIGHTCYAN;
  let lineIndex = 0;

  frame.gotoxy(headerX, startRow);
  frame.putmsg(block.speakerName, speakerAttr);
  frame.putmsg(" ", LIGHTGRAY);
  frame.putmsg(block.timestamp, LIGHTBLUE);

  for (lineIndex = 0; lineIndex < block.rows.length; lineIndex += 1) {
    const row = block.rows[lineIndex];

    if (!row || !row.isBubble) {
      continue;
    }

    frame.gotoxy(bubbleX, startRow + 1 + lineIndex);
    frame.putmsg(" " + padRight(row.text || "", block.width - 2) + " ", bubbleAttr);
  }

  if (block.avatarBinary) {
    blitAvatarToFrame(frame, block.avatarBinary, avatarSize.width, avatarSize.height, avatarX, startRow);
  }

  return startRow + block.height;
}

export function renderTranscript(
  frame: Frame,
  messages: ChatMessage[],
  options: TranscriptRenderOptions
): TranscriptRenderState {
  const blocks = buildLayouts(messages, frame.width, options);
  const selected = pickVisibleBlocks(blocks, frame.height, options.scrollOffsetBlocks || 0);
  const visibleBlocks = selected.blocks;
  const totalHeight = usedHeight(visibleBlocks);
  const renderState: TranscriptRenderState = {
    visibleBlockCount: visibleBlocks.length,
    totalBlockCount: blocks.length,
    maxScrollOffsetBlocks: blocks.length ? (blocks.length - 1) : 0,
    actualScrollOffsetBlocks: selected.actualScrollOffsetBlocks
  };
  let row = Math.max(1, frame.height - totalHeight + 1);
  let index = 0;

  frame.clear(BG_BLACK | LIGHTGRAY);

  if (!messages.length) {
    frame.gotoxy(1, Math.max(1, Math.floor(frame.height / 2)));
    frame.putmsg(centerText(options.emptyText, frame.width), DARKGRAY);
    return renderState;
  }

  for (index = 0; index < visibleBlocks.length; index += 1) {
    const block = visibleBlocks[index];

    if (!block) {
      continue;
    }

    if (block.kind === "notice") {
      row = renderNotice(frame, block, row);
    } else {
      row = renderBubble(frame, block, row, options.avatarLib);
    }

    if (index < visibleBlocks.length - 1) {
      row += 1;
    }
  }

  return renderState;
}
