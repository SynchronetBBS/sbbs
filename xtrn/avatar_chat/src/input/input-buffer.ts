export interface InputViewport {
  text: string;
  cursorColumn: number;
}

export class InputBuffer {
  private value: string;
  private cursor: number;
  private readonly maxLength: number;

  public constructor(maxLength: number) {
    this.value = "";
    this.cursor = 0;
    this.maxLength = maxLength;
  }

  public clear(): void {
    this.value = "";
    this.cursor = 0;
  }

  public getValue(): string {
    return this.value;
  }

  public setValue(value: string, cursor?: number): void {
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
  }

  public getCursor(): number {
    return this.cursor;
  }

  public isEmpty(): boolean {
    return this.value.length === 0;
  }

  public insert(text: string): void {
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
  }

  public backspace(): void {
    if (this.cursor <= 0) {
      return;
    }

    this.value = this.value.substr(0, this.cursor - 1) + this.value.substr(this.cursor);
    this.cursor -= 1;
  }

  public deleteForward(): void {
    if (this.cursor >= this.value.length) {
      return;
    }

    this.value = this.value.substr(0, this.cursor) + this.value.substr(this.cursor + 1);
  }

  public moveLeft(): void {
    if (this.cursor > 0) {
      this.cursor -= 1;
    }
  }

  public moveRight(): void {
    if (this.cursor < this.value.length) {
      this.cursor += 1;
    }
  }

  public moveHome(): void {
    this.cursor = 0;
  }

  public moveEnd(): void {
    this.cursor = this.value.length;
  }

  public getViewport(width: number): InputViewport {
    let start = 0;
    let cursorColumn = 1;
    let text = "";

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
  }
}
