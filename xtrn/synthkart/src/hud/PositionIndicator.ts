/**
 * PositionIndicator - Race position display.
 */

interface PositionData {
  position: number;
  totalRacers: number;
  suffix: string;
}

class PositionIndicator {
  /**
   * Calculate position display data.
   */
  static calculate(position: number, totalRacers: number): PositionData {
    return {
      position: position,
      totalRacers: totalRacers,
      suffix: this.getOrdinalSuffix(position)
    };
  }

  /**
   * Get ordinal suffix for a number (1st, 2nd, 3rd, etc.)
   */
  static getOrdinalSuffix(n: number): string {
    var s = ["th", "st", "nd", "rd"];
    var v = n % 100;
    return s[(v - 20) % 10] || s[v] || s[0];
  }

  /**
   * Format position string (e.g., "3rd / 8")
   */
  static format(data: PositionData): string {
    return data.position + data.suffix + " / " + data.totalRacers;
  }

  /**
   * Calculate race positions for all vehicles.
   * Sorts by lap > track position.
   * Position 1 = first place (furthest along).
   */
  static calculatePositions(vehicles: IVehicle[]): void {
    // Only calculate positions for actual racers (player + AI racers), not commuter NPCs
    var racers = vehicles.filter(function(v) { return !v.isNPC || v.isRacer; });
    
    // Visual offset compensation: the player's visual position appears ahead of their trackZ
    // This offset corrects for the difference between where the player sees themselves
    // and where trackZ says they are. Without this, player shows as behind cars they've passed.
    var VISUAL_OFFSET = 200;  // World units - tune this value
    
    // Sort vehicles by race progress (best = highest lap, then highest trackZ)
    var sorted = racers.slice().sort(function(a, b) {
      // First by lap (higher = better)
      if (a.lap !== b.lap) return b.lap - a.lap;

      // Same lap: higher trackZ = further ahead = better position
      // Apply visual offset for non-NPC (player) vehicles
      var aZ = a.trackZ + (a.isNPC ? 0 : VISUAL_OFFSET);
      var bZ = b.trackZ + (b.isNPC ? 0 : VISUAL_OFFSET);
      return bZ - aZ;
    });

    // Assign positions (index 0 = position 1 = first place)
    for (var i = 0; i < sorted.length; i++) {
      sorted[i].racePosition = i + 1;
    }
  }
}
