export function clamp(value: number, minimum: number, maximum: number): number {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

export function repeatChar(ch: string, count: number): string {
  let output = "";

  while (output.length < count) {
    output += ch;
  }

  if (output.length > count) {
    output = output.substr(0, count);
  }

  return output;
}

export function padRight(text: string, width: number): string {
  if (text.length >= width) {
    return text.substr(0, width);
  }
  return text + repeatChar(" ", width - text.length);
}

export function padLeft(text: string, width: number): string {
  if (text.length >= width) {
    return text.substr(text.length - width);
  }
  return repeatChar(" ", width - text.length) + text;
}

export function clipText(text: string, width: number): string {
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

export function centerText(text: string, width: number): string {
  let leftPadding;

  text = clipText(text, width);
  if (text.length >= width) {
    return text;
  }

  leftPadding = Math.floor((width - text.length) / 2);
  return repeatChar(" ", leftPadding) + text;
}

export function trimText(text: string): string {
  return text.replace(/^\s+|\s+$/g, "");
}

export function wrapText(text: string, width: number): string[] {
  const lines: string[] = [];
  const words = trimText(text).split(/\s+/);
  let current = "";
  let index = 0;

  if (width <= 1) {
    return [clipText(text, 1)];
  }

  if (!trimText(text).length) {
    return [""];
  }

  for (index = 0; index < words.length; index += 1) {
    let word = words[index] || "";

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

export function formatClockTime(timestamp: number): string {
  const date = new Date(timestamp);
  let hours = date.getHours();
  const minutes = date.getMinutes();
  const isPm = hours >= 12;
  let hourText = "";
  let minuteText = "";

  hours = hours % 12;
  if (hours === 0) {
    hours = 12;
  }

  hourText = String(hours);
  minuteText = minutes < 10 ? "0" + String(minutes) : String(minutes);

  return hourText + ":" + minuteText + (isPm ? " pm" : " am");
}



/**
 * Format a timestamp relative to now, matching the future_shell chat style.
 * Shows "just now", "5 minutes ago", "2 hours 30 minutes ago" for same-day,
 * "Today 3:05 pm" for first message of the day, or "MM/DD/YY H:MM am/pm" for older.
 * @param currentMs  Epoch milliseconds of this message
 * @param previousMs Epoch milliseconds of the previous group (0 or undefined for first)
 */
export function formatRelativeTime(currentMs: number, previousMs?: number): string {
  if (!currentMs || isNaN(currentMs)) return "";

  const now = new Date();
  const cur = new Date(currentMs);
  const todayY = now.getFullYear();
  const todayM = now.getMonth();
  const todayD = now.getDate();
  const curY = cur.getFullYear();
  const curM = cur.getMonth();
  const curD = cur.getDate();
  const curH = cur.getHours();
  const curMin = cur.getMinutes();
  const curIsToday = (curY === todayY && curM === todayM && curD === todayD);
  const pad2 = (n: number): string => n < 10 ? "0" + String(n) : String(n);
  const h12 = (h: number): number => h % 12 === 0 ? 12 : h % 12;
  const amPm = (h: number): string => h < 12 ? "am" : "pm";
  const curTimeStr = String(h12(curH)) + ":" + pad2(curMin) + " " + amPm(curH);
  const curDateStr = pad2(curM + 1) + "/" + pad2(curD) + "/" + String(curY).substr(2);

  if (!previousMs) {
    return curIsToday ? "Today " + curTimeStr : curDateStr + " " + curTimeStr;
  }

  const last = new Date(previousMs);
  const lastY = last.getFullYear();
  const lastM = last.getMonth();
  const lastD = last.getDate();

  // Same day as previous message
  if (curY === lastY && curM === lastM && curD === lastD) {
    if (curIsToday) {
      // Relative time from now
      const diffMs = now.getTime() - cur.getTime();
      const totalMin = Math.floor(diffMs / 60000);
      const diffHr = Math.floor(totalMin / 60);
      const diffMin = totalMin % 60;

      if (diffHr > 0) {
        let s = String(diffHr) + (diffHr === 1 ? " hour" : " hours");
        if (diffMin > 0) s += " " + String(diffMin) + (diffMin === 1 ? " minute" : " minutes");
        return s + " ago";
      }
      if (diffMin > 0) {
        return String(diffMin) + (diffMin === 1 ? " minute" : " minutes") + " ago";
      }
      return "just now";
    }
    // Same day but not today — just show time
    return curTimeStr;
  }

  // Current is today, previous was not
  if (curIsToday) {
    return "Today " + curTimeStr;
  }

  // Different days
  return curDateStr + " " + curTimeStr;
}

/**
 * Compact a relative timestamp for narrow displays.
 * "2 hours 30 minutes ago" -> "2h 30m ago", "just now" -> "now"
 */
export function compactTimestamp(text: string): string {
  if (!text) return "";
  let s = text;
  s = s.replace(/hours?/g, "h");
  s = s.replace(/minutes?/g, "m");
  s = s.replace(/seconds?/g, "s");
  s = s.replace(/Today\s*/i, "");
  s = s.replace(/Yesterday\s*/i, "yday ");
  s = s.replace(/\s+/g, " ").replace(/^\s+|\s+$/g, "");
  if (s === "just now") s = "now";
  return s;
}

export function splitWords(text: string): string[] {
  const trimmed = trimText(text);
  if (!trimmed.length) {
    return [];
  }
  return trimmed.split(/\s+/);
}
