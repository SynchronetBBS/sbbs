/**
 * Minimap - Calculates minimap positions.
 */

interface MinimapConfig {
  x: number;      // Screen X position
  y: number;      // Screen Y position
  width: number;  // Width in characters
  height: number; // Height in characters
}

interface MinimapVehicle {
  x: number;
  y: number;
  isPlayer: boolean;
  color: number;
}

class Minimap {
  private config: MinimapConfig;
  private scaleX: number;
  private scaleY: number;
  private offsetX: number;
  private offsetY: number;

  constructor(config: MinimapConfig) {
    this.config = config;
    this.scaleX = 1;
    this.scaleY = 1;
    this.offsetX = 0;
    this.offsetY = 0;
  }

  /**
   * Calculate scale factors for track.
   */
  initForTrack(track: ITrack): void {
    // Find track bounds
    var minX = Infinity, maxX = -Infinity;
    var minY = Infinity, maxY = -Infinity;

    for (var i = 0; i < track.centerline.length; i++) {
      var p = track.centerline[i];
      if (p.x < minX) minX = p.x;
      if (p.x > maxX) maxX = p.x;
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    }

    var trackWidth = maxX - minX;
    var trackHeight = maxY - minY;

    // Calculate scale to fit in minimap
    var mapInnerW = this.config.width - 2;
    var mapInnerH = this.config.height - 2;

    this.scaleX = mapInnerW / (trackWidth || 1);
    this.scaleY = mapInnerH / (trackHeight || 1);

    // Use smaller scale to maintain aspect ratio
    var scale = Math.min(this.scaleX, this.scaleY);
    this.scaleX = scale;
    this.scaleY = scale;

    // Center offset
    this.offsetX = -minX;
    this.offsetY = -minY;
  }

  /**
   * Convert world position to minimap position.
   */
  worldToMinimap(worldX: number, worldY: number): { x: number; y: number } {
    return {
      x: Math.round((worldX + this.offsetX) * this.scaleX) + this.config.x + 1,
      y: Math.round((worldY + this.offsetY) * this.scaleY) + this.config.y + 1
    };
  }

  /**
   * Get vehicle positions for minimap.
   */
  getVehiclePositions(vehicles: IVehicle[], track: ITrack, playerId: number): MinimapVehicle[] {
    var result: MinimapVehicle[] = [];

    for (var i = 0; i < vehicles.length; i++) {
      var v = vehicles[i];
      // Convert track Z to centerline position
      var progress = (v.z % track.length) / track.length;
      var centerlineIdx = Math.floor(progress * track.centerline.length);
      var centerPoint = track.centerline[centerlineIdx] || { x: 0, y: 0 };

      var pos = this.worldToMinimap(centerPoint.x + v.x * 0.1, centerPoint.y);

      result.push({
        x: pos.x,
        y: pos.y,
        isPlayer: v.id === playerId,
        color: v.color
      });
    }

    return result;
  }

  /**
   * Get minimap config for rendering.
   */
  getConfig(): MinimapConfig {
    return this.config;
  }
}
