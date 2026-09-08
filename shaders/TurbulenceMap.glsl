#version 320 es
// @author AuroraGraph
// @category Pattern
// @description Domain-warped fractal turbulence noise. Connect its output to other effects as a map input (e.g. the ThanosSnap dissolveMap pin).
// @revision 2026-09-04 00:00:00

precision highp float;

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform float iFrame;
uniform vec2 iResolution;
uniform float scale;    // Noise zoom @minmax{ 0.5, 16.0 } @init { 3.0 }
uniform float warp;     // Domain warp amount @minmax{ 0.0, 3.0 } @init { 0.6 }
uniform float contrast; // Contrast @minmax{ 0.5, 3.0 } @init { 1.2 }
uniform float speed;    // Animation speed, 0 = static @minmax{ 0.0, 2.0 } @init { 0.0 }

float hash21(vec2 p)
{
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float v = 0.0, a = 0.5;
    mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < 5; i++) {
        v += a * vnoise(p);
        p = rot * p * 2.03 + 11.7;
        a *= 0.5;
    }
    return v;
}

void main()
{
    float aspect = iResolution.x / iResolution.y;
    float t = iFrame * 0.0166667 * speed;
    vec2 p = vec2(uv_coords.x * aspect, uv_coords.y) * scale;
    p += vec2(t * 0.15, -t * 0.06);

    // two-level domain warp
    vec2 q = vec2(fbm(p + 0.10 * t), fbm(p + vec2(5.2, 1.3) - 0.07 * t));
    vec2 r = vec2(fbm(p + warp * q + vec2(1.7, 9.2) + 0.15 * t),
        fbm(p + warp * q + vec2(8.3, 2.8) - 0.126 * t));
    float f = fbm(p + warp * r);

    f = clamp((f - 0.5) * contrast + 0.5, 0.0, 1.0);
    out_color = vec4(vec3(f), 1.0);
}
