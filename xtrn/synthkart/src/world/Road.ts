/**
 * Road - Segment-based road system for pseudo-3D racing.
 *
 * This is the classic "sprite-scaled racer" approach used by OutRun, Pole Position, etc.
 * The road is a series of segments projected from the player's viewpoint.
 * 
 * Key concepts:
 * - Each segment has a Z depth, curvature (dx), and hill (dy)
 * - The road "curves" by accumulating curvature across segments
 * - Player X position is relative to road center (-1 to 1 = road edges)
 * - Steering counteracts road curvature to stay on track
 */

interface RoadSegment {
  /** World Z position of this segment */
  z: number;
  
  /** Curvature: how much road curves left (<0) or right (>0) per segment */
  curve: number;
  
  /** Hill: vertical change (positive = uphill) - for future use */
  hill: number;
  
  /** Road width multiplier (1.0 = normal, can narrow/widen) */
  width: number;
  
  /** Segment color index for striping (0 or 1) */
  stripe: number;
  
  /** Scenery sprite ID on left side, or null */
  spriteLeft: string | null;
  
  /** Scenery sprite ID on right side, or null */
  spriteRight: string | null;
}

/**
 * Road class - manages the segment-based road.
 */
class Road {
  /** All road segments */
  segments: RoadSegment[];
  
  /** Length of each segment in world units */
  segmentLength: number;
  
  /** Base road width in world units */
  baseWidth: number;
  
  /** Total road length */
  totalLength: number;
  
  /** Number of laps */
  laps: number;
  
  /** Track name */
  name: string;

  constructor() {
    this.segments = [];
    this.segmentLength = 200;  // Each segment is 200 world units deep
    this.baseWidth = 2000;     // Road is 2000 world units wide
    this.totalLength = 0;
    this.laps = 3;
    this.name = "Unnamed Road";
  }

  /**
   * Get segment at a given Z position.
   */
  getSegment(z: number): RoadSegment {
    var index = Math.floor(z / this.segmentLength) % this.segments.length;
    if (index < 0) index += this.segments.length;
    return this.segments[index];
  }

  /**
   * Get segment index at a given Z position.
   */
  getSegmentIndex(z: number): number {
    var index = Math.floor(z / this.segmentLength) % this.segments.length;
    if (index < 0) index += this.segments.length;
    return index;
  }

  /**
   * Get the curvature at a given Z position.
   * This is used to calculate how much the player drifts.
   */
  getCurvature(z: number): number {
    return this.getSegment(z).curve;
  }

  /**
   * Check if position is on the road.
   * @param x Player X position (-1 to 1 = on road)
   * @param z Player Z position
   * @returns true if on road
   */
  isOnRoad(x: number, z: number): number {
    var segment = this.getSegment(z);
    var halfWidth = segment.width / 2;
    return Math.abs(x) <= halfWidth ? 1 : 0;
  }
}

/**
 * RoadBuilder - Fluent interface for creating roads.
 */
class RoadBuilder {
  private road: Road;
  private currentZ: number;
  private currentCurve: number;
  private stripeCounter: number;
  private stripeLength: number;

  constructor() {
    this.road = new Road();
    this.currentZ = 0;
    this.currentCurve = 0;
    this.stripeCounter = 0;
    this.stripeLength = 3; // Segments per stripe
  }

  /**
   * Set road name.
   */
  name(n: string): RoadBuilder {
    this.road.name = n;
    return this;
  }

  /**
   * Set number of laps.
   */
  laps(n: number): RoadBuilder {
    this.road.laps = n;
    return this;
  }

  /**
   * Add straight segments.
   */
  straight(numSegments: number): RoadBuilder {
    for (var i = 0; i < numSegments; i++) {
      this.addSegment(0);
    }
    return this;
  }

  /**
   * Add curved segments.
   * @param numSegments Number of segments
   * @param curve Curvature (-1 to 1, negative = left, positive = right)
   */
  curve(numSegments: number, curvature: number): RoadBuilder {
    for (var i = 0; i < numSegments; i++) {
      this.addSegment(curvature);
    }
    return this;
  }

  /**
   * Ease into a curve (gradual increase).
   */
  easeIn(numSegments: number, targetCurve: number): RoadBuilder {
    var startCurve = this.currentCurve;
    for (var i = 0; i < numSegments; i++) {
      var t = i / numSegments;
      this.addSegment(lerp(startCurve, targetCurve, t));
    }
    this.currentCurve = targetCurve;
    return this;
  }

  /**
   * Ease out of a curve (gradual decrease).
   */
  easeOut(numSegments: number): RoadBuilder {
    var startCurve = this.currentCurve;
    for (var i = 0; i < numSegments; i++) {
      var t = i / numSegments;
      this.addSegment(lerp(startCurve, 0, t));
    }
    this.currentCurve = 0;
    return this;
  }

  /**
   * Add a single segment.
   */
  private addSegment(curve: number): void {
    this.road.segments.push({
      z: this.currentZ,
      curve: curve,
      hill: 0,
      width: 1.0,
      stripe: Math.floor(this.stripeCounter / this.stripeLength) % 2,
      spriteLeft: null,
      spriteRight: null
    });
    this.currentZ += this.road.segmentLength;
    this.stripeCounter++;
  }

  /**
   * Build and return the road.
   */
  build(): Road {
    this.road.totalLength = this.currentZ;
    return this.road;
  }
}

/**
 * Create the Neon Coast track.
 */
function createNeonCoastRoad(): Road {
  return new RoadBuilder()
    .name("Neon Coast")
    .laps(3)
    // Start with a straight
    .straight(30)
    // Gentle right curve
    .easeIn(10, 0.4)
    .curve(30, 0.4)
    .easeOut(10)
    // Long straight
    .straight(40)
    // Sharp left curve
    .easeIn(8, -0.6)
    .curve(25, -0.6)
    .easeOut(8)
    // Medium straight
    .straight(25)
    // S-curve
    .easeIn(6, 0.5)
    .curve(15, 0.5)
    .easeOut(6)
    .easeIn(6, -0.5)
    .curve(15, -0.5)
    .easeOut(6)
    // Final straight to finish
    .straight(35)
    .build();
}
