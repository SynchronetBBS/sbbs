/**
 * Vehicle - A racing vehicle for pseudo-3D racing.
 *
 * PSEUDO-3D RACER PHYSICS:
 * In classic sprite-scaled racers (OutRun, Pole Position), the car
 * doesn't turn - the ROAD curves. The player's position is measured
 * relative to the road centerline.
 *
 * Key concepts:
 * - playerX: position from -1.0 (left edge) to +1.0 (right edge)
 * - speed: how fast you move along the Z axis (down the road)
 * - Road curvature pushes you toward the outside of curves
 * - Steering counteracts this centrifugal drift
 * - Off-road (|playerX| > 1.0) slows you down dramatically
 */

interface IVehicle extends IEntity {
  /** Current speed (0 to maxSpeed) */
  speed: number;

  /** Player position relative to road center (-1 to 1 = on road) */
  playerX: number;

  /** Position along the track (Z world units) */
  trackZ: number;

  /** The driver controlling this vehicle */
  driver: IDriver | null;

  /** Current lap (1-indexed) */
  lap: number;

  /** Last checkpoint index passed */
  checkpoint: number;

  /** Race position (1 = first) */
  racePosition: number;

  /** Held item data (type + uses remaining), or null */
  heldItem: HeldItemData | null;
  
  /** Active effects on this vehicle (star, lightning, etc.) */
  activeEffects: ActiveEffect[];
  
  /** Check if vehicle has an active effect of given type */
  hasEffect(type: ItemType): boolean;

  /** Vehicle color for rendering */
  color: number;

  /** Is the vehicle off the road? */
  isOffRoad: boolean;

  /** Is the vehicle crashed (recovering)? */
  isCrashed: boolean;

  /** Crash recovery timer */
  crashTimer: number;

  /** Flash timer for collision/reset visual feedback */
  flashTimer: number;
  
  /** Boost timer (seconds remaining) */
  boostTimer: number;
  
  /** Boost speed multiplier (1.0 = normal) */
  boostMultiplier: number;
  
  /** Minimum speed during boost (prevents deceleration from key issues) */
  boostMinSpeed: number;
  
  /** Is this an NPC (commuter/AI) vehicle? */
  isNPC: boolean;
  
  /** Is this a competitive racer (vs commuter traffic)? */
  isRacer: boolean;
  
  /** NPC vehicle type for sprite selection */
  npcType: string;
  
  /** NPC color index for sprite color */
  npcColorIndex: number;

  /** Player car ID (from CarCatalog) */
  carId: string;
  
  /** Player car color ID (from CarCatalog) */
  carColorId: string;

  /** Update vehicle with road data */
  updatePhysics(road: Road, intent: DriverIntent, dt: number): void;
}

/**
 * Vehicle physics tuning for pseudo-3D racing.
 * These values are tuned for the feel of classic arcade racers.
 */
var VEHICLE_PHYSICS = {
  // Speed settings (world units per second)
  MAX_SPEED: 300,         // Top speed (segments per second * segment length)
  ACCEL: 150,             // Acceleration rate
  BRAKE: 250,             // Braking rate  
  DECEL: 20,              // Natural deceleration when coasting (cruise control)
  OFFROAD_DECEL: 200,     // Heavy slowdown when off-road
  
  // Steering (how fast playerX changes per second)
  STEER_RATE: 2.0,        // Base steering rate
  STEER_SPEED_FACTOR: 0.3,// Steering reduced by this % at max speed
  
  // Centrifugal force - how much curves push you outward  
  CENTRIFUGAL: 0.6,       // Multiplied by speed ratio and curve
  
  // Road boundaries
  ROAD_HALF_WIDTH: 1.0,   // playerX = +/- 1.0 is road edge
  OFFROAD_LIMIT: 1.8,     // Crash if |playerX| exceeds this
  
  // Crash recovery
  CRASH_TIME: 1.5,        // Seconds to recover from crash
};

/**
 * Vehicle implementation for pseudo-3D racing.
 */
class Vehicle extends Entity implements IVehicle {
  speed: number;
  playerX: number;
  trackZ: number;
  driver: IDriver | null;
  lap: number;
  checkpoint: number;
  racePosition: number;
  heldItem: HeldItemData | null;
  activeEffects: ActiveEffect[];
  color: number;
  isOffRoad: boolean;
  isCrashed: boolean;
  crashTimer: number;
  flashTimer: number;
  boostTimer: number;
  boostMultiplier: number;
  boostMinSpeed: number;
  isNPC: boolean;
  isRacer: boolean;
  npcType: string;
  npcColorIndex: number;
  carId: string;
  carColorId: string;

  constructor() {
    super();
    this.speed = 0;
    this.playerX = 0;      // Centered on road
    this.trackZ = 0;       // Start of track
    this.x = 0;            // Screen X (derived from playerX)
    this.z = 0;            // Screen Z (same as trackZ)
    this.driver = null;
    this.lap = 1;
    this.checkpoint = 0;
    this.racePosition = 1;
    this.heldItem = null;
    this.activeEffects = [];
    this.color = YELLOW;
    this.isOffRoad = false;
    this.isCrashed = false;
    this.crashTimer = 0;
    this.flashTimer = 0;
    this.boostTimer = 0;
    this.boostMultiplier = 1.0;
    this.boostMinSpeed = 0;
    this.isNPC = false;
    this.isRacer = false;
    this.npcType = 'sedan';
    this.npcColorIndex = 0;
    this.carId = 'sports';           // Default car
    this.carColorId = 'yellow';      // Default color
  }
  
  /**
   * Check if vehicle has an active effect of given type.
   */
  hasEffect(type: ItemType): boolean {
    for (var i = 0; i < this.activeEffects.length; i++) {
      if (this.activeEffects[i].type === type) return true;
    }
    return false;
  }
  
  /**
   * Add an active effect to this vehicle.
   */
  addEffect(type: ItemType, duration: number, sourceId: number): void {
    // Remove existing effect of same type first
    this.removeEffect(type);
    this.activeEffects.push({ type: type, duration: duration, sourceVehicleId: sourceId });
    
    // Handle immediate effect application
    if (type === ItemType.LIGHTNING) {
      // Lightning strike triggers a crash
      this.triggerCrash();
      logInfo("Vehicle " + this.id + " struck by lightning - crashed!");
    }
  }
  
  /**
   * Remove an active effect by type.
   */
  removeEffect(type: ItemType): void {
    for (var i = this.activeEffects.length - 1; i >= 0; i--) {
      if (this.activeEffects[i].type === type) {
        this.activeEffects.splice(i, 1);
      }
    }
  }
  
  /**
   * Update active effects (countdown timers).
   * Lightning effect pauses countdown while crashed (so slowdown phase starts after crash recovery).
   */
  updateEffects(dt: number): void {
    for (var i = this.activeEffects.length - 1; i >= 0; i--) {
      var effect = this.activeEffects[i];
      
      // Lightning effect pauses during crash - the slowdown should happen AFTER recovery
      if (effect.type === ItemType.LIGHTNING && this.isCrashed) {
        continue;  // Don't count down while crashed
      }
      
      effect.duration -= dt;
      if (effect.duration <= 0) {
        // Effect expired - handle cleanup
        var expiredType = effect.type;
        this.activeEffects.splice(i, 1);
        this.onEffectExpired(expiredType);
      }
    }
  }
  
  /**
   * Called when an effect expires.
   */
  private onEffectExpired(type: ItemType): void {
    switch (type) {
      case ItemType.STAR:
        // Reset boost (Star clears slot immediately, so no held item to clear)
        this.boostTimer = 0;
        this.boostMultiplier = 1.0;
        this.boostMinSpeed = 0;
        logInfo("Star effect expired");
        break;
        
      case ItemType.MUSHROOM_GOLDEN:
      case ItemType.BULLET:
        // These items stay in slot while active - clear them now
        this.boostTimer = 0;
        this.boostMultiplier = 1.0;
        this.boostMinSpeed = 0;
        // Clear the held item since the effect is over
        if (this.heldItem !== null && this.heldItem.type === type) {
          this.heldItem = null;
          logInfo(ItemType[type] + " effect expired - item cleared from slot");
        }
        break;
        
      case ItemType.LIGHTNING:
        // Speed returns to normal (handled by effect check)
        logInfo("Lightning effect expired");
        break;
    }
  }
  /**
   * Update vehicle physics for one frame.
   * This is the core pseudo-3D racer physics.
   */
  updatePhysics(road: Road, intent: DriverIntent, dt: number): void {
    // --- ACTIVE EFFECTS ---
    this.updateEffects(dt);
    
    // --- FLASH TIMER (visual feedback) ---
    if (this.flashTimer > 0) {
      this.flashTimer -= dt;
      if (this.flashTimer < 0) this.flashTimer = 0;
    }
    
    // --- BOOST TIMER ---
    // Only reset boost if no active duration effects (Star, Bullet, Golden Mushroom)
    if (this.boostTimer > 0) {
      this.boostTimer -= dt;
      if (this.boostTimer <= 0) {
        this.boostTimer = 0;
        // Don't reset boost if we have an active duration effect providing it
        if (!this.hasEffect(ItemType.STAR) && 
            !this.hasEffect(ItemType.BULLET) && 
            !this.hasEffect(ItemType.MUSHROOM_GOLDEN)) {
          this.boostMultiplier = 1.0;
          this.boostMinSpeed = 0;
        }
      }
    }
    
    // --- LIGHTNING SLOWDOWN ---
    // If hit by lightning, reduce max speed significantly
    var lightningSlowdown = this.hasEffect(ItemType.LIGHTNING) ? 0.5 : 1.0;
    
    // --- CRASH RECOVERY ---
    if (this.isCrashed) {
      this.crashTimer -= dt;
      if (this.crashTimer <= 0) {
        this.isCrashed = false;
        this.crashTimer = 0;
        this.playerX = 0;  // Reset to center after crash
        this.flashTimer = 0.5;  // Flash when recovering
      }
      return;  // No control during crash
    }

    // --- ACCELERATION / BRAKING ---
    if (intent.accelerate > 0) {
      this.speed += VEHICLE_PHYSICS.ACCEL * dt;
    } else if (intent.accelerate < 0) {
      this.speed -= VEHICLE_PHYSICS.BRAKE * dt;
    } else {
      // Cruise control - very gradual slowdown
      this.speed -= VEHICLE_PHYSICS.DECEL * dt;
    }

    // --- OFF-ROAD DETECTION & SLOWDOWN ---
    this.isOffRoad = Math.abs(this.playerX) > VEHICLE_PHYSICS.ROAD_HALF_WIDTH;
    if (this.isOffRoad) {
      this.speed -= VEHICLE_PHYSICS.OFFROAD_DECEL * dt;
      
      // Flash while off-road to indicate collision with terrain
      if (this.flashTimer <= 0) {
        this.flashTimer = 0.15;  // Quick flashes while off-road
      }
      
      // If we've slowed to a near stop while off-road, reset to center
      if (this.speed < 10) {
        this.speed = 0;
        this.playerX = 0;  // Snap back to center of road
        this.isOffRoad = false;
        this.flashTimer = 0.5;  // Longer flash on reset
      }
    }

    // Clamp speed (can't go negative or over max)
    // Apply boost multiplier to max speed when boosting
    // Apply lightning slowdown if affected
    var effectiveMaxSpeed = VEHICLE_PHYSICS.MAX_SPEED * this.boostMultiplier * lightningSlowdown;
    
    // Enforce minimum speed during boost (prevents deceleration from key handling issues)
    var minSpeed = this.boostMinSpeed > 0 ? this.boostMinSpeed : 0;
    this.speed = clamp(this.speed, minSpeed, effectiveMaxSpeed);

    // Speed ratio is used for steering and centrifugal force
    var speedRatio = this.speed / VEHICLE_PHYSICS.MAX_SPEED;

    // --- BULLET AUTOPILOT ---
    // When Bullet is active, auto-steer toward center of road and ignore player input
    var hasBullet = this.hasEffect(ItemType.BULLET);
    if (hasBullet) {
      // Auto-steer toward center (playerX = 0)
      var autoPilotRate = 3.0;  // Faster than normal steering
      if (this.playerX < -0.05) {
        this.playerX += autoPilotRate * dt;
        if (this.playerX > 0) this.playerX = 0;
      } else if (this.playerX > 0.05) {
        this.playerX -= autoPilotRate * dt;
        if (this.playerX < 0) this.playerX = 0;
      }
      // Skip normal steering and centrifugal force
    } else {
      // --- STEERING ---
      // Can't steer effectively without forward motion
      // At 0 speed, steering should do nothing
      if (this.speed >= 5) {
        // Steering effectiveness decreases at high speed
        var steerMult = 1.0 - (speedRatio * VEHICLE_PHYSICS.STEER_SPEED_FACTOR);
        var steerDelta = intent.steer * VEHICLE_PHYSICS.STEER_RATE * steerMult * dt;
        this.playerX += steerDelta;
      }

      // --- CENTRIFUGAL FORCE ---
      // Road curves push you toward the outside
      // This is what makes steering necessary on curves!
      var curve = road.getCurvature(this.trackZ);
      var centrifugal = curve * speedRatio * VEHICLE_PHYSICS.CENTRIFUGAL * dt;
      this.playerX += centrifugal;
    }

    // --- CRASH CHECK ---
    // Invincible vehicles (Star/Bullet) can't crash from going off road
    var isInvincible = this.hasEffect(ItemType.STAR) || hasBullet;
    if (Math.abs(this.playerX) > VEHICLE_PHYSICS.OFFROAD_LIMIT && !isInvincible) {
      this.triggerCrash();
      return;
    }
    // Clamp invincible vehicles to road limits instead of crashing
    if (isInvincible && Math.abs(this.playerX) > VEHICLE_PHYSICS.ROAD_HALF_WIDTH) {
      this.playerX = clamp(this.playerX, -VEHICLE_PHYSICS.ROAD_HALF_WIDTH, VEHICLE_PHYSICS.ROAD_HALF_WIDTH);
    }

    // --- ADVANCE ALONG TRACK ---
    this.trackZ += this.speed * dt;

    // Wrap trackZ when completing a lap
    if (this.trackZ >= road.totalLength) {
      this.trackZ = this.trackZ % road.totalLength;
    }

    // Sync Entity position for rendering
    this.z = this.trackZ;
    this.x = this.playerX * 20;  // Scale playerX to screen coordinates
  }

  /**
   * Trigger a crash - stop and recover.
   */
  triggerCrash(): void {
    debugLog.warn("CRASH! playerX=" + this.playerX.toFixed(3) + " (limit=" + VEHICLE_PHYSICS.OFFROAD_LIMIT + ")");
    debugLog.info("  trackZ=" + this.trackZ.toFixed(1) + " speed=" + this.speed.toFixed(1));
    debugLog.info("  Entering recovery for " + VEHICLE_PHYSICS.CRASH_TIME + " seconds");
    
    this.isCrashed = true;
    this.crashTimer = VEHICLE_PHYSICS.CRASH_TIME;
    this.speed = 0;
  }
}
