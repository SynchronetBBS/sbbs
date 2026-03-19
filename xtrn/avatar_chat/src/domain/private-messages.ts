import { trimText } from "../util/text";

export function normalizePrivateNick(nick: ChatNick | null | undefined): ChatNick | null {
  const name = trimText(String(nick && nick.name ? nick.name : ""));
  const avatar = nick && nick.avatar ? trimText(String(nick.avatar)) : "";

  if (!name.length) {
    return null;
  }

  return {
    name: name,
    host: nick && nick.host ? trimText(String(nick.host)) : undefined,
    ip: nick && nick.ip ? String(nick.ip) : undefined,
    qwkid: nick && nick.qwkid ? trimText(String(nick.qwkid)).toUpperCase() : undefined,
    avatar: avatar.length ? avatar : undefined
  };
}

export function isPrivateMessage(message: ChatMessage | null | undefined): boolean {
  return !!(
    message &&
    message.private &&
    message.private.to &&
    message.private.to.name &&
    trimText(String(message.private.to.name)).length
  );
}

export function buildPrivateMessage(sender: ChatNick, recipient: ChatNick, text: string, timestamp: number): ChatMessage {
  return {
    nick: normalizePrivateNick(sender) || sender,
    str: text,
    time: timestamp,
    private: {
      to: normalizePrivateNick(recipient) || recipient
    }
  };
}

export function buildPrivateThreadId(nick: ChatNick): string {
  const normalized = normalizePrivateNick(nick);
  const nameKey = normalized
    ? trimText(String(normalized.name)).replace(/[^A-Za-z0-9]/g, "").toUpperCase()
    : "";
  const remoteKey = normalized && normalized.qwkid
    ? normalized.qwkid
    : (normalized && normalized.host ? normalized.host.toUpperCase() : "");

  return [
    nameKey,
    remoteKey
  ].join("|");
}

export function buildPrivateThreadName(nick: ChatNick): string {
  const normalized = normalizePrivateNick(nick);
  return "@" + (normalized ? normalized.name : "");
}

export function resolvePrivatePeerNick(message: ChatMessage, ownAlias: string): ChatNick | null {
  const sender = normalizePrivateNick(message.nick || null);
  const recipient = normalizePrivateNick(message.private ? message.private.to : null);

  if (!sender || !recipient) {
    return null;
  }

  if (sender.name.toUpperCase() === ownAlias.toUpperCase()) {
    return recipient;
  }

  return sender;
}
