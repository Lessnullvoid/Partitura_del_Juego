/*
Partitura del Juego — estudio de descomposición funcional

Estudio de síntesis original derivado de mediciones del audio de referencia:
columna armónica de baja frecuencia, rejillas de impulsos, líneas fijas de
calibración, ruido de banda limitada, interferencia de peine, cortes secos
y decaimiento residual.

No es una recreación por muestreo ni una transcripción literal.
Herramienta independiente de análisis/audición. No ejecutarlo a la vez que
pdj_datamatics.scd, que es el motor de la instalación controlado por lo visual.

Evaluar el archivo completo y después:
    ~dmStart.value;
    ~dmScene.value(\calibration); // probar regímenes sueltos
    ~dmPlayStudy.value;           // demostración comprimida
    ~dmPlayGenerative.value;      // forma abierta y cambiante
    ~dmShutdown.value;
*/

(
// Detener una copia anterior antes de sustituir sus referencias a Routine y
// Group. Sin esto, reevaluar el archivo puede dejar huérfana la rutina generativa.
CmdPeriod.remove(~dmCmdPeriodCleanup);
~dmShutdown.tryPerform(\value);

s.waitForBoot {
	var root = 46.3; // offset deliberado respecto a la grabación medida

	SynthDef(\dmSubColumn, { |out = 0, amp = 0.12, pressure = 0.5,
		rootFreq = 46.3, gate = 1, width = 0.25|
		var drift = LFNoise2.kr([0.021, 0.017, 0.013, 0.011]).range(-0.035, 0.035);
		var ratios = [1, 2.003, 2.997, 4.011];
		var weights = [0.72, 0.22, 0.11, 0.045];
		var env = EnvGen.kr(Env.asr(2.5, 1, 5, \sine), gate, doneAction: 2);
		var sig = SinOsc.ar((rootFreq * ratios) + drift, 0, weights);
		sig = Splay.ar(sig, width, 1, 0);
		sig = LPF.ar(sig, pressure.linexp(0, 1, 150, 900));
		sig = (sig * (1 + (pressure * 2.2))).tanh;
		Out.ar(out, sig * amp.lag(1.5) * env);
	}).add;

	SynthDef(\dmCalibrationLines, { |out = 0, amp = 0.018, gate = 1,
		brightness = 0.5, width = 0.8|
		var freqs = [2017, 6270, 9960, 12540]
			* [1.007, 0.994, 1.011, 0.997];
		var env = EnvGen.kr(Env.asr(0.8, 1, 2.5, \sine), gate, doneAction: 2);
		var breathe = LFNoise1.kr([0.09, 0.071, 0.053, 0.037]).range(0.25, 1);
		var sig = SinOsc.ar(freqs, 0, breathe * [0.65, 0.42, 0.28, 0.18]);
		sig = sig * brightness.linexp(0, 1, 0.35, 1.4);
		Out.ar(out, Splay.ar(sig, width) * amp.lag(1) * env);
	}).add;

	SynthDef(\dmPulseVoice, { |out = 0, amp = 0.1, pan = 0, rootFreq = 46.3,
		brightness = 0.5, decay = 0.09, accent = 0.5|
		var env = EnvGen.ar(Env.perc(0.0003, decay, 1, -9), doneAction: 2);
		var pitchEnv = XLine.ar(rootFreq * (1.2 + accent), rootFreq, decay.max(0.01));
		var low = SinOsc.ar(pitchEnv, 0, 0.85) + (SinOsc.ar(pitchEnv * 2.003, 0, 0.2));
		var imp = Impulse.ar(0) * env;
		var centers = [1990, 6310, 10070, 12620] * brightness.linexp(0, 1, 0.78, 1.13);
		var high = Ringz.ar(imp, centers, decay * [0.16, 0.11, 0.07, 0.045],
			[0.16, 0.11, 0.07, 0.045]).sum;
		var edge = BPF.ar(GrayNoise.ar, 3000 + (brightness * 9000), 0.22)
			* EnvGen.ar(Env.perc(0.0002, 0.006)) * 0.12;
		Out.ar(out, Pan2.ar((low * env + high + edge) * amp, pan));
	}).add;

	SynthDef(\dmPulseMachine, { |out = 0, amp = 0.08, bpm = 161.5,
		density = 0.65, subdivision = 4, mask = 217, rootFreq = 46.3,
		brightness = 0.5, width = 0.8, gate = 1|
		var stepHz = (bpm / 60) * subdivision.clip(1, 12);
		var trig = Impulse.ar(stepHz);
		var count = PulseCount.ar(trig);
		var bit = (mask / (2 ** (count % 8))).floor % 2;
		var deterministicDrop = ((count * 5 + 3) % 17) / 17;
		var active = bit * (deterministicDrop < density);
		var hit = trig * active;
		// `==` es aquí una prueba de identidad del lenguaje y devuelve Boolean false.
		// Una comparación de señal produce el flujo UGen 0/1 necesario.
		var accent = ((count % subdivision.max(1)) < 1);
		var lowDecay = 0.045 + (accent * 0.09);
		var low = Ringz.ar(hit, rootFreq * [1, 2.003], lowDecay, [0.8, 0.18]).sum;
		var centers = [1980, 6280, 10020, 12610]
			* brightness.linexp(0, 1, 0.82, 1.15);
		var high = Ringz.ar(hit, centers, [0.012, 0.007, 0.004, 0.003],
			[0.12, 0.09, 0.055, 0.035]).sum;
		var panPositions = [width.neg, width * 0.35, width * -0.2, width]
			.collect { |position| K2A.ar(position) };
		var pan = Select.ar(count % 4, panPositions);
		var sig = (low + high) * EnvGen.kr(Env.asr(0.02, 1, 0.08), gate);
		Out.ar(out, Pan2.ar(sig * amp.lag(0.2), pan));
	}).add;

	SynthDef(\dmNoiseField, { |out = 0, amp = 0.05, density = 0.5,
		brightness = 0.5, width = 1, gate = 1|
		var env = EnvGen.kr(Env.asr(1.5, 1, 3, \sine), gate, doneAction: 2);
		var centers = [170, 430, 1150, 3600, 9200]
			* brightness.linexp(0, 1, 0.55, 1.7);
		var rq = density.linlin(0, 1, 0.018, 0.22);
		var n = [BrownNoise.ar, PinkNoise.ar, PinkNoise.ar, GrayNoise.ar, WhiteNoise.ar];
		var bands = BPF.ar(n, centers, rq, [0.48, 0.32, 0.21, 0.14, 0.08]);
		var movement = LFNoise1.kr([0.031, 0.041, 0.053, 0.067, 0.079]).range(0.2, 1);
		Out.ar(out, Splay.ar(bands * movement, width) * amp.lag(1) * env);
	}).add;

	SynthDef(\dmCombField, { |out = 0, amp = 0.035, brightness = 0.5,
		feedback = 0.75, width = 0.9, gate = 1|
		var env = EnvGen.kr(Env.asr(0.7, 1, 2, \sine), gate, doneAction: 2);
		var excitation = BPF.ar(PinkNoise.ar, brightness.linexp(0, 1, 180, 5200), 0.16);
		var times = [0.0047, 0.0061, 0.0097, 0.0131] * LFNoise1.kr(0.03 ! 4).range(0.985, 1.015);
		var sig = CombL.ar(excitation, 0.03, times, feedback.linexp(0, 1, 0.12, 3.5));
		Out.ar(out, Splay.ar(sig, width) * amp.lag(0.6) * env);
	}).add;

	SynthDef(\dmBurst, { |out = 0, amp = 0.12, brightness = 0.7,
		dur = 0.18, width = 1|
		var env = EnvGen.ar(Env.perc(0.0005, dur, 1, -5), doneAction: 2);
		var clock = Impulse.ar(XLine.kr(80, 4800, dur));
		var sig = Latch.ar(WhiteNoise.ar, clock);
		sig = RHPF.ar(sig, brightness.linexp(0, 1, 600, 11000), 0.24);
		Out.ar(out, Splay.ar([sig, DelayC.ar(sig, 0.03, 0.011)], width) * env * amp);
	}).add;

	SynthDef(\dmResidue, { |out = 0, amp = 0.045, gate = 1,
		rootFreq = 46.3, brightness = 0.35, width = 0.7|
		var env = EnvGen.kr(Env.asr(4, 1, 14, \sine), gate, doneAction: 2);
		var freqs = rootFreq * [4.91, 5.53, 6.47, 8.02, 9.61];
		var drift = LFNoise2.kr([0.009, 0.011, 0.014, 0.017, 0.021]).range(-0.18, 0.18);
		var sig = SinOsc.ar(freqs + drift, 0, [0.31, 0.24, 0.18, 0.11, 0.07]);
		sig = sig + BPF.ar(PinkNoise.ar, freqs, 0.025, 0.05);
		sig = LPF.ar(sig, brightness.linexp(0, 1, 700, 7000));
		Out.ar(out, Splay.ar(sig, width) * amp.lag(2) * env);
	}).add;

	// Barrido continuo de paso banda: material central de los barridos de
	// ruido ascendentes y descendentes. reverse = 0 sube; reverse = 1 baja.
	SynthDef(\dmNoiseSweep, { |out = 0, amp = 0.055, gate = 1,
		rate = 0.045, reverse = 0, lo = 70, hi = 12500,
		brightness = 0.6, width = 0.9|
		var env = EnvGen.kr(Env.asr(0.4, 1, 2.4, \sine), gate, doneAction: 2);
		var phase = LFTri.kr(rate.max(0.005), 3).range(0, 1);
		var position = (phase * (1 - reverse)) + ((1 - phase) * reverse);
		var center = position.linexp(0, 1, lo, hi)
			* brightness.linexp(0, 1, 0.72, 1.28);
		var source = [PinkNoise.ar, GrayNoise.ar];
		var rq = position.linlin(0, 1, 0.18, 0.035);
		var band = BPF.ar(source, center * [0.985, 1.015], rq);
		var air = HPF.ar(WhiteNoise.ar(0.08), (center * 0.7).clip(300, 9000));
		var sig = Splay.ar(band, width) + Pan2.ar(air, position.linlin(0, 1, width.neg, width));
		Out.ar(out, sig * amp.lag(0.4) * env);
	}).add;

	// Barras espectrales cuantizadas: los saltos de frecuencia central emulan
	// los bloques de banda horizontales visibles entre los pasajes de pulso.
	SynthDef(\dmBandBars, { |out = 0, amp = 0.045, gate = 1,
		rate = 3.2, brightness = 0.55, width = 0.85|
		var env = EnvGen.kr(Env.asr(0.15, 1, 1.2), gate, doneAction: 2);
		var stepA = LFNoise0.kr(rate).range(0, 7).round(1);
		var stepB = LFNoise0.kr(rate * 0.731).range(0, 5).round(1);
		var centerA = 95 * (1.72 ** stepA) * brightness.linexp(0, 1, 0.8, 1.25);
		var centerB = 180 * (1.58 ** stepB);
		var gateA = LFPulse.kr(rate * 0.5, 0, 0.63).lag(0.002);
		var gateB = LFPulse.kr(rate * 0.365, 0.3, 0.48).lag(0.002);
		var a = BPF.ar(PinkNoise.ar, centerA.clip(45, 14000), 0.045) * gateA;
		var b = BPF.ar(GrayNoise.ar, centerB.clip(60, 12000), 0.07) * gateB;
		Out.ar(out, Splay.ar([a, b], width) * amp.lag(0.3) * env);
	}).add;

	// Relojes muy próximos se desfasan entre sí. La repetición produce primero
	// un batido rítmico y después una textura de interferencia más densa.
	SynthDef(\dmPhaseGrid, { |out = 0, amp = 0.055, gate = 1,
		bpm = 161.5, spread = 0.004, rootFreq = 46.3,
		brightness = 0.55, width = 0.9|
		var env = EnvGen.kr(Env.asr(0.08, 1, 0.4), gate, doneAction: 2);
		var base = bpm / 60;
		var rates = base * [3, 4 * (1 + spread), 5 * (1 - (spread * 0.7))];
		var hits = Impulse.ar(rates);
		var lows = Ringz.ar(hits, rootFreq * [1, 2.003, 3.011], [0.08, 0.045, 0.025]);
		var highs = Ringz.ar(
			hits,
			[2050, 6320, 10100] * brightness.linexp(0, 1, 0.8, 1.2),
			[0.011, 0.007, 0.004]
		);
		Out.ar(out, Splay.ar((lows * 0.7) + (highs * 0.16), width) * amp * env);
	}).add;

	// Eventos microscópicos dispersos separados por silencio real. Density
	// controla la probabilidad en el tiempo, no el nivel de una textura continua.
	SynthDef(\dmMicroCuts, { |out = 0, amp = 0.05, gate = 1,
		density = 4, brightness = 0.6, width = 1|
		var env = EnvGen.kr(Env.asr(0.03, 1, 0.3), gate, doneAction: 2);
		var trig = Dust.ar(density.linexp(0, 10, 0.15, 35));
		var grainEnv = Decay2.ar(trig, 0.0002, TRand.ar(0.002, 0.045, trig));
		var center = TExpRand.ar(250, 14000, trig)
			* brightness.linexp(0, 1, 0.75, 1.2);
		var tone = Ringz.ar(trig, center.clip(100, 16000), 0.008);
		var noise = BPF.ar(WhiteNoise.ar, center.clip(100, 16000), 0.12) * grainEnv;
		var pan = TRand.ar(width.neg, width, trig);
		Out.ar(out, Pan2.ar((tone * 0.18) + (noise * 0.45), pan) * amp * env);
	}).add;

	// Puntos de referencia de alta frecuencia pulsan independientes de la máquina de graves.
	SynthDef(\dmBeacons, { |out = 0, amp = 0.018, gate = 1,
		rate = 1.35, brightness = 0.7, width = 0.95|
		var env = EnvGen.kr(Env.asr(0.3, 1, 1.5, \sine), gate, doneAction: 2);
		var freqs = [2010, 6350, 10040, 12680]
			* brightness.linexp(0, 1, 0.9, 1.08);
		var pulses = LFPulse.kr(rate * [1, 0.5, 0.25, 0.125],
			[0, 0.17, 0.41, 0.73], [0.08, 0.05, 0.035, 0.025]).lag(0.001);
		var sig = SinOsc.ar(freqs, 0, pulses * [0.7, 0.45, 0.28, 0.18]);
		Out.ar(out, Splay.ar(sig, width) * amp * env);
	}).add;

	// El espacio negativo se trata como escena: pulsos aislados y una referencia
	// grave casi inaudible impiden que el silencio sea un preset vacío.
	SynthDef(\dmNegativeSpace, { |out = 0, amp = 0.035, gate = 1,
		rootFreq = 46.3, width = 0.9|
		var env = EnvGen.kr(Env.asr(0.05, 1, 1.8), gate, doneAction: 2);
		var trig = Dust.ar(0.22);
		var click = Ringz.ar(trig, TChoose.kr(A2K.kr(trig), [980, 2010, 6320, 10040]), 0.025);
		var pan = TRand.ar(width.neg, width, trig);
		var reference = SinOsc.ar(rootFreq, 0, 0.025);
		Out.ar(out, (Pan2.ar(click * 0.22, pan) + (reference ! 2)) * amp * env);
	}).add;

	// Presión densa de registro grave sin un pulso explícito.
	SynthDef(\dmPressureMass, { |out = 0, amp = 0.075, gate = 1,
		rootFreq = 46.3, pressure = 0.75, width = 0.55|
		var env = EnvGen.kr(Env.asr(1.4, 1, 3.5, \sine), gate, doneAction: 2);
		var drift = LFNoise2.kr([0.07, 0.053, 0.039]).range(0.992, 1.008);
		var tone = SinOsc.ar(rootFreq * [1, 1.5, 2.003] * drift,
			0, [0.68, 0.18, 0.11]);
		var rumble = BPF.ar(BrownNoise.ar, rootFreq * [1.3, 2.7], 0.18,
			[0.26, 0.12]);
		var sig = Splay.ar(tone ++ rumble, width);
		sig = (sig * pressure.linlin(0, 1, 1, 4)).tanh;
		Out.ar(out, sig * amp.lag(0.6) * env);
	}).add;

	// Machine C: una transformación continua. Una banda ancha de ruido se estrecha,
	// se fragmenta y se resuelve en una rejilla de impulsos afinados durante morphTime.
	SynthDef(\dmNoiseToPulse, { |out = 0, amp = 0.07, gate = 1,
		bpm = 161.5, subdivision = 4, morphTime = 18,
		rootFreq = 46.3, brightness = 0.6, width = 0.9|
		var env = EnvGen.kr(Env.asr(0.3, 1, 1.8, \sine), gate, doneAction: 2);
		var morph = Line.kr(0, 1, morphTime.max(1));
		var center = morph.linexp(0, 1, 260, 7200)
			* brightness.linexp(0, 1, 0.75, 1.3);
		var noise = BPF.ar([PinkNoise.ar, GrayNoise.ar],
			center * [0.97, 1.03], morph.linexp(0, 1, 0.32, 0.025));
		var clockRate = (bpm / 60) * morph.linlin(0, 1, 0.5, subdivision);
		var trig = Impulse.ar(clockRate);
		var pulse = Ringz.ar(
			trig,
			rootFreq * [1, 2.003, 4.017, 8.041],
			[0.11, 0.065, 0.035, 0.018],
			[0.62, 0.28, 0.12, 0.055]
		);
		var fragments = noise * Decay2.ar(trig, 0.001, morph.linlin(0, 1, 0.7, 0.045));
		var sig = (noise * (1 - morph).squared)
			+ (fragments * (morph * (1 - morph) * 2.4))
			+ (pulse * morph.squared);
		Out.ar(out, Splay.ar(sig, width) * amp.lag(0.2) * env);
	}).add;

	// Machine D: el drone y el ritmo ocupan ventanas complementarias. El fundido
	// cruza produce pregunta-respuesta en lugar de sumar ambas capas en continuo.
	SynthDef(\dmDroneRhythmInterlock, { |out = 0, amp = 0.075, gate = 1,
		bpm = 161.5, cycle = 10, droneShare = 0.58,
		rootFreq = 46.3, brightness = 0.5, width = 0.8|
		var env = EnvGen.kr(Env.asr(0.4, 1, 2.2, \sine), gate, doneAction: 2);
		var droneWindow = LFPulse.kr(cycle.max(2).reciprocal, 0,
			droneShare.clip(0.2, 0.8)).lag(0.12);
		var rhythmWindow = 1 - droneWindow;
		var drift = LFNoise2.kr([0.018, 0.023, 0.031]).range(0.995, 1.005);
		var drone = SinOsc.ar(rootFreq * [1, 1.501, 2.003] * drift,
			0, [0.62, 0.18, 0.11]);
		var trig = Impulse.ar((bpm / 60) * [2, 3, 4.003]);
		var rhythm = Ringz.ar(
			trig,
			rootFreq * [2.003, 5.01, 9.03]
				* brightness.linexp(0, 1, 0.86, 1.18),
			[0.075, 0.032, 0.014],
			[0.5, 0.2, 0.09]
		);
		var transitionNoise = BPF.ar(PinkNoise.ar,
			brightness.linexp(0, 1, 320, 6200), 0.06)
			* (droneWindow * rhythmWindow * 4);
		var sig = (drone * droneWindow) + (rhythm * rhythmWindow)
			+ (transitionNoise * 0.35);
		Out.ar(out, Splay.ar(sig, width) * amp * env);
	}).add;

	// Interferencia afinada: una red de razones cambia despacio mientras
	// frecuencias cercanas baten entre sí. Aporta contorno y memoria sin
	// melodía principal convencional ni progresión de acordes en temperamento igual.
	SynthDef(\dmTonalInterference, { |out = 0, amp = 0.05, gate = 1,
		rootFreq = 46.3, changeRate = 0.32, detune = 0.006,
		brightness = 0.55, feedback = 0.62, width = 0.9|
		var env = EnvGen.kr(Env.asr(0.8, 1, 3, \sine), gate, doneAction: 2);
		var changeTrig = Impulse.kr(changeRate.max(0.03));
		var step = PulseCount.kr(changeTrig) % 8;
		var ratio = Select.kr(step,
			[1, 9/8, 5/4, 4/3, 3/2, 8/5, 7/4, 2]);
		var pitch = Lag.kr(rootFreq * ratio, 0.45);
		var cluster = SinOsc.ar(
			pitch * [1, 1 + detune, 2.003, 3.011],
			0,
			[0.42, 0.31, 0.12, 0.055]
		);
		var excitation = Splay.ar(cluster, width);
		var delays = (pitch * [1, 1.5]).reciprocal.clip(0.002, 0.05);
		var comb = CombC.ar(excitation, 0.06, delays,
			feedback.linexp(0, 1, 0.18, 3.2));
		var air = BPF.ar(PinkNoise.ar,
			pitch * brightness.linexp(0, 1, 5, 28), 0.035) * 0.12;
		Out.ar(out, (excitation + (comb * 0.55) + (air ! 2)) * amp.lag(0.4) * env);
	}).add;

	SynthDef(\dmMaster, { |in = 0, out = 0, gain = 0.65, space = 0.12|
		var dry = In.ar(in, 2);
		var early = DelayC.ar(dry.reverse, 0.08, [0.017, 0.047]);
		var wet = FreeVerb2.ar(early[0], early[1], space, 0.82, 0.78);
		var sig = dry + (wet * space * 0.45);
		sig = HPF.ar(LPF.ar(sig, 16500), 24);
		sig = (sig * 1.35).tanh;
		Out.ar(out, Limiter.ar(sig * gain, 0.92, 0.01));
	}).add;

	s.sync;

	// ----------------------------------------------------------------------
	// CONTROLADOR IDIOMÁTICO EN EL LADO DEL LENGUAJE
	// ----------------------------------------------------------------------
	~dmRoot = root;
	~dmBus = nil;
	~dmGroup = nil;
	~dmNodes = IdentityDictionary.new;
	~dmStudy = nil;
	~dmEngineActive = false;

	~dmFreeVoices = {
		var keys = ~dmNodes.keys;
		keys.do { |key|
			if(key != \master, {
				~dmNodes[key].tryPerform(\set, \gate, 0);
				~dmNodes.removeAt(key);
			});
		};
	};

	~dmSpawn = { |key, defName, args = #[]|
		~dmNodes[key].tryPerform(\set, \gate, 0);
		~dmNodes[key] = Synth.head(
			~dmGroup,
			defName,
			[\out, ~dmBus] ++ args
		);
	};

	~dmScene = { |name = \calibration|
		if(~dmEngineActive != true, {
			("DM scene ignored (standalone study is not active): " ++ name).warn;
		}, {
			("DM study scene -> " ++ name).postln;
			~dmFreeVoices.value;

			switch(name,
			\calibration, {
				~dmSpawn.value(\sub, \dmSubColumn,
					[\amp, 0.075, \pressure, 0.18, \rootFreq, ~dmRoot]);
				~dmSpawn.value(\lines, \dmCalibrationLines,
					[\amp, 0.012, \brightness, 0.35]);
			},
			\machineA, {
				~dmSpawn.value(\sub, \dmSubColumn,
					[\amp, 0.11, \pressure, 0.7, \rootFreq, ~dmRoot]);
				~dmSpawn.value(\pulse, \dmPulseMachine,
					[\amp, 0.075, \bpm, 159.7, \subdivision, 4,
					\density, 0.72, \mask, 217, \rootFreq, ~dmRoot]);
				~dmSpawn.value(\lines, \dmCalibrationLines,
					[\amp, 0.009, \brightness, 0.7]);
			},
			\machineB, {
				~dmSpawn.value(\sub, \dmSubColumn,
					[\amp, 0.13, \pressure, 0.9, \rootFreq, ~dmRoot]);
				~dmSpawn.value(\pulse, \dmPulseMachine,
					[\amp, 0.065, \bpm, 159.7, \subdivision, 6,
					\density, 0.84, \mask, 181, \brightness, 0.72,
					\rootFreq, ~dmRoot]);
				~dmSpawn.value(\noise, \dmNoiseField,
					[\amp, 0.032, \density, 0.66, \brightness, 0.7]);
			},
			\machineC, {
				~dmSpawn.value(\conversion, \dmNoiseToPulse,
					[\amp, 0.078, \bpm, 159.7, \subdivision, 4,
					\morphTime, 20, \rootFreq, ~dmRoot, \brightness, 0.68]);
				~dmSpawn.value(\beacons, \dmBeacons,
					[\amp, 0.005, \rate, 0.67, \brightness, 0.74]);
			},
			\machineD, {
				~dmSpawn.value(\interlock, \dmDroneRhythmInterlock,
					[\amp, 0.082, \bpm, 159.7, \cycle, 11,
					\droneShare, 0.62, \rootFreq, ~dmRoot, \brightness, 0.56]);
				~dmSpawn.value(\tonal, \dmTonalInterference,
					[\amp, 0.022, \rootFreq, ~dmRoot * 2,
					\changeRate, 0.18, \detune, 0.004, \feedback, 0.35]);
			},
			\noiseRise, {
				~dmSpawn.value(\sweep, \dmNoiseSweep,
					[\amp, 0.062, \rate, 0.038, \reverse, 0,
					\lo, 65, \hi, 13800, \brightness, 0.7]);
				~dmSpawn.value(\lines, \dmCalibrationLines,
					[\amp, 0.004, \brightness, 0.65]);
			},
			\noiseFall, {
				~dmSpawn.value(\sweep, \dmNoiseSweep,
					[\amp, 0.058, \rate, 0.052, \reverse, 1,
					\lo, 80, \hi, 11000, \brightness, 0.48]);
				~dmSpawn.value(\sub, \dmSubColumn,
					[\amp, 0.045, \pressure, 0.24, \rootFreq, ~dmRoot]);
			},
			\bandBars, {
				~dmSpawn.value(\bars, \dmBandBars,
					[\amp, 0.058, \rate, 3.7, \brightness, 0.62]);
				~dmSpawn.value(\beacons, \dmBeacons,
					[\amp, 0.008, \rate, 1.35, \brightness, 0.72]);
			},
			\phaseGrid, {
				~dmSpawn.value(\phase, \dmPhaseGrid,
					[\amp, 0.072, \bpm, 159.7, \spread, 0.006,
					\rootFreq, ~dmRoot, \brightness, 0.66]);
				~dmSpawn.value(\sub, \dmSubColumn,
					[\amp, 0.052, \pressure, 0.46, \rootFreq, ~dmRoot]);
			},
			\microCuts, {
				~dmSpawn.value(\micro, \dmMicroCuts,
					[\amp, 0.065, \density, 5.5, \brightness, 0.76]);
				~dmSpawn.value(\beacons, \dmBeacons,
					[\amp, 0.006, \rate, 0.82, \brightness, 0.78]);
			},
			\beacons, {
				~dmSpawn.value(\beacons, \dmBeacons,
					[\amp, 0.017, \rate, 1.35, \brightness, 0.78]);
				~dmSpawn.value(\lines, \dmCalibrationLines,
					[\amp, 0.004, \brightness, 0.7]);
			},
			\negativeSpace, {
				~dmSpawn.value(\space, \dmNegativeSpace,
					[\amp, 0.045, \rootFreq, ~dmRoot]);
			},
			\pressure, {
				~dmSpawn.value(\pressure, \dmPressureMass,
					[\amp, 0.092, \rootFreq, ~dmRoot, \pressure, 0.82]);
				~dmSpawn.value(\noise, \dmNoiseField,
					[\amp, 0.018, \density, 0.28, \brightness, 0.2]);
			},
			\interference, {
				~dmSpawn.value(\noise, \dmNoiseField,
					[\amp, 0.075, \density, 0.92, \brightness, 0.8]);
				~dmSpawn.value(\comb, \dmCombField,
					[\amp, 0.055, \feedback, 0.85, \brightness, 0.72]);
			},
			\interferenceTonal, {
				~dmSpawn.value(\tonal, \dmTonalInterference,
					[\amp, 0.065, \rootFreq, ~dmRoot * 2,
					\changeRate, 0.26, \detune, 0.007,
					\brightness, 0.62, \feedback, 0.72]);
				~dmSpawn.value(\comb, \dmCombField,
					[\amp, 0.018, \feedback, 0.62, \brightness, 0.52]);
			},
			\interferenceDrift, {
				~dmSpawn.value(\tonal, \dmTonalInterference,
					[\amp, 0.054, \rootFreq, ~dmRoot * 1.5,
					\changeRate, 0.11, \detune, 0.014,
					\brightness, 0.38, \feedback, 0.88]);
				~dmSpawn.value(\noise, \dmNoiseField,
					[\amp, 0.014, \density, 0.24, \brightness, 0.34]);
			},
			\fragmented, {
				~dmSpawn.value(\sub, \dmSubColumn,
					[\amp, 0.09, \pressure, 0.62, \rootFreq, ~dmRoot]);
				~dmSpawn.value(\pulse, \dmPulseMachine,
					[\amp, 0.07, \bpm, 159.7, \subdivision, 3,
					\density, 0.48, \mask, 145, \brightness, 0.58,
					\rootFreq, ~dmRoot]);
				~dmSpawn.value(\comb, \dmCombField,
					[\amp, 0.025, \feedback, 0.58]);
			},
			\rupture, {
				Synth.head(~dmGroup, \dmBurst,
					[\out, ~dmBus, \amp, 0.14, \dur, 0.32]);
			},
			\residue, {
				~dmSpawn.value(\residue, \dmResidue,
					[\amp, 0.045, \rootFreq, ~dmRoot, \brightness, 0.32]);
				~dmSpawn.value(\lines, \dmCalibrationLines,
					[\amp, 0.004, \brightness, 0.18]);
			},
				{ ("Unknown DM scene: " ++ name).warn; }
			);
		});
	};

	~dmShutdown = {
		~dmStudy.tryPerform(\stop);
		~dmStudy = nil;
		~dmEngineActive = false;
		~dmNodes.keysValuesDo { |key, node| node.tryPerform(\free) };
		~dmNodes = IdentityDictionary.new;
		~dmGroup.tryPerform(\freeAll);
		~dmGroup.tryPerform(\free);
		~dmGroup = nil;
		~dmBus.tryPerform(\free);
		~dmBus = nil;
		"DM study stopped".postln;
	};

	~dmStart = {
		if(~pdjEngineActive == true, {
			"DM study not started: pdj_datamatics is active. Run ~shutdown.value first.".warn;
		}, {
			~dmShutdown.value;
			~dmBus = Bus.audio(s, 2);
			~dmGroup = Group.head(s);
			~dmNodes = IdentityDictionary.new;
			~dmNodes[\master] = Synth.tail(
				~dmGroup,
				\dmMaster,
				[\in, ~dmBus, \out, 0, \gain, 0.65, \space, 0.10]
			);
			~dmEngineActive = true;
			~dmScene.value(\calibration);
			"DM study started".postln;
		});
	};

	~dmPlayStudy = {
		if(~pdjEngineActive == true, {
			"DM study not started: pdj_datamatics is active. Run ~shutdown.value first.".warn;
		}, {
			~dmStart.value;
			~dmStudy = Routine({
			8.wait;  ~dmScene.value(\beacons);
			8.wait;  ~dmScene.value(\machineA);
			16.wait; ~dmScene.value(\phaseGrid);
			12.wait; ~dmScene.value(\noiseRise);
			12.wait; ~dmScene.value(\bandBars);
			12.wait; ~dmScene.value(\microCuts);
			9.wait;  ~dmScene.value(\machineC);
			21.wait; ~dmScene.value(\machineB);
			18.wait; ~dmScene.value(\noiseFall);
			10.wait; ~dmScene.value(\interference);
			14.wait; ~dmScene.value(\interferenceTonal);
			18.wait; ~dmScene.value(\machineD);
			22.wait; ~dmScene.value(\interferenceDrift);
			17.wait; ~dmScene.value(\negativeSpace);
			7.wait;  ~dmScene.value(\pressure);
			14.wait; ~dmScene.value(\fragmented);
			16.wait; ~dmScene.value(\rupture);
			4.wait;  ~dmScene.value(\residue);
			24.wait;
			~dmNodes[\residue].tryPerform(\set, \gate, 0);
			~dmNodes[\lines].tryPerform(\set, \gate, 0);
			}).play(SystemClock);
		});
	};

	// Selección abierta de estados. El azar cambia el orden y la duración,
	// mientras el vocabulario de escenas mantiene el resultado acotado.
	~dmPlayGenerative = {
		if(~pdjEngineActive == true, {
			"DM study not started: pdj_datamatics is active. Run ~shutdown.value first.".warn;
		}, {
			~dmStart.value;
			~dmStudy = Routine({
			var scenes = [
				\calibration, \beacons, \machineA, \machineB,
				\machineC, \machineD, \phaseGrid, \noiseRise,
				\bandBars, \microCuts, \noiseFall, \interference,
				\interferenceTonal, \interferenceDrift, \negativeSpace,
				\pressure, \fragmented, \rupture, \residue
			];
			var weights = [
				0.05, 0.05, 0.08, 0.08, 0.08,
				0.08, 0.07, 0.06, 0.06, 0.05,
				0.05, 0.05, 0.06, 0.05, 0.04,
				0.05, 0.04, 0.02, 0.02
			].normalizeSum;
			var previous = \calibration;
			var next, duration;
			loop {
				next = scenes.wchoose(weights);
				while({ next == previous }, {
					next = scenes.wchoose(weights);
				});
				~dmScene.value(next);
				duration = if(next == \machineC, {
					rrand(20.0, 28.0) // dejar que termine el morph ruido-a-pulso
				}, {
					if(next == \machineD, {
						rrand(18.0, 30.0) // mostrar al menos dos ciclos de interlock
					}, {
						if(next == \rupture, {
							rrand(2.0, 5.0)
						}, {
							if(next == \negativeSpace, {
								rrand(5.0, 10.0)
							}, {
								rrand(9.0, 24.0)
							})
						})
					})
				});
				duration.wait;
				previous = next;
			};
			}).play(SystemClock);
		});
	};

	// Cmd+. debe limpiar la Routine y las referencias del lenguaje además de los
	// nodos del servidor; si no, un cambio de escena posterior apunta a un Group ID obsoleto.
	~dmCmdPeriodCleanup = { ~dmShutdown.value };
	CmdPeriod.add(~dmCmdPeriodCleanup);

	"Data-matrix functional study loaded.".postln;
	"Run ~dmStart.value, ~dmPlayStudy.value, or ~dmPlayGenerative.value".postln;
};
)
