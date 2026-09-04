#version 320 es
// @author AuroraGraph
// @category Casino
// @description Raymarched 3D European roulette wheel. Self-playing croupier loop: spin, ball drop with bounces, winner highlight in gold. Fully procedural, no input texture.
// @revision 2026-09-03 00:00:00

precision highp float;

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform float iFrame;
uniform vec2 iResolution;
uniform float spinSpeed;   // Speed of the roulette cycle @minmax{ 0.25, 3.0 } @init { 1.0 }
uniform float cameraYaw;   // Camera azimuth around the wheel @minmax{ -3.14, 3.14 } @init { 0.65 }
uniform float cameraPitch; // Camera elevation above the wheel @minmax{ 0.05, 1.35 } @init { 0.45 }
uniform float exposure;    // Scene exposure @minmax{ 0.4, 3.0 } @init { 1.0 }
uniform float goldTint;    // Gold trim warmth @minmax{ 0.0, 1.0 } @init { 0.85 }

const float PI = 3.14159265359;
const float STEP_ANG = 2.0 * PI / 37.0;
const float CYCLE = 16.0;

// European wheel order: pocket i (at angle i*STEP in wheel frame) shows WHEEL[i]
const int WHEEL[37] = int[37](
    0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10,
    5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26);

// red pockets by number
const bool RED[37] = bool[37](
    false,                                         // 0
    true, false, true, false, true, false, true, false, true, false,  // 1-10
    false, true, false, true, false, true, false, true, true, false, // 11-20
    true, false, true, false, true, false, true, false, false, true, // 21-30
    false, true, false, true, false, true);        // 31-36

// ---------------------------------------------------------------- animation state
float gWheelAngle;
float gBallAngle;
float gBallR;
float gBallY;
float gWinner;
float gDim;
int gWinIdx;

float hash1(float n) { return fract(sin(n) * 43758.5453123); }
float easeOutCubic(float t) { float u = 1.0 - clamp(t, 0.0, 1.0); return 1.0 - u * u * u; }
float easeInOutCubic(float t)
{
    t = clamp(t, 0.0, 1.0);
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) * 0.5;
}

float ballRestY(float r)
{
    // track surface (rim top) at +0.095, pocket floor at -0.035
    if (r > 0.62)
        return mix(-0.035, 0.095, smoothstep(0.62, 0.865, r));
    return -0.035;
}

void updateState(float t)
{
    float cycle = floor(t / CYCLE);
    float ct = t - cycle * CYCLE;
    float wA0 = hash1(cycle * 7.13 + 1.7) * 2.0 * PI;
    float bA0 = hash1(cycle * 3.71 + 9.2) * 2.0 * PI;
    int kPrev = int(hash1(cycle * 5.31 + 4.4) * 37.0);
    int kWin = int(hash1(cycle * 9.77 + 2.9) * 37.0);

    gDim = 0.25 + 0.75 * smoothstep(0.0, 0.55, ct) * smoothstep(CYCLE, CYCLE - 0.55, ct);
    gWinner = 0.0;

    if (ct < 2.0) {
        // idle: ball rests in the previous pocket
        gWheelAngle = wA0 + 0.22 * ct;
        gBallR = 0.51;
        gBallAngle = gWheelAngle + float(kPrev) * STEP_ANG;
    } else if (ct < 8.0) {
        // main spin: wheel one way, ball counter-rotates on the outer track
        float u = (ct - 2.0) / 6.0;
        gWheelAngle = wA0 + 0.22 * ct + 12.0 * easeInOutCubic(u);
        gBallR = 0.895;
        gBallAngle = bA0 - (4.0 * u + 16.0 * easeOutCubic(u));
    } else if (ct < 11.0) {
        // ball drop: spirals in over the frets, rattles, gets captured
        float u = (ct - 8.0) / 3.0;
        float wAt8 = wA0 + 0.22 * 8.0 + 12.0;
        gWheelAngle = wAt8 + 0.34 * (ct - 8.0) + 1.6 * easeOutCubic(u);
        float wAt11 = wAt8 + 0.34 * 3.0 + 1.6;
        float bAt8 = bA0 - 20.0;
        float target = wAt11 + float(kWin) * STEP_ANG;
        float uu = smoothstep(0.0, 1.0, u);
        gBallAngle = mix(bAt8, target, uu) + sin(u * 24.0) * (1.0 - u) * 0.07;
        gBallR = mix(0.895, 0.51, uu) + abs(sin(u * 12.0)) * (1.0 - u) * (1.0 - u) * 0.12;
        if (u > 0.97)
            gBallR = 0.51;
    } else {
        // result: ball rides the winning pocket, gold highlight pulses
        float wAt8 = wA0 + 0.22 * 8.0 + 12.0;
        float wAt11 = wAt8 + 0.34 * 3.0 + 1.6;
        gWheelAngle = wAt11 + 0.30 * (ct - 11.0);
        gBallR = 0.51;
        gBallAngle = gWheelAngle + float(kWin) * STEP_ANG;
        gWinner = smoothstep(11.0, 11.5, ct) * (0.55 + 0.45 * sin(ct * 6.0)) * smoothstep(CYCLE - 0.4, CYCLE - 1.6, ct);
    }

    gWinIdx = (ct < 8.0) ? kPrev : kWin;
    gBallY = ballRestY(gBallR) + 0.048;
}

// ---------------------------------------------------------------- sdf primitives
float sdCylY(vec3 p, float r, float h)
{
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCylX(vec3 p, float r, float h)
{
    vec2 d = abs(vec2(length(p.yz), p.x)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdRing(vec3 p, float rIn, float rOut, float h)
{
    vec2 d = abs(vec2(length(p.xz) - (rIn + rOut) * 0.5, p.y)) - vec2((rOut - rIn) * 0.5, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdTorus(vec3 p, float R, float r)
{
    vec2 q = vec2(length(p.xz) - R, p.y);
    return length(q) - r;
}

float sdCone(vec3 p, float h, float r1, float r2)
{
    vec2 q = vec2(length(p.xz), p.y);
    vec2 k1 = vec2(r2, h);
    vec2 k2 = vec2(r2 - r1, 2.0 * h);
    vec2 ca = vec2(q.x - min(q.x, (q.y < 0.0) ? r1 : r2), abs(q.y) - h);
    vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot(k2, k2), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}

float sdBox(vec3 p, vec3 b)
{
    vec3 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

vec2 rot2(vec2 v, float a)
{
    float c = cos(a), s = sin(a);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

// ---------------------------------------------------------------- scene
// materials: 1 wood rim, 2 gold, 3 sectors, 4 hub metal, 5 ball, 6 table felt
vec2 map(vec3 p)
{
    vec2 res = vec2(1e5, 0.0);
    float d;

    // table
    d = p.y + 0.62;
    if (d < res.x)
        res = vec2(d, 6.0);

    // static wooden rim with gold trim rings
    d = sdRing(p, 0.86, 1.04, 0.095);
    if (d < res.x)
        res = vec2(d, 1.0);
    d = sdTorus(p - vec3(0.0, 0.078, 0.0), 1.048, 0.020);
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdTorus(p - vec3(0.0, 0.078, 0.0), 0.862, 0.016);
    if (d < res.x)
        res = vec2(d, 2.0);

    // rotating wheel head frame
    vec3 q = p;
    q.xz = rot2(q.xz, -gWheelAngle);

    // number band + pocket band (colored sectors)
    d = sdCylY(q - vec3(0.0, -0.0375, 0.0), 0.855, 0.0575);
    if (d < res.x)
        res = vec2(d, 3.0);
    d = sdCylY(q - vec3(0.0, -0.065, 0.0), 0.625, 0.030);
    if (d < res.x)
        res = vec2(d, 3.0);

    // hub
    d = sdCylY(q - vec3(0.0, -0.02, 0.0), 0.40, 0.075);
    if (d < res.x)
        res = vec2(d, 4.0);
    d = sdCone(q - vec3(0.0, 0.07, 0.0), 0.065, 0.40, 0.14);
    if (d < res.x)
        res = vec2(d, 2.0);
    d = length(q - vec3(0.0, 0.155, 0.0)) - 0.095;
    if (d < res.x)
        res = vec2(d, 2.0);

    // hub bolts (angular repetition)
    {
        float a = atan(q.z, q.x);
        float b = (floor(a / (PI / 6.0) + 0.5)) * (PI / 6.0);
        vec3 bp = vec3(cos(b) * 0.315, 0.066, sin(b) * 0.315);
        d = length(q - bp) - 0.021;
        if (d < res.x)
            res = vec2(d, 2.0);
    }

    // gold frets between pockets (angular repetition at pocket boundaries)
    {
        float a = atan(q.z, q.x);
        float r = length(q.xz);
        float cb = a - floor(a / STEP_ANG + 0.5) * STEP_ANG;
        vec3 fq = vec3(r - 0.5125, cb * r, q.y + 0.020);
        d = length(max(abs(fq) - vec3(0.1125, 0.008, 0.027), 0.0)) - 0.004;
        if (d < res.x)
            res = vec2(d, 2.0);
    }

    // ball
    vec3 bp = vec3(cos(gBallAngle) * gBallR, gBallY, sin(gBallAngle) * gBallR);
    d = length(p - bp) - 0.050;
    if (d < res.x)
        res = vec2(d, 5.0);

    return res;
}

// ---------------------------------------------------------------- lighting helpers
vec3 calcNormal(vec3 p)
{
    const float h = 0.0013;
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * h).x + k.yyx * map(p + k.yyx * h).x
        + k.yxy * map(p + k.yxy * h).x + k.xxx * map(p + k.xxx * h).x);
}

float calcAO(vec3 p, vec3 n)
{
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 4; i++) {
        float h = 0.02 + 0.12 * float(i);
        occ += (h - map(p + n * h).x) * sca;
        sca *= 0.70;
    }
    return clamp(1.0 - 1.8 * occ, 0.0, 1.0);
}

float softShadow(vec3 ro, vec3 rd)
{
    float res = 1.0, t = 0.02;
    for (int i = 0; i < 24; i++) {
        float h = map(ro + rd * t).x;
        res = min(res, 11.0 * h / t);
        t += clamp(h, 0.012, 0.16);
        if (res < 0.012 || t > 4.0)
            break;
    }
    return clamp(res, 0.0, 1.0);
}

vec3 envMap(vec3 d)
{
    float h = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix(vec3(0.010, 0.008, 0.012), vec3(0.042, 0.038, 0.052), h);
    c += vec3(1.25, 0.92, 0.58) * pow(clamp(dot(d, normalize(vec3(0.50, 0.62, 0.42))), 0.0, 1.0), 22.0);
    c += vec3(0.45, 0.55, 1.00) * pow(clamp(dot(d, normalize(vec3(-0.62, 0.42, -0.32))), 0.0, 1.0), 16.0) * 0.5;
    return c;
}

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// ---------------------------------------------------------------- shading
vec3 sectorColor(vec3 pos)
{
    vec3 q = pos;
    q.xz = rot2(q.xz, -gWheelAngle);
    float r = length(q.xz);
    float ang = atan(q.z, q.x);
    int k = int(mod(floor(ang / STEP_ANG + 0.5), 37.0));
    int n = WHEEL[k];
    vec3 gold = vec3(1.0, 0.72, 0.28) * (0.55 + 0.45 * goldTint);

    vec3 col;
    if (n == 0)
        col = vec3(0.03, 0.38, 0.14);
    else if (RED[n])
        col = vec3(0.60, 0.065, 0.085);
    else
        col = vec3(0.050, 0.055, 0.065);

    if (r > 0.625) {
        // number band: gold separators between pockets
        float ca = abs(fract(ang / STEP_ANG + 0.5) - 0.5) * STEP_ANG;
        float w = fwidth(ang) * 1.5 + 1e-4;
        float sep = 1.0 - smoothstep(0.008 - w, 0.008 + w, ca);
        col = mix(col, gold * 0.85, sep * 0.8);
    } else {
        // pocket band: darker recess
        col *= 0.42;
    }

    if (k == gWinIdx && gWinner > 0.001) {
        col = mix(col, gold * 1.5, gWinner * 0.6);
    }
    return col;
}

vec3 shade(vec3 p, vec3 n, vec3 rd, float matId, float t)
{
    vec3 gold = vec3(1.0, 0.72, 0.28) * (0.55 + 0.45 * goldTint);
    vec3 albedo;
    float specStr = 0.15, shin = 16.0, emis = 0.0;
    bool chrome = false;

    if (matId < 1.5) { // wooden rim
        float r = length(p.xz);
        float grain = 0.85 + 0.15 * sin(r * 46.0 + 3.0 * sin(atan(p.z, p.x) * 7.0));
        albedo = vec3(0.34, 0.19, 0.09) * grain;
        specStr = 0.25; shin = 20.0;
        if (n.y > 0.6 && r > 0.862 && r < 0.948) { // ball track
            albedo *= 0.34;
            shin = 60.0; specStr = 0.5;
        }
    } else if (matId < 2.5) { // gold
        albedo = gold;
        specStr = 1.5; shin = 72.0;
    } else if (matId < 3.5) { // sectors
        albedo = sectorColor(p);
        specStr = 0.55; shin = 42.0;
        if (gWinner > 0.001)
            emis = gWinner * 0.10;
    } else if (matId < 4.5) { // hub metal
        albedo = vec3(0.17, 0.155, 0.14);
        specStr = 0.4; shin = 26.0;
    } else if (matId < 5.5) { // chrome ball
        chrome = true;
        albedo = vec3(0.04);
    } else { // table felt
        float r = length(p.xz);
        albedo = vec3(0.025, 0.135, 0.070);
        if (r > 1.05 && r < 1.55) { // wooden surround
            float grain = 0.8 + 0.2 * sin(r * 60.0);
            albedo = vec3(0.16, 0.09, 0.045) * grain;
        }
        albedo *= 0.35 + 0.65 * smoothstep(2.6, 0.7, r); // light pool
        specStr = 0.06; shin = 8.0;
    }

    vec3 lig = normalize(vec3(0.52, 0.82, 0.34));
    float occ = calcAO(p, n);
    float sha = softShadow(p + n * 0.012, lig);
    float dif = clamp(dot(n, lig), 0.0, 1.0) * sha;
    float sky = clamp(0.5 + 0.5 * n.y, 0.0, 1.0);
    float ind = clamp(dot(n, normalize(vec3(-0.45, 0.25, -0.55))), 0.0, 1.0);
    float fre = pow(clamp(1.0 + dot(n, rd), 0.0, 1.0), 4.0);
    vec3 hal = normalize(lig - rd);
    float spe = pow(clamp(dot(n, hal), 0.0, 1.0), shin) * dif * (0.2 + 0.8 * fre);

    vec3 lin = vec3(0.0);
    lin += 2.5 * dif * vec3(1.28, 1.08, 0.86) * gDim;
    lin += 0.55 * sky * vec3(0.16, 0.21, 0.28) * occ * gDim;
    lin += 0.35 * ind * vec3(0.32, 0.26, 0.22) * occ * gDim;
    lin += 1.1 * fre * vec3(0.34, 0.31, 0.29) * occ * gDim;

    vec3 col = albedo * lin;
    col += specStr * spe * vec3(1.7, 1.55, 1.30) * gDim * 2.2;

    if (chrome) {
        vec3 refl = reflect(rd, n);
        col = envMap(refl) * occ * gDim * 1.15;
        col += spe * vec3(1.8, 1.7, 1.5) * gDim;
        col += gWinner * gold * 0.55;
    }

    col += albedo * emis * gDim;
    // depth fog into the dark hall
    col = mix(col, vec3(0.010, 0.008, 0.013), 1.0 - exp(-0.035 * t * t));
    return col;
}

// ---------------------------------------------------------------- main
void main()
{
    // iFrame ticks every rendered frame (iTime is timeline-bound and stays 0 while paused)
    float tScene = iFrame * 0.0166667;
    // the display path flips the RT vertically - pre-flip so the image appears upright
    vec2 fragCoord = vec2(uv_coords.x, 1.0 - uv_coords.y) * iResolution;
    updateState(tScene * spinSpeed);

    vec2 pp = (2.0 * fragCoord - iResolution) / iResolution.y;
    float yaw = cameraYaw + 0.06 * sin(tScene * 0.31);
    float pit = clamp(cameraPitch + 0.022 * sin(tScene * 0.23), 0.03, 1.45);
    vec3 ro = vec3(sin(yaw) * cos(pit), sin(pit), cos(yaw) * cos(pit)) * 2.95;
    ro.y += 0.05;
    vec3 ta = vec3(0.0, -0.06, 0.0);
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(vec3(0.0, 1.0, 0.0), ww));
    vec3 vv = cross(ww, uu);
    vec3 rd = normalize(pp.x * uu + pp.y * vv + 1.95 * ww);

    // march (with bounding sphere acceleration)
    float tHit = -1.0;
    float matId = 0.0;
    {
        // bounding sphere around the wheel
        vec3 oc = ro - vec3(0.0, 0.05, 0.0);
        float b = dot(oc, rd);
        float c = dot(oc, oc) - 1.55 * 1.55;
        float hDisc = b * b - c;
        bool hitWheel = hDisc > 0.0;
        bool needTable = rd.y < -0.001;
        if (hitWheel || needTable) {
            float t = 0.02;
            float tEnd = 12.0;
            if (hitWheel && hDisc > 0.0)
                tEnd = -b + sqrt(hDisc) + 0.1;
            if (needTable) {
                // table plane can be hit beyond the bounding sphere
                float tp = (-0.62 - ro.y) / rd.y;
                tEnd = max(tEnd, tp + 0.05);
            }
            for (int i = 0; i < 90; i++) {
                vec3 pos = ro + rd * t;
                vec2 h = map(pos);
                if (abs(h.x) < 0.0009 * t) {
                    tHit = t;
                    matId = h.y;
                    break;
                }
                t += h.x;
                if (t > tEnd)
                    break;
            }
        }
    }

    vec3 col;
    if (tHit > 0.0) {
        vec3 pos = ro + rd * tHit;
        vec3 n = calcNormal(pos);
        col = shade(pos, n, rd, matId, tHit);
    } else {
        col = envMap(rd);
    }

    // vignette + grain + grade
    vec2 q = fragCoord / iResolution;
    col *= 1.0 - 0.36 * pow(length((q - 0.5) * vec2(1.35, 1.0)), 2.3);
    col *= exposure;
    col = aces(col);
    col = pow(col, vec3(0.4545));
    col += (hash1(dot(fragCoord, vec2(12.9898, 78.233)) + fract(tScene) * 37.0) - 0.5) * 0.012;

    out_color = vec4(col, 1.0);
}
