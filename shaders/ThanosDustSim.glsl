#version 320 es
// @author AuroraGraph
// @category Transition
// @description Feedback-optimized dust simulation for the Thanos-snap ablation (pair with ThanosSnapFast). Runs at half resolution in the top-left quadrant of its buffer; every frame the dust field is advected by the wind and new grains are born at the frontier with their exact source color. Wire its own output back to prevFrame (self-loop), feed dissolveMap + inputTex, connect its output to ThanosSnapFast.dustTex and animate ONLY this node's progress. Backward timeline scrub or a resize clears the dust.
// @revision 2026-09-08 00:00:00

precision highp float;

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform sampler2D prevFrame; // @typeTexture self-loop: this node's own previous output
uniform sampler2D inputTex; // @typeTexture
uniform mat3 inputTexMat; // @imagemat
uniform sampler2D dissolveMap; // @typeTexture wire a TurbulenceMap node here
uniform float iFrame;
uniform vec2 iResolution;

uniform float progress; // Dissolve progress, animate on the timeline @minmax{ 0.0, 1.0 } @init { 0.0 }
uniform float lumaBias; // Ablation order luminance bias, keep equal to ThanosSnapFast's @minmax{ -1.0, 1.0 } @init { 0.0 }
uniform float windAngle; // Wind direction in degrees @minmax{ -180.0, 180.0 } @init { -30.0 }
uniform float windStrength; // Flight distance @minmax{ 0.05, 1.2 } @init { 0.45 }
uniform float swirl; // Dust diffusion @minmax{ 0.0, 1.0 } @init { 0.35 }
uniform float drift; // Ambient sway of airborne dust @minmax{ 0.0, 1.0 } @init { 0.15 }
uniform float density; // Fraction of cells that emit dust @minmax{ 0.3, 1.0 } @init { 0.85 }
uniform float timeScale; // Grain age per progress unit @minmax{ 0.5, 6.0 } @init { 2.5 }

const float PI = 3.14159265359;
const float MAGIC = 0.4815; // state texel id
const float EW = 0.04; // frontier half-width (read by the composite)
const float MAX_AGE = 0.45;

float hash21(vec2 p)
{
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

void main()
{
    float tAnim = iFrame * 0.0166667;
    float T = progress * (1.0 + 2.0 * EW) - EW;

    // ---- state from the previous frame
    vec4 st = texelFetch(prevFrame, ivec2(1, 1), 0);
    bool valid = abs(st.r - MAGIC) < 0.002;
    float Tprev = valid ? st.g : T; // fresh session: empty birth window
    float dT = T - Tprev;
    bool reset = !valid || (dT < -0.0005) || (dT > 0.15); // scrub back / jump / re-init
    if (reset) {
        Tprev = T;
        dT = 0.0;
    }

    ivec2 fcI = ivec2(gl_FragCoord.xy);

    // persistent state texel (read by this node and by ThanosSnapFast)
    if (fcI == ivec2(1, 1)) {
        out_color = vec4(MAGIC, T, Tprev, EW);
        return;
    }

    // dust is simulated in the top-left quadrant at half resolution
    vec2 halfRes = iResolution * 0.5;
    if (float(fcI.x) >= halfRes.x || float(fcI.y) >= halfRes.y) {
        out_color = vec4(0.0);
        return;
    }

    vec2 sp = vec2(fcI) + 0.5; // half-res sim pixel center
    vec2 fp = sp * 2.0; // corresponding full-res pixel center

    // ---- birth threshold at this cell
    vec2 mapTexSize = vec2(textureSize(dissolveMap, 0));
    vec2 inTexSize = vec2(textureSize(inputTex, 0));
    mat3 inMat = inputTexMat * mat3(1.0 / inTexSize.x, 0.0, 0.0,
        0.0, 1.0 / inTexSize.y, 0.0,
        0.0, 0.0, 1.0);
    vec4 srcO = texture(inputTex, (vec3(fp, 1.0) * inMat).xy);
    float lumO = dot(srcO.rgb, vec3(0.299, 0.587, 0.114));
    float m0 = texture(dissolveMap, fp / mapTexSize).r;
    m0 = clamp(m0 + lumaBias * (lumO - 0.5) * 0.5, 0.0, 1.0);

    // ---- advect last frame's dust field (premultiplied color, opacity in alpha)
    vec3 dustRGB = vec3(0.0);
    float dustA = 0.0;
    if (!reset && T > 0.0) {
        float windRad = windAngle * PI / 180.0;
        vec2 windDir = vec2(cos(windRad), sin(windRad));
        vec2 perp = vec2(-windDir.y, windDir.x);
        float travel = windStrength * iResolution.y; // full-res px flown over the grain lifetime

        vec2 adv = windDir * (travel * dT * timeScale / MAX_AGE) * 0.5; // in half-res px
        adv += perp * sin(tAnim * 0.8) * (drift * 1.2); // global sway
        adv += (vec2(hash21(sp + fract(iFrame * 0.618) * 7.31),
                     hash21(sp.yx + fract(iFrame * 0.381) * 3.77))
            - 0.5) * (swirl * 1.2); // per-cell diffusion jitter

        vec2 suv = (sp - adv) / halfRes;
        vec4 prev = (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0)
            ? vec4(0.0)
            : texture(prevFrame, suv);

        if (prev.a > 0.003) {
            float decay = 0.9 * dT * timeScale / MAX_AGE; // linear lifetime fade
            float op2 = prev.a - decay;
            if (op2 > 0.003) {
                dustRGB = prev.rgb * (op2 / prev.a);
                dustA = op2;
            }
        }
    }

    // ---- births: the frontier crossed this cell this frame
    if (dT > 0.0 && m0 <= T && m0 > Tprev && dustA <= 0.0) {
        float h = hash21(sp * 0.7311);
        if (h <= density && srcO.a > 0.05) {
            dustRGB = srcO.rgb; // exact birth-place color
            dustA = 1.0;
        }
    }

    out_color = vec4(dustRGB, dustA); // premultiplied dust
}
