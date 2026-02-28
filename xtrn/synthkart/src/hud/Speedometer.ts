/**
 * Speedometer - Speed display calculations.
 */

interface SpeedometerData {
  currentSpeed: number;
  maxSpeed: number;
  percentage: number;
  barLength: number;
  filledLength: number;
}

class Speedometer {
  private maxBarLength: number;

  constructor(maxBarLength: number) {
    this.maxBarLength = maxBarLength;
  }

  /**
   * Calculate speedometer display data.
   */
  calculate(currentSpeed: number, maxSpeed: number): SpeedometerData {
    var percentage = clamp(currentSpeed / maxSpeed, 0, 1);
    var filledLength = Math.round(percentage * this.maxBarLength);

    return {
      currentSpeed: Math.round(currentSpeed),
      maxSpeed: maxSpeed,
      percentage: percentage,
      barLength: this.maxBarLength,
      filledLength: filledLength
    };
  }

  /**
   * Generate bar string with fill characters.
   */
  generateBar(data: SpeedometerData): string {
    var filled = '';
    var empty = '';

    for (var i = 0; i < data.filledLength; i++) {
      filled += '█';
    }
    for (var j = data.filledLength; j < data.barLength; j++) {
      empty += '░';
    }

    return filled + empty;
  }
}
