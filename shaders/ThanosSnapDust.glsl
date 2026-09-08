#version 320 es
// @author AuroraGraph
// @category Transition
// @description Thanos-snap ablation with massive pixel-continuum dust: every ablated image pixel becomes a grain carrying its exact source color, so the flying swarm stays a recognizable ghost of the image. Wire a TurbulenceMap (or any grayscale node) into dissolveMap and sweep progress 0..1 on the timeline. Dissolved areas become transparent.
// @revision 2026-09-08 00:00:00

precision highp float;

vec4 textureSK(sampler2D tex, vec2 fragCoord, mat3 matT)
{
    vec2 texSize = vec2(textureSize(tex, 0));
    mat3 correctedMat = matT * mat3(1.0 / texSize.x, 0.0, 0.0,
        0.0, 1.0 / texSize.y, 0.0,
        0.0, 0.0, 1.0);
    return texture(tex, (vec3(fragCoord, 1.0) * correctedMat).xy);
}

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform sampler2D inputTex; // @typeTexture
uniform mat3 inputTexMat; // @imagemat
uniform sampler2D dissolveMap; // @typeTexture wire a TurbulenceMap node here
uniform float iFrame;
uniform vec2 iResolution;

uniform float progress; // Dissolve progress, animate on the timeline @minmax{ 0.0, 1.0 } @init { 0.0 }
uniform float lumaBias; // Ablation order luminance bias: -1 dark first, +1 bright first @minmax{ -1.0, 1.0 } @init { 0.0 }
uniform float edgeWidth; // Frontier softness @minmax{ 0.005, 0.15 } @init { 0.04 }
uniform float edgeGlow; // Frontier glow intensity @minmax{ 0.0, 3.0 } @init { 1.2 }
uniform float windAngle; // Wind direction in degrees @minmax{ -180.0, 180.0 } @init { -30.0 }
uniform float windStrength; // Flight distance @minmax{ 0.05, 1.2 } @init { 0.45 }
uniform float swirl; // Subtle path wobble @minmax{ 0.0, 1.0 } @init { 0.35 }
uniform float particleSize; // Dust grain radius in px @minmax{ 0.8, 6.0 } @init { 1.6 }
uniform float density; // Fraction of image pixels that emit dust @minmax{ 0.3, 1.0 } @init { 0.85 }
uniform float timeScale; // Grain age per progress unit @minmax{ 0.5, 6.0 } @init { 2.5 }
uniform float drift; // Ambient sway of airborne dust @minmax{ 0.0, 1.0 } @init { 0.15 }

const float PI = 3.14159265359;

float hash21(vec2 p)
{
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

void main()
{
    vec2 uv = uv_coords.xy;
    vec2 px = uv * iResolution;

    // hoisted input sampling matrix (pixel space -> uv)
    vec2 inTexSize = vec2(textureSize(inputTex, 0));
    mat3 inMat = inputTexMat * mat3(1.0 / inTexSize.x, 0.0, 0.0,
        0.0, 1.0 / inTexSize.y, 0.0,
        0.0, 0.0, 1.0);
    vec2 mapTexSize = vec2(textureSize(dissolveMap, 0));

    // threshold sweeps a bit beyond [0,1] so progress 0 keeps everything, 1 dissolves everything
    float T = progress * (1.0 + 2.0 * edgeWidth) - edgeWidth;

    vec4 src = texture(inputTex, (vec3(px, 1.0) * inMat).xy);
    float lum = dot(src.rgb, vec3(0.299, 0.587, 0.114));
    float m = texture(dissolveMap, px / mapTexSize).r;
    m = clamp(m + lumaBias * (lum - 0.5) * 0.5, 0.0, 1.0);

    // ---- remaining image + frontier glow
    float kept = smoothstep(T - edgeWidth, T + edgeWidth, m);
    vec3 col = src.rgb * kept;
    float alpha = src.a * kept;

    float band = 1.0 - clamp(abs(m - T) / edgeWidth, 0.0, 1.0);
    band *= band;
    vec3 ember = vec3(1.0, 0.55, 0.25);
    col += mix(ember, vec3(1.0, 0.9, 0.75), band * 0.35) * band * edgeGlow * kept;
    alpha = max(alpha, band * edgeGlow * 0.12);

    // ---- pixel-continuum dust: every ablated source pixel is a grain
    if (T > 0.0) {
        float windRad = windAngle * PI / 180.0;
        vec2 windDir = vec2(cos(windRad), sin(windRad));
        vec2 perp = vec2(-windDir.y, windDir.x);
        float travel = windStrength * iResolution.y; // px flown at full age
        float maxAge = 0.45;
        float tAnim = iFrame * 0.0166667;

        float r = particleSize;
        float stepLen = max(1.0, r * 0.8);
        int probeCount = int(clamp(travel / stepLen, 4.0, 256.0));
        float effStep = travel / float(probeCount);

        float bestRaw = 0.0;
        float bestFade = 1.0;
        vec3 bestCol = vec3(0.0);

        // march upwind along the flight line; each probe asks where that source
        // pixel's grain should be now, and whether it lands on this pixel
        for (int j = 0; j < 256; j++) {
            if (j >= probeCount)
                break;
            float d = (float(j) + 0.5) * effStep;
            vec2 originPx = px - windDir * d;
            vec2 oCell = floor(originPx);

            float hgate = hash21(oCell * 0.7311);
            if (hgate > density)
                continue;

            // birth time of this source pixel
            float m0 = texture(dissolveMap, originPx / mapTexSize).r;
            float a = (T - m0) * timeScale;
            if (a <= 0.0 || a > maxAge)
                continue;

            // expected grain position: straight wind line + tiny bounded wobble + drift
            float ramp = min(a * 6.0, 1.0);
            vec2 disp = windDir * (travel * (a / maxAge)) * ramp;
            float ph = hash21(oCell + vec2(7.7, 3.1)) * 40.0;
            disp += perp * sin(a * 18.0 + ph) * (swirl * 1.0) * ramp;
            disp += windDir * cos(a * 13.0 + ph * 1.7) * (swirl * 0.5) * ramp;
            disp += perp * sin(tAnim * (0.9 + fract(ph * 0.013)) + ph) * (drift * 1.2) * ramp;
            disp += vec2(0.0, 1.0) * sin(tAnim * 0.7 + ph) * (drift * 0.8) * ramp;

            float dist = length(px - (originPx + disp));
            float cov = smoothstep(r, r * 0.35, dist);
            if (cov <= bestRaw)
                continue;

            // this grain covers the pixel - fetch its exact birth color (deferred, rare)
            vec2 originUV = (vec3(originPx, 1.0) * inMat).xy;
            vec4 srcO = texture(inputTex, originUV);
            if (srcO.a < 0.05)
                continue;

            bestRaw = cov;
            bestCol = srcO.rgb; // exact image ghost - no lift, no sparkle
            bestFade = 1.0 - 0.85 * smoothstep(0.55, 1.0, a / maxAge); // fade only late in flight
        }

        float grainA = bestRaw * bestFade;
        col = mix(col, bestCol, grainA * 0.9);
        alpha = max(alpha, grainA);
    }

    out_color = vec4(col, alpha);
}
