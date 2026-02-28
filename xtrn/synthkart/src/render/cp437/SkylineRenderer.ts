/**
 * SkylineRenderer - Renders the synthwave sky with parallax scrolling.
 *
 * The sky grid is FIXED relative to the road - it does NOT scroll horizontally.
 * Only the sun and mountains scroll (parallax effect).
 * 
 * Sky grid animation:
 * - When moving forward, horizontal lines emerge from horizon and move UP
 * - This creates the "warping through hyperspace" effect
 * - Animation ONLY happens when car is moving (speed > 0)
 */

class SkylineRenderer {
  private composer: SceneComposer;
  private horizonY: number;
  private parallax: ParallaxBackground;
  
  // Accumulated animation phase for horizontal line animation
  private gridAnimPhase: number;

  constructor(composer: SceneComposer, horizonY: number) {
    this.composer = composer;
    this.horizonY = horizonY;
    this.parallax = new ParallaxBackground(80, horizonY);
    this.parallax.resetScroll();  // Start centered
    this.gridAnimPhase = 0;
  }

  /**
   * Render the synthwave sky with parallax.
   * 
   * @param trackPosition Z position (unused now - we use speed*dt for animation)
   * @param curvature Current road curvature for parallax (-1 to 1)
   * @param playerSteer Player steering input for parallax (-1 to 1)
   * @param speed Current speed for animation rate
   * @param dt Delta time
   */
  render(_trackPosition: number, curvature?: number, playerSteer?: number, speed?: number, dt?: number): void {
    // Clear sky area to black first
    this.renderSkyBackground();
    
    // Update parallax scroll for sun/mountains (only when moving)
    if (curvature !== undefined && speed !== undefined && dt !== undefined && speed > 0) {
      this.parallax.updateScroll(curvature, playerSteer || 0, speed, dt);
    }
    
    // Update grid animation phase (only when moving forward)
    if (speed !== undefined && dt !== undefined && speed > 0) {
      // Positive phase change = lines move UP (toward camera)
      // Speed scaled to give nice animation rate
      this.gridAnimPhase += speed * dt * 0.003;
      
      // Keep in 0-1 range
      while (this.gridAnimPhase >= 1) this.gridAnimPhase -= 1;
      while (this.gridAnimPhase < 0) this.gridAnimPhase += 1;
    }
    
    // Render sky grid FIRST (background layer)
    this.renderSkyGrid();
    
    // Render parallax layers (sun, mountains) ON TOP
    this.parallax.render(this.composer);
  }

  /**
   * Render black sky background.
   */
  private renderSkyBackground(): void {
    var bgAttr = makeAttr(BLACK, BG_BLACK);
    for (var y = 0; y < this.horizonY; y++) {
      for (var x = 0; x < 80; x++) {
        this.composer.setCell(x, y, ' ', bgAttr);
      }
    }
  }

  /**
   * Render synthwave sky grid (magenta/cyan).
   * 
   * The grid is ALWAYS centered at x=40 (road vanishing point).
   * It does NOT scroll horizontally - this keeps it aligned with the road.
   * 
   * Horizontal lines animate to create forward motion effect:
   * - Lines "emerge" from the horizon and move upward
   * - Faster speed = faster line movement
   */
  private renderSkyGrid(): void {
    var gridAttr = colorToAttr(PALETTE.SKY_GRID);
    var glowAttr = colorToAttr(PALETTE.SKY_GRID_GLOW);

    var vanishX = 40;  // Always centered - matches road vanishing point
    var screenWidth = 80;

    // Draw from horizon upward
    for (var y = this.horizonY - 1; y >= 1; y--) {
      var distFromHorizon = this.horizonY - y;  // 1 at horizon, increases upward
      
      // Perspective spread - lines spread wider as they get closer (higher up)
      var spread = distFromHorizon * 6;
      
      // --- RADIAL/CONVERGING LINES ---
      // These create the "tunnel" effect, converging to vanishing point
      // Draw them at fixed intervals, extending to screen edges
      for (var offset = 0; offset <= 40; offset += 8) {
        if (offset <= spread) {
          var leftX = vanishX - offset;
          var rightX = vanishX + offset;

          if (offset === 0) {
            // Center vertical line
            this.composer.setCell(vanishX, y, GLYPH.BOX_V, gridAttr);
          } else {
            // Diagonal lines - use / and \ for converging effect
            if (leftX >= 0 && leftX < screenWidth) {
              this.composer.setCell(leftX, y, '/', glowAttr);
            }
            if (rightX >= 0 && rightX < screenWidth) {
              this.composer.setCell(rightX, y, '\\', glowAttr);
            }
          }
        }
      }
      
      // --- HORIZONTAL LINES (animated) ---
      // These create the forward motion effect
      // Phase determines which scanlines have horizontal lines this frame
      
      // Line spacing increases with distance from horizon (perspective)
      // Near horizon: lines closer together, far from horizon: lines further apart
      var lineSpacing = 3;  // Every Nth scanline gets a horizontal line
      
      // Calculate animated phase for this scanline
      // gridAnimPhase goes 0->1 as car moves forward
      // We want lines to appear to move UP (toward camera)
      // So higher Y (closer to horizon) should "lead" the phase
      var scanlinePhase = (this.gridAnimPhase * lineSpacing + (this.horizonY - y) * 0.3) % 1;
      
      // Draw horizontal line if this scanline is in the right phase
      if (scanlinePhase < 0.33) {
        // Extend horizontal lines to full spread width
        var lineSpread = Math.min(spread, 39);
        for (var x = vanishX - lineSpread; x <= vanishX + lineSpread; x++) {
          if (x >= 0 && x < screenWidth) {
            // Don't overwrite the radial lines
            var buffer = this.composer.getBuffer();
            var cell = buffer[y][x];
            if (cell.char === ' ') {
              this.composer.setCell(x, y, GLYPH.BOX_H, glowAttr);
            }
          }
        }
      }
    }
  }
}

