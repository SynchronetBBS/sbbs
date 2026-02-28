/**
 * CarSelector - UI for selecting player car and color before races.
 * 
 * Displays available cars with their stats and a visual preview.
 * Allows color selection for the chosen car.
 * 
 * Navigation:
 * - Up/Down or W/S: Change selected car
 * - Left/Right or A/D: Change color
 * - Enter/Space: Confirm selection
 * - Q/Escape: Cancel (use default car)
 */

interface CarSelection {
  carId: string;
  colorId: string;
  confirmed: boolean;
}

/**
 * CarSelector displays a selection screen and returns the player's choice.
 */
var CarSelector = {
  
  /**
   * Show the car selection screen and return the selected car/color.
   * 
   * @param composer - SceneComposer for rendering
   * @returns Selected car and color IDs
   */
  show: function(composer: SceneComposer): CarSelection {
    var cars = getAllCars();
    var selectedCarIndex = 0;
    var selectedColorIndex = 0;
    
    // Find initial selection (first unlocked car)
    for (var i = 0; i < cars.length; i++) {
      if (cars[i].unlocked) {
        selectedCarIndex = i;
        break;
      }
    }
    
    var running = true;
    var confirmed = false;
    
    while (running) {
      // Get current selection
      var currentCar = cars[selectedCarIndex];
      var availableColors: CarColor[] = [];
      for (var c = 0; c < currentCar.availableColors.length; c++) {
        var col = getCarColor(currentCar.availableColors[c]);
        if (col) availableColors.push(col);
      }
      
      // Clamp color index to available colors
      if (selectedColorIndex >= availableColors.length) {
        selectedColorIndex = 0;
      }
      var currentColor = availableColors[selectedColorIndex];
      
      // Render the selection screen
      this.render(composer, cars, selectedCarIndex, currentColor, selectedColorIndex, availableColors.length);
      
      // Wait for input (inkey with wait mode)
      var key = console.inkey(K_UPPER, 100);
      if (key === '') continue;
      
      switch (key) {
        // Navigation - car selection
        case 'W':
        case KEY_UP:
          selectedCarIndex--;
          if (selectedCarIndex < 0) selectedCarIndex = cars.length - 1;
          // Skip locked cars
          var attempts = 0;
          while (!cars[selectedCarIndex].unlocked && attempts < cars.length) {
            selectedCarIndex--;
            if (selectedCarIndex < 0) selectedCarIndex = cars.length - 1;
            attempts++;
          }
          selectedColorIndex = 0;  // Reset color on car change
          break;
          
        case 'S':
        case KEY_DOWN:
          selectedCarIndex++;
          if (selectedCarIndex >= cars.length) selectedCarIndex = 0;
          // Skip locked cars
          var attempts = 0;
          while (!cars[selectedCarIndex].unlocked && attempts < cars.length) {
            selectedCarIndex++;
            if (selectedCarIndex >= cars.length) selectedCarIndex = 0;
            attempts++;
          }
          selectedColorIndex = 0;  // Reset color on car change
          break;
          
        // Navigation - color selection
        case 'A':
        case KEY_LEFT:
          selectedColorIndex--;
          if (selectedColorIndex < 0) selectedColorIndex = availableColors.length - 1;
          break;
          
        case 'D':
        case KEY_RIGHT:
          selectedColorIndex++;
          if (selectedColorIndex >= availableColors.length) selectedColorIndex = 0;
          break;
          
        // Confirm selection
        case '\r':
        case '\n':
        case ' ':
          if (currentCar.unlocked) {
            confirmed = true;
            running = false;
          }
          break;
          
        // Cancel
        case 'Q':
        case '\x1b':  // Escape
          running = false;
          break;
      }
    }
    
    // Return selection (or default if cancelled)
    if (confirmed) {
      var car = cars[selectedCarIndex];
      var colors: CarColor[] = [];
      for (var c = 0; c < car.availableColors.length; c++) {
        var col = getCarColor(car.availableColors[c]);
        if (col) colors.push(col);
      }
      return {
        carId: car.id,
        colorId: colors[selectedColorIndex].id,
        confirmed: true
      };
    } else {
      // Return default
      return {
        carId: 'sports',
        colorId: 'yellow',
        confirmed: false
      };
    }
  },
  
  /**
   * Render the car selection screen.
   */
  render: function(composer: SceneComposer, cars: CarDefinition[], selectedIndex: number,
                   currentColor: CarColor, colorIndex: number, totalColors: number): void {
    composer.clear();
    
    var width = 80;
    
    // Title
    var titleAttr = makeAttr(LIGHTCYAN, BG_BLACK);
    var title = '=== SELECT YOUR VEHICLE ===';
    composer.writeString(Math.floor((width - title.length) / 2), 1, title, titleAttr);
    
    // Instructions
    var instructAttr = makeAttr(DARKGRAY, BG_BLACK);
    composer.writeString(5, 22, 'W/S: Car  A/D: Color  ENTER: Select  Q: Cancel', instructAttr);
    
    // Car list (left side)
    var listX = 3;
    var listY = 4;
    var listAttr = makeAttr(LIGHTGRAY, BG_BLACK);
    var selectedAttr = makeAttr(WHITE, BG_BLUE);
    var lockedAttr = makeAttr(DARKGRAY, BG_BLACK);
    
    for (var i = 0; i < cars.length; i++) {
      var car = cars[i];
      var y = listY + i * 2;
      var attr = (i === selectedIndex) ? selectedAttr : (car.unlocked ? listAttr : lockedAttr);
      
      // Selection indicator
      var indicator = (i === selectedIndex) ? '>' : ' ';
      composer.writeString(listX, y, indicator, attr);
      
      // Car name
      var name = car.unlocked ? car.name : '??? LOCKED ???';
      composer.writeString(listX + 2, y, name, attr);
      
      // Lock indicator
      if (!car.unlocked) {
        composer.writeString(listX + 2, y + 1, car.unlockHint || 'Complete challenges', lockedAttr);
      }
    }
    
    // Selected car details (right side)
    var detailX = 40;
    var detailY = 4;
    var currentCar = cars[selectedIndex];
    
    if (currentCar.unlocked) {
      // Car name and description
      var nameAttr = makeAttr(YELLOW, BG_BLACK);
      composer.writeString(detailX, detailY, currentCar.name, nameAttr);
      
      var descAttr = makeAttr(LIGHTGRAY, BG_BLACK);
      composer.writeString(detailX, detailY + 1, currentCar.description, descAttr);
      
      // Stats display
      var statsY = detailY + 3;
      var statLabelAttr = makeAttr(CYAN, BG_BLACK);
      var statBarAttr = makeAttr(LIGHTGREEN, BG_BLACK);
      var statBarEmptyAttr = makeAttr(DARKGRAY, BG_BLACK);
      
      // Top Speed
      composer.writeString(detailX, statsY, 'TOP SPEED:', statLabelAttr);
      this.renderStatBar(composer, detailX + 11, statsY, currentCar.stats.topSpeed, statBarAttr, statBarEmptyAttr);
      
      // Acceleration
      composer.writeString(detailX, statsY + 1, 'ACCEL:', statLabelAttr);
      this.renderStatBar(composer, detailX + 11, statsY + 1, currentCar.stats.acceleration, statBarAttr, statBarEmptyAttr);
      
      // Handling
      composer.writeString(detailX, statsY + 2, 'HANDLING:', statLabelAttr);
      this.renderStatBar(composer, detailX + 11, statsY + 2, currentCar.stats.handling, statBarAttr, statBarEmptyAttr);
      
      // Color selection
      var colorY = statsY + 5;
      var colorLabelAttr = makeAttr(LIGHTMAGENTA, BG_BLACK);
      composer.writeString(detailX, colorY, 'COLOR: < ' + currentColor.name + ' >', colorLabelAttr);
      composer.writeString(detailX, colorY + 1, '(' + (colorIndex + 1) + '/' + totalColors + ')', instructAttr);
      
      // Car preview (render the sprite)
      var previewY = colorY + 3;
      this.renderCarPreview(composer, detailX + 8, previewY, currentCar.bodyStyle, currentColor);
      
    } else {
      // Locked car - show mystery
      var lockedMsgAttr = makeAttr(RED, BG_BLACK);
      composer.writeString(detailX, detailY, 'VEHICLE LOCKED', lockedMsgAttr);
      composer.writeString(detailX, detailY + 2, currentCar.unlockHint || 'Complete challenges to unlock', lockedAttr);
    }
    
    // Render buffer to screen
    this.outputToConsole(composer);
  },
  
  /**
   * Output the SceneComposer buffer to the console.
   */
  outputToConsole: function(composer: SceneComposer): void {
    console.clear(BG_BLACK, false);
    var buffer = composer.getBuffer();
    for (var y = 0; y < buffer.length; y++) {
      console.gotoxy(1, y + 1);
      for (var x = 0; x < buffer[y].length; x++) {
        var cell = buffer[y][x];
        console.attributes = cell.attr;
        console.print(cell.char);
      }
    }
  },
  
  /**
   * Render a stat bar (10 chars wide, filled based on value).
   * Value 1.0 = 5 blocks (middle), higher = more, lower = less
   */
  renderStatBar: function(composer: SceneComposer, x: number, y: number, value: number,
                          filledAttr: number, emptyAttr: number): void {
    // Map value to 0-10 scale (0.8 = 4, 1.0 = 5, 1.2 = 6)
    var filled = Math.round(value * 5);
    filled = Math.max(1, Math.min(10, filled));
    
    for (var i = 0; i < 10; i++) {
      var char = (i < filled) ? GLYPH.FULL_BLOCK : GLYPH.LIGHT_SHADE;
      var attr = (i < filled) ? filledAttr : emptyAttr;
      composer.setCell(x + i, y, char, attr);
    }
  },
  
  /**
   * Render a car preview sprite.
   */
  renderCarPreview: function(composer: SceneComposer, x: number, y: number, 
                              bodyStyle: CarBodyStyle, color: CarColor): void {
    // Get the sprite for this car/color combo (no brake lights in preview)
    var sprite = getPlayerCarSprite(bodyStyle, color.id, false);
    var variant = sprite.variants[0];  // Player car only has one variant
    
    // Render the sprite to the composer
    for (var row = 0; row < variant.length; row++) {
      for (var col = 0; col < variant[row].length; col++) {
        var cell = variant[row][col];
        if (cell) {
          composer.setCell(x + col, y + row, cell.char, cell.attr);
        }
      }
    }
    
    // Add a label below
    var labelAttr = makeAttr(DARKGRAY, BG_BLACK);
    composer.writeString(x - 2, y + 3, '(preview)', labelAttr);
  }
};
