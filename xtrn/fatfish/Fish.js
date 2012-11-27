var FISH_TYPE_BASS = "bass";
var FISH_TYPE_EEL = "eel";
var FISH_TYPE_PIKE = "pike";
var FISH_TYPE_TROUT = "trout";

function Fish(ftype) {
    //log("Creating Fish: " + ftype);

    this.x = -1;
    this.x_last = -1;
    this.y = -1;
    this.y_last = -1;

    this.depth = -1;
    this.depth_preferred = 0;

    this.weight = 0;    // Weight in KG.
    this.length = 0;    // Length in MM.
    this.texture = "f";
    this.attr = undefined;

    this.moved = false;
    this.type = ftype;

    this.sight_distance = 3;

    this.is_biting = false;

    this.id = -1;

    // Has this fish been sold?
    this.sold = false;

    /* TODO: Implement stamina. */
    this.stamina = -1;

    // Set Fish's properties based on type.
    switch (ftype) {
        case FISH_TYPE_BASS:
            // Set texture based on type.
            this.texture = "b";
            this.attr = YELLOW;

            this.depth_preferred = -1.75;

            this.length = 360;  // Average length in mm.
            break;

        case FISH_TYPE_EEL:
            this.texture = "e";
            this.attr = DARKGRAY;
            this.depth_preferred = -0.01;
            this.length = 150;
            break;

        case FISH_TYPE_PIKE:
            this.texture = "p";
            this.attr = MAGENTA;

            // Prefers deep.
            this.depth_preferred = -5.0;
            this.length = 650;
            break;

        case FISH_TYPE_TROUT:
            this.texture = "t";
            this.attr = LIGHTGRAY;

            // Prefers shallow.
            this.depth_preferred = -1.0;
            this.length = 500;

            break;
    }

    // Randomize the weight and length some.
    rl = random(101);   // 50-150% of the average length.
    this.length = (((50 + rl) / 100) * this.length).toFixed(0);

    this.weight = fish_length_to_weight(this);

    /* TODO: implement accurate weight calculations
    The formula to calculate the weight for a Northern Pike that is 40 inches long and has a girth of 20 inches is as follows. Northern Pike have a shape factor of 1000 and a girth ratio of (.52)
    WEIGHT = LENGTH x GIRTH / SHAPE FACTOR
    WEIGHT= 40 x (20) / 1000
    WEIGHT= 40 x 400 / 1000
    WEIGHT= 16000 / 1000
    WEIGHT = 16.0 pounds

    Wikipedia:
    W = c * L^b
    b = close to 3.0 for all species.
    c = 7.089 enables you to put in length in meters and weight in kg.

    Another way to calculate:
    http://www.first-nature.com/flyfish/ff-convert.php
    Trout and Salmon Weight (in pounds) = (Length x Girth x Girth)/800 where Length and Girth are expressed in inches.
    Pike Weight (in pounds) = (Length x Girth x Girth)/900 where Length and Girth are expressed in inches.
    */

    // Is this Fish visible?
    this.visible = true;

    // Randomly move.
    this.move_random = function () {
        // 0  -1
        // 1   0
        // 2  +1
        rx = random(3);
        ry = random(3);

        proposed_x = this.x - 1 + rx;
        proposed_y = this.y - 1 + ry;

        set_proposed = false;

        // Check if next position is on lake and in water.
        // On screen.
        if (proposed_x <= lake_x && proposed_y <= lake_y && proposed_x >= 0 && proposed_y >= 0 && lake.map[proposed_y] != undefined && lake.map[proposed_y][proposed_x] != undefined) {

            // Check if proposed location has a preferred depth.
            // x1:  abs (proposed depth - preferred depth)
            // x2:  abs (current depth - preferred depth)
            //  x1,x2 - whichever is smaller, pic that.
            //  If x1, 75% move to proposed position.
            //  If x2, 75% stay in current position.
            if (this.depth_preferred != 0) {
                x1 = Math.abs(lake.map[proposed_y][proposed_x].depth - this.depth_preferred);
                x2 = Math.abs(lake.map[this.y][this.x].depth - this.depth_preferred);
                //log("preferred depth check x1: " + x1.toFixed(3) + ", x2: " + x2.toFixed(3));

                //  x1,x2 - whichever is smaller, pic that position.
                if (x1 <= x2) {
                    //  If x1, 75% move to proposed position.
                    //log("I prefer the proposed position.");

                    r1 = random(4);

                    switch (r1) {
                        case 0:
                        case 1:
                        case 2:
                            // Move to proposed.
                            set_proposed = true;
                            break;
                        case 3:
                        case 4:
                            // Stay at current.
                            set_proposed = false;
                            break;
                    }
                } else {
                    //  If x2, 75% stay in current position.
                    //log("I prefer the current position.");

                    r1 = random(4);

                    switch (r1) {
                        case 0:
                        case 1:
                        case 2:
                            // Stay at current.
                            set_proposed = false;
                            break;
                        case 3:
                        case 4:
                            // Move to proposed.
                            set_proposed = true;
                            break;
                    }
                }
            } else {
                set_proposed = true;
            }

            // Check if proposed location was last location:
            //  Yes: 50% chance to not return to last location.
            if (set_proposed && proposed_x == this.x_last && proposed_y == this.y_last) {
                r1 = random(2);

                switch (r1) {
                    case 0:
                        set_proposed = false;
                        break;
                }

            }

            // Check if proposed location is in water.
            if (lake.map[proposed_y][proposed_x].isWaterFish() && set_proposed) {
                // Proposed position OK: set it.
                this.x_last = this.x;
                this.y_last = this.y;

                this.x = proposed_x;
                this.y = proposed_y;

                if (this.x != this.x_last) {
                    if (this.y != this.y_last) {
                        this.depth = lake.map[this.y][this.x].depth / 2;

                        
                        if (!fish_map[this.x]) {
                            fish_map[this.x] = {};
                        }

                        if (!fish_map[this.x][this.y]) {
                            fish_map[this.x][this.y] = [];
                        }

                        fish_map[this.x][this.y].push(this);
                        
                    }
                }
            }
        }
    }

    // Returns boolean: is Fish at boat?
    this.is_at_boat = function () {
        if (this.x == boat.x) {
            if (this.y == boat.y) {
                return true;
            }
        }

        return false;
    } // End this.is_at_boat()

    // Fish nearby to lure?
    this.is_near_lure = function () {
        if (bar_mode == "cast") {
            // Check x radius around Fish to see if lure is near.
            var dx = Math.abs(this.x - boat.x);
            var dy = Math.abs(this.y - boat.y);
            var distance = Math.sqrt((dx * dx) + (dy * dy)).toFixed(0);
            //log("Distance to lure: " + distance + "m");

            //if (this.x == boat.x && this.y == boat.y) {
            if (distance <= this.sight_distance) {
                //log("Fish: I'm near a lure: " + distance + "m.");
                return true;
            }
        } else {
            return false;
        }
    }

    // Boolean: does the Fish like the lure?
    this.like_lure = function () {
        // Based on Bait, etc.

        /* Base on fish weight vs rod weight */
        var idx = Math.round(this.length / 100);
        var cmp = Math.abs(idx - rod_equipped.weight);
        //log("Fish vs Rod: " + this.type + "  length: " + this.length + "  weight: " + this.weight + "\tindex: " + idx + " -vs - " + rod_equipped.weight);
        //log("CMP: " + cmp);

        // If cmp > 3, inappropriate kind of Rod for this Fish (size mismatch).

        if (cmp <= 4) {
            //log("Fish" + this.id + ": I can fit this in my mouth. cmp: " + cmp);

            var r_like = random(2);

            switch (r_like) {
                case 0:
                    //log("Fish" + this.id + ": I like this lure. cmp: " + cmp);
                    return true;
                    break;
            }
        } else {
            //log("Fish" + this.id + ": rod mismatch. cmp: " + cmp);
        }

        //log("Fish" + this.id + ": I don't like this lure. cmp: " + cmp);
        return false;
    }

    // Move towards x,y.
    this.move_towards = function (ox, oy) {
        // Get proposed location.
        var prop_x = this.x;
        var prop_y = this.y;

        if (prop_x < ox) {
            prop_x++;
        } else if (prop_x > ox) {
            prop_x--;
        }

        if (prop_y < oy) {
            prop_y++;
        } else if (prop_y > oy) {
            prop_y--;
        }

        // Check if it's movable.
        if (lake.map[prop_y][prop_x].isWaterFish()) {
            this.x_last = this.x;
            this.y_last = this.y;

            this.x = prop_x;
            this.y = prop_y;

            // Set Fish's new depth.
            this.depth = lake.map[prop_y][prop_x].depth / 2;

            
            if (!fish_map[this.x]) {
                fish_map[this.x] = {};
            }

            if (!fish_map[this.x][this.y]) {
                fish_map[this.x][this.y] = [];
            }

            fish_map[this.x][this.y].push(this);
            
        } else {
            // If not water/blocked, move randomly.
            this.move_random();
        }

    }


    // Returns boolean: will the Fish bite?
    this.will_bite = function () {

        var r1 = random(10);

        switch (r1) {
            case 1:
                this.is_biting = true;

                // Add Timer to release bite.
                var rd = random(this.length);
                var bite_duration = 150 + Number(this.length) + rd;
                //log("this.length: " + this.length + "  random(len): " + rd + ". Bite dur: " + bite_duration);

                timer_cast.addEvent(bite_duration, false, fish_release_bite, [this]); // Cast event.   
                //log("Fish" + this.id + ": I'm gonna bite this bitch! (" + bite_duration + "ms).");
                return true;
                break;
        }

        //log("Fish" + this.id + ": I don't want to bite.");
        return false;
    } // End this.will_bite()
}

// Return a random Fish.
function get_random_fish() {
    r1 = random(4);

    switch (r1) {
        case 0:
            // Bass
            f = new Fish(FISH_TYPE_BASS);
            break;
        case 1:
            // Pike
            f = new Fish(FISH_TYPE_EEL);
            break;
        case 2:
            // Pike
            f = new Fish(FISH_TYPE_PIKE);
            break;
        case 3:
            // Trout
            f = new Fish(FISH_TYPE_TROUT);
            break;

    }

    // Place it in water.
    found_fish = false;
    while (!found_fish) {
        var rx = random(lake_x);
        var ry = random(lake_y);
        //log("rx, ry: " + rx + "," + ry + "  is water? " + lake.map[ry][rx].isWaterFish());

        if (lake.map[ry][rx].isWaterFish()) {
            f.x = rx;
            f.y = ry;

            // Set the Fish's depth.
            // TODO: Use a better way. For now, half of max depth.
            f.depth = lake.map[ry][rx].depth / 2;

            found_fish = true;
        }
    }

    return f;
}

function fish_length_to_weight_general(mm) {
    /*
        Another way to calculate:
        http://www.first-nature.com/flyfish/ff-convert.php
        Trout and Salmon Weight (in pounds) = (Length x Girth x Girth)/800 where Length and Girth are expressed in inches.
        Pike Weight (in pounds) = (Length x Girth x Girth)/900 where Length and Girth are expressed in inches.
        Typical girth measurement (58% of the length)
    */
    //var v = 7.089 * (Math.pow(mm / 1000, 3));
    //return v;

    var inches = (mm * 0.0393700787).toFixed(2); // Convert mm to inches.
    var girth = (inches * 0.58).toFixed(2);   // Girth in inches.
    var pounds = ((inches * girth * girth) / 800).toFixed(2);
    var kgs = (pounds * 0.453592).toFixed(2); // Convert lb to kg.
    //log("KGs: " + kgs);
    return parseFloat(kgs).toFixed(2);
}

function fish_length_to_weight_bass(mm) {
    /*
        W = c * L^b
        b = close to 3.0 for all species.
        c = 7.089 enables you to put in length in meters and weight in kg.
    */
    //var v = 7.089 * (Math.pow(mm / 1000, 3));
    //log(mm + "mm ---> " + v + "kg");
    //return v;

    // length x length x girth ÷ 1,200
    var inches = (mm * 0.0393700787).toFixed(2); // Convert mm to inches.
    var girth = (inches * 0.58).toFixed(2);   // Girth in inches.
    var pounds = ((inches * inches * girth) / 1200).toFixed(2);
    var kgs = (pounds * 0.453592).toFixed(2); // Convert lb to kg.
    //log("KGs: " + kgs);
    return kgs;
}

function fish_length_to_weight_eel(mm) {
    /*
        Another way to calculate:
        http://www.first-nature.com/flyfish/ff-convert.php
        Trout and Salmon Weight (in pounds) = (Length x Girth x Girth)/800 where Length and Girth are expressed in inches.
        Pike Weight (in pounds) = (Length x Girth x Girth)/900 where Length and Girth are expressed in inches.
        Typical girth measurement (58% of the length)
    */
    //var v = 7.089 * (Math.pow(mm / 1000, 3));
    //return v;


    var inches = (mm * 0.0393700787).toFixed(3); // Convert mm to inches.
    var girth = (inches * 1.0).toFixed(3);   // Girth in inches.
    var pounds = ((inches * girth * girth) / 900).toFixed(3);
    var kgs = (pounds * 0.453592).toFixed(3); // Convert lb to kg.
    //log("KGs: " + kgs);
    return kgs;

}

function fish_length_to_weight_pike(mm) {
    /*
        Another way to calculate:
        http://www.first-nature.com/flyfish/ff-convert.php
        Trout and Salmon Weight (in pounds) = (Length x Girth x Girth)/800 where Length and Girth are expressed in inches.
        Pike Weight (in pounds) = (Length x Girth x Girth)/900 where Length and Girth are expressed in inches.
        Typical girth measurement (58% of the length)
    */
    //var v = 7.089 * (Math.pow(mm / 1000, 3));
    //return v;


    var inches = (mm * 0.0393700787).toFixed(2); // Convert mm to inches.
    var girth = (inches * 0.58).toFixed(2);   // Girth in inches.
    var pounds = ((inches * girth * girth) / 900).toFixed(2);
    var kgs = (pounds * 0.453592).toFixed(2); // Convert lb to kg.
    //log("KGs: " + kgs);
    return kgs;

}

function fish_length_to_weight(fish) {
    var r = undefined;
    switch (fish.type) {
        case FISH_TYPE_BASS:
            r = fish_length_to_weight_bass(fish.length);
            break;
        case FISH_TYPE_EEL:
            r = fish_length_to_weight_eel(fish.length);
            break;
        case FISH_TYPE_PIKE:
            r = fish_length_to_weight_pike(fish.length);
            break;
        case FISH_TYPE_TROUT:
        default:
            r = fish_length_to_weight_general(fish.length);
            break;
    }

    //log("weight() returning: " + parseFloat(r).toSource());
    return parseFloat(r);
}

// Release bite.
function fish_release_bite(fish) {
    //log("fish_release_bite(): " + fish.is_biting);

    if (fish != undefined && fish.is_biting) {
        // Add timer to free the line X seconds after fish releases.
        // This ensures the line cannot be immediately bitten right after a Fish has let go.
        timer_cast.addEvent(1000 + random(4000), false, line_release_bite);

        // Fish no longer biting.
        fish.is_biting = false;

        // No fish biting the line.
        fish_biting = undefined;

        // Reset ANSI.
        cast_ansi = "float-wait-orig.ans";

        // Redraw the cast screen.
        redraw_cast = true;

        //log("Fish" + fish.id + ": I let go of the line.");
    }
}