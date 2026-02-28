/**
 * Kinematics - Vehicle movement physics.
 * 
 * NOTE: Most physics is now handled in Vehicle.updatePhysics().
 * These are utility methods for special cases.
 */

class Kinematics {
  /**
   * Update entity position based on speed and heading.
   * Used for projectiles and NPCs, not player vehicle.
   */
  static update(entity: IEntity, dt: number): void {
    // Move forward based on speed and rotation
    entity.x += Math.sin(entity.rotation) * entity.speed * dt;
    entity.z += Math.cos(entity.rotation) * entity.speed * dt;
  }

  /**
   * Apply friction to slow an entity.
   */
  static applyFriction(entity: IEntity, friction: number, dt: number): void {
    if (entity.speed > 0) {
      entity.speed = Math.max(0, entity.speed - friction * dt);
    }
  }

  /**
   * Apply acceleration to an entity.
   */
  static applyAcceleration(entity: IEntity, accel: number, maxSpeed: number, dt: number): void {
    entity.speed += accel * dt;
    entity.speed = clamp(entity.speed, 0, maxSpeed);
  }
}
