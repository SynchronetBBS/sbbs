/**
 * SynthKartScene3dRuntime - Nintendo 3DS text-layer integration.
 *
 * The race already has a real scene graph, so this adapter turns its Frame.js
 * provenance into stereo depth. It also exposes a small raw-console selector
 * for menus and results screens which do not use Frame.js.
 */

interface Scene3dModule {
  MAX_TEXT_LAYERS: number;
  probe(timeoutMs: number): any;
  supportsTextLayers(version: any): boolean;
  selectTextLayerSeq(layer: number): string;
  defineTextLayerSeq(layer: number, depthWorld: number): string;
  TextDepthLayers: any;
}

class SynthKartScene3dRuntime {
  private mod: Scene3dModule | null;
  private version: any;
  private enabled: boolean;
  private attempted: boolean;
  private depthLayers: any;
  private currentLayer: number;
  private order: string[];
  private indices: { [band: string]: number };
  private depths: { [band: string]: number };
  private spread: number;

  constructor() {
    this.mod = null;
    this.version = null;
    this.enabled = false;
    this.attempted = false;
    this.depthLayers = null;
    this.currentLayer = 0;
    this.spread = 4.2;
    this.order = [
      'glass', 'hud', 'title',
      'vehicleNear', 'roadsideNear',
      'vehicleMid', 'roadsideMid',
      'vehicleFar', 'roadsideFar',
      'content', 'road', 'ground',
      'mountains', 'celestial', 'sky', 'backdrop'
    ];
    this.depths = {
      glass: 0.0,
      hud: 0.03,
      title: 0.07,
      vehicleNear: 0.11,
      roadsideNear: 0.16,
      vehicleMid: 0.24,
      roadsideMid: 0.31,
      vehicleFar: 0.40,
      roadsideFar: 0.48,
      content: 0.54,
      road: 0.60,
      ground: 0.70,
      mountains: 0.80,
      celestial: 0.88,
      sky: 0.95,
      backdrop: 1.0
    };
    this.indices = {};
    for (var i = 0; i < this.order.length; i++) {
      this.indices[this.order[i]] = i;
    }
    // Friendly names used by direct-console screens.
    this.indices.prompt = this.indices.hud;
    this.indices.chrome = this.indices.ground;
  }

  /** Probe once, before any SynthKart art is drawn. */
  initialize(): void {
    if (this.attempted) return;
    this.attempted = true;
    try {
      this.mod = load('/sbbs/mods/load/scene3d.js') as Scene3dModule;
      this.version = this.mod.probe(500);
      if (!this.mod.supportsTextLayers(this.version)) return;
      this.enabled = true;
      this.currentLayer = 0;
      this.writeDepthTable();
      logInfo('SynthKart: 3dBBS text depth enabled');
    } catch (e) {
      this.mod = null;
      this.enabled = false;
      logWarning('SynthKart: scene3d unavailable: ' + e);
    }
  }

  isEnabled(): boolean {
    return this.enabled;
  }

  /** Install the Frame.js provenance patch after frame.js has been loaded. */
  installFrameDepth(): void {
    if (!this.enabled || !this.mod) return;
    // Some legacy SynthKart paths used to reload frame.js. If that happens,
    // discard the patch tied to the old Display constructor and patch the new
    // one rather than silently rendering the next race flat.
    var currentDisplay = js && js.global ? js.global.Display : null;
    if (this.depthLayers && currentDisplay && this.depthLayers._ctor !== currentDisplay) {
      try { this.depthLayers.dispose(); } catch (e) { }
      this.depthLayers = null;
    }
    if (this.depthLayers) return;
    var self = this;
    try {
      var DepthCtor = this.mod.TextDepthLayers;
      var layers = new DepthCtor({
        spread: this.spread,
        order: this.order,
        depths: this.depths,
        bandFor: function(frame: Frame): string {
          return self.bandForFrame(frame);
        },
        log: function(message: string): void {
          logInfo('SynthKart: ' + message);
        }
      });
      if (layers.install(null)) {
        this.depthLayers = layers;
        this.currentLayer = 0;
      }
    } catch (e) {
      logWarning('SynthKart: could not install Frame.js depth: ' + e);
    }
  }

  /** Assign or retune a frame's semantic band before its next cycle. */
  tagFrameDepth(frame: Frame | null, band: string): void {
    if (!this.enabled || !frame) return;
    if (frame._scene3dBand === band) return;
    frame._scene3dBand = band;
    if (this.depthLayers) this.depthLayers.invalidate();
  }

  /** Select a layer for direct console output and keep Frame.js in sync. */
  selectRawDepth(band: string): void {
    if (!this.enabled || !this.mod) return;
    var layer = this.indices[band];
    if (layer === undefined) layer = 0;
    // Frame.js always returns the wire to layer zero after a cycle. Reflect
    // that here before deciding whether a direct-console select is redundant.
    if (this.depthLayers) this.currentLayer = this.depthLayers._curLayer;
    if (layer === this.currentLayer) return;
    try {
      console.write(this.mod.selectTextLayerSeq(layer));
      this.currentLayer = layer;
      if (this.depthLayers) this.depthLayers._curLayer = layer;
    } catch (e) {
      // Rendering must continue in ordinary 2-D if the client disconnects.
    }
  }

  /** Restore the glass and flatten every client layer for the next door. */
  dispose(): void {
    if (!this.enabled || !this.mod) return;
    if (this.depthLayers) {
      try { this.depthLayers.dispose(); } catch (e) { }
      this.depthLayers = null;
    } else {
      var out = this.mod.selectTextLayerSeq(0);
      for (var i = 0; i < this.mod.MAX_TEXT_LAYERS; i++) {
        out += this.mod.defineTextLayerSeq(i, 0);
      }
      try { console.write(out); } catch (e) { }
    }
    this.currentLayer = 0;
    this.enabled = false;
  }

  private bandForFrame(frame: Frame): string {
    var node: Frame | null = frame;
    for (var hops = 0; node && hops < 8; hops++) {
      if (node._scene3dBand) return node._scene3dBand;
      try { node = (node as any).parent || null; } catch (e) { return 'glass'; }
    }
    return 'glass';
  }

  private writeDepthTable(): void {
    if (!this.mod) return;
    var out = this.mod.selectTextLayerSeq(0);
    for (var i = 0; i < this.mod.MAX_TEXT_LAYERS; i++) {
      var band = this.order[i];
      var fraction = band === undefined ? 0 : this.depths[band];
      out += this.mod.defineTextLayerSeq(i, this.spread * (fraction === undefined ? 0 : fraction));
    }
    try { console.write(out); } catch (e) { }
  }
}

var scene3d = new SynthKartScene3dRuntime();
