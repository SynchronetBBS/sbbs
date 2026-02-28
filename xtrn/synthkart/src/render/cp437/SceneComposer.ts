/**
 * SceneComposer - Composes all render layers into final frame.
 */

interface RenderCell {
  char: string;
  attr: number;
}

class SceneComposer {
  private width: number;
  private height: number;
  private buffer: RenderCell[][];
  private defaultAttr: number;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;
    this.buffer = [];
    this.defaultAttr = makeAttr(BLACK, BG_BLACK);  // Black on black default
    this.clear();
  }

  /**
   * Clear buffer to empty space with black background.
   */
  clear(): void {
    this.buffer = [];
    for (var y = 0; y < this.height; y++) {
      var row: RenderCell[] = [];
      for (var x = 0; x < this.width; x++) {
        row.push({ char: ' ', attr: this.defaultAttr });
      }
      this.buffer.push(row);
    }
  }

  /**
   * Set a cell in the buffer (with bounds checking).
   */
  setCell(x: number, y: number, char: string, attr: number): void {
    if (x >= 0 && x < this.width && y >= 0 && y < this.height) {
      this.buffer[y][x] = { char: char, attr: attr };
    }
  }

  /**
   * Get a cell from the buffer (with bounds checking).
   */
  getCell(x: number, y: number): RenderCell | null {
    if (x >= 0 && x < this.width && y >= 0 && y < this.height) {
      return this.buffer[y][x];
    }
    return null;
  }

  /**
   * Write a string horizontally.
   */
  writeString(x: number, y: number, text: string, attr: number): void {
    for (var i = 0; i < text.length; i++) {
      this.setCell(x + i, y, text.charAt(i), attr);
    }
  }

  /**
   * Draw a horizontal line.
   */
  drawHLine(x: number, y: number, length: number, char: string, attr: number): void {
    for (var i = 0; i < length; i++) {
      this.setCell(x + i, y, char, attr);
    }
  }

  /**
   * Draw a vertical line.
   */
  drawVLine(x: number, y: number, length: number, char: string, attr: number): void {
    for (var i = 0; i < length; i++) {
      this.setCell(x, y + i, char, attr);
    }
  }

  /**
   * Draw a box outline.
   */
  drawBox(x: number, y: number, w: number, h: number, attr: number): void {
    // Corners
    this.setCell(x, y, GLYPH.BOX_TL, attr);
    this.setCell(x + w - 1, y, GLYPH.BOX_TR, attr);
    this.setCell(x, y + h - 1, GLYPH.BOX_BL, attr);
    this.setCell(x + w - 1, y + h - 1, GLYPH.BOX_BR, attr);

    // Horizontal edges
    for (var i = 1; i < w - 1; i++) {
      this.setCell(x + i, y, GLYPH.BOX_H, attr);
      this.setCell(x + i, y + h - 1, GLYPH.BOX_H, attr);
    }

    // Vertical edges
    for (var j = 1; j < h - 1; j++) {
      this.setCell(x, y + j, GLYPH.BOX_V, attr);
      this.setCell(x + w - 1, y + j, GLYPH.BOX_V, attr);
    }
  }

  /**
   * Get buffer for frame.js output.
   */
  getBuffer(): RenderCell[][] {
    return this.buffer;
  }

  /**
   * Get a single row as string (for simple output).
   */
  getRow(y: number): string {
    var row = '';
    for (var x = 0; x < this.width; x++) {
      row += this.buffer[y][x].char;
    }
    return row;
  }
}
