import { buildNoticeMessage, normalizeChannelName, trimChannelMessages } from "../domain/chat-model";
import { buildPrivateMessage, buildPrivateThreadId, buildPrivateThreadName, isPrivateMessage, normalizePrivateNick, resolvePrivatePeerNick } from "../domain/private-messages";
import { ACTION_BAR_ACTIONS, ActionBarAction, AppModalState, ChannelListEntry, RosterEntry } from "../domain/ui";
import { InputBuffer } from "../input/input-buffer";
import { AvatarChatConfig } from "../io/config";
import { AvatarCache } from "../render/avatar";
import { renderActionBar, renderModal } from "../render/modal-renderer";
import { renderTranscript, TranscriptRenderState } from "../render/transcript-renderer";
import { clamp, clipText, padRight, trimText } from "../util/text";

interface FrameState {
  root: Frame | null;
  header: Frame | null;
  actions: Frame | null;
  transcript: Frame | null;
  animBg: Frame | null;
  animFg: Frame | null;
  status: Frame | null;
  input: Frame | null;
  overlay: Frame | null;
  modal: Frame | null;
  width: number;
  height: number;
}

interface ModalGeometry {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface PrivateThread {
  id: string;
  name: string;
  peerNick: ChatNick;
  messages: ChatMessage[];
  unreadCount: number;
}

const INPUT_ESCAPE_SEQUENCE_MAP: { [sequence: string]: string } = {
  "\x1b[A": KEY_UP,
  "\x1b[B": KEY_DOWN,
  "\x1b[C": KEY_RIGHT,
  "\x1b[D": KEY_LEFT,
  "\x1bOA": KEY_UP,
  "\x1bOB": KEY_DOWN,
  "\x1bOC": KEY_RIGHT,
  "\x1bOD": KEY_LEFT,
  "\x1b[H": KEY_HOME,
  "\x1b[F": KEY_END,
  "\x1bOH": KEY_HOME,
  "\x1bOF": KEY_END,
  "\x1b[1~": KEY_HOME,
  "\x1b[4~": KEY_END,
  "\x1b[7~": KEY_HOME,
  "\x1b[8~": KEY_END,
  "\x1b[5~": KEY_PAGEUP,
  "\x1b[6~": KEY_PAGEDN
};

const INPUT_ESCAPE_SEQUENCE_PREFIXES: { [prefix: string]: boolean } = {
  "\x1b": true
};

(function seedInputEscapePrefixes(): void {
  let sequence = "";
  let prefixLength = 0;

  for (sequence in INPUT_ESCAPE_SEQUENCE_MAP) {
    if (!Object.prototype.hasOwnProperty.call(INPUT_ESCAPE_SEQUENCE_MAP, sequence)) {
      continue;
    }

    for (prefixLength = 1; prefixLength < sequence.length; prefixLength += 1) {
      INPUT_ESCAPE_SEQUENCE_PREFIXES[sequence.substr(0, prefixLength)] = true;
    }
  }
})();

export class AvatarChatApp {
  private readonly config: AvatarChatConfig;
  private readonly inputBuffer: InputBuffer;
  private readonly avatarCache: AvatarCache;
  private avatarLib: AvatarLibrary | null;
  private chat: JSONChat | null;
  private privateThreads: { [id: string]: PrivateThread };
  private privateThreadOrder: string[];
  private lastPrivateSender: ChatNick | null;
  private frames: FrameState;
  private channelOrder: string[];
  private currentChannel: string;
  private reconnectAt: number;
  private lastError: string;
  private shouldExit: boolean;
  private modalState: AppModalState | null;
  private transcriptSignature: string;
  private headerSignature: string;
  private actionSignature: string;
  private statusSignature: string;
  private inputSignature: string;
  private modalSignature: string;
  private transcriptScrollOffsetBlocks: number;
  private transcriptVisibleBlockCount: number;
  private transcriptMaxScrollOffsetBlocks: number;
  private pendingEscapeSequence: string;
  private pendingEscapeAt: number;
  private lastWasCarriageReturn: boolean;
  private privateMessageKeys: { [key: string]: boolean };
  private publicChannelMessageKeys: { [key: string]: boolean };
  private publicChannelUnreadCounts: { [key: string]: number };
  private lastPrivateHistorySyncAt: number;
  private lastKeyTimestamp: number;
  private idleAnimActive: boolean;
  private animMgr: any;
  private animFrame: string;
  private idleTickInterval: number;
  private lastAnimTickAt: number;
  private embeddedAvatars: { [nameUpper: string]: string };
  private userBbsCache: { [nameUpper: string]: string };

  public constructor(config: AvatarChatConfig) {
    this.config = config;
    this.inputBuffer = new InputBuffer(config.inputMaxLength);
    this.avatarCache = {};
    this.avatarLib = null;
    this.chat = null;
    this.privateThreads = {};
    this.privateThreadOrder = [];
    this.lastPrivateSender = null;
    this.frames = {
      root: null,
      header: null,
      actions: null,
      transcript: null,
      animBg: null,
      animFg: null,
      status: null,
      input: null,
      overlay: null,
      modal: null,
      width: 0,
      height: 0
    };
    this.channelOrder = [normalizeChannelName(config.defaultChannel)];
    this.currentChannel = normalizeChannelName(config.defaultChannel);
    this.reconnectAt = 0;
    this.lastError = "";
    this.shouldExit = false;
    this.modalState = null;
    this.transcriptSignature = "";
    this.headerSignature = "";
    this.actionSignature = "";
    this.statusSignature = "";
    this.inputSignature = "";
    this.modalSignature = "";
    this.transcriptScrollOffsetBlocks = 0;
    this.transcriptVisibleBlockCount = 0;
    this.transcriptMaxScrollOffsetBlocks = 0;
    this.pendingEscapeSequence = "";
    this.pendingEscapeAt = 0;
    this.lastWasCarriageReturn = false;
    this.privateMessageKeys = {};
    this.publicChannelMessageKeys = {};
    this.publicChannelUnreadCounts = {};
    this.lastPrivateHistorySyncAt = 0;
    this.lastKeyTimestamp = Date.now();
    this.idleAnimActive = false;
    this.animMgr = null;
    this.animFrame = "";
    this.idleTickInterval = 0;
    this.lastAnimTickAt = 0;
    this.embeddedAvatars = {};
    this.userBbsCache = {};

    try {
      this.avatarLib = load({}, "avatar_lib.js") as AvatarLibrary;
    } catch (error) {
      this.avatarLib = null;
      log("Avatar Chat: avatar_lib.js unavailable: " + String(error));
    }
  }

  public run(): void {
    try {
      console.clear(BG_BLACK | LIGHTGRAY);
      bbs.command_str = "";
      this.connect();

      while (bbs.online && !js.terminated && !this.shouldExit) {
        this.cycleChat();
        this.reconnectIfDue();
        this.handlePendingInput();
        this.tickIdleAnimations();
        this.render();
      }
    } finally {
      this.destroyFrames();
      console.clear(BG_BLACK | LIGHTGRAY);
      console.home();
    }
  }

  private getOwnAvatarData(): string | undefined {
    let avatarObj = null;

    if (!this.avatarLib || typeof this.avatarLib.read !== "function") {
      return undefined;
    }

    try {
      avatarObj = this.avatarLib.read(user.number, user.alias, null, null) || null;
    } catch (_error) {
      avatarObj = null;
    }

    if (!avatarObj || avatarObj.disabled || !avatarObj.data) {
      return undefined;
    }

    return String(avatarObj.data);
  }

  private connect(): void {
    const desiredChannels = this.getJoinedPublicChannelNames();
    const desiredCurrent = this.currentChannel;
    let client;
    let index = 0;

    try {
      client = new JSONClient(this.config.host, this.config.port);
      this.chat = new JSONChat(user.number, client);
      this.chat.settings.MAX_HISTORY = this.config.maxHistory;
      this.installPrivateMessageHook();
      this.privateThreads = {};
      this.privateThreadOrder = [];
      this.lastPrivateSender = null;
      this.privateMessageKeys = {};
      this.publicChannelMessageKeys = {};
      this.publicChannelUnreadCounts = {};
      this.lastPrivateHistorySyncAt = 0;

      for (index = 0; index < desiredChannels.length; index += 1) {
        const desiredChannel = desiredChannels[index];
        const channelName = normalizeChannelName(desiredChannel || this.config.defaultChannel, this.config.defaultChannel);

        if (!this.getChannelByName(channelName)) {
          this.chat.join(channelName);
        }
      }

      this.loadPrivateHistory();
      this.syncPublicChannelUnreadCounts(false);
      this.syncChannelOrder();

      if (desiredCurrent && (this.getChannelByName(desiredCurrent) || this.getPrivateThreadByName(desiredCurrent))) {
        this.currentChannel = desiredCurrent;
      } else if (this.channelOrder.length > 0) {
        this.currentChannel = this.channelOrder[0] || this.config.defaultChannel;
      }
      this.markCurrentViewRead(this.currentChannel);

      this.transcriptScrollOffsetBlocks = 0;
      this.transcriptVisibleBlockCount = 0;
      this.transcriptMaxScrollOffsetBlocks = 0;
      this.reconnectAt = 0;
      this.lastError = "";
      this.resetRenderSignatures();
    } catch (error) {
      this.chat = null;
      this.scheduleReconnect("Connection failed: " + String(error));
    }
  }

  private cycleChat(): void {
    if (!this.chat) {
      return;
    }

    try {
      this.chat.cycle();
      this.syncPublicChannelUnreadCounts(true);
      this.syncPrivateHistory();
      this.syncChannelOrder();
      this.trimHistories();
    } catch (error) {
      try {
        this.chat.disconnect();
      } catch (_disconnectError) {
      }
      this.chat = null;
      this.scheduleReconnect("Connection lost: " + String(error));
    }
  }

  private installPrivateMessageHook(): void {
    let originalUpdate: ((packet: any) => boolean) | null = null;

    if (!this.chat || typeof this.chat.update !== "function") {
      return;
    }

    originalUpdate = this.chat.update.bind(this.chat);
    this.chat.update = (packet: any): boolean => {
      if (this.handlePrivateMailboxPacket(packet)) {
        return true;
      }

      return originalUpdate ? originalUpdate(packet) : false;
    };
  }

  private loadPrivateHistory(): void {
    const historyLimit = Math.max(50, this.config.maxHistory * 2);
    let history: any[] = [];
    let index = 0;

    if (!this.chat) {
      return;
    }

    this.ensureHistoryArray(this.getMailboxHistoryPath());

    try {
      history = this.chat.client.slice("chat", this.getMailboxHistoryPath(), -historyLimit, undefined, 1) || [];
    } catch (_error) {
      history = [];
    }

    for (index = 0; index < history.length; index += 1) {
      const message = history[index] as ChatMessage;

      if (!isPrivateMessage(message)) {
        continue;
      }

      this.appendPrivateMessage(message, false);
    }

    this.lastPrivateHistorySyncAt = new Date().getTime();
  }

  private handlePrivateMailboxPacket(packet: any): boolean {
    let message: ChatMessage | null = null;

    if (!packet || packet.oper !== "WRITE" || packet.location !== this.getMailboxMessagesPath()) {
      return false;
    }

    message = packet.data as ChatMessage;
    if (!isPrivateMessage(message)) {
      return false;
    }

    this.appendPrivateMessage(message, true);
    return true;
  }

  private appendPrivateMessage(message: ChatMessage, markUnread: boolean): void {
    const peerNick = resolvePrivatePeerNick(message, user.alias);
    let thread;

    if (!peerNick) {
      return;
    }

    if (!this.rememberPrivateMessage(message)) {
      return;
    }

    thread = this.ensurePrivateThread(peerNick);
    thread.messages.push(message);
    trimChannelMessages(thread as any, this.config.maxHistory);

    if (
      markUnread &&
      message.nick &&
      message.nick.name &&
      message.nick.name.toUpperCase() !== user.alias.toUpperCase() &&
      this.currentChannel.toUpperCase() !== thread.name.toUpperCase()
    ) {
      thread.unreadCount += 1;
      this.lastPrivateSender = peerNick;
    }

    if (message.nick && message.nick.name && message.nick.name.toUpperCase() !== user.alias.toUpperCase()) {
      this.lastPrivateSender = peerNick;
    }

    this.syncChannelOrder();
    this.resetRenderSignatures();
  }

  private ensurePrivateThread(peerNick: ChatNick): PrivateThread {
    const normalizedPeer = normalizePrivateNick(peerNick) || peerNick;
    const canonicalPeer = this.resolvePrivateTargetNick(normalizedPeer.name) || normalizedPeer;
    const threadId = buildPrivateThreadId(canonicalPeer);
    let thread = this.privateThreads[threadId];
    let previousName = "";

    if (!thread) {
      thread = {
        id: threadId,
        name: buildPrivateThreadName(canonicalPeer),
        peerNick: canonicalPeer,
        messages: [],
        unreadCount: 0
      };
      this.privateThreads[threadId] = thread;
      this.privateThreadOrder.push(threadId);
    } else {
      previousName = thread.name;
      thread.peerNick = canonicalPeer;
      thread.name = buildPrivateThreadName(canonicalPeer);
      if (previousName.length && this.currentChannel.toUpperCase() === previousName.toUpperCase()) {
        this.currentChannel = thread.name;
      }
    }

    return thread;
  }

  private getPrivateThreadByName(name: string): PrivateThread | null {
    let index = 0;

    for (index = 0; index < this.privateThreadOrder.length; index += 1) {
      const threadId = this.privateThreadOrder[index];
      const thread = threadId ? this.privateThreads[threadId] : null;

      if (thread && thread.name.toUpperCase() === name.toUpperCase()) {
        return thread;
      }
    }

    return null;
  }

  private getJoinedPublicChannelNames(): string[] {
    const publicChannels: string[] = [];
    let index = 0;

    for (index = 0; index < this.channelOrder.length; index += 1) {
      const channelName = this.channelOrder[index];
      if (channelName && channelName.charAt(0) !== "@") {
        publicChannels.push(channelName);
      }
    }

    if (!publicChannels.length) {
      publicChannels.push(normalizeChannelName(this.config.defaultChannel));
    }

    return publicChannels;
  }

  private getMailboxMessagesPath(): string {
    return "channels." + user.alias + ".messages";
  }

  private getMailboxHistoryPath(): string {
    return "channels." + user.alias + ".history";
  }

  private ensureHistoryArray(location: string): void {
    let existing;

    if (!this.chat) {
      return;
    }

    try {
      existing = this.chat.client.read("chat", location, 1);
    } catch (_readError) {
      existing = null;
    }

    if (existing instanceof Array) {
      return;
    }

    this.chat.client.write("chat", location, [], 2);
  }

  private buildPublicMessageKey(channelName: string, message: ChatMessage): string {
    const sender = normalizePrivateNick(message.nick || null);
    const senderKey = sender
      ? (this.normalizePrivateLookupKey(sender.name) + "|" + (sender.qwkid || String(sender.host || "").toUpperCase()))
      : "NOTICE";

    return [
      normalizeChannelName(channelName).toUpperCase(),
      String(message.time || 0),
      senderKey,
      String(message.str || "")
    ].join("|");
  }

  private rememberPublicChannelMessage(channelName: string, message: ChatMessage): boolean {
    const key = this.buildPublicMessageKey(channelName, message);

    if (this.publicChannelMessageKeys[key]) {
      return false;
    }

    this.publicChannelMessageKeys[key] = true;
    return true;
  }

  private syncPublicChannelUnreadCounts(markUnread: boolean): void {
    let key = "";
    let changed = false;

    if (!this.chat) {
      return;
    }

    for (key in this.chat.channels) {
      if (!Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
        continue;
      }

      const channel: ChatChannel | null = this.chat.channels[key] as ChatChannel;
      const unreadKey = channel && channel.name ? channel.name.toUpperCase() : "";
      let index = 0;

      if (!channel || !channel.name || channel.name.charAt(0) === "@") {
        continue;
      }

      if (this.publicChannelUnreadCounts[unreadKey] === undefined) {
        this.publicChannelUnreadCounts[unreadKey] = 0;
      }

      for (index = 0; index < channel.messages.length; index += 1) {
        const message = channel.messages[index];

        if (message && message.nick && message.nick.avatar) {
          const avatarData = String(message.nick.avatar).replace(/^\s+|\s+$/g, "");
          if (avatarData.length) {
            this.embeddedAvatars[message.nick.name.toUpperCase()] = avatarData;
          }
        }

        if (!message || !this.rememberPublicChannelMessage(channel.name, message)) {
          continue;
        }

        // Enrich join/leave notices with BBS name
        if (!message.nick) {
          this.enrichJoinLeaveNotice(message, channel);
        }

        if (
          markUnread &&
          channel.name.toUpperCase() !== this.currentChannel.toUpperCase() &&
          message.nick &&
          message.nick.name &&
          message.nick.name.toUpperCase() !== user.alias.toUpperCase()
        ) {
          this.publicChannelUnreadCounts[unreadKey] += 1;
          changed = true;
        }
      }
    }

    if (changed) {
      this.modalSignature = "";
    }
  }

  private buildPrivateMessageKey(message: ChatMessage): string {
    const sender = normalizePrivateNick(message.nick || null);
    const recipient = normalizePrivateNick(message.private ? message.private.to : null);
    const senderKey = sender
      ? (this.normalizePrivateLookupKey(sender.name) + "|" + (sender.qwkid || String(sender.host || "").toUpperCase()))
      : "";
    const recipientKey = recipient
      ? (this.normalizePrivateLookupKey(recipient.name) + "|" + (recipient.qwkid || String(recipient.host || "").toUpperCase()))
      : "";

    return [
      String(message.time || 0),
      senderKey,
      recipientKey,
      String(message.str || "")
    ].join("|");
  }

  private rememberPrivateMessage(message: ChatMessage): boolean {
    const key = this.buildPrivateMessageKey(message);

    if (this.privateMessageKeys[key]) {
      return false;
    }

    this.privateMessageKeys[key] = true;
    return true;
  }

  private syncPrivateHistory(): void {
    const now = new Date().getTime();
    const historyLimit = Math.max(50, this.config.maxHistory * 2);
    let history: any[] = [];
    let index = 0;

    if (!this.chat) {
      return;
    }

    if (this.lastPrivateHistorySyncAt && now - this.lastPrivateHistorySyncAt < 1000) {
      return;
    }

    this.lastPrivateHistorySyncAt = now;

    try {
      history = this.chat.client.slice("chat", this.getMailboxHistoryPath(), -historyLimit, undefined, 1) || [];
    } catch (_error) {
      history = [];
    }

    for (index = 0; index < history.length; index += 1) {
      const message = history[index] as ChatMessage;

      if (!isPrivateMessage(message)) {
        continue;
      }

      this.appendPrivateMessage(message, true);
    }
  }

  private reconnectIfDue(): void {
    if (this.chat || !this.reconnectAt) {
      return;
    }

    if (new Date().getTime() >= this.reconnectAt) {
      this.connect();
    }
  }

  private scheduleReconnect(errorText: string): void {
    this.lastError = errorText;
    this.reconnectAt = new Date().getTime() + this.config.reconnectDelayMs;
    this.resetRenderSignatures();
  }

  private handlePendingInput(): void {
    const key = this.readInputKey();

    if (!key) {
      return;
    }

    // Any keypress resets idle timer and stops active animations
    this.lastKeyTimestamp = Date.now();
    if (this.idleAnimActive) {
      this.stopIdleAnimations();
    }

    if (this.modalState && this.handleModalInput(key)) {
      return;
    }

    switch (key) {
      case KEY_ESC:
      case "\x1b":
      case "\x11":
        this.performAction("exit");
        return;
      case KEY_UP:
        this.scrollTranscriptOlder(1);
        return;
      case KEY_DOWN:
        this.scrollTranscriptNewer(1);
        return;
      case KEY_PAGEUP:
        this.pageTranscriptOlder();
        return;
      case KEY_PAGEDN:
        this.pageTranscriptNewer();
        return;
      case "\r":
        this.submitInput();
        return;
      case "\t":
        this.handleTabCompletion();
        return;
      case KEY_LEFT:
        this.inputBuffer.moveLeft();
        return;
      case KEY_RIGHT:
        this.inputBuffer.moveRight();
        return;
      case KEY_HOME:
        if (this.inputBuffer.isEmpty()) {
          this.scrollTranscriptToOldest();
          return;
        }
        this.inputBuffer.moveHome();
        return;
      case KEY_END:
        if (this.inputBuffer.isEmpty()) {
          this.scrollTranscriptToLatest();
          return;
        }
        this.inputBuffer.moveEnd();
        return;
      case "\b":
      case "\x7f":
        this.inputBuffer.backspace();
        return;
      case "\x0c":
        this.resetRenderSignatures();
        return;
      default:
        if (this.isPrintable(key)) {
          this.inputBuffer.insert(key);
        }
        return;
    }
  }

  private readInputKey(): string {
    const mode = K_NOCRLF | K_NOECHO | K_NOSPIN | K_EXTKEYS;
    const key = console.inkey(mode, this.pendingEscapeSequence.length ? 0 : this.config.pollDelayMs);

    if (!key) {
      if (this.pendingEscapeSequence.length && new Date().getTime() - this.pendingEscapeAt >= 75) {
        this.pendingEscapeSequence = "";
        this.pendingEscapeAt = 0;
        return KEY_ESC;
      }
      return "";
    }

    return this.normalizeInputKey(key, mode);
  }

  private normalizeInputKey(key: string, mode: number): string {
    let next = "";
    let mapped = "";

    if (this.pendingEscapeSequence.length) {
      this.pendingEscapeSequence += key;
      return this.resolveEscapeSequence(mode);
    }

    if (key === KEY_ESC || key === "\x1b") {
      this.pendingEscapeSequence = KEY_ESC;
      this.pendingEscapeAt = new Date().getTime();
      return this.resolveEscapeSequence(mode);
    }

    if (key === "\r") {
      this.lastWasCarriageReturn = true;
      return key;
    }

    if (key === "\n") {
      if (this.lastWasCarriageReturn) {
        this.lastWasCarriageReturn = false;
        return "";
      }
      return KEY_DOWN;
    }

    this.lastWasCarriageReturn = false;
    mapped = this.normalizeImmediateKey(key);
    if (mapped.length) {
      return mapped;
    }

    next = INPUT_ESCAPE_SEQUENCE_MAP[key] || "";
    return next.length ? next : key;
  }

  private normalizeImmediateKey(key: string): string {
    switch (key) {
      case "KEY_UP":
      case KEY_UP:
        return KEY_UP;
      case "KEY_DOWN":
      case KEY_DOWN:
        return KEY_DOWN;
      case "KEY_LEFT":
      case KEY_LEFT:
        return KEY_LEFT;
      case "KEY_RIGHT":
      case KEY_RIGHT:
        return KEY_RIGHT;
      case "KEY_HOME":
      case KEY_HOME:
        return KEY_HOME;
      case "KEY_END":
      case KEY_END:
        return KEY_END;
      case "KEY_PAGEUP":
      case KEY_PAGEUP:
        return KEY_PAGEUP;
      case "KEY_PAGEDN":
      case KEY_PAGEDN:
        return KEY_PAGEDN;
      default:
        return "";
    }
  }

  private resolveEscapeSequence(mode: number): string {
    let next = "";
    let mapped = "";

    while (this.pendingEscapeSequence.length) {
      mapped = INPUT_ESCAPE_SEQUENCE_MAP[this.pendingEscapeSequence] || "";
      if (mapped.length) {
        this.pendingEscapeSequence = "";
        this.pendingEscapeAt = 0;
        this.lastWasCarriageReturn = false;
        return mapped;
      }

      if (!INPUT_ESCAPE_SEQUENCE_PREFIXES[this.pendingEscapeSequence]) {
        this.pendingEscapeSequence = "";
        this.pendingEscapeAt = 0;
        return KEY_ESC;
      }

      next = console.inkey(mode, 0);
      if (!next) {
        return "";
      }

      this.pendingEscapeSequence += next;
    }

    return "";
  }

  private handleModalInput(key: string): boolean {
    if (!this.modalState) {
      return false;
    }

    switch (key) {
      case KEY_UP:
        this.moveModalSelection(-1);
        return true;
      case KEY_DOWN:
        this.moveModalSelection(1);
        return true;
      case KEY_HOME:
        this.jumpModalSelection(0);
        return true;
      case KEY_END:
        this.jumpModalSelection(this.getModalItemCount() - 1);
        return true;
      case KEY_PAGEUP:
        this.moveModalSelection(-5);
        return true;
      case KEY_PAGEDN:
        this.moveModalSelection(5);
        return true;
      case "\r":
        if ((this.modalState.kind === "channels" || this.modalState.kind === "private") && this.modalState.entries.length) {
          const selected = this.modalState.entries[this.modalState.selectedIndex];
          if (selected) {
            this.currentChannel = selected.name;
            this.markCurrentViewRead(this.currentChannel);
            this.scrollTranscriptToLatest();
          }
        }
        this.closeModal();
        return true;
      case KEY_ESC:
      case "\x1b":
        this.closeModal();
        return true;
      case "\x0c":
        this.resetRenderSignatures();
        return true;
      default:
        return true;
    }
  }

  private submitInput(): void {
    const text = this.inputBuffer.getValue();
    const trimmed = trimText(text);
    const privateThread = this.currentChannel.length ? this.getPrivateThreadByName(this.currentChannel) : null;

    if (!trimmed.length) {
      this.inputBuffer.clear();
      return;
    }

    if (trimmed.charAt(0) === "/") {
      this.handleSlashCommand(trimmed);
      this.inputBuffer.clear();
      return;
    }

    if (!this.chat || !this.currentChannel.length) {
      this.lastError = "Not connected to chat";
      this.inputBuffer.clear();
      return;
    }

    this.scrollTranscriptToLatest();

    if (privateThread) {
      if (!this.sendPrivateMessage(privateThread.peerNick, trimmed)) {
        this.appendViewNotice(this.currentChannel, "Unable to send private message.");
      }
    } else if (!this.sendMessage(this.currentChannel, trimmed)) {
      this.appendNotice(this.currentChannel, "Unable to send message.");
    }

    this.inputBuffer.clear();
  }

  private sendMessage(channelName: string, text: string): boolean {
    const channel = this.getChannelByName(channelName);
    const timestamp = new Date().getTime();
    const ownAvatar = this.getOwnAvatarData();
    const message = {
      nick: {
        name: user.alias,
        host: system.name,
        ip: user.ip_address,
        qwkid: system.qwk_id,
        avatar: ownAvatar
      },
      str: text,
      time: timestamp
    };

    if (!this.chat || !channel) {
      return false;
    }

    try {
      this.chat.client.write("chat", "channels." + channel.name + ".messages", message, 2);
      this.chat.client.push("chat", "channels." + channel.name + ".history", message, 2);
      channel.messages.push(message);
      trimChannelMessages(channel, this.config.maxHistory);
      this.transcriptSignature = "";
      this.statusSignature = "";
      return true;
    } catch (_error) {
      return false;
    }
  }

  private sendPrivateMessage(targetNick: ChatNick, text: string): boolean {
    const recipient = normalizePrivateNick(targetNick);
    const ownAvatar = this.getOwnAvatarData();
    const sender = normalizePrivateNick({
      name: user.alias,
      host: system.name,
      ip: user.ip_address,
      qwkid: system.qwk_id,
      avatar: ownAvatar
    });
    const timestamp = new Date().getTime();
    let thread;
    let message;

    if (!this.chat || !recipient || !sender) {
      return false;
    }

    message = buildPrivateMessage(sender, recipient, text, timestamp);

    try {
      this.ensureHistoryArray("channels." + recipient.name + ".history");
      this.ensureHistoryArray(this.getMailboxHistoryPath());
      this.chat.client.write("chat", "channels." + recipient.name + ".messages", message, 2);
      this.chat.client.push("chat", "channels." + recipient.name + ".history", message, 2);
      this.chat.client.push("chat", this.getMailboxHistoryPath(), message, 2);
      this.rememberPrivateMessage(message);
      thread = this.ensurePrivateThread(recipient);
      thread.messages.push(message);
      thread.unreadCount = 0;
      trimChannelMessages(thread as any, this.config.maxHistory);
      this.currentChannel = thread.name;
      this.markCurrentViewRead(this.currentChannel);
      this.syncChannelOrder();
      this.transcriptSignature = "";
      this.statusSignature = "";
      this.headerSignature = "";
      return true;
    } catch (_error) {
      return false;
    }
  }

  private handleSlashCommand(commandText: string): void {
    const trimmedCommand = trimText(commandText.substr(1));
    const parts = trimmedCommand.split(/\s+/);
    const firstPart = parts.length ? (parts[0] || "") : "";
    const verb = firstPart.length ? firstPart.toUpperCase() : "";
    const args = trimmedCommand.length > verb.length ? trimText(trimmedCommand.substr(verb.length)) : "";
    let targetChannel = "";

    if (!verb.length) {
      return;
    }

    if (verb === "MSG" || verb === "PM" || verb === "TELL" || verb === "WHISPER") {
      this.handlePrivateCommand(args);
      return;
    }

    if (verb === "R" || verb === "REPLY") {
      this.handlePrivateReplyCommand(args);
      return;
    }

    switch (verb) {
      case "HELP":
        this.performAction("help");
        return;
      case "WHO":
        this.performAction("who");
        return;
      case "CHANNELS":
        this.performAction("channels");
        return;
      case "PRIVATE":
      case "PRIVATES":
      case "PMS":
        this.performAction("private");
        return;
      case "EXIT":
      case "QUIT":
        this.performAction("exit");
        return;
      case "OPEN":
      case "CONNECT":
        if (!this.chat) {
          this.connect();
        }
        return;
      case "CLOSE":
      case "DISCONNECT":
        if (this.chat) {
          try {
            this.chat.disconnect();
          } catch (_disconnectError) {
          }
          this.chat = null;
          this.scheduleReconnect("Disconnected.");
        }
        return;
      default:
        break;
    }

    if (!this.chat || !this.currentChannel.length) {
      this.lastError = "Not connected to chat";
      return;
    }

    if (verb === "JOIN" && !args.length) {
      this.appendNotice(this.currentChannel, "Usage: /join <channel>");
      return;
    }

    if (verb === "ME" && !args.length) {
      this.appendNotice(this.currentChannel, "Usage: /me <action>");
      return;
    }

    if (!this.chat.getcmd(this.currentChannel, commandText)) {
      this.appendNotice(this.currentChannel, "Unknown command: " + commandText);
      return;
    }

    if (verb === "JOIN") {
      targetChannel = normalizeChannelName(args, this.config.defaultChannel);
      this.syncChannelOrder();
      if (this.getChannelByName(targetChannel)) {
        this.currentChannel = targetChannel;
        this.markCurrentViewRead(this.currentChannel);
        this.scrollTranscriptToLatest();
      }
      return;
    }

    if (verb === "PART") {
      this.syncChannelOrder();
      if (!this.getChannelByName(this.currentChannel)) {
        this.currentChannel = this.channelOrder.length ? (this.channelOrder[0] || "") : "";
        this.markCurrentViewRead(this.currentChannel);
        this.scrollTranscriptToLatest();
      }
      this.resetRenderSignatures();
      return;
    }

    this.resetRenderSignatures();
  }

  private handlePrivateCommand(args: string): void {
    const parsed = this.parsePrivateCommandArgs(args);
    let recipientName = "";
    let messageText = "";
    let targetNick;

    if (!this.chat) {
      this.lastError = "Not connected to chat";
      return;
    }

    if (!parsed) {
      this.appendViewNotice(this.currentChannel, "Usage: /msg <user> <message>");
      return;
    }

    recipientName = parsed.recipientName;
    messageText = parsed.messageText;

    targetNick = this.resolvePrivateTargetNick(recipientName);
    if (!targetNick) {
      this.appendViewNotice(this.currentChannel, "User not found: " + recipientName + ".");
      return;
    }

    if (!this.sendPrivateMessage(targetNick, messageText)) {
      this.appendViewNotice(this.currentChannel, "Unable to send private message to " + recipientName + ".");
      return;
    }

    this.scrollTranscriptToLatest();
  }

  private handlePrivateReplyCommand(args: string): void {
    if (!this.chat) {
      this.lastError = "Not connected to chat";
      return;
    }

    if (!args.length) {
      this.appendViewNotice(this.currentChannel, "Usage: /r <message>");
      return;
    }

    if (!this.lastPrivateSender || !this.sendPrivateMessage(this.lastPrivateSender, args)) {
      this.appendViewNotice(this.currentChannel, "Nobody has private messaged you yet.");
      return;
    }

    this.scrollTranscriptToLatest();
  }

  private resolvePrivateTargetNick(name: string): ChatNick | null {
    const trimmedName = trimText(name);
    const normalizedTarget = this.normalizePrivateLookupKey(trimmedName);
    let key = "";
    let index = 0;

    if (!trimmedName.length) {
      return null;
    }

    if (this.chat) {
      for (key in this.chat.channels) {
        if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
          const channel = this.chat.channels[key];
          let rosterIndex = 0;

          if (!channel || !channel.users) {
            continue;
          }

          for (rosterIndex = 0; rosterIndex < channel.users.length; rosterIndex += 1) {
            const rosterEntry = this.extractRosterEntry(channel.users[rosterIndex]);

            if (
              rosterEntry &&
              rosterEntry.nick &&
              (
                rosterEntry.name.toUpperCase() === trimmedName.toUpperCase() ||
                this.normalizePrivateLookupKey(rosterEntry.name) === normalizedTarget
              )
            ) {
              return rosterEntry.nick;
            }
          }
        }
      }
    }

    for (index = 0; index < this.privateThreadOrder.length; index += 1) {
      const threadId = this.privateThreadOrder[index];
      const thread = threadId ? this.privateThreads[threadId] : null;

      if (
        thread &&
        (
          thread.peerNick.name.toUpperCase() === trimmedName.toUpperCase() ||
          this.normalizePrivateLookupKey(thread.peerNick.name) === normalizedTarget
        )
      ) {
        return thread.peerNick;
      }
    }

    return null;
  }

  private normalizePrivateLookupKey(text: string): string {
    return trimText(text).replace(/[^A-Za-z0-9]/g, "").toUpperCase();
  }

  private parsePrivateCommandArgs(args: string): { recipientName: string; messageText: string } | null {
    const trimmedArgs = trimText(args);
    let recipientName = "";
    let messageText = "";
    let closingQuoteIndex = 0;
    let spaceIndex = 0;
    let candidateNames;
    let candidateIndex = 0;

    if (!trimmedArgs.length) {
      return null;
    }

    if (trimmedArgs.charAt(0) === "\"") {
      closingQuoteIndex = trimmedArgs.indexOf("\"", 1);
      if (closingQuoteIndex < 2) {
        return null;
      }
      recipientName = trimText(trimmedArgs.substring(1, closingQuoteIndex));
      messageText = trimText(trimmedArgs.substr(closingQuoteIndex + 1));
      return recipientName.length && messageText.length
        ? { recipientName: recipientName, messageText: messageText }
        : null;
    }

    candidateNames = this.listPrivateTargetNames();
    for (candidateIndex = 0; candidateIndex < candidateNames.length; candidateIndex += 1) {
      const candidateName = candidateNames[candidateIndex] || "";

      if (!candidateName.length) {
        continue;
      }

      if (trimmedArgs.substr(0, candidateName.length).toUpperCase() !== candidateName.toUpperCase()) {
        continue;
      }

      if (trimmedArgs.length > candidateName.length && trimmedArgs.charAt(candidateName.length) !== " ") {
        continue;
      }

      recipientName = candidateName;
      messageText = trimText(trimmedArgs.substr(candidateName.length));
      if (recipientName.length && messageText.length) {
        return {
          recipientName: recipientName,
          messageText: messageText
        };
      }
    }

    spaceIndex = trimmedArgs.indexOf(" ");
    if (spaceIndex < 1) {
      return null;
    }

    recipientName = trimText(trimmedArgs.substr(0, spaceIndex));
    messageText = trimText(trimmedArgs.substr(spaceIndex + 1));
    return recipientName.length && messageText.length
      ? { recipientName: recipientName, messageText: messageText }
      : null;
  }

  private listPrivateTargetNames(): string[] {
    const seen: { [key: string]: boolean } = {};
    const names: string[] = [];
    let index = 0;
    let key = "";

    for (index = 0; index < this.privateThreadOrder.length; index += 1) {
      const threadId = this.privateThreadOrder[index];
      const thread = threadId ? this.privateThreads[threadId] : null;
      const name = thread && thread.peerNick ? trimText(thread.peerNick.name) : "";

      if (name.length && !seen[name.toUpperCase()]) {
        seen[name.toUpperCase()] = true;
        names.push(name);
      }
    }

    if (this.chat) {
      for (key in this.chat.channels) {
        if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
          const channel = this.chat.channels[key];

          if (!channel || !channel.users) {
            continue;
          }

          for (index = 0; index < channel.users.length; index += 1) {
            const rosterEntry = this.extractRosterEntry(channel.users[index]);
            const name = rosterEntry ? trimText(rosterEntry.name) : "";

            if (name.length && !seen[name.toUpperCase()]) {
              seen[name.toUpperCase()] = true;
              names.push(name);
            }
          }
        }
      }
    }

    names.sort(function (left, right) {
      return right.length - left.length;
    });

    return names;
  }

  private handleTabCompletion(): void {
    const completion = this.buildTabCompletion();

    if (!completion) {
      return;
    }

    this.inputBuffer.setValue(completion.value, completion.cursor);
    this.inputSignature = "";
  }

  private buildTabCompletion(): { value: string; cursor: number } | null {
    const value = this.inputBuffer.getValue();
    const cursor = this.inputBuffer.getCursor();
    const privateCompletion = this.buildPrivateCommandTabCompletion(value, cursor);

    if (privateCompletion) {
      return privateCompletion;
    }

    return this.buildGenericUserTabCompletion(value, cursor);
  }

  private buildPrivateCommandTabCompletion(value: string, cursor: number): { value: string; cursor: number } | null {
    const beforeCursor = value.substr(0, cursor);
    const afterCursor = value.substr(cursor);
    const match = beforeCursor.match(/^(\/(?:msg|pm|tell|whisper)\s+)([\s\S]*)$/i);
    let fragment = "";
    let candidate = "";
    let completed = "";

    if (!match || trimText(afterCursor).length) {
      return null;
    }

    fragment = match[2] || "";
    if (!fragment.length) {
      return null;
    }

    if (fragment.charAt(0) === "\"") {
      if (fragment.indexOf("\"", 1) >= 0) {
        return null;
      }
      fragment = trimText(fragment.substr(1));
    } else {
      fragment = trimText(fragment);
    }

    if (!fragment.length) {
      return null;
    }

    candidate = this.findAutocompleteCandidate(fragment, this.listPrivateTargetNames());
    if (!candidate.length) {
      return null;
    }

    completed = match[1] + (candidate.indexOf(" ") >= 0 ? ("\"" + candidate + "\" ") : (candidate + " "));
    return {
      value: completed,
      cursor: completed.length
    };
  }

  private buildGenericUserTabCompletion(value: string, cursor: number): { value: string; cursor: number } | null {
    const beforeCursor = value.substr(0, cursor);
    const afterCursor = value.substr(cursor);
    const match = beforeCursor.match(/(^|[\s])([^\s"]+)$/);
    let prefix = "";
    let fragment = "";
    let candidate = "";
    let completed = "";

    if (trimText(afterCursor).length || (beforeCursor.length && beforeCursor.charAt(0) === "/") || !match) {
      return null;
    }

    fragment = match[2] || "";
    prefix = beforeCursor.substr(0, beforeCursor.length - fragment.length);

    if (!fragment.length) {
      return null;
    }

    candidate = this.findAutocompleteCandidate(fragment, this.listPrivateTargetNames());
    if (!candidate.length) {
      return null;
    }

    completed = prefix + candidate + " ";
    return {
      value: completed,
      cursor: completed.length
    };
  }

  private findAutocompleteCandidate(fragment: string, names: string[]): string {
    const normalizedFragment = this.normalizePrivateLookupKey(fragment);
    let index = 0;

    if (!normalizedFragment.length) {
      return "";
    }

    for (index = 0; index < names.length; index += 1) {
      const candidate = names[index] || "";

      if (candidate.length && this.normalizePrivateLookupKey(candidate).indexOf(normalizedFragment) === 0) {
        return candidate;
      }
    }

    return "";
  }

  private performAction(actionId: string): void {
    switch (actionId) {
      case "who":
        this.openRosterModal();
        return;
      case "channels":
        this.openChannelsModal();
        return;
      case "private":
        this.openPrivateThreadsModal();
        return;
      case "help":
        this.openHelpModal();
        return;
      case "exit":
        this.shouldExit = true;
        return;
      default:
        return;
    }
  }

  private openRosterModal(): void {
    const entries = this.buildRosterEntries();
    let selectedIndex = 0;
    let index = 0;

    for (index = 0; index < entries.length; index += 1) {
      const entry = entries[index];
      if (entry && entry.isSelf) {
        selectedIndex = index;
        break;
      }
    }

    this.modalState = {
      kind: "roster",
      title: "Who's Here",
      selectedIndex: selectedIndex,
      entries: entries
    };
    this.resetRenderSignatures();
  }

  private openChannelsModal(): void {
    const entries = this.buildChannelEntries();
    let selectedIndex = 0;
    let index = 0;

    for (index = 0; index < entries.length; index += 1) {
      const entry = entries[index];
      if (entry && entry.isCurrent) {
        selectedIndex = index;
        break;
      }
    }

    this.modalState = {
      kind: "channels",
      title: "Channels",
      selectedIndex: selectedIndex,
      entries: entries
    };
    this.resetRenderSignatures();
  }

  private openPrivateThreadsModal(): void {
    const entries = this.buildPrivateThreadEntries();
    let selectedIndex = 0;
    let index = 0;

    for (index = 0; index < entries.length; index += 1) {
      const entry = entries[index];
      if (entry && entry.isCurrent) {
        selectedIndex = index;
        break;
      }
      if (entry && entry.metaText && entry.metaText.indexOf("new ") === 0) {
        selectedIndex = index;
      }
    }

    this.modalState = {
      kind: "private",
      title: "Private Threads",
      selectedIndex: selectedIndex,
      entries: entries
    };
    this.resetRenderSignatures();
  }

  private openHelpModal(): void {
    this.modalState = {
      kind: "help",
      title: "Help",
      selectedIndex: 0,
      lines: [
        "Slash commands:",
        "/who, /channels, /private, /help, /join <channel>, /part [channel], /me <action>, /msg <user> <message>, /r <message>, /clear",
        "",
        "Keys:",
        "Tab autocompletes user names.",
        "Esc exits the chat or closes a modal.",
        "Arrow keys, Home/End, and Backspace edit the input line.",
        "",
        "The top action bar is keyboard-first today and can grow mouse support later."
      ]
    };
    this.resetRenderSignatures();
  }

  private closeModal(): void {
    this.modalState = null;
    this.destroyModalFrames();
    this.resetRenderSignatures();
  }

  private getModalItemCount(): number {
    if (!this.modalState) {
      return 0;
    }

    if (this.modalState.kind === "help") {
      return 0;
    }

    return this.modalState.entries.length;
  }

  private moveModalSelection(delta: number): void {
    const itemCount = this.getModalItemCount();

    if (!this.modalState || this.modalState.kind === "help" || itemCount < 1) {
      return;
    }

    this.modalState.selectedIndex = clamp(this.modalState.selectedIndex + delta, 0, itemCount - 1);
  }

  private jumpModalSelection(index: number): void {
    const itemCount = this.getModalItemCount();

    if (!this.modalState || this.modalState.kind === "help" || itemCount < 1) {
      return;
    }

    this.modalState.selectedIndex = clamp(index, 0, itemCount - 1);
  }

  private buildRosterEntries(): RosterEntry[] {
    const entries: RosterEntry[] = [];
    const seen: { [key: string]: boolean } = {};
    const channel = this.currentChannel.length ? this.getChannelByName(this.currentChannel) : null;
    let index = 0;

    this.addRosterEntry(
      entries,
      seen,
      user.alias,
      system.name,
      { name: user.alias, host: system.name, ip: user.ip_address, qwkid: system.qwk_id },
      true
    );

    if (channel && channel.users) {
      for (index = 0; index < channel.users.length; index += 1) {
        const rawEntry = channel.users[index] as any;
        const rosterEntry = this.extractRosterEntry(rawEntry);

        if (rosterEntry) {
          this.addRosterEntry(
            entries,
            seen,
            rosterEntry.name,
            rosterEntry.bbs,
            rosterEntry.nick,
            rosterEntry.isSelf
          );
        }
      }
    }

    entries.sort(function (left, right) {
      if (left.isSelf !== right.isSelf) {
        return left.isSelf ? -1 : 1;
      }
      if (left.name.toUpperCase() < right.name.toUpperCase()) {
        return -1;
      }
      if (left.name.toUpperCase() > right.name.toUpperCase()) {
        return 1;
      }
      if (left.bbs.toUpperCase() < right.bbs.toUpperCase()) {
        return -1;
      }
      if (left.bbs.toUpperCase() > right.bbs.toUpperCase()) {
        return 1;
      }
      return 0;
    });

    return entries;
  }

  private addRosterEntry(
    entries: RosterEntry[],
    seen: { [key: string]: boolean },
    name: string,
    bbs: string,
    nick: ChatNick | null,
    isSelf: boolean
  ): void {
    const trimmedName = trimText(name);
    const trimmedBbs = trimText(bbs || system.name);
    const key = trimmedName.toUpperCase() + "|" + trimmedBbs.toUpperCase();

    if (!trimmedName.length || seen[key]) {
      return;
    }

    seen[key] = true;
    entries.push({
      name: trimmedName,
      bbs: trimmedBbs || "Unknown BBS",
      nick: nick,
      isSelf: isSelf
    });
  }

  private extractRosterEntry(rawEntry: any): RosterEntry | null {
    let name = "";
    let bbs = "";
    let nick: ChatNick | null = null;
    let nickValue = null;

    if (!rawEntry) {
      return null;
    }

    nickValue = rawEntry.nick;

    if (nickValue && typeof nickValue === "object") {
      name = trimText(String(nickValue.name || nickValue.alias || nickValue.user || ""));
      bbs = trimText(String(nickValue.host || rawEntry.system || rawEntry.host || rawEntry.bbs || system.name));
      nick = {
        name: name,
        host: bbs,
        ip: nickValue.ip || rawEntry.ip || undefined,
        qwkid: nickValue.qwkid || rawEntry.qwkid || undefined,
        avatar: nickValue.avatar || undefined
      };
    } else {
      name = trimText(String(nickValue || rawEntry.name || rawEntry.alias || rawEntry.user || ""));
      bbs = trimText(String(rawEntry.system || rawEntry.host || rawEntry.bbs || system.name));
      if (name.length) {
        nick = {
          name: name,
          host: bbs,
          qwkid: rawEntry.qwkid || undefined
        };
      }
    }

    if (nick && !nick.avatar && name.length) {
      const embedded = this.embeddedAvatars[name.toUpperCase()];
      if (embedded) {
        nick.avatar = embedded;
      }
    }

    if (!name.length) {
      return null;
    }

    // Cache BBS name for departed-user lookup
    if (bbs && bbs.length) {
      this.userBbsCache[name.toUpperCase()] = bbs;
    }
    return {
      name: name,
      bbs: bbs || "Unknown BBS",
      nick: nick,
      isSelf: name.toUpperCase() === user.alias.toUpperCase()
    };
  }

  /**
   * Enrich a join/leave system notice with the user's BBS name.
   * "PhilTaylor is here." -> "PhilTaylor is here from futureland.today"
   * "Guest has left."     -> "Guest from otherBBS left."
   * Looks up the BBS name from the channel roster.
   */
  private enrichJoinLeaveNotice(message: ChatMessage, channel: ChatChannel): void {
    if (!message || message.nick || !message.str) return;
    const text = message.str;

    // Match "Username is here." pattern
    const hereMatch = /^(.+) is here\.$/.exec(text);
    if (hereMatch) {
      const userName = hereMatch[1] || "";
      const bbsName = this.lookupUserBbs(userName, channel);
      if (bbsName) {
        message.str = userName + " is here from " + bbsName;
      }
      return;
    }

    // Match "Username has left." pattern
    const leftMatch = /^(.+) has left\.$/.exec(text);
    if (leftMatch) {
      const userName = leftMatch[1] || "";
      const bbsName = this.lookupUserBbs(userName, channel);
      if (bbsName) {
        message.str = userName + " from " + bbsName + " left.";
      }
      return;
    }
  }

  /**
   * Look up BBS name for a username from channel roster or embedded avatars cache.
   */
  private lookupUserBbs(userName: string, channel: ChatChannel): string {
    const upper = userName.toUpperCase();

    // Try channel roster first
    if (channel && channel.users) {
      for (let i = 0; i < channel.users.length; i += 1) {
        const entry = this.extractRosterEntry(channel.users[i]);
        if (entry && entry.name.toUpperCase() === upper) {
          return entry.bbs;
        }
      }
    }

    // Fall back to cached BBS name (for departed users)
    return this.userBbsCache[upper] || "";
  }

  private buildChannelEntries(): ChannelListEntry[] {
    const entries: ChannelListEntry[] = [];
    let key = "";

    if (!this.chat) {
      return entries;
    }

    for (key in this.chat.channels) {
      if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
        const channel = this.chat.channels[key];
        const unreadCount = channel && channel.name ? (this.publicChannelUnreadCounts[channel.name.toUpperCase()] || 0) : 0;

        if (!channel || !channel.name || channel.name.charAt(0) === "@") {
          continue;
        }

        entries.push({
          name: channel.name,
          userCount: channel.users ? channel.users.length : 0,
          isCurrent: channel.name.toUpperCase() === this.currentChannel.toUpperCase(),
          metaText: unreadCount > 0
            ? ((channel.users ? String(channel.users.length) : "0") + " | new " + String(unreadCount))
            : (channel.users ? String(channel.users.length) : "0")
        });
      }
    }

    entries.sort(function (left, right) {
      if (left.name.toUpperCase() < right.name.toUpperCase()) {
        return -1;
      }
      if (left.name.toUpperCase() > right.name.toUpperCase()) {
        return 1;
      }
      return 0;
    });

    return entries;
  }

  private buildPrivateThreadEntries(): ChannelListEntry[] {
    const entries: ChannelListEntry[] = [];
    let index = 0;

    for (index = 0; index < this.privateThreadOrder.length; index += 1) {
      const threadId = this.privateThreadOrder[index];
      const thread = threadId ? this.privateThreads[threadId] : null;

      if (!thread) {
        continue;
      }

      entries.push({
        name: thread.name,
        userCount: thread.messages.length,
        isCurrent: thread.name.toUpperCase() === this.currentChannel.toUpperCase(),
        metaText: thread.unreadCount > 0 ? ("new " + String(thread.unreadCount)) : "pm"
      });
    }

    entries.sort(function (left, right) {
      const leftUnread = left.metaText && left.metaText.indexOf("new ") === 0 ? parseInt(left.metaText.substr(4), 10) || 0 : 0;
      const rightUnread = right.metaText && right.metaText.indexOf("new ") === 0 ? parseInt(right.metaText.substr(4), 10) || 0 : 0;

      if (leftUnread !== rightUnread) {
        return rightUnread - leftUnread;
      }

      if (left.isCurrent !== right.isCurrent) {
        return left.isCurrent ? -1 : 1;
      }

      if (left.userCount !== right.userCount) {
        return right.userCount - left.userCount;
      }

      if (left.name.toUpperCase() < right.name.toUpperCase()) {
        return -1;
      }
      if (left.name.toUpperCase() > right.name.toUpperCase()) {
        return 1;
      }
      return 0;
    });

    return entries;
  }

  private appendNotice(channelName: string, text: string): void {
    const channel = this.getChannelByName(channelName);

    if (!channel) {
      this.lastError = text;
      return;
    }

    channel.messages.push(buildNoticeMessage(text));
    trimChannelMessages(channel, this.config.maxHistory);
    this.transcriptSignature = "";
  }

  private appendViewNotice(viewName: string, text: string): void {
    const privateThread = this.getPrivateThreadByName(viewName);

    if (privateThread) {
      privateThread.messages.push(buildNoticeMessage(text));
      trimChannelMessages(privateThread as any, this.config.maxHistory);
      this.transcriptSignature = "";
      return;
    }

    this.appendNotice(viewName, text);
  }

  private scrollTranscriptOlder(step: number): void {
    this.setTranscriptScrollOffset(this.transcriptScrollOffsetBlocks + Math.max(1, step));
  }

  private scrollTranscriptNewer(step: number): void {
    this.setTranscriptScrollOffset(this.transcriptScrollOffsetBlocks - Math.max(1, step));
  }

  private pageTranscriptOlder(): void {
    this.scrollTranscriptOlder(Math.max(1, this.transcriptVisibleBlockCount - 1));
  }

  private pageTranscriptNewer(): void {
    this.scrollTranscriptNewer(Math.max(1, this.transcriptVisibleBlockCount - 1));
  }

  private scrollTranscriptToLatest(): void {
    this.setTranscriptScrollOffset(0);
  }

  private scrollTranscriptToOldest(): void {
    this.setTranscriptScrollOffset(this.transcriptMaxScrollOffsetBlocks);
  }

  private setTranscriptScrollOffset(nextOffset: number): void {
    const clampedOffset = clamp(nextOffset, 0, this.transcriptMaxScrollOffsetBlocks);

    if (clampedOffset === this.transcriptScrollOffsetBlocks) {
      return;
    }

    this.transcriptScrollOffsetBlocks = clampedOffset;
    this.transcriptSignature = "";
    this.statusSignature = "";
  }

  private isPrintable(key: string): boolean {
    const code = key.charCodeAt(0);
    return key.length === 1 && code >= 32 && code !== 127;
  }

  private cycleChannel(): void {
    let index = 0;

    if (this.channelOrder.length < 2) {
      return;
    }

    for (index = 0; index < this.channelOrder.length; index += 1) {
      const channelName = this.channelOrder[index];

      if (!channelName) {
        continue;
      }

      if (channelName.toUpperCase() === this.currentChannel.toUpperCase()) {
        this.currentChannel = this.channelOrder[(index + 1) % this.channelOrder.length] || this.config.defaultChannel;
        this.markCurrentViewRead(this.currentChannel);
        this.scrollTranscriptToLatest();
        this.transcriptSignature = "";
        this.headerSignature = "";
        return;
      }
    }

    this.currentChannel = this.channelOrder[0] || this.config.defaultChannel;
    this.markCurrentViewRead(this.currentChannel);
    this.scrollTranscriptToLatest();
  }

  private syncChannelOrder(): void {
    const nextOrder: string[] = [];
    let key = "";
    let index = 0;

    if (!this.chat) {
      return;
    }

    for (index = 0; index < this.channelOrder.length; index += 1) {
      const channelName = this.channelOrder[index];
      const existing = channelName ? this.getChannelByName(channelName) : null;
      if (existing) {
        nextOrder.push(existing.name);
      }
    }

    for (key in this.chat.channels) {
      if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
        const channel = this.chat.channels[key];
        if (channel && !this.channelExists(nextOrder, channel.name)) {
          nextOrder.push(channel.name);
        }
      }
    }

    for (index = 0; index < this.privateThreadOrder.length; index += 1) {
      const threadId = this.privateThreadOrder[index];
      const thread = threadId ? this.privateThreads[threadId] : null;

      if (thread && !this.channelExists(nextOrder, thread.name)) {
        nextOrder.push(thread.name);
      }
    }

    this.channelOrder = nextOrder;

    if (!this.channelOrder.length) {
      this.currentChannel = "";
      this.transcriptScrollOffsetBlocks = 0;
      return;
    }

    if (!this.currentChannel.length || (!this.getChannelByName(this.currentChannel) && !this.getPrivateThreadByName(this.currentChannel))) {
      this.currentChannel = this.channelOrder[0] || "";
      this.markCurrentViewRead(this.currentChannel);
      this.scrollTranscriptToLatest();
    }
  }

  private channelExists(channels: string[], target: string): boolean {
    let index = 0;

    for (index = 0; index < channels.length; index += 1) {
      const channelName = channels[index];
      if (channelName && channelName.toUpperCase() === target.toUpperCase()) {
        return true;
      }
    }

    return false;
  }

  private trimHistories(): void {
    let key = "";

    if (!this.chat) {
      return;
    }

    for (key in this.chat.channels) {
      if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
        const channel = this.chat.channels[key];
        if (channel) {
          trimChannelMessages(channel, this.config.maxHistory);
        }
      }
    }
  }

  private getChannelByName(name: string): ChatChannel | null {
    let key = "";
    const upper = name.toUpperCase();

    if (!this.chat) {
      return null;
    }

    for (key in this.chat.channels) {
      if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
        const channel = this.chat.channels[key];
        if (channel && channel.name.toUpperCase() === upper) {
          return channel;
        }
      }
    }

    return null;
  }

  private ensureFrames(): void {
    const width = console.screen_columns;
    const height = console.screen_rows;
    const transcriptHeight = Math.max(1, height - 4);

    if (
      this.frames.root &&
      this.frames.width === width &&
      this.frames.height === height
    ) {
      return;
    }

    this.destroyFrames();

    this.frames.root = new Frame(1, 1, width, height, BG_BLACK | LIGHTGRAY);
    this.frames.root.open();

    this.frames.header = new Frame(1, 1, width, 1, BG_GREEN | BLACK, this.frames.root);
    this.frames.header.open();

    this.frames.actions = new Frame(1, 2, width, 1, BG_BLUE | WHITE, this.frames.root);
    this.frames.actions.open();

    // Animation background frame: created BEFORE transcript so it's naturally
    // behind it in the canvas iteration order. transparent = true so only
    // painted pixels show; empty cells let root background through.
    this.frames.animBg = new Frame(1, 3, width, transcriptHeight, BG_BLACK | LIGHTGRAY, this.frames.root);
    this.frames.animBg.transparent = true;
    this.frames.animBg.open();

    this.frames.transcript = new Frame(1, 3, width, transcriptHeight, BG_BLACK | LIGHTGRAY, this.frames.root);
    this.frames.transcript.transparent = true;
    this.frames.transcript.open();

    // Animation foreground frame: renders ABOVE transcript content (z-above).
    // transparent = true so sprites float over chat without occluding background.
    this.frames.animFg = new Frame(1, 3, width, transcriptHeight, BG_BLACK | LIGHTGRAY, this.frames.root);
    this.frames.animFg.transparent = true;
    this.frames.animFg.open();
    // Move animFg above transcript so figlet/avatars float over chat content
    this.frames.animFg.top();

    this.frames.status = new Frame(1, transcriptHeight + 3, width, 1, BG_MAGENTA | WHITE, this.frames.root);
    this.frames.status.open();

    this.frames.input = new Frame(1, transcriptHeight + 4, width, 1, BG_BLACK | WHITE, this.frames.root);
    this.frames.input.open();

    this.frames.width = width;
    this.frames.height = height;
    this.resetRenderSignatures();
  }

  private ensureModalFrames(): void {
    const geometry = this.getModalGeometry();

    if (!this.modalState || !this.frames.root || !geometry) {
      this.destroyModalFrames();
      return;
    }

    if (
      this.frames.overlay &&
      this.frames.modal &&
      this.frames.overlay.width === this.frames.width &&
      this.frames.overlay.height === this.frames.height &&
      this.frames.modal.width === geometry.width &&
      this.frames.modal.height === geometry.height
    ) {
      return;
    }

    this.destroyModalFrames();

    this.frames.overlay = new Frame(1, 1, this.frames.width, this.frames.height, BG_BLACK | DARKGRAY, this.frames.root);
    this.frames.overlay.open();

    this.frames.modal = new Frame(
      geometry.x,
      geometry.y,
      geometry.width,
      geometry.height,
      BG_BLACK | LIGHTGRAY,
      this.frames.overlay
    );
    this.frames.modal.open();
  }

  private getModalGeometry(): ModalGeometry | null {
    let width = 0;
    let height = 0;
    let x = 0;
    let y = 0;

    if (!this.modalState || !this.frames.width || !this.frames.height) {
      return null;
    }

    switch (this.modalState.kind) {
      case "roster":
        width = clamp(this.frames.width - 6, 36, 74);
        height = clamp(this.frames.height - 6, 12, 18);
        break;
      case "channels":
        width = clamp(this.frames.width - 10, 30, 56);
        height = clamp(this.frames.height - 8, 10, 16);
        break;
      case "private":
        width = clamp(this.frames.width - 10, 30, 56);
        height = clamp(this.frames.height - 8, 10, 16);
        break;
      case "help":
        width = clamp(this.frames.width - 8, 34, 64);
        height = clamp(this.frames.height - 8, 12, 18);
        break;
      default:
        return null;
    }

    x = Math.max(1, Math.floor((this.frames.width - width) / 2) + 1);
    y = Math.max(1, Math.floor((this.frames.height - height) / 2) + 1);

    return {
      x: x,
      y: y,
      width: width,
      height: height
    };
  }

  private destroyModalFrames(): void {
    if (this.frames.modal) {
      this.frames.modal.close();
    }
    if (this.frames.overlay) {
      this.frames.overlay.close();
    }

    this.frames.modal = null;
    this.frames.overlay = null;
  }

  private destroyFrames(): void {
    if (this.idleAnimActive) {
      this.stopIdleAnimations();
    }
    this.animMgr = null;
    this.animFrame = "";

    this.destroyModalFrames();

    if (this.frames.input) {
      this.frames.input.close();
    }
    if (this.frames.status) {
      this.frames.status.close();
    }
    if (this.frames.animFg) {
      this.frames.animFg.close();
    }
    if (this.frames.animBg) {
      this.frames.animBg.close();
    }
    if (this.frames.transcript) {
      this.frames.transcript.close();
    }
    if (this.frames.actions) {
      this.frames.actions.close();
    }
    if (this.frames.header) {
      this.frames.header.close();
    }
    if (this.frames.root) {
      this.frames.root.close();
    }

    this.frames.root = null;
    this.frames.header = null;
    this.frames.actions = null;
    this.frames.transcript = null;
    this.frames.animBg = null;
    this.frames.animFg = null;
    this.frames.status = null;
    this.frames.input = null;
    this.frames.width = 0;
    this.frames.height = 0;
  }


  // ---- Idle Animation System ----

  /** Names of animations that render on the FOREGROUND frame (above chat content) */
  private static readonly FOREGROUND_ANIMATIONS: string[] = ["figlet_message", "avatars_float"];

  /** Unified animation registry: name -> constructor key in CanvasAnimations (or special handling) */
  private static readonly ANIMATION_MAP: { [name: string]: string } = {
    // Background-preference animations
    tv_static: "TvStatic",
    matrix_rain: "MatrixRain",
    life: "Life",
    starfield: "Starfield",
    fireflies: "Fireflies",
    sine_wave: "SineWave",
    comet_trails: "CometTrails",
    plasma: "Plasma",
    fireworks: "Fireworks",
    aurora: "Aurora",
    fire_smoke: "FireSmoke",
    ocean_ripple: "OceanRipple",
    lissajous: "LissajousTrails",
    lightning: "LightningStorm",
    tunnel: "RecursiveTunnel",
    // Foreground-preference animations
    figlet_message: "FigletMessage",
    avatars_float: "AvatarsFloat"
  };

  private collectChatNicks(): ChatNick[] {
    const nicks: ChatNick[] = [];
    const seen: { [key: string]: boolean } = {};
    let key = "";

    if (!this.chat) {
      return nicks;
    }

    for (key in this.chat.channels) {
      if (!Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
        continue;
      }

      const channel = this.chat.channels[key] as ChatChannel;
      if (!channel) {
        continue;
      }

      // Harvest embedded avatar data from recent messages
      let msgIndex = 0;
      for (msgIndex = 0; msgIndex < channel.messages.length; msgIndex += 1) {
        const message = channel.messages[msgIndex];
        if (message && message.nick && message.nick.avatar) {
          const avatarStr = String(message.nick.avatar).replace(/^\s+|\s+$/g, "");
          if (avatarStr.length) {
            this.embeddedAvatars[message.nick.name.toUpperCase()] = avatarStr;
          }
        }
      }

      // Collect nicks from user list, enriching with harvested avatar data
      if (!channel.users) {
        continue;
      }

      let userIndex = 0;
      for (userIndex = 0; userIndex < channel.users.length; userIndex += 1) {
        const rosterEntry = this.extractRosterEntry(channel.users[userIndex]);
        if (!rosterEntry || !rosterEntry.nick || !rosterEntry.nick.name) {
          continue;
        }

        const nickKey = rosterEntry.nick.name.toUpperCase();
        if (seen[nickKey]) {
          continue;
        }
        seen[nickKey] = true;

        nicks.push(rosterEntry.nick);
      }
    }

    return nicks;
  }

  private buildAnimOpts(): { [key: string]: any } {
    const cfg = this.config.idleAnimations;
    return {
      switch_interval: cfg.switchInterval,
      fps: cfg.fps,
      random: cfg.random,
      sequence: cfg.sequence,
      clear_on_switch: cfg.clearOnSwitch,
      debug: false,
      figlet_messages: cfg.figletMessages,
      figlet_refresh: cfg.figletRefresh,
      figlet_fonts: cfg.figletFonts || undefined,
      figlet_colors: cfg.figletColors,
      figlet_move: cfg.figletMove,
      use_avatar_frames: cfg.useAvatarFrames,
      avatar_lib: this.avatarLib || undefined,
      getChatNicks: (): ChatNick[] => this.collectChatNicks(),
      star_count: cfg.starCount,
      aurora_speed: cfg.auroraSpeed,
      aurora_wave: cfg.auroraWave,
      matrix_sparse: cfg.matrixSparse,
      plasma_speed: cfg.plasmaSpeed,
      plasma_scale: cfg.plasmaScale,
      tunnel_speed: cfg.tunnelSpeed,
      tunnel_scale: cfg.tunnelScale,
      lissajous_speed: cfg.lissajousSpeed,
      fire_decay: cfg.fireDecay,
      ripple_count: cfg.rippleCount
    };
  }

  /**
   * Initialise ONE AnimationManager that holds every registered animation.
   * Each animation has a *frame preference* (foreground or background).
   * When the manager switches to a new animation, the frameForAnim callback
   * returns the correct frame and clears the previously-used one so only a
   * single animation is ever visible at any given time.
   */
  private initIdleAnimManagers(): void {
    const cfg = this.config.idleAnimations;
    if (!cfg.enabled) return;

    const CA = (js.global as any).CanvasAnimations;
    const AF = (js.global as any).AvatarsFloat;
    if (!CA) {
      log("Avatar Chat: CanvasAnimations not loaded, idle animations disabled");
      return;
    }

    const disableSet: { [name: string]: boolean } = {};
    for (let i = 0; i < cfg.disable.length; i++) {
      const disabledName = cfg.disable[i];
      if (disabledName) {
        disableSet[disabledName] = true;
      }
    }

    const opts = this.buildAnimOpts();

    // Capture frame refs for the closure
    const animBg = this.frames.animBg;
    const animFg = this.frames.animFg;
    const fgSet: { [name: string]: boolean } = {};
    for (let i = 0; i < AvatarChatApp.FOREGROUND_ANIMATIONS.length; i++) {
      const fgName = AvatarChatApp.FOREGROUND_ANIMATIONS[i];
      if (fgName) fgSet[fgName] = true;
    }

    // frameForAnim callback: returns the preferred frame for this animation
    // and clears the OTHER frame so only one animation is visible.
    const self = this;
    opts.frameForAnim = function (name: string, _defaultFrame: any): any {
      if (fgSet[name]) {
        // Foreground animation – clear bg frame
        if (animBg) { try { animBg.clear(); animBg.invalidate(); } catch (_e) {} }
        self.animFrame = "fg";
        return animFg || _defaultFrame;
      }
      // Background animation – clear fg frame
      if (animFg) { try { animFg.clear(); animFg.invalidate(); } catch (_e) {} }
      self.animFrame = "bg";
      return animBg || _defaultFrame;
    };

    // Create ONE manager; default frame is animBg (most animations are bg)
    const defaultFrame = animBg || animFg;
    if (!defaultFrame) return;
    this.animMgr = new CA.AnimationManager(defaultFrame, opts);

    // Register all animations from the unified map
    for (const name in AvatarChatApp.ANIMATION_MAP) {
      if (disableSet[name]) continue;
      const ctorKey = AvatarChatApp.ANIMATION_MAP[name];
      if (!ctorKey) continue;

      if (name === "avatars_float" && AF && AF.AvatarsFloat) {
        this.animMgr.add(name, AF.AvatarsFloat);
      } else if (CA[ctorKey]) {
        this.animMgr.add(name, CA[ctorKey]);
      }
    }

    // Compute tick interval from FPS
    this.idleTickInterval = Math.max(50, Math.floor(1000 / Math.max(1, cfg.fps)));
  }

  private startIdleAnimations(): void {
    if (this.idleAnimActive) return;
    if (!this.animMgr) {
      this.initIdleAnimManagers();
    }
    if (!this.animMgr) return;

    this.idleAnimActive = true;
    this.lastAnimTickAt = Date.now();

    try {
      this.animMgr.start();
    } catch (e) {
      log("Avatar Chat: animation start error: " + String(e));
    }
  }

  private stopIdleAnimations(): void {
    if (!this.idleAnimActive) return;
    this.idleAnimActive = false;

    if (this.animMgr) {
      try { this.animMgr.dispose(); } catch (_e) {}
    }

    // Clear both animation frames and force transcript redraw
    if (this.frames.animBg) {
      try { this.frames.animBg.clear(); this.frames.animBg.invalidate(); } catch (_e) {}
    }
    if (this.frames.animFg) {
      try { this.frames.animFg.clear(); this.frames.animFg.invalidate(); } catch (_e) {}
    }
    if (this.frames.transcript) {
      try { this.frames.transcript.invalidate(); } catch (_e) {}
    }
    this.resetRenderSignatures();
  }

  private tickIdleAnimations(): void {
    const cfg = this.config.idleAnimations;
    if (!cfg.enabled) return;

    const now = Date.now();
    const elapsed = now - this.lastKeyTimestamp;

    if (!this.idleAnimActive) {
      // Check if we've been idle long enough
      if (elapsed >= cfg.idleTimeoutSeconds * 1000) {
        this.startIdleAnimations();
      }
      return;
    }

    // Throttle animation ticks to configured FPS
    if (now - this.lastAnimTickAt < this.idleTickInterval) {
      return;
    }
    this.lastAnimTickAt = now;

    // Check if it's time to switch to the next animation
    const switchNow = this.animMgr && this.animMgr.lastSwitch &&
      (time() - this.animMgr.lastSwitch) >= cfg.switchInterval;

    if (this.animMgr) {
      try {
        if (switchNow) this.animMgr.start();
        this.animMgr.tick();
      } catch (e) {
        log("Avatar Chat: animation tick error: " + String(e));
      }
    }
  }

  // ---- End Idle Animation System ----

  private render(): void {
    this.ensureFrames();
    this.renderHeader();
    this.renderActions();
    this.renderTranscript();
    this.renderStatus();
    this.renderInput();
    this.renderActiveModal();

    if (this.frames.root) {
      this.frames.root.cycle();
    }
  }

  private renderHeader(): void {
    const headerFrame = this.frames.header;
    const channel = this.currentChannel.length ? this.getChannelByName(this.currentChannel) : null;
    const privateThread = this.currentChannel.length ? this.getPrivateThreadByName(this.currentChannel) : null;
    const users = channel && channel.users ? channel.users.length : 0;
    let text = "";

    if (privateThread) {
      text = clipText(
        " Avatar Chat | pm " +
        privateThread.peerNick.name +
        " | messages " +
        String(privateThread.messages.length) +
        " | joined " +
        String(this.channelOrder.length) +
        " | " +
        this.config.host +
        ":" +
        String(this.config.port),
        this.frames.width
      );
    } else {
      text = clipText(
        " Avatar Chat | " +
        (this.currentChannel || "offline") +
        " | users " +
        String(users) +
        " | joined " +
        String(this.channelOrder.length) +
        " | " +
        this.config.host +
        ":" +
        String(this.config.port),
        this.frames.width
      );
    }

    if (!headerFrame) {
      return;
    }

    if (text === this.headerSignature) {
      return;
    }

    headerFrame.clear(BG_GREEN | BLACK);
    headerFrame.gotoxy(1, 1);
    headerFrame.putmsg(padRight(text, headerFrame.width), BG_GREEN | BLACK);
    this.headerSignature = text;
  }

  private renderActions(): void {
    const actionsFrame = this.frames.actions;
    const unreadPmCount = this.getUnreadPrivateThreadCount();
    const flashPhase = unreadPmCount > 0 ? Math.floor(new Date().getTime() / 500) % 2 : 0;
    const actions = ACTION_BAR_ACTIONS.map(function (action) {
      const nextAction: ActionBarAction = {
        id: action.id,
        label: action.label
      };

      if (action.id === "private" && unreadPmCount > 0) {
        nextAction.label = "/private [" + String(unreadPmCount) + "]";
        nextAction.attr = flashPhase ? (BG_RED | YELLOW) : (BG_CYAN | BLACK);
      }

      return nextAction;
    });
    const signature = String(this.frames.width) + "|" + actions.map(function (action) {
      return action.id + ":" + action.label + ":" + String(action.attr || 0);
    }).join("|");

    if (!actionsFrame) {
      return;
    }

    if (signature === this.actionSignature) {
      return;
    }

    renderActionBar(actionsFrame, actions);
    this.actionSignature = signature;
  }

  private renderTranscript(): void {
    const transcriptFrame = this.frames.transcript;
    const channel = this.currentChannel.length ? this.getChannelByName(this.currentChannel) : null;
    const privateThread = this.currentChannel.length ? this.getPrivateThreadByName(this.currentChannel) : null;
    const messages = channel ? channel.messages : (privateThread ? privateThread.messages : []);
    const signature = this.buildTranscriptSignature(messages);
    let renderState: TranscriptRenderState;
    let emptyText = "";

    if (!transcriptFrame) {
      return;
    }

    if (signature === this.transcriptSignature) {
      return;
    }

    if (!this.chat) {
      emptyText = this.buildDisconnectedText();
    } else if (!channel && !privateThread) {
      emptyText = "No joined channels. Use /join <channel>.";
    } else if (privateThread) {
      emptyText = "No private messages yet.";
    } else {
      emptyText = "No messages yet.";
    }

    renderState = renderTranscript(
      transcriptFrame,
      messages,
      {
        ownAlias: user.alias,
        ownUserNumber: user.number,
        avatarLib: this.avatarLib,
        avatarCache: this.avatarCache,
        emptyText: emptyText,
        scrollOffsetBlocks: this.transcriptScrollOffsetBlocks
      }
    );

    this.transcriptVisibleBlockCount = renderState.visibleBlockCount;
    this.transcriptMaxScrollOffsetBlocks = renderState.maxScrollOffsetBlocks;
    this.transcriptScrollOffsetBlocks = renderState.actualScrollOffsetBlocks;

    this.transcriptSignature = signature;
  }

  private renderStatus(): void {
    const statusFrame = this.frames.status;
    const text = clipText(this.buildStatusText(), this.frames.width);

    if (!statusFrame) {
      return;
    }

    if (text === this.statusSignature) {
      return;
    }

    statusFrame.clear(BG_MAGENTA | WHITE);
    statusFrame.gotoxy(1, 1);
    statusFrame.putmsg(padRight(text, statusFrame.width), BG_MAGENTA | WHITE);
    this.statusSignature = text;
  }

  private renderInput(): void {
    const inputFrame = this.frames.input;
    const viewportWidth = Math.max(1, this.frames.width - 2);
    const viewport = this.inputBuffer.getViewport(viewportWidth);
    const signature = this.inputBuffer.getValue() + "|" + String(viewport.cursorColumn) + "|" + String(this.frames.width);
    let cursorX = 0;
    let cursorChar = " ";

    if (!inputFrame) {
      return;
    }

    if (signature === this.inputSignature) {
      return;
    }

    inputFrame.clear(BG_BLACK | WHITE);
    inputFrame.gotoxy(1, 1);
    inputFrame.putmsg(">", YELLOW);
    inputFrame.putmsg(padRight(viewport.text, viewportWidth), WHITE);

    cursorX = viewport.cursorColumn + 1;
    if (viewport.cursorColumn <= viewport.text.length) {
      cursorChar = viewport.text.charAt(viewport.cursorColumn - 1);
    }
    inputFrame.setData(cursorX - 1, 0, cursorChar, BG_CYAN | BLACK, false);
    this.inputSignature = signature;
  }

  private renderActiveModal(): void {
    const signature = this.buildModalSignature();

    if (!this.modalState) {
      this.destroyModalFrames();
      return;
    }

    this.ensureModalFrames();

    if (!this.frames.overlay || !this.frames.modal) {
      return;
    }

    if (signature === this.modalSignature) {
      return;
    }

    renderModal(
      this.frames.overlay,
      this.frames.modal,
      this.modalState,
      {
        ownAlias: user.alias,
        ownUserNumber: user.number,
        avatarLib: this.avatarLib,
        avatarCache: this.avatarCache
      }
    );

    this.modalSignature = signature;
  }

  private buildTranscriptSignature(messages: ChatMessage[]): string {
    let lastMessage = "";
    let lastTime = 0;

    if (messages.length) {
      const message = messages[messages.length - 1];
      if (message) {
        lastMessage = message.str || "";
        lastTime = message.time || 0;
      }
    }

    return [
      String(this.frames.width),
      String(this.frames.height),
      this.currentChannel,
      String(!!this.chat),
      String(this.transcriptScrollOffsetBlocks),
      String(messages.length),
      String(lastTime),
      lastMessage
    ].join("|");
  }

  private buildStatusText(): string {
    const unreadPmCount = this.getUnreadPrivateThreadCount();

    if (this.modalState) {
      switch (this.modalState.kind) {
        case "roster":
          return "Who's Here | Up/Down move | Esc close";
        case "channels":
          return "Channels | Enter switch | Esc close";
        case "private":
          return "Private Threads | Enter switch | Esc close";
        case "help":
          return "Help | Esc close";
        default:
          break;
      }
    }

    if (!this.chat) {
      return this.buildDisconnectedText() + " | /connect | Esc exit";
    }

    if (this.transcriptScrollOffsetBlocks > 0) {
      return "History " + String(this.transcriptScrollOffsetBlocks) + " back | Up/PgUp older | Down/PgDn newer | End latest";
    }

    if (this.getPrivateThreadByName(this.currentChannel)) {
      return "Private chat | /msg <user> <message> | /r <message> | /private | /channels | Tab user | Esc exit";
    }

    if (unreadPmCount > 0) {
      return "Private unread " + String(unreadPmCount) + " | /private | /msg <user> <message> | /r <message> | Tab user | Esc exit";
    }

    return "Up/PgUp history | /who /channels /private /help | /join /part /me /msg /r /clear | Tab user | Esc exit";
  }

  private getUnreadPrivateThreadCount(): number {
    let total = 0;
    let index = 0;

    for (index = 0; index < this.privateThreadOrder.length; index += 1) {
      const threadId = this.privateThreadOrder[index];
      const thread = threadId ? this.privateThreads[threadId] : null;

      if (thread) {
        total += thread.unreadCount;
      }
    }

    return total;
  }

  private markCurrentViewRead(viewName: string): void {
    this.markPrivateThreadRead(viewName);
    this.markPublicChannelRead(viewName);
  }

  private markPublicChannelRead(viewName: string): void {
    const normalizedName = trimText(viewName).toUpperCase();

    if (!normalizedName.length || normalizedName.charAt(0) === "@") {
      return;
    }

    if (!this.publicChannelUnreadCounts[normalizedName]) {
      return;
    }

    this.publicChannelUnreadCounts[normalizedName] = 0;
    this.modalSignature = "";
  }

  private markPrivateThreadRead(viewName: string): void {
    const thread = this.getPrivateThreadByName(viewName);

    if (!thread || thread.unreadCount === 0) {
      return;
    }

    thread.unreadCount = 0;
    this.headerSignature = "";
    this.statusSignature = "";
    this.actionSignature = "";
  }

  private buildDisconnectedText(): string {
    let seconds = 0;

    if (this.reconnectAt) {
      seconds = Math.ceil((this.reconnectAt - new Date().getTime()) / 1000);
      if (seconds < 0) {
        seconds = 0;
      }
    }

    if (this.lastError.length) {
      return clipText(this.lastError, Math.max(10, this.frames.width - 16)) + " | retry " + String(seconds) + "s";
    }

    return "Disconnected";
  }

  private resetRenderSignatures(): void {
    this.transcriptSignature = "";
    this.headerSignature = "";
    this.actionSignature = "";
    this.statusSignature = "";
    this.inputSignature = "";
    this.modalSignature = "";
  }

  private buildModalSignature(): string {
    const parts: string[] = [];
    let index = 0;

    if (!this.modalState) {
      return "";
    }

    parts.push(this.modalState.kind);
    parts.push(this.modalState.title);
    parts.push(String(this.modalState.selectedIndex));
    parts.push(String(this.frames.width));
    parts.push(String(this.frames.height));

    if (this.modalState.kind === "help") {
      for (index = 0; index < this.modalState.lines.length; index += 1) {
        parts.push(this.modalState.lines[index] || "");
      }
      return parts.join("|");
    }

    for (index = 0; index < this.modalState.entries.length; index += 1) {
      const entry = this.modalState.entries[index];

      if (!entry) {
        continue;
      }

      if (this.modalState.kind === "roster") {
        const rosterEntry = entry as RosterEntry;
        parts.push(rosterEntry.name + "@" + rosterEntry.bbs + ":" + String(rosterEntry.isSelf));
        continue;
      }

      const channelEntry = entry as ChannelListEntry;
      parts.push(channelEntry.name + ":" + String(channelEntry.userCount) + ":" + String(channelEntry.isCurrent) + ":" + String(channelEntry.metaText || ""));
    }

    return parts.join("|");
  }
}
