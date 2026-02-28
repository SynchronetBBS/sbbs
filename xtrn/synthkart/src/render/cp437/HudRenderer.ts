/**
 * HudRenderer - Renders the heads-up display.
 */

class HudRenderer {
  private composer: SceneComposer;

  constructor(composer: SceneComposer) {
    this.composer = composer;
  }

  /**
   * Render complete HUD.
   */
  render(hudData: HudData): void {
    this.renderTopBar(hudData);
    this.renderLapProgress(hudData);
    this.renderSpeedometer(hudData);
    this.renderItemSlot(hudData);
    
    // Render countdown overlay if race hasn't started
    if (hudData.countdown > 0 && hudData.raceMode === RaceMode.GRAND_PRIX) {
      this.renderCountdown(hudData.countdown);
    }
    // Note: raceFinished overlay removed - now using dedicated results screen
  }

  /**
   * Render top information bar.
   */
  private renderTopBar(data: HudData): void {
    var y = 0;
    var valueAttr = colorToAttr(PALETTE.HUD_VALUE);
    var labelAttr = colorToAttr(PALETTE.HUD_LABEL);

    // Top bar background
    for (var x = 0; x < 80; x++) {
      this.composer.setCell(x, y, ' ', makeAttr(BLACK, BG_BLACK));
    }

    // Lap counter
    this.composer.writeString(2, y, "LAP", labelAttr);
    this.composer.writeString(6, y, data.lap + "/" + data.totalLaps, valueAttr);

    // Position
    this.composer.writeString(14, y, "POS", labelAttr);
    var posSuffix = PositionIndicator.getOrdinalSuffix(data.position);
    this.composer.writeString(18, y, data.position + posSuffix, valueAttr);

    // Time
    this.composer.writeString(26, y, "TIME", labelAttr);
    this.composer.writeString(31, y, LapTimer.format(data.lapTime), valueAttr);

    // Best lap
    if (data.bestLapTime > 0) {
      this.composer.writeString(45, y, "BEST", labelAttr);
      this.composer.writeString(50, y, LapTimer.format(data.bestLapTime), valueAttr);
    }

    // Speed (right side)
    this.composer.writeString(66, y, "SPD", labelAttr);
    var speedStr = this.padLeft(data.speed.toString(), 3);
    this.composer.writeString(70, y, speedStr, valueAttr);
  }

  /**
   * Render lap progress bar on row 1.
   */
  private renderLapProgress(data: HudData): void {
    var y = 1;
    var barX = 2;
    var barWidth = 60;

    var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
    var filledAttr = colorToAttr({ fg: CYAN, bg: BG_BLACK });
    var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var finishAttr = colorToAttr({ fg: WHITE, bg: BG_MAGENTA });

    // Row background
    for (var x = 0; x < 80; x++) {
      this.composer.setCell(x, y, ' ', makeAttr(BLACK, BG_BLACK));
    }

    // Progress label with percentage
    var pct = Math.floor(data.lapProgress * 100);
    var pctStr = this.padLeft(pct.toString(), 3) + "%";
    this.composer.writeString(barX, y, "TRACK", labelAttr);
    this.composer.writeString(barX + 6, y, pctStr, colorToAttr(PALETTE.HUD_VALUE));

    // Progress bar
    var barStartX = barX + 12;
    this.composer.setCell(barStartX, y, '[', labelAttr);

    var fillWidth = Math.floor(data.lapProgress * barWidth);

    for (var i = 0; i < barWidth; i++) {
      var isFinish = (i === barWidth - 1);  // Last position is finish line
      var attr: number;
      var char: string;

      if (isFinish) {
        // Checkered flag at finish
        attr = finishAttr;
        char = GLYPH.CHECKER;
      } else if (i < fillWidth) {
        attr = filledAttr;
        char = GLYPH.FULL_BLOCK;
      } else {
        attr = emptyAttr;
        char = GLYPH.LIGHT_SHADE;
      }

      this.composer.setCell(barStartX + 1 + i, y, char, attr);
    }

    this.composer.setCell(barStartX + barWidth + 1, y, ']', labelAttr);

    // Show finish flag icon at end
    this.composer.writeString(barStartX + barWidth + 3, y, "FINISH", finishAttr);
  }

  /**
   * Render graphical speedometer.
   */
  private renderSpeedometer(data: HudData): void {
    var y = 23;
    var barX = 2;
    var barWidth = 20;

    var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
    var filledAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
    var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var highAttr = colorToAttr({ fg: LIGHTRED, bg: BG_BLACK });

    this.composer.writeString(barX, y, "[", labelAttr);

    var fillAmount = data.speed / data.speedMax;
    var fillWidth = Math.round(fillAmount * barWidth);

    for (var i = 0; i < barWidth; i++) {
      var attr = i < fillWidth ?
        (fillAmount > 0.8 ? highAttr : filledAttr) :
        emptyAttr;
      var char = i < fillWidth ? GLYPH.FULL_BLOCK : GLYPH.LIGHT_SHADE;
      this.composer.setCell(barX + 1 + i, y, char, attr);
    }

    this.composer.writeString(barX + barWidth + 1, y, "]", labelAttr);
  }

  /**
   * Render held item slot with visual icons.
   * Icons are right-aligned to x=79 (frame edge).
   * Multi-use items show count to the left of the icon.
   */
  private renderItemSlot(data: HudData): void {
    var bottomY = 23;
    var rightEdge = 79;  // Right edge of frame
    
    if (data.heldItem === null) {
      // No item - show empty indicator
      var emptyAttr = colorToAttr(PALETTE.HUD_LABEL);
      this.composer.writeString(rightEdge - 3, bottomY, "----", emptyAttr);
      return;
    }
    
    var itemType = data.heldItem.type;
    var uses = data.heldItem.uses;
    
    // Get icon data for this item type
    var icon = this.getItemIcon(itemType);
    
    // Calculate icon position (right-aligned)
    var iconWidth = icon.lines[0].length;
    var iconHeight = icon.lines.length;
    var iconX = rightEdge - iconWidth + 1;
    var iconY = bottomY - iconHeight + 1;
    
    // Render icon
    for (var row = 0; row < iconHeight; row++) {
      var line = icon.lines[row];
      for (var col = 0; col < line.length; col++) {
        var ch = line.charAt(col);
        if (ch !== ' ') {
          // Check for special color codes in icon
          var attr = this.getIconCharAttr(ch, icon, itemType);
          this.composer.setCell(iconX + col, iconY + row, ch, attr);
        }
      }
    }
    
    // Show uses count if > 1 (to the left of icon)
    if (uses > 1) {
      var countAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
      this.composer.setCell(iconX - 2, bottomY, 'x', colorToAttr(PALETTE.HUD_LABEL));
      this.composer.setCell(iconX - 1, bottomY, String(uses).charAt(0), countAttr);
    }
  }
  
  /**
   * Get icon sprite data for an item type.
   * Returns { lines: string[], color: {fg, bg}, altColor?: {fg, bg} }
   */
  private getItemIcon(type: ItemType): { lines: string[], color: { fg: number, bg: number }, altColor?: { fg: number, bg: number } } {
    switch (type) {
      // === MUSHROOM ICONS ===
      case ItemType.MUSHROOM:
      case ItemType.MUSHROOM_TRIPLE:
        // Red mushroom with spots
        //  @@
        // @##@
        //  ||
        return {
          lines: [
            ' @@ ',
            '@##@',
            ' || '
          ],
          color: { fg: LIGHTRED, bg: BG_BLACK },      // @ = cap
          altColor: { fg: WHITE, bg: BG_BLACK }       // # = spots, | = stem
        };
        
      case ItemType.MUSHROOM_GOLDEN:
        // Golden mushroom (shiny)
        return {
          lines: [
            ' @@ ',
            '@##@',
            ' || '
          ],
          color: { fg: YELLOW, bg: BG_BLACK },
          altColor: { fg: WHITE, bg: BG_BLACK }
        };
      
      // === SHELL ICONS ===
      case ItemType.GREEN_SHELL:
      case ItemType.GREEN_SHELL_TRIPLE:
        // Green turtle shell
        //  /^\
        // (O O)
        //  \_/
        return {
          lines: [
            ' /^\\ ',
            '(O O)',
            ' \\_/ '
          ],
          color: { fg: LIGHTGREEN, bg: BG_BLACK }
        };
        
      case ItemType.RED_SHELL:
      case ItemType.RED_SHELL_TRIPLE:
      case ItemType.SHELL:
      case ItemType.SHELL_TRIPLE:
        // Red turtle shell
        return {
          lines: [
            ' /^\\ ',
            '(O O)',
            ' \\_/ '
          ],
          color: { fg: LIGHTRED, bg: BG_BLACK }
        };
        
      case ItemType.BLUE_SHELL:
        // Blue spiny shell with wings
        //  ~*~
        // <(@)>
        //  \_/
        return {
          lines: [
            ' ~*~ ',
            '<(@)>',
            ' \\_/ '
          ],
          color: { fg: LIGHTBLUE, bg: BG_BLACK },
          altColor: { fg: LIGHTCYAN, bg: BG_BLACK }   // wings
        };
      
      // === BANANA ICON ===
      case ItemType.BANANA:
      case ItemType.BANANA_TRIPLE:
        // Banana peel
        //  /\\
        // (  )
        //  \/
        return {
          lines: [
            '  /\\ ',
            ' (  )',
            '  \\/ '
          ],
          color: { fg: YELLOW, bg: BG_BLACK }
        };
      
      // === STAR ICON ===
      case ItemType.STAR:
        // Invincibility star
        //   *
        //  ***
        // *****
        //  * *
        return {
          lines: [
            '  *  ',
            ' *** ',
            '*****',
            ' * * '
          ],
          color: { fg: YELLOW, bg: BG_BLACK }
        };
      
      // === LIGHTNING ICON ===
      case ItemType.LIGHTNING:
        // Lightning bolt
        //  /|
        // /-'
        // |/
        return {
          lines: [
            ' /| ',
            '/-\' ',
            '|/  '
          ],
          color: { fg: YELLOW, bg: BG_BLACK },
          altColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        };
      
      // === BULLET ICON ===
      case ItemType.BULLET:
        // Bullet Bill
        //  __
        // |===>
        //  --
        return {
          lines: [
            ' __ ',
            '|==>',
            ' -- '
          ],
          color: { fg: WHITE, bg: BG_BLACK },
          altColor: { fg: DARKGRAY, bg: BG_BLACK }
        };
      
      default:
        // Unknown item - show ?
        return {
          lines: [
            ' ? '
          ],
          color: { fg: YELLOW, bg: BG_BLACK }
        };
    }
  }
  
  /**
   * Get attribute for a character in an icon based on the character.
   */
  private getIconCharAttr(ch: string, icon: { lines: string[], color: { fg: number, bg: number }, altColor?: { fg: number, bg: number } }, itemType: ItemType): number {
    // Special characters use altColor
    var useAlt = false;
    
    switch (itemType) {
      case ItemType.MUSHROOM:
      case ItemType.MUSHROOM_TRIPLE:
      case ItemType.MUSHROOM_GOLDEN:
        // # = spots (white), | = stem (white), @ = cap (main color)
        useAlt = (ch === '#' || ch === '|');
        break;
        
      case ItemType.BLUE_SHELL:
        // < > ~ = wings (altColor)
        useAlt = (ch === '<' || ch === '>' || ch === '~');
        break;
        
      case ItemType.LIGHTNING:
        // Alternate flash effect
        useAlt = (Math.floor(Date.now() / 150) % 2 === 0);
        break;
        
      case ItemType.STAR:
        // Rainbow cycling for star
        var starColors = [YELLOW, LIGHTRED, LIGHTGREEN, LIGHTCYAN, LIGHTMAGENTA, WHITE];
        var colorIdx = Math.floor(Date.now() / 100) % starColors.length;
        return makeAttr(starColors[colorIdx], BG_BLACK);
        
      case ItemType.BULLET:
        // = and > are lighter, rest is dark
        useAlt = (ch !== '=' && ch !== '>');
        break;
    }
    
    if (useAlt && icon.altColor) {
      return makeAttr(icon.altColor.fg, icon.altColor.bg);
    }
    return makeAttr(icon.color.fg, icon.color.bg);
  }

  /**
   * Pad string on left.
   */
  private padLeft(str: string, len: number): string {
    while (str.length < len) {
      str = ' ' + str;
    }
    return str;
  }

  /**
   * Render stoplight countdown graphic in center of screen.
   */
  private renderCountdown(countdown: number): void {
    var countNum = Math.ceil(countdown);
    var centerX = 40;
    var topY = 8;
    
    // Stoplight frame colors
    var frameAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var poleAttr = colorToAttr({ fg: BROWN, bg: BG_BLACK });
    
    // Light states based on countdown
    var redOn = countNum >= 3;
    var yellowOn = countNum === 2;
    var greenOn = countNum <= 1;
    
    // Light colors (on vs off)
    var redAttr = redOn ? colorToAttr({ fg: LIGHTRED, bg: BG_RED }) : colorToAttr({ fg: RED, bg: BG_BLACK });
    var yellowAttr = yellowOn ? colorToAttr({ fg: YELLOW, bg: BG_BROWN }) : colorToAttr({ fg: BROWN, bg: BG_BLACK });
    var greenAttr = greenOn ? colorToAttr({ fg: LIGHTGREEN, bg: BG_GREEN }) : colorToAttr({ fg: GREEN, bg: BG_BLACK });
    
    // Stoplight housing (7 chars wide, 9 rows tall)
    var boxX = centerX - 3;
    
    // Top of housing
    this.composer.setCell(boxX, topY, GLYPH.DBOX_TL, frameAttr);
    for (var i = 1; i < 6; i++) {
      this.composer.setCell(boxX + i, topY, GLYPH.DBOX_H, frameAttr);
    }
    this.composer.setCell(boxX + 6, topY, GLYPH.DBOX_TR, frameAttr);
    
    // RED light row (row 1-2)
    this.composer.setCell(boxX, topY + 1, GLYPH.DBOX_V, frameAttr);
    this.composer.writeString(boxX + 1, topY + 1, " ", frameAttr);
    this.composer.setCell(boxX + 2, topY + 1, GLYPH.FULL_BLOCK, redAttr);
    this.composer.setCell(boxX + 3, topY + 1, GLYPH.FULL_BLOCK, redAttr);
    this.composer.setCell(boxX + 4, topY + 1, GLYPH.FULL_BLOCK, redAttr);
    this.composer.writeString(boxX + 5, topY + 1, " ", frameAttr);
    this.composer.setCell(boxX + 6, topY + 1, GLYPH.DBOX_V, frameAttr);
    
    // Separator
    this.composer.setCell(boxX, topY + 2, GLYPH.DBOX_V, frameAttr);
    this.composer.writeString(boxX + 1, topY + 2, "     ", frameAttr);
    this.composer.setCell(boxX + 6, topY + 2, GLYPH.DBOX_V, frameAttr);
    
    // YELLOW light row (row 3-4)
    this.composer.setCell(boxX, topY + 3, GLYPH.DBOX_V, frameAttr);
    this.composer.writeString(boxX + 1, topY + 3, " ", frameAttr);
    this.composer.setCell(boxX + 2, topY + 3, GLYPH.FULL_BLOCK, yellowAttr);
    this.composer.setCell(boxX + 3, topY + 3, GLYPH.FULL_BLOCK, yellowAttr);
    this.composer.setCell(boxX + 4, topY + 3, GLYPH.FULL_BLOCK, yellowAttr);
    this.composer.writeString(boxX + 5, topY + 3, " ", frameAttr);
    this.composer.setCell(boxX + 6, topY + 3, GLYPH.DBOX_V, frameAttr);
    
    // Separator
    this.composer.setCell(boxX, topY + 4, GLYPH.DBOX_V, frameAttr);
    this.composer.writeString(boxX + 1, topY + 4, "     ", frameAttr);
    this.composer.setCell(boxX + 6, topY + 4, GLYPH.DBOX_V, frameAttr);
    
    // GREEN light row (row 5-6)
    this.composer.setCell(boxX, topY + 5, GLYPH.DBOX_V, frameAttr);
    this.composer.writeString(boxX + 1, topY + 5, " ", frameAttr);
    this.composer.setCell(boxX + 2, topY + 5, GLYPH.FULL_BLOCK, greenAttr);
    this.composer.setCell(boxX + 3, topY + 5, GLYPH.FULL_BLOCK, greenAttr);
    this.composer.setCell(boxX + 4, topY + 5, GLYPH.FULL_BLOCK, greenAttr);
    this.composer.writeString(boxX + 5, topY + 5, " ", frameAttr);
    this.composer.setCell(boxX + 6, topY + 5, GLYPH.DBOX_V, frameAttr);
    
    // Bottom of housing
    this.composer.setCell(boxX, topY + 6, GLYPH.DBOX_BL, frameAttr);
    for (var j = 1; j < 6; j++) {
      this.composer.setCell(boxX + j, topY + 6, GLYPH.DBOX_H, frameAttr);
    }
    this.composer.setCell(boxX + 6, topY + 6, GLYPH.DBOX_BR, frameAttr);
    
    // Pole below stoplight
    this.composer.setCell(centerX, topY + 7, GLYPH.DBOX_V, poleAttr);
    this.composer.setCell(centerX, topY + 8, GLYPH.DBOX_V, poleAttr);
    
    // "GO!" text when green
    if (greenOn && countNum <= 0) {
      var goAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
      this.composer.writeString(centerX - 2, topY + 9, "GO!!!", goAttr);
    }
  }
}
