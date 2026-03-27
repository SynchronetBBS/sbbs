// Avatar Chat - Auto-generated, do not edit directly.
// Built: 2026-03-25T22:59:12.277Z
load("sbbsdefs.js");
load("key_defs.js");
load("frame.js");
load("json-client.js");
load("json-chat.js");
(function() {
  // repo/xtrn/avatar_chat/build/util/text.js
  function clamp(value, minimum, maximum) {
    if (value < minimum) {
      return minimum;
    }
    if (value > maximum) {
      return maximum;
    }
    return value;
  }
  function repeatChar(ch, count) {
    var output = "";
    while (output.length < count) {
      output += ch;
    }
    if (output.length > count) {
      output = output.substr(0, count);
    }
    return output;
  }
  function padRight(text, width) {
    if (text.length >= width) {
      return text.substr(0, width);
    }
    return text + repeatChar(" ", width - text.length);
  }
  function clipText(text, width) {
    if (width <= 0) {
      return "";
    }
    if (text.length <= width) {
      return text;
    }
    if (width <= 3) {
      return text.substr(0, width);
    }
    return text.substr(0, width - 3) + "...";
  }
  function centerText(text, width) {
    var leftPadding;
    text = clipText(text, width);
    if (text.length >= width) {
      return text;
    }
    leftPadding = Math.floor((width - text.length) / 2);
    return repeatChar(" ", leftPadding) + text;
  }
  function trimText(text) {
    return text.replace(/^\s+|\s+$/g, "");
  }
  function wrapText(text, width) {
    var lines = [];
    var words = trimText(text).split(/\s+/);
    var current = "";
    var index = 0;
    if (width <= 1) {
      return [clipText(text, 1)];
    }
    if (!trimText(text).length) {
      return [""];
    }
    for (index = 0; index < words.length; index += 1) {
      var word = words[index] || "";
      while (word.length > width) {
        if (current.length > 0) {
          lines.push(current);
          current = "";
        }
        lines.push(word.substr(0, width));
        word = word.substr(width);
      }
      if (!current.length) {
        current = word;
      } else if (current.length + 1 + word.length <= width) {
        current += " " + word;
      } else {
        lines.push(current);
        current = word;
      }
    }
    if (current.length > 0) {
      lines.push(current);
    }
    return lines;
  }
  function formatRelativeTime(currentMs, previousMs) {
    if (!currentMs || isNaN(currentMs))
      return "";
    var now = /* @__PURE__ */ new Date();
    var cur = new Date(currentMs);
    var todayY = now.getFullYear();
    var todayM = now.getMonth();
    var todayD = now.getDate();
    var curY = cur.getFullYear();
    var curM = cur.getMonth();
    var curD = cur.getDate();
    var curH = cur.getHours();
    var curMin = cur.getMinutes();
    var curIsToday = curY === todayY && curM === todayM && curD === todayD;
    var pad2 = function(n) {
      return n < 10 ? "0" + String(n) : String(n);
    };
    var h12 = function(h) {
      return h % 12 === 0 ? 12 : h % 12;
    };
    var amPm = function(h) {
      return h < 12 ? "am" : "pm";
    };
    var curTimeStr = String(h12(curH)) + ":" + pad2(curMin) + " " + amPm(curH);
    var curDateStr = pad2(curM + 1) + "/" + pad2(curD) + "/" + String(curY).substr(2);
    if (!previousMs) {
      return curIsToday ? "Today " + curTimeStr : curDateStr + " " + curTimeStr;
    }
    var last = new Date(previousMs);
    var lastY = last.getFullYear();
    var lastM = last.getMonth();
    var lastD = last.getDate();
    if (curY === lastY && curM === lastM && curD === lastD) {
      if (curIsToday) {
        var diffMs = now.getTime() - cur.getTime();
        var totalMin = Math.floor(diffMs / 6e4);
        var diffHr = Math.floor(totalMin / 60);
        var diffMin = totalMin % 60;
        if (diffHr > 0) {
          var s = String(diffHr) + (diffHr === 1 ? " hour" : " hours");
          if (diffMin > 0)
            s += " " + String(diffMin) + (diffMin === 1 ? " minute" : " minutes");
          return s + " ago";
        }
        if (diffMin > 0) {
          return String(diffMin) + (diffMin === 1 ? " minute" : " minutes") + " ago";
        }
        return "just now";
      }
      return curTimeStr;
    }
    if (curIsToday) {
      return "Today " + curTimeStr;
    }
    return curDateStr + " " + curTimeStr;
  }
  function compactTimestamp(text) {
    if (!text)
      return "";
    var s = text;
    s = s.replace(/hours?/g, "h");
    s = s.replace(/minutes?/g, "m");
    s = s.replace(/seconds?/g, "s");
    s = s.replace(/Today\s*/i, "");
    s = s.replace(/Yesterday\s*/i, "yday ");
    s = s.replace(/\s+/g, " ").replace(/^\s+|\s+$/g, "");
    if (s === "just now")
      s = "now";
    return s;
  }

  // repo/xtrn/avatar_chat/build/domain/chat-model.js
  function normalizeChannelName(name, fallback) {
    var trimmed = trimText(name);
    if (trimmed.length) {
      return trimmed;
    }
    if (fallback && trimText(fallback).length) {
      return trimText(fallback);
    }
    return "main";
  }
  function trimChannelMessages(channel, maxHistory) {
    var overflow = channel.messages.length - maxHistory;
    if (overflow > 0) {
      channel.messages.splice(0, overflow);
    }
  }
  function buildNoticeMessage(text, timestamp) {
    return {
      str: text,
      time: timestamp || (/* @__PURE__ */ new Date()).getTime()
    };
  }
  function groupMessages(messages, ownAlias) {
    var groups = [];
    var ownAliasUpper = ownAlias.toUpperCase();
    var lastBubble = null;
    var index = 0;
    for (index = 0; index < messages.length; index += 1) {
      var message = messages[index];
      var speakerName = "";
      var side = "left";
      var speakerUpper = "";
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
      if (lastBubble && lastBubble.kind === "bubble" && lastBubble.side === side && lastBubble.speakerName.toUpperCase() === speakerUpper) {
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

  // repo/xtrn/avatar_chat/build/domain/bitmap.js
  var LENGTH_BASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258];
  var LENGTH_EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
  var DISTANCE_BASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
  var DISTANCE_EXTRA = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
  function isBitmapMessage(text) {
    return typeof text === "string" && text.indexOf("[BITMAP|") === 0 && text.charAt(text.length - 1) === "]";
  }
  function parseBitmapMessage(text) {
    var inner = "";
    var parts = [];
    var width = 0;
    var height = 0;
    if (!isBitmapMessage(text)) {
      return null;
    }
    inner = text.slice(1, -1);
    parts = inner.split("|");
    if (parts.length !== 5 || parts[0] !== "BITMAP") {
      return null;
    }
    width = parseInt(parts[1] || "", 10) || 0;
    height = parseInt(parts[2] || "", 10) || 0;
    if (width < 1 || height < 1 || !(parts[4] || "").length || (parts[4] || "").length % 2 !== 0) {
      return null;
    }
    return {
      width: width,
      height: height,
      fromName: parts[3] || "",
      hexData: parts[4] || ""
    };
  }
  function decodeBitmap(hexData, expectedWidth, expectedHeight) {
    var compressed = hexToBytes(hexData);
    var decompressed = inflateZlib(compressed, 0);
    var bitmap = [];
    var fgs = [];
    var bgs = [];
    var chars = [];
    var dataHeight = 0;
    var dataLength = 0;
    var slicePoint = 0;
    var totalPixels = 0;
    var dataWidth = 0;
    var width = 0;
    var height = 0;
    var actualWidth = 0;
    var actualHeight = 0;
    var index = 0;
    if (decompressed.length < 4) {
      return {
        bitmap: bitmap,
        width: 0,
        height: 0,
        actualWidth: 0,
        actualHeight: 0
      };
    }
    dataHeight = decompressed[0] || 0;
    if (dataHeight < 1) {
      return {
        bitmap: bitmap,
        width: 0,
        height: 0,
        actualWidth: 0,
        actualHeight: 0
      };
    }
    dataLength = decompressed.length - 1;
    slicePoint = Math.floor(dataLength / 3);
    totalPixels = slicePoint;
    dataWidth = Math.floor(totalPixels / dataHeight);
    width = expectedWidth || dataWidth;
    height = expectedHeight || dataHeight;
    if (width * height !== totalPixels) {
      width = dataWidth;
      height = dataHeight;
    }
    actualWidth = dataWidth;
    actualHeight = dataHeight;
    for (index = 0; index < slicePoint; index += 1) {
      fgs.push(decompressed[1 + index] || 0);
      bgs.push(decompressed[1 + slicePoint + index] || 0);
      chars.push(decompressed[1 + slicePoint * 2 + index] || 32);
    }
    for (index = 0; index < totalPixels; index += 1) {
      var charCode = chars[index] || 32;
      var ch = "";
      try {
        ch = ascii(charCode);
      } catch (_error) {
        ch = String.fromCharCode(charCode);
      }
      bitmap.push({
        ch: ch,
        charCode: charCode,
        fg: fgs[index] || 0,
        bg: bgs[index] || 0
      });
    }
    return {
      bitmap: bitmap,
      width: width,
      height: height,
      actualWidth: actualWidth,
      actualHeight: actualHeight
    };
  }
  function hexToBytes(hex) {
    var bytes = [];
    var index = 0;
    for (index = 0; index < hex.length; index += 2) {
      bytes.push(parseInt(hex.substr(index, 2), 16) || 0);
    }
    return bytes;
  }
  function createInflateState(bytes, offset) {
    return {
      bytes: bytes,
      position: offset,
      bitBuffer: 0,
      bitCount: 0
    };
  }
  function readByte(state) {
    var value = state.bytes[state.position];
    state.position += 1;
    return value === void 0 ? 0 : value;
  }
  function readBits(state, count) {
    var buffer = state.bitBuffer;
    var available = state.bitCount;
    var out = 0;
    while (available < count) {
      buffer |= readByte(state) << available;
      available += 8;
    }
    out = buffer & (1 << count) - 1;
    state.bitBuffer = buffer >>> count;
    state.bitCount = available - count;
    return out;
  }
  function alignByte(state) {
    state.bitBuffer = 0;
    state.bitCount = 0;
  }
  function reverseBits(value, count) {
    var result = 0;
    var index = 0;
    for (index = 0; index < count; index += 1) {
      result = result << 1 | value & 1;
      value >>= 1;
    }
    return result;
  }
  function buildHuffmanTable(codeLengths) {
    var table = {
      maxBits: 0,
      map: {}
    };
    var counts = [];
    var nextCodes = [];
    var index = 0;
    var code = 0;
    for (index = 0; index < codeLengths.length; index += 1) {
      if ((codeLengths[index] || 0) > table.maxBits) {
        table.maxBits = codeLengths[index] || 0;
      }
    }
    for (index = 0; index <= table.maxBits; index += 1) {
      counts[index] = 0;
    }
    for (index = 0; index < codeLengths.length; index += 1) {
      counts[codeLengths[index] || 0] = (counts[codeLengths[index] || 0] || 0) + 1;
    }
    counts[0] = 0;
    for (index = 1; index <= table.maxBits; index += 1) {
      code = code + (counts[index - 1] || 0) << 1;
      nextCodes[index] = code;
    }
    for (index = 0; index < codeLengths.length; index += 1) {
      var length = codeLengths[index] || 0;
      if (!length) {
        continue;
      }
      var nextCode = nextCodes[length] || 0;
      var key = String(reverseBits(nextCode, length) | length << 16);
      table.map[key] = index;
      nextCodes[length] = nextCode + 1;
    }
    return table;
  }
  function readHuffmanCode(table, state) {
    var code = 0;
    var length = 0;
    for (length = 1; length <= table.maxBits; length += 1) {
      code |= readBits(state, 1) << length - 1;
      if (table.map[String(code | length << 16)] !== void 0) {
        return table.map[String(code | length << 16)] || 0;
      }
    }
    throw new Error("Huffman decode failed");
  }
  function buildFixedLiteralTable() {
    var lengths = [];
    var index = 0;
    for (index = 0; index <= 287; index += 1) {
      lengths[index] = 0;
    }
    for (index = 0; index <= 143; index += 1) {
      lengths[index] = 8;
    }
    for (index = 144; index <= 255; index += 1) {
      lengths[index] = 9;
    }
    for (index = 256; index <= 279; index += 1) {
      lengths[index] = 7;
    }
    for (index = 280; index <= 287; index += 1) {
      lengths[index] = 8;
    }
    return buildHuffmanTable(lengths);
  }
  function buildFixedDistanceTable() {
    var lengths = [];
    var index = 0;
    for (index = 0; index < 32; index += 1) {
      lengths[index] = 5;
    }
    return buildHuffmanTable(lengths);
  }
  function decodeDynamicTables(state) {
    var hlit = readBits(state, 5) + 257;
    var hdist = readBits(state, 5) + 1;
    var hclen = readBits(state, 4) + 4;
    var order = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];
    var codeLengths = [];
    var index = 0;
    for (index = 0; index < 19; index += 1) {
      codeLengths[index] = 0;
    }
    for (index = 0; index < hclen; index += 1) {
      codeLengths[order[index] || 0] = readBits(state, 3);
    }
    var codeTable = buildHuffmanTable(codeLengths);
    function readLengths(count) {
      var out = [];
      var previous = 0;
      while (out.length < count) {
        var symbol = readHuffmanCode(codeTable, state);
        var repeat = 0;
        var repeatIndex = 0;
        if (symbol <= 15) {
          out.push(symbol);
          previous = symbol;
          continue;
        }
        if (symbol === 16) {
          repeat = 3 + readBits(state, 2);
          for (repeatIndex = 0; repeatIndex < repeat; repeatIndex += 1) {
            out.push(previous);
          }
          continue;
        }
        if (symbol === 17) {
          repeat = 3 + readBits(state, 3);
          previous = 0;
          for (repeatIndex = 0; repeatIndex < repeat; repeatIndex += 1) {
            out.push(0);
          }
          continue;
        }
        if (symbol === 18) {
          repeat = 11 + readBits(state, 7);
          previous = 0;
          for (repeatIndex = 0; repeatIndex < repeat; repeatIndex += 1) {
            out.push(0);
          }
          continue;
        }
        throw new Error("Bad RLE in code lengths");
      }
      return out;
    }
    return {
      lit: buildHuffmanTable(readLengths(hlit)),
      dist: buildHuffmanTable(readLengths(hdist))
    };
  }
  function inflateRaw(bytes, start) {
    var state = createInflateState(bytes, start);
    var output = [];
    var fixedLiterals = buildFixedLiteralTable();
    var fixedDistances = buildFixedDistanceTable();
    var done = false;
    while (!done) {
      var isFinal = readBits(state, 1);
      var blockType = readBits(state, 2);
      var literalTable = null;
      var distanceTable = null;
      if (blockType === 0) {
        var length = 0;
        var notLength = 0;
        var index = 0;
        alignByte(state);
        length = readByte(state) | readByte(state) << 8;
        notLength = readByte(state) | readByte(state) << 8;
        if ((length ^ 65535) !== notLength) {
          throw new Error("Stored block length mismatch");
        }
        for (index = 0; index < length; index += 1) {
          output.push(readByte(state));
        }
      } else {
        if (blockType === 1) {
          literalTable = fixedLiterals;
          distanceTable = fixedDistances;
        } else if (blockType === 2) {
          var tables = decodeDynamicTables(state);
          literalTable = tables.lit;
          distanceTable = tables.dist;
        } else {
          throw new Error("Invalid DEFLATE block type");
        }
        while (literalTable && distanceTable) {
          var symbol = readHuffmanCode(literalTable, state);
          if (symbol < 256) {
            output.push(symbol);
            continue;
          }
          if (symbol === 256) {
            break;
          }
          var lengthIndex = symbol - 257;
          var length = (LENGTH_BASE[lengthIndex] || 0) + (LENGTH_EXTRA[lengthIndex] || 0 ? readBits(state, LENGTH_EXTRA[lengthIndex] || 0) : 0);
          var distanceSymbol = readHuffmanCode(distanceTable, state);
          var distance = (DISTANCE_BASE[distanceSymbol] || 0) + (DISTANCE_EXTRA[distanceSymbol] || 0 ? readBits(state, DISTANCE_EXTRA[distanceSymbol] || 0) : 0);
          var base = output.length - distance;
          var copyIndex = 0;
          if (base < 0) {
            throw new Error("Invalid DEFLATE distance");
          }
          for (copyIndex = 0; copyIndex < length; copyIndex += 1) {
            output.push(output[base + copyIndex] || 0);
          }
        }
      }
      if (isFinal) {
        done = true;
      }
    }
    return output;
  }
  function inflateZlib(bytes, offset) {
    var position = offset || 0;
    var cmf = bytes[position] || 0;
    var flg = bytes[position + 1] || 0;
    position += 2;
    if ((cmf & 15) !== 8) {
      throw new Error("Unsupported zlib compression method");
    }
    if (flg & 32) {
      position += 4;
    }
    return inflateRaw(bytes, position);
  }

  // repo/xtrn/avatar_chat/build/domain/private-messages.js
  function normalizePrivateNick(nick) {
    var name = trimText(String(nick && nick.name ? nick.name : ""));
    var avatar = nick && nick.avatar ? trimText(String(nick.avatar)) : "";
    if (!name.length) {
      return null;
    }
    return {
      name: name,
      host: nick && nick.host ? trimText(String(nick.host)) : void 0,
      ip: nick && nick.ip ? String(nick.ip) : void 0,
      qwkid: nick && nick.qwkid ? trimText(String(nick.qwkid)).toUpperCase() : void 0,
      avatar: avatar.length ? avatar : void 0
    };
  }
  function isPrivateMessage(message) {
    return !!(message && message.private && message.private.to && message.private.to.name && trimText(String(message.private.to.name)).length);
  }
  function buildPrivateMessage(sender, recipient, text, timestamp) {
    return {
      nick: normalizePrivateNick(sender) || sender,
      str: text,
      time: timestamp,
      private: {
        to: normalizePrivateNick(recipient) || recipient
      }
    };
  }
  function buildPrivateThreadId(nick) {
    var normalized = normalizePrivateNick(nick);
    var nameKey = normalized ? trimText(String(normalized.name)).replace(/[^A-Za-z0-9]/g, "").toUpperCase() : "";
    var remoteKey = normalized && normalized.qwkid ? normalized.qwkid : normalized && normalized.host ? normalized.host.toUpperCase() : "";
    return [
      nameKey,
      remoteKey
    ].join("|");
  }
  function buildPrivateThreadName(nick) {
    var normalized = normalizePrivateNick(nick);
    return "@" + (normalized ? normalized.name : "");
  }
  function resolvePrivatePeerNick(message, ownAlias) {
    var sender = normalizePrivateNick(message.nick || null);
    var recipient = normalizePrivateNick(message.private ? message.private.to : null);
    if (!sender || !recipient) {
      return null;
    }
    if (sender.name.toUpperCase() === ownAlias.toUpperCase()) {
      return recipient;
    }
    return sender;
  }

  // repo/xtrn/avatar_chat/build/domain/ui.js
  var ACTION_BAR_ACTIONS = [
    { id: "who", label: "/who" },
    { id: "img", label: "/img" },
    { id: "channels", label: "/channels" },
    { id: "private", label: "/private" },
    { id: "help", label: "/help" },
    { id: "autocomplete", label: "Tab user" },
    { id: "exit", label: "Esc exit" }
  ];

  // repo/xtrn/avatar_chat/build/input/input-buffer.js
  var InputBuffer = (
    /** @class */
    function() {
      function InputBuffer2(maxLength) {
        this.value = "";
        this.cursor = 0;
        this.maxLength = maxLength;
      }
      InputBuffer2.prototype.clear = function() {
        this.value = "";
        this.cursor = 0;
      };
      InputBuffer2.prototype.getValue = function() {
        return this.value;
      };
      InputBuffer2.prototype.setValue = function(value, cursor) {
        this.value = String(value || "");
        if (this.value.length > this.maxLength) {
          this.value = this.value.substr(0, this.maxLength);
        }
        if (typeof cursor !== "number" || isNaN(cursor)) {
          this.cursor = this.value.length;
          return;
        }
        if (cursor < 0) {
          cursor = 0;
        }
        if (cursor > this.value.length) {
          cursor = this.value.length;
        }
        this.cursor = cursor;
      };
      InputBuffer2.prototype.getCursor = function() {
        return this.cursor;
      };
      InputBuffer2.prototype.isEmpty = function() {
        return this.value.length === 0;
      };
      InputBuffer2.prototype.insert = function(text) {
        if (!text.length) {
          return;
        }
        if (this.value.length >= this.maxLength) {
          return;
        }
        if (this.value.length + text.length > this.maxLength) {
          text = text.substr(0, this.maxLength - this.value.length);
        }
        this.value = this.value.substr(0, this.cursor) + text + this.value.substr(this.cursor);
        this.cursor += text.length;
      };
      InputBuffer2.prototype.backspace = function() {
        if (this.cursor <= 0) {
          return;
        }
        this.value = this.value.substr(0, this.cursor - 1) + this.value.substr(this.cursor);
        this.cursor -= 1;
      };
      InputBuffer2.prototype.deleteForward = function() {
        if (this.cursor >= this.value.length) {
          return;
        }
        this.value = this.value.substr(0, this.cursor) + this.value.substr(this.cursor + 1);
      };
      InputBuffer2.prototype.moveLeft = function() {
        if (this.cursor > 0) {
          this.cursor -= 1;
        }
      };
      InputBuffer2.prototype.moveRight = function() {
        if (this.cursor < this.value.length) {
          this.cursor += 1;
        }
      };
      InputBuffer2.prototype.moveHome = function() {
        this.cursor = 0;
      };
      InputBuffer2.prototype.moveEnd = function() {
        this.cursor = this.value.length;
      };
      InputBuffer2.prototype.getViewport = function(width) {
        var start = 0;
        var cursorColumn = 1;
        var text = "";
        if (width <= 0) {
          return {
            text: "",
            cursorColumn: 1
          };
        }
        if (this.cursor >= width) {
          start = this.cursor - width + 1;
        }
        if (start < 0) {
          start = 0;
        }
        text = this.value.substr(start, width);
        cursorColumn = this.cursor - start + 1;
        if (cursorColumn < 1) {
          cursorColumn = 1;
        }
        if (cursorColumn > width) {
          cursorColumn = width;
        }
        return {
          text: text,
          cursorColumn: cursorColumn
        };
      };
      return InputBuffer2;
    }()
  );

  // repo/xtrn/avatar_chat/build/render/avatar.js
  var routeMapCache = null;
  function resolveAvatarDimensions(avatarLib) {
    if (avatarLib && avatarLib.defs) {
      return {
        width: avatarLib.defs.width || 10,
        height: avatarLib.defs.height || 6
      };
    }
    return {
      width: 10,
      height: 6
    };
  }
  function avatarCacheKey(nick, ownAlias) {
    var embeddedAvatar = nick.avatar ? String(nick.avatar).replace(/^\s+|\s+$/g, "") : "";
    var remoteKey = resolveAvatarNetaddr(nick) || nick.host || "local";
    if (embeddedAvatar.length) {
      return nick.name.toUpperCase() + "@EMBEDDED:" + md5_calc(embeddedAvatar);
    }
    if (nick.name.toUpperCase() === ownAlias.toUpperCase()) {
      return "local:" + nick.name.toUpperCase();
    }
    return nick.name.toUpperCase() + "@" + String(remoteKey).toUpperCase();
  }
  function normalizeQwkId(qwkid) {
    if (!qwkid) {
      return null;
    }
    qwkid = String(qwkid);
    if (!qwkid.length) {
      return null;
    }
    return qwkid.toUpperCase();
  }
  function resolveAvatarNetaddr(nick) {
    var qwkid = normalizeQwkId(nick.qwkid);
    if (qwkid) {
      return qwkid;
    }
    return nick.host || null;
  }
  function pushLookupCandidate(list, seen, value) {
    var normalized = value ? String(value).replace(/^\s+|\s+$/g, "").toUpperCase() : "";
    if (!normalized.length || seen[normalized]) {
      return;
    }
    seen[normalized] = true;
    list.push(normalized);
  }
  function loadRouteMap() {
    var map = {};
    var routeFile = new File(system.data_dir + "qnet/route.dat");
    if (routeMapCache) {
      return routeMapCache;
    }
    if (!routeFile.open("r")) {
      routeMapCache = map;
      return routeMapCache;
    }
    try {
      while (!routeFile.eof) {
        var line = routeFile.readln(2048);
        var match = line ? line.match(/^\s*\d{2}\/\d{2}\/\d{2}\s+([^:\s]+):([^:\s]+)/) : null;
        var qwkid = match ? normalizeQwkId(match[1]) : null;
        var route = match ? normalizeQwkId(match[2]) : null;
        if (!qwkid || !route || qwkid === route) {
          continue;
        }
        if (!map[qwkid]) {
          map[qwkid] = [];
        }
        if (map[qwkid].indexOf(route) === -1) {
          map[qwkid].push(route);
        }
      }
    } finally {
      routeFile.close();
    }
    routeMapCache = map;
    return routeMapCache;
  }
  function resolveAvatarLookupCandidates(nick) {
    var candidates = [];
    var seen = {};
    var qwkid = normalizeQwkId(nick.qwkid);
    var routeMap = qwkid ? loadRouteMap() : null;
    var index = 0;
    pushLookupCandidate(candidates, seen, qwkid);
    if (qwkid && routeMap && routeMap[qwkid]) {
      for (index = 0; index < routeMap[qwkid].length; index += 1) {
        pushLookupCandidate(candidates, seen, routeMap[qwkid][index]);
      }
    }
    pushLookupCandidate(candidates, seen, nick.host || null);
    return candidates;
  }
  function lookupRemoteAvatar(avatarLib, nick, bbsId) {
    var candidates = resolveAvatarLookupCandidates(nick);
    var avatarObj = null;
    var index = 0;
    var nameHash = "";
    if (typeof avatarLib.read_netuser !== "function") {
      for (index = 0; index < candidates.length; index += 1) {
        avatarObj = avatarLib.read(0, nick.name, candidates[index], bbsId) || null;
        if (avatarObj && !avatarObj.disabled && avatarObj.data) {
          return avatarObj;
        }
      }
      return null;
    }
    nameHash = "md5:" + md5_calc(nick.name);
    for (index = 0; index < candidates.length; index += 1) {
      avatarObj = avatarLib.read_netuser(nick.name, candidates[index]) || null;
      if (!avatarObj && nameHash.length) {
        avatarObj = avatarLib.read_netuser(nameHash, candidates[index]) || null;
      }
      if (avatarObj && !avatarObj.disabled && avatarObj.data) {
        return avatarObj;
      }
    }
    return null;
  }
  function lookupAvatarBinary(avatarLib, nick, ownAlias, ownUserNumber, cache) {
    var cached;
    var avatarObj;
    var decoded;
    var localUserNumber;
    var localQwkId;
    var nickQwkId;
    var netaddr;
    var key = "";
    if (!avatarLib || !nick || !nick.name) {
      return null;
    }
    key = avatarCacheKey(nick, ownAlias);
    cached = cache[key];
    if (cached !== void 0) {
      return cached;
    }
    avatarObj = null;
    try {
      localQwkId = normalizeQwkId(system.qwk_id);
      nickQwkId = normalizeQwkId(nick.qwkid);
      netaddr = resolveAvatarNetaddr(nick);
      if (nick.avatar && String(nick.avatar).replace(/^\s+|\s+$/g, "").length) {
        avatarObj = {
          data: String(nick.avatar).replace(/^\s+|\s+$/g, "")
        };
      } else if (nick.name.toUpperCase() === ownAlias.toUpperCase()) {
        avatarObj = avatarLib.read(ownUserNumber, ownAlias, null, null) || null;
      } else {
        localUserNumber = 0;
        if (!nickQwkId || nickQwkId === localQwkId) {
          localUserNumber = system.matchuser(nick.name);
        }
        if (localUserNumber > 0) {
          avatarObj = avatarLib.read(localUserNumber, nick.name, null, null) || null;
        } else {
          avatarObj = lookupRemoteAvatar(avatarLib, nick, nick.host || null);
          if (!avatarObj) {
            avatarObj = avatarLib.read(0, nick.name, netaddr, nick.host || null) || null;
          }
        }
      }
    } catch (_error) {
      avatarObj = null;
    }
    if (!avatarObj || avatarObj.disabled || !avatarObj.data) {
      cache[key] = null;
      return null;
    }
    try {
      decoded = base64_decode(avatarObj.data);
    } catch (_decodeError) {
      decoded = null;
    }
    decoded = decoded || null;
    cache[key] = decoded;
    return decoded;
  }
  function blitAvatarToFrame(frame, avatarBinary, avatarWidth, avatarHeight, x, y) {
    var offset = 0;
    var row = 0;
    for (row = 0; row < avatarHeight; row += 1) {
      var column = 0;
      for (column = 0; column < avatarWidth; column += 1) {
        var ch = "";
        var attr = 0;
        if (x + column > frame.width || y + row > frame.height) {
          offset += 2;
          continue;
        }
        if (offset + 1 >= avatarBinary.length) {
          return;
        }
        ch = avatarBinary.substr(offset, 1);
        attr = ascii(avatarBinary.substr(offset + 1, 1));
        frame.setData(x + column - 1, y + row - 1, ch, attr, false);
        offset += 2;
      }
    }
  }

  // repo/xtrn/avatar_chat/build/render/modal-renderer.js
  var BOX_TOP_LEFT = "\xDA";
  var BOX_TOP_RIGHT = "\xBF";
  var BOX_BOTTOM_LEFT = "\xC0";
  var BOX_BOTTOM_RIGHT = "\xD9";
  var BOX_HORIZONTAL = "\xC4";
  var BOX_VERTICAL = "\xB3";
  var SELECTED_POINTER = ">";
  function drawBorder(frame, title) {
    var innerWidth = Math.max(1, frame.width - 2);
    var topLine = "";
    var bottomLine = "";
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
      var row = 0;
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
  function drawHorizontalRule(frame, y, x, width) {
    if (width < 1 || y < 1 || y > frame.height) {
      return;
    }
    frame.gotoxy(x, y);
    frame.putmsg(repeatChar(BOX_HORIZONTAL, width), DARKGRAY);
  }
  function drawVerticalRule(frame, x, y, height) {
    var row = 0;
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
  function fillCells(frame, x, y, width, attr) {
    var column = 0;
    for (column = 0; column < width; column += 1) {
      frame.setData(x + column - 1, y - 1, " ", attr, false);
    }
  }
  function drawListRow(frame, x, y, width, text, selected, accentAttr) {
    var rowWidth = Math.max(1, width);
    var prefixWidth = Math.min(2, rowWidth);
    var textWidth = Math.max(0, rowWidth - prefixWidth);
    var visibleText = clipText(text, textWidth);
    var rowAttr = selected ? BG_CYAN | BLACK : LIGHTGRAY;
    var markerAttr = selected ? BG_CYAN | BLACK : accentAttr;
    var rowText = repeatChar(" ", prefixWidth) + visibleText;
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
  function writeStyledToken(frame, x, y, width, token) {
    var visible = clipText(token.text, width);
    if (x < 1 || y < 1 || width < 1 || !visible.length) {
      return 0;
    }
    frame.gotoxy(x, y);
    frame.putmsg(visible, token.attr);
    return visible.length;
  }
  function writeStyledTokens(frame, x, y, width, tokens) {
    var writeX = x;
    var remaining = width;
    var index = 0;
    for (index = 0; index < tokens.length; index += 1) {
      var token = tokens[index];
      if (!token) {
        continue;
      }
      var written = writeStyledToken(frame, writeX, y, remaining, token);
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
  function extractUnreadCount(metaText) {
    var match = String(metaText || "").match(/new\s+(\d+)/i);
    if (!match) {
      return 0;
    }
    return parseInt(match[1] || "0", 10) || 0;
  }
  function drawCommandFooter(frame, textTokens) {
    drawHorizontalRule(frame, frame.height - 1, 2, frame.width - 2);
    writeStyledTokens(frame, 3, frame.height - 1, Math.max(1, frame.width - 4), textTokens);
  }
  function drawChannelLikeRow(frame, x, y, width, entry, primaryLabel, selected) {
    var rowWidth = Math.max(1, width);
    var contentWidth = Math.max(0, rowWidth - 1);
    var unreadCount = extractUnreadCount(entry.metaText);
    var baseBg = selected ? BG_BLUE : BG_BLACK;
    var pointerAttr = BG_BLACK | LIGHTMAGENTA;
    var nameAttr = baseBg | (selected ? LIGHTCYAN : LIGHTRED);
    var primaryLabelAttr = baseBg | (selected ? LIGHTGRAY : CYAN);
    var primaryValueAttr = baseBg | (selected ? WHITE : LIGHTCYAN);
    var secondaryLabelAttr = baseBg | (selected ? CYAN : BROWN);
    var secondaryValueAttr = baseBg | (selected ? LIGHTGREEN : YELLOW);
    var separatorAttr = baseBg | LIGHTGRAY;
    var prefixText = entry.isCurrent ? "* " : "  ";
    var primaryTokens = [];
    var secondaryTokens = [];
    var nameWidth = 0;
    var nameText = "";
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
    nameWidth = contentWidth - prefixText.length - primaryTokens.reduce(function(total, token) {
      return total + token.text.length;
    }, 0) - secondaryTokens.reduce(function(total, token) {
      return total + token.text.length;
    }, 0);
    if (secondaryTokens.length && nameWidth < 6) {
      nameWidth += secondaryTokens.reduce(function(total, token) {
        return total + token.text.length;
      }, 0);
      secondaryTokens = [];
    }
    if (nameWidth < 1) {
      nameWidth = 1;
    }
    nameText = clipText(entry.name, nameWidth);
    writeStyledTokens(frame, x + 1, y, contentWidth, [
      { text: prefixText, attr: nameAttr },
      { text: nameText, attr: nameAttr }
    ].concat(primaryTokens, secondaryTokens));
  }
  function renderActionTokens(frame, actions) {
    var x = 1;
    var index = 0;
    frame.clear(BG_BLUE | WHITE);
    for (index = 0; index < actions.length; index += 1) {
      var action = actions[index];
      var token = "";
      var attr = index % 2 === 0 ? BG_CYAN | BLACK : BG_BLUE | WHITE;
      if (!action) {
        continue;
      }
      if (action.attr !== void 0) {
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
  function renderRosterDetails(frame, x, y, width, entry, context) {
    var avatarSize = resolveAvatarDimensions(context.avatarLib);
    var avatarX = x;
    var avatarY = y;
    var detailX = width >= avatarSize.width + 18 ? x + avatarSize.width + 2 : x;
    var detailY = width >= avatarSize.width + 18 ? y : y + avatarSize.height + 1;
    var avatarBinary = lookupAvatarBinary(context.avatarLib, entry.nick, context.ownAlias, context.ownUserNumber, context.avatarCache);
    var roleLabel = entry.isSelf ? "You" : "User";
    var nameLabel = clipText(entry.name, Math.max(8, width - (detailX - x)));
    var bbsTitle = "BBS";
    var bbsLabel = clipText(entry.bbs, Math.max(8, width - (detailX - x)));
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
  function renderRosterModal(frame, modal, context) {
    var contentWidth = Math.max(1, frame.width - 2);
    var contentHeight = Math.max(1, frame.height - 2);
    var listWidth = Math.min(28, Math.max(20, Math.floor((contentWidth - 3) / 2)));
    var detailX = listWidth + 4;
    var listHeight = Math.max(1, contentHeight - 3);
    var selectedIndex = modal.entries.length ? Math.max(0, Math.min(modal.selectedIndex, modal.entries.length - 1)) : 0;
    var topIndex = modal.entries.length > listHeight ? Math.max(0, Math.min(selectedIndex - Math.floor(listHeight / 2), modal.entries.length - listHeight)) : 0;
    var selectedEntry = modal.entries[selectedIndex] || modal.entries[0];
    var row = 0;
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
      var entry = modal.entries[topIndex + row];
      if (!entry) {
        break;
      }
      drawListRow(frame, 2, 2 + row, listWidth + 1, entry.name, topIndex + row === selectedIndex, entry.isSelf ? LIGHTCYAN : LIGHTMAGENTA);
    }
    if (selectedEntry) {
      renderRosterDetails(frame, detailX, 3, Math.max(10, frame.width - detailX - 1), selectedEntry, context);
    }
    frame.gotoxy(3, frame.height - 1);
    frame.putmsg(clipText("Up/Down move | Esc close", frame.width - 4), LIGHTBLUE);
  }
  function renderChannelsModal(frame, modal) {
    var bodyHeight = Math.max(1, frame.height - 3);
    var selectedIndex = modal.entries.length ? Math.max(0, Math.min(modal.selectedIndex, modal.entries.length - 1)) : 0;
    var topIndex = modal.entries.length > bodyHeight ? Math.max(0, Math.min(selectedIndex - Math.floor(bodyHeight / 2), modal.entries.length - bodyHeight)) : 0;
    var row = 0;
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
      var entry = modal.entries[topIndex + row];
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
  function renderPrivateThreadsModal(frame, modal) {
    var bodyHeight = Math.max(1, frame.height - 3);
    var selectedIndex = modal.entries.length ? Math.max(0, Math.min(modal.selectedIndex, modal.entries.length - 1)) : 0;
    var topIndex = modal.entries.length > bodyHeight ? Math.max(0, Math.min(selectedIndex - Math.floor(bodyHeight / 2), modal.entries.length - bodyHeight)) : 0;
    var row = 0;
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
      var entry = modal.entries[topIndex + row];
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
  function renderHelpModal(frame, modal) {
    var width = Math.max(1, frame.width - 4);
    var row = 0;
    var writeY = 2;
    drawBorder(frame, modal.title);
    drawHorizontalRule(frame, frame.height - 1, 2, frame.width - 2);
    for (row = 0; row < modal.lines.length; row += 1) {
      var wrapped = wrapText(modal.lines[row] || "", width);
      var innerIndex = 0;
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
  function renderActionBar(frame, actions) {
    renderActionTokens(frame, actions);
  }
  function renderModal(overlay, modalFrame, modal, context) {
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

  // repo/xtrn/avatar_chat/build/render/transcript-renderer.js
  function measureBubbleWidth(rows) {
    var width = 0;
    var index = 0;
    for (index = 0; index < rows.length; index += 1) {
      var row = rows[index];
      var line = row && row.isBubble ? row.text || "" : "";
      if (line.length > width) {
        width = line.length;
      }
    }
    return width;
  }
  function buildLayouts(messages, frameWidth, options) {
    var groups = groupMessages(messages, options.ownAlias);
    var avatarSize = resolveAvatarDimensions(options.avatarLib);
    var blocks = [];
    var maxBubbleWidth = Math.max(14, Math.min(frameWidth - avatarSize.width - 4, 54));
    var index = 0;
    for (index = 0; index < groups.length; index += 1) {
      var group = groups[index];
      if (!group) {
        continue;
      }
      if (group.kind === "notice") {
        var noticeText = group.messages[0] ? group.messages[0].text : "";
        var noticeLines = wrapText(noticeText, Math.max(10, frameWidth - 4));
        blocks.push({
          kind: "notice",
          lines: noticeLines,
          height: noticeLines.length
        });
        continue;
      }
      var prevGroupTime = 0;
      if (index > 0) {
        var prevGroup = groups[index - 1];
        if (prevGroup && prevGroup.messages.length) {
          var prevLast = prevGroup.messages[prevGroup.messages.length - 1];
          if (prevLast)
            prevGroupTime = prevLast.time;
        }
      }
      blocks.push(buildBubbleLayout(group, maxBubbleWidth, avatarSize.height, options, prevGroupTime));
    }
    return blocks;
  }
  function buildBubbleLayout(group, maxBubbleWidth, avatarHeight, options, prevGroupTime) {
    var rows = [];
    var lastMessage = group.messages.length ? group.messages[group.messages.length - 1] : null;
    var timestamp = "";
    if (lastMessage) {
      timestamp = formatRelativeTime(lastMessage.time, prevGroupTime || 0);
      var headerSpace = maxBubbleWidth - (group.speakerName || "").length - 2;
      if (headerSpace < timestamp.length) {
        timestamp = compactTimestamp(timestamp);
      }
    }
    var index = 0;
    var messageIndex = 0;
    var width = 0;
    var avatarBinary = null;
    for (messageIndex = 0; messageIndex < group.messages.length; messageIndex += 1) {
      var message = group.messages[messageIndex];
      var messageLines = wrapText(message ? message.text : "", Math.max(8, maxBubbleWidth - 2));
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
    avatarBinary = lookupAvatarBinary(options.avatarLib, group.nick, options.ownAlias, options.ownUserNumber, options.avatarCache);
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
  function pickVisibleBlocks(blocks, frameHeight, scrollOffsetBlocks) {
    var selected = [];
    var bottomIndex = Math.max(0, blocks.length - 1 - Math.max(0, scrollOffsetBlocks));
    var used = 0;
    var index = bottomIndex;
    while (index >= 0) {
      var block = blocks[index];
      if (!block) {
        index -= 1;
        continue;
      }
      var gap = selected.length > 0 ? 1 : 0;
      var nextUsed = used + gap + block.height;
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
      actualScrollOffsetBlocks: blocks.length ? blocks.length - 1 - bottomIndex : 0
    };
  }
  function usedHeight(blocks) {
    var total = 0;
    var index = 0;
    for (index = 0; index < blocks.length; index += 1) {
      var block = blocks[index];
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
  function renderNotice(frame, block, startRow) {
    var index = 0;
    for (index = 0; index < block.lines.length; index += 1) {
      var line = block.lines[index] || "";
      var leftOffset = Math.max(0, Math.floor((frame.width - line.length) / 2));
      frame.gotoxy(1 + leftOffset, startRow + index);
      frame.putmsg(line, DARKGRAY);
    }
    return startRow + block.lines.length;
  }
  function renderBubble(frame, block, startRow, avatarLib) {
    var avatarSize = resolveAvatarDimensions(avatarLib);
    var avatarX = block.side === "left" ? 1 : frame.width - avatarSize.width + 1;
    var bubbleX = block.side === "left" ? avatarSize.width + 3 : Math.max(1, avatarX - 2 - block.width);
    var headerText = block.speakerName + " " + block.timestamp;
    var headerX = block.side === "left" ? bubbleX : Math.max(1, avatarX - 2 - headerText.length);
    var bubbleAttr = block.side === "left" ? BG_CYAN | BLACK : BG_BLUE | WHITE;
    var speakerAttr = block.side === "left" ? LIGHTMAGENTA : LIGHTCYAN;
    var lineIndex = 0;
    frame.gotoxy(headerX, startRow);
    frame.putmsg(block.speakerName, speakerAttr);
    frame.putmsg(" ", LIGHTGRAY);
    frame.putmsg(block.timestamp, LIGHTBLUE);
    for (lineIndex = 0; lineIndex < block.rows.length; lineIndex += 1) {
      var row = block.rows[lineIndex];
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
  function renderTranscript(frame, messages, options) {
    var blocks = buildLayouts(messages, frame.width, options);
    var selected = pickVisibleBlocks(blocks, frame.height, options.scrollOffsetBlocks || 0);
    var visibleBlocks = selected.blocks;
    var totalHeight = usedHeight(visibleBlocks);
    var renderState = {
      visibleBlockCount: visibleBlocks.length,
      totalBlockCount: blocks.length,
      maxScrollOffsetBlocks: blocks.length ? blocks.length - 1 : 0,
      actualScrollOffsetBlocks: selected.actualScrollOffsetBlocks
    };
    var row = Math.max(1, frame.height - totalHeight + 1);
    var index = 0;
    frame.clear(BG_BLACK | LIGHTGRAY);
    if (!messages.length) {
      frame.gotoxy(1, Math.max(1, Math.floor(frame.height / 2)));
      frame.putmsg(centerText(options.emptyText, frame.width), DARKGRAY);
      return renderState;
    }
    for (index = 0; index < visibleBlocks.length; index += 1) {
      var block = visibleBlocks[index];
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

  // repo/xtrn/avatar_chat/build/app/avatar-chat-app.js
  var INPUT_ESCAPE_SEQUENCE_MAP = {
    "\x1B[A": KEY_UP,
    "\x1B[B": KEY_DOWN,
    "\x1B[C": KEY_RIGHT,
    "\x1B[D": KEY_LEFT,
    "\x1BOA": KEY_UP,
    "\x1BOB": KEY_DOWN,
    "\x1BOC": KEY_RIGHT,
    "\x1BOD": KEY_LEFT,
    "\x1B[H": KEY_HOME,
    "\x1B[F": KEY_END,
    "\x1BOH": KEY_HOME,
    "\x1BOF": KEY_END,
    "\x1B[1~": KEY_HOME,
    "\x1B[4~": KEY_END,
    "\x1B[7~": KEY_HOME,
    "\x1B[8~": KEY_END,
    "\x1B[5~": KEY_PAGEUP,
    "\x1B[6~": KEY_PAGEDN
  };
  var INPUT_ESCAPE_SEQUENCE_PREFIXES = {
    "\x1B": true
  };
  var BITMAP_MAX_HISTORY = 20;
  (function seedInputEscapePrefixes() {
    var sequence = "";
    var prefixLength = 0;
    for (sequence in INPUT_ESCAPE_SEQUENCE_MAP) {
      if (!Object.prototype.hasOwnProperty.call(INPUT_ESCAPE_SEQUENCE_MAP, sequence)) {
        continue;
      }
      for (prefixLength = 1; prefixLength < sequence.length; prefixLength += 1) {
        INPUT_ESCAPE_SEQUENCE_PREFIXES[sequence.substr(0, prefixLength)] = true;
      }
    }
  })();
  var AvatarChatApp = (
    /** @class */
    function() {
      function AvatarChatApp2(config) {
        this.config = config;
        var defaultChannel = this.getDefaultPublicChannel();
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
        this.channelOrder = [defaultChannel];
        this.currentChannel = defaultChannel;
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
        this.bitmapQueue = [];
        this.bitmapViewerActive = false;
        this.bitmapViewerIndex = -1;
        this.bitmapViewerScrollY = 0;
        this.bitmapViewerUse256Color = true;
        this.bitmapViewerSignature = "";
        this.unviewedBitmapCount = 0;
        this.motdText = "";
        this.motdTimestamp = 0;
        this.lastMotdRefreshAt = 0;
        try {
          this.avatarLib = load({}, "avatar_lib.js");
        } catch (error) {
          this.avatarLib = null;
          log("Avatar Chat: avatar_lib.js unavailable: " + String(error));
        }
      }
      AvatarChatApp2.prototype.run = function() {
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
      };
      AvatarChatApp2.prototype.getOwnAvatarData = function() {
        var avatarObj = null;
        if (!this.avatarLib || typeof this.avatarLib.read !== "function") {
          return void 0;
        }
        try {
          avatarObj = this.avatarLib.read(user.number, user.alias, null, null) || null;
        } catch (_error) {
          avatarObj = null;
        }
        if (!avatarObj || avatarObj.disabled || !avatarObj.data) {
          return void 0;
        }
        return String(avatarObj.data);
      };
      AvatarChatApp2.prototype.connect = function() {
        var desiredChannels = this.getJoinedPublicChannelNames();
        var desiredCurrent = this.currentChannel;
        var client;
        var index = 0;
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
          this.bitmapQueue = [];
          this.bitmapViewerActive = false;
          this.bitmapViewerIndex = -1;
          this.bitmapViewerScrollY = 0;
          this.bitmapViewerUse256Color = true;
          this.bitmapViewerSignature = "";
          this.unviewedBitmapCount = 0;
          this.motdText = "";
          this.motdTimestamp = 0;
          this.lastMotdRefreshAt = 0;
          for (index = 0; index < desiredChannels.length; index += 1) {
            var desiredChannel = desiredChannels[index];
            var channelName = normalizeChannelName(desiredChannel || this.getDefaultPublicChannel(), this.getDefaultPublicChannel());
            if (!this.getChannelByName(channelName)) {
              this.chat.join(channelName);
            }
          }
          this.loadPrivateHistory();
          this.syncPublicChannelUnreadCounts(false);
          this.syncChannelOrder();
          this.refreshMotd(true);
          if (desiredCurrent && (this.getChannelByName(desiredCurrent) || this.getPrivateThreadByName(desiredCurrent))) {
            this.currentChannel = desiredCurrent;
          } else if (this.channelOrder.length > 0) {
            this.currentChannel = this.channelOrder[0] || this.getDefaultPublicChannel();
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
      };
      AvatarChatApp2.prototype.cycleChat = function() {
        if (!this.chat) {
          return;
        }
        try {
          this.chat.cycle();
          this.syncPublicChannelUnreadCounts(true);
          this.syncPrivateHistory();
          this.syncChannelOrder();
          this.refreshMotd(false);
          this.trimHistories();
        } catch (error) {
          try {
            this.chat.disconnect();
          } catch (_disconnectError) {
          }
          this.chat = null;
          this.scheduleReconnect("Connection lost: " + String(error));
        }
      };
      AvatarChatApp2.prototype.installPrivateMessageHook = function() {
        var _this = this;
        var originalUpdate = null;
        if (!this.chat || typeof this.chat.update !== "function") {
          return;
        }
        originalUpdate = this.chat.update.bind(this.chat);
        this.chat.update = function(packet) {
          if (_this.handlePrivateMailboxPacket(packet)) {
            return true;
          }
          return originalUpdate ? originalUpdate(packet) : false;
        };
      };
      AvatarChatApp2.prototype.loadPrivateHistory = function() {
        var historyLimit = Math.max(50, this.config.maxHistory * 2);
        var history = [];
        var index = 0;
        if (!this.chat) {
          return;
        }
        this.ensureHistoryArray(this.getMailboxHistoryPath());
        try {
          history = this.chat.client.slice("chat", this.getMailboxHistoryPath(), -historyLimit, void 0, 1) || [];
        } catch (_error) {
          history = [];
        }
        for (index = 0; index < history.length; index += 1) {
          var message = history[index];
          if (!isPrivateMessage(message)) {
            continue;
          }
          this.appendPrivateMessage(message, false);
        }
        this.lastPrivateHistorySyncAt = (/* @__PURE__ */ new Date()).getTime();
      };
      AvatarChatApp2.prototype.handlePrivateMailboxPacket = function(packet) {
        var message = null;
        if (!packet || packet.oper !== "WRITE" || packet.location !== this.getMailboxMessagesPath()) {
          return false;
        }
        message = packet.data;
        if (!isPrivateMessage(message)) {
          return false;
        }
        this.appendPrivateMessage(message, true);
        return true;
      };
      AvatarChatApp2.prototype.appendPrivateMessage = function(message, markUnread) {
        var peerNick = resolvePrivatePeerNick(message, user.alias);
        var isIncoming = !this.isOwnMessage(message);
        var thread;
        var addedBitmap = false;
        if (!peerNick) {
          return;
        }
        if (!this.rememberPrivateMessage(message)) {
          return;
        }
        thread = this.ensurePrivateThread(peerNick);
        thread.messages.push(message);
        trimChannelMessages(thread, this.config.maxHistory);
        addedBitmap = this.processBitmapMessage(thread.name, message);
        if (addedBitmap && isIncoming) {
          thread.messages.push(buildNoticeMessage(this.buildPrivateBitmapNoticeText(message), message.time));
          trimChannelMessages(thread, this.config.maxHistory);
        }
        if (markUnread && message.nick && message.nick.name && message.nick.name.toUpperCase() !== user.alias.toUpperCase() && this.currentChannel.toUpperCase() !== thread.name.toUpperCase()) {
          thread.unreadCount += 1;
          this.lastPrivateSender = peerNick;
        }
        if (message.nick && message.nick.name && message.nick.name.toUpperCase() !== user.alias.toUpperCase()) {
          this.lastPrivateSender = peerNick;
        }
        this.syncChannelOrder();
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.ensurePrivateThread = function(peerNick) {
        var normalizedPeer = normalizePrivateNick(peerNick) || peerNick;
        var canonicalPeer = this.resolvePrivateTargetNick(normalizedPeer.name) || normalizedPeer;
        var threadId = buildPrivateThreadId(canonicalPeer);
        var thread = this.privateThreads[threadId];
        var previousName = "";
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
      };
      AvatarChatApp2.prototype.getPrivateThreadByName = function(name) {
        var index = 0;
        for (index = 0; index < this.privateThreadOrder.length; index += 1) {
          var threadId = this.privateThreadOrder[index];
          var thread = threadId ? this.privateThreads[threadId] : null;
          if (thread && thread.name.toUpperCase() === name.toUpperCase()) {
            return thread;
          }
        }
        return null;
      };
      AvatarChatApp2.prototype.getMotdChannelName = function() {
        return normalizeChannelName(this.config.motdChannel || "motd", "motd");
      };
      AvatarChatApp2.prototype.isMotdChannelName = function(name) {
        var trimmed = trimText(String(name || ""));
        if (!trimmed.length) {
          return false;
        }
        return normalizeChannelName(trimmed, "").toUpperCase() === this.getMotdChannelName().toUpperCase();
      };
      AvatarChatApp2.prototype.isCurrentUserSysop = function() {
        if (user.is_sysop === true) {
          return true;
        }
        if (user.security && typeof user.security.level === "number" && user.security.level >= 90) {
          return true;
        }
        if (typeof user.compare_ars === "function") {
          try {
            return user.compare_ars("SYSOP") === true;
          } catch (_error) {
          }
        }
        return false;
      };
      AvatarChatApp2.prototype.isMotdHostSystem = function() {
        var configuredQwkid = trimText(String(this.config.motdHostQwkid || "")).toUpperCase();
        var configuredSystem = trimText(String(this.config.motdHostSystem || "")).toUpperCase();
        var configuredHost = trimText(String(this.config.host || "")).toUpperCase();
        var localQwkid = trimText(String(system.qwk_id || "")).toUpperCase();
        var localSystem = trimText(String(system.name || "")).toUpperCase();
        if (configuredQwkid.length) {
          return localQwkid === configuredQwkid;
        }
        if (configuredSystem.length) {
          return localSystem === configuredSystem;
        }
        if (configuredHost === "127.0.0.1" || configuredHost === "LOCALHOST") {
          return true;
        }
        return localSystem === configuredHost;
      };
      AvatarChatApp2.prototype.canManageMotd = function() {
        return this.isCurrentUserSysop() && this.isMotdHostSystem();
      };
      AvatarChatApp2.prototype.getDefaultPublicChannel = function() {
        var defaultChannel = normalizeChannelName(this.config.defaultChannel || "main", "main");
        if (this.isMotdChannelName(defaultChannel) && !this.canManageMotd()) {
          return "main";
        }
        return defaultChannel;
      };
      AvatarChatApp2.prototype.shouldIncludePublicChannel = function(channelName) {
        if (!channelName || channelName.charAt(0) === "@") {
          return false;
        }
        if (this.isMotdChannelName(channelName) && !this.canManageMotd()) {
          return false;
        }
        return true;
      };
      AvatarChatApp2.prototype.getJoinedPublicChannelNames = function() {
        var publicChannels = [];
        var index = 0;
        for (index = 0; index < this.channelOrder.length; index += 1) {
          var channelName = this.channelOrder[index];
          if (channelName && this.shouldIncludePublicChannel(channelName)) {
            publicChannels.push(channelName);
          }
        }
        if (this.canManageMotd() && !this.channelExists(publicChannels, this.getMotdChannelName())) {
          publicChannels.push(this.getMotdChannelName());
        }
        if (!publicChannels.length) {
          publicChannels.push(this.getDefaultPublicChannel());
        }
        return publicChannels;
      };
      AvatarChatApp2.prototype.getMailboxMessagesPath = function() {
        return "channels." + user.alias + ".messages";
      };
      AvatarChatApp2.prototype.getMailboxHistoryPath = function() {
        return "channels." + user.alias + ".history";
      };
      AvatarChatApp2.prototype.buildMotdPreview = function(message) {
        var text = trimText(String(message && message.str ? message.str : "").replace(/\s+/g, " "));
        var parsed = parseBitmapMessage(text);
        if (parsed) {
          return "[image " + String(parsed.width || 0) + "x" + String(parsed.height || 0) + "]";
        }
        return text;
      };
      AvatarChatApp2.prototype.refreshMotd = function(force) {
        var now = (/* @__PURE__ */ new Date()).getTime();
        var motdChannel = this.getMotdChannelName();
        var history = [];
        var nextText = "";
        var nextTimestamp = 0;
        var index = 0;
        if (!this.chat) {
          if (this.motdText.length || this.motdTimestamp > 0) {
            this.motdText = "";
            this.motdTimestamp = 0;
            this.headerSignature = "";
          }
          this.lastMotdRefreshAt = 0;
          return;
        }
        if (!force && now - this.lastMotdRefreshAt < 5e3) {
          return;
        }
        this.lastMotdRefreshAt = now;
        try {
          history = this.chat.client.slice("chat", "channels." + motdChannel + ".history", -10, void 0, 1) || [];
        } catch (_error) {
          history = [];
        }
        for (index = history.length - 1; index >= 0; index -= 1) {
          var message = history[index];
          var preview = this.buildMotdPreview(message);
          if (!message || isPrivateMessage(message) || !preview.length) {
            continue;
          }
          nextText = preview;
          nextTimestamp = message.time || 0;
          break;
        }
        if (nextText !== this.motdText || nextTimestamp !== this.motdTimestamp) {
          this.motdText = nextText;
          this.motdTimestamp = nextTimestamp;
          this.headerSignature = "";
        }
      };
      AvatarChatApp2.prototype.ensureHistoryArray = function(location) {
        var existing;
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
      };
      AvatarChatApp2.prototype.normalizeBitmapSenderName = function(name) {
        var trimmed = trimText(name);
        if (trimmed.indexOf("BLOCKBRAIN:") === 0) {
          return trimText(trimmed.substr(11));
        }
        if (trimmed.indexOf("DISCORD:") === 0) {
          return trimText(trimmed.substr(8));
        }
        return trimmed;
      };
      AvatarChatApp2.prototype.isOwnNickName = function(name) {
        return !!name.length && name.toUpperCase() === user.alias.toUpperCase();
      };
      AvatarChatApp2.prototype.isOwnMessage = function(message) {
        return !!(message && message.nick && message.nick.name && this.isOwnNickName(message.nick.name));
      };
      AvatarChatApp2.prototype.buildPrivateBitmapNoticeText = function(message) {
        var sender = "";
        if (message && message.nick && message.nick.name) {
          sender = this.normalizeBitmapSenderName(message.nick.name);
        }
        if (!sender.length) {
          sender = "Someone";
        }
        return sender + " sent you a private bitmap. Type /img to see it.";
      };
      AvatarChatApp2.prototype.formatBitmapViewerElapsed = function(timestamp) {
        var diffMs = Math.max(0, (/* @__PURE__ */ new Date()).getTime() - timestamp);
        var totalMinutes = Math.floor(diffMs / 6e4);
        var days = Math.floor(totalMinutes / 1440);
        var hours = Math.floor(totalMinutes % 1440 / 60);
        var minutes = totalMinutes % 60;
        if (days > 0) {
          return String(days) + "d" + (hours > 0 ? " " + String(hours) + "h" : "") + " ago";
        }
        if (hours > 0) {
          return String(hours) + "h" + (minutes > 0 ? " " + String(minutes) + "m" : "") + " ago";
        }
        if (minutes > 0) {
          return String(minutes) + "m ago";
        }
        return "now";
      };
      AvatarChatApp2.prototype.formatPrivateBitmapViewerTimestamp = function(timestamp) {
        var friendly = compactTimestamp(formatRelativeTime(timestamp));
        var elapsed = this.formatBitmapViewerElapsed(timestamp);
        if (!friendly.length) {
          return elapsed;
        }
        if (!elapsed.length) {
          return friendly;
        }
        return friendly + " - " + elapsed;
      };
      AvatarChatApp2.prototype.renderBitmapViewerTitle = function(frame, entry) {
        var fillAttr = BG_BLACK | LIGHTGRAY;
        var remaining = frame.width;
        function writeSegment(text, attr) {
          var chunk = text || "";
          if (remaining < 1 || !chunk.length) {
            return;
          }
          if (chunk.length > remaining) {
            chunk = chunk.substr(0, remaining);
          }
          frame.putmsg(chunk, attr);
          remaining -= chunk.length;
        }
        frame.gotoxy(1, 1);
        if (entry.isIncomingPrivate) {
          writeSegment("[", BG_BLACK | WHITE);
          writeSegment("PRIVATE IMAGE", BG_BLACK | LIGHTRED);
          writeSegment("] ", BG_BLACK | WHITE);
          writeSegment(entry.fromName, BG_BLACK | YELLOW);
          writeSegment(" sent to you ", fillAttr);
          writeSegment(this.formatPrivateBitmapViewerTimestamp(entry.time), BG_BLACK | WHITE);
          writeSegment(".", fillAttr);
        } else {
          writeSegment("Image " + String(this.bitmapViewerIndex + 1) + "/" + String(this.bitmapQueue.length) + " | " + entry.fromName + " | " + entry.sourceChannel, BG_BLACK | LIGHTCYAN);
        }
        if (remaining > 0) {
          frame.putmsg(padRight("", remaining), fillAttr);
        }
      };
      AvatarChatApp2.prototype.processBitmapMessage = function(sourceChannel, message) {
        var text = message && message.str ? String(message.str) : "";
        var parsed = parseBitmapMessage(text);
        var decoded;
        var sender = "";
        var entry;
        if (!parsed) {
          return false;
        }
        try {
          decoded = decodeBitmap(parsed.hexData, parsed.width, parsed.height);
        } catch (error) {
          log("Avatar Chat bitmap decode error: " + String(error));
          this.appendViewNotice(sourceChannel, "Received an image payload that could not be decoded.");
          return false;
        }
        if (!decoded.bitmap.length) {
          this.appendViewNotice(sourceChannel, "Received an empty image payload.");
          return false;
        }
        sender = this.normalizeBitmapSenderName(parsed.fromName || (message.nick && message.nick.name ? message.nick.name : "Image"));
        if (!sender.length) {
          sender = "Image";
        }
        entry = {
          bitmap: decoded.bitmap,
          width: decoded.width,
          height: decoded.height,
          actualWidth: decoded.actualWidth,
          actualHeight: decoded.actualHeight,
          fromName: sender,
          sourceChannel: sourceChannel,
          time: message && message.time ? message.time : (/* @__PURE__ */ new Date()).getTime(),
          isPrivate: sourceChannel.charAt(0) === "@",
          isIncomingPrivate: sourceChannel.charAt(0) === "@" && !this.isOwnMessage(message)
        };
        this.bitmapQueue.push(entry);
        while (this.bitmapQueue.length > BITMAP_MAX_HISTORY) {
          this.bitmapQueue.shift();
          if (this.bitmapViewerIndex > 0) {
            this.bitmapViewerIndex -= 1;
          }
        }
        if (this.bitmapQueue.length && this.bitmapViewerIndex >= this.bitmapQueue.length) {
          this.bitmapViewerIndex = this.bitmapQueue.length - 1;
        }
        this.unviewedBitmapCount += 1;
        if (this.unviewedBitmapCount > this.bitmapQueue.length) {
          this.unviewedBitmapCount = this.bitmapQueue.length;
        }
        this.actionSignature = "";
        this.statusSignature = "";
        this.bitmapViewerSignature = "";
        return true;
      };
      AvatarChatApp2.prototype.getRenderableMessages = function(messages) {
        var visible = [];
        var index = 0;
        for (index = 0; index < messages.length; index += 1) {
          var message = messages[index];
          if (!message) {
            continue;
          }
          if (message.str && isBitmapMessage(String(message.str))) {
            continue;
          }
          visible.push(message);
        }
        return visible;
      };
      AvatarChatApp2.prototype.hasImagesForView = function(viewName) {
        var normalized = trimText(viewName).toUpperCase();
        var index = 0;
        if (!normalized.length) {
          return false;
        }
        for (index = 0; index < this.bitmapQueue.length; index += 1) {
          var entry = this.bitmapQueue[index];
          if (entry && entry.sourceChannel.toUpperCase() === normalized) {
            return true;
          }
        }
        return false;
      };
      AvatarChatApp2.prototype.findLatestBitmapIndexForView = function(viewName) {
        var normalized = trimText(viewName).toUpperCase();
        var index = 0;
        if (!normalized.length) {
          return -1;
        }
        for (index = this.bitmapQueue.length - 1; index >= 0; index -= 1) {
          var entry = this.bitmapQueue[index];
          if (entry && entry.sourceChannel.toUpperCase() === normalized) {
            return index;
          }
        }
        return -1;
      };
      AvatarChatApp2.prototype.openBitmapViewer = function() {
        var startIndex = this.bitmapQueue.length - 1;
        if (!this.bitmapQueue.length) {
          if (this.currentChannel.length) {
            this.appendViewNotice(this.currentChannel, "No images queued yet.");
          }
          return;
        }
        if (this.modalState) {
          this.closeModal();
        }
        this.bitmapViewerActive = true;
        if (this.currentChannel.length) {
          var currentViewIndex = this.findLatestBitmapIndexForView(this.currentChannel);
          if (currentViewIndex >= 0) {
            startIndex = currentViewIndex;
          }
        }
        this.bitmapViewerIndex = startIndex;
        this.bitmapViewerScrollY = 0;
        this.bitmapViewerUse256Color = true;
        this.bitmapViewerSignature = "";
        this.unviewedBitmapCount = 0;
        this.actionSignature = "";
        this.statusSignature = "";
        this.transcriptSignature = "";
        this.destroyModalFrames();
      };
      AvatarChatApp2.prototype.closeBitmapViewer = function() {
        if (!this.bitmapViewerActive) {
          return;
        }
        this.bitmapViewerActive = false;
        this.bitmapViewerIndex = -1;
        this.bitmapViewerScrollY = 0;
        this.bitmapViewerSignature = "";
        this.destroyModalFrames();
        try {
          console.print("\x1B[0m");
          console.attributes = WHITE;
        } catch (_error) {
        }
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.handleBitmapViewerInput = function(key) {
        if (!this.bitmapViewerActive) {
          return false;
        }
        switch (key) {
          case KEY_LEFT:
          case "[":
            this.moveBitmapViewer(-1);
            return true;
          case KEY_RIGHT:
          case "]":
            this.moveBitmapViewer(1);
            return true;
          case KEY_UP:
            this.scrollBitmapViewer(-1);
            return true;
          case KEY_DOWN:
            this.scrollBitmapViewer(1);
            return true;
          case KEY_PAGEUP:
            this.scrollBitmapViewer(-10);
            return true;
          case KEY_PAGEDN:
            this.scrollBitmapViewer(10);
            return true;
          case KEY_HOME:
            this.bitmapViewerScrollY = 0;
            this.bitmapViewerSignature = "";
            return true;
          case KEY_END:
            this.bitmapViewerScrollY = this.getBitmapViewerMaxScrollY();
            this.bitmapViewerSignature = "";
            return true;
          case "c":
          case "C":
            this.bitmapViewerUse256Color = !this.bitmapViewerUse256Color;
            this.bitmapViewerSignature = "";
            return true;
          case "\f":
            this.bitmapViewerSignature = "";
            return true;
          case KEY_ESC:
          case "\x1B":
          case "\r":
          case " ":
            this.closeBitmapViewer();
            return true;
          default:
            this.closeBitmapViewer();
            return true;
        }
      };
      AvatarChatApp2.prototype.moveBitmapViewer = function(delta) {
        var nextIndex = this.bitmapViewerIndex + delta;
        if (nextIndex < 0 || nextIndex >= this.bitmapQueue.length) {
          return;
        }
        this.bitmapViewerIndex = nextIndex;
        this.bitmapViewerScrollY = 0;
        this.bitmapViewerSignature = "";
      };
      AvatarChatApp2.prototype.getBitmapViewerMaxScrollY = function() {
        var entry = this.bitmapViewerIndex >= 0 ? this.bitmapQueue[this.bitmapViewerIndex] : null;
        var contentHeight = Math.max(1, this.frames.height - 2);
        if (!entry) {
          return 0;
        }
        return Math.max(0, entry.height - contentHeight);
      };
      AvatarChatApp2.prototype.scrollBitmapViewer = function(delta) {
        var maxScroll = this.getBitmapViewerMaxScrollY();
        var nextScroll = clamp(this.bitmapViewerScrollY + delta, 0, maxScroll);
        if (nextScroll === this.bitmapViewerScrollY) {
          return;
        }
        this.bitmapViewerScrollY = nextScroll;
        this.bitmapViewerSignature = "";
      };
      AvatarChatApp2.prototype.buildPublicMessageKey = function(channelName, message) {
        var sender = normalizePrivateNick(message.nick || null);
        var senderKey = sender ? this.normalizePrivateLookupKey(sender.name) + "|" + (sender.qwkid || String(sender.host || "").toUpperCase()) : "NOTICE";
        return [
          normalizeChannelName(channelName).toUpperCase(),
          String(message.time || 0),
          senderKey,
          String(message.str || "")
        ].join("|");
      };
      AvatarChatApp2.prototype.rememberPublicChannelMessage = function(channelName, message) {
        var key = this.buildPublicMessageKey(channelName, message);
        if (this.publicChannelMessageKeys[key]) {
          return false;
        }
        this.publicChannelMessageKeys[key] = true;
        return true;
      };
      AvatarChatApp2.prototype.syncPublicChannelUnreadCounts = function(markUnread) {
        var key = "";
        var changed = false;
        if (!this.chat) {
          return;
        }
        for (key in this.chat.channels) {
          if (!Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
            continue;
          }
          var channel = this.chat.channels[key];
          var unreadKey = channel && channel.name ? channel.name.toUpperCase() : "";
          var index = 0;
          if (!channel || !channel.name || !this.shouldIncludePublicChannel(channel.name)) {
            continue;
          }
          if (this.publicChannelUnreadCounts[unreadKey] === void 0) {
            this.publicChannelUnreadCounts[unreadKey] = 0;
          }
          for (index = 0; index < channel.messages.length; index += 1) {
            var message = channel.messages[index];
            if (message && message.nick && message.nick.avatar) {
              var avatarData = String(message.nick.avatar).replace(/^\s+|\s+$/g, "");
              if (avatarData.length) {
                this.embeddedAvatars[message.nick.name.toUpperCase()] = avatarData;
              }
            }
            if (!message || !this.rememberPublicChannelMessage(channel.name, message)) {
              continue;
            }
            this.processBitmapMessage(channel.name, message);
            if (!message.nick) {
              this.enrichJoinLeaveNotice(message, channel);
            }
            if (markUnread && channel.name.toUpperCase() !== this.currentChannel.toUpperCase() && message.nick && message.nick.name && message.nick.name.toUpperCase() !== user.alias.toUpperCase()) {
              this.publicChannelUnreadCounts[unreadKey] += 1;
              changed = true;
            }
          }
        }
        if (changed) {
          this.modalSignature = "";
        }
      };
      AvatarChatApp2.prototype.buildPrivateMessageKey = function(message) {
        var sender = normalizePrivateNick(message.nick || null);
        var recipient = normalizePrivateNick(message.private ? message.private.to : null);
        var senderKey = sender ? this.normalizePrivateLookupKey(sender.name) + "|" + (sender.qwkid || String(sender.host || "").toUpperCase()) : "";
        var recipientKey = recipient ? this.normalizePrivateLookupKey(recipient.name) + "|" + (recipient.qwkid || String(recipient.host || "").toUpperCase()) : "";
        return [
          String(message.time || 0),
          senderKey,
          recipientKey,
          String(message.str || "")
        ].join("|");
      };
      AvatarChatApp2.prototype.rememberPrivateMessage = function(message) {
        var key = this.buildPrivateMessageKey(message);
        if (this.privateMessageKeys[key]) {
          return false;
        }
        this.privateMessageKeys[key] = true;
        return true;
      };
      AvatarChatApp2.prototype.syncPrivateHistory = function() {
        var now = (/* @__PURE__ */ new Date()).getTime();
        var historyLimit = Math.max(50, this.config.maxHistory * 2);
        var history = [];
        var index = 0;
        if (!this.chat) {
          return;
        }
        if (this.lastPrivateHistorySyncAt && now - this.lastPrivateHistorySyncAt < 1e3) {
          return;
        }
        this.lastPrivateHistorySyncAt = now;
        try {
          history = this.chat.client.slice("chat", this.getMailboxHistoryPath(), -historyLimit, void 0, 1) || [];
        } catch (_error) {
          history = [];
        }
        for (index = 0; index < history.length; index += 1) {
          var message = history[index];
          if (!isPrivateMessage(message)) {
            continue;
          }
          this.appendPrivateMessage(message, true);
        }
      };
      AvatarChatApp2.prototype.reconnectIfDue = function() {
        if (this.chat || !this.reconnectAt) {
          return;
        }
        if ((/* @__PURE__ */ new Date()).getTime() >= this.reconnectAt) {
          this.connect();
        }
      };
      AvatarChatApp2.prototype.scheduleReconnect = function(errorText) {
        this.lastError = errorText;
        this.reconnectAt = (/* @__PURE__ */ new Date()).getTime() + this.config.reconnectDelayMs;
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.handlePendingInput = function() {
        var key = this.readInputKey();
        if (!key) {
          return;
        }
        this.lastKeyTimestamp = Date.now();
        if (this.idleAnimActive) {
          this.stopIdleAnimations();
        }
        if (this.bitmapViewerActive && this.handleBitmapViewerInput(key)) {
          return;
        }
        if (this.modalState && this.handleModalInput(key)) {
          return;
        }
        switch (key) {
          case KEY_ESC:
          case "\x1B":
          case "":
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
          case "	":
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
          case "\x7F":
            this.inputBuffer.backspace();
            return;
          case "\f":
            this.resetRenderSignatures();
            return;
          default:
            if (this.isPrintable(key)) {
              this.inputBuffer.insert(key);
            }
            return;
        }
      };
      AvatarChatApp2.prototype.readInputKey = function() {
        var mode = K_NOCRLF | K_NOECHO | K_NOSPIN | K_EXTKEYS;
        var key = console.inkey(mode, this.pendingEscapeSequence.length ? 0 : this.config.pollDelayMs);
        if (!key) {
          if (this.pendingEscapeSequence.length && (/* @__PURE__ */ new Date()).getTime() - this.pendingEscapeAt >= 75) {
            this.pendingEscapeSequence = "";
            this.pendingEscapeAt = 0;
            return KEY_ESC;
          }
          return "";
        }
        return this.normalizeInputKey(key, mode);
      };
      AvatarChatApp2.prototype.normalizeInputKey = function(key, mode) {
        var next = "";
        var mapped = "";
        if (this.pendingEscapeSequence.length) {
          this.pendingEscapeSequence += key;
          return this.resolveEscapeSequence(mode);
        }
        if (key === KEY_ESC || key === "\x1B") {
          this.pendingEscapeSequence = KEY_ESC;
          this.pendingEscapeAt = (/* @__PURE__ */ new Date()).getTime();
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
      };
      AvatarChatApp2.prototype.normalizeImmediateKey = function(key) {
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
      };
      AvatarChatApp2.prototype.resolveEscapeSequence = function(mode) {
        var next = "";
        var mapped = "";
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
      };
      AvatarChatApp2.prototype.handleModalInput = function(key) {
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
              var selected = this.modalState.entries[this.modalState.selectedIndex];
              if (selected) {
                this.currentChannel = selected.name;
                this.markCurrentViewRead(this.currentChannel);
                this.scrollTranscriptToLatest();
              }
            }
            this.closeModal();
            return true;
          case KEY_ESC:
          case "\x1B":
            this.closeModal();
            return true;
          case "\f":
            this.resetRenderSignatures();
            return true;
          default:
            return true;
        }
      };
      AvatarChatApp2.prototype.submitInput = function() {
        var text = this.inputBuffer.getValue();
        var trimmed = trimText(text);
        var privateThread = this.currentChannel.length ? this.getPrivateThreadByName(this.currentChannel) : null;
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
      };
      AvatarChatApp2.prototype.sendMessage = function(channelName, text) {
        var channel = this.getChannelByName(channelName);
        var timestamp = (/* @__PURE__ */ new Date()).getTime();
        var ownAvatar = this.getOwnAvatarData();
        var message = {
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
        if (this.isMotdChannelName(channel.name) && !this.canManageMotd()) {
          return false;
        }
        try {
          this.chat.client.write("chat", "channels." + channel.name + ".messages", message, 2);
          this.chat.client.push("chat", "channels." + channel.name + ".history", message, 2);
          channel.messages.push(message);
          trimChannelMessages(channel, this.config.maxHistory);
          this.transcriptSignature = "";
          this.statusSignature = "";
          if (this.isMotdChannelName(channel.name)) {
            this.motdText = this.buildMotdPreview(message);
            this.motdTimestamp = timestamp;
            this.lastMotdRefreshAt = timestamp;
            this.headerSignature = "";
          }
          return true;
        } catch (_error) {
          return false;
        }
      };
      AvatarChatApp2.prototype.sendPrivateMessage = function(targetNick, text) {
        var recipient = normalizePrivateNick(targetNick);
        var ownAvatar = this.getOwnAvatarData();
        var sender = normalizePrivateNick({
          name: user.alias,
          host: system.name,
          ip: user.ip_address,
          qwkid: system.qwk_id,
          avatar: ownAvatar
        });
        var timestamp = (/* @__PURE__ */ new Date()).getTime();
        var thread;
        var message;
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
          trimChannelMessages(thread, this.config.maxHistory);
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
      };
      AvatarChatApp2.prototype.handleSlashCommand = function(commandText) {
        var trimmedCommand = trimText(commandText.substr(1));
        var parts = trimmedCommand.split(/\s+/);
        var firstPart = parts.length ? parts[0] || "" : "";
        var verb = firstPart.length ? firstPart.toUpperCase() : "";
        var args = trimmedCommand.length > verb.length ? trimText(trimmedCommand.substr(verb.length)) : "";
        var targetChannel = "";
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
          case "IMG":
          case "IMAGE":
          case "IMAGES":
          case "ART":
          case "PIC":
          case "PICS":
            this.openBitmapViewer();
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
        if (verb === "JOIN") {
          targetChannel = normalizeChannelName(args, this.getDefaultPublicChannel());
          if (this.isMotdChannelName(targetChannel) && !this.canManageMotd()) {
            this.appendViewNotice(this.currentChannel, "The motd channel is reserved for the host sysop.");
            return;
          }
        }
        if (!this.chat.getcmd(this.currentChannel, commandText)) {
          this.appendNotice(this.currentChannel, "Unknown command: " + commandText);
          return;
        }
        if (verb === "JOIN") {
          targetChannel = normalizeChannelName(args, this.getDefaultPublicChannel());
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
            this.currentChannel = this.channelOrder.length ? this.channelOrder[0] || "" : "";
            this.markCurrentViewRead(this.currentChannel);
            this.scrollTranscriptToLatest();
          }
          this.resetRenderSignatures();
          return;
        }
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.handlePrivateCommand = function(args) {
        var parsed = this.parsePrivateCommandArgs(args);
        var recipientName = "";
        var messageText = "";
        var targetNick;
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
      };
      AvatarChatApp2.prototype.handlePrivateReplyCommand = function(args) {
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
      };
      AvatarChatApp2.prototype.resolvePrivateTargetNick = function(name) {
        var trimmedName = trimText(name);
        var normalizedTarget = this.normalizePrivateLookupKey(trimmedName);
        var key = "";
        var index = 0;
        if (!trimmedName.length) {
          return null;
        }
        if (this.chat) {
          for (key in this.chat.channels) {
            if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
              var channel = this.chat.channels[key];
              var rosterIndex = 0;
              if (!channel || !channel.users) {
                continue;
              }
              for (rosterIndex = 0; rosterIndex < channel.users.length; rosterIndex += 1) {
                var rosterEntry = this.extractRosterEntry(channel.users[rosterIndex]);
                if (rosterEntry && rosterEntry.nick && (rosterEntry.name.toUpperCase() === trimmedName.toUpperCase() || this.normalizePrivateLookupKey(rosterEntry.name) === normalizedTarget)) {
                  return rosterEntry.nick;
                }
              }
            }
          }
        }
        for (index = 0; index < this.privateThreadOrder.length; index += 1) {
          var threadId = this.privateThreadOrder[index];
          var thread = threadId ? this.privateThreads[threadId] : null;
          if (thread && (thread.peerNick.name.toUpperCase() === trimmedName.toUpperCase() || this.normalizePrivateLookupKey(thread.peerNick.name) === normalizedTarget)) {
            return thread.peerNick;
          }
        }
        return null;
      };
      AvatarChatApp2.prototype.normalizePrivateLookupKey = function(text) {
        return trimText(text).replace(/[^A-Za-z0-9]/g, "").toUpperCase();
      };
      AvatarChatApp2.prototype.parsePrivateCommandArgs = function(args) {
        var trimmedArgs = trimText(args);
        var recipientName = "";
        var messageText = "";
        var closingQuoteIndex = 0;
        var spaceIndex = 0;
        var candidateNames;
        var candidateIndex = 0;
        if (!trimmedArgs.length) {
          return null;
        }
        if (trimmedArgs.charAt(0) === '"') {
          closingQuoteIndex = trimmedArgs.indexOf('"', 1);
          if (closingQuoteIndex < 2) {
            return null;
          }
          recipientName = trimText(trimmedArgs.substring(1, closingQuoteIndex));
          messageText = trimText(trimmedArgs.substr(closingQuoteIndex + 1));
          return recipientName.length && messageText.length ? { recipientName: recipientName, messageText: messageText } : null;
        }
        candidateNames = this.listPrivateTargetNames();
        for (candidateIndex = 0; candidateIndex < candidateNames.length; candidateIndex += 1) {
          var candidateName = candidateNames[candidateIndex] || "";
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
        return recipientName.length && messageText.length ? { recipientName: recipientName, messageText: messageText } : null;
      };
      AvatarChatApp2.prototype.listPrivateTargetNames = function() {
        var seen = {};
        var names = [];
        var index = 0;
        var key = "";
        for (index = 0; index < this.privateThreadOrder.length; index += 1) {
          var threadId = this.privateThreadOrder[index];
          var thread = threadId ? this.privateThreads[threadId] : null;
          var name = thread && thread.peerNick ? trimText(thread.peerNick.name) : "";
          if (name.length && !seen[name.toUpperCase()]) {
            seen[name.toUpperCase()] = true;
            names.push(name);
          }
        }
        if (this.chat) {
          for (key in this.chat.channels) {
            if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
              var channel = this.chat.channels[key];
              if (!channel || !channel.users) {
                continue;
              }
              for (index = 0; index < channel.users.length; index += 1) {
                var rosterEntry = this.extractRosterEntry(channel.users[index]);
                var name = rosterEntry ? trimText(rosterEntry.name) : "";
                if (name.length && !seen[name.toUpperCase()]) {
                  seen[name.toUpperCase()] = true;
                  names.push(name);
                }
              }
            }
          }
        }
        names.sort(function(left, right) {
          return right.length - left.length;
        });
        return names;
      };
      AvatarChatApp2.prototype.handleTabCompletion = function() {
        var completion = this.buildTabCompletion();
        if (!completion) {
          return;
        }
        this.inputBuffer.setValue(completion.value, completion.cursor);
        this.inputSignature = "";
      };
      AvatarChatApp2.prototype.buildTabCompletion = function() {
        var value = this.inputBuffer.getValue();
        var cursor = this.inputBuffer.getCursor();
        var privateCompletion = this.buildPrivateCommandTabCompletion(value, cursor);
        if (privateCompletion) {
          return privateCompletion;
        }
        return this.buildGenericUserTabCompletion(value, cursor);
      };
      AvatarChatApp2.prototype.buildPrivateCommandTabCompletion = function(value, cursor) {
        var beforeCursor = value.substr(0, cursor);
        var afterCursor = value.substr(cursor);
        var match = beforeCursor.match(/^(\/(?:msg|pm|tell|whisper)\s+)([\s\S]*)$/i);
        var fragment = "";
        var candidate = "";
        var completed = "";
        if (!match || trimText(afterCursor).length) {
          return null;
        }
        fragment = match[2] || "";
        if (!fragment.length) {
          return null;
        }
        if (fragment.charAt(0) === '"') {
          if (fragment.indexOf('"', 1) >= 0) {
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
        completed = match[1] + (candidate.indexOf(" ") >= 0 ? '"' + candidate + '" ' : candidate + " ");
        return {
          value: completed,
          cursor: completed.length
        };
      };
      AvatarChatApp2.prototype.buildGenericUserTabCompletion = function(value, cursor) {
        var beforeCursor = value.substr(0, cursor);
        var afterCursor = value.substr(cursor);
        var match = beforeCursor.match(/(^|[\s])([^\s"]+)$/);
        var prefix = "";
        var fragment = "";
        var candidate = "";
        var completed = "";
        if (trimText(afterCursor).length || beforeCursor.length && beforeCursor.charAt(0) === "/" || !match) {
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
      };
      AvatarChatApp2.prototype.findAutocompleteCandidate = function(fragment, names) {
        var normalizedFragment = this.normalizePrivateLookupKey(fragment);
        var index = 0;
        if (!normalizedFragment.length) {
          return "";
        }
        for (index = 0; index < names.length; index += 1) {
          var candidate = names[index] || "";
          if (candidate.length && this.normalizePrivateLookupKey(candidate).indexOf(normalizedFragment) === 0) {
            return candidate;
          }
        }
        return "";
      };
      AvatarChatApp2.prototype.performAction = function(actionId) {
        switch (actionId) {
          case "who":
            this.openRosterModal();
            return;
          case "img":
            this.openBitmapViewer();
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
      };
      AvatarChatApp2.prototype.openRosterModal = function() {
        var entries = this.buildRosterEntries();
        var selectedIndex = 0;
        var index = 0;
        for (index = 0; index < entries.length; index += 1) {
          var entry = entries[index];
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
      };
      AvatarChatApp2.prototype.openChannelsModal = function() {
        var entries = this.buildChannelEntries();
        var selectedIndex = 0;
        var index = 0;
        for (index = 0; index < entries.length; index += 1) {
          var entry = entries[index];
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
      };
      AvatarChatApp2.prototype.openPrivateThreadsModal = function() {
        var entries = this.buildPrivateThreadEntries();
        var selectedIndex = 0;
        var index = 0;
        for (index = 0; index < entries.length; index += 1) {
          var entry = entries[index];
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
      };
      AvatarChatApp2.prototype.openHelpModal = function() {
        this.modalState = {
          kind: "help",
          title: "Help",
          selectedIndex: 0,
          lines: [
            "Slash commands:",
            "/who, /img, /channels, /private, /help, /join <channel>, /part [channel], /me <action>, /msg <user> <message>, /r <message>, /clear",
            "",
            "Keys:",
            "Tab autocompletes user names.",
            "Esc exits the chat or closes a modal.",
            "Arrow keys, Home/End, and Backspace edit the input line.",
            "Image viewer uses Up/Down to scroll, [ ] to switch images, and C to toggle 256/16 color.",
            "",
            "The top action bar is keyboard-first today and can grow mouse support later."
          ]
        };
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.closeModal = function() {
        this.modalState = null;
        this.destroyModalFrames();
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.getModalItemCount = function() {
        if (!this.modalState) {
          return 0;
        }
        if (this.modalState.kind === "help") {
          return 0;
        }
        return this.modalState.entries.length;
      };
      AvatarChatApp2.prototype.moveModalSelection = function(delta) {
        var itemCount = this.getModalItemCount();
        if (!this.modalState || this.modalState.kind === "help" || itemCount < 1) {
          return;
        }
        this.modalState.selectedIndex = clamp(this.modalState.selectedIndex + delta, 0, itemCount - 1);
      };
      AvatarChatApp2.prototype.jumpModalSelection = function(index) {
        var itemCount = this.getModalItemCount();
        if (!this.modalState || this.modalState.kind === "help" || itemCount < 1) {
          return;
        }
        this.modalState.selectedIndex = clamp(index, 0, itemCount - 1);
      };
      AvatarChatApp2.prototype.buildRosterEntries = function() {
        var entries = [];
        var seen = {};
        var channel = this.currentChannel.length ? this.getChannelByName(this.currentChannel) : null;
        var index = 0;
        this.addRosterEntry(entries, seen, user.alias, system.name, { name: user.alias, host: system.name, ip: user.ip_address, qwkid: system.qwk_id }, true);
        if (channel && channel.users) {
          for (index = 0; index < channel.users.length; index += 1) {
            var rawEntry = channel.users[index];
            var rosterEntry = this.extractRosterEntry(rawEntry);
            if (rosterEntry) {
              this.addRosterEntry(entries, seen, rosterEntry.name, rosterEntry.bbs, rosterEntry.nick, rosterEntry.isSelf);
            }
          }
        }
        entries.sort(function(left, right) {
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
      };
      AvatarChatApp2.prototype.addRosterEntry = function(entries, seen, name, bbs2, nick, isSelf) {
        var trimmedName = trimText(name);
        var trimmedBbs = trimText(bbs2 || system.name);
        var key = trimmedName.toUpperCase() + "|" + trimmedBbs.toUpperCase();
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
      };
      AvatarChatApp2.prototype.extractRosterEntry = function(rawEntry) {
        var name = "";
        var bbs2 = "";
        var nick = null;
        var nickValue = null;
        if (!rawEntry) {
          return null;
        }
        nickValue = rawEntry.nick;
        if (nickValue && typeof nickValue === "object") {
          name = trimText(String(nickValue.name || nickValue.alias || nickValue.user || ""));
          bbs2 = trimText(String(nickValue.host || rawEntry.system || rawEntry.host || rawEntry.bbs || system.name));
          nick = {
            name: name,
            host: bbs2,
            ip: nickValue.ip || rawEntry.ip || void 0,
            qwkid: nickValue.qwkid || rawEntry.qwkid || void 0,
            avatar: nickValue.avatar || void 0
          };
        } else {
          name = trimText(String(nickValue || rawEntry.name || rawEntry.alias || rawEntry.user || ""));
          bbs2 = trimText(String(rawEntry.system || rawEntry.host || rawEntry.bbs || system.name));
          if (name.length) {
            nick = {
              name: name,
              host: bbs2,
              qwkid: rawEntry.qwkid || void 0
            };
          }
        }
        if (nick && !nick.avatar && name.length) {
          var embedded = this.embeddedAvatars[name.toUpperCase()];
          if (embedded) {
            nick.avatar = embedded;
          }
        }
        if (!name.length) {
          return null;
        }
        if (bbs2 && bbs2.length) {
          this.userBbsCache[name.toUpperCase()] = bbs2;
        }
        return {
          name: name,
          bbs: bbs2 || "Unknown BBS",
          nick: nick,
          isSelf: name.toUpperCase() === user.alias.toUpperCase()
        };
      };
      AvatarChatApp2.prototype.enrichJoinLeaveNotice = function(message, channel) {
        if (!message || message.nick || !message.str)
          return;
        var text = message.str;
        var hereMatch = /^(.+) is here\.$/.exec(text);
        if (hereMatch) {
          var userName = hereMatch[1] || "";
          var bbsName = this.lookupUserBbs(userName, channel);
          if (bbsName) {
            message.str = userName + " is here from " + bbsName;
          }
          return;
        }
        var leftMatch = /^(.+) has left\.$/.exec(text);
        if (leftMatch) {
          var userName = leftMatch[1] || "";
          var bbsName = this.lookupUserBbs(userName, channel);
          if (bbsName) {
            message.str = userName + " from " + bbsName + " left.";
          }
          return;
        }
      };
      AvatarChatApp2.prototype.lookupUserBbs = function(userName, channel) {
        var upper = userName.toUpperCase();
        if (channel && channel.users) {
          for (var i = 0; i < channel.users.length; i += 1) {
            var entry = this.extractRosterEntry(channel.users[i]);
            if (entry && entry.name.toUpperCase() === upper) {
              return entry.bbs;
            }
          }
        }
        return this.userBbsCache[upper] || "";
      };
      AvatarChatApp2.prototype.buildChannelEntries = function() {
        var entries = [];
        var key = "";
        if (!this.chat) {
          return entries;
        }
        for (key in this.chat.channels) {
          if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
            var channel = this.chat.channels[key];
            var unreadCount = channel && channel.name ? this.publicChannelUnreadCounts[channel.name.toUpperCase()] || 0 : 0;
            if (!channel || !channel.name || !this.shouldIncludePublicChannel(channel.name)) {
              continue;
            }
            entries.push({
              name: channel.name,
              userCount: channel.users ? channel.users.length : 0,
              isCurrent: channel.name.toUpperCase() === this.currentChannel.toUpperCase(),
              metaText: unreadCount > 0 ? (channel.users ? String(channel.users.length) : "0") + " | new " + String(unreadCount) : channel.users ? String(channel.users.length) : "0"
            });
          }
        }
        entries.sort(function(left, right) {
          if (left.name.toUpperCase() < right.name.toUpperCase()) {
            return -1;
          }
          if (left.name.toUpperCase() > right.name.toUpperCase()) {
            return 1;
          }
          return 0;
        });
        return entries;
      };
      AvatarChatApp2.prototype.buildPrivateThreadEntries = function() {
        var entries = [];
        var index = 0;
        for (index = 0; index < this.privateThreadOrder.length; index += 1) {
          var threadId = this.privateThreadOrder[index];
          var thread = threadId ? this.privateThreads[threadId] : null;
          if (!thread) {
            continue;
          }
          entries.push({
            name: thread.name,
            userCount: thread.messages.length,
            isCurrent: thread.name.toUpperCase() === this.currentChannel.toUpperCase(),
            metaText: thread.unreadCount > 0 ? "new " + String(thread.unreadCount) : "pm"
          });
        }
        entries.sort(function(left, right) {
          var leftUnread = left.metaText && left.metaText.indexOf("new ") === 0 ? parseInt(left.metaText.substr(4), 10) || 0 : 0;
          var rightUnread = right.metaText && right.metaText.indexOf("new ") === 0 ? parseInt(right.metaText.substr(4), 10) || 0 : 0;
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
      };
      AvatarChatApp2.prototype.appendNotice = function(channelName, text) {
        var channel = this.getChannelByName(channelName);
        if (!channel) {
          this.lastError = text;
          return;
        }
        channel.messages.push(buildNoticeMessage(text));
        trimChannelMessages(channel, this.config.maxHistory);
        this.transcriptSignature = "";
      };
      AvatarChatApp2.prototype.appendViewNotice = function(viewName, text) {
        var privateThread = this.getPrivateThreadByName(viewName);
        if (privateThread) {
          privateThread.messages.push(buildNoticeMessage(text));
          trimChannelMessages(privateThread, this.config.maxHistory);
          this.transcriptSignature = "";
          return;
        }
        this.appendNotice(viewName, text);
      };
      AvatarChatApp2.prototype.scrollTranscriptOlder = function(step) {
        this.setTranscriptScrollOffset(this.transcriptScrollOffsetBlocks + Math.max(1, step));
      };
      AvatarChatApp2.prototype.scrollTranscriptNewer = function(step) {
        this.setTranscriptScrollOffset(this.transcriptScrollOffsetBlocks - Math.max(1, step));
      };
      AvatarChatApp2.prototype.pageTranscriptOlder = function() {
        this.scrollTranscriptOlder(Math.max(1, this.transcriptVisibleBlockCount - 1));
      };
      AvatarChatApp2.prototype.pageTranscriptNewer = function() {
        this.scrollTranscriptNewer(Math.max(1, this.transcriptVisibleBlockCount - 1));
      };
      AvatarChatApp2.prototype.scrollTranscriptToLatest = function() {
        this.setTranscriptScrollOffset(0);
      };
      AvatarChatApp2.prototype.scrollTranscriptToOldest = function() {
        this.setTranscriptScrollOffset(this.transcriptMaxScrollOffsetBlocks);
      };
      AvatarChatApp2.prototype.setTranscriptScrollOffset = function(nextOffset) {
        var clampedOffset = clamp(nextOffset, 0, this.transcriptMaxScrollOffsetBlocks);
        if (clampedOffset === this.transcriptScrollOffsetBlocks) {
          return;
        }
        this.transcriptScrollOffsetBlocks = clampedOffset;
        this.transcriptSignature = "";
        this.statusSignature = "";
      };
      AvatarChatApp2.prototype.isPrintable = function(key) {
        var code = key.charCodeAt(0);
        return key.length === 1 && code >= 32 && code !== 127;
      };
      AvatarChatApp2.prototype.cycleChannel = function() {
        var index = 0;
        if (this.channelOrder.length < 2) {
          return;
        }
        for (index = 0; index < this.channelOrder.length; index += 1) {
          var channelName = this.channelOrder[index];
          if (!channelName) {
            continue;
          }
          if (channelName.toUpperCase() === this.currentChannel.toUpperCase()) {
            this.currentChannel = this.channelOrder[(index + 1) % this.channelOrder.length] || this.getDefaultPublicChannel();
            this.markCurrentViewRead(this.currentChannel);
            this.scrollTranscriptToLatest();
            this.transcriptSignature = "";
            this.headerSignature = "";
            return;
          }
        }
        this.currentChannel = this.channelOrder[0] || this.getDefaultPublicChannel();
        this.markCurrentViewRead(this.currentChannel);
        this.scrollTranscriptToLatest();
      };
      AvatarChatApp2.prototype.syncChannelOrder = function() {
        var nextOrder = [];
        var key = "";
        var index = 0;
        if (!this.chat) {
          return;
        }
        for (index = 0; index < this.channelOrder.length; index += 1) {
          var channelName = this.channelOrder[index];
          var existing = channelName ? this.getChannelByName(channelName) : null;
          if (existing && this.shouldIncludePublicChannel(existing.name)) {
            nextOrder.push(existing.name);
          }
        }
        for (key in this.chat.channels) {
          if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
            var channel = this.chat.channels[key];
            if (channel && this.shouldIncludePublicChannel(channel.name) && !this.channelExists(nextOrder, channel.name)) {
              nextOrder.push(channel.name);
            }
          }
        }
        for (index = 0; index < this.privateThreadOrder.length; index += 1) {
          var threadId = this.privateThreadOrder[index];
          var thread = threadId ? this.privateThreads[threadId] : null;
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
        if (!this.currentChannel.length || !this.getChannelByName(this.currentChannel) && !this.getPrivateThreadByName(this.currentChannel)) {
          this.currentChannel = this.channelOrder[0] || "";
          this.markCurrentViewRead(this.currentChannel);
          this.scrollTranscriptToLatest();
        } else if (this.isMotdChannelName(this.currentChannel) && !this.canManageMotd()) {
          this.currentChannel = this.channelOrder[0] || this.getDefaultPublicChannel();
          this.markCurrentViewRead(this.currentChannel);
          this.scrollTranscriptToLatest();
        }
      };
      AvatarChatApp2.prototype.channelExists = function(channels, target) {
        var index = 0;
        for (index = 0; index < channels.length; index += 1) {
          var channelName = channels[index];
          if (channelName && channelName.toUpperCase() === target.toUpperCase()) {
            return true;
          }
        }
        return false;
      };
      AvatarChatApp2.prototype.trimHistories = function() {
        var key = "";
        if (!this.chat) {
          return;
        }
        for (key in this.chat.channels) {
          if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
            var channel = this.chat.channels[key];
            if (channel) {
              trimChannelMessages(channel, this.config.maxHistory);
            }
          }
        }
      };
      AvatarChatApp2.prototype.getChannelByName = function(name) {
        var key = "";
        var upper = name.toUpperCase();
        if (!this.chat) {
          return null;
        }
        for (key in this.chat.channels) {
          if (Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
            var channel = this.chat.channels[key];
            if (channel && channel.name.toUpperCase() === upper) {
              return channel;
            }
          }
        }
        return null;
      };
      AvatarChatApp2.prototype.ensureFrames = function() {
        var width = console.screen_columns;
        var height = console.screen_rows;
        var transcriptHeight = Math.max(1, height - 4);
        if (this.frames.root && this.frames.width === width && this.frames.height === height) {
          return;
        }
        this.destroyFrames();
        this.frames.root = new Frame(1, 1, width, height, BG_BLACK | LIGHTGRAY);
        this.frames.root.open();
        this.frames.header = new Frame(1, 1, width, 1, BG_GREEN | BLACK, this.frames.root);
        this.frames.header.open();
        this.frames.actions = new Frame(1, 2, width, 1, BG_BLUE | WHITE, this.frames.root);
        this.frames.actions.open();
        this.frames.animBg = new Frame(1, 3, width, transcriptHeight, BG_BLACK | LIGHTGRAY, this.frames.root);
        this.frames.animBg.transparent = true;
        this.frames.animBg.open();
        this.frames.transcript = new Frame(1, 3, width, transcriptHeight, BG_BLACK | LIGHTGRAY, this.frames.root);
        this.frames.transcript.transparent = true;
        this.frames.transcript.open();
        this.frames.animFg = new Frame(1, 3, width, transcriptHeight, BG_BLACK | LIGHTGRAY, this.frames.root);
        this.frames.animFg.transparent = true;
        this.frames.animFg.open();
        this.frames.animFg.top();
        this.frames.status = new Frame(1, transcriptHeight + 3, width, 1, BG_MAGENTA | WHITE, this.frames.root);
        this.frames.status.open();
        this.frames.input = new Frame(1, transcriptHeight + 4, width, 1, BG_BLACK | WHITE, this.frames.root);
        this.frames.input.open();
        this.frames.width = width;
        this.frames.height = height;
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.ensureModalFrames = function() {
        var geometry = this.getModalGeometry();
        if (!this.modalState || !this.frames.root || !geometry) {
          this.destroyModalFrames();
          return;
        }
        if (this.frames.overlay && this.frames.modal && this.frames.overlay.width === this.frames.width && this.frames.overlay.height === this.frames.height && this.frames.modal.width === geometry.width && this.frames.modal.height === geometry.height) {
          return;
        }
        this.destroyModalFrames();
        this.frames.overlay = new Frame(1, 1, this.frames.width, this.frames.height, BG_BLACK | DARKGRAY, this.frames.root);
        this.frames.overlay.open();
        this.frames.modal = new Frame(geometry.x, geometry.y, geometry.width, geometry.height, BG_BLACK | LIGHTGRAY, this.frames.overlay);
        this.frames.modal.open();
      };
      AvatarChatApp2.prototype.ensureBitmapViewerFrames = function() {
        if (!this.bitmapViewerActive || !this.frames.root) {
          this.destroyModalFrames();
          return;
        }
        if (this.frames.overlay && this.frames.modal && this.frames.overlay.width === this.frames.width && this.frames.overlay.height === this.frames.height && this.frames.modal.width === this.frames.width && this.frames.modal.height === this.frames.height) {
          return;
        }
        this.destroyModalFrames();
        this.frames.overlay = new Frame(1, 1, this.frames.width, this.frames.height, BG_BLACK | BLACK, this.frames.root);
        this.frames.overlay.open();
        this.frames.modal = new Frame(1, 1, this.frames.width, this.frames.height, BG_BLACK | BLACK, this.frames.overlay);
        this.frames.modal.open();
      };
      AvatarChatApp2.prototype.getModalGeometry = function() {
        var width = 0;
        var height = 0;
        var x = 0;
        var y = 0;
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
      };
      AvatarChatApp2.prototype.destroyModalFrames = function() {
        if (this.frames.modal) {
          this.frames.modal.close();
        }
        if (this.frames.overlay) {
          this.frames.overlay.close();
        }
        this.frames.modal = null;
        this.frames.overlay = null;
      };
      AvatarChatApp2.prototype.destroyFrames = function() {
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
      };
      AvatarChatApp2.prototype.collectChatNicks = function() {
        var nicks = [];
        var seen = {};
        var key = "";
        if (!this.chat) {
          return nicks;
        }
        for (key in this.chat.channels) {
          if (!Object.prototype.hasOwnProperty.call(this.chat.channels, key)) {
            continue;
          }
          var channel = this.chat.channels[key];
          if (!channel) {
            continue;
          }
          var msgIndex = 0;
          for (msgIndex = 0; msgIndex < channel.messages.length; msgIndex += 1) {
            var message = channel.messages[msgIndex];
            if (message && message.nick && message.nick.avatar) {
              var avatarStr = String(message.nick.avatar).replace(/^\s+|\s+$/g, "");
              if (avatarStr.length) {
                this.embeddedAvatars[message.nick.name.toUpperCase()] = avatarStr;
              }
            }
          }
          if (!channel.users) {
            continue;
          }
          var userIndex = 0;
          for (userIndex = 0; userIndex < channel.users.length; userIndex += 1) {
            var rosterEntry = this.extractRosterEntry(channel.users[userIndex]);
            if (!rosterEntry || !rosterEntry.nick || !rosterEntry.nick.name) {
              continue;
            }
            var nickKey = rosterEntry.nick.name.toUpperCase();
            if (seen[nickKey]) {
              continue;
            }
            seen[nickKey] = true;
            nicks.push(rosterEntry.nick);
          }
        }
        return nicks;
      };
      AvatarChatApp2.prototype.buildAnimOpts = function() {
        var _this = this;
        var cfg = this.config.idleAnimations;
        return {
          switch_interval: cfg.switchInterval,
          fps: cfg.fps,
          random: cfg.random,
          sequence: cfg.sequence,
          clear_on_switch: cfg.clearOnSwitch,
          debug: false,
          figlet_messages: cfg.figletMessages,
          figlet_refresh: cfg.figletRefresh,
          figlet_fonts: cfg.figletFonts || void 0,
          figlet_colors: cfg.figletColors,
          figlet_move: cfg.figletMove,
          use_avatar_frames: cfg.useAvatarFrames,
          avatar_lib: this.avatarLib || void 0,
          getChatNicks: function() {
            return _this.collectChatNicks();
          },
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
      };
      AvatarChatApp2.prototype.initIdleAnimManagers = function() {
        var cfg = this.config.idleAnimations;
        if (!cfg.enabled)
          return;
        var CA = js.global.CanvasAnimations;
        var AF = js.global.AvatarsFloat;
        if (!CA) {
          log("Avatar Chat: CanvasAnimations not loaded, idle animations disabled");
          return;
        }
        var disableSet = {};
        for (var i = 0; i < cfg.disable.length; i++) {
          var disabledName = cfg.disable[i];
          if (disabledName) {
            disableSet[disabledName] = true;
          }
        }
        var opts = this.buildAnimOpts();
        var animBg = this.frames.animBg;
        var animFg = this.frames.animFg;
        var fgSet = {};
        for (var i = 0; i < AvatarChatApp2.FOREGROUND_ANIMATIONS.length; i++) {
          var fgName = AvatarChatApp2.FOREGROUND_ANIMATIONS[i];
          if (fgName)
            fgSet[fgName] = true;
        }
        var self = this;
        opts.frameForAnim = function(name2, _defaultFrame) {
          if (fgSet[name2]) {
            if (animBg) {
              try {
                animBg.clear();
                animBg.invalidate();
              } catch (_e) {
              }
            }
            self.animFrame = "fg";
            return animFg || _defaultFrame;
          }
          if (animFg) {
            try {
              animFg.clear();
              animFg.invalidate();
            } catch (_e) {
            }
          }
          self.animFrame = "bg";
          return animBg || _defaultFrame;
        };
        var defaultFrame = animBg || animFg;
        if (!defaultFrame)
          return;
        this.animMgr = new CA.AnimationManager(defaultFrame, opts);
        for (var name in AvatarChatApp2.ANIMATION_MAP) {
          if (disableSet[name])
            continue;
          var ctorKey = AvatarChatApp2.ANIMATION_MAP[name];
          if (!ctorKey)
            continue;
          if (name === "avatars_float" && AF && AF.AvatarsFloat) {
            this.animMgr.add(name, AF.AvatarsFloat);
          } else if (CA[ctorKey]) {
            this.animMgr.add(name, CA[ctorKey]);
          }
        }
        this.idleTickInterval = Math.max(50, Math.floor(1e3 / Math.max(1, cfg.fps)));
      };
      AvatarChatApp2.prototype.startIdleAnimations = function() {
        if (this.idleAnimActive)
          return;
        if (!this.animMgr) {
          this.initIdleAnimManagers();
        }
        if (!this.animMgr)
          return;
        this.idleAnimActive = true;
        this.lastAnimTickAt = Date.now();
        try {
          this.animMgr.start();
        } catch (e) {
          log("Avatar Chat: animation start error: " + String(e));
        }
      };
      AvatarChatApp2.prototype.stopIdleAnimations = function() {
        if (!this.idleAnimActive)
          return;
        this.idleAnimActive = false;
        if (this.animMgr) {
          try {
            this.animMgr.dispose();
          } catch (_e) {
          }
        }
        if (this.frames.animBg) {
          try {
            this.frames.animBg.clear();
            this.frames.animBg.invalidate();
          } catch (_e) {
          }
        }
        if (this.frames.animFg) {
          try {
            this.frames.animFg.clear();
            this.frames.animFg.invalidate();
          } catch (_e) {
          }
        }
        if (this.frames.transcript) {
          try {
            this.frames.transcript.invalidate();
          } catch (_e) {
          }
        }
        this.resetRenderSignatures();
      };
      AvatarChatApp2.prototype.tickIdleAnimations = function() {
        var cfg = this.config.idleAnimations;
        if (!cfg.enabled)
          return;
        var now = Date.now();
        var elapsed = now - this.lastKeyTimestamp;
        if (!this.idleAnimActive) {
          if (elapsed >= cfg.idleTimeoutSeconds * 1e3) {
            this.startIdleAnimations();
          }
          return;
        }
        if (now - this.lastAnimTickAt < this.idleTickInterval) {
          return;
        }
        this.lastAnimTickAt = now;
        var switchNow = this.animMgr && this.animMgr.lastSwitch && time() - this.animMgr.lastSwitch >= cfg.switchInterval;
        if (this.animMgr) {
          try {
            if (switchNow)
              this.animMgr.start();
            this.animMgr.tick();
          } catch (e) {
            log("Avatar Chat: animation tick error: " + String(e));
          }
        }
      };
      AvatarChatApp2.prototype.render = function() {
        this.ensureFrames();
        if (this.bitmapViewerActive) {
          this.renderBitmapViewer();
          return;
        }
        this.renderHeader();
        this.renderActions();
        this.renderTranscript();
        this.renderStatus();
        this.renderInput();
        this.renderActiveModal();
        if (this.frames.root) {
          this.frames.root.cycle();
        }
      };
      AvatarChatApp2.prototype.renderHeader = function() {
        var headerFrame = this.frames.header;
        var channel = this.currentChannel.length ? this.getChannelByName(this.currentChannel) : null;
        var privateThread = this.currentChannel.length ? this.getPrivateThreadByName(this.currentChannel) : null;
        var users = channel && channel.users ? channel.users.length : 0;
        var infoText = "";
        var text = "";
        var headerAttr = BG_GREEN | BLACK;
        var headerState = "";
        if (privateThread) {
          infoText = clipText(" Avatar Chat | pm " + privateThread.peerNick.name + " | messages " + String(privateThread.messages.length) + " | joined " + String(this.channelOrder.length) + " | " + this.config.host + ":" + String(this.config.port), this.frames.width);
        } else {
          infoText = clipText(" Avatar Chat | " + (this.currentChannel || "offline") + " | users " + String(users) + " | joined " + String(this.channelOrder.length) + " | " + this.config.host + ":" + String(this.config.port), this.frames.width);
        }
        headerState = this.renderHeaderMode(infoText, this.frames.width, this.motdText.length ? " MOTD | " + this.motdText : "");
        text = headerState.substr(5);
        headerAttr = headerState.indexOf("motd|") === 0 ? BG_GREEN | WHITE : BG_GREEN | BLACK;
        if (!headerFrame) {
          return;
        }
        if (headerState === this.headerSignature) {
          return;
        }
        headerFrame.clear(headerAttr);
        headerFrame.gotoxy(1, 1);
        headerFrame.putmsg(padRight(text, headerFrame.width), headerAttr);
        this.headerSignature = headerState;
      };
      AvatarChatApp2.prototype.renderHeaderMode = function(infoText, width, motdText) {
        var normalizedMotd = trimText(motdText);
        var statusText = clipText(infoText, width);
        var now = (/* @__PURE__ */ new Date()).getTime();
        var statusDuration = 4500;
        var motdStartPause = 1250;
        var motdEndPause = 900;
        var motdStepMs = 250;
        var motdBuffer = normalizedMotd + "   ";
        var maxOffset = Math.max(0, motdBuffer.length - width);
        var motdDuration = normalizedMotd.length ? maxOffset > 0 ? motdStartPause + (maxOffset + 1) * motdStepMs + motdEndPause : 5e3 : 0;
        var totalDuration = statusDuration + motdDuration;
        var motdElapsed = 0;
        var scrollOffset = 0;
        var tickerText = "";
        if (!normalizedMotd.length || width < 12 || totalDuration <= 0) {
          return "info|" + statusText;
        }
        if (now % totalDuration < statusDuration) {
          return "info|" + statusText;
        }
        motdElapsed = now % totalDuration - statusDuration;
        if (maxOffset <= 0) {
          return "motd|" + clipText(normalizedMotd, width);
        }
        if (motdElapsed <= motdStartPause) {
          scrollOffset = 0;
        } else if (motdElapsed >= motdStartPause + (maxOffset + 1) * motdStepMs) {
          scrollOffset = maxOffset;
        } else {
          scrollOffset = Math.floor((motdElapsed - motdStartPause) / motdStepMs);
          if (scrollOffset > maxOffset) {
            scrollOffset = maxOffset;
          }
        }
        tickerText = motdBuffer.substr(scrollOffset, width);
        return "motd|" + padRight(tickerText, width);
      };
      AvatarChatApp2.prototype.renderActions = function() {
        var actionsFrame = this.frames.actions;
        var unreadPmCount = this.getUnreadPrivateThreadCount();
        var imageCount = this.bitmapQueue.length;
        var unviewedImageCount = this.unviewedBitmapCount;
        var flashPhase = unreadPmCount > 0 ? Math.floor((/* @__PURE__ */ new Date()).getTime() / 500) % 2 : 0;
        var imageFlashPhase = unviewedImageCount > 0 ? Math.floor((/* @__PURE__ */ new Date()).getTime() / 500) % 2 : 0;
        var actions = ACTION_BAR_ACTIONS.map(function(action) {
          var nextAction = {
            id: action.id,
            label: action.label
          };
          if (action.id === "img" && imageCount > 0) {
            nextAction.label = "/img [" + String(imageCount) + "]";
            if (unviewedImageCount > 0) {
              nextAction.attr = imageFlashPhase ? BG_RED | YELLOW : BG_CYAN | BLACK;
            }
          }
          if (action.id === "private" && unreadPmCount > 0) {
            nextAction.label = "/private [" + String(unreadPmCount) + "]";
            nextAction.attr = flashPhase ? BG_RED | YELLOW : BG_CYAN | BLACK;
          }
          return nextAction;
        });
        var signature = String(this.frames.width) + "|" + actions.map(function(action) {
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
      };
      AvatarChatApp2.prototype.renderTranscript = function() {
        var transcriptFrame = this.frames.transcript;
        var channel = this.currentChannel.length ? this.getChannelByName(this.currentChannel) : null;
        var privateThread = this.currentChannel.length ? this.getPrivateThreadByName(this.currentChannel) : null;
        var rawMessages = channel ? channel.messages : privateThread ? privateThread.messages : [];
        var messages = this.getRenderableMessages(rawMessages);
        var signature = this.buildTranscriptSignature(messages);
        var renderState;
        var emptyText = "";
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
          emptyText = this.hasImagesForView(this.currentChannel) ? "No text messages yet. Use /img to view images." : "No private messages yet.";
        } else {
          emptyText = this.hasImagesForView(this.currentChannel) ? "No text messages yet. Use /img to view images." : "No messages yet.";
        }
        renderState = renderTranscript(transcriptFrame, messages, {
          ownAlias: user.alias,
          ownUserNumber: user.number,
          avatarLib: this.avatarLib,
          avatarCache: this.avatarCache,
          emptyText: emptyText,
          scrollOffsetBlocks: this.transcriptScrollOffsetBlocks
        });
        this.transcriptVisibleBlockCount = renderState.visibleBlockCount;
        this.transcriptMaxScrollOffsetBlocks = renderState.maxScrollOffsetBlocks;
        this.transcriptScrollOffsetBlocks = renderState.actualScrollOffsetBlocks;
        this.transcriptSignature = signature;
      };
      AvatarChatApp2.prototype.renderStatus = function() {
        var statusFrame = this.frames.status;
        var text = clipText(this.buildStatusText(), this.frames.width);
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
      };
      AvatarChatApp2.prototype.renderInput = function() {
        var inputFrame = this.frames.input;
        var viewportWidth = Math.max(1, this.frames.width - 2);
        var viewport = this.inputBuffer.getViewport(viewportWidth);
        var signature = this.inputBuffer.getValue() + "|" + String(viewport.cursorColumn) + "|" + String(this.frames.width);
        var cursorX = 0;
        var cursorChar = " ";
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
      };
      AvatarChatApp2.prototype.color256To16 = function(color) {
        var normalized = color;
        var gray = 0;
        var index = 0;
        var red = 0;
        var green = 0;
        var blue = 0;
        var bright = 0;
        if (normalized < 0) {
          normalized = 0;
        }
        if (normalized < 16) {
          return normalized;
        }
        if (normalized >= 232) {
          gray = normalized - 232;
          if (gray < 6) {
            return 0;
          }
          if (gray < 12) {
            return 8;
          }
          if (gray < 18) {
            return 7;
          }
          return 15;
        }
        index = normalized - 16;
        red = Math.floor(index / 36);
        green = Math.floor(index % 36 / 6);
        blue = index % 6;
        bright = red > 2 || green > 2 || blue > 2 ? 8 : 0;
        return bright | (red > 1 ? 4 : 0) | (green > 1 ? 2 : 0) | (blue > 1 ? 1 : 0);
      };
      AvatarChatApp2.prototype.sanitizeBitmapChar = function(cell, fg, bg) {
        var ch = cell.ch || " ";
        if (cell.charCode === 7 || cell.charCode === 8 || cell.charCode === 9 || cell.charCode === 10 || cell.charCode === 11 || cell.charCode === 12 || cell.charCode === 13 || cell.charCode === 27) {
          return " ";
        }
        if (fg === bg && (cell.charCode === 220 || cell.charCode === 223)) {
          try {
            ch = ascii(219);
          } catch (_error) {
            ch = String.fromCharCode(219);
          }
        }
        return ch;
      };
      AvatarChatApp2.prototype.buildBitmapViewerSignature = function() {
        var entry = this.bitmapViewerIndex >= 0 ? this.bitmapQueue[this.bitmapViewerIndex] || null : null;
        if (!this.bitmapViewerActive || !entry) {
          return "";
        }
        return [
          String(this.frames.width),
          String(this.frames.height),
          String(this.bitmapQueue.length),
          String(this.bitmapViewerIndex),
          String(this.bitmapViewerScrollY),
          String(this.bitmapViewerUse256Color),
          String(entry.time || 0),
          String(entry.width),
          String(entry.height),
          String(entry.isIncomingPrivate ? Math.floor((/* @__PURE__ */ new Date()).getTime() / 6e4) : 0)
        ].join("|");
      };
      AvatarChatApp2.prototype.renderBitmapViewer = function() {
        var signature = this.buildBitmapViewerSignature();
        var entry = this.bitmapViewerIndex >= 0 ? this.bitmapQueue[this.bitmapViewerIndex] || null : null;
        var modalFrame = this.frames.modal;
        var status = "";
        var contentHeight = 0;
        var maxScrollY = 0;
        var row = 0;
        if (!this.bitmapViewerActive) {
          this.destroyModalFrames();
          return;
        }
        if (!entry) {
          this.closeBitmapViewer();
          return;
        }
        this.ensureBitmapViewerFrames();
        modalFrame = this.frames.modal;
        if (!this.frames.overlay || !modalFrame) {
          return;
        }
        maxScrollY = this.getBitmapViewerMaxScrollY();
        if (this.bitmapViewerScrollY > maxScrollY) {
          this.bitmapViewerScrollY = maxScrollY;
        }
        entry = this.bitmapQueue[this.bitmapViewerIndex] || null;
        if (!entry) {
          this.closeBitmapViewer();
          return;
        }
        if (signature === this.bitmapViewerSignature) {
          return;
        }
        contentHeight = Math.max(1, modalFrame.height - 2);
        status = "";
        if (this.bitmapQueue.length > 1) {
          status += "[ ] " + String(this.bitmapViewerIndex + 1) + "/" + String(this.bitmapQueue.length) + " | ";
        }
        if (maxScrollY > 0) {
          status += "Up/Down scroll | ";
        }
        status += "C " + (this.bitmapViewerUse256Color ? "256c" : "16c") + " | Esc close | " + String(entry.width) + "x" + String(entry.height);
        if (maxScrollY > 0) {
          status += " | y " + String(this.bitmapViewerScrollY) + "/" + String(maxScrollY);
        }
        status = clipText(status, modalFrame.width);
        this.frames.overlay.clear(BG_BLACK | BLACK);
        modalFrame.clear(BG_BLACK | BLACK);
        this.renderBitmapViewerTitle(modalFrame, entry);
        modalFrame.gotoxy(1, modalFrame.height);
        modalFrame.putmsg(padRight(status, modalFrame.width), BG_BLACK | LIGHTCYAN);
        if (!this.bitmapViewerUse256Color) {
          var displayWidth = modalFrame.width;
          for (row = 0; row < contentHeight; row += 1) {
            var srcY = row + this.bitmapViewerScrollY;
            var column = 0;
            if (srcY >= entry.height) {
              continue;
            }
            for (column = 0; column < displayWidth && column < entry.width; column += 1) {
              var cell = entry.bitmap[srcY * entry.width + column];
              var fg16 = 7;
              var bg16 = 0;
              var attr = WHITE;
              var ch = " ";
              if (!cell) {
                continue;
              }
              fg16 = this.color256To16(cell.fg);
              bg16 = this.color256To16(cell.bg);
              ch = this.sanitizeBitmapChar(cell, fg16, bg16);
              attr = (bg16 & 7) << 4 | fg16 & 15;
              modalFrame.setData(column, row + 1, ch, attr, false);
            }
          }
        }
        if (this.frames.root) {
          this.frames.root.cycle();
        }
        if (this.bitmapViewerUse256Color) {
          var displayWidth = modalFrame.width;
          for (row = 0; row < contentHeight; row += 1) {
            var srcY = row + this.bitmapViewerScrollY;
            var line = "";
            var lastFg = -1;
            var lastBg = -1;
            var column = 0;
            if (srcY >= entry.height) {
              continue;
            }
            console.gotoxy(1, row + 2);
            for (column = 0; column < displayWidth && column < entry.width; column += 1) {
              var cell = entry.bitmap[srcY * entry.width + column];
              var fg = 7;
              var bg = 0;
              var ch = " ";
              if (!cell) {
                continue;
              }
              fg = cell.fg;
              bg = cell.bg;
              ch = this.sanitizeBitmapChar(cell, fg, bg);
              if (fg !== lastFg || bg !== lastBg) {
                line += "\x1B[38;5;" + String(fg) + "m\x1B[48;5;" + String(bg) + "m";
                lastFg = fg;
                lastBg = bg;
              }
              line += ch;
            }
            line += "\x1B[0m";
            console.print(line);
          }
          console.gotoxy(1, modalFrame.height);
        }
        this.bitmapViewerSignature = this.buildBitmapViewerSignature();
      };
      AvatarChatApp2.prototype.renderActiveModal = function() {
        var signature = this.buildModalSignature();
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
        renderModal(this.frames.overlay, this.frames.modal, this.modalState, {
          ownAlias: user.alias,
          ownUserNumber: user.number,
          avatarLib: this.avatarLib,
          avatarCache: this.avatarCache
        });
        this.modalSignature = signature;
      };
      AvatarChatApp2.prototype.buildTranscriptSignature = function(messages) {
        var lastMessage = "";
        var lastTime = 0;
        if (messages.length) {
          var message = messages[messages.length - 1];
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
      };
      AvatarChatApp2.prototype.buildStatusText = function() {
        var unreadPmCount = this.getUnreadPrivateThreadCount();
        var imageHint = this.buildBitmapStatusHint();
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
          return this.buildDisconnectedText() + " | /connect | Esc exit" + imageHint;
        }
        if (this.transcriptScrollOffsetBlocks > 0) {
          return "History " + String(this.transcriptScrollOffsetBlocks) + " back | Up/PgUp older | Down/PgDn newer | End latest" + imageHint;
        }
        if (this.getPrivateThreadByName(this.currentChannel)) {
          return "Private chat | /img | /msg <user> <message> | /r <message> | /private | /channels | Tab user | Esc exit" + imageHint;
        }
        if (unreadPmCount > 0) {
          return "Private unread " + String(unreadPmCount) + " | /img | /private | /msg <user> <message> | /r <message> | Tab user | Esc exit" + imageHint;
        }
        return "Up/PgUp history | /who /img /channels /private /help | /join /part /me /msg /r /clear | Tab user | Esc exit" + imageHint;
      };
      AvatarChatApp2.prototype.buildBitmapStatusHint = function() {
        if (this.unviewedBitmapCount > 0) {
          return " | new img " + String(this.unviewedBitmapCount) + " /img";
        }
        if (this.bitmapQueue.length > 0) {
          return " | /img " + String(this.bitmapQueue.length);
        }
        return "";
      };
      AvatarChatApp2.prototype.getUnreadPrivateThreadCount = function() {
        var total = 0;
        var index = 0;
        for (index = 0; index < this.privateThreadOrder.length; index += 1) {
          var threadId = this.privateThreadOrder[index];
          var thread = threadId ? this.privateThreads[threadId] : null;
          if (thread) {
            total += thread.unreadCount;
          }
        }
        return total;
      };
      AvatarChatApp2.prototype.markCurrentViewRead = function(viewName) {
        this.markPrivateThreadRead(viewName);
        this.markPublicChannelRead(viewName);
      };
      AvatarChatApp2.prototype.markPublicChannelRead = function(viewName) {
        var normalizedName = trimText(viewName).toUpperCase();
        if (!normalizedName.length || normalizedName.charAt(0) === "@") {
          return;
        }
        if (!this.publicChannelUnreadCounts[normalizedName]) {
          return;
        }
        this.publicChannelUnreadCounts[normalizedName] = 0;
        this.modalSignature = "";
      };
      AvatarChatApp2.prototype.markPrivateThreadRead = function(viewName) {
        var thread = this.getPrivateThreadByName(viewName);
        if (!thread || thread.unreadCount === 0) {
          return;
        }
        thread.unreadCount = 0;
        this.headerSignature = "";
        this.statusSignature = "";
        this.actionSignature = "";
      };
      AvatarChatApp2.prototype.buildDisconnectedText = function() {
        var seconds = 0;
        if (this.reconnectAt) {
          seconds = Math.ceil((this.reconnectAt - (/* @__PURE__ */ new Date()).getTime()) / 1e3);
          if (seconds < 0) {
            seconds = 0;
          }
        }
        if (this.lastError.length) {
          return clipText(this.lastError, Math.max(10, this.frames.width - 16)) + " | retry " + String(seconds) + "s";
        }
        return "Disconnected";
      };
      AvatarChatApp2.prototype.resetRenderSignatures = function() {
        this.transcriptSignature = "";
        this.headerSignature = "";
        this.actionSignature = "";
        this.statusSignature = "";
        this.inputSignature = "";
        this.modalSignature = "";
        this.bitmapViewerSignature = "";
      };
      AvatarChatApp2.prototype.buildModalSignature = function() {
        var parts = [];
        var index = 0;
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
          var entry = this.modalState.entries[index];
          if (!entry) {
            continue;
          }
          if (this.modalState.kind === "roster") {
            var rosterEntry = entry;
            parts.push(rosterEntry.name + "@" + rosterEntry.bbs + ":" + String(rosterEntry.isSelf));
            continue;
          }
          var channelEntry = entry;
          parts.push(channelEntry.name + ":" + String(channelEntry.userCount) + ":" + String(channelEntry.isCurrent) + ":" + String(channelEntry.metaText || ""));
        }
        return parts.join("|");
      };
      AvatarChatApp2.FOREGROUND_ANIMATIONS = ["figlet_message", "avatars_float"];
      AvatarChatApp2.ANIMATION_MAP = {
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
      return AvatarChatApp2;
    }()
  );

  // repo/xtrn/avatar_chat/build/io/config.js
  var DEFAULT_IDLE = {
    enabled: true,
    idleTimeoutSeconds: 180,
    switchInterval: 60,
    fps: 4,
    random: true,
    sequence: [],
    disable: [],
    clearOnSwitch: true,
    figletMessages: "Avatar Chat",
    figletRefresh: 180,
    figletFonts: "",
    figletColors: true,
    figletMove: true,
    useAvatarFrames: true,
    starCount: 180,
    auroraSpeed: 0.12,
    auroraWave: 0.35,
    matrixSparse: 4,
    plasmaSpeed: 0.18,
    plasmaScale: 0.12,
    tunnelSpeed: 0.22,
    tunnelScale: 0.17,
    lissajousSpeed: 0.12,
    fireDecay: 1,
    rippleCount: 4
  };
  var DEFAULT_CONFIG = {
    host: "127.0.0.1",
    port: 10088,
    defaultChannel: "main",
    motdChannel: "motd",
    motdHostSystem: "",
    motdHostQwkid: "",
    maxHistory: 200,
    pollDelayMs: 25,
    reconnectDelayMs: 3e3,
    inputMaxLength: 500,
    idleAnimations: DEFAULT_IDLE
  };
  function readString(file, key, defaultValue) {
    var value = file.iniGetValue(null, key, defaultValue);
    if (value === void 0 || value === null) {
      return defaultValue;
    }
    return String(value);
  }
  function readNumber(file, key, defaultValue) {
    var value = file.iniGetValue(null, key, defaultValue);
    var parsed = parseInt(String(value), 10);
    if (isNaN(parsed)) {
      return defaultValue;
    }
    return parsed;
  }
  function readFloat(file, key, defaultValue) {
    var value = file.iniGetValue(null, key, defaultValue);
    var parsed = parseFloat(String(value));
    if (isNaN(parsed)) {
      return defaultValue;
    }
    return parsed;
  }
  function readBool(file, key, defaultValue) {
    var value = file.iniGetValue(null, key, defaultValue);
    if (value === void 0 || value === null) {
      return defaultValue;
    }
    var s = String(value).toLowerCase().trim();
    if (s === "true" || s === "1" || s === "yes") {
      return true;
    }
    if (s === "false" || s === "0" || s === "no") {
      return false;
    }
    return defaultValue;
  }
  function readList(file, key, defaultValue) {
    var value = file.iniGetValue(null, key, "");
    if (value === void 0 || value === null || String(value).trim() === "") {
      return defaultValue;
    }
    return String(value).split(",").map(function(s) {
      return s.trim();
    }).filter(Boolean);
  }
  function loadIdleAnimConfig(file) {
    return {
      enabled: readBool(file, "idle_enabled", DEFAULT_IDLE.enabled),
      idleTimeoutSeconds: readNumber(file, "idle_timeout_seconds", DEFAULT_IDLE.idleTimeoutSeconds),
      switchInterval: readNumber(file, "idle_switch_interval", DEFAULT_IDLE.switchInterval),
      fps: readNumber(file, "idle_fps", DEFAULT_IDLE.fps),
      random: readBool(file, "idle_random", DEFAULT_IDLE.random),
      sequence: readList(file, "idle_sequence", DEFAULT_IDLE.sequence),
      disable: readList(file, "idle_disable", DEFAULT_IDLE.disable),
      clearOnSwitch: readBool(file, "idle_clear_on_switch", DEFAULT_IDLE.clearOnSwitch),
      figletMessages: readString(file, "idle_figlet_messages", DEFAULT_IDLE.figletMessages),
      figletRefresh: readNumber(file, "idle_figlet_refresh", DEFAULT_IDLE.figletRefresh),
      figletFonts: readString(file, "idle_figlet_fonts", DEFAULT_IDLE.figletFonts),
      figletColors: readBool(file, "idle_figlet_colors", DEFAULT_IDLE.figletColors),
      figletMove: readBool(file, "idle_figlet_move", DEFAULT_IDLE.figletMove),
      useAvatarFrames: readBool(file, "idle_use_avatar_frames", DEFAULT_IDLE.useAvatarFrames),
      starCount: readNumber(file, "idle_star_count", DEFAULT_IDLE.starCount),
      auroraSpeed: readFloat(file, "idle_aurora_speed", DEFAULT_IDLE.auroraSpeed),
      auroraWave: readFloat(file, "idle_aurora_wave", DEFAULT_IDLE.auroraWave),
      matrixSparse: readNumber(file, "idle_matrix_sparse", DEFAULT_IDLE.matrixSparse),
      plasmaSpeed: readFloat(file, "idle_plasma_speed", DEFAULT_IDLE.plasmaSpeed),
      plasmaScale: readFloat(file, "idle_plasma_scale", DEFAULT_IDLE.plasmaScale),
      tunnelSpeed: readFloat(file, "idle_tunnel_speed", DEFAULT_IDLE.tunnelSpeed),
      tunnelScale: readFloat(file, "idle_tunnel_scale", DEFAULT_IDLE.tunnelScale),
      lissajousSpeed: readFloat(file, "idle_lissajous_speed", DEFAULT_IDLE.lissajousSpeed),
      fireDecay: readNumber(file, "idle_fire_decay", DEFAULT_IDLE.fireDecay),
      rippleCount: readNumber(file, "idle_ripple_count", DEFAULT_IDLE.rippleCount)
    };
  }
  function loadConfig() {
    var configPath = js.exec_dir + "avatar_chat.ini";
    var file = new File(configPath);
    var config = {
      host: DEFAULT_CONFIG.host,
      port: DEFAULT_CONFIG.port,
      defaultChannel: DEFAULT_CONFIG.defaultChannel,
      motdChannel: DEFAULT_CONFIG.motdChannel,
      motdHostSystem: DEFAULT_CONFIG.motdHostSystem,
      motdHostQwkid: DEFAULT_CONFIG.motdHostQwkid,
      maxHistory: DEFAULT_CONFIG.maxHistory,
      pollDelayMs: DEFAULT_CONFIG.pollDelayMs,
      reconnectDelayMs: DEFAULT_CONFIG.reconnectDelayMs,
      inputMaxLength: DEFAULT_CONFIG.inputMaxLength,
      idleAnimations: DEFAULT_IDLE
    };
    if (!file.open("r")) {
      return config;
    }
    config.host = readString(file, "host", DEFAULT_CONFIG.host);
    config.port = readNumber(file, "port", DEFAULT_CONFIG.port);
    config.defaultChannel = readString(file, "default_channel", DEFAULT_CONFIG.defaultChannel);
    config.motdChannel = readString(file, "motd_channel", DEFAULT_CONFIG.motdChannel);
    config.motdHostSystem = readString(file, "motd_host_system", DEFAULT_CONFIG.motdHostSystem);
    config.motdHostQwkid = readString(file, "motd_host_qwkid", DEFAULT_CONFIG.motdHostQwkid);
    config.maxHistory = readNumber(file, "max_history", DEFAULT_CONFIG.maxHistory);
    config.pollDelayMs = readNumber(file, "poll_delay_ms", DEFAULT_CONFIG.pollDelayMs);
    config.reconnectDelayMs = readNumber(file, "reconnect_delay_ms", DEFAULT_CONFIG.reconnectDelayMs);
    config.inputMaxLength = readNumber(file, "input_max_length", DEFAULT_CONFIG.inputMaxLength);
    config.idleAnimations = loadIdleAnimConfig(file);
    file.close();
    if (config.port < 1) {
      config.port = DEFAULT_CONFIG.port;
    }
    if (config.maxHistory < 10) {
      config.maxHistory = DEFAULT_CONFIG.maxHistory;
    }
    if (config.pollDelayMs < 5) {
      config.pollDelayMs = DEFAULT_CONFIG.pollDelayMs;
    }
    if (config.reconnectDelayMs < 250) {
      config.reconnectDelayMs = DEFAULT_CONFIG.reconnectDelayMs;
    }
    if (config.inputMaxLength < 64) {
      config.inputMaxLength = DEFAULT_CONFIG.inputMaxLength;
    }
    return config;
  }

  // repo/xtrn/avatar_chat/build/main.js
  function loadIdleAnimationModules() {
    try {
      js.global.CanvasAnimations = load(js.exec_dir + "lib/canvas-animations.js");
    } catch (error) {
      log("Avatar Chat: canvas-animations.js unavailable: " + String(error));
    }
    try {
      js.global.AvatarsFloat = load(js.exec_dir + "lib/avatars-float.js");
    } catch (error) {
      log("Avatar Chat: avatars-float.js unavailable: " + String(error));
    }
  }
  function main() {
    loadIdleAnimationModules();
    var app = new AvatarChatApp(loadConfig());
    try {
      app.run();
    } catch (error) {
      log("Avatar Chat fatal error: " + String(error));
      console.clear(BG_BLACK | LIGHTGRAY);
      console.home();
      console.writeln("Avatar Chat error:");
      console.writeln(String(error));
    }
  }
  main();
})();
