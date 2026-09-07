(
~pdjVolumetricCompat = {
    // Capa de compatibilidad: mensajes volumétricos /pdjv y recuentos de blobs heredados /pdj.
    ~vpcState = Array.fill(8, { IdentityDictionary.new });
    ~pdjProgramState = IdentityDictionary.new;
    ~pdjProgramState[\groups] = Array.fill(2, 0);
    8.do { |ch|
        OSCdef(("pdjvTime"++ch).asSymbol, { |msg|
            ~gMotion.wrapAt(ch).set(msg[1] ? 0);
        }, "/pdjv/channel/" ++ ch ++ "/timeline/time");
        OSCdef(("pdjvScene"++ch).asSymbol, { |msg|
            ~mode.wrapAt(ch).set(msg[1]);
        }, "/pdjv/channel/" ++ ch ++ "/scene/id");
        OSCdef(("pdjvPreset"++ch).asSymbol, { |msg|
            ~preset.wrapAt(ch).set(msg[1] ? 0);
        }, "/pdjv/channel/" ++ ch ++ "/scene/preset");
        OSCdef(("pdjvCam"++ch).asSymbol, { |msg|
            ~cameraMode.wrapAt(ch).set(msg[1]);
        }, "/pdjv/channel/" ++ ch ++ "/camera/mode");
        OSCdef(("pdjvBlobs"++ch).asSymbol, { |msg|
            ~blobCount.wrapAt(ch).set(msg[1] ? 0);
        }, "/pdj/channel/" ++ ch ++ "/blobs/count");
        [
            "enabled", "depthSource", "maskMode", "preset",
            "gridWidth", "gridHeight", "depthScale", "pointSize",
            "luminanceFloor", "colorGain", "cameraYaw",
            "cameraDistance", "feedbackEnabled", "feedbackDecay"
        ].do { |parameter|
            OSCdef(("pdjvVpc" ++ ch ++ parameter).asSymbol, { |msg|
                ~vpcState[ch][parameter.asSymbol] = msg[1];
            }, "/pdjv/channel/" ++ ch ++ "/vpc/" ++ parameter);
        };
        ["enabled", "moment", "group_video_count", "takeover"].do { |parameter|
            OSCdef(("pdjvProgram" ++ ch ++ parameter).asSymbol, { |msg|
                ~pdjProgramState[parameter.asSymbol] = msg[1];
                if(parameter == "group_video_count") {
                    ~pdjProgramState[\groups][ch.div(4)] = msg[1] ? 0;
                };
            }, "/pdj/channel/" ++ ch ++ "/program/" ++ parameter);
        };
    };
    // Controles opcionales del receptor del runtime de instalación en el puerto 9002.
    ~pdjRuntime = NetAddr("127.0.0.1", 9002);
    ~pdjNextMoment = { ~pdjRuntime.sendMsg("/pdjv/program/next", 1) };
    ~pdjGlobalTakeover = { |generator = -1|
        ~pdjRuntime.sendMsg("/pdjv/program/takeover", generator)
    };
    ~pdjGeneratorControl = { |parameter, value|
        ~pdjRuntime.sendMsg("/pdjv/generator/" ++ parameter, value)
    };
    OSCdef(\pdjvGenerator, { |msg| ~globalGenerator.set(msg[1]); }, '/pdjv/generator');
    OSCdef(\pdjvPresetIn, { |msg| ~globalPreset.set(msg[1] ? 0); }, '/pdjv/preset');
    OSCdef(\pdjvPlay, { |msg| ~playing.set(msg[1] ? 1); }, '/pdjv/play');
    OSCdef(\pdjvCameraMode, { |msg| ~cameraModeGlobal.set(msg[1]); }, '/pdjv/camera/mode');
    "pdj volumetric OSC compatibility loaded".postln;
};
)
