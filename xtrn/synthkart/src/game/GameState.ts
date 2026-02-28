/**
 * GameState - Container for all game state.
 */

/**
 * Race mode types.
 */
enum RaceMode {
  TIME_TRIAL = 'time_trial',   // Solo, just beat your own times
  GRAND_PRIX = 'grand_prix'    // Race against 7 CPU opponents
}

/**
 * Race result for a single racer.
 */
interface RaceResult {
  vehicleId: number;
  name: string;
  position: number;
  totalTime: number;
  bestLap: number;
  lapTimes: number[];
  isPlayer: boolean;
}

/**
 * Cup result after completing a series of races.
 */
interface CupResult {
  trackResults: RaceResult[][];  // Results for each track
  standings: { name: string; points: number; isPlayer: boolean }[];
  trophy: 'gold' | 'silver' | 'bronze' | 'none';
}

interface GameState {
  /** Current track (legacy, for checkpoint system) */
  track: ITrack;
  
  /** Track definition (for ID and metadata) */
  trackDefinition: TrackDefinition;

  /** Road segments for pseudo-3D rendering and physics */
  road: Road;

  /** All vehicles in the race */
  vehicles: IVehicle[];

  /** Player's vehicle (also in vehicles array) */
  playerVehicle: IVehicle;

  /** Item system state */
  items: Item[];

  /** Game time in seconds */
  time: number;

  /** Whether race is in progress */
  racing: boolean;

  /** Whether race is finished */
  finished: boolean;

  /** Current camera X offset */
  cameraX: number;
  
  /** Race mode */
  raceMode: RaceMode;
  
  /** Current lap start time (for lap time tracking) */
  lapStartTime: number;
  
  /** Best lap time achieved */
  bestLapTime: number;
  
  /** All lap times for current race */
  lapTimes: number[];
  
  /** Race results (populated when race finishes) */
  raceResults: RaceResult[];
  
  /** Countdown timer before race starts (-1 if race has started) */
  countdown: number;
  
  /** Whether the race has officially started (after countdown) */
  raceStarted: boolean;
}

/**
 * Create initial game state.
 */
function createInitialState(track: ITrack, trackDef: TrackDefinition, road: Road, playerVehicle: IVehicle, raceMode?: RaceMode): GameState {
  return {
    track: track,
    trackDefinition: trackDef,
    road: road,
    vehicles: [playerVehicle],
    playerVehicle: playerVehicle,
    items: [],
    time: 0,
    racing: false,
    finished: false,
    cameraX: 0,
    raceMode: raceMode || RaceMode.TIME_TRIAL,
    lapStartTime: 0,
    bestLapTime: -1,
    lapTimes: [],
    raceResults: [],
    countdown: 3,  // 3 second countdown
    raceStarted: false
  };
}

