/**
 * CpuDriver - AI-controlled driver.
 * Stub implementation for Iteration 0.
 */

class CpuDriver implements IDriver {
  private difficulty: number;

  constructor(difficulty: number) {
    this.difficulty = clamp(difficulty, 0, 1);
  }

  update(_vehicle: IVehicle, _track: ITrack, _dt: number): DriverIntent {
    // Stub: just drive forward
    // Full AI implementation in Iteration 2
    return {
      accelerate: 0.8 * this.difficulty,
      steer: 0,
      useItem: false
    };
  }
}
