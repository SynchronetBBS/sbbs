/**
 * Shell - Projectile items using vehicle-like track behavior.
 * 
 * Shells travel along the track like high-speed vehicles:
 * - RED SHELL: Homes toward next vehicle ahead, accelerates to ~500-600
 * - GREEN SHELL: Travels straight, can fire forward or backward
 * - BLUE SHELL: Homes toward 1st place vehicle, accelerates gradually
 * 
 * All shells start at minimum speed (300 = car max speed) and accelerate.
 * This reuses our pseudo-3D track system instead of true projectile physics.
 */

/** Shell types */
enum ShellType {
  GREEN = 0,  // Straight path, can hit anyone
  RED = 1,    // Homes to next vehicle ahead
  BLUE = 2    // Homes to 1st place
}

/** Shell speed constants */
var SHELL_PHYSICS = {
  MIN_SPEED: 300,           // Minimum shell speed (same as car max)
  GREEN_TARGET_SPEED: 400,  // Green shell target speed
  RED_TARGET_SPEED: 550,    // Red shell target speed (needs to catch up)
  BLUE_TARGET_SPEED: 700,   // Blue shell target speed (must reach 1st place)
  ACCELERATION: 200,        // How fast shells accelerate (units/sec^2)
  BACKWARD_SPEED: -250      // Backward shell speed (negative = behind vehicle)
};

interface IProjectile extends IEntity {
  /** Shell type (green/red/blue) */
  shellType: ShellType;

  /** Track position (like vehicle.trackZ) */
  trackZ: number;

  /** Lateral position (like vehicle.playerX) */
  playerX: number;

  /** Movement speed along track (negative = backward) */
  speed: number;
  
  /** Target speed to accelerate toward (optional, for shells) */
  targetSpeed?: number;

  /** ID of vehicle that fired this */
  ownerId: number;

  /** Target vehicle ID (for homing shells) */
  targetId: number;

  /** Time to live (despawn timer) */
  ttl: number;

  /** Has this shell hit something? */
  isDestroyed: boolean;
  
  /** Is this shell traveling backward? (optional, for shells) */
  isBackward?: boolean;
}

class Shell extends Item implements IProjectile {
  shellType: ShellType;
  trackZ: number;
  playerX: number;
  speed: number;
  targetSpeed: number;
  ownerId: number;
  targetId: number;
  ttl: number;
  isDestroyed: boolean;
  isBackward: boolean;

  constructor(shellType: ShellType) {
    super(ItemType.SHELL);
    this.shellType = shellType;
    this.trackZ = 0;
    this.playerX = 0;
    this.speed = SHELL_PHYSICS.MIN_SPEED;  // Start at minimum speed
    this.targetSpeed = SHELL_PHYSICS.GREEN_TARGET_SPEED;  // Default target
    this.ownerId = -1;
    this.targetId = -1;
    this.ttl = 10;  // 10 seconds max lifetime
    this.isDestroyed = false;
    this.isBackward = false;
  }

  /**
   * Fire a green shell (straight ahead or behind).
   * Direction based on whether player was braking (backward) or accelerating (forward).
   * @param vehicle - The vehicle firing the shell
   * @param backward - If true, fire behind the vehicle
   */
  static fireGreen(vehicle: IVehicle, backward: boolean = false): Shell {
    var shell = new Shell(ShellType.GREEN);
    shell.ownerId = vehicle.id;
    shell.isBackward = backward;
    
    if (backward) {
      // Fire behind: start behind vehicle, travel backward
      shell.trackZ = vehicle.trackZ - 15;
      shell.playerX = vehicle.playerX;
      shell.speed = SHELL_PHYSICS.BACKWARD_SPEED;  // Negative = backward
      shell.targetSpeed = SHELL_PHYSICS.BACKWARD_SPEED;
      logInfo("GREEN SHELL fired BACKWARD at speed=" + shell.speed.toFixed(0));
    } else {
      // Fire forward: start ahead, accelerate forward
      shell.trackZ = vehicle.trackZ + 15;
      shell.playerX = vehicle.playerX;
      shell.speed = SHELL_PHYSICS.MIN_SPEED;
      shell.targetSpeed = SHELL_PHYSICS.GREEN_TARGET_SPEED;
      logInfo("GREEN SHELL fired FORWARD, starting at speed=" + shell.speed.toFixed(0) + ", target=" + shell.targetSpeed);
    }
    return shell;
  }

  /**
   * Fire a red shell (homes to next vehicle ahead).
   * Red shells accelerate from minimum speed to target speed.
   */
  static fireRed(vehicle: IVehicle, vehicles: IVehicle[]): Shell {
    var shell = new Shell(ShellType.RED);
    shell.trackZ = vehicle.trackZ + 15;
    shell.playerX = vehicle.playerX;
    shell.ownerId = vehicle.id;
    shell.speed = SHELL_PHYSICS.MIN_SPEED;  // Start at minimum
    shell.targetSpeed = SHELL_PHYSICS.RED_TARGET_SPEED;  // Accelerate to this
    
    // Find next vehicle ahead
    shell.targetId = Shell.findNextVehicleAhead(vehicle, vehicles);
    logInfo("RED SHELL fired, starting at speed=" + shell.speed.toFixed(0) + ", target speed=" + shell.targetSpeed + ", homing to vehicle " + shell.targetId);
    return shell;
  }

  /**
   * Fire a blue shell (homes to 1st place).
   * Blue shell accelerates gradually to stay visible longer.
   * Does not despawn until hitting 1st place.
   */
  static fireBlue(vehicle: IVehicle, vehicles: IVehicle[]): Shell {
    var shell = new Shell(ShellType.BLUE);
    shell.trackZ = vehicle.trackZ + 15;
    shell.playerX = vehicle.playerX;
    shell.ownerId = vehicle.id;
    shell.speed = SHELL_PHYSICS.MIN_SPEED;  // Start at minimum, accelerate gradually
    shell.targetSpeed = SHELL_PHYSICS.BLUE_TARGET_SPEED;
    
    // Find 1st place vehicle
    shell.targetId = Shell.findFirstPlace(vehicles);
    logInfo("BLUE SHELL fired, starting at speed=" + shell.speed.toFixed(0) + ", target speed=" + shell.targetSpeed + ", homing to 1st place (vehicle " + shell.targetId + ")");
    return shell;
  }

  /**
   * Find the next vehicle ahead of the shooter.
   */
  static findNextVehicleAhead(shooter: IVehicle, vehicles: IVehicle[]): number {
    var bestId = -1;
    var bestDist = Infinity;
    
    for (var i = 0; i < vehicles.length; i++) {
      var v = vehicles[i];
      if (v.id === shooter.id) continue;
      
      // Must be ahead (higher trackZ, accounting for lap wrapping)
      var dist = v.trackZ - shooter.trackZ;
      if (dist > 0 && dist < bestDist) {
        bestDist = dist;
        bestId = v.id;
      }
    }
    return bestId;
  }

  /**
   * Find the vehicle in 1st place.
   */
  static findFirstPlace(vehicles: IVehicle[]): number {
    for (var i = 0; i < vehicles.length; i++) {
      if (vehicles[i].racePosition === 1) {
        return vehicles[i].id;
      }
    }
    return -1;
  }

  /**
   * Update shell position and check for hits.
   * Returns true if shell should be removed.
   */
  update(dt: number, vehicles: IVehicle[], roadLength: number): boolean {
    if (this.isDestroyed) return true;
    
    // Decrease TTL (but blue shells never despawn on timeout)
    this.ttl -= dt;
    if (this.ttl <= 0 && this.shellType !== ShellType.BLUE) {
      logInfo("Shell despawned (TTL)");
      return true;
    }
    
    // --- ACCELERATION ---
    // Shells accelerate toward their target speed
    if (!this.isBackward && this.speed < this.targetSpeed) {
      this.speed += SHELL_PHYSICS.ACCELERATION * dt;
      if (this.speed > this.targetSpeed) {
        this.speed = this.targetSpeed;
      }
    }
    
    // Move along track (speed can be negative for backward shells)
    this.trackZ += this.speed * dt;
    
    // Wrap around track (handle both forward and backward)
    if (this.trackZ >= roadLength) {
      this.trackZ = this.trackZ % roadLength;
    } else if (this.trackZ < 0) {
      this.trackZ = roadLength + this.trackZ;
    }
    
    // Homing behavior for red/blue shells (only when moving forward)
    if (!this.isBackward && (this.shellType === ShellType.RED || this.shellType === ShellType.BLUE)) {
      var target = this.findVehicleById(vehicles, this.targetId);
      if (target) {
        // Steer toward target's lateral position
        var homingRate = 2.0;  // How fast we home in
        if (this.playerX < target.playerX - 0.05) {
          this.playerX += homingRate * dt;
        } else if (this.playerX > target.playerX + 0.05) {
          this.playerX -= homingRate * dt;
        }
      }
      // If target is gone, shell continues straight but keeps its visual type
      // (don't change shellType - that would make red shells appear green)
    }
    
    // Check for collisions with vehicles
    for (var i = 0; i < vehicles.length; i++) {
      var v = vehicles[i];
      if (v.id === this.ownerId) continue;  // Can't hit self
      if (v.isCrashed) continue;  // Already crashed
      
      // Check for invincibility (Star or Bullet)
      var isInvincible = false;
      for (var e = 0; e < v.activeEffects.length; e++) {
        var effectType = v.activeEffects[e].type;
        if (effectType === ItemType.STAR || effectType === ItemType.BULLET) {
          isInvincible = true;
          break;
        }
      }
      if (isInvincible) continue;  // Invincible - shell passes through harmlessly
      
      // Check collision (similar to vehicle-vehicle)
      var latDist = Math.abs(this.playerX - v.playerX);
      var longDist = Math.abs(this.trackZ - v.trackZ);
      
      if (latDist < 0.5 && longDist < 15) {
        // HIT!
        this.applyHitToVehicle(v);
        this.isDestroyed = true;
        return true;
      }
    }
    
    return false;
  }

  /**
   * Find vehicle by ID.
   */
  private findVehicleById(vehicles: IVehicle[], id: number): IVehicle | null {
    for (var i = 0; i < vehicles.length; i++) {
      if (vehicles[i].id === id) return vehicles[i];
    }
    return null;
  }

  /**
   * Apply shell hit effect to a vehicle.
   * Knocks vehicle to edge of road at 0 mph - must accelerate and steer back.
   */
  private applyHitToVehicle(vehicle: IVehicle): void {
    // Full stop - dramatic impact
    vehicle.speed = 0;
    
    // Knock to side of road (but stay ON the road, not off it)
    // playerX range: -1.0 to +1.0 is on-road, so knock to ±0.7 to ±0.9
    var knockDirection = vehicle.playerX >= 0 ? 1 : -1;  // Knock in direction already moving
    vehicle.playerX = knockDirection * (0.7 + Math.random() * 0.2);  // 0.7 to 0.9
    
    // Longer flash for more dramatic visual feedback
    vehicle.flashTimer = 1.5;
    
    var shellNames = ['GREEN', 'RED', 'BLUE'];
    var direction = this.isBackward ? ' (backward)' : '';
    logInfo(shellNames[this.shellType] + " SHELL" + direction + " hit vehicle " + vehicle.id + " - knocked to edge at playerX=" + vehicle.playerX.toFixed(2) + "!");
  }
}

