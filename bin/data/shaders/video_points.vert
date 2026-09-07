#version 410

uniform mat4 modelViewProjectionMatrix;
uniform sampler2DRect videoTex;
uniform sampler2DRect maskTex;
uniform sampler2DRect depthTex;
uniform vec2 videoTextureSize;
uniform vec2 maskTextureSize;
uniform vec2 depthTextureSize;
uniform float videoAspect;
uniform float depthScale;
uniform float depthCenter;
uniform float pointSize;
uniform float luminanceFloor;
uniform float luminanceCeiling;
uniform float gammaVal;
uniform float colorGain;
uniform float opacity;
uniform float zInvert;
uniform float xyScale;
uniform int depthSource;
uniform int maskMode;
uniform float playerEmphasis;
uniform float stadiumOpacity;
uniform vec4 analysisBBox;
uniform float analysisAvailable;
uniform float transitionAmount;
uniform float transitionDisplacement;
uniform float transitionTime;
uniform float transitionSeed;

in vec4 position;
in vec2 texcoord;
out vec4 pointColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32 + transitionSeed);
    return fract(p.x * p.y);
}

void main() {
    vec3 color = texture(videoTex, texcoord * videoTextureSize).rgb * colorGain;
    color = pow(max(color, vec3(0.0)), vec3(max(0.01, gammaVal)));
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float grey = luminance;
    luminance = clamp(luminance, luminanceFloor, luminanceCeiling);
    vec2 analysisUv = (texcoord - analysisBBox.xy) /
                      max(analysisBBox.zw, vec2(0.0001));
    bool inAnalysisBounds =
        all(greaterThanEqual(analysisUv, vec2(0.0))) &&
        all(lessThanEqual(analysisUv, vec2(1.0)));
    float analysisValid = analysisAvailable *
                          (inAnalysisBounds ? 1.0 : 0.0);
    vec2 safeAnalysisUv = clamp(analysisUv, vec2(0.0), vec2(1.0));
    float z = luminance;
    if (depthSource == 1) {
        float d = texture(depthTex, safeAnalysisUv * depthTextureSize).r;
        z = mix(luminance, d,
                analysisValid * step(0.001, d));
    } else if (depthSource == 2) {
        float d = texture(depthTex, safeAnalysisUv * depthTextureSize).r;
        z = mix(luminance, d, analysisValid * 0.65);
    }
    float mask = texture(maskTex, safeAnalysisUv * maskTextureSize).r *
                 analysisValid;
    float alpha = opacity * smoothstep(luminanceFloor, luminanceFloor + 0.08, luminance);
    if (maskMode == 1)
        alpha *= step(0.5, mask);
    else if (maskMode == 2)
        alpha *= mix(stadiumOpacity, playerEmphasis, mask);
    vec3 p = position.xyz;
    p.x *= videoAspect * xyScale;
    p.y *= xyScale;
    float signZ = zInvert > 0.5 ? -1.0 : 1.0;
    p.z = (z - depthCenter) * depthScale * signZ;
    float transitionNoise = hash21(
        floor(texcoord * vec2(512.0)) +
        floor(transitionTime * 6.0) * 0.013);
    float transitionVisible = smoothstep(
        transitionAmount - 0.12,
        transitionAmount + 0.12,
        transitionNoise);
    vec3 outward = normalize(vec3(p.xy, 0.18 + transitionNoise));
    p += outward * transitionDisplacement *
         transitionAmount * transitionAmount *
         (0.35 + transitionNoise);
    alpha *= transitionVisible;
    gl_Position = modelViewProjectionMatrix * vec4(p, 1.0);
    gl_PointSize = pointSize * (0.55 + luminance * 1.25);
    // La instalación es estrictamente monocroma: los puntos transportan solo la
    // luminancia del vídeo, nunca su croma.
    pointColor = vec4(vec3(grey), alpha);
}
