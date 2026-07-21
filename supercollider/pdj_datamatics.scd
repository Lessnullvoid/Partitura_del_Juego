/*
================================================================================
  Partitura del Juego — Datamatics Audio Engine (SuperCollider)
  Noise-minimal / glitch / Ryoji Ikeda language

  Receives OSC from openFrameworks (OSCSender) on UDP port 9001:
    /pdj/channel/{0-3}/flow/magnitude|angle
    /pdj/channel/{0-3}/motion/energy
    /pdj/channel/{0-3}/blobs/count
    /pdj/channel/{0-3}/contour/length
    /pdj/channel/{0-3}/blob/{0-7}/x|y|vx|vy|area|bbW|bbH
    /pdj/channel/{0-3}/event/collision|ball|ball/x|ball/y|crowd|leg_distance
    /pdj/channel/{0-3}/score/mode   (0-13)

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

	// Per-channel base frequency (Ikeda-like pure tones, non-overlapping)
	~baseFreqs = [880, 1320, 1760, 2640];

	// Macro state — wanders slowly over minutes so the piece never sits still.
	// Read by synths (via control buses, smooth) and by language-side
	// routines (beat tempo, trigger scaling).
	~macro = (
		noise:   0.25,
		beat:    0.30,
		space:   0.30,
		density: 0.35
	);

	// ScoreMode → texture bias (matches GraphicScore.h enum)
	// 0 BwClean … 13 Flash
	~modeGlitch = [
		0.05, // BwClean
		0.35, // ScanLine
		0.25, // BBoxTracker
		0.55, // BinaryText
		0.40, // Waveform
		0.50, // GridData
		0.70, // Barcode
		0.10, // VideoNormal
		0.30, // VideoSquares
		0.60, // VideoNumbers
		0.45, // VideoLines
		0.55, // ThermalVision
		0.65, // SlitScan
		1.00  // Flash
	];

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
			prevColl:  0,
			prevBall:  0,
			lastGlitch: 0.0,
			ampBus:    Bus.control(s, 1),
			freqBus:   Bus.control(s, 1),
			panBus:    Bus.control(s, 1),
			noiseBus:  Bus.control(s, 1),
			glitchBus: Bus.control(s, 1)
		)
	});

	// Global macro control buses — audio-rate readers .lag() these so
	// every drift the Routine below makes glides in, never jumps.
	~gNoiseBus   = Bus.control(s, 1);
	~gDensityBus = Bus.control(s, 1);
	~gSpaceBus   = Bus.control(s, 1);

	~ch.do { |st, i|
		st.ampBus.set(0);
		st.freqBus.set(~baseFreqs[i]);
		st.panBus.set(i.linlin(0, 3, -0.85, 0.85));
		st.noiseBus.set(0);
		st.glitchBus.set(0);
	};
	~gNoiseBus.set(~macro.noise);
	~gDensityBus.set(~macro.density);
	~gSpaceBus.set(~macro.space);

	// --------------------------------------------------------------------------
	// SYNTHDEFS — clinical, sparse, high-frequency, dry
	// --------------------------------------------------------------------------

	// Sustained pure tone bed (Ikeda sine) — always carries a slow,
	// independent detune drift so it is never perfectly static.
	SynthDef(\pdjTone, { |out = 0, ampBus, freqBus, panBus, master = 0.2|
		var amp   = In.kr(ampBus, 1).lag(0.08);
		var freq  = In.kr(freqBus, 1).lag(0.05).clip(40, 12000);
		var pan   = In.kr(panBus, 1).lag(0.12);
		var drift = LFNoise2.kr(rrand(0.03, 0.09)).range(-0.015, 0.015);
		var sig   = SinOsc.ar(freq * (1 + drift)) * 0.7
			+ SinOsc.ar(freq * 2.001 * (1 + drift)) * 0.08; // faint odd harmonic
		sig = HPF.ar(sig, 80);
		sig = Pan2.ar(sig * amp * master, pan);
		Out.ar(out, sig);
	}).add;

	// Sparse high-click / data impulse (collision / ball).
	// panEnd != panStart -> the click sweeps across the stereo field,
	// direction/speed set from the detected blob's velocity.
	SynthDef(\pdjClick, { |out = 0, freq = 8000, amp = 0.4, panStart = 0, panEnd = 0, decay = 0.012|
		var env = EnvGen.ar(Env.perc(0.0002, decay, 1, -8), doneAction: 2);
		var pan = Line.kr(panStart, panEnd, decay * 4).clip(-1, 1);
		var sig = Impulse.ar(0) * env;
		sig = Ringz.ar(sig, freq, decay * 0.6) * 0.35
			+ (WhiteNoise.ar * env * 0.15);
		sig = HPF.ar(sig, 2000);
		Out.ar(out, Pan2.ar(sig * amp, pan));
	}).add;

	// Short digital glitch burst (barcode / binary / flash language)
	SynthDef(\pdjGlitch, { |out = 0, amp = 0.25, panStart = 0, panEnd = 0, rate = 40, dur = 0.06|
		var env = EnvGen.ar(Env.linen(0.001, dur, 0.01), doneAction: 2);
		var pan = Line.kr(panStart, panEnd, dur).clip(-1, 1);
		var clk = Impulse.ar(rate);
		var sig = Latch.ar(WhiteNoise.ar, clk);
		sig = RHPF.ar(sig, TExpRand.ar(2000, 12000, clk), 0.3);
		sig = sig * env * amp;
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Motion-driven noise grain (scanline / contour hiss) —
	// scaled additionally by the global macro noise level.
	SynthDef(\pdjNoiseBed, { |out = 0, noiseBus, panBus, master = 0.12|
		var amp = In.kr(noiseBus, 1).lag(0.15);
		var pan = In.kr(panBus, 1).lag(0.2);
		var sig = PinkNoise.ar * 0.55 + (WhiteNoise.ar * 0.2);
		sig = BPF.ar(sig, 4500 + (amp * 5500), 0.35);
		sig = HPF.ar(sig, 1600);
		Out.ar(out, Pan2.ar(sig * amp * master, pan));
	}).add;

	// Bit-reduction pulse when glitch bus is high (Ikeda data stutter)
	SynthDef(\pdjBitPulse, { |out = 0, glitchBus, panBus, master = 0.18|
		var g    = In.kr(glitchBus, 1).lag(0.05);
		var pan  = In.kr(panBus, 1).lag(0.1);
		var trig = Dust.ar(g * 28);
		var env  = EnvGen.ar(Env.perc(0.0005, 0.02), trig);
		var sig  = Latch.ar(LFNoise0.ar(8000 + (g * 8000)), Impulse.ar(2000 + (g * 6000)));
		sig = HPF.ar(sig * env, 3000) * g * master;
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Crowd density → sub / low sine pressure (minimal, not bass music)
	SynthDef(\pdjSub, { |out = 0, amp = 0, freq = 55, pan = 0|
		var a   = amp.lag(0.4);
		var sig = SinOsc.ar(freq) * a * 0.35;
		sig = LPF.ar(sig, 120);
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Rhythmic pulse / "battement" — tempo and timbre driven from the
	// scheduling routine below (~trigBeat), which reads live motion data.
	// tone=1 -> sine body (organic pulse), tone=0 -> hard data click.
	SynthDef(\pdjBeat, { |out = 0, freq = 90, amp = 0.4, panStart = 0, panEnd = 0, decay = 0.09, tone = 0.5|
		var env   = EnvGen.ar(Env.perc(0.001, decay, 1, -4), doneAction: 2);
		var clickEnv = EnvGen.ar(Env.perc(0.0004, 0.012), doneAction: 0);
		var pan   = Line.kr(panStart, panEnd, decay * 2.5).clip(-1, 1);
		var body  = SinOsc.ar(freq) * env;
		var click = HPF.ar(WhiteNoise.ar, 3500) * clickEnv;
		var sig   = (body * tone) + (click * (1 - tone) * 0.8);
		Out.ar(out, Pan2.ar(sig * amp, pan));
	}).add;

	// Continuously evolving textural wash — the piece's "breathing"
	// noise layer; center frequency and width wander on their own,
	// amplitude tracks the macro noise level.
	SynthDef(\pdjWash, { |out = 0, noiseBus, master = 0.1|
		var amt = In.kr(noiseBus, 1).lag(2);
		var cf  = LFNoise1.kr(0.045).exprange(500, 6500);
		var bw  = LFNoise1.kr(0.03).exprange(0.08, 0.5);
		var pan = LFNoise1.kr(0.07);
		var sig = BPF.ar(WhiteNoise.ar, cf, bw);
		Out.ar(out, Pan2.ar(sig * amt * master, pan));
	}).add;

	// Data crackle / dust — density tracks macro noise, each grain gets
	// a random spatial position (adds a constant sense of "space").
	SynthDef(\pdjCrackle, { |out = 0, noiseBus, master = 0.16|
		var amt  = In.kr(noiseBus, 1).lag(1.5);
		var dens = amt.linexp(0.001, 1, 1.5, 160);
		var trig = Dust.ar(dens);
		var env  = EnvGen.ar(Env.perc(0.0003, 0.01), trig);
		var pan  = TRand.ar(-1, 1, trig);
		var sig  = HPF.ar(WhiteNoise.ar, 3200) * env * master;
		Out.ar(out, Pan2.ar(sig, pan));
	}).add;

	// Spatial send: soft ping-pong delay tapped from the mix, wetness
	// set by macro space + individual event depth sends (see ~trigClick).
	SynthDef(\pdjDelayFX, { |inBus, out = 0, spaceBus, mix = 0.25|
		var wet  = In.kr(spaceBus, 1).lag(3);
		var sig  = In.ar(inBus, 2);
		var dTime = LFNoise1.kr(0.08).exprange(0.12, 0.32);
		var dly  = CombL.ar(sig, 0.5, dTime, 2.4);
		dly = LPF.ar(dly, 4500);
		Out.ar(out, dly * wet * mix);
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

	~tones  = Array.fill(~numChannels, { |i|
		Synth(\pdjTone, [
			\out, ~fxBus,
			\ampBus, ~ch[i].ampBus,
			\freqBus, ~ch[i].freqBus,
			\panBus, ~ch[i].panBus,
			\master, 0.22
		], addAction: \addToHead);
	});

	~noiseBeds = Array.fill(~numChannels, { |i|
		Synth(\pdjNoiseBed, [
			\out, ~fxBus,
			\noiseBus, ~ch[i].noiseBus,
			\panBus, ~ch[i].panBus,
			\master, 0.10
		], addAction: \addToHead);
	});

	~bitPulses = Array.fill(~numChannels, { |i|
		Synth(\pdjBitPulse, [
			\out, ~fxBus,
			\glitchBus, ~ch[i].glitchBus,
			\panBus, ~ch[i].panBus,
			\master, 0.16
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
		\out, ~fxBus, \noiseBus, ~gNoiseBus, \master, 0.11
	], addAction: \addToHead);

	~crackle = Synth(\pdjCrackle, [
		\out, ~fxBus, \noiseBus, ~gNoiseBus, \master, 0.16
	], addAction: \addToHead);

	~delayFX = Synth(\pdjDelayFX, [
		\inBus, ~fxBus, \out, ~delayBus, \spaceBus, ~gSpaceBus, \mix, 0.3
	], addAction: \addToHead);

	// --------------------------------------------------------------------------
	// MACRO EVOLUTION — slow random walk, biased by the actual motion
	// average across all 4 channels, so the piece drifts on its own but
	// still answers to what is really happening on screen.
	// --------------------------------------------------------------------------
	~macroEvolve = Routine({
		loop {
			var avgEnergy = ~ch.collect(_.energy).sum / ~numChannels;

			~macro.noise   = (~macro.noise   + rrand(-0.22, 0.22) + (avgEnergy * 0.15)).clip(0.05, 0.9);
			~macro.beat    = (~macro.beat    + rrand(-0.28, 0.28) + (avgEnergy * 0.25)).clip(0.0,  1.0);
			~macro.space   = (~macro.space   + rrand(-0.22, 0.22)).clip(0.1,  0.95);
			~macro.density = (~macro.density + rrand(-0.22, 0.22) + (avgEnergy * 0.2)).clip(0.05, 1.0);

			~gNoiseBus.set(~macro.noise);
			~gDensityBus.set(~macro.density);
			~gSpaceBus.set(~macro.space);

			rrand(6.0, 14.0).wait;
		}
	}).play;

	// --------------------------------------------------------------------------
	// MAPPING HELPERS
	// --------------------------------------------------------------------------
	// Velocity -> pan destination. A blob moving right/down during a
	// trigger makes the sound sweep in that direction across the field;
	// this is the direct "space follows visual detection" link.
	~panSweep = { |ch|
		var st = ~ch[ch];
		var start = st.blob0X.linlin(0, 1, -1, 1) * ~macro.space.linlin(0, 1, 0.35, 1);
		var end   = (start + (st.blob0Vx * 3.0)).clip(-1, 1);
		[start, end];
	};

	~applyContinuous = { |ch|
		var st, energy, flow, modeG, amp, freq, noise, glitch, pan, spaceW;
		st     = ~ch[ch];
		energy = st.energy;
		flow   = st.flowMag;
		modeG  = ~modeGlitch.clipAt(st.mode.asInteger.clip(0, 13));
		spaceW = ~macro.space.linlin(0, 1, 0.35, 1); // narrow<->full stereo width

		// Sparse amp: only speaks when motion exists (Ikeda silence)
		amp = (energy * 1.8 + (flow * 0.15)).clip(0, 1);
		amp = amp * amp; // quadratic — prefers quiet

		freq = ~baseFreqs[ch]
			* (1 + (st.blob0Y.linlin(0, 1, -0.12, 0.35)))
			* (1 + (flow * 0.04));
		freq = freq.clip(200, 10000);

		pan = st.blob0X.linlin(0, 1, -0.95, 0.95) * spaceW;

		noise  = ((energy * 0.55 + (st.contour / 4000).clip(0, 0.4) + (st.crowd * 0.25)).clip(0, 1))
			* (0.5 + ~macro.noise);
		glitch = ((modeG * 0.7 + (st.blobs / 8) * 0.3 + energy * 0.2).clip(0, 1))
			* (0.5 + ~macro.density);

		st.ampBus.set(amp);
		st.freqBus.set(freq);
		st.panBus.set(pan);
		st.noiseBus.set((noise * 0.85).clip(0, 1.3));
		st.glitchBus.set(glitch.clip(0, 1.3));
		~subs[ch].set(\amp, st.crowd.clip(0, 1) * 0.55);
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
			\amp, if(kind == \ball, 0.35, 0.45),
			\panStart, sweep[0],
			\panEnd, sweep[1],
			\decay, if(kind == \ball, 0.018, 0.01)
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
				\amp, (0.18 + (dens * 0.2)) * (0.6 + ~macro.density),
				\panStart, sweep[0], \panEnd, sweep[1],
				\rate, 20 + (dens * 80),
				\dur, 0.03 + (dens * 0.08)
			]);
		});
	};

	// Motion-derived "battement" — tempo rides on energy + flow, gated
	// probabilistically by the macro beat level so it fades in/out of
	// existence rather than running mechanically at a fixed clock.
	~trigBeat = { |ch|
		var st    = ~ch[ch];
		var sweep = ~panSweep.(ch);
		var freq  = 50 + (ch * 9) + (st.blob0Y * 45);
		var tone  = (1 - ~macro.density).clip(0.05, 0.95);
		Synth(\pdjBeat, [
			\out, ~fxBus,
			\freq, freq,
			\amp, 0.28 + (~macro.beat * 0.3) + (st.crowd * 0.15),
			\panStart, sweep[0], \panEnd, sweep[1],
			\decay, 0.045 + (0.12 * (1 - ~macro.beat)),
			\tone, tone
		]);
	};

	~beatRoutines = Array.fill(~numChannels, { |i|
		Routine({
			loop {
				var st       = ~ch[i];
				var energyG  = (st.energy + (st.crowd * 0.4)).clip(0, 1);
				var bpm      = (58 + (st.energy * 130) + (st.flowMag * 15)).clip(50, 200);
				var fireProb = ((~macro.beat * 0.55) + (energyG * 0.55)).clip(0, 0.92);
				if(~macro.beat > 0.08 and: { fireProb.coin }, { ~trigBeat.(i) });
				(60 / bpm).wait;
			}
		}).play;
	});

	// --------------------------------------------------------------------------
	// OSC RECEIVE — paths match OSCSender.cpp
	// --------------------------------------------------------------------------
	thisProcess.openUDPPort(~oscPort);

	~numChannels.do { |ch|

		OSCdef(("pdj_energy_" ++ ch).asSymbol, { |msg|
			~ch[ch].energy = msg[1].asFloat.clip(0, 1);
			~applyContinuous.(ch);
		}, ("/pdj/channel/" ++ ch ++ "/motion/energy").asSymbol);

		OSCdef(("pdj_flowMag_" ++ ch).asSymbol, { |msg|
			~ch[ch].flowMag = msg[1].asFloat.max(0);
			~applyContinuous.(ch);
		}, ("/pdj/channel/" ++ ch ++ "/flow/magnitude").asSymbol);

		OSCdef(("pdj_flowAng_" ++ ch).asSymbol, { |msg|
			~ch[ch].flowAng = msg[1].asFloat;
		}, ("/pdj/channel/" ++ ch ++ "/flow/angle").asSymbol);

		OSCdef(("pdj_blobs_" ++ ch).asSymbol, { |msg|
			var n = msg[1].asInteger;
			~ch[ch].blobs = n;
			if(n >= 4, { ~trigGlitch.(ch, (n / 8).clip(0, 1), false) });
			~applyContinuous.(ch);
		}, ("/pdj/channel/" ++ ch ++ "/blobs/count").asSymbol);

		OSCdef(("pdj_contour_" ++ ch).asSymbol, { |msg|
			~ch[ch].contour = msg[1].asFloat.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/contour/length").asSymbol);

		OSCdef(("pdj_blob0x_" ++ ch).asSymbol, { |msg|
			~ch[ch].blob0X = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/blob/0/x").asSymbol);

		OSCdef(("pdj_blob0y_" ++ ch).asSymbol, { |msg|
			~ch[ch].blob0Y = msg[1].asFloat.clip(0, 1);
			~applyContinuous.(ch);
		}, ("/pdj/channel/" ++ ch ++ "/blob/0/y").asSymbol);

		OSCdef(("pdj_blob0vx_" ++ ch).asSymbol, { |msg|
			~ch[ch].blob0Vx = msg[1].asFloat.clip(-1, 1);
		}, ("/pdj/channel/" ++ ch ++ "/blob/0/vx").asSymbol);

		OSCdef(("pdj_blob0vy_" ++ ch).asSymbol, { |msg|
			~ch[ch].blob0Vy = msg[1].asFloat.clip(-1, 1);
		}, ("/pdj/channel/" ++ ch ++ "/blob/0/vy").asSymbol);

		OSCdef(("pdj_crowd_" ++ ch).asSymbol, { |msg|
			~ch[ch].crowd = msg[1].asFloat.clip(0, 1);
			~applyContinuous.(ch);
		}, ("/pdj/channel/" ++ ch ++ "/event/crowd").asSymbol);

		OSCdef(("pdj_leg_" ++ ch).asSymbol, { |msg|
			~ch[ch].leg = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/event/leg_distance").asSymbol);

		OSCdef(("pdj_mode_" ++ ch).asSymbol, { |msg|
			var m = msg[1].asInteger;
			var prev = ~ch[ch].mode;
			~ch[ch].mode = m;
			// Mode change → short data glitch (score cut)
			if(m != prev, {
				~trigGlitch.(ch, ~modeGlitch.clipAt(m.clip(0, 13)), true);
				if(m == 13, { // Flash
					~trigClick.(ch, \collision);
				});
			});
			~applyContinuous.(ch);
		}, ("/pdj/channel/" ++ ch ++ "/score/mode").asSymbol);

		OSCdef(("pdj_coll_" ++ ch).asSymbol, { |msg|
			var v = msg[1].asInteger;
			if((v == 1) && (~ch[ch].prevColl == 0), {
				~trigClick.(ch, \collision);
				~trigGlitch.(ch, 0.8, true);
			});
			~ch[ch].prevColl = v;
		}, ("/pdj/channel/" ++ ch ++ "/event/collision").asSymbol);

		OSCdef(("pdj_ball_" ++ ch).asSymbol, { |msg|
			var v = msg[1].asInteger;
			if((v == 1) && (~ch[ch].prevBall == 0), {
				~trigClick.(ch, \ball);
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

	// --------------------------------------------------------------------------
	// LIVE CONTROLS
	// --------------------------------------------------------------------------
	~setMaster  = { |g| ~masterGain = g; ~master.set(\gain, g) };
	~setNoise   = { |v| ~macro.noise   = v.clip(0, 1); ~gNoiseBus.set(v) };
	~setBeat    = { |v| ~macro.beat    = v.clip(0, 1) };
	~setSpace   = { |v| ~macro.space   = v.clip(0, 1); ~gSpaceBus.set(v) };
	~setDensity = { |v| ~macro.density = v.clip(0, 1); ~gDensityBus.set(v) };

	~shutdown = {
		~numChannels.do { |ch|
			[
				"pdj_energy_", "pdj_flowMag_", "pdj_flowAng_", "pdj_blobs_",
				"pdj_contour_", "pdj_blob0x_", "pdj_blob0y_",
				"pdj_blob0vx_", "pdj_blob0vy_", "pdj_crowd_",
				"pdj_leg_", "pdj_mode_", "pdj_coll_", "pdj_ball_",
				"pdj_ballX_", "pdj_ballY_"
			].do { |prefix|
				OSCdef((prefix ++ ch).asSymbol).free;
			};
		};
		~testOsc.stop;
		~macroEvolve.stop;
		~beatRoutines.do(_.stop);
		~tones.do(_.free);
		~noiseBeds.do(_.free);
		~bitPulses.do(_.free);
		~subs.do(_.free);
		~wash.free;
		~crackle.free;
		~delayFX.free;
		~master.free;
		~ch.do { |st|
			st.ampBus.free; st.freqBus.free; st.panBus.free;
			st.noiseBus.free; st.glitchBus.free;
		};
		~gNoiseBus.free; ~gDensityBus.free; ~gSpaceBus.free;
		~fxBus.free; ~delayBus.free;
		"pdj_datamatics shut down.".postln;
	};

	// --------------------------------------------------------------------------
	// OPTIONAL: synthetic OSC feed (no oF required)
	// --------------------------------------------------------------------------
	~testOsc = Routine({
		var net = NetAddr("127.0.0.1", ~oscPort);
		inf.do {
			~numChannels.do { |ch|
				var e = exprand(0.02, 0.7);
				var mode = 14.rand;
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/motion/energy").asSymbol, e);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/flow/magnitude").asSymbol, e * 3);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/blobs/count").asSymbol, 8.rand);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/blob/0/x").asSymbol, 1.0.rand);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/blob/0/y").asSymbol, 1.0.rand);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/blob/0/vx").asSymbol, 2.0.rand2);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/blob/0/vy").asSymbol, 2.0.rand2);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/event/crowd").asSymbol, 1.0.rand * 0.5);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/score/mode").asSymbol, mode);
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
			(1/15).wait; // ~15 Hz test stream (oF is ~30)
		};
	});

	"".postln;
	"========================================================".postln;
	"  PDJ Datamatics — listening OSC UDP :" ++ ~oscPort.asString.postln;
	"  ~testOsc.play;      // synthetic feed".postln;
	"  ~setMaster.(0.4);   // master gain".postln;
	"  ~setNoise.(0.6);    // noise wash / crackle level".postln;
	"  ~setBeat.(0.7);     // battement / pulse presence".postln;
	"  ~setSpace.(0.8);    // stereo width + delay send".postln;
	"  ~setDensity.(0.6);  // glitch density".postln;
	"  ~macro;             // inspect current macro state".postln;
	"  ~shutdown.();       // teardown".postln;
	"========================================================".postln;
});
)
