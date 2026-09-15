// =============================================================================
// Partitura del Juego — Audio Launcher
// =============================================================================
// Entry point for the distribution package.  sclang runs this file and it
// automatically chains the two-step boot sequence:
//
//   Step 1 -> pdj_audio_config.scd   (DANTE detection, server reboot)
//   Step 2 -> pdj_datamatics.scd     (engine, SynthDefs, OSC listener)
//
// All three files must be in the same directory.
//
// Distribution: launched via "Start Audio.command".
// Development: select-all this file in the SC IDE and press Cmd+Return.
//   (or evaluate just pdj_audio_config.scd, then pdj_datamatics.scd manually)
// =============================================================================

(
var dir     = PathName(thisProcess.nowExecutingPath).pathOnly;
var cfgPath = dir ++ "pdj_audio_config.scd";
var engPath = dir ++ "pdj_datamatics.scd";

// Server selection:
//   DANTE  -> Server.local (external scsynth). Server.internal ignores
//             s.options.device / outDevice and always uses the system default,
//             making it impossible to route to Dante Virtual Soundcard.
//   Stereo -> Server.internal. The external scsynth is silent on built-in
//             speakers on certain macOS/CoreAudio configurations.
if("PDJ_AUDIO_DEVICE".getenv.notNil and: { "PDJ_AUDIO_DEVICE".getenv != "" }, {
	Server.default = Server.local;
}, {
	Server.default = Server.internal;
});
s = Server.default;

// Verify all files are present before doing anything
[cfgPath, engPath].do { |path|
	if(File.exists(path).not, {
		("PDJ LAUNCHER ERROR: file not found — " ++ path).postln;
		"Ensure pdj_audio_config.scd and pdj_datamatics.scd are in the same".postln;
		"folder as pdj_launcher.scd.".postln;
		thisProcess.hardStop;
	});
};

"".postln;
"============================================================".postln;
"  Partitura del Juego  |  Audio Engine".postln;
"============================================================".postln;

// List all audio devices visible to SC before DANTE detection
"Available audio devices:".postln;
ServerOptions.devices.do { |d| ("  " ++ d).postln };
"".postln;

// --------------------------------------------------------------------------
// Step 1: detect DANTE, configure server options, reboot
// --------------------------------------------------------------------------
"[1/3] Configuring audio device (pdj_audio_config.scd)...".postln;
thisProcess.interpreter.executeFile(cfgPath);

// --------------------------------------------------------------------------
// Step 2: wait for the server — with automatic fallback to built-in speakers
// if the chosen device fails to initialize (e.g. virtual / network devices
// that look external but cannot open a real audio stream).
// --------------------------------------------------------------------------
Routine({
	var bootTimeout = 12;  // seconds to wait before declaring a failure
	var waited      = 0;
	var booted      = false;

	// Poll until the server is running or timeout expires
	while({ s.serverRunning.not and: { waited < bootTimeout } }, {
		1.wait;
		waited = waited + 1;
	});

	booted = s.serverRunning;

	// If the device we chose caused a crash, fall back to built-in speakers
	if(booted.not and: { ~pdjAudioMode != "builtin" }, {
		"".postln;
		("PDJ: device '" ++ (~pdjAudioDevice ? "?") ++ "' failed to open.").postln;
		"PDJ: falling back to built-in speakers (stereo).".postln;
		"".postln;
		s.options.device               = nil;
		s.options.numOutputBusChannels = 2;
		s.options.numInputBusChannels  = 0;
		s.options.sampleRate           = nil;
		s.options.blockSize            = 128;
		~pdjAudioMode   = "builtin";
		~pdjAudioDevice = nil;
		~numSpeakers    = 2;
		s.reboot;

		// Wait again for fallback boot
		waited = 0;
		while({ s.serverRunning.not and: { waited < bootTimeout } }, {
			1.wait;
			waited = waited + 1;
		});
		booted = s.serverRunning;
	});

	if(booted.not, {
		"PDJ FATAL: could not initialize audio on any device.".postln;
		"Check System Settings -> Sound and try again.".postln;
		^nil
	});

	// Server is running — continue as a normal waitForBoot callback
	// (defer to AppClock so SC UI stays responsive)
	AppClock.sched(0, {

	// Print full configuration report
	var modeLabel = switch(~pdjAudioMode,
		"dante",    { "DANTE — 8ch / 48 kHz (DVS ch 1-8)" },
		"external", { "external device — stereo" },
		"builtin",  { "built-in speakers — stereo  [no external device connected]" },
		{ "stereo (unknown mode)" }
	);
	var deviceLabel = (~pdjAudioDevice ?? { "macOS default (built-in)" }).asString;

	"".postln;
	"[2/3] Audio server ready — configuration report:".postln;
	"------------------------------------------------------------".postln;
	("  Mode        : " ++ modeLabel).postln;
	("  Device      : " ++ deviceLabel).postln;
	("  Output ch   : " ++ s.options.numOutputBusChannels).postln;
	("  Sample rate : " ++ s.sampleRate ++ " Hz").postln;
	("  Block size  : " ++ s.options.blockSize ++ " samples").postln;
	("  Speakers    : " ++ ~numSpeakers).postln;
	("  Rendering   : " ++ if(~pdjAudioMode == "dante", {
		"8-channel DBAP"
	}, {
		"traditional stereo Pan2"
	})).postln;
	if(~pdjAudioMode == "dante" and: { ~speakerPositions.notNil }, {
		"  Speaker layout — room coords (x: L->R, y: front->rear):".postln;
		~speakerPositions.do { |pos, i|
			("    [" ++ i ++ "] AC5-ch" ++ (i + 1)
				++ " (DVS " ++ ((~pdjOutputOffset ? 0) + i + 1) ++ ")"
				++ "  x=" ++ pos[0].round(0.01)
				++ "  y=" ++ pos[1].round(0.01)).postln;
		};
	});
	if(~pdjAudioMode == "builtin", {
		"".postln;
		"  To enable 8-channel DANTE output:".postln;
		"    1. Install + license Dante Virtual Soundcard (audinate.com)".postln;
		"    2. Open DVS: set TX=8 RX=2 48kHz, click Enable".postln;
		"    3. Connect Ethernet to the venue switch (same as DANTE 5)".postln;
		"    4. Run Check Audio.command to confirm, then restart".postln;
	});
	("  Memory      : " ++ s.options.memSize ++ " KB").postln;
	"------------------------------------------------------------".postln;
	"".postln;

	// --------------------------------------------------------------------------
	// Step 3: load the engine
	// --------------------------------------------------------------------------
	"[3/3] Loading engine (pdj_datamatics.scd)...".postln;
	thisProcess.interpreter.executeFile(engPath);

	// --------------------------------------------------------------------------
	// Post-boot guidance and audio test
	// --------------------------------------------------------------------------
	AppClock.sched(0.5, {

		"".postln;
		"============================================================".postln;
		case
		{ ~pdjAudioMode == "dante" } {
			"  Ready.  8-channel DANTE output active.".postln;
			("  Output offset : bus " ++ (~pdjOutputOffset ? 0)
				++ " (DVS ch " ++ ((~pdjOutputOffset ? 0) + 1) ++ "-"
				++ ((~pdjOutputOffset ? 0) + 8) ++ " -> AmpCrown-5)").postln;
			"  Starting DANTE speaker test -- one channel at a time...".postln;
			"============================================================".postln;
			"".postln;
			Routine({
				var offset = ~pdjOutputOffset ? 0;
				8.do { |ch|
					var dvsChannel = offset + ch + 1;
					("  >> Speaker " ++ (ch + 1) ++ " of 8  (DVS ch "
						++ dvsChannel ++ " -> AmpCrown-5 input " ++ (ch + 1) ++ ")").postln;
					{
						var env = EnvGen.ar(Env.perc(0.01, 0.8), doneAction: 2);
						var tone = SinOsc.ar(440 + (ch * 110), 0, 0.35) * env;
						Out.ar(offset + ch, tone);
					}.play;
					1.2.wait;
				};
				"".postln;
				"  >> All 8 channels tested.".postln;
				"  If you heard tones on the AmpCrown-5 speakers, DANTE is confirmed.".postln;
				"  If silent: check Dante Controller subscriptions and amp power.".postln;
				"  Run  ~speakerTest.();  to repeat the test anytime.".postln;
				"".postln;
				"  Start Partitura_del_Juego.app to begin.".postln;
			}).play(AppClock);
		}
		{ ~pdjAudioMode == "external" } {
			"  Ready.  Stereo output on external device.".postln;
			"  Start Partitura_del_Juego.app to begin.".postln;
			// Brief sweep to confirm hardware path
			AppClock.sched(1.5, {
				"  [ audio check: 2-second sweep on external device ]".postln;
				{
					var env = EnvGen.ar(Env.linen(0.05, 1.6, 0.3), doneAction: 2);
					var sig = SinOsc.ar(XLine.kr(440, 1320, 1.6), 0, 0.25) * env;
					Out.ar(0, sig ! s.options.numOutputBusChannels)
				}.play;
				nil
			});
			"  Waiting for live OSC from Partitura_del_Juego.app.".postln;
		}
		{ // builtin or any other fallback
			"  Ready.  Routing to built-in speakers.".postln;
			// Brief sweep to confirm built-in path
			AppClock.sched(1.5, {
				"  [ audio check: 2-second sweep on built-in speakers ]".postln;
				{
					var env = EnvGen.ar(Env.linen(0.05, 1.6, 0.3), doneAction: 2);
					var sig = SinOsc.ar(XLine.kr(440, 1320, 1.6), 0, 0.25) * env;
					Out.ar(0, sig ! s.options.numOutputBusChannels)
				}.play;
				nil
			});
			"  Waiting for live OSC from Partitura_del_Juego.app.".postln;
			"  Synthetic OSC remains manual and must not run with the app.".postln;
		};
		"============================================================".postln;
		"".postln;

		nil
	});

	// --------------------------------------------------------------------------
	// Periodic health log — prints to this terminal every 2 minutes
	// --------------------------------------------------------------------------
	AppClock.sched(120, {
		Routine({
			loop {
				var nch  = if(~ch.notNil,      { ~ch.size.asString }, { "?" });
				var srvc = if(s.serverRunning, { "running" },         { "STOPPED" });
				var mode = (~pdjAudioMode ?? { "?" }).asString;
				var spk  = ~numSpeakers.asString;
				("[PDJ health] server:" ++ srvc ++ "  mode:" ++ mode
					++ "  speakers:" ++ spk ++ "  channels:" ++ nch).postln;
				120.wait;
			}
		}).play(AppClock);
		nil
	});

		nil  // end AppClock.sched(0)
	});
}).play(AppClock);
)
