/**
 * SpriteRenderer - Renders vehicle and object sprites.
 * Stub for Iteration 0.
 */

class SpriteRenderer {
  private composer: SceneComposer;
  private horizonY: number;
  private roadBottom: number;

  constructor(composer: SceneComposer, horizonY: number) {
    this.composer = composer;
    this.horizonY = horizonY;
    this.roadBottom = 23;
  }

  /**
   * Render player vehicle (always at bottom center).
   */
  renderPlayerVehicle(steerOffset: number): void {
    var baseY = 20;
    var baseX = 38 + Math.round(steerOffset * 2);

    // Simple car shape
    var bodyAttr = colorToAttr(PALETTE.PLAYER_BODY);
    var trimAttr = colorToAttr(PALETTE.PLAYER_TRIM);

    //    ▄█▄
    //   █████
    //   ▀▄█▄▀
    this.composer.setCell(baseX, baseY, GLYPH.LOWER_HALF, bodyAttr);
    this.composer.setCell(baseX + 1, baseY, GLYPH.FULL_BLOCK, trimAttr);
    this.composer.setCell(baseX + 2, baseY, GLYPH.LOWER_HALF, bodyAttr);

    this.composer.setCell(baseX - 1, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
    this.composer.setCell(baseX, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
    this.composer.setCell(baseX + 1, baseY + 1, GLYPH.FULL_BLOCK, trimAttr);
    this.composer.setCell(baseX + 2, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
    this.composer.setCell(baseX + 3, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);

    this.composer.setCell(baseX - 1, baseY + 2, GLYPH.UPPER_HALF, bodyAttr);
    this.composer.setCell(baseX, baseY + 2, GLYPH.LOWER_HALF, bodyAttr);
    this.composer.setCell(baseX + 1, baseY + 2, GLYPH.FULL_BLOCK, trimAttr);
    this.composer.setCell(baseX + 2, baseY + 2, GLYPH.LOWER_HALF, bodyAttr);
    this.composer.setCell(baseX + 3, baseY + 2, GLYPH.UPPER_HALF, bodyAttr);
  }

  /**
   * Render other vehicles.
   * @param relativeZ - Distance from player (positive = ahead)
   * @param relativeX - Lateral offset from player
   */
  renderOtherVehicle(relativeZ: number, relativeX: number, color: number): void {
    if (relativeZ < 5 || relativeZ > 200) return;

    // Calculate screen position based on distance
    var t = Math.max(0, Math.min(1, 1 - (relativeZ / 200)));
    var screenY = Math.round(this.horizonY + t * (this.roadBottom - this.horizonY - 3));
    var scale = t;
    var screenX = 40 + Math.round(relativeX * scale * 2);

    if (screenY < this.horizonY || screenY > this.roadBottom - 3) return;

    // Simple scaled car
    var attr = makeAttr(color, BG_BLACK);

    if (scale > 0.5) {
      // Large (close)
      this.composer.setCell(screenX, screenY, GLYPH.UPPER_HALF, attr);
      this.composer.setCell(screenX + 1, screenY, GLYPH.UPPER_HALF, attr);
      this.composer.setCell(screenX, screenY + 1, GLYPH.FULL_BLOCK, attr);
      this.composer.setCell(screenX + 1, screenY + 1, GLYPH.FULL_BLOCK, attr);
    } else if (scale > 0.25) {
      // Medium
      this.composer.setCell(screenX, screenY, GLYPH.DARK_SHADE, attr);
      this.composer.setCell(screenX, screenY + 1, GLYPH.FULL_BLOCK, attr);
    } else {
      // Small (far)
      this.composer.setCell(screenX, screenY, GLYPH.MEDIUM_SHADE, attr);
    }
  }

  /**
   * Render item boxes.
   */
  renderItemBox(relativeZ: number, relativeX: number): void {
    if (relativeZ < 5 || relativeZ > 200) return;

    var t = Math.max(0, Math.min(1, 1 - (relativeZ / 200)));
    var screenY = Math.round(this.horizonY + t * (this.roadBottom - this.horizonY - 2));
    var screenX = 40 + Math.round(relativeX * t * 2);

    if (screenY < this.horizonY || screenY > this.roadBottom - 2) return;

    var attr = colorToAttr(PALETTE.ITEM_BOX);
    this.composer.setCell(screenX, screenY, '?', attr);
  }
}
