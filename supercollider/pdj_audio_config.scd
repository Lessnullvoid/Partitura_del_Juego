// =============================================================================
// Partitura del Juego — Audio Device Configuration
// =============================================================================
// Run this block ONCE before loading pdj_datamatics.scd each session.
// It scans all audio devices and selects the best available output:
//
//   DANTE detected   -> 8 output channels @ 48 kHz  (AmpCrown via DVS)
//   External device  -> 2 output channels @ system rate  (stereo on interface)
//   No external      -> 2 output channels on built-in speakers  (safe fallback)
//
// The selected mode is stored in  ~pdjAudioMode  (string):
//   "dante"    "external"    "builtin"
//
// Boot order:
//   1. Enable Dante Virtual Soundcard (menu-bar app, TX=8 RX=2 48kHz).
//      — skip if not using DANTE.
//   2. Evaluate this file (select all -> Cmd+Return).
//   3. Wait for "Server ready" in the Post Window.
//   4. Evaluate pdj_datamatics.scd.
//
// To list all available audio devices in the Post Window:
//   ServerOptions.devices.do({ |d| d.postln });
// =============================================================================

(
var allDevices = ServerOptions.devices;

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------
// String.find(substr) returns index or nil — safe substring check in SC 3.x
var strContains = { |haystack, needle| haystack.find(needle).notNil };

var isDanteDevice = { |name|
	var lower = name.asString.toLower;
	strContains.(lower, "dante") or: { strContains.(lower, "dvs") }
		or: { strContains.(lower, "virtual soundcard") }
};

// Exclude built-in Mac hardware AND virtual/software audio devices.
// Virtual devices look "external" to SC but have no real speaker output;
// using them causes CoreAudio to crash the server at boot.
var isBuiltIn = { |name|
	var lower = name.asString.toLower;
	// Mac hardware
	strContains.(lower, "built-in") or: { strContains.(lower, "built in") }
		or: { strContains.(lower, "macbook") }
		or: { strContains.(lower, "imac") }
		or: { strContains.(lower, "mac mini") }
		or: { strContains.(lower, "mac pro") }
		or: { strContains.(lower, "mac studio") }
		or: { strContains.(lower, "microphone") }
		or: { strContains.(lower, "aggregate") }
		or: { strContains.(lower, "multi-output") }
		// Virtual routing / loopback drivers
		or: { strContains.(lower, "blackhole") }
		or: { strContains.(lower, "loopback") }
		or: { strContains.(lower, "soundflower") }
		// Network / app virtual audio devices (no real speaker output)
		or: { strContains.(lower, "ndi") }          // NDI Audio
		or: { strContains.(lower, "iriun") }        // Iriun Webcam
		or: { strContains.(lower, "zoom") }         // ZoomAudioDevice
		or: { strContains.(lower, "webcam") }       // any webcam virtual audio
		or: { strContains.(lower, "teams") }        // Microsoft Teams
		or: { strContains.(lower, "discord") }      // Discord virtual audio
};

// ------------------------------------------------------------------
// Check for device pre-selected by Start Audio.command (env vars)
// PDJ_AUDIO_DEVICE = exact device name (empty = built-in default)
// ------------------------------------------------------------------
var envDevice = "PDJ_AUDIO_DEVICE".getenv;

// ------------------------------------------------------------------
// Device scan (used when no env override is present)
// ------------------------------------------------------------------
var danteDevice    = allDevices.detect(isDanteDevice);
var builtInOutput  = allDevices.detect({ |d|
	var lower = d.asString.toLower;
	strContains.(lower, "speaker") or: { strContains.(lower, "output") }
});
var externalDevice = if(envDevice.notNil and: { envDevice != "" }, {
	// Use whatever the user picked, bypassing the virtual-device filter
	allDevices.detect({ |d| d.asString == envDevice })
		?? { allDevices.detect({ |d| d.asString.find(envDevice).notNil }) }
}, {
	allDevices.detect({ |d|
		isDanteDevice.(d).not and: { isBuiltIn.(d).not }
	})
});

// ------------------------------------------------------------------
// If user explicitly chose built-in (envDevice == ""), force builtin mode
// ------------------------------------------------------------------
var forcedBuiltin = (envDevice.notNil and: { envDevice == "" } and: { danteDevice.isNil });

// ------------------------------------------------------------------
// Configure server and store mode
// ------------------------------------------------------------------
if(danteDevice.notNil, {

	// ---- DANTE 8-channel mode ----------------------------------------
	// Server.local is used for DANTE (set in pdj_launcher.scd) because
	// Server.internal ignores device selection on macOS.
	// SC outputs 8 channels on DVS ch 1-8. Route them to AmpCrown-5
	// (or whichever amp) in Dante Controller.
	s.options.device               = danteDevice;
	s.options.numOutputBusChannels = 8;
	s.options.numInputBusChannels  = 0;
	s.options.sampleRate           = 48000;
	s.options.blockSize            = 256;  // safer than 128 for network audio

	~pdjDante        = true;
	~pdjAudioMode    = "dante";
	~pdjAudioDevice  = danteDevice;
	~numSpeakers     = 8;
	~pdjOutputOffset = 0;

	"".postln;
	"PDJ audio >> DANTE mode".postln;
	("  Device  : " ++ danteDevice).postln;
	"  Output  : 8ch @ 48 kHz (DVS ch 1-8)".postln;

}, {
	// No DANTE — stereo in all remaining cases
	s.options.numOutputBusChannels = 2;
	s.options.numInputBusChannels  = 0;     // output-only; the piece uses no audio input
	s.options.sampleRate           = nil;   // inherit system sample rate
	s.options.blockSize            = 128;

	~pdjDante    = false;
	~numSpeakers = 2;

	if(externalDevice.notNil and: { forcedBuiltin.not }, {

		// ---- External interface (USB, Thunderbolt, etc.) ----------------
		s.options.device  = externalDevice;
		~pdjAudioMode     = "external";
		~pdjAudioDevice   = externalDevice;

		"".postln;
		"PDJ audio >> external device (stereo)".postln;
		("  Device  : " ++ externalDevice).postln;
		"  Output  : 2ch @ system rate".postln;
		"  Note    : DANTE not found; connect Ethernet + enable DVS for 8ch".postln;

	}, {

		// ---- No external device (or user chose built-in) ----------------
		// Bind the output explicitly. On systems with virtual/aggregate
		// devices, CoreAudio's default alias can open successfully but route
		// the server stream somewhere other than the selected speakers.
		s.options.device    = nil;
		s.options.outDevice = builtInOutput;
		~pdjAudioMode     = "builtin";
		~pdjAudioDevice   = builtInOutput;

		"".postln;
		if(forcedBuiltin, {
			"PDJ audio >> built-in speakers (stereo)  [user selected]".postln;
		}, {
			"PDJ audio >> built-in speakers (stereo)".postln;
			"  Device  : no external audio device detected".postln;
		});
		("  Device  : " ++ (builtInOutput ?? { "macOS default" })).postln;
		"  Output  : 2ch on built-in speakers".postln;
		"  Note    : connect a DANTE (8ch) or USB interface (stereo) for".postln;
		"            external sound. Audio works as-is for local testing.".postln;

	});
});

s.options.memSize = 65536;  // 64 MB sample memory

// Reboot with new options. pdj_datamatics.scd reads ~numSpeakers after boot.
s.reboot;
)
