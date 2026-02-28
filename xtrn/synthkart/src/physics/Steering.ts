/**
 * Steering - Vehicle steering physics.
 * 
 * NOTE: Main player steering is now handled in Vehicle.updatePhysics().
 * These methods are for NPCs and special cases.
 */

class Steering {
  /**
   * Apply steering input to vehicle heading.
   * Steering effectiveness decreases with speed.
   */
  static apply(vehicle: IVehicle, input: number, steerSpeed: number, dt: number): void {
    if (Math.abs(input) < 0.01) return;
    if (vehicle.speed < 1) return; // Can't steer while stationary

    // Steering is less responsive at high speeds
    var speedFactor = 1 - (vehicle.speed / VEHICLE_PHYSICS.MAX_SPEED) * 0.5;
    var steerAmount = input * steerSpeed * speedFactor * dt;

    vehicle.rotation += steerAmount;
    vehicle.rotation = wrapAngle(vehicle.rotation);
  }

  /**
   * Apply lateral movement (arcade-style steering).
   * Instead of rotating, vehicle moves left/right directly.
   */
  static applyLateral(vehicle: IVehicle, input: number, lateralSpeed: number, dt: number): void {
    if (Math.abs(input) < 0.01) return;
    if (vehicle.speed < 1) return;

    // Lateral movement proportional to speed
    var speedFactor = vehicle.speed / VEHICLE_PHYSICS.MAX_SPEED;
    vehicle.x += input * lateralSpeed * speedFactor * dt;
  }
}
