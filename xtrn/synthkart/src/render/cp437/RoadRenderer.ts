/**
 * RoadRenderer - Renders the pseudo-3D road with cyan grid aesthetic
 * and off-road scenery (trees, rocks, dirt) on black background.
 */

class RoadRenderer {
  private composer: SceneComposer;
  private horizonY: number;
  private roadBottom: number;

  constructor(composer: SceneComposer, horizonY: number) {
    this.composer = composer;
    this.horizonY = horizonY;
    this.roadBottom = 23;  // Bottom of screen
  }

  /**
   * Render the road surface and off-road areas.
   */
  render(trackPosition: number, cameraX: number, track: ITrack, road: Road): void {
    var roadLength = road ? road.totalLength : 55000;

    for (var screenY = this.roadBottom; screenY > this.horizonY; screenY--) {
      var t = (this.roadBottom - screenY) / (this.roadBottom - this.horizonY);
      var distance = 1 / (1 - t * 0.95);

      // Road width narrows with distance
      var roadWidth = Math.round(40 / distance);
      var halfWidth = Math.floor(roadWidth / 2);

      // Road center can shift based on track curves
      var curveOffset = 0;
      if (track && typeof track.getCenterlineX === 'function') {
        curveOffset = Math.round(track.getCenterlineX(trackPosition + distance * 10) * 0.1);
      }
      var centerX = 40 + curveOffset - Math.round(cameraX * 0.5);

      // Render this scanline
      this.renderScanline(screenY, centerX, halfWidth, distance, trackPosition, roadLength);
    }
  }

  /**
   * Render a single road scanline with off-road scenery.
   */
  private renderScanline(y: number, centerX: number, halfWidth: number, distance: number, trackZ: number, roadLength: number): void {
    var width = 80;

    // Road edges
    var leftEdge = centerX - halfWidth;
    var rightEdge = centerX + halfWidth;

    // Calculate stripe phase for animated dashes
    var stripePhase = Math.floor((trackZ + distance * 5) / 15) % 2;

    // Cyan road attribute
    var roadAttr = colorToAttr(distance < 10 ? PALETTE.ROAD_LIGHT : PALETTE.ROAD_DARK);
    var gridAttr = colorToAttr(PALETTE.ROAD_GRID);

    // Check if this scanline is at the start/finish line
    var worldZ = trackZ + distance * 5;
    var wrappedZ = worldZ % roadLength;
    if (wrappedZ < 0) wrappedZ += roadLength;
    var isFinishLine = (wrappedZ < 200) || (wrappedZ > roadLength - 200);

    for (var x = 0; x < width; x++) {
      if (x >= leftEdge && x <= rightEdge) {
        // === ON ROAD ===
        if (isFinishLine) {
          this.renderFinishLineCell(x, y, centerX, leftEdge, rightEdge, distance);
        } else {
          this.renderRoadCell(x, y, centerX, leftEdge, rightEdge, stripePhase, distance, roadAttr, gridAttr);
        }
      } else {
        // === OFF ROAD ===
        this.renderOffroadCell(x, y, leftEdge, rightEdge, distance, trackZ);
      }
    }
  }

  /**
   * Render a checkered finish line cell.
   */
  private renderFinishLineCell(x: number, y: number, centerX: number, leftEdge: number, rightEdge: number, distance: number): void {
    // Edge markers (bright white for finish)
    if (x === leftEdge || x === rightEdge) {
      this.composer.setCell(x, y, GLYPH.BOX_V, colorToAttr({ fg: WHITE, bg: BG_BLACK }));
      return;
    }

    // Checkered pattern: alternate black/white based on x and y
    var checkerSize = Math.max(1, Math.floor(3 / distance));
    var checkerX = Math.floor((x - centerX) / checkerSize);
    var checkerY = Math.floor(y / 2);
    var isWhite = ((checkerX + checkerY) % 2) === 0;

    if (isWhite) {
      this.composer.setCell(x, y, GLYPH.FULL_BLOCK, colorToAttr({ fg: WHITE, bg: BG_LIGHTGRAY }));
    } else {
      this.composer.setCell(x, y, ' ', colorToAttr({ fg: BLACK, bg: BG_BLACK }));
    }
  }

  /**
   * Render a road cell with cyan grid aesthetic.
   */
  private renderRoadCell(x: number, y: number, centerX: number, leftEdge: number, rightEdge: number,
                         stripePhase: number, distance: number, roadAttr: number, gridAttr: number): void {

    // Edge markers (bright cyan)
    if (x === leftEdge || x === rightEdge) {
      this.composer.setCell(x, y, GLYPH.BOX_V, colorToAttr(PALETTE.ROAD_EDGE));
      return;
    }

    // Center stripe (animated dashes)
    if (Math.abs(x - centerX) < 1 && stripePhase === 0) {
      this.composer.setCell(x, y, GLYPH.BOX_V, colorToAttr(PALETTE.ROAD_STRIPE));
      return;
    }

    // Grid lines on road (horizontal dashes for perspective)
    var gridPhase = Math.floor(distance) % 3;
    if (gridPhase === 0 && distance > 5) {
      this.composer.setCell(x, y, GLYPH.BOX_H, gridAttr);
    } else {
      // Regular road surface
      var shade = this.getShadeForDistance(distance);
      this.composer.setCell(x, y, shade, roadAttr);
    }
  }

  /**
   * Render off-road scenery cell.
   * Only triggers sprite drawing at specific anchor points to avoid duplicates.
   */
  private renderOffroadCell(x: number, y: number, leftEdge: number, rightEdge: number,
                            distance: number, trackZ: number): void {

    // Black background by default
    var bgAttr = makeAttr(BLACK, BG_BLACK);
    this.composer.setCell(x, y, ' ', bgAttr);

    // Add scenery based on position and pseudo-random placement
    var side = x < leftEdge ? 'left' : 'right';
    var distFromRoad = side === 'left' ? leftEdge - x : x - rightEdge;

    // Scale factor for pseudo-3D scaling (larger when closer)
    var scale = Math.min(1, 3 / distance);

    // Pseudo-random seed based on world position (trackZ determines which objects exist)
    // Use larger grid cells to space out objects and prevent overlap
    var gridSize = 8;  // Objects spaced 8 units apart in world Z
    var worldZ = Math.floor(trackZ + distance * 10);
    var gridZ = Math.floor(worldZ / gridSize);
    
    // Only draw at specific x positions to space objects horizontally
    var anchorX = (side === 'left') ? leftEdge - 8 : rightEdge + 8;
    
    // Check if this x,y is an anchor point for a sprite
    var seed = gridZ * 1000 + (side === 'left' ? 1 : 2);
    var rand = this.pseudoRandom(seed);
    
    // Only the specific anchor x draws the sprite (not every cell)
    var shouldDraw = (Math.abs(x - anchorX) < 2) && 
                     (rand < 0.7) && 
                     (distFromRoad > 3 && distFromRoad < 20);
    
    if (shouldDraw && x === anchorX) {
      this.drawSceneryObject(x, y, distance, scale, rand);
    } else if (distFromRoad <= 2) {
      // Dirt/grass edge
      this.composer.setCell(x, y, GLYPH.GRASS, colorToAttr(PALETTE.OFFROAD_DIRT));
    }
  }

  /**
   * Draw a scenery object (tree, rock, bush) with proper multi-character sprites.
   * Uses half-blocks for two-tone cells. Scale: far=1, med=3, close=4-5, very close=5-6.
   */
  private drawSceneryObject(x: number, y: number, distance: number, scale: number, rand: number): void {
    // Choose object type based on random value
    if (rand < 0.05) {
      // Tree - brown trunk, green/lightgreen leaves
      this.drawTree(x, y, distance, scale);
    } else if (rand < 0.10) {
      // Rock - gray, wide and flat
      this.drawRock(x, y, distance, scale);
    } else {
      // Bush - green, round
      this.drawBush(x, y, distance, scale);
    }
  }

  /**
   * Draw a tree with trunk and foliage using half-blocks.
   * Far: single half-block (green top, brown bottom)
   * Scales up to 5-6 wide with fuller foliage wider than trunk.
   */
  private drawTree(x: number, y: number, distance: number, _scale: number): void {
    // Two-tone cell: UPPER_HALF = fg is top color, bg is bottom color
    var leafTrunkAttr = makeAttr(LIGHTGREEN, BG_BROWN);  // Green top, brown bottom
    var leafAttr = makeAttr(LIGHTGREEN, BG_BLACK);
    var leafDarkAttr = makeAttr(GREEN, BG_BLACK);
    var trunkAttr = makeAttr(BROWN, BG_BLACK);

    if (distance > 8) {
      // Far: single half-block - green top, brown bottom (1 wide)
      this.safePut(x, y, GLYPH.UPPER_HALF, leafTrunkAttr);
    } else if (distance > 5) {
      // Medium-far: 3 wide, 2 tall - foliage on top row, trunk below
      this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x, y - 1, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x, y, GLYPH.UPPER_HALF, leafTrunkAttr);  // Green/brown transition
    } else if (distance > 3) {
      // Medium: 4 wide, 3 tall - wider foliage
      this.safePut(x - 1, y - 2, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 2, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x - 1, y - 1, GLYPH.DARK_SHADE, leafDarkAttr);
      this.safePut(x, y - 1, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 1, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 1, GLYPH.DARK_SHADE, leafDarkAttr);
      this.safePut(x, y, GLYPH.UPPER_HALF, leafTrunkAttr);
      this.safePut(x + 1, y, GLYPH.UPPER_HALF, leafTrunkAttr);
    } else if (distance > 1.5) {
      // Close: 5 wide, 4 tall - full tree with wide canopy
      this.safePut(x - 2, y - 3, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x - 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 3, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x - 2, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
      this.safePut(x - 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
      this.safePut(x - 1, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
      this.safePut(x, y - 1, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, trunkAttr);
      this.safePut(x, y + 1, GLYPH.UPPER_HALF, trunkAttr);
    } else {
      // Very close: 6 wide, 5 tall - massive palm tree
      this.safePut(x - 2, y - 4, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x - 1, y - 4, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x, y - 4, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 4, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 4, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 3, y - 4, GLYPH.UPPER_HALF, leafDarkAttr);
      this.safePut(x - 2, y - 3, GLYPH.FULL_BLOCK, leafDarkAttr);
      this.safePut(x - 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 3, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 3, y - 3, GLYPH.FULL_BLOCK, leafDarkAttr);
      this.safePut(x - 1, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
      this.safePut(x, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
      this.safePut(x + 2, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
      this.safePut(x, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
      this.safePut(x + 1, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, trunkAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, trunkAttr);
      this.safePut(x, y + 1, GLYPH.UPPER_HALF, trunkAttr);
      this.safePut(x + 1, y + 1, GLYPH.UPPER_HALF, trunkAttr);
    }
  }

  /**
   * Draw a rock - gray, WIDE and flat (not tall like trees).
   * Far: single half-block. Scales to 5-6 wide but only 1-2 tall.
   */
  private drawRock(x: number, y: number, distance: number, _scale: number): void {
    var rockAttr = makeAttr(DARKGRAY, BG_BLACK);
    var rockLightAttr = makeAttr(LIGHTGRAY, BG_BLACK);

    if (distance > 8) {
      // Far: single half-block
      this.safePut(x, y, GLYPH.LOWER_HALF, rockAttr);
    } else if (distance > 5) {
      // Medium-far: 3 wide, 1 tall - flat boulder
      this.safePut(x - 1, y, GLYPH.LOWER_HALF, rockAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
      this.safePut(x + 1, y, GLYPH.LOWER_HALF, rockAttr);
    } else if (distance > 3) {
      // Medium: 4 wide, 1 tall - wider boulder
      this.safePut(x - 1, y, GLYPH.LOWER_HALF, rockAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, rockAttr);
      this.safePut(x + 2, y, GLYPH.LOWER_HALF, rockAttr);
    } else if (distance > 1.5) {
      // Close: 5 wide, 2 tall - big flat rock
      this.safePut(x - 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
      this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
      this.safePut(x, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
      this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, rockAttr);
      this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
      this.safePut(x - 2, y, GLYPH.LOWER_HALF, rockAttr);
      this.safePut(x - 1, y, GLYPH.FULL_BLOCK, rockAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, rockAttr);
      this.safePut(x + 2, y, GLYPH.LOWER_HALF, rockAttr);
    } else {
      // Very close: 6 wide, 2 tall - massive flat boulder
      this.safePut(x - 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
      this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
      this.safePut(x, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
      this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
      this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
      this.safePut(x + 3, y - 1, GLYPH.UPPER_HALF, rockAttr);
      this.safePut(x - 2, y, GLYPH.LOWER_HALF, rockAttr);
      this.safePut(x - 1, y, GLYPH.FULL_BLOCK, rockAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, rockLightAttr);
      this.safePut(x + 2, y, GLYPH.FULL_BLOCK, rockAttr);
      this.safePut(x + 3, y, GLYPH.LOWER_HALF, rockAttr);
    }
  }

  /**
   * Draw a bush - green, round and wide.
   * Far: single half-block. Scales to 4-5 wide but stays short.
   */
  private drawBush(x: number, y: number, distance: number, _scale: number): void {
    var bushAttr = makeAttr(GREEN, BG_BLACK);
    var bushLightAttr = makeAttr(LIGHTGREEN, BG_BLACK);

    if (distance > 8) {
      // Far: single half-block
      this.safePut(x, y, GLYPH.LOWER_HALF, bushAttr);
    } else if (distance > 5) {
      // Medium-far: 3 wide, 1 tall
      this.safePut(x - 1, y, GLYPH.LOWER_HALF, bushAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
      this.safePut(x + 1, y, GLYPH.LOWER_HALF, bushAttr);
    } else if (distance > 3) {
      // Medium: 4 wide, 1 tall
      this.safePut(x - 1, y, GLYPH.LOWER_HALF, bushAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, bushAttr);
      this.safePut(x + 2, y, GLYPH.LOWER_HALF, bushAttr);
    } else if (distance > 1.5) {
      // Close: 5 wide, 2 tall - round bush
      this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, bushAttr);
      this.safePut(x, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
      this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
      this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, bushAttr);
      this.safePut(x - 2, y, GLYPH.LOWER_HALF, bushAttr);
      this.safePut(x - 1, y, GLYPH.FULL_BLOCK, bushAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, bushLightAttr);
      this.safePut(x + 2, y, GLYPH.FULL_BLOCK, bushAttr);
      this.safePut(x + 3, y, GLYPH.LOWER_HALF, bushAttr);
    } else {
      // Very close: 6 wide, 2 tall - big round bush
      this.safePut(x - 2, y - 1, GLYPH.UPPER_HALF, bushAttr);
      this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
      this.safePut(x, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
      this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
      this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, bushAttr);
      this.safePut(x + 3, y - 1, GLYPH.UPPER_HALF, bushAttr);
      this.safePut(x - 2, y, GLYPH.LOWER_HALF, bushAttr);
      this.safePut(x - 1, y, GLYPH.FULL_BLOCK, bushAttr);
      this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
      this.safePut(x + 1, y, GLYPH.FULL_BLOCK, bushLightAttr);
      this.safePut(x + 2, y, GLYPH.FULL_BLOCK, bushAttr);
      this.safePut(x + 3, y, GLYPH.LOWER_HALF, bushAttr);
    }
  }

  /**
   * Safely put a cell if in bounds.
   */
  private safePut(x: number, y: number, char: string, attr: number): void {
    if (x >= 0 && x < 80 && y >= this.horizonY && y <= this.roadBottom) {
      this.composer.setCell(x, y, char, attr);
    }
  }

  /**
   * Simple pseudo-random function.
   */
  private pseudoRandom(seed: number): number {
    var x = Math.sin(seed * 12.9898) * 43758.5453;
    return x - Math.floor(x);
  }

  /**
   * Get shade character for distance.
   */
  private getShadeForDistance(distance: number): string {
    if (distance < 5) return GLYPH.FULL_BLOCK;
    if (distance < 15) return GLYPH.DARK_SHADE;
    if (distance < 30) return GLYPH.MEDIUM_SHADE;
    return GLYPH.LIGHT_SHADE;
  }
}
