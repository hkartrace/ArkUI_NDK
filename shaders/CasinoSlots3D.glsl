#version 320 es
// @author AuroraGraph
// @category Casino
// @description Raymarched 3D slot machine with a mouse-pullable lever, persistent balance and win payouts. Wire the node output back to its inputTex pin to keep state between frames (previous-frame feedback), then drag the lever knob down in the Composition preview and release to spin. Without feedback the shader runs a self-playing demo.
// @revision 2026-09-04 00:00:00

precision highp float;

layout(location = 0) in highp vec4 uv_coords;
layout(location = 0) out highp vec4 out_color;

uniform float iFrame;
uniform vec2 iResolution;
uniform vec4 iMouse;         // xy = current mouse (image px, top-left origin), zw = previous frame
uniform sampler2D inputTex;  // node's own previous frame (self-loop feedback)
uniform float spinSpeed;     // Reel animation speed @minmax{ 0.25, 3.0 } @init { 1.0 }
uniform float cameraYaw;     // Camera azimuth around the cabinet @minmax{ -1.6, 1.6 } @init { 0.28 }
uniform float cameraPitch;   // Camera elevation @minmax{ 0.02, 0.9 } @init { 0.17 }
uniform float exposure;      // Scene exposure @minmax{ 0.4, 3.0 } @init { 1.0 }
uniform float marqueeGlow;   // Marquee and lamp intensity @minmax{ 0.0, 2.0 } @init { 1.0 }
uniform float bet;           // Bet per lever pull @minmax{ 1.0, 500.0 } @init { 25.0 }

const float PI = 3.14159265359;
const float CELL_ARC = PI / 3.0; // 6 cells per reel revolution
const float FRONT = PI / 2.0; // window center angle on reel
const float MAGIC = 0.7311;

// ---------------------------------------------------------------- game state (persisted via feedback pixels)
int gPhase; // 0 idle, 1 spinning, 2 result
float gPhaseT;
float gSeed;
int gBalance;
int gLastWin;
int gMult;
float gWinT;
float gLever;
bool gGrabbed;
float gGrabY; // normalized by height
bool gDemo;
float gDim;
float gFlash;
float gJack;
float gSpinBtn;

float gReelAng[3];
float gReelBlur[3];

float hash1(float n) { return fract(sin(n) * 43758.5453123); }
float reelHash(int k, int reel) { return fract(sin(float(k) * 127.1 + float(reel) * 311.7) * 43758.5453); }

// weighted strip: cherry, lemon, bell, bar, diamond, seven
int symOf(float h)
{
    if (h < 0.27)
        return 0;
    if (h < 0.50)
        return 1;
    if (h < 0.68)
        return 2;
    if (h < 0.84)
        return 3;
    if (h < 0.95)
        return 4;
    return 5;
}

int kStarOf(float seed, int reel)
{
    return int(hash1(seed * 13.71 + 7.7 * float(reel) + 3.1) * 83.0);
}

float reelStopT2(float seed) // last reel stop time incl. near-miss delay
{
    float t = 6.5;
    if (symOf(reelHash(kStarOf(seed, 0), 0)) == symOf(reelHash(kStarOf(seed, 1), 1)))
        t += 1.0;
    return t;
}

void computeReels(int phase, float phaseT, float seed)
{
    float stopT[3];
    stopT[0] = 4.9;
    stopT[1] = 5.7;
    stopT[2] = reelStopT2(seed);
    for (int i = 0; i < 3; i++) {
        int k = kStarOf(seed, i);
        float th0 = hash1(seed * 3.37 + 1.91 * float(i)) * 2.0 * PI;
        float dth = mod(FRONT - float(k) * CELL_ARC - th0, 2.0 * PI);
        dth += 2.0 * PI * (4.0 + float(i));
        float startT = 1.3 + 0.22 * float(i);

        float ang = th0;
        float blur = 0.0;
        if (phase == 1) {
            if (phaseT < startT) {
                ang = th0;
            } else if (phaseT < stopT[i]) {
                float u = (phaseT - startT) / (stopT[i] - startT);
                float m = 1.0 - pow(1.0 - u, 2.6);
                ang = th0 + dth * m;
                blur = dth * 2.6 * pow(1.0 - u, 1.6) / (stopT[i] - startT) * 0.006;
            } else {
                ang = th0 + dth;
                float w = phaseT - stopT[i];
                ang += 0.05 * sin(clamp(w * 9.0, 0.0, PI)) * exp(-w * 5.0); // settle wobble
            }
        } else {
            ang = th0 + dth; // resting
        }
        gReelAng[i] = ang;
        gReelBlur[i] = blur;
    }
}

int evalMult(float seed)
{
    int s0 = symOf(reelHash(kStarOf(seed, 0), 0));
    int s1 = symOf(reelHash(kStarOf(seed, 1), 1));
    int s2 = symOf(reelHash(kStarOf(seed, 2), 2));
    if (s0 == s1 && s1 == s2) {
        const int M[6] = int[6](10, 8, 25, 20, 50, 150);
        return M[s0];
    }
    int cherries = (s0 == 0 ? 1 : 0) + (s1 == 0 ? 1 : 0) + (s2 == 0 ? 1 : 0);
    if (cherries == 2)
        return 2;
    return 0;
}

int winSym(float seed)
{
    return symOf(reelHash(kStarOf(seed, 0), 0));
}

// ---------------------------------------------------------------- feedback state io
void decodeState()
{
    vec4 a = texelFetch(inputTex, ivec2(1, 1), 0);
    vec4 b = texelFetch(inputTex, ivec2(2, 1), 0);
    vec4 c = texelFetch(inputTex, ivec2(3, 1), 0);

    if (abs(a.r - MAGIC) > 0.002) {
        // first frame / not wired / resized - fresh session in demo mode
        gPhase = 0;
        gPhaseT = 0.0;
        gSeed = 0.137;
        gBalance = 1000;
        gLastWin = 0;
        gMult = 0;
        gWinT = 0.0;
        gLever = 0.0;
        gGrabbed = false;
        gGrabY = 0.0;
        gDemo = true;
        return;
    }

    float v = a.g * 4.0;
    gPhase = int(min(floor(v + 0.03), 2.0));
    gPhaseT = clamp(v - floor(v + 0.03), 0.0, 1.0) * 16.0;
    gLever = clamp(a.b, 0.0, 1.0);
    gGrabbed = a.a > 0.25;
    gGrabY = clamp((a.a - 0.5) / 0.4995, 0.0, 1.0);

    int v16 = int(round(b.r * 255.0)) * 256 + int(round(b.g * 255.0));
    gBalance = v16 - 32768;
    gSeed = round(b.b * 255.0) / 255.0;
    gLastWin = int(round(b.a * 1023.0)) * 64;
    gMult = int(round(c.r * 255.0));
    gWinT = clamp(c.g * 8.0, 0.0, 8.0);
    gDemo = c.b > 0.5;
}

vec4 encodeStateA()
{
    float grabPack = gGrabbed ? 0.5 + 0.4995 * clamp(gGrabY, 0.0, 1.0) : 0.0;
    return vec4(MAGIC, (float(gPhase) + clamp(gPhaseT, 0.0, 15.99) / 16.0) / 4.0, gLever, grabPack);
}

vec4 encodeStateB()
{
    int v16 = clamp(gBalance + 32768, 0, 65535);
    float hi = float(v16 / 256) / 255.0;
    float lo = float(v16 - (v16 / 256) * 256) / 255.0;
    float seedQ = floor(clamp(gSeed, 0.0, 0.999) * 255.0) / 255.0;
    float winQ = float(clamp(gLastWin, 0, 65472) / 64) / 1023.0;
    return vec4(hi, lo, seedQ, winQ);
}

vec4 encodeStateC()
{
    return vec4(float(clamp(gMult, 0, 255)) / 255.0, clamp(gWinT, 0.0, 8.0) / 8.0, gDemo ? 1.0 : 0.0, 0.0);
}

// ---------------------------------------------------------------- sdf primitives
float sdBox(vec3 p, vec3 b)
{
    vec3 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 b, float r) { return sdBox(p, b) - r; }

float sdCylX(vec3 p, float r, float h)
{
    vec2 d = abs(vec2(length(p.yz), p.x)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCylZ(vec3 p, float r, float h)
{
    vec2 d = abs(vec2(length(p.xy), p.z)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r)
{
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

vec2 rot2(vec2 v, float a)
{
    float c = cos(a), s = sin(a);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

vec3 leverKnobPos()
{
    vec3 pivot = vec3(1.14, 1.38, 0.06);
    float a = mix(0.32, 1.45, gLever);
    vec3 dir = normalize(vec3(0.12, cos(a), sin(a)));
    return pivot + 0.55 * dir;
}

// ---------------------------------------------------------------- scene
// materials: 1 cabinet, 2 gold/chrome, 3-5 reels, 6 floor, 7 marquee, 8 lamps, 9 button, 10 plinth, 11 knob
vec2 map(vec3 p)
{
    vec2 res = vec2(1e5, 0.0);
    float d;

    d = p.y;
    if (d < res.x)
        res = vec2(d, 6.0);

    float body = sdRoundBox(p - vec3(0.0, 1.32, 0.0), vec3(1.02, 1.32, 0.40), 0.06);
    float cut = sdRoundBox(p - vec3(0.0, 1.66, 0.30), vec3(0.86, 0.52, 0.30), 0.04);
    d = max(body, -cut);
    if (d < res.x)
        res = vec2(d, 1.0);

    d = sdRoundBox(p - vec3(0.0, 0.05, 0.0), vec3(1.10, 0.05, 0.44), 0.03);
    if (d < res.x)
        res = vec2(d, 10.0);

    d = sdRoundBox(p - vec3(0.0, 2.19, 0.43), vec3(0.95, 0.030, 0.025), 0.015);
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdRoundBox(p - vec3(0.0, 1.13, 0.43), vec3(0.95, 0.030, 0.025), 0.015);
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdRoundBox(p - vec3(-0.90, 1.66, 0.43), vec3(0.030, 0.55, 0.025), 0.015);
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdRoundBox(p - vec3(0.90, 1.66, 0.43), vec3(0.030, 0.55, 0.025), 0.015);
    if (d < res.x)
        res = vec2(d, 2.0);

    d = sdRoundBox(p - vec3(0.0, 2.50, 0.36), vec3(0.80, 0.17, 0.05), 0.05);
    if (d < res.x)
        res = vec2(d, 7.0);

    for (int i = 0; i < 7; i++) {
        vec3 lp = vec3(-0.72 + 0.24 * float(i), 2.50, 0.43);
        d = length(p - lp) - 0.040;
        if (d < res.x)
            res = vec2(d, 8.0);
    }

    for (int i = 0; i < 3; i++) {
        vec3 rp = p - vec3(-0.56 + 0.56 * float(i), 1.66, 0.02);
        d = sdCylX(rp, 0.36, 0.24);
        if (d < res.x)
            res = vec2(d, 3.0 + float(i));
    }

    d = sdBox(p - vec3(0.0, 1.66, 0.415), vec3(0.90, 0.006, 0.012));
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdBox(p - vec3(-0.865, 1.66, 0.44), vec3(0.028, 0.034, 0.02));
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdBox(p - vec3(0.865, 1.66, 0.44), vec3(0.028, 0.034, 0.02));
    if (d < res.x)
        res = vec2(d, 2.0);

    d = sdBox(p - vec3(-0.35, 0.82, 0.415), vec3(0.26, 0.09, 0.012));
    if (d < res.x)
        res = vec2(d, 2.0);
    d = sdCylZ(p - vec3(0.45, 0.82, 0.42), 0.085, 0.030);
    if (d < res.x)
        res = vec2(d, 9.0);

    // lever: mount bracket, chrome rod, red knob
    d = sdRoundBox(p - vec3(1.10, 1.40, 0.04), vec3(0.05, 0.10, 0.13), 0.03);
    if (d < res.x)
        res = vec2(d, 2.0);
    {
        vec3 knob = leverKnobPos();
        d = sdCapsule(p, vec3(1.14, 1.38, 0.06), knob - normalize(knob - vec3(1.14, 1.38, 0.06)) * 0.10, 0.028);
        if (d < res.x)
            res = vec2(d, 2.0);
        d = length(p - knob) - 0.105;
        if (d < res.x)
            res = vec2(d, 11.0);
    }

    return res;
}

// ---------------------------------------------------------------- 2d symbol sdf
float sdBox2(vec2 p, vec2 b)
{
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdSeg(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

float sdEllipse(vec2 p, vec2 r)
{
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

float symSDF(vec2 s, int sym)
{
    if (sym == 0) { // cherry
        float d1 = length(s - vec2(-0.26, 0.08)) - 0.23;
        float d2 = length(s - vec2(0.24, 0.16)) - 0.23;
        float stem = min(sdSeg(s, vec2(0.08, 0.58), vec2(-0.26, 0.08)),
            sdSeg(s, vec2(0.08, 0.58), vec2(0.24, 0.16))) - 0.035;
        float leaf = sdEllipse(s - vec2(0.36, 0.62), vec2(0.19, 0.085));
        return min(min(d1, d2), min(stem, leaf));
    } else if (sym == 1) { // lemon
        return sdEllipse(rot2(s, 0.42), vec2(0.52, 0.35));
    } else if (sym == 2) { // bell
        float dome = max(length(s - vec2(0.0, 0.12)) - 0.33, s.y - 0.28);
        float skirt = sdBox2(s - vec2(0.0, 0.22), vec2(0.28, 0.13));
        float rim = sdBox2(s - vec2(0.0, -0.26), vec2(0.35, 0.055)) - 0.02;
        float clap = length(s - vec2(0.0, -0.40)) - 0.075;
        return min(min(min(dome, skirt), rim), clap);
    } else if (sym == 3) { // BAR plate
        return sdBox2(s, vec2(0.52, 0.25)) - 0.06;
    } else if (sym == 4) { // diamond kite
        vec2 a = abs(s);
        return (a.x / 0.40 + a.y / 0.66 - 1.0) * 0.40;
    }
    // seven: top bar + diagonal
    float top = sdBox2(s - vec2(0.02, 0.33), vec2(0.33, 0.075)) - 0.03;
    vec2 sq = rot2(s - vec2(0.06, -0.03), -0.55);
    float diag = sdBox2(sq, vec2(0.38, 0.075)) - 0.03;
    return min(top, diag);
}

vec3 symColor(int sym, vec2 s)
{
    if (sym == 0)
        return vec3(0.80, 0.07, 0.10);
    if (sym == 1)
        return vec3(0.93, 0.76, 0.12);
    if (sym == 2)
        return vec3(0.92, 0.71, 0.16);
    if (sym == 3) { // BAR: dark plate + white stripes
        float bar1 = sdBox2(s - vec2(0.0, 0.09), vec2(0.36, 0.055));
        float bar2 = sdBox2(s - vec2(0.0, -0.09), vec2(0.36, 0.055));
        float m = 1.0 - smoothstep(0.0, 0.03, min(bar1, bar2));
        return mix(vec3(0.08, 0.08, 0.11), vec3(0.93, 0.91, 0.85), m);
    }
    if (sym == 4)
        return vec3(0.18, 0.70, 0.92);
    return vec3(0.86, 0.09, 0.11);
}

// ---------------------------------------------------------------- reel shading
vec3 cellColor(vec3 rq, int reel, float angOff)
{
    float a = atan(rq.z, rq.y) - gReelAng[reel] + angOff;
    float w = a / CELL_ARC + 0.5;
    float k = floor(w);
    float v = fract(w) - 0.5;
    int sym = symOf(reelHash(int(k), reel));

    vec2 s = vec2(rq.x / 0.24 * 0.72, v * 2.55);
    float d = symSDF(s, sym);
    float aa = max(fwidth(d) * 1.2, 0.014);
    float m = 1.0 - smoothstep(-aa, aa, d);

    vec3 col = vec3(0.92, 0.885, 0.83); // ivory
    col = mix(col, symColor(sym, s), m);
    col *= 1.0 - 0.22 * (1.0 - smoothstep(0.02, 0.12, abs(d))); // emboss ring

    // winning front symbol glow
    float aFront = atan(rq.z, rq.y) - gReelAng[reel];
    float dFront = abs(mod(aFront - FRONT + PI, 2.0 * PI) - PI);
    float inFront = 1.0 - smoothstep(CELL_ARC * 0.42, CELL_ARC * 0.60, dFront);
    col += gFlash * vec3(1.0, 0.75, 0.25) * m * inFront;
    return col;
}

vec3 reelSurf(vec3 rq, int reel)
{
    vec3 c = cellColor(rq, reel, 0.0);
    float blur = gReelBlur[reel];
    if (blur > 0.0004) {
        c = (c + cellColor(rq, reel, blur) + cellColor(rq, reel, -blur)) / 3.0;
    }
    return c;
}

// ---------------------------------------------------------------- lighting helpers
vec3 calcNormal(vec3 p)
{
    const float h = 0.0015;
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * h).x + k.yyx * map(p + k.yyx * h).x
        + k.yxy * map(p + k.yxy * h).x + k.xxx * map(p + k.xxx * h).x);
}

float calcAO(vec3 p, vec3 n)
{
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 4; i++) {
        float h = 0.02 + 0.13 * float(i);
        occ += (h - map(p + n * h).x) * sca;
        sca *= 0.70;
    }
    return clamp(1.0 - 1.8 * occ, 0.0, 1.0);
}

float softShadow(vec3 ro, vec3 rd)
{
    float res = 1.0, t = 0.03;
    for (int i = 0; i < 22; i++) {
        float h = map(ro + rd * t).x;
        res = min(res, 10.0 * h / t);
        t += clamp(h, 0.015, 0.18);
        if (res < 0.012 || t > 5.0)
            break;
    }
    return clamp(res, 0.0, 1.0);
}

vec3 envMap(vec3 d)
{
    float h = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix(vec3(0.008, 0.006, 0.010), vec3(0.038, 0.033, 0.046), h);
    c += vec3(1.2, 0.85, 0.5) * pow(clamp(dot(d, normalize(vec3(0.35, 0.72, 0.55))), 0.0, 1.0), 20.0);
    c += vec3(0.5, 0.35, 0.6) * pow(clamp(dot(d, normalize(vec3(-0.6, 0.5, -0.4))), 0.0, 1.0), 14.0) * 0.4;
    return c;
}

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// ---------------------------------------------------------------- 7-segment hud font
float segBox(vec2 p, vec2 c, vec2 hs) { return sdBox2(p - c, hs) - 0.025; }

float d7(vec2 p, int ch)
{
    // segment bits: a=64 b=32 c=16 d=8 e=4 f=2 g=1
    int mask = 0;
    if (ch == 0) mask = 126;
    else if (ch == 1) mask = 48;
    else if (ch == 2) mask = 109;
    else if (ch == 3) mask = 121;
    else if (ch == 4) mask = 51;
    else if (ch == 5) mask = 91;
    else if (ch == 6) mask = 95;
    else if (ch == 7) mask = 112;
    else if (ch == 8) mask = 127;
    else if (ch == 9) mask = 123;
    else if (ch == 10) mask = 1; // '-'
    else if (ch == 11) mask = 91; // '$' = 5 + center bar

    float d = 1e5;
    if ((mask & 64) != 0)
        d = min(d, segBox(p, vec2(0.0, 0.50), vec2(0.26, 0.045)));
    if ((mask & 32) != 0)
        d = min(d, segBox(p, vec2(0.26, 0.25), vec2(0.045, 0.22)));
    if ((mask & 16) != 0)
        d = min(d, segBox(p, vec2(0.26, -0.25), vec2(0.045, 0.22)));
    if ((mask & 8) != 0)
        d = min(d, segBox(p, vec2(0.0, -0.50), vec2(0.26, 0.045)));
    if ((mask & 4) != 0)
        d = min(d, segBox(p, vec2(-0.26, -0.25), vec2(0.045, 0.22)));
    if ((mask & 2) != 0)
        d = min(d, segBox(p, vec2(-0.26, 0.25), vec2(0.045, 0.22)));
    if ((mask & 1) != 0)
        d = min(d, segBox(p, vec2(0.0, 0.0), vec2(0.26, 0.045)));
    if (ch == 11)
        d = min(d, sdBox2(p, vec2(0.022, 0.60))); // $ vertical stroke
    return d;
}

int pow10i(int n)
{
    int v = 1;
    for (int j = 0; j < n; j++)
        v *= 10;
    return v;
}

// draws an amount ("$123" / "$-45") right-aligned at anchor; returns glow contribution for this pixel
vec3 drawAmount(vec2 fc, int amount, vec2 anchor, float h, vec3 col)
{
    vec3 acc = vec3(0.0);
    int av = abs(amount);
    int slots = 1;
    int n = av;
    while (n >= 10) {
        n /= 10;
        slots++;
    }
    if (amount < 0)
        slots++; // minus sign

    float step = 0.78 * h;
    for (int i = 0; i < 8; i++) {
        if (i >= slots + 1)
            break; // +1 for '$'
        int ch;
        if (i == 0)
            ch = 11; // '$'
        else {
            int idx = i - 1;
            if (amount < 0 && idx == slots - 1)
                ch = 10; // minus occupies the leftmost slot
            else
                ch = (av / pow10i(idx)) % 10;
        }
        vec2 c = anchor - vec2(float(i) * step, 0.0);
        vec2 p = (fc - c) / (h * 0.5);
        if (abs(p.x) < 1.6 && abs(p.y) < 1.6) {
            float d = d7(p, ch);
            float m = 1.0 - smoothstep(-0.08, 0.08, d);
            float glow = 1.0 - smoothstep(-0.45, 0.55, d);
            acc += col * m + col * 0.30 * glow;
        }
    }
    return acc;
}

// small down arrow hint (pull the lever)
vec3 drawPullHint(vec2 fc, vec2 c, float t)
{
    vec2 p = fc - c;
    float bounce = sin(t * 3.1) * 0.24;
    p.y -= bounce * 26.0;
    float tri = max(abs(p.x) - (14.0 - p.y * 0.42), p.y - 12.0);
    tri = max(tri, -p.y - 16.0);
    float stem = max(abs(p.x) - 2.5, abs(p.y + 12.0) - 8.0);
    float d = min(tri, stem);
    float m = 1.0 - smoothstep(-1.2, 1.2, d);
    float a = clamp(2.2 - t * 0.0, 0.0, 1.0) * (0.55 + 0.45 * sin(t * 3.1));
    return vec3(1.0, 0.82, 0.4) * m * a;
}

// ---------------------------------------------------------------- shading
vec3 shade(vec3 p, vec3 n, vec3 rd, float matId, float t, float tScene)
{
    vec3 gold = vec3(1.0, 0.72, 0.28);
    vec3 albedo;
    float specStr = 0.15, shin = 16.0;
    vec3 emis = vec3(0.0);

    if (matId < 1.5) { // cabinet lacquer
        albedo = vec3(0.30, 0.07, 0.095);
        specStr = 0.45; shin = 34.0;
        if (p.z < 0.39 && p.y > 1.10 && p.y < 2.22 && abs(p.x) < 0.88) {
            albedo *= 0.30;
            specStr = 0.1;
        }
    } else if (matId < 2.5) { // gold / chrome
        albedo = gold;
        specStr = 1.5; shin = 72.0;
    } else if (matId < 5.5) { // reels
        int reel = int(matId - 3.0);
        albedo = reelSurf(p - vec3(-0.56 + 0.56 * float(reel), 1.66, 0.02), reel);
        specStr = 0.22; shin = 18.0;
    } else if (matId < 6.5) { // floor carpet
        float r = length(p.xz);
        albedo = vec3(0.085, 0.016, 0.032);
        albedo *= 0.30 + 0.70 * smoothstep(3.2, 0.6, r);
        specStr = 0.05; shin = 6.0;
    } else if (matId < 7.5) { // marquee panel
        albedo = gold * 0.35;
        float pulse = 0.75 + 0.25 * sin(tScene * 2.1);
        emis = vec3(1.0, 0.78, 0.42) * (0.9 * pulse + 1.6 * gJack) * marqueeGlow;
        specStr = 0.2;
    } else if (matId < 8.5) { // lamps
        int idx = int(floor(p.x / 0.24 + 3.5));
        float chase = (mod(float(idx) + floor(tScene * 7.0), 3.0) < 1.0) ? 1.0 : 0.0;
        float strobe = (gFlash > 0.001 || gJack > 0.001) ? ((floor(tScene * 13.0) < 1.0) ? 1.0 : 0.0) : 0.0;
        float lit = max(chase, strobe);
        albedo = mix(vec3(0.25, 0.18, 0.08), gold, lit);
        emis = vec3(1.0, 0.82, 0.5) * lit * 2.2 * marqueeGlow;
        specStr = 0.3;
    } else if (matId < 9.5) { // spin button
        albedo = vec3(0.55, 0.06, 0.07);
        emis = vec3(0.95, 0.10, 0.10) * (0.25 + 0.30 * gSpinBtn);
        specStr = 0.5; shin = 40.0;
    } else if (matId < 10.5) { // plinth
        albedo = vec3(0.10, 0.09, 0.085);
        specStr = 0.3; shin = 24.0;
    } else { // lever knob
        albedo = vec3(0.62, 0.05, 0.06);
        specStr = 1.3; shin = 64.0;
        emis = vec3(0.9, 0.12, 0.10) * gLever * 0.35;
    }

    vec3 lig = normalize(vec3(0.40, 0.85, 0.45));
    float occ = calcAO(p, n);
    float sha = softShadow(p + n * 0.015, lig);
    float dif = clamp(dot(n, lig), 0.0, 1.0) * sha;
    float sky = clamp(0.5 + 0.5 * n.y, 0.0, 1.0);
    float ind = clamp(dot(n, normalize(vec3(-0.5, 0.2, -0.6))), 0.0, 1.0);
    float fre = pow(clamp(1.0 + dot(n, rd), 0.0, 1.0), 4.0);
    vec3 hal = normalize(lig - rd);
    float spe = pow(clamp(dot(n, hal), 0.0, 1.0), shin) * dif * (0.2 + 0.8 * fre);

    vec3 lin = vec3(0.0);
    lin += 2.4 * dif * vec3(1.25, 1.05, 0.85) * gDim;
    lin += 0.50 * sky * vec3(0.15, 0.20, 0.27) * occ * gDim;
    lin += 0.30 * ind * vec3(0.30, 0.24, 0.28) * occ * gDim;
    lin += 1.0 * fre * vec3(0.32, 0.29, 0.30) * occ * gDim;

    vec3 col = albedo * lin;
    col += specStr * spe * vec3(1.7, 1.55, 1.30) * gDim * 2.2;
    col += emis * gDim;

    col = mix(col, vec3(0.008, 0.006, 0.011), 1.0 - exp(-0.028 * t * t));
    return col;
}

// ---------------------------------------------------------------- main
void main()
{
    float tScene = iFrame * 0.0166667;
    vec2 fragCoord = vec2(uv_coords.x, 1.0 - uv_coords.y) * iResolution; // y-up, matches display

    // camera
    vec2 pp0 = vec2(0.0);
    float yaw = cameraYaw + 0.05 * sin(tScene * 0.27);
    float pit = clamp(cameraPitch + 0.018 * sin(tScene * 0.21), 0.02, 1.2);
    vec3 ro = vec3(sin(yaw) * cos(pit), 1.45 + sin(pit), cos(yaw) * cos(pit)) * 4.35;
    vec3 ta = vec3(0.0, 1.42, 0.0);
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(vec3(0.0, 1.0, 0.0), ww));
    vec3 vv = cross(ww, uu);

    decodeState();

    float dt = 1.0 / 60.0;

    if (gDemo) {
        // self-playing showcase driven by frame time
        float ct = mod(tScene * spinSpeed, 13.0);
        float cycle = floor(tScene * spinSpeed / 13.0);
        gSeed = hash1(cycle * 5.31 + 4.4);
        float spinEnd = reelStopT2(gSeed) + 0.6;
        if (ct < 1.3) {
            gPhase = 0;
            gPhaseT = 0.0;
        } else if (ct < spinEnd) {
            gPhase = 1;
            gPhaseT = ct - 1.3;
        } else if (ct < 12.4) {
            gPhase = 2;
            gPhaseT = ct - spinEnd;
            gMult = evalMult(gSeed);
        } else {
            gPhase = 0;
            gPhaseT = 0.0;
            gMult = 0;
        }
        gDim = 0.25 + 0.75 * smoothstep(0.0, 0.5, ct) * smoothstep(13.0, 12.5, ct);
        gWinT = (gPhase == 2) ? (2.8 - gPhaseT) : 0.0;
    } else {
        // interactive session: advance timers
        float dtG = dt * spinSpeed;
        if (gPhase == 1) {
            gPhaseT += dtG;
            if (gPhaseT >= reelStopT2(gSeed) + 0.6) {
                gPhase = 2;
                gPhaseT = 0.0;
                gMult = evalMult(gSeed);
                if (gMult > 0) {
                    gLastWin = int(bet + 0.5) * gMult;
                    gBalance += gLastWin; // balance may go negative on losses - by design
                    gWinT = 2.8;
                } else {
                    gLastWin = 0;
                    gWinT = 0.0;
                }
            }
        } else if (gPhase == 2) {
            gPhaseT += dtG;
            gWinT = max(gWinT - dtG, 0.0);
            if (gPhaseT > 2.6) {
                gPhase = 0;
                gPhaseT = 0.0;
            }
        }
        gDim = 0.97 + 0.03 * sin(tScene * 7.3);
    }

    // ---- lever interaction (iMouse updates only while LMB is held in the preview)
    {
        vec2 mouseGL = vec2(iMouse.x, iResolution.y - iMouse.y);
        bool mouseHeld = (abs(iMouse.x - iMouse.z) + abs(iMouse.y - iMouse.w)) > 0.01;

        // project knob to screen (same camera basis / focal as the ray setup)
        vec3 kv = leverKnobPos() - ro;
        float kz = dot(kv, ww);
        vec2 ks = vec2(dot(kv, uu), dot(kv, vv)) / kz * 1.85;
        vec2 knobScreen = vec2(ks.x * iResolution.y * 0.5 + iResolution.x * 0.5,
            ks.y * iResolution.y * 0.5 + iResolution.y * 0.5);

        float grabRadius = max(38.0, iResolution.y * 0.055);
        if (!gGrabbed && mouseHeld && length(mouseGL - knobScreen) < grabRadius) {
            gGrabbed = true;
            gGrabY = clamp(mouseGL.y / iResolution.y, 0.0, 1.0);
        }
        if (gGrabbed) {
            if (mouseHeld) {
                float grabYpx = gGrabY * iResolution.y;
                gLever = clamp((grabYpx - mouseGL.y) / (0.30 * iResolution.y), 0.0, 1.0);
            } else {
                // released
                gGrabbed = false;
                if (gLever > 0.7 && gPhase == 0) {
                    gPhase = 1;
                    gPhaseT = 0.0;
                    gSeed = hash1(iFrame * 0.01713 + 3.7);
                    gBalance -= int(bet + 0.5);
                    gLastWin = 0;
                    gMult = 0;
                    gWinT = 0.0;
                    gDemo = false; // pulling the lever ends demo mode
                }
            }
        }
        if (!gGrabbed && gPhase == 0)
            gLever = max(gLever - dt * 3.5, 0.0); // spring back

        gSpinBtn = 0.5 + 0.5 * sin(tScene * 2.6);
    }

    // win glow / jackpot strobe
    if (gMult > 0 && gPhase == 2 && gWinT > 0.0) {
        float env = smoothstep(0.0, 0.25, 2.8 - gWinT) * smoothstep(0.0, 0.4, gWinT);
        gFlash = (0.55 + 0.45 * sin(tScene * 11.0)) * env;
        if (winSym(gSeed) == 5)
            gJack = (0.5 + 0.5 * sin(tScene * 16.0)) * env;
    } else {
        gFlash = 0.0;
        gJack = 0.0;
    }

    computeReels(gPhase, gPhaseT, gSeed);

    // ---- raymarch
    vec2 pp = (2.0 * fragCoord - iResolution) / iResolution.y;
    vec3 rd = normalize(pp.x * uu + pp.y * vv + 1.85 * ww);

    float tHit = -1.0;
    float matId = 0.0;
    {
        vec3 oc = ro - vec3(0.0, 1.34, 0.0);
        float b = dot(oc, rd);
        float c = dot(oc, oc) - 1.80 * 1.80;
        float hDisc = b * b - c;
        bool hitCab = hDisc > 0.0;
        bool needFloor = rd.y < -0.001;
        if (hitCab || needFloor) {
            float t = 0.02;
            float tEnd = 14.0;
            if (hitCab)
                tEnd = -b + sqrt(hDisc) + 0.1;
            if (needFloor) {
                float tp = (0.0 - ro.y) / rd.y;
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
        col = shade(pos, n, rd, matId, tHit, tScene);
    } else {
        col = envMap(rd);
    }

    // ---- HUD (balance / bet / win)
    {
        float hudFade = smoothstep(iResolution.y - 92.0, iResolution.y - 10.0, fragCoord.y);
        col *= mix(1.0, 0.42, hudFade * 0.75);

        vec3 balCol = (gBalance < 0) ? vec3(1.0, 0.32, 0.28) : vec3(1.0, 0.80, 0.35);
        float dH = 21.0;
        col += drawAmount(fragCoord, gBalance, vec2(iResolution.x * 0.5 + 0.62 * dH, iResolution.y - 30.0), dH, balCol);

        // bet (right) and last win (left), smaller
        float dS = 13.0;
        col += drawAmount(fragCoord, int(bet + 0.5),
            vec2(iResolution.x - 24.0 + 0.62 * dS, iResolution.y - 66.0), dS, vec3(0.75, 0.85, 0.95));
        if (gLastWin > 0 && gWinT > 0.0) {
            vec3 winCol = vec3(0.5, 1.0, 0.6) * (0.7 + 0.3 * sin(tScene * 10.0));
            col += drawAmount(fragCoord, gLastWin,
                vec2(iResolution.x * 0.32 + 0.62 * dS, iResolution.y - 66.0), dS, winCol);
        }

        // pull hint arrow near the knob when idle
        if (gPhase == 0 && !gGrabbed) {
            vec3 kv = leverKnobPos() - ro;
            float kz2 = dot(kv, ww);
            vec2 ks2 = vec2(dot(kv, uu), dot(kv, vv)) / kz2 * 1.85;
            vec2 knobS = vec2(ks2.x * iResolution.y * 0.5 + iResolution.x * 0.5,
                ks2.y * iResolution.y * 0.5 + iResolution.y * 0.5);
            col += drawPullHint(fragCoord, knobS + vec2(0.0, 46.0), tScene);
        }
    }

    // ---- grade
    vec2 q = fragCoord / iResolution;
    col *= 1.0 - 0.36 * pow(length((q - 0.5) * vec2(1.35, 1.0)), 2.3);
    col *= exposure;
    col = aces(col);
    col = pow(col, vec3(0.4545));
    col += (hash1(dot(fragCoord, vec2(12.9898, 78.233)) + fract(tScene) * 41.0) - 0.5) * 0.012;

    // ---- write state texels (hidden in the bottom-left corner of the texture)
    ivec2 fcI = ivec2(gl_FragCoord.xy);
    if (fcI == ivec2(1, 1)) {
        out_color = encodeStateA();
    } else if (fcI == ivec2(2, 1)) {
        out_color = encodeStateB();
    } else if (fcI == ivec2(3, 1)) {
        out_color = encodeStateC();
    } else {
        out_color = vec4(col, 1.0);
    }
}
