#include "OSCSender.h"

void OSCSender::setup(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    reconnect();
}

void OSCSender::reconnect() {
    sender_.setup(host_, port_);
    ofLogNotice("OSCSender") << "Sending to " << host_ << ":" << port_;
}

void OSCSender::send(const CVData& d) {
    int ch = d.channelIdx;
    std::string base = "/pdj/channel/" + ofToString(ch);

    ofxOscBundle coreBundle;
    ofxOscBundle blobBundle;
    ofxOscBundle contextBundle;
    ofxOscBundle generatorBundle;

    auto addFloat = [](ofxOscBundle& target, const std::string& addr, float v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addFloatArg(v);
        target.addMessage(m);
    };
    auto addInt = [](ofxOscBundle& target, const std::string& addr, int v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addIntArg(v);
        target.addMessage(m);
    };
    auto addString = [](ofxOscBundle& target, const std::string& addr, const std::string& v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addStringArg(v);
        target.addMessage(m);
    };

    // Secuencia y tiempo permiten al receptor detectar pérdida, reordenación o caducidad.
    addInt  (coreBundle, base + "/state/frame", d.frameSequence);
    addFloat(coreBundle, base + "/state/time",  d.timestamp);

    // Flujo + movimiento
    addFloat(coreBundle, base + "/flow/magnitude", d.flowMagnitude);
    addFloat(coreBundle, base + "/flow/angle",     d.flowAngle);
    addFloat(coreBundle, base + "/motion/energy",  d.motionEnergy);

    // Blobs
    addInt  (coreBundle, base + "/blobs/count",    d.blobCount);
    addFloat(coreBundle, base + "/contour/length", d.contourLength);

    // Publica siempre todos los huecos fijos. Vaciar los no usados evita que una
    // detección desaparecida permanezca indefinidamente en el estado de SuperCollider.
    // Un mensaje compacto por hueco mantiene el datagrama UDP por debajo del tamaño de fragmentación.
    int maxBlobs = std::min((int)d.blobs.size(), 8);
    for (int i = 0; i < 8; i++) {
        std::string bp = base + "/blob/" + ofToString(i);
        bool active = i < maxBlobs;
        ofxOscMessage m;
        m.setAddress(bp + "/state");
        m.addIntArg(active ? 1 : 0);
        m.addFloatArg(active ? d.blobs[i].x    : 0.f);
        m.addFloatArg(active ? d.blobs[i].y    : 0.f);
        m.addFloatArg(active ? d.blobs[i].vx   : 0.f);
        m.addFloatArg(active ? d.blobs[i].vy   : 0.f);
        m.addFloatArg(active ? d.blobs[i].area : 0.f);
        m.addFloatArg(active ? d.blobs[i].bbW  : 0.f);
        m.addFloatArg(active ? d.blobs[i].bbH  : 0.f);
        blobBundle.addMessage(m);
    }

    // Eventos
    addInt  (coreBundle, base + "/event/collision",    d.events.collision    ? 1 : 0);
    addInt  (coreBundle, base + "/event/ball",         d.events.ballDetected ? 1 : 0);
    addFloat(coreBundle, base + "/event/ball/x",       d.events.ballPos.x);
    addFloat(coreBundle, base + "/event/ball/y",       d.events.ballPos.y);
    addFloat(coreBundle, base + "/event/crowd",        d.events.crowdDensity);
    addFloat(coreBundle, base + "/event/leg_distance", d.events.legDistance);

    // Partitura visual resuelta de forma continua. Estos valores pueden cambiar
    // cada fotograma aunque la revisión de capítulo siga estable, así que van
    // en el bundle principal y no en el bundle de generador solo-si-cambia.
    addFloat(coreBundle, base + "/generator/phase", d.generatorChapterPhase);
    addInt(coreBundle, base + "/generator/stage", d.generatorStage);
    addFloat(coreBundle, base + "/generator/stage_progress",
             d.generatorStageProgress);
    addFloat(coreBundle, base + "/generator/envelope", d.generatorEnvelope);
    addFloat(coreBundle, base + "/generator/intensity", d.generatorIntensity);
    addFloat(coreBundle, base + "/generator/density", d.generatorDensity);
    addFloat(coreBundle, base + "/generator/role_phase", d.generatorRolePhase);
    addFloat(coreBundle, base + "/generator/propagation_delay",
             d.generatorPropagationDelay);
    addInt(coreBundle, base + "/generator/observed_group",
           d.generatorObservedGroup);

    // Hay un solo transporte para la instalación. El canal cero lo publica
    // cada fotograma; SuperCollider lo usa como referencia común de flanco
    // de beat/subdivision para los ocho roles de canal.
    if (ch == 0) {
        ofxOscMessage clock;
        clock.setAddress("/pdj/clock/state");
        clock.addFloatArg(d.generatorBpm);
        clock.addFloatArg(d.generatorBeatPhase);
        clock.addIntArg(d.generatorBeatIndex);
        clock.addIntArg(d.generatorSubdivisionIndex);
        clock.addIntArg(d.generatorBeatSubdivision);
        clock.addIntArg(d.generatorSubdivisionPulse > 0.f ? 1 : 0);
        coreBundle.addMessage(clock);

        // Un paquete compacto de análisis global. Describe la condición
        // visual/de datos colectiva que eligió el movimiento musical actual.
        ofxOscMessage collective;
        collective.setAddress("/pdj/collective/state");
        collective.addIntArg(d.collectiveMovement);
        collective.addFloatArg(d.collectivePhase);
        collective.addFloatArg(d.collectiveActivity);
        collective.addFloatArg(d.collectiveCoherence);
        collective.addFloatArg(d.collectiveDiversity);
        collective.addFloatArg(d.collectiveConvergence);
        collective.addFloatArg(d.collectivePopulation);
        collective.addFloatArg(d.collectiveTension);
        collective.addIntArg(d.collectiveDominantGenerator);
        collective.addIntArg(d.collectiveRevision);
        coreBundle.addMessage(collective);
    }

    // Vídeo, partitura visual y momento de interpretación compartido. Las
    // revisiones van en cada instantánea, así un paquete de cambio perdido se recupera en el siguiente.
    addString(contextBundle, base + "/video/name",         d.videoName);
    addFloat (contextBundle, base + "/video/position",     d.videoPosition);
    addFloat (contextBundle, base + "/video/duration",     d.videoDuration);
    addInt   (contextBundle, base + "/video/revision",     d.videoRevision);
    addInt   (contextBundle, base + "/video/plan_type",    d.videoPlanType);
    addInt   (contextBundle, base + "/video/shared",       d.videoShared);
    addInt   (contextBundle, base + "/score/mode",         d.scoreMode);
    addInt   (contextBundle, base + "/score/revision",     d.scoreRevision);
    addInt   (contextBundle, base + "/director/temporal",  d.temporalPhase);
    addFloat (contextBundle, base + "/director/speed",     d.speedMultiplier);
    addInt   (contextBundle, base + "/director/clear",     d.clearPhase);
    addFloat (contextBundle, base + "/director/clear_alpha", d.clearAlpha);
    addInt   (generatorBundle, base + "/generator/active",       d.generatorActive);
    addInt   (generatorBundle, base + "/generator/content",
              d.composerContent);
    addInt   (generatorBundle, base + "/generator/mode",         d.generatorMode);
    addInt   (generatorBundle, base + "/generator/revision",     d.generatorRevision);
    addInt   (generatorBundle, base + "/generator/organization", d.organizationMode);
    addInt   (generatorBundle, base + "/generator/role",         d.screenRole);
    addInt   (generatorBundle, base + "/generator/stage",        d.generatorStage);
    addFloat (generatorBundle, base + "/generator/stage_progress",
              d.generatorStageProgress);
    addFloat (generatorBundle, base + "/generator/beat_phase",   d.generatorBeatPhase);
    addInt   (generatorBundle, base + "/generator/beat_index",   d.generatorBeatIndex);
    addFloat (generatorBundle, base + "/generator/subdivision",  d.generatorSubdivisionPulse);
    addFloat (generatorBundle, base + "/generator/envelope",     d.generatorEnvelope);
    addInt   (generatorBundle, base + "/generator/seed",         d.generatorSeed);
    addInt   (generatorBundle, base + "/generator/resolved_seed",
              d.generatorResolvedSeed);
    addInt   (generatorBundle, base + "/generator/transition",   d.transitionActive);
    addInt   (generatorBundle, base + "/program/enabled", d.programEnabled);
    addInt   (generatorBundle, base + "/program/moment",
              d.installationMoment);
    addInt   (generatorBundle, base + "/program/group_video_count",
              d.groupVideoOccupancy);
    addInt   (generatorBundle, base + "/program/takeover",
              d.globalTakeover);

    std::string vpcBase = "/pdjv/channel/" + ofToString(ch) + "/vpc";
    addInt   (generatorBundle, vpcBase + "/enabled", d.vpcEnabled);
    addInt   (generatorBundle, vpcBase + "/depthSource", d.vpcDepthSource);
    addInt   (generatorBundle, vpcBase + "/maskMode", d.vpcMaskMode);
    addInt   (generatorBundle, vpcBase + "/preset", d.vpcPreset);
    addInt   (generatorBundle, vpcBase + "/gridWidth", d.vpcGridWidth);
    addInt   (generatorBundle, vpcBase + "/gridHeight", d.vpcGridHeight);
    addFloat (generatorBundle, vpcBase + "/depthScale", d.vpcDepthScale);
    addFloat (generatorBundle, vpcBase + "/pointSize", d.vpcPointSize);
    addFloat (generatorBundle, vpcBase + "/luminanceFloor",
              d.vpcLuminanceFloor);
    addFloat (generatorBundle, vpcBase + "/colorGain", d.vpcColorGain);
    addFloat (generatorBundle, vpcBase + "/cameraYaw", d.vpcCameraYaw);
    addFloat (generatorBundle, vpcBase + "/cameraDistance", d.vpcCameraDistance);
    addInt   (generatorBundle, vpcBase + "/feedbackEnabled", d.vpcFeedbackEnabled);
    addFloat (generatorBundle, vpcBase + "/feedbackDecay",
              d.vpcFeedbackDecay);

    // coreBundle y blobBundle llevan datos CV/evento que impulsan la reactividad
    // de audio y deben enviarse cada fotograma.
    sender_.sendBundle(coreBundle);
    sender_.sendBundle(blobBundle);

    // Avanza el contador de heartbeat para refrescar los bundles de estado al
    // menos cada kHeartbeatInterval fotogramas aunque las revisiones no hayan cambiado.
    heartbeatCounter_ = (heartbeatCounter_ + 1) % kHeartbeatInterval;
    const bool heartbeat = (heartbeatCounter_ == 0);

    // contextBundle: los metadatos de vídeo y el modo de partitura cambian con poca frecuencia.
    const bool contextDirty = (d.videoRevision != lastVideoRevision_ ||
                                d.scoreRevision != lastScoreRevision_);
    if (contextDirty || heartbeat) {
        sender_.sendBundle(contextBundle);
        lastVideoRevision_ = d.videoRevision;
        lastScoreRevision_ = d.scoreRevision;
    }

    // generatorBundle: la config de generator / VPC cambia con poca frecuencia.
    const bool generatorDirty = (d.generatorRevision != lastGeneratorRevision_);
    if (generatorDirty || heartbeat) {
        sender_.sendBundle(generatorBundle);
        lastGeneratorRevision_ = d.generatorRevision;
    }
}
