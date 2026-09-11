#version 320 es
// @author AuroraGraph
// @category Distortion
// @description Rotation compensation for a hinged display panel (hinge on the left edge). The panel content is reprojected so a viewer looking straight at the panel's unfolded virtual position sees the texture undistorted, as if the screen were transparent and never rotated. At angleDeg 0 the output is an exact passthrough. Set resolution and dpi to the physical panel, viewDist to the eye distance in inches.
// @revision 2026-09-10 00:00:00

precision highp float;

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform sampler2D inputTex; // @typeTexture
uniform mat3 inputTexMat; // @imagemat
uniform vec2 iResolution;

uniform float angleDeg; // Panel rotation around the hinge in degrees @minmax{ 0.0, 180.0 } @init { 0.0 }
uniform float dpi; // Panel pixels per inch @minmax{ 50.0, 1000.0 } @init { 460.0 }
uniform vec2 resolution; // Physical panel resolution in pixels
uniform float viewDist; // Eye distance from the virtual plane in inches @minmax{ 4.0, 60.0 } @init { 14.0 }
uniform float eyeOffsetX; // Eye horizontal offset in inches @minmax{ -10.0, 10.0 } @init { 0.0 }
uniform vec4 uvRect; // Texture region x0,y0,x1,y1
uniform float brightnessComp; // Radiometric compensation strength @minmax{ 0.0, 1.0 } @init { 1.0 }
uniform float edgeFade; // Fade out-of-projection rays to black @minmax{ 0.0, 1.0 } @init { 1.0 }
uniform float smoothStretch; // Smooth stretched sampling at grazing angles @minmax{ 0.0, 1.0 } @init { 1.0 }

const float PI = 3.14159265359;

void main()
{
    vec2 uv = uv_coords.xy;

    // input sampling matrix (pixel space -> uv), hoisted
    vec2 texSize = vec2(textureSize(inputTex, 0));
    mat3 inMat = inputTexMat * mat3(1.0 / texSize.x, 0.0, 0.0,
        0.0, 1.0 / texSize.y, 0.0,
        0.0, 0.0, 1.0);

    // physical panel dimensions (inches)
    float W = resolution.x / dpi;
    float H = resolution.y / dpi;

    // panel-local physical coordinates, hinge along x = 0 (left edge)
    float xp = uv.x * W;
    float yp = uv.y * H;

    float th = radians(clamp(angleDeg, 0.0, 180.0));
    float sn = sin(th);
    float cs = cos(th);

    // rotated physical position of this panel pixel
    vec3 P = vec3(xp * cs, yp, xp * sn);

    // eye: straight-on to the virtual (unfolded) panel position
    float D = viewDist;
    vec3 E = vec3(W * 0.5 + eyeOffsetX, H * 0.5, D);

    // ray E -> P against the virtual plane z = 0
    float denom = D - P.z;
    float t = D / max(denom, 1e-4);
    vec3 V = E + t * (P - E);

    // virtual hit -> texture coordinates through uvRect
    vec2 vuv = V.xy / vec2(W, H);
    vec2 tuv = uvRect.xy + vuv * (uvRect.zw - uvRect.xy);

    // validity + smooth edge fade (pixel at eye depth, ray outside the texture)
    float fade = smoothstep(0.0, 0.25, denom);
    float m = 0.035;
    fade *= smoothstep(0.0, m, vuv.x) * (1.0 - smoothstep(1.0 - m, 1.0, vuv.x));
    fade *= smoothstep(0.0, m, vuv.y) * (1.0 - smoothstep(1.0 - m, 1.0, vuv.y));
    fade = mix(1.0, fade, edgeFade);
    fade = max(fade, 0.0); // keep the flow uniform so derivatives stay defined

    // analytic jacobian of the projection (virtual area per panel area)
    float dtdxp = sn * t * t / D;
    float dVxdxp = t * cs + (P.x - E.x) * dtdxp;
    float dVydxp = (P.y - E.y) * dtdxp;
    float dVydyp = t;
    float J = abs(dVxdxp * dVydyp);

    // radiometric compensation: match apparent brightness of the virtual plane
    // scale = (J * cosVirt / distV^2) / (cosPanel / distP^2), distV = t * distP
    vec3 toEyeP = normalize(E - P);
    vec3 nPanel = vec3(-sn, 0.0, cs);
    float cosPanel = max(abs(dot(nPanel, toEyeP)), 1e-3);
    vec3 dirV = normalize(V - E);
    float cosVirt = abs(dirV.z);
    float scale = J * cosVirt / (cosPanel * t * t);
    scale = mix(1.0, clamp(scale, 0.25, 4.0), brightnessComp);

    // sampling with adaptive smoothing along the stretch direction
    vec2 dx = dFdx(tuv);
    vec2 dy = dFdy(tuv);
    float maxSpan = max(texSize.x, texSize.y);
    float stretchPx = max(length(dx), length(dy)) * maxSpan;

    vec2 sdir = (length(dx) >= length(dy)) ? dx : dy;
    float slen = length(sdir);
    vec3 col;
    if (smoothStretch > 0.5 && stretchPx > 1.5 && slen > 1e-6) {
        vec2 dir = sdir / slen;
        float stepLen = min(stretchPx, 4.0) / maxSpan;
        vec3 c0 = texture(inputTex, (vec3(tuv * texSize, 1.0) * inMat).xy).rgb;
        vec3 c1 = texture(inputTex, (vec3((tuv + dir * stepLen * 0.5) * texSize, 1.0) * inMat).xy).rgb;
        vec3 c2 = texture(inputTex, (vec3((tuv - dir * stepLen * 0.5) * texSize, 1.0) * inMat).xy).rgb;
        col = (c0 + c1 + c2) / 3.0;
    } else {
        col = texture(inputTex, (vec3(tuv * texSize, 1.0) * inMat).xy).rgb;
    }

    col *= scale * fade;
    out_color = vec4(col, 1.0);
}
