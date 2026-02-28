/**
 * Collision - Collision detection and response.
 * Stub for Iteration 0.
 */

interface AABB {
  x: number;
  z: number;
  halfWidth: number;
  halfLength: number;
}

class Collision {
  /**
   * Check if two AABBs overlap.
   */
  static aabbOverlap(a: AABB, b: AABB): boolean {
    return Math.abs(a.x - b.x) < (a.halfWidth + b.halfWidth) &&
           Math.abs(a.z - b.z) < (a.halfLength + b.halfLength);
  }

  /**
   * Get AABB for a vehicle.
   */
  static vehicleToAABB(v: IVehicle): AABB {
    return {
      x: v.x,
      z: v.z,
      halfWidth: 4,   // Vehicle half-width
      halfLength: 6   // Vehicle half-length
    };
  }

  /**
   * Check if vehicle is off track.
   */
  static isOffTrack(vehicle: IVehicle, track: ITrack): boolean {
    var centerX = track.getCenterlineX(vehicle.z);
    var lateralDist = Math.abs(vehicle.x - centerX);
    return lateralDist > track.width / 2;
  }

  /**
   * Resolve vehicle-to-boundary collision.
   */
  static resolveBoundary(vehicle: IVehicle, track: ITrack): void {
    var centerX = track.getCenterlineX(vehicle.z);
    var halfWidth = track.width / 2;
    var lateralDist = vehicle.x - centerX;

    if (Math.abs(lateralDist) > halfWidth) {
      // Push back onto track
      var dir = lateralDist > 0 ? 1 : -1;
      vehicle.x = centerX + dir * halfWidth * 0.95;

      // Slow down
      vehicle.speed *= 0.8;
    }
  }

  /**
   * Detect and resolve all collisions.
   * Stub - full implementation in later iteration.
   */
  static processCollisions(vehicles: IVehicle[], track: ITrack): void {
    // Check boundary collisions
    for (var i = 0; i < vehicles.length; i++) {
      this.resolveBoundary(vehicles[i], track);
    }
  }

  /**
   * Process vehicle-to-vehicle collisions.
   * Called separately from track boundary collisions.
   */
  static processVehicleCollisions(vehicles: IVehicle[]): void {
    // Check all pairs of vehicles
    for (var i = 0; i < vehicles.length; i++) {
      for (var j = i + 1; j < vehicles.length; j++) {
        var a = vehicles[i];
        var b = vehicles[j];
        
        // Skip if either is crashed/recovering
        if (a.isCrashed || b.isCrashed) continue;
        
        // Skip if both are NPCs (let them overlap, player doesn't care)
        if (a.isNPC && b.isNPC) continue;
        
        // Check for invincibility (Star or Bullet effects)
        var aInvincible = this.isInvincible(a);
        var bInvincible = this.isInvincible(b);
        
        // Check collision using playerX and trackZ (road-relative coordinates)
        // Collision box sized to match visual representation
        var latDist = Math.abs(a.playerX - b.playerX);
        var longDist = Math.abs(a.trackZ - b.trackZ);
        
        // Collision thresholds - cars are about 0.3 wide and 8-10 units long
        var collisionLat = 0.4;    // Lateral collision threshold
        var collisionLong = 10;    // Longitudinal collision threshold
        
        if (latDist < collisionLat && longDist < collisionLong) {
          // Handle invincibility
          if (aInvincible && !bInvincible) {
            // A plows through B - B takes full hit, A unaffected
            this.applyCollisionDamage(b, a);
          } else if (bInvincible && !aInvincible) {
            // B plows through A - A takes full hit, B unaffected
            this.applyCollisionDamage(a, b);
          } else if (!aInvincible && !bInvincible) {
            // Normal collision - both affected
            this.resolveVehicleCollision(a, b);
          }
          // If both invincible, no collision effect
        }
      }
    }
  }
  
  /**
   * Check if vehicle has invincibility (Star or Bullet effect).
   */
  static isInvincible(vehicle: IVehicle): boolean {
    var v = vehicle as Vehicle;
    if (!v.hasEffect) return false;
    return v.hasEffect(ItemType.STAR) || v.hasEffect(ItemType.BULLET);
  }
  
  /**
   * Apply collision damage to a vehicle hit by an invincible vehicle.
   * Same dramatic effect as hitting a shell or banana - knocked to road edge at 0 mph.
   */
  static applyCollisionDamage(victim: IVehicle, _hitter: IVehicle): void {
    // Full stop - dramatic impact like hitting a shell
    victim.speed = 0;
    
    // Knock to side of road (but stay ON the road, not off it)
    // playerX range: -1.0 to +1.0 is on-road, so knock to ±0.7 to ±0.9
    var knockDirection = victim.playerX >= 0 ? 1 : -1;  // Knock in direction already moving
    victim.playerX = knockDirection * (0.7 + Math.random() * 0.2);  // 0.7 to 0.9
    
    // Longer flash for dramatic visual feedback
    victim.flashTimer = 1.5;
    
    logInfo("Invincible collision! Vehicle " + victim.id + " knocked to edge at playerX=" + victim.playerX.toFixed(2) + ", speed=0!");
  }

  /**
   * Resolve collision between two vehicles.
   */
  static resolveVehicleCollision(a: IVehicle, b: IVehicle): void {
    // Determine which vehicle was "hit" (the slower/rear one takes more damage)
    var aAhead = a.trackZ > b.trackZ;
    var faster = aAhead ? a : b;
    var slower = aAhead ? b : a;
    
    // Push vehicles apart laterally
    var pushForce = 0.15;
    if (a.playerX < b.playerX) {
      a.playerX -= pushForce;
      b.playerX += pushForce;
    } else {
      a.playerX += pushForce;
      b.playerX -= pushForce;
    }
    
    // Speed exchange - rear-ended vehicle gets pushed, rear-ender slows
    var speedTransfer = 20;
    
    // If player hits NPC from behind, slow player and push NPC
    if (!faster.isNPC && slower.isNPC) {
      // Player hit NPC from behind
      faster.speed = Math.max(0, faster.speed - speedTransfer * 1.5);
      slower.speed = Math.min(VEHICLE_PHYSICS.MAX_SPEED, slower.speed + speedTransfer);
      faster.flashTimer = 0.3;
    } else if (faster.isNPC && !slower.isNPC) {
      // NPC hit player from behind (shouldn't happen often with commuters)
      slower.speed = Math.max(0, slower.speed - speedTransfer * 0.5);
      faster.speed = Math.max(0, faster.speed - speedTransfer);
      slower.flashTimer = 0.3;
    } else {
      // NPC-NPC or theoretical player-player
      faster.speed = Math.max(0, faster.speed - speedTransfer);
      slower.speed = Math.max(0, slower.speed - speedTransfer * 0.5);
    }
    
    // Both vehicles flash briefly
    if (a.flashTimer <= 0) a.flashTimer = 0.2;
    if (b.flashTimer <= 0) b.flashTimer = 0.2;
  }
}
