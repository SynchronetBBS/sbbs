/**
 * Mushroom - Speed boost item.
 */

class Mushroom extends Item {
  /** Boost multiplier */
  static BOOST_MULTIPLIER: number = 1.4;

  /** Boost duration in seconds */
  static BOOST_DURATION: number = 3.0;

  constructor() {
    super(ItemType.MUSHROOM);
  }

  /**
   * Apply mushroom effect to vehicle.
   * Sets boost timer and multiplier for sustained speed increase.
   * Also locks in a minimum speed to prevent deceleration from key handling issues.
   */
  static applyEffect(vehicle: IVehicle): void {
    vehicle.boostTimer = Mushroom.BOOST_DURATION;
    vehicle.boostMultiplier = Mushroom.BOOST_MULTIPLIER;
    
    // Lock in current speed as minimum - prevents deceleration from key issues
    vehicle.boostMinSpeed = Math.max(vehicle.speed, VEHICLE_PHYSICS.MAX_SPEED * 0.5);
    
    // Immediate speed bump to get into the boost zone
    vehicle.speed = Math.min(
      vehicle.speed * 1.3,
      VEHICLE_PHYSICS.MAX_SPEED * Mushroom.BOOST_MULTIPLIER
    );
    
    logInfo("Mushroom boost activated! Duration: " + Mushroom.BOOST_DURATION + "s, minSpeed: " + vehicle.boostMinSpeed);
  }
}
