/**
 * Banana - Stationary hazard dropped behind vehicle.
 * 
 * Acts like a non-moving obstacle on the track.
 * - Placed at vehicle's current position when dropped
 * - Causes spinout when hit
 * - Destroyed on impact
 */

class Banana extends Item implements IProjectile {
  shellType: ShellType;  // Not used, but required by IProjectile
  trackZ: number;
  playerX: number;
  speed: number;
  ownerId: number;
  targetId: number;
  ttl: number;
  isDestroyed: boolean;

  constructor() {
    super(ItemType.BANANA);
    this.shellType = ShellType.GREEN;  // Dummy value
    this.trackZ = 0;
    this.playerX = 0;
    this.speed = 0;  // Bananas don't move
    this.ownerId = -1;
    this.targetId = -1;
    this.ttl = 60;  // Stay on track for 60 seconds
    this.isDestroyed = false;
  }

  /**
   * Drop a banana behind the vehicle.
   */
  static drop(vehicle: IVehicle): Banana {
    var banana = new Banana();
    banana.trackZ = vehicle.trackZ - 20;  // Drop behind vehicle
    banana.playerX = vehicle.playerX;      // Same lateral position
    banana.ownerId = vehicle.id;
    logInfo("BANANA dropped at trackZ=" + banana.trackZ.toFixed(0) + ", playerX=" + banana.playerX.toFixed(2));
    return banana;
  }

  /**
   * Update banana (mostly just TTL countdown).
   */
  update(dt: number, vehicles: IVehicle[], roadLength: number): boolean {
    if (this.isDestroyed) return true;
    
    // Decrease TTL
    this.ttl -= dt;
    if (this.ttl <= 0) {
      logInfo("Banana despawned (TTL)");
      return true;
    }
    
    // Wrap around track if needed
    if (this.trackZ < 0) {
      this.trackZ += roadLength;
    } else if (this.trackZ >= roadLength) {
      this.trackZ = this.trackZ % roadLength;
    }
    
    // Check for collisions with vehicles
    for (var i = 0; i < vehicles.length; i++) {
      var v = vehicles[i];
      if (v.id === this.ownerId) continue;  // Can't hit own banana immediately
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
      if (isInvincible) continue;  // Invincible - banana has no effect
      
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
   * Apply banana hit effect to a vehicle.
   * Knocks vehicle to edge of road at 0 mph - must accelerate and steer back.
   */
  private applyHitToVehicle(vehicle: IVehicle): void {
    // Full stop - spinout effect
    vehicle.speed = 0;
    
    // Knock to side of road (but stay ON the road, not off it)
    // playerX range: -1.0 to +1.0 is on-road, so knock to ±0.7 to ±0.9
    var knockDirection = vehicle.playerX >= 0 ? 1 : -1;  // Knock in direction already moving
    vehicle.playerX = knockDirection * (0.7 + Math.random() * 0.2);  // 0.7 to 0.9
    
    // Longer flash for dramatic spinout visual
    vehicle.flashTimer = 2.0;
    
    logInfo("BANANA hit vehicle " + vehicle.id + " - spun out to edge at playerX=" + vehicle.playerX.toFixed(2) + "!");
  }
}
