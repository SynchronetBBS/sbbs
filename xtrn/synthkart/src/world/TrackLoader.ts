/**
 * TrackLoader - Loads track data from JSON files.
 *
 * NOTE: For Iteration 0, this returns hardcoded data.
 * File I/O will be implemented in Iteration 1.
 * See ADR-0003 for the track JSON schema.
 */

class TrackLoader {
  // Reserved for future file I/O (Iteration 1)
  // private basePath: string;

  constructor() {
    // Tracks will be loaded from data/tracks/ subdirectory
    // Synchronet automatically resolves relative paths from script directory
    // TODO: Enable in Iteration 1
    // this.basePath = "data/tracks/";
  }

  /**
   * Load a track by ID.
   *
   * TODO (Iteration 1): Implement actual file loading:
   *   var f = new File(this.basePath + trackId + ".json");
   *   if (f.open("r")) {
   *     var content = f.read();
   *     f.close();
   *     return this.parseTrackJson(JSON.parse(content));
   *   }
   */
  load(trackId: string): ITrack {
    logInfo("TrackLoader: Loading track '" + trackId + "'");

    // For now, return hardcoded data matching neon_coast_01.json
    if (trackId === "neon_coast_01") {
      return this.loadNeonCoast();
    }

    // Fallback to default track
    logWarning("TrackLoader: Unknown track '" + trackId + "', using default");
    return this.loadNeonCoast();
  }

  /**
   * Hardcoded Neon Coast track for Iteration 0.
   */
  private loadNeonCoast(): ITrack {
    var track = new Track();
    track.name = "Neon Coast";
    track.width = 40;
    track.laps = 3;

    // Centerline forms a simple oval
    track.centerline = [
      { x: 0, y: 0 },
      { x: 100, y: 0 },
      { x: 200, y: 50 },
      { x: 250, y: 150 },
      { x: 200, y: 250 },
      { x: 100, y: 300 },
      { x: 0, y: 300 },
      { x: -100, y: 250 },
      { x: -150, y: 150 },
      { x: -100, y: 50 }
    ];

    // Calculate length from centerline
    track.length = this.calculateTrackLength(track.centerline);

    // Checkpoints
    track.checkpoints = [
      { z: 0 },
      { z: track.length * 0.25 },
      { z: track.length * 0.5 },
      { z: track.length * 0.75 }
    ];

    // Spawn points (8 positions in 2 columns)
    track.spawnPoints = [
      { x: -5, z: -10 },
      { x: 5, z: -10 },
      { x: -5, z: -25 },
      { x: 5, z: -25 },
      { x: -5, z: -40 },
      { x: 5, z: -40 },
      { x: -5, z: -55 },
      { x: 5, z: -55 }
    ];

    // Scenery
    track.scenery = {
      sky: {
        type: 'sunset',
        sunAzimuth: 270,
        horizonColors: ['#ff00ff', '#ff6600', '#ffff00']
      },
      props: {
        palmTrees: 0.7,
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
        density: 0.6
      }
    };

    return track;
  }

  /**
   * Calculate total track length from centerline points.
   */
  private calculateTrackLength(centerline: Point2D[]): number {
    var length = 0;
    for (var i = 0; i < centerline.length; i++) {
      var p1 = centerline[i];
      var p2 = centerline[(i + 1) % centerline.length];
      length += distance(p1, p2);
    }
    return length;
  }

  /**
   * Parse a track JSON object (for future file loading).
   * @ts-ignore Unused until Iteration 1
   */
  /* istanbul ignore next */
  parseTrackJson(data: any): ITrack {
    var track = new Track();

    // Meta
    track.name = data.meta?.name || "Unknown";
    track.laps = data.race?.laps || 3;

    // Geometry
    track.width = data.geometry?.width || 40;
    track.centerline = (data.geometry?.centerline || []).map(function(p: number[]) {
      return { x: p[0], y: p[1] };
    });
    track.length = data.geometry?.length || this.calculateTrackLength(track.centerline);

    // Race
    track.checkpoints = (data.race?.checkpoints || []).map(function(z: number) {
      return { z: z };
    });
    track.spawnPoints = data.race?.spawnPoints || [];

    // Scenery
    if (data.scenery) {
      track.scenery = data.scenery;
    }

    return track;
  }
}
