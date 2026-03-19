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

export function splitWords(text: string): string[] {
  const trimmed = trimText(text);
  if (!trimmed.length) {
    return [];
  }
  return trimmed.split(/\s+/);
}
