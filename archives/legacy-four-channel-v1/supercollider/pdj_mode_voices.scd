/*
  Scene-specific cinematic voices for pdj_datamatics.scd.
  This file is loaded automatically by the main engine.
*/

~installModeVoices = {
	var addVoice;

	addVoice = { |name, voiceFunc|
		SynthDef(name, { |out = 0, ampBus, panBus, tensionBus, spectralBus,
			sceneAmp = 0.7, color = 0.2, variant = 0, evolution = 0,
			lifeGate = 1, master = 0.14|
			var activity = In.kr(ampBus, 1).lag(3);
			var pan = In.kr(panBus, 1).lag(2.5);
			var tension = In.kr(tensionBus, 1).lagud(0.4, 8).clip(0, 1);
			var evo = evolution.lag(5).clip(0, 1);
			var anchor = In.kr(spectralBus, 1).lag(4) * evo.linexp(0, 1, 0.58, 2.2);
			var base = (31.7 + (variant * 1.75)) * evo.linexp(0, 1, 0.88, 1.38);
			var life = EnvGen.kr(Env.asr(0.45, 1, 1.8, \sine), lifeGate, doneAction: 2);
			var form = 0.65 + ((evo * pi).sin * 0.35);
			var presence = sceneAmp.lag(0.7) * form
				* (0.34 + (activity * 0.30) + (tension * 0.12));
			var sig = voiceFunc.value(base, anchor, tension, activity, color, variant, evo);
			var ceiling = evo.linexp(0, 1, 3500, 17000) * tension.linexp(0, 1, 0.8, 1.15);
			sig = LeakDC.ar(LPF.ar(sig, ceiling.clip(1800, 18000)));
			Out.ar(out, Pan2.ar(sig * presence * life * master, pan * 0.72));
		}).add;
	};

	addVoice.(\pdjModeBwClean, { |base, anchor, tension, activity, color, variant, evo|
		Mix.ar(SinOsc.ar(
			base * [1, 1.0026 + (variant * 0.0003), 2.005],
			0, [0.48, 0.30, 0.10]
		)) + (BPF.ar(BrownNoise.ar, base * evo.linexp(0, 1, 4, 18), 0.045 + (evo * 0.06)) * (0.05 + (evo * 0.18)))
	});

	addVoice.(\pdjModeScanLine, { |base, anchor, tension, activity, color, variant, evo|
		var scan = LFNoise1.kr(0.035 + (evo * 0.18) + (variant * 0.012)).exprange(0.45, 2.8 + (evo * 2));
		SinOsc.ar(base * [1, 3.011], 0, [0.18, 0.10]).sum
			+ (BPF.ar(PinkNoise.ar, anchor * scan, 0.025 + (tension * 0.055)) * 0.72)
	});

	addVoice.(\pdjModeBBoxTracker, { |base, anchor, tension, activity, color, variant|
		Mix.ar(SinOsc.ar(
			base * [1, 5.03 + (variant * 0.015), 9.11 + (tension * 0.08)],
			0, [0.20, 0.18, 0.10]
		)) + (BPF.ar(PinkNoise.ar, anchor * 0.72, 0.06) * (0.12 + activity) * 0.28)
	});

	addVoice.(\pdjModeBinaryText, { |base, anchor, tension, activity, color, variant, evo|
		var step = LFNoise0.kr(0.2 + (evo * 2.4) + (variant * 0.11)).range(0.985, 1.015 + (tension * 0.025) + (evo * 0.02));
		SinOsc.ar(base * [1, 8.03, 13.07] * step, 0, [0.16, 0.14, 0.09]).sum
			+ (LPF.ar(LFPulse.ar(2 + (variant * 0.5), 0, 0.08 + (tension * 0.18)), 1800) * 0.11)
	});

	addVoice.(\pdjModeWaveform, { |base, anchor, tension, activity, color, variant, evo|
		var curve = SinOsc.kr(0.012 + (evo * 0.14) + (variant * 0.006), variant).range(0.994 - (evo * 0.008), 1.006 + (tension * 0.006) + (evo * 0.012));
		SinOsc.ar(base * [1, 2.003, 4.009] * curve, 0, [0.38, 0.14, 0.055]).sum
			+ (BPF.ar(BrownNoise.ar, anchor * 0.38, 0.08 + (activity * 0.08)) * 0.18)
	});

	addVoice.(\pdjModeGridData, { |base, anchor, tension, activity, color, variant, evo|
		var trig = Dust.ar(0.08 + (evo.squared * 1.4) + (activity * 0.7) + (tension * 0.4));
		SinOsc.ar(base, 0, 0.13)
			+ Ringz.ar(
				trig, base * [8.01, 13.03, 21.07],
				0.7 + (tension * 1.2), [0.24, 0.16, 0.08]
			).sum
	});

	addVoice.(\pdjModeBarcode, { |base, anchor, tension, activity, color, variant, evo|
		var bars = BPF.ar(
			GrayNoise.ar, anchor * [0.7, 1.3, 2.1],
			0.025 + (tension * 0.035)
		).sum * 0.36;
		SinOsc.ar(base * [1, 7.01], 0, [0.12, 0.10]).sum
			+ CombL.ar(bars, 0.12, 0.018 + (evo * 0.055) + (variant * 0.003), 0.8 + tension + (evo * 1.8))
	});

	addVoice.(\pdjModeVideoNormal, { |base, anchor, tension, activity, color, variant|
		SinOsc.ar(
			base * [1, 1.0032, 2.006, 3.012],
			0, [0.44, 0.27, 0.11, 0.05]
		).sum + (BPF.ar(BrownNoise.ar, base * (7 + variant), 0.09) * 0.20)
	});

	addVoice.(\pdjModeVideoSquares, { |base, anchor, tension, activity, color, variant, evo|
		var centers = anchor * [0.42, 0.8, 1.55, 2.7]
			* LFNoise1.kr(
				[0.027, 0.033, 0.041, 0.049] + (variant * 0.002)
			).range(0.92 - (evo * 0.25), 1.08 + (evo * 0.35));
		SinOsc.ar(base, 0, 0.13)
			+ (Mix.ar(BPF.ar(PinkNoise.ar(0.7), centers, 0.035 + (tension * 0.045))) * 0.38)
	});

	addVoice.(\pdjModeVideoNumbers, { |base, anchor, tension, activity, color, variant, evo|
		var digit = LFNoise0.kr(0.08 + (evo * 1.1) + (variant * 0.035)).range(0, 9).round(1);
		var ratio = 1 + (digit * 0.083);
		SinOsc.ar(base * [1, ratio, ratio * 4.01], 0, [0.16, 0.20, 0.12]).sum
			+ (BPF.ar(PinkNoise.ar, anchor * ratio, 0.04) * 0.30)
	});

	addVoice.(\pdjModeVideoLines, { |base, anchor, tension, activity, color, variant|
		var edges = BPF.ar(
			PinkNoise.ar, anchor * [1.4, 2.9, 5.2],
			0.018 + (tension * 0.05)
		).sum * 0.34;
		SinOsc.ar(base * [1, 2.007], 0, [0.16, 0.11]).sum + edges
	});

	addVoice.(\pdjModeThermal, { |base, anchor, tension, activity, color, variant, evo|
		SinOsc.ar(base * [0.78, 1, 1.0045], 0, [0.18, 0.30, 0.22]).sum
			+ (LPF.ar(BrownNoise.ar, 120 + (evo * 2600) + (tension * 700)) * (0.12 + (evo * 0.32)))
	});

	addVoice.(\pdjModeSlitScan, { |base, anchor, tension, activity, color, variant, evo|
		var source = SinOsc.ar(base, 0, 0.12)
			+ (BPF.ar(PinkNoise.ar, anchor * 0.7, 0.035) * 0.30);
		source + (CombL.ar(
			source, 0.45, 0.08 + (evo * 0.27) + (variant * 0.018),
			1.8 + (evo * 5.5) + (tension * 2)
		) * 0.78)
	});

	addVoice.(\pdjModeFlash, { |base, anchor, tension, activity, color, variant|
		SinOsc.ar(base * [1, 16.03], 0, [0.16, 0.035]).sum
			+ (BPF.ar(WhiteNoise.ar, anchor * 3.5, 0.12 + (tension * 0.08)) * 0.16)
	});

	SynthDef(\pdjKick, { |out = 0, amp = 0.22, pan = 0, weight = 0.5|
		var env = EnvGen.ar(
			Env.perc(0.002, 0.75 + (weight * 0.35), 1, -7),
			doneAction: 2
		);
		var pitch = XLine.kr(72 + (weight * 24), 29, 0.19);
		var body = SinOsc.ar(pitch, 0, 0.72)
			+ (SinOsc.ar(pitch * 1.006, 0, 0.24));
		var edge = BPF.ar(PinkNoise.ar, 950 + (weight * 850), 0.18)
			* EnvGen.ar(Env.perc(0.001, 0.035)) * 0.10;
		Out.ar(out, Pan2.ar((body * env + edge) * amp, pan));
	}).add;

	~modeSynthNames = [
		\pdjModeBwClean, \pdjModeScanLine, \pdjModeBBoxTracker,
		\pdjModeBinaryText, \pdjModeWaveform, \pdjModeGridData,
		\pdjModeBarcode, \pdjModeVideoNormal, \pdjModeVideoSquares,
		\pdjModeVideoNumbers, \pdjModeVideoLines, \pdjModeThermal,
		\pdjModeSlitScan, \pdjModeFlash
	];
};
