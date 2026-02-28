/**
 * Driver - Interface for vehicle controllers (human or AI).
 */

interface DriverIntent {
  /** Acceleration input: -1 (brake) to 1 (full throttle) */
  accelerate: number;

  /** Steering input: -1 (left) to 1 (right) */
  steer: number;

  /** Whether to use held item */
  useItem: boolean;
}

interface IDriver {
  /**
   * Update driver and return control intent.
   */
  update(vehicle: IVehicle, track: ITrack, dt: number): DriverIntent;
}

/**
 * Create a neutral (no input) driver intent.
 */
function neutralIntent(): DriverIntent {
  return {
    accelerate: 0,
    steer: 0,
    useItem: false
  };
}
