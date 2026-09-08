#version 320 es
// @author AuroraGraph
// @category Transition
// @description Full-resolution composite for the feedback-optimized Thanos-snap ablation (pair with ThanosDustSim). Reads the dust field and the dissolve threshold T from ThanosDustSim's buffer, keeps the crisp image mask and ember frontier at full res, and adds the upsampled colored dust. Animated progress lives on the ThanosDustSim node only.
// @revision 2026-09-08 00:00:00

precision highp float;

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform sampler2D dustTex; // @typeTexture from ThanosDustSim
uniform sampler2D inputTex; // @typeTexture
uniform mat3 inputTexMat; // @imagemat
uniform sampler2D dissolveMap; // @typeTexture same TurbulenceMap as the sim
uniform vec2 iResolution;

uniform float lumaBias; // keep equal to ThanosDustSim's @minmax{ -1.0, 1.0 } @init { 0.0 }
uniform float edgeGlow; // Frontier glow intensity @minmax{ 0.0, 3.0 } @init { 1.2 }

void main()
{
    vec2 uv = uv_coords.xy;
    vec2 px = uv * iResolution;

    // ---- dissolve state from the sim's buffer (T and frontier width)
    vec4 st = texelFetch(dustTex, ivec2(1, 1), 0);
    bool valid = abs(st.r - 0.4815) < 0.002;
    float T = valid ? st.g : 0.0;
    float ew = valid ? st.a : 0.04;

    vec2 inTexSize = vec2(textureSize(inputTex, 0));
    mat3 inMat = inputTexMat * mat3(1.0 / inTexSize.x, 0.0, 0.0,
        0.0, 1.0 / inTexSize.y, 0.0,
        0.0, 0.0, 1.0);
    vec2 mapTexSize = vec2(textureSize(dissolveMap, 0));

    vec4 src = texture(inputTex, (vec3(px, 1.0) * inMat).xy);
    float lum = dot(src.rgb, vec3(0.299, 0.587, 0.114));
    float m = texture(dissolveMap, px / mapTexSize).r;
    m = clamp(m + lumaBias * (lum - 0.5) * 0.5, 0.0, 1.0);

    // ---- remaining image + frontier glow
    float kept = smoothstep(T - ew, T + ew, m);
    vec3 col = src.rgb * kept;
    float alpha = src.a * kept;

    float band = 1.0 - clamp(abs(m - T) / ew, 0.0, 1.0);
    band *= band;
    vec3 ember = vec3(1.0, 0.55, 0.25);
    col += mix(ember, vec3(1.0, 0.9, 0.75), band * 0.35) * band * edgeGlow * kept;
    alpha = max(alpha, band * edgeGlow * 0.12);

    // ---- dust from the sim's half-res quadrant (premultiplied color)
    vec4 dust = texture(dustTex, uv * 0.5);
    col += dust.rgb;
    alpha = max(alpha, dust.a);

    out_color = vec4(col, alpha);
}
