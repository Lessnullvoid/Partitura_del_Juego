// =============================================================================
// Partitura del Juego — Configuración del dispositivo de audio
// =============================================================================
// Evaluar este bloque UNA VEZ antes de cargar pdj_datamatics.scd en cada sesión.
// Detecta automáticamente Dante Virtual Soundcard en la red local.
//
// DANTE detectado   -> 8 canales de salida @ 48 kHz (Sala Abierta / DANTE 5)
// DANTE no encontrado -> 2 canales de salida @ frecuencia del sistema (rescate estéreo)
//
// Orden de arranque:
//   1. Activar Dante Virtual Soundcard (app de la barra de menú, TX=8 RX=2 48kHz).
//   2. Evaluar este archivo (seleccionar todo -> Cmd+Return).
//   3. Esperar "Server ready" en el Post Window.
//   4. Evaluar pdj_datamatics.scd.
//
// Para listar todos los dispositivos de audio en el Post Window:
//   ServerOptions.devices.do({ |d| d.postln });
// =============================================================================

(
var allDevices  = ServerOptions.devices;
var danteDevice = allDevices.detect({ |d|
	var lower = d.asString.toLower;
	lower.includesSubString("dante") or: { lower.includesSubString("dvs") }
});
var isDante = danteDevice.notNil;

~pdjDante    = isDante;
~numSpeakers = if(isDante, { 8 }, { 2 });

if(isDante, {
	s.options.device               = danteDevice;
	s.options.numOutputBusChannels = 8;
	s.options.numInputBusChannels  = 2;
	s.options.sampleRate           = 48000;
	s.options.blockSize            = 256;  // más seguro que 128 para audio de red
	("PDJ audio: DANTE detected [" ++ danteDevice ++ "] -> 8ch @ 48kHz").postln;
}, {
	s.options.device               = nil;  // usar la salida por defecto de macOS
	s.options.numOutputBusChannels = 2;
	s.options.numInputBusChannels  = 2;
	s.options.sampleRate           = nil;  // heredar la frecuencia de muestreo del sistema
	s.options.blockSize            = 128;
	"PDJ audio: DANTE not found -> stereo fallback on default device".postln;
	"  (connect Ethernet + enable DVS app if DANTE is expected)".postln;
});

s.options.memSize = 65536;  // 64 MB de memoria de muestras

// Reiniciar con las opciones nuevas. pdj_datamatics.scd lee ~numSpeakers tras el boot.
s.reboot;
)
