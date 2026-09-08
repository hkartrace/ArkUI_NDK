#version 320 es
// @author AuroraGraph
// @category Transition
// @description Thanos-snap ablation: the image slowly disintegrates into fine dust that flies away in the wind, each mote keeping the color of the spot it was born at. Wire a TurbulenceMap (or any grayscale node) into dissolveMap and sweep progress 0..1 on the timeline. Dissolved areas become transparent.
// @revision 2026-09-04 00:00:00

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
uniform float swirl; // Wobble of flight paths @minmax{ 0.0, 1.0 } @init { 0.35 }
uniform float particleSize; // Dust mote radius in px @minmax{ 1.0, 8.0 } @init { 3.0 }
uniform float density; // Dust density @minmax{ 0.3, 1.0 } @init { 0.85 }
uniform float timeScale; // Mote age per progress unit @minmax{ 0.5, 6.0 } @init { 2.5 }
uniform float drift; // Ambient sway of airborne dust @minmax{ 0.0, 1.0 } @init { 0.15 }

const float PI = 3.14159265359;

float hash21(vec2 p)
{
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float hash11(float n) { return fract(sin(n) * 43758.5453123); }

void main()
{
    vec2 uv = uv_coords.xy;
    vec2 px = uv * iResolution;
    float aspect = iResolution.x / iResolution.y;

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

    // ---- dust particles
    if (T > 0.0) {
        float windRad = windAngle * PI / 180.0;
        vec2 windDir = vec2(cos(windRad), sin(windRad));
        vec2 perp = vec2(-windDir.y, windDir.x);
        float travel = windStrength * iResolution.y; // px flown at full age
        float cellPx = max(6.0, iResolution.y / 80.0);
        vec2 grid = vec2(cellPx, cellPx);
        float maxAge = 0.45;
        float tAnim = iFrame * 0.0166667;

        vec3 pcol = vec3(0.0);
        float palpha = 0.0;

        // march upwind along the straight flight line, probing one cell per cell-width
        int probeCount = int(clamp(travel / cellPx, 4.0, 48.0));
        vec2 prevCell = vec2(1e9, 1e9);
        for (int j = 0; j < 48; j++) {
            if (j >= probeCount)
                break;
            vec2 q = px - windDir * ((float(j) + 0.5) * cellPx);
            vec2 cellId = floor(q / grid);
            if (cellId == prevCell)
                continue;
            prevCell = cellId;

            for (int sub = 0; sub < 2; sub++) {
                float h1 = hash21(cellId + vec2(17.3 * float(sub) + 0.7, 9.1 * float(sub) + 3.3));
                float h2 = hash21(cellId + vec2(5.2 - 7.7 * float(sub), 23.7 + 3.1 * float(sub)));
                float h3 = hash11(dot(cellId, vec2(1.0, 157.0)) + float(sub) * 27.13);
                if (h3 > density)
                    continue;

                vec2 originPx = (cellId + vec2(0.25 + 0.5 * h1, 0.25 + 0.5 * h2)) * grid;
                vec2 originUV = (vec3(originPx, 1.0) * inMat).xy;

                // birth time from the map at the mote's birth place
                float m0 = texture(dissolveMap, originPx / mapTexSize).r;
                m0 = clamp(m0 + (h2 - 0.5) * 0.03, 0.0, 1.0);
                vec4 srcO = texture(inputTex, originUV);
                if (srcO.a < 0.05)
                    continue; // motes only where the image had content
                float lumO = dot(srcO.rgb, vec3(0.299, 0.587, 0.114));
                m0 = clamp(m0 + lumaBias * (lumO - 0.5) * 0.5, 0.0, 1.0);

                float a = (T - m0) * timeScale; // mote age
                if (a <= 0.0 || a > maxAge)
                    continue;

                float ramp = min(a * 6.0, 1.0);
                vec2 disp = windDir * (travel * (a / maxAge)) * ramp;
                float ph = h1 * 6.2831853 + h2 * 3.7;
                // small per-mote wobble (kept below cell size so the upwind probe finds it)
                disp += perp * sin(a * 18.0 + ph) * (swirl * cellPx * 0.45) * ramp;
                disp += windDir * cos(a * 13.0 + ph * 1.7) * (swirl * cellPx * 0.18) * ramp;
                disp += perp * sin(tAnim * (0.9 + h2) + ph * 2.0) * (drift * cellPx * 0.50) * ramp;
                disp += vec2(0.0, 1.0) * sin(tAnim * 0.7 + ph) * (drift * cellPx * 0.30) * ramp;

                vec2 posPx = originPx + disp;
                float r = particleSize * (1.0 - 0.55 * (a / maxAge));
                float d = length(px - posPx);
                float disc = smoothstep(r, r * 0.25, d);
                float mote = disc * min(a * 25.0, 1.0) * pow(1.0 - a / maxAge, 0.65);
                if (mote <= 0.0)
                    continue;

                float sparkle = 0.9 + 0.25 * sin(tAnim * 6.0 + ph * 5.0);
                pcol += srcO.rgb * mote * 0.9 * sparkle;
                palpha += mote * 0.9;
            }
        }

        col += pcol;
        alpha = max(alpha, min(palpha, 1.0));
    }

    out_color = vec4(col, alpha);
}
