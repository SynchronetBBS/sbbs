#!/bin/bash
# Build script for Outrun
# Uses Node.js v22 for TypeScript compilation

set -e

cd "$(dirname "$0")"

# Use nvm's Node.js instead of Synchronet's
export PATH=/home/sbbs/.nvm/versions/node/v22.20.0/bin:$PATH

echo "Compiling TypeScript..."
npx tsc

echo "Bundling JavaScript..."
cat dist/bootstrap.js \
    dist/util/Math2D.js \
    dist/util/Rand.js \
    dist/util/DebugLogger.js \
    dist/util/Logging.js \
    dist/util/Config.js \
    dist/timing/Clock.js \
    dist/timing/FixedTimestep.js \
    dist/input/InputMap.js \
    dist/input/Controls.js \
    dist/entities/Entity.js \
    dist/entities/Driver.js \
    dist/entities/HumanDriver.js \
    dist/entities/CpuDriver.js \
    dist/entities/CommuterDriver.js \
    dist/entities/RacerDriver.js \
    dist/entities/CarCatalog.js \
    dist/entities/Vehicle.js \
    dist/world/Road.js \
    dist/world/TrackCatalog.js \
    dist/world/Track.js \
    dist/world/TrackLoader.js \
    dist/world/Checkpoints.js \
    dist/world/SpawnPoints.js \
    dist/physics/Kinematics.js \
    dist/physics/Steering.js \
    dist/physics/Collision.js \
    dist/items/Item.js \
    dist/items/Mushroom.js \
    dist/items/Shell.js \
    dist/items/Banana.js \
    dist/items/ItemSystem.js \
    dist/hud/Hud.js \
    dist/hud/Minimap.js \
    dist/hud/Speedometer.js \
    dist/hud/LapTimer.js \
    dist/hud/PositionIndicator.js \
    dist/highscores/HighScoreManager.js \
    dist/highscores/HighScoreDisplay.js \
    dist/render/cp437/Palette.js \
    dist/render/cp437/GlyphAtlas.js \
    dist/render/cp437/SceneComposer.js \
    dist/render/cp437/RoadRenderer.js \
    dist/render/cp437/ParallaxBackground.js \
    dist/render/cp437/SkylineRenderer.js \
    dist/render/cp437/SpriteRenderer.js \
    dist/render/cp437/HudRenderer.js \
    dist/render/ansi/ANSILoader.js \
    dist/render/themes/Theme.js \
    dist/render/themes/CitySprites.js \
    dist/render/themes/BeachSprites.js \
    dist/render/themes/HorrorSprites.js \
    dist/render/themes/WinterSprites.js \
    dist/render/themes/DesertSprites.js \
    dist/render/themes/JungleSprites.js \
    dist/render/themes/CandySprites.js \
    dist/render/themes/SpaceSprites.js \
    dist/render/themes/CastleSprites.js \
    dist/render/themes/VillainSprites.js \
    dist/render/themes/RuinsSprites.js \
    dist/render/themes/StadiumSprites.js \
    dist/render/themes/KaijuSprites.js \
    dist/render/themes/UnderwaterSprites.js \
    dist/render/sprites/NPCVehicleSprites.js \
    dist/render/sprites/PlayerCarSprites.js \
    dist/render/themes/SynthwaveSprites.js \
    dist/render/themes/SynthwaveTheme.js \
    dist/render/themes/CityNightTheme.js \
    dist/render/themes/SunsetBeachTheme.js \
    dist/render/themes/TwilightForestTheme.js \
    dist/render/themes/HauntedHollowTheme.js \
    dist/render/themes/WinterWonderlandTheme.js \
    dist/render/themes/CactusCanyonTheme.js \
    dist/render/themes/TropicalJungleTheme.js \
    dist/render/themes/CandyLandTheme.js \
    dist/render/themes/RainbowRoadTheme.js \
    dist/render/themes/DarkCastleTheme.js \
    dist/render/themes/VillainsLairTheme.js \
    dist/render/themes/AncientRuinsTheme.js \
    dist/render/themes/ThunderStadiumTheme.js \
    dist/render/themes/GlitchTheme.js \
    dist/render/themes/KaijuRampageTheme.js \
    dist/render/themes/UnderwaterTheme.js \
    dist/render/themes/ANSITunnelSprites.js \
    dist/render/themes/ANSITunnelTheme.js \
    dist/render/frames/FrameManager.js \
    dist/render/frames/Sprite.js \
    dist/render/frames/FrameRenderer.js \
    dist/render/Renderer.js \
    dist/game/GameState.js \
    dist/game/Systems.js \
    dist/game/Cup.js \
    dist/game/Game.js \
    dist/ui/TrackSelector.js \
    dist/ui/CarSelector.js \
    dist/ui/CupStandings.js \
    dist/main.js \
    > dist/synthkart.js

# Copy to project root for distribution
cp dist/synthkart.js synthkart.js

echo "Build complete: synthkart.js"
