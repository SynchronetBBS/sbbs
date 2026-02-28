/**
 * Cup.ts - Cup/Circuit race series management.
 * 
 * A Cup is a series of races where points are awarded based on finishing position.
 * Total points determine the cup winner.
 */

/** Points awarded for each finishing position (1st through 10th) */
var CUP_POINTS = [15, 12, 10, 8, 6, 5, 4, 3, 2, 1];

/** Get points for a finishing position (1-indexed) */
function getPointsForPosition(position: number): number {
  if (position < 1 || position > CUP_POINTS.length) return 0;
  return CUP_POINTS[position - 1];
}

/** Racer's standing in the cup */
interface CupRacerStanding {
  id: number;
  name: string;
  isPlayer: boolean;
  points: number;
  /** Position in each race (1-indexed, 0 = DNF) */
  raceResults: number[];
}

/** Result of a single race in a cup */
interface CupRaceResult {
  trackId: string;
  trackName: string;
  /** Final positions for all racers, indexed by racer ID */
  positions: { [racerId: number]: number };
  /** Player's race time in seconds */
  playerTime: number;
  /** Player's best lap time in seconds */
  playerBestLap: number;
}

/** Cup definition - which tracks are in the cup */
interface CupDefinition {
  id: string;
  name: string;
  trackIds: string[];
  description?: string;
}

/** Current state of a cup in progress */
interface CupState {
  definition: CupDefinition;
  currentRaceIndex: number;
  standings: CupRacerStanding[];
  raceResults: CupRaceResult[];
  /** Total elapsed time for the circuit */
  totalTime: number;
  /** Sum of best laps across all races */
  totalBestLaps: number;
  isComplete: boolean;
}

/**
 * Cup manager - handles cup state and progression.
 */
class CupManager {
  private state: CupState | null = null;
  
  /** Predefined cups */
  static readonly CUPS: CupDefinition[] = [
    {
      id: 'neon_cup',
      name: 'Neon Cup',
      trackIds: ['neon_coast', 'twilight_forest', 'sunset_beach'],
      description: 'A tour through the neon-lit night'
    },
    {
      id: 'nature_cup',
      name: 'Nature Cup',
      trackIds: ['twilight_forest', 'sunset_beach', 'cactus_canyon', 'tropical_jungle'],
      description: 'Race through nature\'s finest'
    },
    {
      id: 'spooky_cup',
      name: 'Spooky Cup',
      trackIds: ['haunted_hollow', 'dark_castle', 'ancient_ruins'],
      description: 'Face your fears on the track'
    },
    {
      id: 'fantasy_cup',
      name: 'Fantasy Cup',
      trackIds: ['candy_land', 'rainbow_road', 'dark_castle', 'kaiju_rampage'],
      description: 'A journey through imagination'
    },
    {
      id: 'grand_prix',
      name: 'Grand Prix',
      trackIds: ['neon_coast', 'sunset_beach', 'twilight_forest', 'haunted_hollow', 
                 'cactus_canyon', 'tropical_jungle', 'rainbow_road', 'thunder_stadium'],
      description: 'The ultimate challenge - 8 tracks!'
    }
  ];
  
  /**
   * Start a new cup.
   */
  startCup(cupDef: CupDefinition, racerNames: string[]): void {
    // Initialize standings for all racers
    var standings: CupRacerStanding[] = [];
    
    // Player is always ID 1
    standings.push({
      id: 1,
      name: 'YOU',
      isPlayer: true,
      points: 0,
      raceResults: []
    });
    
    // AI racers (IDs 2-8 typically)
    for (var i = 0; i < racerNames.length; i++) {
      standings.push({
        id: i + 2,
        name: racerNames[i],
        isPlayer: false,
        points: 0,
        raceResults: []
      });
    }
    
    this.state = {
      definition: cupDef,
      currentRaceIndex: 0,
      standings: standings,
      raceResults: [],
      totalTime: 0,
      totalBestLaps: 0,
      isComplete: false
    };
  }
  
  /**
   * Get current cup state.
   */
  getState(): CupState | null {
    return this.state;
  }
  
  /**
   * Get the track ID for the current race.
   */
  getCurrentTrackId(): string | null {
    if (!this.state) return null;
    if (this.state.currentRaceIndex >= this.state.definition.trackIds.length) return null;
    return this.state.definition.trackIds[this.state.currentRaceIndex];
  }
  
  /**
   * Get current race number (1-indexed for display).
   */
  getCurrentRaceNumber(): number {
    if (!this.state) return 0;
    return this.state.currentRaceIndex + 1;
  }
  
  /**
   * Get total number of races in this cup.
   */
  getTotalRaces(): number {
    if (!this.state) return 0;
    return this.state.definition.trackIds.length;
  }
  
  /**
   * Record the results of a completed race.
   */
  recordRaceResult(
    trackId: string,
    trackName: string,
    racerPositions: { id: number; position: number }[],
    playerTime: number,
    playerBestLap: number
  ): void {
    if (!this.state) return;
    
    // Build positions map
    var positions: { [racerId: number]: number } = {};
    for (var i = 0; i < racerPositions.length; i++) {
      var rp = racerPositions[i];
      positions[rp.id] = rp.position;
    }
    
    // Store race result
    var result: CupRaceResult = {
      trackId: trackId,
      trackName: trackName,
      positions: positions,
      playerTime: playerTime,
      playerBestLap: playerBestLap
    };
    this.state.raceResults.push(result);
    
    // Update standings
    for (var j = 0; j < this.state.standings.length; j++) {
      var standing = this.state.standings[j];
      var position = positions[standing.id] || 8;  // DNF = last place
      standing.raceResults.push(position);
      standing.points += getPointsForPosition(position);
    }
    
    // Update times
    this.state.totalTime += playerTime;
    this.state.totalBestLaps += playerBestLap;
    
    // Sort standings by points (descending)
    this.state.standings.sort(function(a, b) {
      return b.points - a.points;
    });
    
    // Advance to next race
    this.state.currentRaceIndex++;
    
    // Check if cup is complete
    if (this.state.currentRaceIndex >= this.state.definition.trackIds.length) {
      this.state.isComplete = true;
    }
  }
  
  /**
   * Get current standings sorted by points.
   */
  getStandings(): CupRacerStanding[] {
    if (!this.state) return [];
    return this.state.standings.slice();  // Return copy
  }
  
  /**
   * Get the player's current cup position (1-indexed).
   */
  getPlayerCupPosition(): number {
    if (!this.state) return 0;
    for (var i = 0; i < this.state.standings.length; i++) {
      if (this.state.standings[i].isPlayer) {
        return i + 1;
      }
    }
    return 0;
  }
  
  /**
   * Check if the cup is complete.
   */
  isCupComplete(): boolean {
    return this.state ? this.state.isComplete : false;
  }
  
  /**
   * Check if player won the cup.
   */
  didPlayerWin(): boolean {
    if (!this.state || !this.state.isComplete) return false;
    return this.state.standings[0].isPlayer;
  }
  
  /**
   * Get player's final points.
   */
  getPlayerPoints(): number {
    if (!this.state) return 0;
    for (var i = 0; i < this.state.standings.length; i++) {
      if (this.state.standings[i].isPlayer) {
        return this.state.standings[i].points;
      }
    }
    return 0;
  }
  
  /**
   * Clear cup state (return to menu).
   */
  clear(): void {
    this.state = null;
  }
}
