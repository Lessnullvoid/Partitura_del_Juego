// =============================================================================
// Partitura del Juego — Motor de audio Datamatics (SuperCollider)
// Datamatics auténtico de Ikeda: el dato como material/estructura/señal,
// unidades binarias elementales, un continuo clic->ritmo->altura->ruido,
// montaje por módulos con cortes secos, y un bucle recursivo de
// «meta-datamatics». Sin melodía, sin armonía, sin sistema de escala/acorde:
// la estructura nace de rejillas, umbrales, densidad y discontinuidad,
// según el método documentado del propio Ikeda.
//
// Estética vocal: orientada al drone/ruido ambiental más que al glitch crudo.
// Las capas atmosféricas orbitan un único centro espectral de deriva lenta.
//
// Recibe OSC de openFrameworks (OSCSender) en el puerto UDP 9001:
//   /pdj/channel/{0-7}/flow/magnitude|angle
//   /pdj/channel/{0-7}/motion/energy
//   /pdj/channel/{0-7}/blobs/count
//   /pdj/channel/{0-7}/contour/length
//   /pdj/channel/{0-7}/blob/{0-7}/state
//   /pdj/channel/{0-7}/event/collision|ball|ball/x|ball/y|crowd|leg_distance
//   /pdj/channel/{0-7}/score/*
//   /pdj/channel/{0-7}/video/*
//   /pdj/channel/{0-7}/director/*
//   /pdj/channel/{0-7}/generator/*
//   /pdj/channel/{0-7}/program/*
//   /pdj/clock/state
//
// Uso: arrancar este archivo y después lanzar Partitura_del_Juego.
// Alimentación sintética opcional: ~testOsc.play
// No ejecutar data_matrix_functional_study.scd al mismo tiempo: este archivo
// es el motor de la instalación. Detener con Cmd+. o ~shutdown.()
// =============================================================================

(
var scriptDir = PathName(thisProcess.nowExecutingPath).pathOnly;

// Recargar el archivo debe desmontar el controlador previo del lenguaje
// antes de sustituir sus referencias. Detener también el estudio de análisis
// independiente: ambos motores tienen grupos, relojes y cadenas de salida
// propios y no deben coexistir.
CmdPeriod.remove(~pdjCmdPeriodCleanup);
~shutdown.tryPerform(\value);
~dmShutdown.tryPerform(\value);

s.waitForBoot({

	// --------------------------------------------------------------------------
	// CONFIGURACIÓN
	// --------------------------------------------------------------------------
	// Asignar ~oscPort antes de cargar para usar un puerto aislado de validación/prueba.
	~oscPort     = ~oscPort ? 9001;
	~numChannels = 8;
	~masterGain  = 0.55;

	// Número de altavoces multicanal — lo fija pdj_audio_config.scd antes del boot.
	// 8 con Dante Virtual Soundcard (Sala Abierta / DANTE 5 / D3-1..D3-8).
	// 2 en reserva estéreo (dispositivo por defecto, sin DANTE en la red).
	~numSpeakers = ~numSpeakers ? 2;

	// --------------------------------------------------------------------------
	// MOTOR ESPACIAL DBAP
	// Posiciones de altavoz en coordenadas de sala (x, y), normalizadas 0-1.
	//   x: izquierda (0) -> derecha (1)
	//   y: frente (0) -> fondo (1)
	// El orden de índices coincide con el de canales de DANTE 5:
	//   0=D3-1, 1=D3-2, 2=D3-3, 3=D3-4, 4=D3-5, 5=D3-6, 6=D3-7, 7=D3-8
	// Derivado de la planta de Sala Abierta. Afinar in situ con
	// ~speakerTest tras medir las posiciones físicas.
	// --------------------------------------------------------------------------
	~speakerPositions = if(~numSpeakers == 8, {
		[
			[0.82, 0.12],  // D3-1: derecha, frente (alimenta 2 altavoces físicos)
			[0.50, 0.08],  // D3-2: centro, frente
			[0.14, 0.16],  // D3-3: izquierda, frente (alimenta 2 altavoces físicos)
			[0.04, 0.50],  // D3-4: muro izquierdo, media profundidad
			[0.10, 0.84],  // D3-5: izquierda, fondo
			[0.40, 0.58],  // D3-6: agrupación central
			[0.56, 0.74],  // D3-7: centro trasero
			[0.44, 0.54]   // D3-8: agrupación central (cerca de D3-6)
		]
	}, {
		// Reserva estéreo: dos posiciones virtuales de altavoz en L/R
		[[0.0, 0.5], [1.0, 0.5]]
	});

	// Distance-Based Amplitude Panning (DBAP).
	// srcX, srcY : posición de la fuente en coordenadas de sala (0-1).
	// rolloff    : exponente de distancia — 1.4 = foco moderado; 2.0 = nítido.
	// spread     : 0 = fuente puntual (altavoz más cercano), 1 = todos iguales.
	// Devuelve un array de ganancias de ~numSpeakers, normalizado a potencia constante.
	~dbapGains = { |srcX, srcY, rolloff = 1.4, spread = 0.0|
		var gains = ~speakerPositions.collect({ |pos|
			var dx   = pos[0] - srcX;
			var dy   = pos[1] - srcY;
			var dist = ((dx * dx) + (dy * dy)).sqrt.max(0.001);
			(dist ** rolloff.neg) + (spread * 0.5)
		});
		var rms = (gains.collect({ |g| g * g }).sum / gains.size).sqrt.max(0.001);
		gains / rms
	};

	// Rutina de prueba de altavoces — aislar cada uno en secuencia.
	// Ejecutar con ~testOsc.play o oF activo; escuchar y recorrer la sala.
	~speakerTest = {
		Routine({
			~numSpeakers.do { |spk|
				var g = Array.fill(~numSpeakers, 0.0);
				g[spk] = 0.8;
				~ch.do { |st| st.gainBus.setn(g) };
				("PDJ speaker test: " ++ spk ++ " -> DANTE D3-" ++ (spk + 1)).postln;
				2.0.wait;
			};
			// restaurar difusión centrada
			~ch.do { |st, i|
				st.gainBus.setn(~dbapGains.(
					i.linlin(0, ~numChannels - 1, 0.1, 0.9), 0.5, 1.4, 0.3
				));
			};
			"PDJ speaker test done.".postln;
		}).play;
	};

	// Deriva orgánica a corto plazo sobre la línea base del módulo — nunca
	// tan grande como para ocultar su identidad; solo lo mantiene vivo.
	~macro = (
		noise:   0.10,
		space:   0.70,
		density: 0.10
	);
	~crushBase = 0.0; // línea base de cuantización fijada por el módulo

	// La partitura de vídeo es la forma. Cada modo de GraphicScore elige una
	// escena sonora cinematográfica estable; el CV en vivo se mueve *dentro*
	// de esa escena en lugar de sustituirla. Valores deliberadamente contenidos:
	// el drone es continuo, el pulso es detalle de primer plano, air es el
	// brillo del campo de ruido.
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

	// Vocabulario de generadores de VisualComposer (orden del enum de VisualGenerator.h).
	// El generador visual elige la familia de material; su etapa temporal
	// articula ese material, y el CV solo se mueve dentro de la escena elegida.
	// legacyMode selecciona una de las voces cinematográficas del banco de
	// tonos; el resto de campos expone las máquinas data.matrix ya presentes
	// en este motor: drone, pulso, ruido, interferencia y ruptura.
	~generatorProfiles = [
		(name: \rasterPulse,     family: \pulse,    legacyMode: 3,  tension: 0.28, drone: 0.42, air: 0.15, pulse: 0.42, color: 0.75, space: 0.58, crush: 0.11),
		(name: \bitMatrix,       family: \data,     legacyMode: 3,  tension: 0.34, drone: 0.38, air: 0.20, pulse: 0.34, color: 0.82, space: 0.62, crush: 0.18),
		(name: \modularGrid,     family: \grid,     legacyMode: 5,  tension: 0.30, drone: 0.48, air: 0.18, pulse: 0.31, color: 0.65, space: 0.68, crush: 0.09),
		(name: \phaseLines,      family: \tonal,    legacyMode: 10, tension: 0.24, drone: 0.68, air: 0.26, pulse: 0.08, color: 0.48, space: 0.88, crush: 0.02),
		(name: \vectorField,     family: \machineD, legacyMode: 4,  tension: 0.30, drone: 0.72, air: 0.27, pulse: 0.14, color: 0.38, space: 0.92, crush: 0.02),
		(name: \dataLedger,      family: \data,     legacyMode: 9,  tension: 0.26, drone: 0.45, air: 0.18, pulse: 0.20, color: 0.62, space: 0.70, crush: 0.08),
		(name: \signalTrace,     family: \tonal,    legacyMode: 12, tension: 0.22, drone: 0.70, air: 0.31, pulse: 0.07, color: 0.42, space: 0.96, crush: 0.02),
		(name: \thresholdBridge, family: \machineC, legacyMode: 11, tension: 0.48, drone: 0.48, air: 0.52, pulse: 0.38, color: 0.76, space: 0.74, crush: 0.24),
		(name: \pulse,           family: \pulse,    legacyMode: 2,  tension: 0.38, drone: 0.34, air: 0.16, pulse: 0.56, color: 0.68, space: 0.58, crush: 0.10),
		(name: \barScan,         family: \bars,     legacyMode: 6,  tension: 0.46, drone: 0.35, air: 0.58, pulse: 0.16, color: 0.84, space: 0.78, crush: 0.16),
		(name: \granularRaster,  family: \bars,     legacyMode: 6,  tension: 0.52, drone: 0.42, air: 0.50, pulse: 0.28, color: 0.88, space: 0.72, crush: 0.22),
		(name: \orbitalRings,    family: \tonal,    legacyMode: 9,  tension: 0.28, drone: 0.52, air: 0.16, pulse: 0.24, color: 0.72, space: 0.86, crush: 0.06),
		(name: \strobe,          family: \rupture,  legacyMode: 13, tension: 0.82, drone: 0.14, air: 0.08, pulse: 0.62, color: 0.96, space: 0.42, crush: 0.48),
		(name: \dividedStrobe,   family: \rupture,  legacyMode: 13, tension: 0.70, drone: 0.22, air: 0.12, pulse: 0.58, color: 0.92, space: 0.58, crush: 0.38),
		(name: \analogNoise,     family: \noise,    legacyMode: 11, tension: 0.76, drone: 0.28, air: 0.90, pulse: 0.05, color: 0.90, space: 0.92, crush: 0.34)
	];
	~breathProfile = (name: \negativeSpace, family: \breath, legacyMode: 0,
		tension: 0.03, drone: 0.30, air: 0.06, pulse: 0.0,
		color: 0.10, space: 0.98, crush: 0.0);

	// Instrumentación estable ligada a los ocho valores de ScreenRole.
	~roleDroneGain = [1.30, 0.72, 0.52, 0.88, 0.58, 1.35, 1.04, 0.50];
	~rolePulseGain = [0.42, 0.34, 1.35, 0.72, 0.56, 0.28, 0.62, 0.92];
	~roleAirGain   = [0.42, 0.86, 0.38, 1.18, 1.32, 1.16, 0.82, 0.48];
	~visualTransport = (
		bpm: 90.0, beatPhase: 0.0, beatIndex: -1,
		subdivisionIndex: -1, absoluteSubdivision: -1,
		lastPacketAt: 0.0
	);
	~scoreMoment = -1;
	~scoreTakeover = 0;
	~collectiveNames = [
		\suspension, \codification, \accumulation, \propagation,
		\convergence, \fragmentation, \saturation, \rupture, \residue
	];
	~collective = (
		movement: -1, revision: -1, phase: 0.0,
		activity: 0.0, coherence: 0.0, diversity: 0.0,
		convergence: 0.0, population: 0.0, tension: 0.0,
		dominantGenerator: -1
	);
	~collectiveTargets = (noise: 0.13, density: 0.20, space: 0.58, crush: 0.05);
	~pdjEngineActive = false;

	// Ayudante puro del grafo UGen (sin acceso a buses): combina redondeo de
	// profundidad de bits con reducción de cadencia sample-and-hold: la firma
	// audible de los límites de cuantización digital (sección 11 del análisis).
	// Solo UGens de núcleo (round + Latch); no hace falta una clase bitcrusher.
	~crushSignal = { |sig, amt|
		var bits    = amt.linlin(0, 1, 24, 3);
		var levels  = 2.0.pow(bits);
		var holdHz  = amt.linexp(0.001, 1, 44100, 1200);
		var quantized = (sig * levels).round(1) / levels;
		Latch.ar(quantized, Impulse.ar(holdHz));
	};

	// Ayudante del lado del lenguaje — sonificación binaria literal: lectura
	// de 8 bits de un valor detectado real, sin metáfora.
	~binaryBits = { |n, digits = 8|
		Array.fill(digits, { |i| (n >> (digits - 1 - i)) & 1 });
	};

	// ScoreMode → peso del corte de montaje (coincide con el enum de GraphicScore.h).
	// Solo escala el estallido transitorio de glitch al cambiar score/modo:
	// es una discontinuidad real de la señal visual, así que merece un corte
	// audible, no una respuesta melódica.
	// 0 BwClean … 13 Flash
	~modeGlitch = [
		0.05, 0.35, 0.25, 0.55, 0.40, 0.50, 0.70, 0.10,
		0.30, 0.60, 0.45, 0.55, 0.65, 1.00
	];

	// --------------------------------------------------------------------------
	// FORMA — máquina de estados por módulos según el modelo formal del
	// análisis (sección 17): Calibration -> Codification -> Accumulation ->
	// DimensionalExpansion -> Saturation -> Rupture -> Recursion. Cada módulo
	// fija su registro de cadencia de pulso, su «dimensión» (cuántas capas
	// están activas a la vez) y si la rejilla queda anclada a los eventos OSC
	// entrantes (microsincronía) o corre de forma independiente (contrapunto).
	// --------------------------------------------------------------------------
	~form = (
		name:       \calibration,
		dimension:  1,       // 1..4 capas activas: tono / pulso+rejilla / campo de ruido / espacio
		rateLo:     0.3,     // Hz — cota inferior del registro de clic/pulso de este módulo
		rateHi:     3,       // Hz — cota superior (barrido clic->ritmo->altura->ruido)
		lockToOSC:  true     // true: la rejilla dispara con eventos OSC; false: reloj propio
	);
	~gridStep = 0;

	// --------------------------------------------------------------------------
	// ESTADO (CV suavizado que refleja el flujo de oF)
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
			generatorActive: 0,
			composerContent: -1,
			generatorMode: -1,
			generatorRevision: 0,
			generatorAppliedRevision: -1,
			generatorOrganization: 0,
			generatorRole: 0,
			generatorStage: 0,
			generatorPreviousStage: -1,
			generatorStageProgress: 0.0,
			generatorChapterPhase: 0.0,
			generatorBeatPhase: 0.0,
			generatorBeatIndex: -1,
			generatorSubdivisionIndex: -1,
			generatorSeed: 1,
			generatorResolvedSeed: 1,
			generatorEnvelope: 0.0,
			generatorIntensity: 0.0,
			generatorDensity: 0.0,
			generatorRolePhase: 0.0,
			generatorPropagationDelay: 0.0,
			generatorObservedGroup: 1,
			transitionActive: 0,
			programEnabled: 0,
			installationMoment: 0,
			groupVideoOccupancy: 0,
			globalTakeover: 0,
			prevColl:  0,
			prevBall:  0,
			lastGlitch: 0.0,
			lastImpact: 0.0,
			lastBall:   0.0,
			lastKick:   0.0,
			scene:      ~modeProfiles[0],
		ampBus:    Bus.control(s, 1),
		gainBus:   Bus.control(s, ~numSpeakers),
		rateBus:   Bus.control(s, 1),
		noiseBus:  Bus.control(s, 1),
		tensionBus: Bus.control(s, 1)
		)
	});

	// Buses de control globales — gobernados por módulo/meta-realimentación;
	// los lectores a tasa de audio aplican .lag() para que los cambios
	// deslicen en una fracción de segundo (rápido como corte a escala de
	// módulo, lento como para evitar clics).
	~gNoiseBus   = Bus.control(s, 1);
	~gDensityBus = Bus.control(s, 1);
	~gSpaceBus   = Bus.control(s, 1);
	~gCrushBus   = Bus.control(s, 1); // cantidad de cuantización (técnica de umbral)
	~gGate1Bus   = Bus.control(s, 1); // capa de dimensión 1: banco de tonos
	~gGate2Bus   = Bus.control(s, 1); // capa de dimensión 2: motor de pulso
	~gGate3Bus   = Bus.control(s, 1); // capa de dimensión 3: campo de ruido
	~selfBus     = Bus.control(s, 1); // meta-datamatics: nivel de salida master realimentado
	~gSpectralBus = Bus.control(s, 1); // frecuencia central compartida en deriva — coherencia atmosférica
	~gMotionBus  = Bus.control(s, 1); // movimiento integrado, nunca CV crudo a cadencia de fotograma
	~gCrowdBus   = Bus.control(s, 1); // masa de público integrada
	~gTensionBus = Bus.control(s, 1); // presión cinematográfica global, subida rápida / caída lenta

	~ch.do { |st, i|
		st.ampBus.set(0);
		st.gainBus.setn(~dbapGains.(
			i.linlin(0, ~numChannels - 1, 0.1, 0.9), 0.5, 1.4, 0.3
		));
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
		scriptDir +/+ "pdj_mode_voices.scd"
	);
	~installModeVoices.value;

	// --------------------------------------------------------------------------
	// SYNTHDEFS — solo unidades binarias elementales: senos, ruido, impulsos,
	// silencio. Sin voz afinada/melódica.
	// --------------------------------------------------------------------------

	// Cuatro familias de drone emparentadas, elegidas por la escena visual y
	// fundidas despacio. La variante de canal cambia razones y tasas de deriva
	// sin romper el lenguaje espectral común:
	//   0 abismo/película, 1 barrido/vigilancia, 2 dato/vidrio, 3 temporal/bruma.
	SynthDef(\pdjToneBank, { |out = 0, ampBus, gainBus, gateBus, motionBus, crowdBus,
		tensionBus, subFreq = 36, highFreq = 13500, sceneAmp = 0.7, color = 0.2,
		voice = 0, variant = 0, master = 0.22|
		var activity = In.kr(ampBus, 1).lag(4);
		var gains    = In.kr(gainBus, ~numSpeakers).lag(0.8).max(0);
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
		Out.ar(out, (sig ! ~numSpeakers) * gains);
	}).add;

	// El continuo clic -> ritmo -> altura -> ruido, en un solo generador.
	// Un único tren de Impulse cuya cadencia la gobierna el CV en vivo:
	// lenta se lee como clics discretos, media como ritmo, rápida fusiona
	// en altura/timbre, muy rápida se lee como ruido.
	// Ringz a la misma cadencia deja que la altura emerja de la repetición.
	// La resonancia pesa mucho más que el impulso crudo y tiene un ring más
	// largo, así el generador se oye como tono/textura sostenida y en
	// evolución, no como tren de clics secos: elegante, no percusivo.
	SynthDef(\pdjPulseEngine, { |out = 0, rateBus, ampBus, gainBus, gateBus, crushBus,
		tensionBus, sceneAmp = 0, color = 0.5, variant = 0, master = 0.12|
		var rate  = In.kr(rateBus, 1).lag(2).clip(0.15, 60);
		var amp   = In.kr(ampBus, 1).lag(3);
		var gains = In.kr(gainBus, ~numSpeakers).lag(0.8).max(0);
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
		Out.ar(out, (sig ! ~numSpeakers) * gains);
	}).add;

	// Clic agudo disperso / impulso de dato (colisión / balón / lecturas de rejilla).
	// panEnd != panStart -> el clic barre el campo estéreo;
	// dirección/velocidad salen de la velocidad del blob detectado. Decaimiento
	// un poco más largo y componente de ruido más suave: un signo de
	// puntuación sobre el lecho de drone, no un pico por encima.
	SynthDef(\pdjClick, { |out = 0, freq = 8000, amp = 0.4, panStart = 0, panEnd = 0, decay = 0.016|
		var env = EnvGen.ar(Env.perc(0.0006, decay, 1, -6), doneAction: 2);
		var pos = Line.kr(panStart, panEnd, decay * 4).linlin(-1, 1, 0, 2);
		var sig = Impulse.ar(0) * env;
		sig = Ringz.ar(sig, freq, decay * 0.9) * 0.4
			+ (WhiteNoise.ar * env * 0.08);
		sig = HPF.ar(sig, 1800);
		Out.ar(out, PanAz.ar(~numSpeakers, sig * amp, pos, 1, 0.5));
	}).add;

	// Estallido corto de glitch digital — cortes de montaje (cambios de modo,
	// ráfagas de blobs, el estallido de ruido del módulo Rupture). Sin
	// vínculo a ninguna altura/escala.
	SynthDef(\pdjGlitch, { |out = 0, amp = 0.25, panStart = 0, panEnd = 0, rate = 40, dur = 0.06|
		var env = EnvGen.ar(Env.linen(0.004, dur, 0.02), doneAction: 2);
		var pos = Line.kr(panStart, panEnd, dur).linlin(-1, 1, 0, 2);
		var clk = Impulse.ar(rate);
		var sig = Latch.ar(WhiteNoise.ar, clk);
		sig = RHPF.ar(sig, TExpRand.ar(1500, 9000, clk), 0.35);
		sig = sig * env * amp;
		Out.ar(out, PanAz.ar(~numSpeakers, sig, pos, 1, 0.5));
	}).add;

	// Primer plano de eventos significativos: la colisión es una onda de
	// presión cinematográfica grave; la detección del balón, un rastro
	// espacial breve. Ninguno es un glitch genérico de banda ancha; ambos
	// conservan el campo ambiental.
	SynthDef(\pdjImpact, { |out = 0, amp = 0.3, pan = 0, depth = 0.5|
		var env  = EnvGen.ar(Env.perc(0.008, 2.8, 1, -5), doneAction: 2);
		var pos  = pan.linlin(-1, 1, 0, 2);
		var freq = XLine.kr(62 + (depth * 24), 28, 1.8);
		var low  = SinOsc.ar(freq, 0, 0.55) + (BPF.ar(BrownNoise.ar, 90, 0.25) * 0.25);
		var edge = BPF.ar(PinkNoise.ar, 900 + (depth * 1800), 0.18) * 0.08;
		Out.ar(out, PanAz.ar(~numSpeakers, (low + edge) * env * amp, pos, 1, 0.5));
	}).add;

	SynthDef(\pdjTrace, { |out = 0, amp = 0.16, panStart = 0, panEnd = 0, height = 0.5|
		var env = EnvGen.ar(Env.perc(0.01, 1.4, 1, -6), doneAction: 2);
		var pos = Line.kr(panStart, panEnd, 1.2).linlin(-1, 1, 0, 2);
		var cf  = height.linexp(0, 1, 650, 5200);
		var sig = BPF.ar(PinkNoise.ar, cf * XLine.kr(1.2, 0.72, 1.2), 0.07);
		Out.ar(out, PanAz.ar(~numSpeakers, sig * env * amp, pos, 1, 0.6));
	}).add;

	// Servicio de clear de GlobalDirector: un levantamiento espectral difuso
	// marca el wipe mientras clearAlpha retira masa de los drones persistentes.
	SynthDef(\pdjClearSweep, { |out = 0, amp = 0.10, dur = 2.2|
		var env    = EnvGen.ar(Env.linen(0.25, dur, 1.4, 1, \sine), doneAction: 2);
		var center = XLine.kr(260, 6200, dur + 1.65);
		var sig    = BPF.ar(PinkNoise.ar, center, 0.09)
			+ (BPF.ar(BrownNoise.ar, center * 0.22, 0.12) * 0.45);
		// Difusión omnidireccional con retardos cortos de decorrelación por altavoz
		var delays = Array.fill(~numSpeakers, { |i| 0.017 + (i * 0.005) });
		sig = Array.fill(~numSpeakers, { |i| DelayC.ar(sig, 0.08, delays[i]) });
		Out.ar(out, sig * env * amp);
	}).add;

	// Barra de ruido de la partitura visual: BarScan y AnalogNoise la usan
	// en los límites de etapa. Direction > 0 sube por el espectro;
	// direction <= 0 cae hacia el registro de residuo.
	SynthDef(\pdjScoreSweep, { |out = 0, amp = 0.10, dur = 2.4,
		direction = 1, panStart = -0.8, panEnd = 0.8, color = 0.6|
		var rising  = direction > 0;
		var start   = Select.kr(rising, [7200, 180]);
		var finish  = Select.kr(rising, [180, 7200]);
		var center  = XLine.kr(start, finish, dur.max(0.05));
		var env     = EnvGen.ar(Env.linen(0.03, dur.max(0.05), 0.45, 1, \sine), doneAction: 2);
		var pos     = Line.kr(panStart, panEnd, dur.max(0.05)).linlin(-1, 1, 0, 2);
		var source  = PinkNoise.ar + (GrayNoise.ar * color.clip(0, 1) * 0.38);
		var sig     = BPF.ar(source, center, 0.035 + (color * 0.08));
		sig = sig + (HPF.ar(source, center.clip(900, 9000)) * 0.045);
		Out.ar(out, PanAz.ar(~numSpeakers, sig * env * amp, pos, 1, 0.6));
	}).add;

	// Centro espectral lento compartido — una sola frecuencia errante que
	// orbitan el wash y el lecho de ruido, para que las capas ambientales
	// se oigan como un cuerpo coherente y no como fuentes de ruido
	// afinadas por separado. Es el dispositivo principal de coherencia formal.
	SynthDef(\pdjSpectralAnchor, { |out, target = 480|
		var anchor = target.lag(8) * LFNoise2.kr(0.018).range(0.975, 1.025);
		Out.kr(out, anchor);
	}).add;

	// Grano de ruido impulsado por el movimiento (siseo de scanline / contorno)
	// — ruido de banda ancha elemental, enmascarado por la capa 3 de dimensión
	// y cuantizado por crush. El centro del filtro sigue el ancla espectral
	// compartida (véase pdjSpectralAnchor) en lugar de vagar por su cuenta,
	// así forma parte del mismo cuerpo sonoro que el wash y no un siseo aparte.
	SynthDef(\pdjNoiseBed, { |out = 0, noiseBus, gainBus, gateBus, crushBus, spectralBus,
		motionBus, tensionBus, sceneAmp = 0.2, color = 0.3, voice = 0, variant = 0, master = 0.12|
		var amp     = In.kr(noiseBus, 1).lag(0.15);
		var gains   = In.kr(gainBus, ~numSpeakers).lag(0.8).max(0);
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
		sig = sig * (0.12 + amp.sqrt) * (0.7 + (tension * 0.3)) * sceneAmp.lag(5) * gate * master;
		Out.ar(out, (sig ! ~numSpeakers) * gains);
	}).add;

	// Densidad de público → presión de seno grave — unidad elemental de subgraves,
	// independiente del sub fijo del banco de tonos (esta sigue al público, no
	// a la actividad); sigue en registro fijo, no melódico.
	SynthDef(\pdjSub, { |out = 0, amp = 0, freq = 55, gainBus|
		var a     = amp.lag(6);
		var gains = In.kr(gainBus, ~numSpeakers).lag(2.0).max(0);
		var sig   = (SinOsc.ar(freq) + (SinOsc.ar(freq * 1.005) * 0.45)) * a * 0.22;
		sig = LPF.ar(sig, 120);
		Out.ar(out, (sig ! ~numSpeakers) * gains);
	}).add;

	// Wash textural en evolución continua — portadora principal de drone/ambiente
	// de la pieza (capa de dimensión 3). Su frecuencia central vaga alrededor
	// del ancla espectral compartida (ancho de banda más estrecho y resonante)
	// para que «cante» en lugar de sisear: un lecho elegante y coherente
	// sobre el que se asientan el lecho de ruido y los motores de pulso.
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
		// Slow wander across speakers via PanAz with wandering position
		var wanderPos = LFNoise1.kr(0.035 + (tension * 0.025)).range(0, 2);
		var sig    = BPF.ar(PinkNoise.ar, cf, bw);
		sig = sig + (BPF.ar(BrownNoise.ar, cf * 0.24, 0.09) * (0.45 + (crowd * 0.35)));
		sig = sig + (BPF.ar(GrayNoise.ar, cf * (3.2 + tension), 0.035 + (tension * 0.04)) * motion * (0.08 + (tension * 0.10)));
		sig = ~crushSignal.(sig, crush * 0.25);
		sig = sig * (0.15 + amt) * (0.72 + (tension * 0.28)) * gate * master;
		Out.ar(out, PanAz.ar(~numSpeakers, sig, wanderPos, 1, 0.8));
	}).add;

	// Crackle / polvo de dato — la densidad sigue el ruido macro; cada grano
	// recibe una posición espacial aleatoria; parte del campo de ruido global (capa 3).
	SynthDef(\pdjCrackle, { |out = 0, noiseBus, gateBus, density = 0.1, color = 0.5, master = 0.08|
		var amt  = In.kr(noiseBus, 1).lag(1.5);
		var gate = In.kr(gateBus, 1).lag(4);
		var dens = density.lag(5).linexp(0.001, 1, 0.08, 6) * (0.2 + (amt * 0.6));
		var trig    = Dust.ar(dens);
		var env     = Decay2.ar(trig, 0.002, 0.018 + (color * 0.05));
		// Random speaker selection: TRand maps to PanAz pos range [0, 2)
		var randPos = TRand.ar(0, 2, trig);
		var cf      = color.linexp(0, 1, 800, 9000);
		var sig     = BPF.ar(PinkNoise.ar, cf, 0.08) * env * master;
		// Very narrow width (0.05) = nearly point-source per grain
		Out.ar(out, PanAz.ar(~numSpeakers, sig * gate, randPos, 1, 0.05));
	}).add;

	// Envío espacial: delay ping-pong suave que alimenta una reverb algorítmica
	// larga; la humedad la marcan macro space y los envíos de profundidad de
	// cada evento (véase ~trigClick). Es la mitad «ambiental» de la estética
	// drone/ambiente — una cola difusa real, no solo una repetición de delay —
	// y el análogo sonoro de la profundidad dimensional (capa 4).
	SynthDef(\pdjDelayFX, { |inBus, out = 0, spaceBus, mix = 0.25|
		var wet     = In.kr(spaceBus, 1).lag(3);
		// Mix down all input channels to a mono signal for reverb input
		var sig     = Mix.ar(In.ar(inBus, ~numSpeakers));
		var dly     = DelayC.ar(sig, 0.12, [0.071, 0.103]);
		var verb    = GVerb.ar(
			Mix.ar(dly), 50, 5, 0.5, 0.7, 15, 0,
			wet.linlin(0, 1, 0.34, 0.72),
			wet.linlin(0, 1, 0.15, 0.55), 300
		);
		// Diffuse reverb tail across all speakers with slow per-speaker wander
		var diffuse = (verb[0] + verb[1]) * 0.5;
		diffuse     = HPF.ar(LPF.ar(diffuse, 6200), 45);
		var wander  = LFNoise1.kr(
			Array.fill(~numSpeakers, { |i| 0.022 + (i * 0.004) })
		).range(0.4, 1.0);
		Out.ar(out, (diffuse ! ~numSpeakers) * wander * wet * mix);
	}).add;

	// Meta-datamatics: lee la amplitud de la mezcla master y la escribe en
	// un bus de control para que el sistema realimente su propia salida en
	// los parámetros generativos (véase ~selfFeedbackRoutine más abajo).
	SynthDef(\pdjSelfAnalysis, { |inBus, out|
		var sig = Mix.ar(In.ar(inBus, ~numSpeakers));
		var amp = Amplitude.ar(sig, 0.01, 0.35);
		Out.kr(out, A2K.kr(amp));
	}).add;

	// Bus master: mezcla seca + envío espacial, clip suave + limiter
	SynthDef(\pdjMaster, { |inBus, delayBus, out = 0, gain = 0.55|
		var dry = In.ar(inBus,    ~numSpeakers);
		var wet = In.ar(delayBus, ~numSpeakers);
		var sig = (dry + wet) * gain;
		sig = (sig * 1.2).tanh * 0.9;
		sig = Limiter.ar(sig, 0.95, 0.01);
		Out.ar(out, sig);
	}).add;

	s.sync;

	// --------------------------------------------------------------------------
	// GRAFO DE AUDIO
	// --------------------------------------------------------------------------
	~fxBus    = Bus.audio(s, ~numSpeakers);
	~delayBus = Bus.audio(s, ~numSpeakers);
	~master = Synth(\pdjMaster, [\inBus, ~fxBus, \delayBus, ~delayBus, \gain, ~masterGain]);
	~channelBases = [31.7, 33.1, 35.9, 38.3, 41.1, 43.7, 47.3, 50.9];

	~toneBanks = Array.fill(~numChannels, { |i|
		Synth(~modeSynthNames[0], [
			\out, ~fxBus,
			\ampBus, ~ch[i].ampBus,
			\gainBus, ~ch[i].gainBus,
			\tensionBus, ~ch[i].tensionBus,
			\spectralBus, ~gSpectralBus,
			\sceneAmp, ~ch[i].scene.drone,
			\color, ~ch[i].scene.color,
			\variant, i % 4,
			\evolution, ~ch[i].videoPos,
			\master, 0.24
		], addAction: \addToHead);
	});

	~spawnModeVoice = { |ch, mode, variantOverride|
		var old = ~toneBanks[ch];
		var profile = ~ch[ch].scene;
		var variant = if(variantOverride.isNil, {
			((~ch[ch].videoRevision.max(0) + ~ch[ch].modeRevision + ch) % 4)
		}, {
			variantOverride.asInteger % 4
		});
		if(old.notNil, { old.set(\lifeGate, 0) });
		~toneBanks[ch] = Synth(~modeSynthNames.clipAt(mode.clip(0, 13)), [
			\out, ~fxBus,
			\ampBus, ~ch[ch].ampBus,
			\gainBus, ~ch[ch].gainBus,
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
			\gainBus, ~ch[i].gainBus,
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
			\gainBus, ~ch[i].gainBus,
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
			\gainBus, ~ch[i].gainBus
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

	// Los paquetes compactos /blob/{slot}/state se reducen a geometría estable
	// por canal. Así se evita componer desde el slot 0 del contour-finder,
	// cuya identidad puede cambiar entre fotogramas.
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
	// CONDUCTOR DE DATOS — la telemetría a cadencia de fotograma se integra
	// en control musical lento. Sin paseo aleatorio autónomo ni modulación
	// por realimentación de salida: todo cambio grande tiene ahora una causa visible.
	// --------------------------------------------------------------------------
	~dataConductor = Routine({
		1.wait; // dar tiempo a que terminen de instalarse las funciones de mapeo
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
			var avgEvolution = (~ch.collect({ |st|
				if(st.composerContent >= 0, { st.generatorChapterPhase }, { st.videoPos })
			}).sum / ~numChannels).clip(0, 1);
			var clipArc = (~ch.collect({ |st|
				var phase = if(st.composerContent >= 0, {
					st.generatorChapterPhase
				}, {
					st.videoPos
				});
				(phase.clip(0, 1) * pi).sin
			}).sum / ~numChannels);
			var tension = (
				sceneTension + (energy * 0.28) + (flow * 0.16) + (crowd * 0.16)
				+ (complexity * 0.10) + (population * 0.08) + (body * 0.04)
				+ (geometry * 0.10) + (trackedSpeed * 0.12) + (fastBias * 0.22)
				+ (slowBias * 0.06) + (clear * 0.24) + (clipArc * 0.05)
			).clip(0, 1);
			var avgY = (~ch.collect({ |st| st[\centroidY] }).sum / ~numChannels).clip(0, 1);
			var spectralTarget = avgY.linexp(0, 1, 1900, 180)
				* color.linexp(0, 1, 0.72, 1.8) * tension.linexp(0, 1, 0.9, 1.35);
			var collectiveActivity = ~collective.activity.clip(0, 1);
			var collectiveTension = ~collective.tension.clip(0, 1);
			var targetNoise = ~collectiveTargets.noise.clip(0, 1);
			var targetDensity = ~collectiveTargets.density.clip(0, 1);
			var targetSpace = ~collectiveTargets.space.clip(0, 1);
			var targetCrush = ~collectiveTargets.crush.clip(0, 1);

			~macro.noise = (
				(air * (0.38 + (motion * 0.62)) * (0.55 + (avgEvolution * 0.70)) * 0.62)
				+ (targetNoise * 0.38) + (collectiveActivity * 0.08)
			).clip(0.005, 0.90);
			~macro.space = ((space * 0.62) + (targetSpace * 0.38)).clip(0.35, 1);
			~macro.density = (
				(((motion * color * 0.72) + (tension * 0.16)) * 0.58)
				+ (targetDensity * 0.34) + (collectiveActivity * 0.08)
			).clip(0, 0.92);
			~crushBase = ((crush * 0.55) + (targetCrush * 0.45)).clip(0, 1);
			tension = (tension + (collectiveTension * 0.24)).clip(0, 1);

			~gMotionBus.set(motion);
			~gCrowdBus.set(crowd);
			~gTensionBus.set(tension);
			~gNoiseBus.set(~macro.noise);
			~gDensityBus.set(~macro.density);
			~gSpaceBus.set(~macro.space);
			~gCrushBus.set(~crushBase);
			~spectralAnchor.set(\target, spectralTarget.clip(90, 4200));
			// La elongación del cuerpo es un cue textural secundario y silencioso:
			// las figuras de pie, extendidas en vertical, revelan un poco más de microsound.
			~crackle.set(\density, ((~macro.density * 0.78) + (body * 0.16)).clip(0, 0.7), \color, color);

			~numChannels.do { |ch| ~applyContinuous.(ch) };
			0.25.wait;
		}
	}).play;

	// --------------------------------------------------------------------------
	// PUERTAS DE DIMENSIÓN — cuántas capas están activas a la vez. Es el
	// análogo sonoro de la expansión visual 2D -> 3D -> 4D descrita en el
	// análisis (sección 9): capa plana/única -> capas añadidas ->
	// simultaneidad plena.
	// --------------------------------------------------------------------------
	~setDimension = { |n|
		~form.dimension = n;
		~gGate1Bus.set(if(n >= 1, 1, 0));
		~gGate2Bus.set(if(n >= 2, 1, 0));
		~gGate3Bus.set(if(n >= 3, 1, 0));
	};

	// --------------------------------------------------------------------------
	// MÁQUINA DE ESTADOS FORMAL — montaje por módulos (secciones 8, 17). Cada
	// módulo fija su registro de cadencia de pulso, dimensión, régimen de
	// sincronía y línea base de macro/crush, y se sostiene decenas de segundos
	// antes de un corte abrupto al siguiente módulo.
	// --------------------------------------------------------------------------
	~applyModule = { |name|
		~form.name = name;
		case
		{ name == \calibration } {
			// Señales dispersas establecen el vocabulario: solo tonos fijos,
			// clics individuales lentos si algo se cuela. El módulo más
			// elegante y de ruido más seco — sobre todo drone + cola de reverb.
			~form.rateLo = 0.3; ~form.rateHi = 3; ~form.lockToOSC = true;
			~macro.noise = 0.06; ~macro.space = 0.55; ~macro.density = 0.08; ~crushBase = 0.0;
			~setDimension.(1);
		}
		{ name == \codification } {
			// Rejillas y pulsos implican un sistema legible — la rejilla queda
			// anclada a eventos OSC reales (microsincronía). Sigue siendo un
			// módulo de ambiente/drone, solo que con la capa de pulso
			// añadida sobre el banco de tonos.
			~form.rateLo = 1; ~form.rateHi = 14; ~form.lockToOSC = true;
			~macro.noise = 0.13; ~macro.space = 0.58; ~macro.density = 0.20; ~crushBase = 0.05;
			~setDimension.(2);
		}
		{ name == \accumulation } {
			// Aumentan las capas y la densidad; el reloj corre ahora por su
			// cuenta — separación contrapuntística deliberada respecto a lo visual.
			~form.rateLo = 3; ~form.rateHi = 500; ~form.lockToOSC = false;
			~macro.noise = 0.35; ~macro.space = 0.60; ~macro.density = 0.45; ~crushBase = 0.20;
			~setDimension.(3);
		}
		{ name == \dimensionalExpansion } {
			// Los patrones planos se vuelven campos espaciales: la dimensión
			// rampa 1->4 de forma progresiva a lo largo de la duración del módulo.
			~form.rateLo = 3; ~form.rateHi = 4000; ~form.lockToOSC = false;
			~macro.noise = 0.45; ~macro.space = 0.75; ~macro.density = 0.55; ~crushBase = 0.30;
			~setDimension.(1);
			Routine({
				[2, 3, 4].do { |n| rrand(4.0, 7.0).wait; ~setDimension.(n) };
			}).play;
		}
		{ name == \saturation } {
			// La información supera la capacidad perceptiva: registro pleno,
			// dimensión plena, cuantización intensa.
			~form.rateLo = 80; ~form.rateHi = 15000; ~form.lockToOSC = false;
			~macro.noise = 0.70; ~macro.space = 0.90; ~macro.density = 0.85; ~crushBase = 0.50;
			~setDimension.(4);
		}
		{ name == \rupture } {
			// Un corte seco: todo se silencia al instante y después un
			// estallido de banda ancha — «load, replace, switch, interrupt».
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
			// Vuelve material anterior (Calibration/Codification), pero
			// pasado por cuantización — deconstruido, no repetido
			// (meta-datamatics, sección 13) — con espacio generoso para
			// oírse como memoria lejana y borrosa, no como rehacer agresivo.
			~form.rateLo = 0.3; ~form.rateHi = 10; ~form.lockToOSC = true;
			~macro.noise = 0.18; ~macro.space = 0.68; ~macro.density = 0.16; ~crushBase = 0.45;
			~setDimension.(2);
		};

		~gNoiseBus.set(~macro.noise);
		~gDensityBus.set(~macro.density);
		~gSpaceBus.set(~macro.space);

		("PDJ module -> " ++ name ++ "   dim=" ++ ~form.dimension ++ "   lockToOSC=" ++ ~form.lockToOSC).postln;
	};

	// El analizador visual/de datos elige estos movimientos de forma no lineal.
	// Reutilizan el ADN formal de la pista analizada pero los controla la imagen:
	// la convergencia estable ancla el ritmo, la divergencia lo fragmenta, la
	// saturación llena el espectro, la ruptura lo corta y el residuo conserva
	// una memoria transformada.
	~applyCollectiveMovement = { |movement|
		var previous = ~collective.movement;
		var safe = movement.asInteger.clip(0, ~collectiveNames.size - 1);
		var name = ~collectiveNames[safe];
		~collective.movement = safe;
		case
		{ name == \suspension } {
			~form.name = \collectiveSuspension;
			~form.rateLo = 0.18; ~form.rateHi = 2.2; ~form.lockToOSC = true;
			~collectiveTargets = (noise: 0.025, density: 0.025, space: 0.94, crush: 0.0);
			~setDimension.(1);
		}
		{ name == \codification } {
			~form.name = \collectiveCodification;
			~form.rateLo = 1.0; ~form.rateHi = 16; ~form.lockToOSC = true;
			~collectiveTargets = (noise: 0.11, density: 0.22, space: 0.60, crush: 0.07);
			~setDimension.(2);
		}
		{ name == \accumulation } {
			~form.name = \collectiveAccumulation;
			~form.rateLo = 2.5; ~form.rateHi = 520; ~form.lockToOSC = false;
			~collectiveTargets = (noise: 0.34, density: 0.48, space: 0.66, crush: 0.20);
			~setDimension.(3);
		}
		{ name == \propagation } {
			~form.name = \collectivePropagation;
			~form.rateLo = 1.5; ~form.rateHi = 2400; ~form.lockToOSC = false;
			~collectiveTargets = (noise: 0.27, density: 0.34, space: 0.96, crush: 0.14);
			~setDimension.(3);
			~trigScoreSweep.(0, 1, 0.055, 3.8);
		}
		{ name == \convergence } {
			~form.name = \collectiveConvergence;
			~form.rateLo = 2.0; ~form.rateHi = 42; ~form.lockToOSC = true;
			~collectiveTargets = (noise: 0.17, density: 0.68, space: 0.48, crush: 0.10);
			~setDimension.(3);
			Synth(\pdjClearSweep, [\out, ~delayBus, \amp, 0.055, \dur, 2.2]);
			~trigKick.(0, 0.72);
		}
		{ name == \fragmentation } {
			~form.name = \collectiveFragmentation;
			~form.rateLo = 6; ~form.rateHi = 4800; ~form.lockToOSC = false;
			~collectiveTargets = (noise: 0.42, density: 0.62, space: 0.74, crush: 0.38);
			~setDimension.(3);
		}
		{ name == \saturation } {
			~form.name = \collectiveSaturation;
			~form.rateLo = 70; ~form.rateHi = 15000; ~form.lockToOSC = false;
			~collectiveTargets = (noise: 0.78, density: 0.88, space: 0.90, crush: 0.58);
			~setDimension.(4);
			~trigScoreSweep.(0, 1, 0.09, 4.2);
		}
		{ name == \rupture } {
			~form.name = \collectiveRupture;
			~form.rateLo = 120; ~form.rateHi = 15000; ~form.lockToOSC = true;
			~collectiveTargets = (noise: 0.01, density: 0.02, space: 0.32, crush: 1.0);
			~setDimension.(0);
			~gCrushBus.set(1);
			Routine({
				0.16.wait;
				~trigGlitch.((~collective.revision.abs % ~numChannels), 1.0, true);
			}).play;
		}
		{ name == \residue } {
			~form.name = \collectiveResidue;
			~form.rateLo = 0.22; ~form.rateHi = 7; ~form.lockToOSC = true;
			~collectiveTargets = (noise: 0.10, density: 0.08, space: 0.98, crush: 0.32);
			~setDimension.(2);
			if([6, 7].includes(previous), {
				~trigScoreSweep.(0, -1, 0.045, 5.5);
			});
		};
		~gNoiseBus.set(~collectiveTargets.noise);
		~gDensityBus.set(~collectiveTargets.density);
		~gSpaceBus.set(~collectiveTargets.space);
		~gCrushBus.set(~collectiveTargets.crush);
		("PDJ collective movement -> " ++ name
			++ "   activity=" ++ ~collective.activity.round(0.01)
			++ " convergence=" ++ ~collective.convergence.round(0.01)).postln;
	};

	~formOrder = [\calibration, \codification, \accumulation, \dimensionalExpansion, \saturation, \rupture, \recursion];
	~formDurs  = (
		calibration: 18, codification: 22, accumulation: 26,
		dimensionalExpansion: 30, saturation: 20, rupture: 8, recursion: 24
	);

	// Se conserva como vocabulario de investigación manual vía ~goto, pero
	// nunca se programa: una forma autónoma se desfasa de las cuatro partituras de vídeo.
	~formRoutine = nil;

	// --------------------------------------------------------------------------
	// AYUDANTES DE MAPEO
	// --------------------------------------------------------------------------
	// Velocidad -> destino de panorámica. Un blob que se mueve a la derecha/abajo
	// durante un disparo hace que el sonido barre en esa dirección;
	// es el vínculo directo «el espacio sigue a la detección visual».
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

	// ----------------------------------------------------------------------
	// VisualComposer -> PUENTE DE PARTITURA DE AUDIO
	// ----------------------------------------------------------------------
	~isGeneratorData = { |st|
		[\data, \grid].includes(st.scene.family)
	};

	~trigScoreSweep = { |ch, direction = 1, amp = 0.08, dur = 2.4|
		var st = ~ch[ch];
		var start = ch.linlin(0, ~numChannels - 1, -0.82, 0.82);
		var finish = (start.neg + ((st.flowAng / pi).clip(-1, 1) * 0.18)).clip(-1, 1);
		Synth(\pdjScoreSweep, [
			\out, ~delayBus,
			\amp, amp,
			\dur, dur,
			\direction, direction,
			\panStart, start,
			\panEnd, finish,
			\color, st.scene.color
		]);
	};

	~applyGeneratorMode = { |ch, mode|
		var st = ~ch[ch];
		var safeMode = mode.asInteger.clip(0, ~generatorProfiles.size - 1);
		var profile = ~generatorProfiles[safeMode];
		var variant = st.generatorResolvedSeed.abs % 4;
		st.scene = profile;
		~spawnModeVoice.(ch, profile.legacyMode, variant);
		~pulseEngines[ch].set(
			\sceneAmp, profile.pulse,
			\color, profile.color,
			\variant, variant
		);
		~noiseBeds[ch].set(
			\sceneAmp, profile.air,
			\color, profile.color,
			\voice, profile.legacyMode % 4,
			\variant, variant
		);
		("PDJ generator ch" ++ ch ++ " -> " ++ profile.name
			++ " / " ++ profile.family).postln;
	};

	~applyComposerChapter = { |ch|
		var st = ~ch[ch];
		case
		{ st.composerContent < 0 } {
			// Compositor desactivado: volver con limpieza al GraphicScore heredado.
			~applyVisualMode.(ch, st.mode.clip(0, 13));
		}
		{ st.composerContent == 0 } {
			~applyVisualMode.(ch, st.mode.clip(0, 13));
		}
		{ st.composerContent == 2 } {
			st.scene = ~breathProfile;
			~spawnModeVoice.(ch, ~breathProfile.legacyMode,
				st.generatorResolvedSeed.abs % 4);
			~pulseEngines[ch].set(\sceneAmp, 0.0);
			~noiseBeds[ch].set(\sceneAmp, ~breathProfile.air,
				\color, ~breathProfile.color, \voice, 3);
			("PDJ generator ch" ++ ch ++ " -> negativeSpace").postln;
		}
		{ st.composerContent == 3 } {
			~applyGeneratorMode.(ch, 7); // ThresholdBridge posee las transiciones.
		}
		{
			~applyGeneratorMode.(ch, st.generatorMode);
		};
		st.generatorAppliedRevision = st.generatorRevision;
		st.evolutionStage = -1;
	};

	~applyProgramMoment = { |moment|
		~scoreMoment = moment.asInteger.clip(0, 2);
		case
		{ ~scoreMoment == 0 } {
			~form.name = \visualPulseSystem;
			~form.rateLo = 0.75; ~form.rateHi = 32; ~form.lockToOSC = true;
			~setDimension.(2);
		}
		{ ~scoreMoment == 1 } {
			~form.name = \visualBarScanSystem;
			~form.rateLo = 0.3; ~form.rateHi = 1800; ~form.lockToOSC = true;
			~setDimension.(3);
		}
		{
			~form.name = \visualIntercalation;
			~form.rateLo = 0.4; ~form.rateHi = 4200; ~form.lockToOSC = false;
			~setDimension.(3);
		};
		("PDJ visual program moment -> " ++ ~form.name).postln;
	};

	~applyScoreStage = { |ch, stage|
		var st = ~ch[ch];
		var family = st.scene.family;
		var leader = (st.generatorOrganization != 0) or: { ch == 0 };
		if((st.composerContent > 0) and: { leader }, {
			case
			{ stage == 2 } {
				if([\bars, \noise, \machineC].includes(family), {
					~trigScoreSweep.(ch, 1, 0.055 + (st.generatorIntensity * 0.045), 1.8);
				});
				if(family == \rupture, { ~trigGlitch.(ch, 0.82, true) });
				if([\pulse, \machineD].includes(family), {
					~trigKick.(ch, 0.44 + (st.generatorIntensity * 0.34));
				});
			}
			{ stage == 3 } {
				if([\bars, \noise].includes(family), {
					~trigScoreSweep.(ch, -1, 0.05 + (st.generatorDensity * 0.04), 2.6);
				});
				if(family == \machineC, {
					~trigScoreSweep.(ch, -1, 0.045, 1.5);
					~trigKick.(ch, 0.72);
				});
			}
			{ stage == 4 } {
				if(family == \rupture, { ~trigGlitch.(ch, 0.28, true) });
			};
		});
	};

	// Una decisión determinista por subdivisión visual. La semilla entrante
	// hace que el mismo capítulo visual produzca el mismo esqueleto rítmico,
	// y cada flanco de entrada crea como máximo un evento de servidor.
	~onVisualSubdivision = { |beat, subdivision, subdivisionCount|
		var count = subdivisionCount.asInteger.max(1);
		var absolute = (beat.asInteger * count) + subdivision.asInteger;
		~ch.do { |st, ch|
			var family = st.scene.family;
			var leader = (st.generatorOrganization != 0) or: { ch == 0 };
			var probability = (
				(st.scene.pulse * 0.46) + (st.generatorDensity * 0.32)
				+ (st.generatorIntensity * 0.12)
			).clip(0.02, 0.78);
			var dice = ((st.generatorResolvedSeed.abs + (absolute * 37)
				+ (ch * 101)) % 1000) / 1000;
			var hit = dice < probability;
			if((st.generatorActive > 0) and: { leader } and: { hit }, {
				case
				{ [\data, \grid].includes(family) } {
					~trigGridHit.(ch, absolute % 8);
				}
				{ [\pulse, \machineC].includes(family) } {
					if(subdivision == 0, {
						~trigKick.(ch, 0.34 + (st.generatorIntensity * 0.34))
					}, {
						~trigGridHit.(ch, absolute % 8)
					});
				}
				{ family == \machineD } {
					if((subdivision == 0) and: { (beat % 2) == (ch % 2) }, {
						~trigKick.(ch, 0.40 + (st.generatorDensity * 0.22))
					}, {
						if((absolute % 4) == (ch % 4), { ~trigTrace.(ch) });
					});
				}
				{ family == \tonal } {
					if((absolute % 8) == (ch % 8), { ~trigTrace.(ch) });
				}
				{ family == \rupture } {
					if((absolute % 4) == 0, { ~trigGlitch.(ch, probability, true) });
				};
			});
		};
	};

	~applyContinuous = { |ch|
		var st, energy, flow, amp, rate, noise, spaceW, activity, tension;
		var slowBias, fastBias, clear, clipArc, voiceTarget, dynamicColor, variant;
		var droneAmp, airAmp, pulseAmp;
		var srcX, srcY, rolloff, spread, gains;
		var evolution, stage, formRise, formFall, microWindow, genEnvelope;
		var scoreActive, scoreIntensity, scoreDensity, scoreEnvelope;
		var roleDrone, rolePulse, roleAir, organizationGain;
		var collectiveMovement, collectivePhase;
		st     = ~ch[ch];
		~deriveBlobState.(st);
		energy = (st.energy * 6).clip(0, 1);
		flow   = (st.flowMag / 4).clip(0, 1);
		spaceW = ~macro.space.linlin(0, 1, 0.35, 1); // anchura estéreo estrecha<->plena
		slowBias = ((1 - st.speedMultiplier) / 0.7).clip(0, 1);
		fastBias = ((st.speedMultiplier - 1) / 2.5).clip(0, 1);
		clear = st.clearAlpha.clip(0, 1);
		collectiveMovement = ~collective.movement.clip(0, 8).asInteger;
		collectivePhase = ~collective.phase.clip(0, 1);
		scoreActive = st.composerContent >= 0;
		genEnvelope = if(scoreActive, {
			st.generatorEnvelope.clip(0, 1)
		}, {
			0
		});
		scoreIntensity = if(scoreActive, { st.generatorIntensity.clip(0, 1) }, { 0 });
		scoreDensity = if(scoreActive, { st.generatorDensity.clip(0, 1) }, { 0 });
		scoreEnvelope = if(scoreActive, { genEnvelope }, { 1 });
		evolution = if(scoreActive, {
			st.generatorChapterPhase.clip(0, 1)
		}, {
			st.videoPos.clip(0, 1)
		});
		clipArc = (evolution * pi).sin;
		stage = if(scoreActive, {
			st.generatorStage.clip(0, 4).asInteger
		}, {
			(evolution * 4).floor.clip(0, 3).asInteger
		});
		if(stage != st.evolutionStage, {
			st.evolutionStage = stage;
			("PDJ evolution ch" ++ ch ++ " -> stage " ++ stage ++ "  pos=" ++ evolution.round(0.01)).postln;
		});
		formRise = evolution.linlin(0, 0.30, 0.58, 1).clip(0.58, 1);
		formFall = evolution.linlin(0.78, 1, 1, 0.62).clip(0.62, 1);
		microWindow = evolution.linlin(0.35, 0.62, 0, 1).clip(0, 1)
			* evolution.linlin(0.82, 1, 1, 0).clip(0, 1);

		// El movimiento cambia la presión dentro de una escena persistente;
		// nunca enciende y apaga el cimiento ambiental a cadencia de fotograma.
		activity = (
			(energy * 0.64) + (flow * 0.16) + ((st[\blobSpeed] * 10).clip(0, 1) * 0.20)
		).clip(0, 1);
		if(scoreActive, {
			activity = ((activity * 0.58) + (scoreIntensity * 0.42)).clip(0, 1);
		});
		amp = activity.squared;
		tension = (
			st.scene.tension + (energy * 0.26) + (flow * 0.12)
			+ (st.crowd * 0.14) + ((st.contour / 7000).clip(0, 1) * 0.09)
			+ ((st.blobs / 8).clip(0, 1) * 0.07) + (st.leg * 0.04)
			+ (st[\blobSpread] * 0.10) + ((st[\blobSpeed] * 10).clip(0, 1) * 0.12)
			+ (fastBias * 0.22) + (slowBias * 0.06) + (clear * 0.24) + (clipArc * 0.05)
			+ (~collective.tension.clip(0, 1) * 0.22)
		).clip(0, 1);

		// DBAP spatial engine — source position derived from detected blobs.
		// x: 0=left wall, 1=right wall  /  y: 0=front, 1=rear
		srcX = if(st.blobs > 0, {
			st[\centroidX]
		}, {
			ch.linlin(0, ~numChannels - 1, 0.1, 0.9)
		});
		srcY = (st[\centroidY] ? 0.5);
		// Subtle flow-direction bias: nudge source slightly toward movement direction.
		srcX = (srcX + (st.flowAng.cos * flow * 0.06)).clip(0, 1);
		srcY = (srcY + (st.flowAng.sin.neg * flow * 0.06)).clip(0, 1);
		// Composer role phase adds subtle x-drift per channel's assigned role.
		if(scoreActive, {
			srcX = (srcX + ((st.generatorRolePhase - 0.5) * 0.10)).clip(0, 1);
		});
		// Spread: few blobs = focused point source; many = diffuse across all speakers.
		spread = (st.blobs.clip(0, 8) / 8.0).linlin(0, 1, 0.0, 0.55);
		// Rolloff: still scene = sharp localisation; fast flow = broader halo.
		rolloff = flow.linlin(0, 1, 1.8, 0.9);

		// El pulso sigue microscópico: los modos de dato lo revelan; los
		// cinematográficos lo reprimen vía scene.pulse. El recuento de blobs
		// cambia la subdivisión; el movimiento controla la velocidad.
		rate = (
			0.3 + (activity * 7) + (st.blobs.clip(0, 8) * 0.28)
			+ (genEnvelope * 3.5) + (scoreDensity * 2.8)
		).clip(0.3, 12) * st.speedMultiplier.clip(0.35, 2.2);

		noise = (
			(activity * 0.42) + ((st.contour / 6000).clip(0, 0.22))
			+ (st.crowd * 0.16) + (st[\blobSpread] * 0.14) + (st[\areaMass] * 0.10)
			+ (scoreDensity * 0.22)
		).clip(0, 1);

		// El servicio del director crea una transformación vocal clara: cámara
		// lenta tira de cada escena hacia la bruma temporal; cámara rápida hacia dato/vidrio.
		voiceTarget = if(st.scene.voice.isNil, {
			st.scene.legacyMode % 4
		}, {
			st.scene.voice
		});
		voiceTarget = (voiceTarget * (1 - (slowBias * 0.78))) + (3 * slowBias * 0.78);
		voiceTarget = (voiceTarget * (1 - (fastBias * 0.72))) + (2 * fastBias * 0.72);
		dynamicColor = (
			st.scene.color + (fastBias * 0.18) - (slowBias * 0.08) + (clipArc * 0.07)
		).clip(0.02, 1);
		variant = if(scoreActive, {
			st.generatorResolvedSeed.abs % 4
		}, {
			(st.videoRevision.max(0) + st.modeRevision + ch) % 4
		});
		roleDrone = if(scoreActive, { ~roleDroneGain.wrapAt(st.generatorRole) }, { 1 });
		rolePulse = if(scoreActive, { ~rolePulseGain.wrapAt(st.generatorRole) }, { 1 });
		roleAir = if(scoreActive, { ~roleAirGain.wrapAt(st.generatorRole) }, { 1 });
		organizationGain = if(scoreActive and: { st.generatorOrganization == 0 }, {
			if(ch == 0, { 1 }, { 0 })
		}, { 1 });
		if(scoreActive and: { st.generatorOrganization == 3 }
			and: { st.generatorObservedGroup == 0 }, {
			roleDrone = roleDrone * 1.12;
			rolePulse = rolePulse * 0.55;
			roleAir = roleAir * 1.25;
		});
		if(collectiveMovement == 4, {
			// Convergence colapsa el primer plano al canal 0 pero conserva un
			// halo silencioso de los demás roles de pantalla.
			organizationGain = if(scoreActive, { if(ch == 0, { 1 }, { 0.18 }) }, { 1 });
		});
		droneAmp = st.scene.drone * formRise * formFall
			* (1 - (clear * 0.86)) * (1 + (slowBias * 0.08))
			* roleDrone * organizationGain * scoreEnvelope;
		airAmp = st.scene.air * (0.45 + (evolution * 0.85))
			* (1 - (clear * 0.48)) * (1 + (fastBias * 0.42) + (tension * 0.12))
			* roleAir * organizationGain * scoreEnvelope;
		pulseAmp = (st.scene.pulse + (genEnvelope * 0.18)) * microWindow * 0.28
			* (1 - clear) * (0.72 + (fastBias * 0.75) - (slowBias * 0.48))
			* rolePulse * organizationGain * scoreEnvelope;
		if(scoreActive and: { st.scene.family == \machineC }, {
			// Machine C: el ruido se condensa de forma progresiva en pulsos rítmicos.
			airAmp = airAmp * evolution.linlin(0, 0.82, 1.35, 0.42).clip(0.42, 1.35);
			pulseAmp = pulseAmp * evolution.linlin(0.18, 1, 0.20, 1.42).clip(0.20, 1.42);
		});
		if(scoreActive and: { st.scene.family == \machineD }, {
			// Machine D: se conserva el drone mientras las ventanas rítmicas se abren y cierran.
			pulseAmp = pulseAmp * (0.42 + ((evolution * 4 * pi).sin.abs * 0.88));
			droneAmp = droneAmp * (0.88 + ((evolution * 2 * pi).cos.abs * 0.18));
		});
		if(scoreActive and: { st.scene.family == \bars }, {
			airAmp = airAmp * (0.62 + (clipArc * 0.82));
		});

		// El movimiento formal global reorganiza cada voz local sin borrar
		// su identidad de generador. Es la consecuencia audible del
		// comportamiento colectivo de la imagen, no otro programador de escenas.
		case
		{ collectiveMovement == 0 } { // suspensión / espacio negativo real
			droneAmp = droneAmp * 0.48;
			airAmp = airAmp * 0.16;
			pulseAmp = 0;
			// Spatially: very focused, tighten toward detected position
			rolloff = rolloff * 1.6;
			spread = spread * 0.3;
		}
		{ collectiveMovement == 2 } { // accumulation
			droneAmp = droneAmp * collectivePhase.linlin(0, 1, 0.62, 1.0);
			airAmp = airAmp * collectivePhase.linlin(0, 1, 0.42, 1.38);
			pulseAmp = pulseAmp * collectivePhase.linlin(0, 1, 0.55, 1.24);
			rate = rate * collectivePhase.linlin(0, 1, 0.62, 1.45);
		}
		{ collectiveMovement == 3 } { // propagación: sweep srcX per-channel via collectivePhase
			srcX = ((ch.linlin(0, ~numChannels - 1, 0.0, 1.0) + collectivePhase) % 1.0);
			srcY = 0.4;
			pulseAmp = pulseAmp * 0.72;
			airAmp = airAmp * 1.12;
		}
		{ collectiveMovement == 4 } { // convergencia: collapse toward D3-2 (front center)
			srcX = 0.50; srcY = 0.08; rolloff = 2.2; spread = 0.0;
			droneAmp = droneAmp * 1.18;
			airAmp = airAmp * 0.64;
			pulseAmp = pulseAmp * 1.48;
			rate = ((~visualTransport.bpm / 60) * 4).clip(1, 18);
		}
		{ collectiveMovement == 5 } { // fragmentación: maximum spread, exploits D3-6/D3-8 cluster
			droneAmp = droneAmp * 0.38;
			airAmp = airAmp * 1.18;
			pulseAmp = pulseAmp * 1.42;
			rate = (rate * 1.85).clip(0.3, 36);
			spread = 0.85; rolloff = 0.8;
		}
		{ collectiveMovement == 6 } { // saturación espectral
			droneAmp = droneAmp * 0.72;
			airAmp = airAmp * 1.72;
			pulseAmp = pulseAmp * 1.30;
			rate = (rate * 3.2).clip(0.3, 60);
		}
		{ collectiveMovement == 7 } { // ruptura seca: solo queda la voz del evento
			droneAmp = 0; airAmp = 0; pulseAmp = 0;
		}
		{ collectiveMovement == 8 } { // residuo / memoria transformada: slightly more focused
			droneAmp = droneAmp * 0.52;
			airAmp = airAmp * 0.24;
			pulseAmp = 0;
			rolloff = rolloff * 1.2;
			spread = spread * 0.6;
		};

		// Compute per-speaker DBAP gains and write to gainBus.
		gains = ~dbapGains.(srcX, srcY, rolloff, spread);
		// Dim gains proportionally on cleared channels (room wipe spatial complement).
		gains = gains * (1 - (clear * 0.88));
		// Store for one-shot event triggers so clicks/impacts follow detected position.
		st[\spatGains] = gains;

		st.ampBus.set(amp);
		st.gainBus.setn(gains);
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
				* st.scene.drone * roleDrone * organizationGain * scoreEnvelope
				* (1 - (clear * 0.92))).clip(0, 0.32)
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
		// Envío de profundidad: los eventos más atrás (ballY bajo) echan
		// más al bus de espacio/delay; los cercanos quedan más secos.
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
		// Limitación: oF envía ~30 fps — mantener los glitches dispersos
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
	// SECUENCIADOR DE REJILLA BINARIA — el dato como ritmo, en literal.
	// En cada paso se lee un valor detectado real (suma de blobs de todos
	// los canales) como número binario de 8 bits; cada bit abre un clic
	// en esa posición de la rejilla. El ritmo audible ES la representación
	// binaria de un valor real.
	//
	// Alternancia de sincronía contrapuntística: si ~form.lockToOSC es true,
	// el disparo sale de los manejadores OSC de colisión/balón
	// (microsincronía). Si es false, ~gridClock lo dispara en su propia
	// subdivisión fija, deliberadamente independiente de lo visual.
	// --------------------------------------------------------------------------
	~fireGridStep = {
		var count = (~ch.collect(_.blobs).sum).clip(0, 255).asInteger;
		var bits  = ~binaryBits.(count, 8);
		var dataChannels = (0 .. (~numChannels - 1)).select({ |ch|
			if(~ch[ch].generatorActive > 0, {
				~isGeneratorData.(~ch[ch])
			}, {
				~isDataMode.(~ch[ch].mode)
			})
		});
		~gridStep = (~gridStep + 1) % 8;
		if((bits[~gridStep] == 1) and: { dataChannels.notEmpty }, {
			var ch = dataChannels.wrapAt(~gridStep);
			~trigGridHit.(ch, ~gridStep);
		});
	};

	~trigGridHit = { |ch, stepIdx|
		var sweep = ~panSweep.(ch);
		var freq  = 1800 + (stepIdx * 900); // la altura refleja la posición del bit — lectura literal
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
	// RECEPCIÓN OSC — las rutas coinciden con OSCSender.cpp
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
			if((previous >= 0) and: { revision != previous }, {
				~ch[ch].evolutionStage = -1;
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

		OSCdef(("pdj_generatorActive_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorActive = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/generator/active").asSymbol);

		OSCdef(("pdj_generatorContent_" ++ ch).asSymbol, { |msg|
			~ch[ch].composerContent = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/generator/content").asSymbol);

		OSCdef(("pdj_generatorMode_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorMode = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/generator/mode").asSymbol);

		OSCdef(("pdj_generatorRevision_" ++ ch).asSymbol, { |msg|
			var revision = msg[1].asInteger;
			~ch[ch].generatorRevision = revision;
			if(revision != ~ch[ch].generatorAppliedRevision, {
				// La revisión llega antes que los campos stage/seed/program del
				// mismo paquete OSC. Se aplaza un slice del programador para
				// aplicar el capítulo desde una instantánea completa, no una
				// mezcla de campos viejos y nuevos.
				SystemClock.sched(0.015, {
					if(~ch[ch].generatorRevision == revision, {
						~applyComposerChapter.(ch);
					});
					nil
				});
			});
		}, ("/pdj/channel/" ++ ch ++ "/generator/revision").asSymbol);

		OSCdef(("pdj_generatorOrganization_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorOrganization = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/generator/organization").asSymbol);

		OSCdef(("pdj_generatorRole_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorRole = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/generator/role").asSymbol);

		OSCdef(("pdj_generatorStage_" ++ ch).asSymbol, { |msg|
			var stage = msg[1].asInteger.clip(0, 4);
			var previous = ~ch[ch].generatorStage;
			~ch[ch].generatorPreviousStage = previous;
			~ch[ch].generatorStage = stage;
			if(stage != previous, {
				SystemClock.sched(0.015, {
					if(~ch[ch].generatorStage == stage, {
						~applyScoreStage.(ch, stage);
					});
					nil
				});
			});
		}, ("/pdj/channel/" ++ ch ++ "/generator/stage").asSymbol);

		OSCdef(("pdj_generatorStageProgress_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorStageProgress = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/stage_progress").asSymbol);

		OSCdef(("pdj_generatorPhase_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorChapterPhase = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/phase").asSymbol);

		OSCdef(("pdj_generatorIntensity_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorIntensity = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/intensity").asSymbol);

		OSCdef(("pdj_generatorDensity_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorDensity = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/density").asSymbol);

		OSCdef(("pdj_generatorRolePhase_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorRolePhase = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/role_phase").asSymbol);

		OSCdef(("pdj_generatorPropagationDelay_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorPropagationDelay = msg[1].asFloat.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/generator/propagation_delay").asSymbol);

		OSCdef(("pdj_generatorObservedGroup_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorObservedGroup = msg[1].asInteger.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/observed_group").asSymbol);

		OSCdef(("pdj_generatorBeatPhase_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorBeatPhase = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/beat_phase").asSymbol);

		OSCdef(("pdj_generatorBeat_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorBeatIndex = msg[1].asInteger;
		}, ("/pdj/channel/" ++ ch ++ "/generator/beat_index").asSymbol);

		OSCdef(("pdj_generatorEnvelope_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorEnvelope = msg[1].asFloat.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/envelope").asSymbol);

		OSCdef(("pdj_generatorSeed_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorSeed = msg[1].asInteger.max(1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/seed").asSymbol);

		OSCdef(("pdj_generatorResolvedSeed_" ++ ch).asSymbol, { |msg|
			~ch[ch].generatorResolvedSeed = msg[1].asInteger.max(1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/resolved_seed").asSymbol);

		OSCdef(("pdj_generatorTransition_" ++ ch).asSymbol, { |msg|
			~ch[ch].transitionActive = msg[1].asInteger.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/generator/transition").asSymbol);

		OSCdef(("pdj_programEnabled_" ++ ch).asSymbol, { |msg|
			~ch[ch].programEnabled = msg[1].asInteger.clip(0, 1);
		}, ("/pdj/channel/" ++ ch ++ "/program/enabled").asSymbol);

		OSCdef(("pdj_programMoment_" ++ ch).asSymbol, { |msg|
			var moment = msg[1].asInteger.clip(0, 2);
			~ch[ch].installationMoment = moment;
			if((ch == 0) and: { moment != ~scoreMoment }, {
				~applyProgramMoment.(moment);
			});
		}, ("/pdj/channel/" ++ ch ++ "/program/moment").asSymbol);

		OSCdef(("pdj_programGroupVideoCount_" ++ ch).asSymbol, { |msg|
			~ch[ch].groupVideoOccupancy = msg[1].asInteger.max(0);
		}, ("/pdj/channel/" ++ ch ++ "/program/group_video_count").asSymbol);

		OSCdef(("pdj_programTakeover_" ++ ch).asSymbol, { |msg|
			var takeover = msg[1].asInteger.clip(0, 1);
			var previous = ~ch[ch].globalTakeover;
			~ch[ch].globalTakeover = takeover;
			if((ch == 0) and: { takeover == 1 } and: { previous == 0 }, {
				~scoreTakeover = 1;
				~trigScoreSweep.(0, 1, 0.10, 2.8);
			});
			if((ch == 0) and: { takeover == 0 }, { ~scoreTakeover = 0 });
		}, ("/pdj/channel/" ++ ch ++ "/program/takeover").asSymbol);

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
				// La posición sigue al evento en el mismo paquete OSC de oF.
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

	// Transporte global publicado una vez por fotograma visual desde el canal cero.
	// La detección de flanco usa beat + índice de subdivisión, de modo que un
	// flag de pulso sostenido o un heartbeat repetido no dupliquen eventos de audio.
	OSCdef(\pdj_visual_clock, { |msg|
		var bpm = msg[1].asFloat.clip(20, 300);
		var phase = msg[2].asFloat.clip(0, 1);
		var beat = msg[3].asInteger;
		var subdivision = msg[4].asInteger.max(0);
		var subdivisionCount = msg[5].asInteger.max(1);
		var absolute = (beat * subdivisionCount) + subdivision;
		var previous = ~visualTransport.absoluteSubdivision;
		~visualTransport.bpm = bpm;
		~visualTransport.beatPhase = phase;
		~visualTransport.beatIndex = beat;
		~visualTransport.subdivisionIndex = subdivision;
		~visualTransport.absoluteSubdivision = absolute;
		~visualTransport.lastPacketAt = Main.elapsedTime;
		if((previous >= 0) and: { absolute != previous }, {
			~onVisualSubdivision.(beat, subdivision, subdivisionCount);
		});
	}, '/pdj/clock/state');

	// Estado formal global asistido por datos, calculado una vez a partir de
	// todos los canales visuales. Los cambios musicales grandes siguen evidencia
	// colectiva, no una segunda línea de tiempo autónoma dentro de SuperCollider.
	OSCdef(\pdj_collective_state, { |msg|
		var movement = msg[1].asInteger.clip(0, 8);
		var revision = msg[10].asInteger.max(0);
		var changed = (movement != ~collective.movement)
			or: { revision != ~collective.revision };
		~collective.phase = msg[2].asFloat.clip(0, 1);
		~collective.activity = msg[3].asFloat.clip(0, 1);
		~collective.coherence = msg[4].asFloat.clip(0, 1);
		~collective.diversity = msg[5].asFloat.clip(0, 1);
		~collective.convergence = msg[6].asFloat.clip(0, 1);
		~collective.population = msg[7].asFloat.clip(0, 1);
		~collective.tension = msg[8].asFloat.clip(0, 1);
		~collective.dominantGenerator = msg[9].asInteger;
		~collective.revision = revision;
		if(changed, { ~applyCollectiveMovement.(movement) });
	}, '/pdj/collective/state');

	// Un canal se considera desconectado si no ha empezado ninguna instantánea
	// completa durante un segundo. Así un emisor roto se hace visible en lugar
	// de componer en silencio con valores de CV indefinidamente obsoletos.
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
	// CONTROLES EN VIVO
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
	~setGenerator = { |mode = 0, organization = 2|
		~numChannels.do { |ch|
			var st = ~ch[ch];
			st.composerContent = 1;
			st.generatorActive = 1;
			st.generatorMode = mode.asInteger.clip(0, 14);
			st.generatorOrganization = organization.asInteger.clip(0, 3);
			st.generatorRole = ch;
			st.generatorResolvedSeed = 1001 + (ch * 97) + mode;
			st.generatorRevision = st.generatorRevision + 1;
			~applyComposerChapter.(ch);
		};
	};

	~shutdown = {
		if(~pdjEngineActive == true, {
			// Bajar primero la bandera para que este desmontaje sea seguro si se llama dos veces.
			~pdjEngineActive = false;
			~numChannels.do { |ch|
			[
				"pdj_frame_", "pdj_sourceTime_", "pdj_energy_", "pdj_flowMag_",
				"pdj_flowAng_", "pdj_blobs_", "pdj_contour_", "pdj_crowd_",
				"pdj_leg_", "pdj_mode_", "pdj_coll_", "pdj_ball_",
				"pdj_ballX_", "pdj_ballY_", "pdj_modeRevision_",
				"pdj_videoName_", "pdj_videoPos_", "pdj_videoDur_",
				"pdj_videoRevision_", "pdj_temporal_", "pdj_speed_",
				"pdj_clear_", "pdj_clearAlpha_", "pdj_generatorActive_",
				"pdj_generatorContent_", "pdj_generatorMode_",
				"pdj_generatorRevision_", "pdj_generatorOrganization_",
				"pdj_generatorRole_", "pdj_generatorStage_",
				"pdj_generatorStageProgress_", "pdj_generatorPhase_",
				"pdj_generatorIntensity_", "pdj_generatorDensity_",
				"pdj_generatorRolePhase_", "pdj_generatorPropagationDelay_",
				"pdj_generatorObservedGroup_", "pdj_generatorBeatPhase_",
				"pdj_generatorBeat_", "pdj_generatorEnvelope_",
				"pdj_generatorSeed_", "pdj_generatorResolvedSeed_",
				"pdj_generatorTransition_", "pdj_programEnabled_",
				"pdj_programMoment_", "pdj_programGroupVideoCount_",
				"pdj_programTakeover_"
				].do { |prefix|
					OSCdef((prefix ++ ch).asSymbol).free;
				};
				8.do { |slot|
					OSCdef(("pdj_blobState_" ++ ch ++ "_" ++ slot).asSymbol).free;
				};
			};
			OSCdef(\pdj_visual_clock).free;
			OSCdef(\pdj_collective_state).free;
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
				st.ampBus.free; st.gainBus.free; st.rateBus.free; st.noiseBus.free; st.tensionBus.free;
			};
			~gNoiseBus.free; ~gDensityBus.free; ~gSpaceBus.free; ~gCrushBus.free;
			~gGate1Bus.free; ~gGate2Bus.free; ~gGate3Bus.free; ~selfBus.free; ~gSpectralBus.free;
			~gMotionBus.free; ~gCrowdBus.free; ~gTensionBus.free;
			~fxBus.free; ~delayBus.free;
			"pdj_datamatics shut down.".postln;
		});
		CmdPeriod.remove(~pdjCmdPeriodCleanup);
	};

	// Cmd+. suele liberar nodos del servidor pero deja vivos OSCdefs y
	// variables del Environment. Pasarlo por el desmontaje completo para que
	// IDs de nodo obsoletos no reciban después /n_set ni /s_new.
	~pdjCmdPeriodCleanup = { ~shutdown.value };
	CmdPeriod.add(~pdjCmdPeriodCleanup);

	// --------------------------------------------------------------------------
	// OPCIONAL: alimentación OSC sintética (sin oF)
	// --------------------------------------------------------------------------
	~testOsc = Routine({
		var net = NetAddr("127.0.0.1", ~oscPort);
		var tick = 0;
		inf.do {
			var beatPosition = (tick / 15) * 1.5;
			var beat = beatPosition.floor.asInteger;
			var beatPhase = beatPosition.frac;
			var subdivision = (beatPhase * 4).floor.asInteger.clip(0, 3);
			var collectiveRevision = (tick / 90).floor.asInteger;
			var collectiveMovement = collectiveRevision % 9;
			net.sendMsg('/pdj/clock/state', 90.0, beatPhase, beat, subdivision, 4, 1);
			net.sendMsg('/pdj/collective/state',
				collectiveMovement, (tick % 90) / 90,
				collectiveMovement.linlin(0, 8, 0.05, 0.88),
				0.25 + (0.65.rand), 0.15 + (0.75.rand),
				if(collectiveMovement == 4, { 1.0 }, { 0.25 }),
				0.55.rand, collectiveMovement.linlin(0, 8, 0.08, 0.92),
				if(collectiveMovement == 4, { 8 }, { -1 }), collectiveRevision);
			~numChannels.do { |ch|
				var e = exprand(0.02, 0.7);
				var mode = (((tick / 75).floor) + ch) % 15; // sostener cada generador cinco segundos
				var revision = (tick / 75).floor.asInteger;
				var chapterPhase = (tick % 75) / 75;
				var stage = case
					{ chapterPhase < 0.12 } { 0 }
					{ chapterPhase < 0.55 } { 1 }
					{ chapterPhase < 0.72 } { 2 }
					{ chapterPhase < 0.90 } { 3 }
					{ 4 };
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
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/phase").asSymbol, chapterPhase);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/intensity").asSymbol, e.clip(0, 1));
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/density").asSymbol, (0.25 + e).clip(0, 1));
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/role_phase").asSymbol, ch / 8);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/propagation_delay").asSymbol, 0.0);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/observed_group").asSymbol, if(ch < 4, { 1 }, { 0 }));
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/score/mode").asSymbol, mode);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/score/revision").asSymbol, revision);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/name").asSymbol, "synthetic_" ++ revision);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/position").asSymbol, (tick % 75) / 75);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/duration").asSymbol, 5.0);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/video/revision").asSymbol, revision);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/active").asSymbol, 1);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/content").asSymbol, 1);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/mode").asSymbol, mode);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/revision").asSymbol, revision + 1);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/organization").asSymbol, 2);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/role").asSymbol, ch);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/stage").asSymbol, stage);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/stage_progress").asSymbol, chapterPhase);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/beat_phase").asSymbol, beatPhase);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/beat_index").asSymbol, beat);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/envelope").asSymbol, (chapterPhase * pi).sin.clip(0, 1));
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/seed").asSymbol, 1001 + (revision * 31) + ch);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/resolved_seed").asSymbol, 2001 + (revision * 67) + ch);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/generator/transition").asSymbol, 0);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/program/enabled").asSymbol, 1);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/program/moment").asSymbol, (revision / 3).floor.asInteger % 3);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/program/group_video_count").asSymbol, 1);
				net.sendMsg(("/pdj/channel/" ++ ch ++ "/program/takeover").asSymbol, 0);
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
			(1/15).wait; // flujo de prueba ~15 Hz (oF está a ~30)
		};
	});

	~pdjEngineActive = true;
	"".postln;
	"========================================================".postln;
	("  PDJ Datamatics — listening OSC UDP :" ++ ~oscPort.asString).postln;
	"  ~testOsc.play;      // synthetic feed".postln;
	"  ~setMaster.(0.4);   // master gain".postln;
	"  ~setScene.(12);     // audition visual scene 0..13 (12 = slit-scan)".postln;
	"  ~setGenerator.(7);  // audition generator 0..14 (7 = Machine C)".postln;
	"  ~auditionModes.();  // hear all 14 mode SynthDefs, five seconds each".postln;
	"  ~ch.collect(_.scene); // inspect active video/generator sound scenes".postln;
	"  ~shutdown.();       // teardown".postln;
	"========================================================".postln;
});
)
