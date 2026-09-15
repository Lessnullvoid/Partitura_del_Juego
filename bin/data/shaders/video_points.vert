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
uniform int colorMode;
uniform vec4 cvBlobs[8];
uniform int cvBlobCount;
uniform float cvPadding;
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

vec3 ironbow(float t) {
    t = clamp(t, 0.0, 1.0);
    const vec3 c0 = vec3(0.000, 0.000, 0.000);
    const vec3 c1 = vec3(0.110, 0.040, 0.320);
    const vec3 c2 = vec3(0.520, 0.022, 0.190);
    const vec3 c3 = vec3(0.920, 0.215, 0.020);
    const vec3 c4 = vec3(1.000, 0.840, 0.060);
    const vec3 c5 = vec3(1.000, 1.000, 1.000);
    float s = t * 5.0;
    int i = int(s);
    float f = fract(s);
    if (i == 0) return mix(c0, c1, f);
    if (i == 1) return mix(c1, c2, f);
    if (i == 2) return mix(c2, c3, f);
    if (i == 3) return mix(c3, c4, f);
    return mix(c4, c5, f);
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
    float thermalValue = clamp(
        (grey - luminanceFloor) /
        max(0.0001, luminanceCeiling - luminanceFloor),
        0.0, 1.0);
    bool inDetection = false;
    if (colorMode == 2) {
        for (int i = 0; i < 8; ++i) {
            if (i >= cvBlobCount) break;
            vec4 blob = cvBlobs[i];
            vec2 halfSize = blob.zw + vec2(cvPadding);
            if (all(lessThan(abs(texcoord - blob.xy), halfSize))) {
                inDetection = true;
                break;
            }
        }
    }
    vec3 displayColor = vec3(grey);
    if (colorMode == 1 || inDetection)
        displayColor = ironbow(thermalValue);
    pointColor = vec4(displayColor, alpha);
}
