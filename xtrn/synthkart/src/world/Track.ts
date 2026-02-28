/**
 * Track - Race track definition.
 */

interface Checkpoint {
  /** Z position along track */
  z: number;
}

interface SpawnPoint {
  /** Lateral offset from centerline */
  x: number;
  /** Distance behind start line */
  z: number;
}

interface SceneryHints {
  /** Sky appearance */
  sky: {
    type: 'sunset' | 'night' | 'day';
    sunAzimuth: number;
    horizonColors: string[];
  };
  /** Roadside prop density */
  props: {
    palmTrees: number;
    buildings: number;
    billboards: number;
  };
  /** Road appearance */
  road: {
    color: string;
    stripeColor: string;
    stripeWidth: number;
  };
  /** Background skyline */
  skyline: {
    style: 'city' | 'mountains' | 'ocean' | 'desert';
    density: number;
  };
}

interface ITrack {
  /** Track display name */
  name: string;

  /** Centerline as array of 2D points */
  centerline: Point2D[];

  /** Road width in world units */
  width: number;

  /** Total track length */
  length: number;

  /** Checkpoints to validate lap completion */
  checkpoints: Checkpoint[];

  /** Starting grid positions */
  spawnPoints: SpawnPoint[];

  /** Number of laps */
  laps: number;

  /** Visual theming */
  scenery: SceneryHints;

  /** Get centerline X position at given Z */
  getCenterlineX(z: number): number;
}

/**
 * Track implementation.
 */
class Track implements ITrack {
  name: string;
  centerline: Point2D[];
  width: number;
  length: number;
  checkpoints: Checkpoint[];
  spawnPoints: SpawnPoint[];
  laps: number;
  scenery: SceneryHints;

  constructor() {
    this.name = "Unnamed Track";
    this.centerline = [];
    this.width = 40;
    this.length = 0;
    this.checkpoints = [];
    this.spawnPoints = [];
    this.laps = 3;
    this.scenery = {
      sky: {
        type: 'sunset',
        sunAzimuth: 270,
        horizonColors: ['#ff00ff', '#ff6600', '#ffff00']
      },
      props: {
        palmTrees: 0.5,
        buildings: 0.3,
        billboards: 0.2
      },
      road: {
        color: '#333333',
        stripeColor: '#ffffff',
        stripeWidth: 2
      },
      skyline: {
        style: 'city',
        density: 0.5
      }
    };
  }

  /**
   * Get centerline X position at given Z.
   * Linear interpolation between centerline points.
   */
  getCenterlineX(z: number): number {
    if (this.centerline.length < 2) return 0;

    // Wrap Z to track length
    var wrappedZ = z % this.length;
    if (wrappedZ < 0) wrappedZ += this.length;

    // Find segment (simplified - assumes evenly spaced points)
    var segmentLength = this.length / this.centerline.length;
    var segmentIndex = Math.floor(wrappedZ / segmentLength);
    var t = (wrappedZ % segmentLength) / segmentLength;

    var p1 = this.centerline[segmentIndex];
    var p2 = this.centerline[(segmentIndex + 1) % this.centerline.length];

    return lerp(p1.x, p2.x, t);
  }

  /**
   * Get track direction (radians) at given Z.
   */
  getDirection(z: number): number {
    if (this.centerline.length < 2) return 0;

    var segmentLength = this.length / this.centerline.length;
    var segmentIndex = Math.floor((z % this.length) / segmentLength);

    var p1 = this.centerline[segmentIndex];
    var p2 = this.centerline[(segmentIndex + 1) % this.centerline.length];

    return Math.atan2(p2.y - p1.y, p2.x - p1.x);
  }
}
