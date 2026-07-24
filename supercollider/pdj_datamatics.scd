/*
================================================================================
  Partitura del Juego — Datamatics Audio Engine (SuperCollider)
  Ikeda-authentic datamatics: data as material/structure/signal, elementary
  binary units, a click->rhythm->pitch->noise continuum, module-based montage
  with hard cuts, and a recursive "meta-datamatics" feedback loop. No melody,
  no harmony, no scale/chord system — structure comes from grids, thresholds,
  density, and discontinuity, per Ikeda's own documented method.

  Voice aesthetic: drone/ambient-noise oriented rather than raw glitch. The
  atmospheric layers (wash, noise bed) orbit one shared, slowly drifting
  spectral center for coherence; the tone bank beats gently between fixed
  partials for warmth; the pulse engine favors sustained resonance over dry
  clicks; and a proper reverb tail (not just delay) gives the space register
  a real sense of depth. Transients (clicks/glitches) punctuate this bed —
  they no longer dominate it.

  Receives OSC from openFrameworks (OSCSender) on UDP port 9001:
    /pdj/channel/{0-3}/flow/magnitude|angle
    /pdj/channel/{0-3}/motion/energy
    /pdj/channel/{0-3}/blobs/count
    /pdj/channel/{0-3}/contour/length
    /pdj/channel/{0-3}/blob/{0-7}/state
        args: active, x, y, vx, vy, area, bbW, bbH
    /pdj/channel/{0-3}/event/collision|ball|ball/x|ball/y|crowd|leg_distance
    /pdj/channel/{0-3}/score/mode   (0-13)
    /pdj/channel/{0-3}/state/frame|time
    /pdj/channel/{0-3}/video/name|position|duration|revision
    /pdj/channel/{0-3}/score/revision
    /pdj/channel/{0-3}/director/temporal|speed|clear|clear_alpha

  Usage:
    1. Boot this file (Cmd+Enter on the outer block, or evaluate sections top-down)
    2. Launch Partitura_del_Juego — OSC host/port must match (~localhost:9001)
    3. Optional: evaluate ~testOsc.play for a synthetic feed without oF

  Stop:  Cmd+.   or   ~shutdown.()
================================================================================
*/

(
s.waitForBoot({

	// --------------------------------------------------------------------------
	// CONFIG
	// --------------------------------------------------------------------------
	~oscPort     = 9001;
	~numChannels = 4;
	~masterGain  = 0.55;

	// Short-term organic wander on top of the module baseline below — never
	// large enough to obscure the module's identity, just keeps it alive.
	~macro = (
		noise:   0.10,
		space:   0.70,
		density: 0.10
	);
	~crushBase = 0.0; // module-set baseline for quantization amount

	// The video score is the form. Each GraphicScore mode selects a stable
	// cinematic sound scene; live CV moves *inside* that scene instead of
	// replacing it. Values are deliberately restrained: drone is continuous,
	// pulse is foreground detail, air is the noise-field brightness.
	~modeProfiles = [
		(name: \bwClean,      voice: 0, tension: 0.04, drone: 0.72, air: 0.16, pulse: 0.00, color: 0.18, space: 0.82, crush: 0.00),
		(name: \scanLine,     voice: 1, tension: 0.18, drone: 0.62, air: 0.28, pulse: 0.08, color: 0.58, space: 0.76, crush: 0.02),
		(name: \bboxTracker,  voice: 1, tension: 0.22, drone: 0.58, air: 0.20, pulse: 0.12, color: 0.42, space: 0.68, crush: 0.02),
		(name: \binaryText,   voice: 2, tension: 0.32, drone: 0.48, air: 0.24, pulse: 0.25, color: 0.72, space: 0.66, crush: 0.08),
		(name: \waveform,     voice: 3, tension: 0.14, drone: 0.70, air: 0.25, pulse: 0.06, color: 0.32, space: 0.84, crush: 0.00),
		(name: \gridData,     voice: 2, tension: 0.28, drone: 0.50, air: 0.22, pulse: 0.22, color: 0.64, space: 0.70, crush: 0.06),
		(name: \barcode,      voice: 2, tension: 0.42, drone: 0.44, air: 0.32, pulse: 0.28, color: 0.82, space: 0.62, crush: 0.10),
		(name: \videoNormal,  voice: 0, tension: 0.06, drone: 0.82, air: 0.18, pulse: 0.00, color: 0.12, space: 0.90, crush: 0.00),
		(name: \videoSquares, voice: 3, tension: 0.16, drone: 0.66, air: 0.24, pulse: 0.08, color: 0.38, space: 0.82, crush: 0.01),
		(name: \videoNumbers, voice: 2, tension: 0.30, drone: 0.56, air: 0.28, pulse: 0.18, color: 0.68, space: 0.74, crush: 0.05),
		(name: \videoLines,   voice: 3, tension: 0.20, drone: 0.64, air: 0.34, pulse: 0.05, color: 0.50, space: 0.86, crush: 0.01),
		(name: \thermal,      voice: 0, tension: 0.20, drone: 0.88, air: 0.12, pulse: 0.00, color: 0.08, space: 0.88, crush: 0.00),
		(name: \slitScan,     voice: 3, tension: 0.24, drone: 0.78, air: 0.30, pulse: 0.03, color: 0.28, space: 0.96, crush: 0.02),
		(name: \flash,        voice: 2, tension: 0.78, drone: 0.20, air: 0.06, pulse: 0.00, color: 0.90, space: 0.98, crush: 0.12)
	];

	// Pure UGen-graph helper (no bus access) — combines bit-depth rounding
	// with sample-and-hold rate reduction: the audible signature of digital
	// quantization limits (section 11 of the analysis). Built from core
	// UGens only (round + Latch), no bitcrusher class required.
	~crushSignal = { |sig, amt|
		var bits    = amt.linlin(0, 1, 24, 3);
		var levels  = 2.0.pow(bits);
		var holdHz  = amt.linexp(0.001, 1, 44100, 1200);
		var quantized = (sig * levels).round(1) / levels;
		Latch.ar(quantized, Impulse.ar(holdHz));
	};

	// Language-side helper — literal binary sonification: an 8-bit read-out
	// of a real detected value, no metaphor involved.
	~binaryBits = { |n, digits = 8|
		Array.fill(digits, { |i| (n >> (digits - 1 - i)) & 1 });
	};

	// ScoreMode → montage-cut weight (matches GraphicScore.h enum). Used only
	// to scale the transient glitch burst fired on a score/mode change —
	// this is a genuine discontinuity in the visual signal, so it deserves
	// an audible cut, not a melodic response.
	// 0 BwClean … 13 Flash
	~modeGlitch = [
		0.05, 0.35, 0.25, 0.55, 0.40, 0.50, 0.70, 0.10,
		0.30, 0.60, 0.45, 0.55, 0.65, 1.00
	];

	// --------------------------------------------------------------------------
	// FORM — a module state-machine following the analysis's own formal model
	// (section 17) directly: Calibration -> Codification -> Accumulation ->
	// DimensionalExpansion -> Saturation -> Rupture -> Recursion. Each module
	// sets its allowed pulse-rate register, its "dimension" (how many layers
	// are simultaneously active), and whether the grid is locked to incoming
	// OSC events (micro-synchrony) or runs independently (contrapuntal).
	// --------------------------------------------------------------------------
	~form = (
		name:       \calibration,
		dimension:  1,       // 1..4 active layers: tone / pulse+grid / noise field / space
		rateLo:     0.3,     // Hz — lower bound of this module's click/pulse register
		rateHi:     3,       // Hz — upper bound (drives the click->rhythm->pitch->noise sweep)
		lockToOSC:  true     // true: grid fires from OSC events; false: grid runs on its own clock
	);
	~gridStep = 0;

	// --------------------------------------------------------------------------
	// STATE (smoothed CV mirrors oF stream)
	// --------------------------------------------------------------------------
	~ch = Array.fill(~numChannels, {
		(
			energy:    0.0,
			flowMag:   0.0,
			flowAng:   0.0,
			blobs:     0,
			contour:   0.0,
			crowd:     0.0,
			leg:       0.0,
			mode:      0,
			ballX:     0.5,
			ballY:     0.5,
			blob0X:    0.5,
			blob0Y:    0.5,
			blob0Vx:   0.0,
			blob0Vy:   0.0,
			blobData:  Array.fill(8, {
				(active: 0, x: 0.0, y: 0.0, vx: 0.0, vy: 0.0, area: 0.0, bbW: 0.0, bbH: 0.0)
			}),
			frame:      0,
			sourceTime: 0.0,
			lastSeen:   0.0,
			online:     false,
			dropped:    0,
			videoName:  "",
			videoPos:   0.0,
			videoDur:   0.0,
			evolutionStage: -1,
			videoRevision: -1,
			modeRevision: 0,
			temporalPhase: 0,
			speedMultiplier: 1.0,
			clearPhase: 0,
			clearAlpha: 0.0,
			prevColl:  0,
			prevBall:  0,
			lastGlitch: 0.0,
			lastImpact: 0.0,
			lastBall:   0.0,
			lastKick:   0.0,
			scene:      ~modeProfiles[0],
			ampBus:    Bus.control(s, 1),
			panBus:    Bus.control(s, 1),
			rateBus:   Bus.control(s, 1),
			noiseBus:  Bus.control(s, 1),
			tensionBus: Bus.control(s, 1)
		)
	});

	// Global control buses — module/meta-feedback driven, audio-rate readers
	// .lag() these so changes glide in over a fraction of a second (fast
	// enough to read as a cut at module scale, slow enough to avoid clicks).
	~gNoiseBus   = Bus.control(s, 1);
	~gDensityBus = Bus.control(s, 1);
	~gSpaceBus   = Bus.control(s, 1);
	~gCrushBus   = Bus.control(s, 1); // quantization amount (threshold technique)
	~gGate1Bus   = Bus.control(s, 1); // dimension layer 1: tone bank
	~gGate2Bus   = Bus.control(s, 1); // dimension layer 2: pulse engine
	~gGate3Bus   = Bus.control(s, 1); // dimension layer 3: noise field
	~selfBus     = Bus.control(s, 1); // meta-datamatics: master output level fed back in
	~gSpectralBus = Bus.control(s, 1); // shared drifting center freq — atmosphere coherence
	~gMotionBus  = Bus.control(s, 1); // integrated motion, never raw frame-rate CV
	~gCrowdBus   = Bus.control(s, 1); // integrated crowd mass
	~gTensionBus = Bus.control(s, 1); // global cinematic pressure, fast rise / slow release

	~ch.do { |st, i|
		st.ampBus.set(0);
		st.panBus.set(i.linlin(0, 3, -0.85, 0.85));
		st.rateBus.set(1);
		st.noiseBus.set(0);
		st.tensionBus.set(st.scene.tension);
	};
	~gNoiseBus.set(~macro.noise);
	~gDensityBus.set(~macro.density);
	~gSpaceBus.set(~macro.space);
	~gCrushBus.set(~crushBase);
	~gMotionBus.set(0);
	~gCrowdBus.set(0);
	~gTensionBus.set(0.05);
	~gGate1Bus.set(1); ~gGate2Bus.set(1); ~gGate3Bus.set(1);

	thisProcess.interpreter.executeFile(
		"/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/supercollider/pdj_mode_voices.scd"
	);
	~installModeVoices.value;

	// --------------------------------------------------------------------------
	// SYNTHDEFS — elementary binary units only: sines, noise, impulses,
	// silence. No pitched/melodic voice.
	// --------------------------------------------------------------------------

	// Four related drone families, selected by the visual scene and slowly
	// crossfaded. Channel variant changes ratios and drift rates without
	// breaking the common spectral language:
	//   0 abyss/film, 1 scan/surveillance, 2 data/glass, 3 temporal/haze.
	SynthDef(\pdjToneBank, { |out = 0, ampBus, panBus, gateBus, motionBus, crowdBus,
		tensionBus, subFreq = 36, highFreq = 13500, sceneAmp = 0.7, color = 0.2,
		voice = 0, variant = 0, master = 0.22|
		var activity = In.kr(ampBus, 1).lag(4);
		var pan      = In.kr(panBus, 1).lag(3);
		var gate     = In.kr(gateBus, 1).lag(5);
		var motion   = In.kr(motionBus, 1).lag(4);
		var crowd    = In.kr(crowdBus, 1).lag(6);
		var tension  = In.kr(tensionBus, 1).lagud(0.45, 8).clip(0, 1);
		var presence = sceneAmp.lag(5) * (0.19 + (activity * 0.32) + (crowd * 0.16) + (tension * 0.10));
		var v        = variant.clip(0, 3);
		var detune   = 1 + (tension.squared * (0.004 + (v * 0.0015)));
		var drift    = LFNoise2.kr([0.007, 0.011, 0.015, 0.019] + (v * 0.001)).range(-0.14, 0.14);
		var abyss    = Mix.ar(SinOsc.ar(
			(subFreq * [1, 1.0028 + (v * 0.0004), 2.004, 4.009] * detune) + drift,
			0, [0.40, 0.28, 0.12, 0.045]
		));
		var scan     = Mix.ar(SinOsc.ar(
			(subFreq * [1, 3.013 + (v * 0.009), 7.031 + (v * 0.013)] * detune) + drift.keep(3),
			0, [0.32, 0.12, 0.045]
		)) + (BPF.ar(PinkNoise.ar, subFreq * (10 + v), 0.035 + (tension * 0.08)) * 0.16);
		var data     = Mix.ar(SinOsc.ar(
			subFreq * [1, 8.03 + (v * 0.03), 13.07 + (v * 0.04)] * detune,
			0, [0.25, 0.055, 0.035]
		)) + (BPF.ar(GrayNoise.ar, subFreq * color.linexp(0, 1, 18, 70), 0.025 + (tension * 0.06)) * 0.11);
		var haze     = (abyss * 0.52)
			+ (BPF.ar(BrownNoise.ar, subFreq * (5.5 + v), 0.045 + (motion * 0.11)) * 0.28)
			+ (BPF.ar(PinkNoise.ar, subFreq * (15 + (v * 2)), 0.06 + (tension * 0.08)) * 0.13);
		var air      = SinOsc.ar(
			highFreq * (1 + LFNoise2.kr(0.019 + (v * 0.003)).range(-0.0015, 0.0015)),
			0, 0.012 * color * (0.35 + tension)
		);
		var sig      = SelectX.ar(voice.lag(4).clip(0, 3), [abyss, scan, data, haze]) + air;
		sig = LPF.ar(sig, tension.linexp(0, 1, 1800, 10500));
		sig = LeakDC.ar(sig * presence * gate * master);
		Out.ar(out, Pan2.ar(sig, pan * 0.65));
	}).add;

	// The click -> rhythm -> pitch -> noise continuum, in one generator.
	// A single Impulse train whose rate is driven continuously by live CV
	// data: slow rate reads as discrete clicks, moderate rate as rhythm,
	// fast rate fuses into pitch/timbre, very fast rate reads as noise.
	// Ringz at the same rate lets pitch emerge naturally from repetition.
	// The resonance is weighted heavily over the raw impulse and given a
	// longer ring time, so the generator reads as a sustained, evolving
	// tone/texture rather than a dry click train — elegant, not percussive.
	SynthDef(\pdjPulseEngine, { |out = 0, rateBus, ampBus, panBus, gateBus, crushBus,
		tensionBus, sceneAmp = 0, color = 0.5, variant = 0, master = 0.12|
		var rate  = In.kr(rateBus, 1).lag(2).clip(0.15, 60);
		var amp   = In.kr(ampBus, 1).lag(3);
		var pan   = In.kr(panBus, 1).lag(2);
		var gate  = In.kr(gateBus, 1).lag(4);
		var crush = In.kr(crushBus, 1).lag(0.4).clip(0, 1);
		var tension = In.kr(tensionBus, 1).lagud(0.35, 6);
		var imp   = Impulse.ar(rate);
		var center = color.linexp(0, 1, 700, 7200) * (1 + (variant * 0.045));
		var sig   = Ringz.ar(
			imp,
			center * [1, 1.006 + (variant * 0.001)] * (1 + (tension * 0.012)),
			0.035 + (color * 0.08) + (tension * 0.04)
		).sum * 0.35;
		sig = BPF.ar(sig, center, 0.18);
		sig = ~crushSignal.(sig, crush * 0.45);
		sig = sig * amp.sqrt * sceneAmp.lag(5) * (0.55 + (tension * 0.45)) * gate * master;
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Sparse high-click / data impulse (collision / ball / grid read-outs).
	// panEnd != panStart -> the click sweeps across the stereo field,
	// direction/speed set from the detected blob's velocity. Slightly
	// longer decay and softer noise component than before — a punctuation
	// mark on the drone bed, not a spike above it.
	SynthDef(\pdjClick, { |out = 0, freq = 8000, amp = 0.4, panStart = 0, panEnd = 0, decay = 0.016|
		var env = EnvGen.ar(Env.perc(0.0006, decay, 1, -6), doneAction: 2);
		var pan = Line.kr(panStart, panEnd, decay * 4).clip(-1, 1);
		var sig = Impulse.ar(0) * env;
		sig = Ringz.ar(sig, freq, decay * 0.9) * 0.4
			+ (WhiteNoise.ar * env * 0.08);
		sig = HPF.ar(sig, 1800);
		Out.ar(out, Pan2.ar(sig * amp, pan));
	}).add;

	// Short digital glitch burst — montage cuts (mode changes, blob bursts,
	// the Rupture module's noise burst). Not tied to any pitch/scale.
	SynthDef(\pdjGlitch, { |out = 0, amp = 0.25, panStart = 0, panEnd = 0, rate = 40, dur = 0.06|
		var env = EnvGen.ar(Env.linen(0.004, dur, 0.02), doneAction: 2);
		var pan = Line.kr(panStart, panEnd, dur).clip(-1, 1);
		var clk = Impulse.ar(rate);
		var sig = Latch.ar(WhiteNoise.ar, clk);
		sig = RHPF.ar(sig, TExpRand.ar(1500, 9000, clk), 0.35);
		sig = sig * env * amp;
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Meaningful event foreground: collision is a low cinematic pressure
	// wave, ball detection is a short spatial trace. Neither is a generic
	// broadband glitch; both preserve the ambient field.
	SynthDef(\pdjImpact, { |out = 0, amp = 0.3, pan = 0, depth = 0.5|
		var env = EnvGen.ar(Env.perc(0.008, 2.8, 1, -5), doneAction: 2);
		var freq = XLine.kr(62 + (depth * 24), 28, 1.8);
		var low = SinOsc.ar(freq, 0, 0.55) + (BPF.ar(BrownNoise.ar, 90, 0.25) * 0.25);
		var edge = BPF.ar(PinkNoise.ar, 900 + (depth * 1800), 0.18) * 0.08;
		Out.ar(out, Pan2.ar((low + edge) * env * amp, pan));
	}).add;

	SynthDef(\pdjTrace, { |out = 0, amp = 0.16, panStart = 0, panEnd = 0, height = 0.5|
		var env = EnvGen.ar(Env.perc(0.01, 1.4, 1, -6), doneAction: 2);
		var pan = Line.kr(panStart, panEnd, 1.2);
		var cf = height.linexp(0, 1, 650, 5200);
		var sig = BPF.ar(PinkNoise.ar, cf * XLine.kr(1.2, 0.72, 1.2), 0.07);
		Out.ar(out, Pan2.ar(sig * env * amp, pan));
	}).add;

	// GlobalDirector clear service: a diffuse spectral lift marks the wipe
	// while clearAlpha removes mass from the persistent channel drones.
	SynthDef(\pdjClearSweep, { |out = 0, amp = 0.10, dur = 2.2|
		var env = EnvGen.ar(Env.linen(0.25, dur, 1.4, 1, \sine), doneAction: 2);
		var center = XLine.kr(260, 6200, dur + 1.65);
		var sig = BPF.ar(PinkNoise.ar, center, 0.09)
			+ (BPF.ar(BrownNoise.ar, center * 0.22, 0.12) * 0.45);
		sig = [DelayC.ar(sig, 0.08, 0.017), DelayC.ar(sig, 0.08, 0.053)];
		Out.ar(out, sig * env * amp);
	}).add;

	// Shared slow spectral center — a single wandering frequency that the
	// wash and noise-bed layers both orbit, so the ambient/atmospheric
	// layers read as one coherent body rather than independently-tuned
	// noise sources. This is the main "compositional coherence" device.
	SynthDef(\pdjSpectralAnchor, { |out, target = 480|
		var anchor = target.lag(8) * LFNoise2.kr(0.018).range(0.975, 1.025);
		Out.kr(out, anchor);
	}).add;

	// Motion-driven noise grain (scanline / contour hiss) — elementary
	// broadband noise, gated by dimension layer 3, quantized by crush.
	// Its filter center follows the shared spectral anchor (see
	// pdjSpectralAnchor) rather than wandering independently, so it reads
	// as part of the same sonic body as the wash instead of separate hiss.
	SynthDef(\pdjNoiseBed, { |out = 0, noiseBus, panBus, gateBus, crushBus, spectralBus,
		motionBus, tensionBus, sceneAmp = 0.2, color = 0.3, voice = 0, variant = 0, master = 0.12|
		var amp     = In.kr(noiseBus, 1).lag(0.15);
		var pan     = In.kr(panBus, 1).lag(3);
		var gate    = In.kr(gateBus, 1).lag(5);
		var crush   = In.kr(crushBus, 1).lag(0.4).clip(0, 1);
		var anchor  = In.kr(spectralBus, 1).lag(1.5);
		var motion  = In.kr(motionBus, 1).lag(3);
		var tension = In.kr(tensionBus, 1).lagud(0.45, 8);
		var center  = anchor * color.linexp(0, 1, 0.55, 3.5)
			* (1 + (voice.lag(4) * 0.18)) * (1 + (variant * 0.035));
		var sig     = BPF.ar(PinkNoise.ar, center, 0.055 + (motion * 0.08) + (tension * 0.09));
		sig = sig + (BPF.ar(BrownNoise.ar, center * 0.31, 0.10 + (tension * 0.05)) * 0.42);
		sig = sig + (BPF.ar(GrayNoise.ar, center * (2.3 + (variant * 0.2)), 0.04) * tension.squared * 0.12);
		sig = ~crushSignal.(sig, crush * 0.3);
		Out.ar(out, Pan2.ar(sig * (0.12 + amp.sqrt) * (0.7 + (tension * 0.3)) * sceneAmp.lag(5) * gate * master, pan));
	}).add;

	// Crowd density → sub / low sine pressure — elementary sub-bass unit,
	// independent of the tone bank's fixed sub (this one tracks crowd, not
	// activity), still fixed-register, not melodic.
	SynthDef(\pdjSub, { |out = 0, amp = 0, freq = 55, pan = 0|
		var a   = amp.lag(6);
		var sig = (SinOsc.ar(freq) + (SinOsc.ar(freq * 1.005) * 0.45)) * a * 0.22;
		sig = LPF.ar(sig, 120);
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Continuously evolving textural wash — the piece's main drone/ambient
	// carrier (dimension layer 3). Its center frequency wanders around the
	// shared spectral anchor (narrower, more resonant bandwidth than
	// before) so it "sings" rather than hisses — an elegant, coherent bed
	// that the noise bed and pulse engines sit on top of.
	SynthDef(\pdjWash, { |out = 0, noiseBus, gateBus, crushBus, spectralBus,
		motionBus, crowdBus, tensionBus, master = 0.13|
		var amt    = In.kr(noiseBus, 1).lag(2.5);
		var gate   = In.kr(gateBus, 1).lag(0.4);
		var crush  = In.kr(crushBus, 1).lag(0.6).clip(0, 1);
		var anchor = In.kr(spectralBus, 1).lag(2);
		var motion = In.kr(motionBus, 1).lag(5);
		var crowd  = In.kr(crowdBus, 1).lag(8);
		var tension = In.kr(tensionBus, 1).lagud(0.6, 10);
		var cf     = anchor * LFNoise1.kr(0.025).exprange(0.72, 1.38);
		var bw     = 0.035 + (motion * 0.08) + (tension * 0.09);
		var pan    = LFNoise1.kr(0.035 + (tension * 0.025)) * (0.55 + (tension * 0.35));
		var sig    = BPF.ar(PinkNoise.ar, cf, bw);
		sig = sig + (BPF.ar(BrownNoise.ar, cf * 0.24, 0.09) * (0.45 + (crowd * 0.35)));
		sig = sig + (BPF.ar(GrayNoise.ar, cf * (3.2 + tension), 0.035 + (tension * 0.04)) * motion * (0.08 + (tension * 0.10)));
		sig = ~crushSignal.(sig, crush * 0.25);
		Out.ar(out, Pan2.ar(sig * (0.15 + amt) * (0.72 + (tension * 0.28)) * gate * master, pan));
	}).add;

	// Data crackle / dust — density tracks macro noise, each grain gets a
	// random spatial position; part of the global noise field (layer 3).
	SynthDef(\pdjCrackle, { |out = 0, noiseBus, gateBus, density = 0.1, color = 0.5, master = 0.08|
		var amt  = In.kr(noiseBus, 1).lag(1.5);
		var gate = In.kr(gateBus, 1).lag(4);
		var dens = density.lag(5).linexp(0.001, 1, 0.08, 6) * (0.2 + (amt * 0.6));
		var trig = Dust.ar(dens);
		var env  = Decay2.ar(trig, 0.002, 0.018 + (color * 0.05));
		var pan  = TRand.ar(-1, 1, trig);
		var cf   = color.linexp(0, 1, 800, 9000);
		var sig  = BPF.ar(PinkNoise.ar, cf, 0.08) * env * master;
		Out.ar(out, Pan2.ar(sig * gate, pan));
	}).add;

	// Spatial send: soft ping-pong delay feeding a long algorithmic reverb,
	// wetness set by macro space + individual event depth sends (see
	// ~trigClick). This is the "ambient" half of the drone/ambient
	// aesthetic — a real diffuse tail rather than just a delay repeat —
	// and the audio analogue of dimensional depth (layer 4).
	SynthDef(\pdjDelayFX, { |inBus, out = 0, spaceBus, mix = 0.25|
		var wet   = In.kr(spaceBus, 1).lag(3);
		var sig   = In.ar(inBus, 2);
		var early = DelayC.ar(sig, 0.12, [0.071, 0.103]);
		var tail = FreeVerb2.ar(
			early[0] + (sig[1] * 0.18),
			early[1] + (sig[0] * 0.18),
			wet.linlin(0, 1, 0.34, 0.72),
			wet.linlin(0, 1, 0.72, 0.96),
			0.62
		);
		tail = LPF.ar(HPF.ar(tail, 45), 6200);
		Out.ar(out, tail * wet * mix);
	}).add;

	// Meta-datamatics: reads the master mix's own amplitude and writes it
	// to a control bus so the system can feed its own output back into its
	// generative parameters (see ~selfFeedbackRoutine below).
	SynthDef(\pdjSelfAnalysis, { |inBus, out|
		var sig = Mix.ar(In.ar(inBus, 2));
		var amp = Amplitude.ar(sig, 0.01, 0.35);
		Out.kr(out, A2K.kr(amp));
	}).add;

	// Master bus: dry mix + space send, soft clip + limiter
	SynthDef(\pdjMaster, { |inBus, delayBus, out = 0, gain = 0.55|
		var dry = In.ar(inBus, 2);
		var wet = In.ar(delayBus, 2);
		var sig = (dry + wet) * gain;
		sig = (sig * 1.2).tanh * 0.9;
		sig = Limiter.ar(sig, 0.95, 0.01);
		Out.ar(out, sig);
	}).add;

	s.sync;

	// --------------------------------------------------------------------------
	// AUDIO GRAPH
	// --------------------------------------------------------------------------
	~fxBus    = Bus.audio(s, 2);
	~delayBus = Bus.audio(s, 2);
	~master = Synth(\pdjMaster, [\inBus, ~fxBus, \delayBus, ~delayBus, \gain, ~masterGain]);
	~channelBases = [31.7, 33.1, 35.9, 38.3];

	~toneBanks = Array.fill(~numChannels, { |i|
		Synth(~modeSynthNames[0], [
			\out, ~fxBus,
			\ampBus, ~ch[i].ampBus,
			\panBus, ~ch[i].panBus,
			\tensionBus, ~ch[i].tensionBus,
			\spectralBus, ~gSpectralBus,
			\sceneAmp, ~ch[i].scene.drone,
			\color, ~ch[i].scene.color,
			\variant, i % 4,
			\evolution, ~ch[i].videoPos,
			\master, 0.24
		], addAction: \addToHead);
	});

	~spawnModeVoice = { |ch, mode|
		var old = ~toneBanks[ch];
		var profile = ~ch[ch].scene;
		var variant = ((~ch[ch].videoRevision.max(0) + ~ch[ch].modeRevision + ch) % 4);
		if(old.notNil, { old.set(\lifeGate, 0) });
		~toneBanks[ch] = Synth(~modeSynthNames.clipAt(mode.clip(0, 13)), [
			\out, ~fxBus,
			\ampBus, ~ch[ch].ampBus,
			\panBus, ~ch[ch].panBus,
			\tensionBus, ~ch[ch].tensionBus,
			\spectralBus, ~gSpectralBus,
			\sceneAmp, profile.drone,
			\color, profile.color,
			\variant, variant,
			\evolution, ~ch[ch].videoPos,
			\lifeGate, 1,
			\master, 0.24
		], addAction: \addToHead);
	};

	~pulseEngines = Array.fill(~numChannels, { |i|
		Synth(\pdjPulseEngine, [
			\out, ~fxBus,
			\rateBus, ~ch[i].rateBus,
			\ampBus, ~ch[i].ampBus,
			\panBus, ~ch[i].panBus,
			\gateBus, ~gGate2Bus,
			\crushBus, ~gCrushBus,
			\tensionBus, ~ch[i].tensionBus,
			\sceneAmp, ~ch[i].scene.pulse,
			\color, ~ch[i].scene.color,
			\variant, i % 4,
			\master, 0.025
		], addAction: \addToHead);
	});

	~spectralAnchor = Synth(\pdjSpectralAnchor, [\out, ~gSpectralBus, \target, 420], addAction: \addToHead);

	~noiseBeds = Array.fill(~numChannels, { |i|
		Synth(\pdjNoiseBed, [
			\out, ~fxBus,
			\noiseBus, ~ch[i].noiseBus,
			\panBus, ~ch[i].panBus,
			\gateBus, ~gGate3Bus,
			\crushBus, ~gCrushBus,
			\spectralBus, ~gSpectralBus,
			\motionBus, ~gMotionBus,
			\tensionBus, ~ch[i].tensionBus,
			\sceneAmp, ~ch[i].scene.air,
			\color, ~ch[i].scene.color,
			\voice, ~ch[i].scene.voice,
			\variant, i % 4,
			\master, 0.06
		], addAction: \addToHead);
	});

	~subs = Array.fill(~numChannels, { |i|
		Synth(\pdjSub, [
			\out, ~fxBus,
			\amp, 0,
			\freq, 40 + (i * 7),
			\pan, i.linlin(0, 3, -0.6, 0.6)
		], addAction: \addToHead);
	});

	~wash = Synth(\pdjWash, [
		\out, ~fxBus, \noiseBus, ~gNoiseBus, \gateBus, ~gGate3Bus, \crushBus, ~gCrushBus,
		\spectralBus, ~gSpectralBus, \motionBus, ~gMotionBus, \crowdBus, ~gCrowdBus,
		\tensionBus, ~gTensionBus, \master, 0.075
	], addAction: \addToHead);

	~crackle = Synth(\pdjCrackle, [
		\out, ~fxBus, \noiseBus, ~gNoiseBus, \gateBus, ~gGate3Bus,
		\density, 0.08, \color, 0.5, \master, 0.018
	], addAction: \addToHead);

	~delayFX = Synth(\pdjDelayFX, [
		\inBus, ~fxBus, \out, ~delayBus, \spaceBus, ~gSpaceBus, \mix, 0.38
	], addAction: \addToHead);

	~selfAnalysis = Synth(\pdjSelfAnalysis, [
		\inBus, ~fxBus, \out, ~selfBus
	], addAction: \addToHead);

	// Compact /blob/{slot}/state packets are reduced to stable per-channel
	// geometry. This avoids composing from contour-finder slot 0, whose identity
	// can change between frames.
	~deriveBlobState = { |st|
		var active = st.blobData.select({ |blob| blob.active == 1 });
		if(active.notEmpty, {
			var xs = active.collect(_.x);
			var ys = active.collect(_.y);
			st[\centroidX] = (xs.sum / active.size).clip(0, 1);
			st[\centroidY] = (ys.sum / active.size).clip(0, 1);
			st[\blobSpread] = (
				((xs.maxItem - xs.minItem).squared + (ys.maxItem - ys.minItem).squared).sqrt
			).clip(0, 1);
			st[\blobSpeed] = (
				active.collect({ |blob| ((blob.vx.squared) + (blob.vy.squared)).sqrt }).sum / active.size
			).clip(0, 0.2);
			st[\areaMass] = (active.collect(_.area).sum / 129600).clip(0, 1);
		}, {
			st[\centroidX] = 0.5;
			st[\centroidY] = 0.5;
			st[\blobSpread] = 0;
			st[\blobSpeed] = 0;
			st[\areaMass] = 0;
		});
	};

	// --------------------------------------------------------------------------
	// DATA CONDUCTOR — frame-rate telemetry is integrated into slow musical
	// control. No autonomous random walk and no output-feedback modulation:
	// every large change now has a visible cause.
	// --------------------------------------------------------------------------
	~dataConductor = Routine({
		1.wait; // allow mapping functions below to finish installing
		~ch.do({ |st| ~deriveBlobState.(st) });
		loop {
			var energy = (~ch.collect({ |st| (st.energy * 6).clip(0, 1) }).sum / ~numChannels);
			var flow = (~ch.collect({ |st| (st.flowMag / 4).clip(0, 1) }).sum / ~numChannels);
			var crowd = (~ch.collect(_.crowd).sum / ~numChannels).clip(0, 1);
			var body = (~ch.collect(_.leg).sum / ~numChannels).clip(0, 1);
			var motion = ((energy * 0.76) + (flow * 0.24)).clip(0, 1);
			var air = (~ch.collect({ |st| st.scene.air }).sum / ~numChannels);
			var space = (~ch.collect({ |st| st.scene.space }).sum / ~numChannels);
			var crush = (~ch.collect({ |st| st.scene.crush }).sum / ~numChannels);
			var color = (~ch.collect({ |st| st.scene.color }).sum / ~numChannels);
			var sceneTension = (~ch.collect({ |st| st.scene.tension }).sum / ~numChannels);
			var complexity = (~ch.collect({ |st| (st.contour / 7000).clip(0, 1) }).sum / ~numChannels);
			var population = (~ch.collect({ |st| (st.blobs / 8).clip(0, 1) }).sum / ~numChannels);
			var geometry = (~ch.collect({ |st| st[\blobSpread] }).sum / ~numChannels);
			var trackedSpeed = (~ch.collect({ |st| (st[\blobSpeed] * 10).clip(0, 1) }).sum / ~numChannels);
			var directorSpeed = (~ch.collect(_.speedMultiplier).sum / ~numChannels).clip(0, 4);
			var fastBias = ((directorSpeed - 1) / 2.5).clip(0, 1);
			var slowBias = ((1 - directorSpeed) / 0.7).clip(0, 1);
			var clear = (~ch.collect(_.clearAlpha).sum / ~numChannels).clip(0, 1);
			var avgEvolution = (~ch.collect(_.videoPos).sum / ~numChannels).clip(0, 1);
			var clipArc = (~ch.collect({ |st| (st.videoPos.clip(0, 1) * pi).sin }).sum / ~numChannels);
			var tension = (
				sceneTension + (energy * 0.28) + (flow * 0.16) + (crowd * 0.16)
				+ (complexity * 0.10) + (population * 0.08) + (body * 0.04)
				+ (geometry * 0.10) + (trackedSpeed * 0.12) + (fastBias * 0.22)
				+ (slowBias * 0.06) + (clear * 0.24) + (clipArc * 0.05)
			).clip(0, 1);
			var avgY = (~ch.collect({ |st| st[\centroidY] }).sum / ~numChannels).clip(0, 1);
			var spectralTarget = avgY.linexp(0, 1, 1900, 180)
				* color.linexp(0, 1, 0.72, 1.8) * tension.linexp(0, 1, 0.9, 1.35);

			~macro.noise = (
				air * (0.38 + (motion * 0.62)) * (0.55 + (avgEvolution * 0.70))
			).clip(0.01, 0.62);
			~macro.space = space.clip(0.45, 1);
			~macro.density = ((motion * color * 0.72) + (tension * 0.16)).clip(0, 0.7);
			~crushBase = crush;

			~gMotionBus.set(motion);
			~gCrowdBus.set(crowd);
			~gTensionBus.set(tension);
			~gNoiseBus.set(~macro.noise);
			~gDensityBus.set(~macro.density);
			~gSpaceBus.set(~macro.space);
			~gCrushBus.set(crush);
			~spectralAnchor.set(\target, spectralTarget.clip(90, 4200));
			// Body elongation is a quiet secondary texture cue: standing,
			// vertically extended figures reveal slightly more microsound.
			~crackle.set(\density, ((~macro.density * 0.78) + (body * 0.16)).clip(0, 0.7), \color, color);

			~numChannels.do { |ch| ~applyContinuous.(ch) };
			0.25.wait;
		}
	}).play;

	// --------------------------------------------------------------------------
	// DIMENSION GATES — how many layers are simultaneously active. This is
	// the audio analogue of the 2D -> 3D -> 4D visual expansion described
	// in the analysis (section 9): flat/single layer -> added layers ->
	// full simultaneity.
	// --------------------------------------------------------------------------
	~setDimension = { |n|
		~form.dimension = n;
		~gGate1Bus.set(if(n >= 1, 1, 0));
		~gGate2Bus.set(if(n >= 2, 1, 0));
		~gGate3Bus.set(if(n >= 3, 1, 0));
	};

	// --------------------------------------------------------------------------
	// FORM STATE-MACHINE — module-based montage (sections 8, 17). Each
	// module sets its pulse-rate register, dimension, sync regime, and
	// macro/crush baseline, then holds for tens of seconds before an
	// abrupt cut to the next module.
	// --------------------------------------------------------------------------
	~applyModule = { |name|
		~form.name = name;
		case
		{ name == \calibration } {
			// Sparse signals establish the vocabulary: fixed tones only,
			// slow individual clicks if anything leaks through. The most
			// elegant, driest-noise module — mostly drone + reverb tail.
			~form.rateLo = 0.3; ~form.rateHi = 3; ~form.lockToOSC = true;
			~macro.noise = 0.06; ~macro.space = 0.55; ~macro.density = 0.08; ~crushBase = 0.0;
			~setDimension.(1);
		}
		{ name == \codification } {
			// Grids and pulses imply a readable system — grid is locked
			// tightly to real OSC events (micro-synchrony). Still an
			// ambient/drone-forward module, just with the pulse layer
			// added on top of the tone bank.
			~form.rateLo = 1; ~form.rateHi = 14; ~form.lockToOSC = true;
			~macro.noise = 0.13; ~macro.space = 0.58; ~macro.density = 0.20; ~crushBase = 0.05;
			~setDimension.(2);
		}
		{ name == \accumulation } {
			// Layers and density increase; clock runs independently now —
			// deliberate contrapuntal separation from the visual events.
			~form.rateLo = 3; ~form.rateHi = 500; ~form.lockToOSC = false;
			~macro.noise = 0.35; ~macro.space = 0.60; ~macro.density = 0.45; ~crushBase = 0.20;
			~setDimension.(3);
		}
		{ name == \dimensionalExpansion } {
			// Flat patterns become spatial fields: dimension ramps 1->4
			// progressively across the module's own duration.
			~form.rateLo = 3; ~form.rateHi = 4000; ~form.lockToOSC = false;
			~macro.noise = 0.45; ~macro.space = 0.75; ~macro.density = 0.55; ~crushBase = 0.30;
			~setDimension.(1);
			Routine({
				[2, 3, 4].do { |n| rrand(4.0, 7.0).wait; ~setDimension.(n) };
			}).play;
		}
		{ name == \saturation } {
			// Information exceeds perceptual capacity: full register,
			// full dimension, heavy quantization.
			~form.rateLo = 80; ~form.rateHi = 15000; ~form.lockToOSC = false;
			~macro.noise = 0.70; ~macro.space = 0.90; ~macro.density = 0.85; ~crushBase = 0.50;
			~setDimension.(4);
		}
		{ name == \rupture } {
			// A hard cut: everything silenced instantly, then one sharp
			// broadband burst — matching "load, replace, switch, interrupt."
			~form.lockToOSC = true;
			~macro.noise = 0.02; ~macro.space = 0.30; ~macro.density = 0.05; ~crushBase = 1.0;
			~setDimension.(0);
			~gCrushBus.set(1);
			Routine({
				rrand(1.2, 2.2).wait;
				~trigGlitch.(~numChannels.rand, 1.0, true);
			}).play;
		}
		{ name == \recursion } {
			// Earlier (Calibration/Codification) material returns, but
			// routed through quantization — deconstructed, not repeated
			// (meta-datamatics, section 13) — with generous space so it
			// reads as a distant, blurred memory rather than a harsh redo.
			~form.rateLo = 0.3; ~form.rateHi = 10; ~form.lockToOSC = true;
			~macro.noise = 0.18; ~macro.space = 0.68; ~macro.density = 0.16; ~crushBase = 0.45;
			~setDimension.(2);
		};

		~gNoiseBus.set(~macro.noise);
		~gDensityBus.set(~macro.density);
		~gSpaceBus.set(~macro.space);

		("PDJ module -> " ++ name ++ "   dim=" ++ ~form.dimension ++ "   lockToOSC=" ++ ~form.lockToOSC).postln;
	};

	~formOrder = [\calibration, \codification, \accumulation, \dimensionalExpansion, \saturation, \rupture, \recursion];
	~formDurs  = (
		calibration: 18, codification: 22, accumulation: 26,
		dimensionalExpansion: 30, saturation: 20, rupture: 8, recursion: 24
	);

	// Kept as a manual research vocabulary through ~goto, but never scheduled:
	// an autonomous form would drift out of phase with the four video scores.
	~formRoutine = nil;

	// --------------------------------------------------------------------------
	// MAPPING HELPERS
	// --------------------------------------------------------------------------
	// Velocity -> pan destination. A blob moving right/down during a
	// trigger makes the sound sweep in that direction across the field;
	// this is the direct "space follows visual detection" link.
	~panSweep = { |ch|
		var st, centerX, start, end;
		st = ~ch[ch];
		~deriveBlobState.(st);
		centerX = st[\centroidX];
		if(centerX.isNil, { centerX = 0.5 });
		start = centerX.linlin(0, 1, -1, 1) * ~macro.space.linlin(0, 1, 0.35, 1);
		end   = (start + (st.blob0Vx * 3.0)).clip(-1, 1);
		[start, end];
	};

	~isDataMode = { |m| [3, 5, 6, 9].includes(m) };

	~applyVisualMode = { |ch, mode|
		var profile = ~modeProfiles.clipAt(mode.clip(0, 13));
		~ch[ch].scene = profile;
		~spawnModeVoice.(ch, mode);
		~pulseEngines[ch].set(
			\sceneAmp, profile.pulse,
			\color, profile.color
		);
		~noiseBeds[ch].set(
			\sceneAmp, profile.air,
			\color, profile.color,
			\voice, profile.voice
		);
		("PDJ visual scene ch" ++ ch ++ " -> " ++ profile.name).postln;
	};

	~applyContinuous = { |ch|
		var st, energy, flow, amp, pan, rate, noise, spaceW, activity, tension;
		var slowBias, fastBias, clear, clipArc, voiceTarget, dynamicColor, variant;
		var droneAmp, airAmp, pulseAmp, positionX;
		var evolution, stage, formRise, formFall, microWindow;
		st     = ~ch[ch];
		~deriveBlobState.(st);
		energy = (st.energy * 6).clip(0, 1);
		flow   = (st.flowMag / 4).clip(0, 1);
		spaceW = ~macro.space.linlin(0, 1, 0.35, 1); // narrow<->full stereo width
		slowBias = ((1 - st.speedMultiplier) / 0.7).clip(0, 1);
		fastBias = ((st.speedMultiplier - 1) / 2.5).clip(0, 1);
		clear = st.clearAlpha.clip(0, 1);
		evolution = st.videoPos.clip(0, 1);
		clipArc = (evolution * pi).sin;
		stage = (evolution * 4).floor.clip(0, 3).asInteger;
		if(stage != st.evolutionStage, {
			st.evolutionStage = stage;
			("PDJ evolution ch" ++ ch ++ " -> stage " ++ stage ++ "  pos=" ++ evolution.round(0.01)).postln;
		});
		formRise = evolution.linlin(0, 0.30, 0.58, 1).clip(0.58, 1);
		formFall = evolution.linlin(0.78, 1, 1, 0.62).clip(0.62, 1);
		microWindow = evolution.linlin(0.35, 0.62, 0, 1).clip(0, 1)
			* evolution.linlin(0.82, 1, 1, 0).clip(0, 1);

		// Motion changes pressure inside a persistent scene; it never switches
		// the ambient foundation off and on at camera frame rate.
		activity = (
			(energy * 0.64) + (flow * 0.16) + ((st[\blobSpeed] * 10).clip(0, 1) * 0.20)
		).clip(0, 1);
		amp = activity.squared;
		tension = (
			st.scene.tension + (energy * 0.26) + (flow * 0.12)
			+ (st.crowd * 0.14) + ((st.contour / 7000).clip(0, 1) * 0.09)
			+ ((st.blobs / 8).clip(0, 1) * 0.07) + (st.leg * 0.04)
			+ (st[\blobSpread] * 0.10) + ((st[\blobSpeed] * 10).clip(0, 1) * 0.12)
			+ (fastBias * 0.22) + (slowBias * 0.06) + (clear * 0.24) + (clipArc * 0.05)
		).clip(0, 1);

		// Position remains primary; optical-flow direction adds only a small
		// directional bias, making the field follow movement without jumping.
		positionX = if(st.blobs > 0, {
			st[\centroidX]
		}, {
			ch.linlin(0, ~numChannels - 1, 0.12, 0.88)
		});
		pan = (
			positionX.linlin(0, 1, -0.85, 0.85)
			+ ((st.flowAng / pi).clip(-1, 1) * flow * 0.18)
		).clip(-1, 1) * spaceW;

		// Pulse remains microscopic: data modes reveal it, cinematic modes
		// suppress it through scene.pulse. Blob count changes subdivision,
		// while movement controls speed.
		rate = (
			0.3 + (activity * 7) + (st.blobs.clip(0, 8) * 0.28)
		).clip(0.3, 12) * st.speedMultiplier.clip(0.35, 2.2);

		noise = (
			(activity * 0.42) + ((st.contour / 6000).clip(0, 0.22))
			+ (st.crowd * 0.16) + (st[\blobSpread] * 0.14) + (st[\areaMass] * 0.10)
		).clip(0, 1);

		// Director service creates a clear voice transformation: slow motion
		// pulls every scene toward temporal haze; fast motion toward data/glass.
		voiceTarget = st.scene.voice;
		voiceTarget = (voiceTarget * (1 - (slowBias * 0.78))) + (3 * slowBias * 0.78);
		voiceTarget = (voiceTarget * (1 - (fastBias * 0.72))) + (2 * fastBias * 0.72);
		dynamicColor = (
			st.scene.color + (fastBias * 0.18) - (slowBias * 0.08) + (clipArc * 0.07)
		).clip(0.02, 1);
		variant = ((st.videoRevision.max(0) + st.modeRevision + ch) % 4);
		droneAmp = st.scene.drone * formRise * formFall
			* (1 - (clear * 0.86)) * (1 + (slowBias * 0.08));
		airAmp = st.scene.air * (0.45 + (evolution * 0.85))
			* (1 - (clear * 0.48)) * (1 + (fastBias * 0.42) + (tension * 0.12));
		pulseAmp = st.scene.pulse * microWindow * 0.28
			* (1 - clear) * (0.72 + (fastBias * 0.75) - (slowBias * 0.48));

		st.ampBus.set(amp);
		st.panBus.set(pan);
		st.rateBus.set(rate);
		st.noiseBus.set(noise);
		st.tensionBus.set(tension);
		~toneBanks[ch].set(
			\sceneAmp, droneAmp.clip(0, 1), \color, dynamicColor,
			\variant, variant, \evolution, evolution
		);
		~pulseEngines[ch].set(
			\sceneAmp, pulseAmp.clip(0, 0.55), \color, dynamicColor, \variant, variant
		);
		~noiseBeds[ch].set(
			\sceneAmp, airAmp.clip(0, 0.65), \color, dynamicColor,
			\voice, voiceTarget.clip(0, 3), \variant, variant
		);
		~subs[ch].set(
			\amp, ((0.025 + (st.crowd * 0.24) + (st[\areaMass] * 0.08))
				* st.scene.drone * (1 - (clear * 0.92))).clip(0, 0.32)
		);
	};

	~trigImpact = { |ch|
		var st = ~ch[ch];
		var centerX;
		~deriveBlobState.(st);
		centerX = st[\centroidX];
		if(centerX.isNil, { centerX = 0.5 });
		Synth(\pdjImpact, [
			\out, ~fxBus,
			\amp, 0.20 + (st.energy.clip(0, 1) * 0.20),
			\pan, centerX.linlin(0, 1, -0.7, 0.7),
			\depth, st.crowd
		]);
	};

	~trigTrace = { |ch|
		var st = ~ch[ch];
		var start = st.ballX.linlin(0, 1, -0.9, 0.9);
		Synth(\pdjTrace, [
			\out, ~delayBus,
			\amp, 0.12 + (st.flowMag.clip(0, 8) * 0.008),
			\panStart, start,
			\panEnd, (start + (st.blob0Vx * 4)).clip(-1, 1),
			\height, 1 - st.ballY
		]);
	};

	~trigVideoCut = { |ch|
		var st = ~ch[ch];
		var pan = ch.linlin(0, ~numChannels - 1, -0.75, 0.75);
		Synth(\pdjTrace, [
			\out, ~delayBus,
			\amp, 0.075,
			\panStart, pan,
			\panEnd, (pan * -0.35),
			\height, ((st.videoRevision + ch) % 4).linlin(0, 3, 0.2, 0.8)
		]);
	};

	~trigKick = { |ch, weight = 0.5|
		var now = Main.elapsedTime;
		var st = ~ch[ch];
		if((now - st.lastKick) > 0.45, {
			st.lastKick = now;
			Synth(\pdjKick, [
				\out, ~fxBus,
				\amp, 0.14 + (weight * 0.10),
				\pan, ch.linlin(0, ~numChannels - 1, -0.55, 0.55),
				\weight, weight.clip(0, 1)
			]);
		});
	};

	~trigClick = { |ch, kind = \collision|
		var sweep = ~panSweep.(ch);
		var st    = ~ch[ch];
		var freq  = if(kind == \ball, {
			5000 + (st.ballY * 5000)
		}, {
			7000 + (ch * 900) + (st.energy * 3000)
		});
		Synth(\pdjClick, [
			\out, ~fxBus,
			\freq, freq,
			\amp, if(kind == \ball, 0.28, 0.36),
			\panStart, sweep[0],
			\panEnd, sweep[1],
			\decay, if(kind == \ball, 0.022, 0.015)
		]);
		// Depth send: events further back (low ballY) throw more into
		// the space/delay bus, near events stay drier.
		if(kind == \ball, {
			Synth(\pdjClick, [
				\out, ~delayBus,
				\freq, freq * 1.5,
				\amp, (1 - st.ballY).clip(0, 1) * 0.22 * ~macro.space,
				\panStart, sweep[0], \panEnd, sweep[1],
				\decay, 0.05
			]);
		});
	};

	~trigGlitch = { |ch, dens = 0.5, force = false|
		var now = Main.elapsedTime;
		// Throttle: oF sends ~30 fps — keep glitches sparse
		if(force or: { (now - ~ch[ch].lastGlitch) > 0.12 }, {
			var sweep = ~panSweep.(ch);
			~ch[ch].lastGlitch = now;
			Synth(\pdjGlitch, [
				\out, ~fxBus,
				\amp, (0.055 + (dens * 0.07)) * (0.55 + (~macro.density * 0.45)),
				\panStart, sweep[0], \panEnd, sweep[1],
				\rate, 20 + (dens * 80),
				\dur, 0.03 + (dens * 0.08)
			]);
		});
	};

	// --------------------------------------------------------------------------
	// BINARY GRID SEQUENCER — literal data-as-rhythm. Once per step, a real
	// detected value (summed blob count across channels) is read as an
	// 8-bit binary number; each bit gates a click on that grid position.
	// The audible rhythm IS the binary representation of a real value.
	//
	// Contrapuntal sync alternation: when ~form.lockToOSC is true, this
	// firing is triggered directly from collision/ball OSC handlers
	// (micro-synchrony). When false, ~gridClock below fires it on its own
	// fixed subdivision, deliberately independent of visual events.
	// --------------------------------------------------------------------------
	~fireGridStep = {
		var count = (~ch.collect(_.blobs).sum).clip(0, 255).asInteger;
		var bits  = ~binaryBits.(count, 8);
		var dataChannels = (0 .. (~numChannels - 1)).select({ |ch| ~isDataMode.(~ch[ch].mode) });
		~gridStep = (~gridStep + 1) % 8;
		if((bits[~gridStep] == 1) and: { dataChannels.notEmpty }, {
			var ch = dataChannels.wrapAt(~gridStep);
			~trigGridHit.(ch, ~gridStep);
		});
	};

	~trigGridHit = { |ch, stepIdx|
		var sweep = ~panSweep.(ch);
		var freq  = 1800 + (stepIdx * 900); // pitch reflects bit position — a literal read-out
		Synth(\pdjClick, [
			\out, ~delayBus,
			\freq, freq,
			\amp, 0.022 + (~macro.density * 0.018),
			\panStart, sweep[0], \panEnd, sweep[1],
			\decay, 0.028
		]);
	};

	~gridClock = Routine({
		loop {
			if(~ch.any({ |st| ~isDataMode.(st.mode) }), { ~fireGridStep.value });
			0.68.wait;
		}
	}).play;

	// --------------------------------------------------------------------------
	// OSC RECEIVE — paths match OSCSender.cpp
	// --------------------------------------------------------------------------
	thisProcess.openUDPPort(~oscPort);

	~numChannels.do { |ch|

		OSCdef(("pdj_frame_" ++ ch).asSymbol, { |msg|
			var next = msg[1].asInteger;
			var prev = ~ch[ch].frame;
			if((prev > 0) and: { next > (prev + 1) }, {
				~ch[ch].dropped = ~ch[ch].dropped + (next - prev - 1);
			});
			~ch[ch].frame = next;
			~ch[ch].lastSeen = Main.elapsedTime;
			~ch[ch].online = true;
		}, ("/pdj/channel/" ++ ch ++ "/state/frame").asSymbol);

		OSCdef(("pdj_sourceTime_" ++ ch).asSymbol, { |msg|
			~ch[ch].sourceTime = msg[1].asFloat;
		}, ("/pdj/channel/" ++ ch ++ "/state/time").asSymbol);

		OSCdef(("pdj_energy_" ++ ch).asSymbol, { |msg|
			~ch[ch].energy = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/motion/energy").asSymbol);

		OSCdef(("pdj_flowMag_" ++ ch).asSymbol, { |msg|
			~ch[ch].flowMag = msg[1].asFloat.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/flow/magnitude").asSymbol);

		OSCdef(("pdj_flowAng_" ++ ch).asSymbol, { |msg|
			~ch[ch].flowAng = msg[1].asFloat;
		}, ("/pdj/channel/" ++ ch ++ "/flow/angle").asSymbol);

		OSCdef(("pdj_blobs_" ++ ch).asSymbol, { |msg|
			var n = msg[1].asInteger;
			~ch[ch].blobs = n;
		}, ("/pdj/channel/" ++ ch ++ "/blobs/count").asSymbol);

		OSCdef(("pdj_contour_" ++ ch).asSymbol, { |msg|
			~ch[ch].contour = msg[1].asFloat.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/contour/length").asSymbol);

		8.do { |slot|
			var defName = ("pdj_blobState_" ++ ch ++ "_" ++ slot).asSymbol;
			var address = ("/pdj/channel/" ++ ch ++ "/blob/" ++ slot ++ "/state").asSymbol;
			OSCdef(defName, { |msg|
				var blob = ~ch[ch].blobData[slot];
				blob.active = msg[1].asInteger;
				blob.x = msg[2].asFloat;
				blob.y = msg[3].asFloat;
				blob.vx = msg[4].asFloat;
				blob.vy = msg[5].asFloat;
				blob.area = msg[6].asFloat;
				blob.bbW = msg[7].asFloat;
				blob.bbH = msg[8].asFloat;
				if(slot == 0, {
					~ch[ch].blob0X = blob.x.clip(0, 1);
					~ch[ch].blob0Y = blob.y.clip(0, 1);
					~ch[ch].blob0Vx = blob.vx.clip(-1, 1);
					~ch[ch].blob0Vy = blob.vy.clip(-1, 1);
				});
			}, address);
		};

		OSCdef(("pdj_crowd_" ++ ch).asSymbol, { |msg|
			~ch[ch].crowd = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/event/crowd").asSymbol);

		OSCdef(("pdj_leg_" ++ ch).asSymbol, { |msg|
			~ch[ch].leg = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/event/leg_distance").asSymbol);

		OSCdef(("pdj_mode_" ++ ch).asSymbol, { |msg|
			var m = msg[1].asInteger;
			var prev = ~ch[ch].mode;
			if(~auditioning.not, {
				~ch[ch].mode = m;
				if(m != prev, {
					~applyVisualMode.(ch, m);
					if(m == 13, { ~trigGlitch.(ch, 0.20, true) });
				});
			});
		}, ("/pdj/channel/" ++ ch ++ "/score/mode").asSymbol);

		OSCdef(("pdj_modeRevision_" ++ ch).asSymbol, { |msg|
			~ch[ch].modeRevision = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/score/revision").asSymbol);

		OSCdef(("pdj_videoName_" ++ ch).asSymbol, { |msg|
			~ch[ch].videoName = msg[1].asString;
		}, ("/pdj/channel/" ++ ch ++ "/video/name").asSymbol);

		OSCdef(("pdj_videoPos_" ++ ch).asSymbol, { |msg|
			~ch[ch].videoPos = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/video/position").asSymbol);

		OSCdef(("pdj_videoDur_" ++ ch).asSymbol, { |msg|
			~ch[ch].videoDur = msg[1].asFloat.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/video/duration").asSymbol);

		OSCdef(("pdj_videoRevision_" ++ ch).asSymbol, { |msg|
			var revision = msg[1].asInteger;
			var previous = ~ch[ch].videoRevision;
			var variant;
			~ch[ch].videoRevision = revision;
			~ch[ch].evolutionStage = -1;
			if((previous >= 0) and: { revision != previous }, {
				variant = ((revision + ~ch[ch].modeRevision + ch) % 4);
				~toneBanks[ch].set(\variant, variant);
				~pulseEngines[ch].set(\variant, variant);
				~noiseBeds[ch].set(\variant, variant);
				~trigVideoCut.(ch);
				~trigKick.(ch, 0.62);
				("PDJ video cut ch" ++ ch ++ " -> " ++ ~ch[ch].videoName).postln;
			});
		}, ("/pdj/channel/" ++ ch ++ "/video/revision").asSymbol);

		OSCdef(("pdj_temporal_" ++ ch).asSymbol, { |msg|
			~ch[ch].temporalPhase = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/director/temporal").asSymbol);

		OSCdef(("pdj_speed_" ++ ch).asSymbol, { |msg|
			~ch[ch].speedMultiplier = msg[1].asFloat.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/director/speed").asSymbol);

		OSCdef(("pdj_clear_" ++ ch).asSymbol, { |msg|
			var phase = msg[1].asInteger;
			var previous = ~ch[ch].clearPhase;
			~ch[ch].prevClearPhase = previous;
			~ch[ch].clearPhase = phase;
			if((ch == 0) && (previous == 0) && (phase != 0), {
				Synth(\pdjClearSweep, [\out, ~fxBus, \amp, 0.085, \dur, 1.8]);
				~trigKick.(0, 0.82);
			});
		}, ("/pdj/channel/" ++ ch ++ "/director/clear").asSymbol);

		OSCdef(("pdj_clearAlpha_" ++ ch).asSymbol, { |msg|
			~ch[ch].clearAlpha = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/director/clear_alpha").asSymbol);

		OSCdef(("pdj_coll_" ++ ch).asSymbol, { |msg|
			var v = msg[1].asInteger;
			var now = Main.elapsedTime;
			if((v == 1) && (~ch[ch].prevColl == 0) && ((now - ~ch[ch].lastImpact) > 0.35), {
				~ch[ch].lastImpact = now;
				~trigImpact.(ch);
			});
			~ch[ch].prevColl = v;
		}, ("/pdj/channel/" ++ ch ++ "/event/collision").asSymbol);

		OSCdef(("pdj_ball_" ++ ch).asSymbol, { |msg|
			var v = msg[1].asInteger;
			var now = Main.elapsedTime;
			if((v == 1) && (~ch[ch].prevBall == 0) && ((now - ~ch[ch].lastBall) > 0.25), {
				~ch[ch].lastBall = now;
				// Position follows the event in the same oF OSC bundle.
				SystemClock.sched(0.01, { ~trigTrace.(ch); nil });
			});
			~ch[ch].prevBall = v;
		}, ("/pdj/channel/" ++ ch ++ "/event/ball").asSymbol);

		OSCdef(("pdj_ballX_" ++ ch).asSymbol, { |msg|
			~ch[ch].ballX = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/event/ball/x").asSymbol);

		OSCdef(("pdj_ballY_" ++ ch).asSymbol, { |msg|
			~ch[ch].ballY = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/event/ball/y").asSymbol);
	};

	// A channel is considered offline if no complete snapshot has begun for
	// one second. This makes a broken sender visible instead of silently
	// composing from indefinitely stale CV values.
	~telemetryWatch = Routine({
		loop {
			var now = Main.elapsedTime;
			~ch.do { |st, ch|
				var wasOnline = st.online;
				st.online = (now - st.lastSeen) < 1.0;
				if(wasOnline and: { st.online.not }, {
					("PDJ OSC stale: channel " ++ ch).warn;
				});
			};
			0.5.wait;
		}
	}).play;

	// --------------------------------------------------------------------------
	// LIVE CONTROLS
	// --------------------------------------------------------------------------
	~setMaster  = { |g| ~masterGain = g; ~master.set(\gain, g) };
	~setNoise   = { |v| ~macro.noise   = v.clip(0, 1); ~gNoiseBus.set(v) };
	~setSpace   = { |v| ~macro.space   = v.clip(0, 1); ~gSpaceBus.set(v) };
	~setDensity = { |v| ~macro.density = v.clip(0, 1); ~gDensityBus.set(v) };
	~setCrush   = { |v| ~crushBase     = v.clip(0, 1); ~gCrushBus.set(v) };
	~auditioning = false;
	~setScene = { |mode = 0|
		~numChannels.do { |ch|
			~ch[ch].mode = mode.clip(0, 13);
			~applyVisualMode.(ch, ~ch[ch].mode);
		};
	};
	~auditionModes = {
		~auditionRoutine.tryPerform(\stop);
		~auditioning = true;
		~auditionRoutine = Routine({
			14.do { |mode|
				("PDJ audition mode " ++ mode ++ " -> " ++ ~modeSynthNames[mode]).postln;
				~setScene.(mode);
				5.wait;
			};
			~auditioning = false;
		}).play;
	};

	~shutdown = {
		~numChannels.do { |ch|
			[
				"pdj_frame_", "pdj_sourceTime_", "pdj_energy_", "pdj_flowMag_",
				"pdj_flowAng_", "pdj_blobs_", "pdj_contour_", "pdj_crowd_",
				"pdj_leg_", "pdj_mode_", "pdj_coll_", "pdj_ball_",
				"pdj_ballX_", "pdj_ballY_", "pdj_modeRevision_",
				"pdj_videoName_", "pdj_videoPos_", "pdj_videoDur_",
				"pdj_videoRevision_", "pdj_temporal_", "pdj_speed_",
				"pdj_clear_", "pdj_clearAlpha_"
			].do { |prefix|
				OSCdef((prefix ++ ch).asSymbol).free;
			};
			8.do { |slot|
				OSCdef(("pdj_blobState_" ++ ch ++ "_" ++ slot).asSymbol).free;
			};
		};
		~testOsc.stop;
		~auditionRoutine.tryPerform(\stop);
		~auditioning = false;
		~dataConductor.stop;
		~gridClock.stop;
		~telemetryWatch.stop;
		~toneBanks.do(_.free);
		~pulseEngines.do(_.free);
		~noiseBeds.do(_.free);
		~subs.do(_.free);
		~wash.free;
		~crackle.free;
		~delayFX.free;
		~selfAnalysis.free;
		~spectralAnchor.free;
		~master.free;
		~ch.do { |st|
			st.ampBus.free; st.panBus.free; st.rateBus.free; st.noiseBus.free; st.tensionBus.free;
		};
		~gNoiseBus.free; ~gDensityBus.free; ~gSpaceBus.free; ~gCrushBus.free;
		~gGate1Bus.free; ~gGate2Bus.free; ~gGate3Bus.free; ~selfBus.free; ~gSpectralBus.free;
		~gMotionBus.free; ~gCrowdBus.free; ~gTensionBus.free;
		~fxBus.free; ~delayBus.free;
		"pdj_datamatics shut down.".postln;
	};

	// --------------------------------------------------------------------------
	// OPTIONAL: synthetic OSC feed (no oF required)
	// --------------------------------------------------------------------------
	~testOsc = Routine({
		var net = NetAddr("127.0.0.1", ~oscPort);
		var tick = 0;
		inf.do {
			~numChannels.do { |ch|
				var e = exprand(0.02, 0.7);
				var mode = (((tick / 75).floor) + ch) % 14; // hold each visual scene for five seconds
				var revision = (tick / 75).floor.asInteger;
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/state/frame").asSymbol, tick + 1);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/state/time").asSymbol, Main.elapsedTime);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/motion/energy").asSymbol, e);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/flow/magnitude").asSymbol, e * 3);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/flow/angle").asSymbol, pi.rand2);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/blobs/count").asSymbol, 8.rand);
				net.sendMsg(
					("/pdj/channel/" ++ ch ++ "/blob/0/state").asSymbol,
					1, 1.0.rand, 1.0.rand, 2.0.rand2, 2.0.rand2,
					4000.0.rand, 0.4.rand, 0.4.rand
				);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/crowd").asSymbol, 1.0.rand * 0.5);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/score/mode").asSymbol, mode);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/score/revision").asSymbol, revision);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/name").asSymbol, "synthetic_" ++ revision);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/position").asSymbol, (tick % 75) / 75);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/duration").asSymbol, 5.0);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/revision").asSymbol, revision);
				if(0.08.coin, {
					net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/collision").asSymbol, 1);
				}, {
					net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/collision").asSymbol, 0);
				});
				if(0.05.coin, {
					net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/ball").asSymbol, 1);
					net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/ball/x").asSymbol, 1.0.rand);
					net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/ball/y").asSymbol, 1.0.rand);
				}, {
					net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/ball").asSymbol, 0);
				});
			};
			tick = tick + 1;
			(1/15).wait; // ~15 Hz test stream (oF is ~30)
		};
	});

	"".postln;
	"========================================================".postln;
	"  PDJ Datamatics — listening OSC UDP :" ++ ~oscPort.asString.postln;
	"  ~testOsc.play;      // synthetic feed".postln;
	"  ~setMaster.(0.4);   // master gain".postln;
	"  ~setScene.(12);     // audition visual scene 0..13 (12 = slit-scan)".postln;
	"  ~auditionModes.();  // hear all 14 mode SynthDefs, five seconds each".postln;
	"  ~ch.collect(_.scene); // inspect active video-driven sound scenes".postln;
	"  ~shutdown.();       // teardown".postln;
	"========================================================".postln;
});
)
