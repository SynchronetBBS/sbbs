/**
 * CarCatalog - Defines player car types with stats and visual configurations.
 * 
 * Each car has:
 * - Visual style (different body shapes)
 * - Performance stats (top speed, acceleration, handling)
 * - Available color options
 * - Brake light positions for visual feedback
 */

/**
 * Car performance stats - all values are multipliers (1.0 = baseline).
 */
interface CarStats {
  /** Top speed multiplier (1.0 = 300 max speed) */
  topSpeed: number;
  /** Acceleration multiplier (1.0 = 150 accel) */
  acceleration: number;
  /** Handling/steering multiplier (1.0 = 2.0 steer rate) */
  handling: number;
}

/**
 * Car color configuration.
 */
interface CarColor {
  /** Unique color identifier */
  id: string;
  /** Display name */
  name: string;
  /** Primary body color (ANSI foreground) */
  body: number;
  /** Trim/accent color (ANSI foreground) */
  trim: number;
  /** Effect flash color (alternate from body for visibility) */
  effectFlash: number;
}

/**
 * Brake light cell position in sprite.
 */
interface BrakeLightCell {
  /** X position in sprite grid */
  x: number;
  /** Y position in sprite grid */
  y: number;
}

/**
 * Car body style - defines the sprite shape.
 */
type CarBodyStyle = 'sports' | 'muscle' | 'compact' | 'super' | 'classic';

/**
 * Full car definition.
 */
interface CarDefinition {
  /** Unique car identifier */
  id: string;
  /** Display name shown in selection */
  name: string;
  /** Short description */
  description: string;
  /** Body style for sprite selection */
  bodyStyle: CarBodyStyle;
  /** Performance stats */
  stats: CarStats;
  /** Available colors for this car */
  availableColors: string[];
  /** Default color ID */
  defaultColor: string;
  /** Positions of brake light cells in sprite */
  brakeLights: BrakeLightCell[];
  /** Whether this car is unlocked by default */
  unlocked: boolean;
  /** Unlock requirement description (if locked) */
  unlockHint?: string;
}

/**
 * Color palette - all safe high-ANSI colors that won't blend with track backgrounds.
 * Track backgrounds use low-ANSI (0-7) so high-ANSI (8-15) provides contrast.
 */
var CAR_COLORS: { [id: string]: CarColor } = {
  // Classic/default - high visibility
  'yellow': {
    id: 'yellow',
    name: 'Sunshine Yellow',
    body: YELLOW,
    trim: WHITE,
    effectFlash: LIGHTCYAN  // Flash cyan when yellow car has effect
  },
  'red': {
    id: 'red',
    name: 'Racing Red',
    body: LIGHTRED,
    trim: WHITE,
    effectFlash: YELLOW  // Flash yellow when red car has effect
  },
  'blue': {
    id: 'blue',
    name: 'Electric Blue',
    body: LIGHTBLUE,
    trim: LIGHTCYAN,
    effectFlash: YELLOW
  },
  'green': {
    id: 'green',
    name: 'Neon Green',
    body: LIGHTGREEN,
    trim: WHITE,
    effectFlash: LIGHTMAGENTA
  },
  'cyan': {
    id: 'cyan',
    name: 'Cyber Cyan',
    body: LIGHTCYAN,
    trim: WHITE,
    effectFlash: LIGHTMAGENTA
  },
  'magenta': {
    id: 'magenta',
    name: 'Synthwave Pink',
    body: LIGHTMAGENTA,
    trim: WHITE,
    effectFlash: LIGHTCYAN
  },
  'white': {
    id: 'white',
    name: 'Ghost White',
    body: WHITE,
    trim: LIGHTCYAN,
    effectFlash: YELLOW
  },
  'orange': {
    id: 'orange',
    name: 'Sunset Orange',
    body: YELLOW,  // YELLOW on black looks orange-ish in ANSI
    trim: LIGHTRED,
    effectFlash: LIGHTCYAN
  }
};

/**
 * Available car definitions.
 */
var CAR_CATALOG: CarDefinition[] = [
  {
    id: 'sports',
    name: 'TURBO GT',
    description: 'Balanced performance for all tracks',
    bodyStyle: 'sports',
    stats: {
      topSpeed: 1.0,
      acceleration: 1.0,
      handling: 1.0
    },
    availableColors: ['yellow', 'red', 'blue', 'green', 'cyan', 'magenta', 'white'],
    defaultColor: 'yellow',
    brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],  // Sides of body row
    unlocked: true
  },
  {
    id: 'muscle',
    name: 'THUNDER V8',
    description: 'Raw power, slower handling',
    bodyStyle: 'muscle',
    stats: {
      topSpeed: 1.15,      // Faster top speed
      acceleration: 1.1,   // Good acceleration
      handling: 0.85       // Slower steering
    },
    availableColors: ['red', 'yellow', 'blue', 'white', 'orange'],
    defaultColor: 'red',
    brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
    unlocked: true
  },
  {
    id: 'compact',
    name: 'SWIFT RS',
    description: 'Quick and nimble, lower top speed',
    bodyStyle: 'compact',
    stats: {
      topSpeed: 0.9,       // Lower top speed
      acceleration: 1.2,   // Quick acceleration
      handling: 1.25       // Excellent handling
    },
    availableColors: ['cyan', 'green', 'magenta', 'yellow', 'white'],
    defaultColor: 'cyan',
    brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
    unlocked: true
  },
  {
    id: 'super',
    name: 'PHANTOM X',
    description: 'Elite supercar - unlock to drive',
    bodyStyle: 'super',
    stats: {
      topSpeed: 1.2,       // Highest top speed
      acceleration: 1.15,  // Great acceleration
      handling: 1.1        // Good handling
    },
    availableColors: ['white', 'red', 'blue', 'magenta'],
    defaultColor: 'white',
    brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
    unlocked: false,
    unlockHint: 'Win a Grand Prix to unlock'
  },
  {
    id: 'classic',
    name: 'RETRO 86',
    description: 'Old school charm, balanced feel',
    bodyStyle: 'classic',
    stats: {
      topSpeed: 0.95,
      acceleration: 1.0,
      handling: 1.05
    },
    availableColors: ['yellow', 'red', 'white', 'green', 'blue'],
    defaultColor: 'yellow',
    brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
    unlocked: true
  }
];

/**
 * Get a car definition by ID.
 */
function getCarDefinition(carId: string): CarDefinition | null {
  for (var i = 0; i < CAR_CATALOG.length; i++) {
    if (CAR_CATALOG[i].id === carId) {
      return CAR_CATALOG[i];
    }
  }
  return null;
}

/**
 * Get a color definition by ID.
 */
function getCarColor(colorId: string): CarColor | null {
  return CAR_COLORS[colorId] || null;
}

/**
 * Get all unlocked cars.
 */
function getUnlockedCars(): CarDefinition[] {
  var unlocked: CarDefinition[] = [];
  for (var i = 0; i < CAR_CATALOG.length; i++) {
    if (CAR_CATALOG[i].unlocked) {
      unlocked.push(CAR_CATALOG[i]);
    }
  }
  return unlocked;
}

/**
 * Get all cars (for display in selection screen).
 */
function getAllCars(): CarDefinition[] {
  return CAR_CATALOG.slice();  // Return copy
}

/**
 * Unlock a car by ID (e.g., after winning Grand Prix).
 */
function unlockCar(carId: string): boolean {
  for (var i = 0; i < CAR_CATALOG.length; i++) {
    if (CAR_CATALOG[i].id === carId) {
      CAR_CATALOG[i].unlocked = true;
      return true;
    }
  }
  return false;
}

/**
 * Check if a car is unlocked.
 */
function isCarUnlocked(carId: string): boolean {
  var car = getCarDefinition(carId);
  return car !== null && car.unlocked;
}

/**
 * Get the effect flash color for a car+color combo.
 * This ensures effects are visible regardless of car color.
 */
function getEffectFlashColor(colorId: string): number {
  var color = getCarColor(colorId);
  return color ? color.effectFlash : LIGHTCYAN;
}

/**
 * Apply car stats to vehicle physics.
 * Call this when selecting a car to modify VEHICLE_PHYSICS constants.
 */
function applyCarStats(carId: string): void {
  var car = getCarDefinition(carId);
  if (!car) return;
  
  // Store base values (should be set once at init)
  if (typeof (VEHICLE_PHYSICS as any)._baseMaxSpeed === 'undefined') {
    (VEHICLE_PHYSICS as any)._baseMaxSpeed = VEHICLE_PHYSICS.MAX_SPEED;
    (VEHICLE_PHYSICS as any)._baseAccel = VEHICLE_PHYSICS.ACCEL;
    (VEHICLE_PHYSICS as any)._baseSteer = VEHICLE_PHYSICS.STEER_RATE;
  }
  
  // Apply multipliers
  VEHICLE_PHYSICS.MAX_SPEED = (VEHICLE_PHYSICS as any)._baseMaxSpeed * car.stats.topSpeed;
  VEHICLE_PHYSICS.ACCEL = (VEHICLE_PHYSICS as any)._baseAccel * car.stats.acceleration;
  VEHICLE_PHYSICS.STEER_RATE = (VEHICLE_PHYSICS as any)._baseSteer * car.stats.handling;
  
  logInfo('Applied car stats for ' + car.name + 
          ': speed=' + VEHICLE_PHYSICS.MAX_SPEED.toFixed(0) +
          ', accel=' + VEHICLE_PHYSICS.ACCEL.toFixed(0) +
          ', steer=' + VEHICLE_PHYSICS.STEER_RATE.toFixed(2));
}

/**
 * Reset vehicle physics to base values.
 */
function resetCarStats(): void {
  if (typeof (VEHICLE_PHYSICS as any)._baseMaxSpeed !== 'undefined') {
    VEHICLE_PHYSICS.MAX_SPEED = (VEHICLE_PHYSICS as any)._baseMaxSpeed;
    VEHICLE_PHYSICS.ACCEL = (VEHICLE_PHYSICS as any)._baseAccel;
    VEHICLE_PHYSICS.STEER_RATE = (VEHICLE_PHYSICS as any)._baseSteer;
  }
}
