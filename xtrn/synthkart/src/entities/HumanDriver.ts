/**
 * HumanDriver - Driver controlled by keyboard input.
 */

class HumanDriver implements IDriver {
  private controls: Controls;
  /** Whether the driver is allowed to move (false during countdown) */
  private canMove: boolean;

  constructor(controls: Controls) {
    this.controls = controls;
    this.canMove = true;  // Default to allowing movement
  }

  /**
   * Set whether the driver can move (used for countdown blocking).
   */
  setCanMove(canMove: boolean): void {
    this.canMove = canMove;
  }

  update(_vehicle: IVehicle, _track: ITrack, _dt: number): DriverIntent {
    // Block all input during countdown
    if (!this.canMove) {
      return {
        accelerate: 0,
        steer: 0,
        useItem: false
      };
    }

    return {
      accelerate: this.controls.getAcceleration(),
      steer: this.controls.getSteering(),
      useItem: this.controls.wasJustPressed(GameAction.USE_ITEM)
    };
  }
}
