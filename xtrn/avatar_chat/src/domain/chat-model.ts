import { trimText } from "../util/text";

export interface GroupMessage {
  text: string;
  time: number;
}

export interface MessageGroup {
  kind: "bubble" | "notice";
  side: "left" | "right";
  speakerName: string;
  nick: ChatNick | null;
  messages: GroupMessage[];
}

export function normalizeChannelName(name: string, fallback?: string): string {
  const trimmed = trimText(name);
  if (trimmed.length) {
    return trimmed;
  }
  if (fallback && trimText(fallback).length) {
    return trimText(fallback);
  }
  return "main";
}

export function trimChannelMessages(channel: ChatChannel, maxHistory: number): void {
  const overflow = channel.messages.length - maxHistory;
  if (overflow > 0) {
    channel.messages.splice(0, overflow);
  }
}

export function buildNoticeMessage(text: string, timestamp?: number): ChatMessage {
  return {
    str: text,
    time: timestamp || new Date().getTime()
  };
}

export function groupMessages(messages: ChatMessage[], ownAlias: string): MessageGroup[] {
  const groups: MessageGroup[] = [];
  const ownAliasUpper = ownAlias.toUpperCase();
  let lastBubble: MessageGroup | null = null;
  let index = 0;

  for (index = 0; index < messages.length; index += 1) {
    const message = messages[index];
    let speakerName = "";
    let side: "left" | "right" = "left";
    let speakerUpper = "";

    if (!message) {
      continue;
    }

    if (!message.nick || !message.nick.name) {
      groups.push({
        kind: "notice",
        side: "left",
        speakerName: "",
        nick: null,
        messages: [{
          text: message.str || "",
          time: message.time
        }]
      });
      lastBubble = null;
      continue;
    }

    speakerName = message.nick.name;
    speakerUpper = speakerName.toUpperCase();
    side = speakerUpper === ownAliasUpper ? "right" : "left";

    if (
      lastBubble &&
      lastBubble.kind === "bubble" &&
      lastBubble.side === side &&
      lastBubble.speakerName.toUpperCase() === speakerUpper
    ) {
      lastBubble.messages.push({
        text: message.str || "",
        time: message.time
      });
      continue;
    }

    lastBubble = {
      kind: "bubble",
      side: side,
      speakerName: speakerName,
      nick: message.nick,
      messages: [{
        text: message.str || "",
        time: message.time
      }]
    };
    groups.push(lastBubble);
  }

  return groups;
}
