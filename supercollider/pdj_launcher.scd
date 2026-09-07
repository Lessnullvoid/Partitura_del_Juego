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
// Step 2: wait for the server that pdj_audio_config.scd rebooted
// --------------------------------------------------------------------------
s.waitForBoot({

	// Print full configuration report
	"".postln;
	"[2/3] Audio server ready — configuration report:".postln;
	"------------------------------------------------------------".postln;
	("  DANTE mode  : " ++ if(~pdjDante, { "YES  (8ch / 48 kHz)" }, { "NO   (stereo fallback)" })).postln;
	("  Device      : " ++ (s.options.device ?? { "(system default)" })).postln;
	("  Output ch   : " ++ s.options.numOutputBusChannels).postln;
	("  Input ch    : " ++ s.options.numInputBusChannels).postln;
	("  Sample rate : " ++ s.sampleRate ++ " Hz").postln;
	("  Block size  : " ++ s.options.blockSize ++ " samples").postln;
	("  ~numSpeakers: " ++ ~numSpeakers).postln;
	if(~pdjDante and: { ~speakerPositions.notNil }, {
		"  Speaker layout — room coords (x: L>R, y: front>rear):".postln;
		~speakerPositions.do { |pos, i|
			("    [" ++ i ++ "] D3-" ++ (i + 1) ++ "  x=" ++ pos[0].round(0.01)
				++ "  y=" ++ pos[1].round(0.01)).postln;
		};
	}, {
		"  Speaker layout: stereo virtual L/R at x=0 and x=1".postln;
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
	// Periodic health log — prints to this terminal every 2 minutes
	// Useful for venue monitoring; does not affect audio.
	// --------------------------------------------------------------------------
	AppClock.sched(60, {
		Routine({
			loop {
				var nch  = if(~ch.notNil,      { ~ch.size.asString }, { "?" });
				var srvc = if(s.serverRunning, { "running" },         { "STOPPED" });
				var dnte = if(~pdjDante,       { "DANTE" },           { "stereo" });
				var spk  = ~numSpeakers.asString;
				("[PDJ health] server:" ++ srvc ++ "  mode:" ++ dnte
					++ "  speakers:" ++ spk ++ "  channels:" ++ nch).postln;
				120.wait;
			}
		}).play(AppClock);
		nil
	});
});
)
