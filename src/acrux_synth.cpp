#include "acrux_synth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr int SR = ACRUX_AUDIO_SAMPLE_RATE;
constexpr float BPM = 62.0f;
constexpr float BEAT = 60.0f / BPM;
constexpr int TOTAL_BEATS = 64;
constexpr int LOOP_FRAMES = (int)(TOTAL_BEATS * BEAT * SR + 0.5f);
constexpr float TWO_PI = 6.2831853071795864769f;
constexpr int MAX_SFX_VOICES = 4;
constexpr int SEASON_MORPH_FRAMES = (int)(16.0f * BEAT * SR + 0.5f);
constexpr float EPILOGUE_OVERLAY_GAIN = 1.65f;

struct SfxData { int16_t* pcm; int frames; };
struct Voice { int id; int pos; float pan; unsigned serial; };

static int16_t* g_music[4] = {};
static SfxData g_sfx[ACRUX_SFX_COUNT] = {};
static Voice g_voice[MAX_SFX_VOICES] = {};
static float g_sine[256];
static float g_midi[128];
static int g_relay = 0;
static int g_songFrame = 0;
static float g_musicGain = 0.62f;
static float g_pathHum = 0.0f;
static uint32_t g_humPhase = 0;
static unsigned g_serial = 1;
static bool g_epilogue = false;
static int g_seasonFrom = -1;
static int g_seasonTo = ACRUX_SEASON_WINTER;
static int g_seasonMorphFrame = SEASON_MORPH_FRAMES;

static float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
static float midiHz(int n) { return g_midi[clampi(n,0,127)]; }
static int frameAtBeat(float beat) { return (int)(beat * BEAT * SR + 0.5f); }

static float envAt(int i, int n, int a, int d, float s, int r) {
    if (i < a) return a > 0 ? (float)i / (float)a : 1.0f;
    if (i < a + d) {
        float q = d > 0 ? (float)(i-a)/(float)d : 1.0f;
        return 1.0f + (s-1.0f)*q;
    }
    if (i < n-r) return s;
    if (r <= 0) return 0.0f;
    float q = (float)(i-(n-r))/(float)r;
    return s * (1.0f-q);
}

enum WaveKind { W_SINE=0, W_TRI=1, W_SQUARE=2 };

static void addToneHz(float* dst, int dstFrames, int start, int frames,
                      float f0, float f1, WaveKind wave, float amp,
                      float attackSec, float decaySec, float sustain, float releaseSec) {
    if (!dst || frames <= 0 || start >= dstFrames) return;
    if (start < 0) { frames += start; start = 0; }
    if (start + frames > dstFrames) frames = dstFrames-start;
    if (frames <= 0) return;
    int a=(int)(attackSec*SR), d=(int)(decaySec*SR), r=(int)(releaseSec*SR);
    uint32_t phase=0;
    for (int i=0;i<frames;i++) {
        float u = frames>1 ? (float)i/(float)(frames-1) : 0.0f;
        float f = f0 + (f1-f0)*u;
        uint32_t inc = (uint32_t)(f * (4294967296.0 / (double)SR));
        phase += inc;
        float v;
        if (wave == W_SINE) v = g_sine[phase >> 24];
        else if (wave == W_SQUARE) v = (phase & 0x80000000u) ? -1.0f : 1.0f;
        else {
            float x=(float)(phase >> 16)*(1.0f/65536.0f);
            v=4.0f*(x < 0.5f ? (0.5f-x) : (x-0.5f))-1.0f;
        }
        dst[start+i] += v * amp * envAt(i,frames,a,d,sustain,r);
    }
}

static void addToneMidi(float* dst, int dstFrames, int start, int frames, int midi,
                        float detune, WaveKind wave, float amp,
                        float a, float d, float s, float r) {
    float f=midiHz(midi)*(1.0f+detune);
    addToneHz(dst,dstFrames,start,frames,f,f,wave,amp,a,d,s,r);
}

static void addEpiano(float* b, int n, int start, float durSec, int midi, float amp, bool echo=true) {
    int f=(int)(durSec*SR);
    addToneMidi(b,n,start,f,midi,-0.0015f,W_SINE,amp,.004f,.14f,.16f,.30f);
    addToneMidi(b,n,start,f,midi,+0.0012f,W_SINE,amp*.45f,.004f,.14f,.13f,.30f);
    addToneMidi(b,n,start,f,midi+12,0,W_SINE,amp*.14f,.003f,.07f,.04f,.23f);
    addToneMidi(b,n,start,f,midi+19,0,W_TRI,amp*.028f,.001f,.035f,.01f,.16f);
    addToneMidi(b,n,start,(int)(.10f*SR),midi,0,W_TRI,amp*.09f,.001f,.018f,.02f,.05f);
    if (echo) {
        int es=start+frameAtBeat(.25f);
        addEpiano(b,n,es,durSec*.78f,midi,amp*.07f,false);
    }
}

static void addEpluck(float* b,int n,int start,float durSec,int midi,float amp,bool echo=true) {
    int f=(int)(durSec*SR);
    addToneMidi(b,n,start,f,midi,-.0008f,W_TRI,amp,.001f,.06f,.06f,.14f);
    addToneMidi(b,n,start,f,midi+12,0,W_SINE,amp*.18f,.001f,.04f,.025f,.11f);
    addToneMidi(b,n,start,(int)(.055f*SR),midi+7,0,W_SQUARE,amp*.018f,.0005f,.008f,.006f,.022f);
    if (echo) addEpluck(b,n,start+frameAtBeat(.25f),durSec*.72f,midi,amp*.06f,false);
}

static const uint8_t kProg[32][4] = {
    {45,48,52,59},{41,45,48,52},{48,52,55,59},{43,47,50,57},
    {45,48,52,55},{41,45,52,55},{48,52,57,59},{43,47,52,57},
    {45,48,52,59},{41,48,52,57},{48,52,55,62},{43,47,52,59},
    {45,52,55,59},{41,45,48,55},{48,52,59,64},{43,50,52,57},
    {40,47,52,59},{41,48,52,57},{38,45,50,57},{43,47,52,59},
    {45,48,52,59},{41,45,52,55},{48,52,55,59},{43,47,50,57},
    {45,48,52,59},{41,45,48,52},{48,52,55,59},{43,47,50,57},
    {45,48,52,55},{41,45,52,55},{48,52,57,59},{43,47,52,57}
};
static const uint8_t kRoots[32] = {
    33,29,36,31,33,29,36,31, 33,29,36,31,33,29,36,31,
    28,29,26,31,33,29,36,31, 33,29,36,31,33,29,36,31
};

struct NoteEv { uint8_t halfBeat; uint8_t midi; uint16_t ms; };
static const NoteEv kMelody[] = {
    {1,64,400},{2,59,320},{3,60,400},{9,64,400},{10,67,320},{11,69,520},
    {17,64,380},{18,59,320},{19,60,380},{24,67,380},{25,69,340},{26,71,520},{30,69,640},
    {34,67,360},{36,64,320},{38,60,360},{42,69,380},{44,67,320},{46,64,440},
    {50,67,360},{52,71,340},{54,69,420},{58,72,400},{60,71,380},{62,69,520},
    {69,59,320},{77,60,360},{85,64,360},{92,62,400},
    {97,64,400},{98,59,320},{99,60,400},{105,64,400},{106,67,320},{107,69,520},
    {113,67,400},{114,69,360},{115,71,440},{120,72,420},{122,71,400},{124,69,500}
};

static void renderKeys(float* b, float gain) {
    for (int bar=0;bar<32;bar++) {
        int st=frameAtBeat((float)bar*2.0f);
        for (int k=0;k<4;k++) addEpiano(b,LOOP_FRAMES,st,2.0f*BEAT,kProg[bar][k],.077f*gain,true);
        int note=-1; float off=0;
        if (bar==1||bar==3||bar==5||bar==7) { note=kProg[bar][3]+12; off=1.5f; }
        else if (bar==9||bar==13) { note=kProg[bar][2]+12; off=1.0f; }
        else if (bar==11||bar==15) { note=kProg[bar][2]+12; off=1.5f; }
        else if (bar==18) { note=kProg[bar][1]+12; off=1.0f; }
        else if (bar==22) { note=kProg[bar][1]+12; off=1.5f; }
        else if (bar==25||bar==29) { note=kProg[bar][3]+12; off=1.0f; }
        else if (bar==27) { note=kProg[bar][3]+12; off=1.5f; }
        if (note>=0) addEpiano(b,LOOP_FRAMES,st+frameAtBeat(off),.40f,note,.052f*gain,true);
    }
}

static void renderBass(float* b,float gain) {
    for (int bar=0;bar<32;bar++) {
        int st=frameAtBeat((float)bar*2.0f), root=kRoots[bar];
        addToneMidi(b,LOOP_FRAMES,st,(int)(.54f*SR),root,0,W_TRI,.242f*gain,.004f,.10f,.15f,.27f);
        addToneMidi(b,LOOP_FRAMES,st+frameAtBeat(1),(int)(.34f*SR),root+12,0,W_SINE,.070f*gain,.004f,.07f,.068f,.18f);
        if (bar==2||bar==6||bar==10||bar==14||bar==26||bar==30)
            addToneMidi(b,LOOP_FRAMES,st+frameAtBeat(1.5f),(int)(.30f*SR),root+7,0,W_TRI,.058f*gain,.003f,.06f,.05f,.16f);
    }
}

static void addKick(float* b,int start,float gain) {
    addToneHz(b,LOOP_FRAMES,start,(int)(.23f*SR),90,40,W_SINE,.285f*gain,.001f,.045f,.055f,.135f);
}
static void addRim(float* b,int start,float gain) {
    addToneHz(b,LOOP_FRAMES,start,(int)(.076f*SR),390,235,W_TRI,.066f*gain,.0005f,.012f,.015f,.038f);
    addToneHz(b,LOOP_FRAMES,start,(int)(.090f*SR),175,112,W_SINE,.044f*gain,.0005f,.017f,.017f,.047f);
}
static void renderDrums(float* b,float gain) {
    for (int beat=0;beat<64;beat++) {
        float bg=(beat>=32&&beat<48)?0.75f:1.0f;
        int st=frameAtBeat((float)beat);
        if ((beat&1)==0) addKick(b,st,gain*bg); else addRim(b,st,gain*bg);
        if (beat==2||beat==6||beat==10||beat==14||beat==18||beat==22||beat==26||beat==30||beat==50||beat==54||beat==58||beat==62)
            addToneHz(b,LOOP_FRAMES,st+frameAtBeat(.5f),(int)(.032f*SR),840,630,W_TRI,.014f*gain,.0004f,.006f,.006f,.017f);
    }
    const float sync[] = {23.5f,31.5f,55.5f,63.5f};
    for (float x:sync) addToneHz(b,LOOP_FRAMES,frameAtBeat(x),(int)(.17f*SR),82,39,W_SINE,.125f*gain,.001f,.035f,.04f,.09f);
}

static const uint8_t kArpPat[8][4] = {
    {0,2,1,3},{0,1,2,1},{0,2,3,2},{0,1,3,2},
    {0,2,1,3},{0,1,2,3},{0,2,3,1},{0,1,2,1}
};
static void renderArp(float* b,float gain) {
    for (int bar=0;bar<32;bar++) {
        int st=frameAtBeat((float)bar*2.0f);
        for (int k=0;k<4;k++) {
            if (bar>=16&&bar<24&&k==2) continue;
            int note=kProg[bar][kArpPat[bar&7][k]];
            if (((bar>=8&&bar<16)||(bar>=24&&bar<32)) && k==3 && (bar&1)) note+=12;
            addEpluck(b,LOOP_FRAMES,st+frameAtBeat(.5f*k),.29f,note,.080f*gain,true);
        }
    }
}

static void renderPad(float* b,float gain) {
    for (int bar=0;bar<32;bar+=2) {
        int st=frameAtBeat((float)bar*2.0f);
        int count=(bar>=16&&bar<24)?2:3;
        int notes[3]={kProg[bar][0], kProg[bar][2], kProg[bar][3]};
        if (count==2) { notes[0]=kProg[bar][0]; notes[1]=kProg[bar][1]; }
        for (int k=0;k<count;k++)
            addToneMidi(b,LOOP_FRAMES,st,frameAtBeat(4),notes[k],0,W_SINE,.030f*gain,.20f,.26f,.28f,.60f);
    }
}

static bool relay2MelodyBeat(float beat) {
    return (beat>=0&&beat<2) || (beat>=16&&beat<19) || (beat>=34&&beat<35) || (beat>=48&&beat<50);
}
static void renderMelody(float* b,float gain,bool relay2Only) {
    for (unsigned i=0;i<sizeof(kMelody)/sizeof(kMelody[0]);i++) {
        float beat=0.5f*kMelody[i].halfBeat;
        if (relay2Only && !relay2MelodyBeat(beat)) continue;
        int st=frameAtBeat(beat);
        addEpiano(b,LOOP_FRAMES,st,kMelody[i].ms*.001f,kMelody[i].midi,.102f*gain,true);
        addEpluck(b,LOOP_FRAMES,st,kMelody[i].ms*.00055f,kMelody[i].midi+12,.020f*gain,true);
    }
}

static bool buildMusic() {
    for (int r=0;r<4;r++) {
        float* b=(float*)calloc(LOOP_FRAMES,sizeof(float));
        if (!b) return false;
        renderKeys(b,1.0f);
        renderBass(b,r==0?0.92f:0.90f);
        if (r==1) { renderDrums(b,.36f); renderArp(b,.18f); }
        else if (r==2) { renderDrums(b,.49f); renderArp(b,.43f); renderPad(b,.29f); renderMelody(b,.18f,true); }
        else if (r==3) { renderDrums(b,.61f); renderArp(b,.55f); renderPad(b,.47f); renderMelody(b,.76f,false); }
        float peak=0.0001f;
        for (int i=0;i<LOOP_FRAMES;i++) { float a=fabsf(b[i]); if (a>peak) peak=a; }
        float scale=.82f/peak;
        g_music[r]=(int16_t*)malloc(LOOP_FRAMES*sizeof(int16_t));
        if (!g_music[r]) { free(b); return false; }
        for (int i=0;i<LOOP_FRAMES;i++) {
            float v=clampf(b[i]*scale,-1,1);
            g_music[r][i]=(int16_t)(v*32767.0f);
        }
        free(b);
    }
    return true;
}

static SfxData makeSfx(int id) {
    float dur=1.0f;
    switch(id) {
        case ACRUX_SFX_PICKUP: dur=.35f; break; case ACRUX_SFX_BLINK: dur=.80f; break;
        case ACRUX_SFX_MOTH_ALERT: dur=.65f; break; case ACRUX_SFX_MOTH_CONTACT: dur=.42f; break;
        case ACRUX_SFX_BOAR_ALERT: dur=.72f; break; case ACRUX_SFX_BOAR_CONTACT: dur=.48f; break;
        case ACRUX_SFX_PATH_BREAK: dur=.44f; break; case ACRUX_SFX_REPAIR: dur=.82f; break;
        case ACRUX_SFX_RELAY: dur=1.15f; break; case ACRUX_SFX_STAR: dur=.42f; break;
        case ACRUX_SFX_FAILURE: dur=1.0f; break;
        case ACRUX_SFX_RUSTLE: dur=.52f; break; case ACRUX_SFX_SNARE: dur=.46f; break;
        case ACRUX_SFX_PATH_PLACE: dur=.30f; break; case ACRUX_SFX_PATH_REMOVE: dur=.32f; break;
        case ACRUX_SFX_SLEEP: dur=1.05f; break; case ACRUX_SFX_DAY_TRANSITION: dur=1.35f; break;
    }
    int n=(int)(dur*SR); float* b=(float*)calloc(n,sizeof(float)); SfxData out={};
    if (!b) return out;
    auto tone=[&](float st,float d,float f0,float f1,WaveKind w,float a){
        addToneHz(b,n,(int)(st*SR),(int)(d*SR),f0,f1,w,a,.001f,.035f,.12f,d*.45f);
    };
    if (id==ACRUX_SFX_PICKUP) { tone(0,.18f,392,430,W_TRI,.48f); tone(.055f,.20f,523,560,W_SINE,.30f); }
    else if (id==ACRUX_SFX_BLINK) { tone(0,.52f,82,47,W_SINE,.78f); tone(.02f,.36f,146,92,W_TRI,.30f); tone(.07f,.62f,540,690,W_SINE,.15f); tone(.13f,.44f,810,960,W_SINE,.085f); }
    else if (id==ACRUX_SFX_MOTH_ALERT) {
        for (int k=0;k<13;k++) { float st=.025f*k + .006f*(k%3); tone(st,.055f,static_cast<float>(1280+(k%2)*180),980,W_TRI,.16f); }
        tone(0,.52f,760,820,W_SINE,.08f);
    } else if (id==ACRUX_SFX_MOTH_CONTACT) { tone(0,.23f,1450,760,W_TRI,.38f); tone(.025f,.34f,510,170,W_SINE,.40f); }
    else if (id==ACRUX_SFX_BOAR_ALERT) { tone(0,.28f,72,58,W_TRI,.30f); tone(.22f,.14f,96,50,W_SINE,.60f); tone(.47f,.15f,88,44,W_SINE,.58f); }
    else if (id==ACRUX_SFX_BOAR_CONTACT) { tone(0,.42f,132,38,W_SINE,.88f); tone(.015f,.18f,220,72,W_TRI,.30f); tone(.10f,.24f,64,46,W_SINE,.27f); }
    else if (id==ACRUX_SFX_PATH_BREAK) {
        const float st[]={0,.018f,.043f,.071f,.106f}; const float f0[]={1650,1420,1180,980,810}; const float f1[]={720,610,520,430,350};
        for(int k=0;k<5;k++){ tone(st[k],.038f,f0[k],f1[k],W_TRI,.23f-.025f*k); tone(st[k]+.001f,.020f,f0[k]*1.45f,f1[k]*1.08f,W_SQUARE,.008f); }
        const float cs[]={.135f,.158f,.184f,.214f,.248f,.286f}; const float cf[]={620,530,460,390,330,275};
        for(int k=0;k<6;k++){ tone(cs[k],.030f,cf[k],cf[k]*.76f,W_TRI,.085f-.009f*k); tone(cs[k]+.038f,.025f,cf[k],cf[k]*.80f,W_TRI,.018f); }
        tone(.025f,.22f,210,118,W_SINE,.055f);
    } else if (id==ACRUX_SFX_REPAIR) { tone(0,.10f,180,125,W_TRI,.28f); tone(.105f,.075f,520,410,W_SQUARE,.12f); tone(.15f,.62f,78,156,W_SINE,.38f); tone(.24f,.46f,330,392,W_SINE,.13f); }
    else if (id==ACRUX_SFX_RELAY) { tone(0,.20f,86,54,W_SINE,.52f); tone(.09f,.16f,260,160,W_TRI,.18f); tone(.16f,.86f,110,146,W_SINE,.28f); tone(.29f,.74f,220,220,W_TRI,.12f); tone(.38f,.68f,330,330,W_SINE,.08f); }
    else if (id==ACRUX_SFX_STAR) { tone(0,.22f,659,710,W_TRI,.28f); tone(.045f,.26f,988,988,W_SINE,.16f); }
    else if (id==ACRUX_SFX_FAILURE) { tone(0,.95f,165,37,W_SINE,.58f); tone(0,.70f,330,82,W_TRI,.12f); }
    else if (id==ACRUX_SFX_RUSTLE) {
        // Dry foliage / brush movement without white-noise crackle: irregular short tonal flecks.
        const float st[]={0,.041f,.086f,.151f,.205f,.294f,.362f};
        const float f0[]={1180,930,1360,760,1090,840,1240};
        for(int k=0;k<7;k++){ tone(st[k],.055f,f0[k],f0[k]*.68f,W_TRI,.10f-.006f*k); }
        tone(.06f,.34f,310,245,W_SINE,.055f);
    } else if (id==ACRUX_SFX_SNARE) {
        // Trap catches: light metallic latch + short tension drop.
        tone(0,.075f,980,620,W_TRI,.24f);
        tone(.028f,.095f,620,410,W_SQUARE,.055f);
        tone(.085f,.21f,260,118,W_SINE,.24f);
        tone(.19f,.18f,540,330,W_TRI,.08f);
    } else if (id==ACRUX_SFX_PATH_PLACE) {
        // Physical tile seating only; newly placed PATH is not charged until the next day.
        tone(0,.085f,250,155,W_TRI,.24f);
        tone(.055f,.080f,430,340,W_SINE,.14f);
        tone(.105f,.11f,150,118,W_SINE,.12f);
    } else if (id==ACRUX_SFX_PATH_REMOVE) {
        // Small release/lift, intentionally not an electrical power-down.
        tone(0,.075f,175,265,W_TRI,.18f);
        tone(.060f,.10f,390,520,W_SINE,.13f);
        tone(.135f,.075f,250,195,W_TRI,.09f);
    } else if (id==ACRUX_SFX_SLEEP) {
        // Warm low-power settling gesture: A/E -> lower A, no notification chime.
        tone(0,.72f,220,214,W_SINE,.18f);
        tone(.04f,.66f,165,160,W_TRI,.10f);
        tone(.22f,.78f,110,82,W_SINE,.20f);
    } else if (id==ACRUX_SFX_DAY_TRANSITION) {
        // The extremely short daylight charges the circuit: restrained upward harmonic bloom.
        tone(0,.82f,110,132,W_SINE,.18f);
        tone(.12f,.82f,165,198,W_TRI,.10f);
        tone(.28f,.74f,220,264,W_SINE,.09f);
        tone(.46f,.64f,330,396,W_SINE,.055f);
    }
    float peak=.0001f; for(int i=0;i<n;i++){float a=fabsf(b[i]);if(a>peak)peak=a;} float sc=.88f/peak;
    out.pcm=(int16_t*)malloc(n*sizeof(int16_t)); out.frames=n;
    if (out.pcm) for(int i=0;i<n;i++) out.pcm[i]=(int16_t)(clampf(b[i]*sc,-1,1)*32767.0f);
    free(b); return out;
}

static bool buildSfx() {
    for (int i=0;i<ACRUX_SFX_COUNT;i++) { g_sfx[i]=makeSfx(i); if(!g_sfx[i].pcm) return false; }
    return true;
}
}

bool AcruxSynth_Init() {
    for (int i=0;i<256;i++) g_sine[i]=sinf(TWO_PI*(float)i/256.0f);
    for (int i=0;i<128;i++) g_midi[i]=440.0f*powf(2.0f,((float)i-69.0f)/12.0f);
    memset(g_voice,0,sizeof(g_voice)); for(int i=0;i<MAX_SFX_VOICES;i++) g_voice[i].id=-1;
    if (!buildMusic()) { AcruxSynth_Shutdown(); return false; }
    if (!buildSfx()) { AcruxSynth_Shutdown(); return false; }
    if (!AcruxEpilogue_Init(LOOP_FRAMES)) { AcruxSynth_Shutdown(); return false; }
    g_songFrame=0; g_relay=0; g_serial=1;
    g_epilogue=false; g_seasonFrom=-1; g_seasonTo=ACRUX_SEASON_WINTER;
    g_seasonMorphFrame=SEASON_MORPH_FRAMES;
    return true;
}

void AcruxSynth_Shutdown() {
    AcruxEpilogue_Shutdown();
    for(int r=0;r<4;r++){ free(g_music[r]); g_music[r]=nullptr; }
    for(int i=0;i<ACRUX_SFX_COUNT;i++){ free(g_sfx[i].pcm); g_sfx[i]={}; }
}
void AcruxSynth_SetRelayCount(int r){ g_relay=clampi(r,0,3); }
void AcruxSynth_EnterEpilogue(AcruxSeason firstSeason){
    g_epilogue=true;
    g_seasonFrom=-1;
    g_seasonTo=clampi(static_cast<int>(firstSeason),0,ACRUX_SEASON_COUNT-1);
    g_seasonMorphFrame=0;
}
void AcruxSynth_EnterEpilogueImmediate(AcruxSeason season){
    const int s=clampi(static_cast<int>(season),0,ACRUX_SEASON_COUNT-1);
    g_epilogue=true;
    g_seasonFrom=s;
    g_seasonTo=s;
    g_seasonMorphFrame=SEASON_MORPH_FRAMES;
}
void AcruxSynth_SetEpilogueSeason(AcruxSeason season){
    const int next=clampi(static_cast<int>(season),0,ACRUX_SEASON_COUNT-1);
    if(!g_epilogue){
        g_seasonFrom=next;
        g_seasonTo=next;
        g_seasonMorphFrame=SEASON_MORPH_FRAMES;
        return;
    }
    if(next==g_seasonTo) return;
    g_seasonFrom=g_seasonTo;
    g_seasonTo=next;
    g_seasonMorphFrame=0;
}
void AcruxSynth_SetEpilogueSeasonImmediate(AcruxSeason season){
    const int s=clampi(static_cast<int>(season),0,ACRUX_SEASON_COUNT-1);
    g_seasonFrom=s;
    g_seasonTo=s;
    g_seasonMorphFrame=SEASON_MORPH_FRAMES;
}
void AcruxSynth_ExitEpilogue(){
    g_epilogue=false;
    g_seasonFrom=-1;
    g_seasonMorphFrame=SEASON_MORPH_FRAMES;
}
bool AcruxSynth_IsEpilogue(){ return g_epilogue; }
AcruxSeason AcruxSynth_CurrentSeason(){
    return static_cast<AcruxSeason>(clampi(g_seasonTo,0,ACRUX_SEASON_COUNT-1));
}
void AcruxSynth_ResetPlayback(){
    g_songFrame=0; g_relay=0; g_pathHum=0.0f; g_humPhase=0; g_serial=1;
    g_epilogue=false;
    g_seasonFrom=-1;
    g_seasonTo=ACRUX_SEASON_WINTER;
    g_seasonMorphFrame=SEASON_MORPH_FRAMES;
    for(int i=0;i<MAX_SFX_VOICES;i++){ g_voice[i].id=-1; g_voice[i].pos=0; g_voice[i].pan=0; g_voice[i].serial=0; }
}
void AcruxSynth_SetMusicGain(float g){ g_musicGain=clampf(g,0,1); }
void AcruxSynth_SetPathHum(float a){ g_pathHum=clampf(a,0,1); }
int AcruxSynth_LoopFrames(){ return LOOP_FRAMES; }

void AcruxSynth_PlaySfx(AcruxSfx id,float pan) {
    if(id<0||id>=ACRUX_SFX_COUNT) return;
    int slot=-1; unsigned oldest=0xffffffffu;
    for(int i=0;i<MAX_SFX_VOICES;i++){
        if(g_voice[i].id<0 || g_voice[i].pos>=g_sfx[g_voice[i].id].frames){slot=i;break;}
        if(g_voice[i].serial<oldest){oldest=g_voice[i].serial;slot=i;}
    }
    g_voice[slot].id=id; g_voice[slot].pos=0; g_voice[slot].pan=clampf(pan,-1,1); g_voice[slot].serial=g_serial++;
}

void AcruxSynth_RenderStereo(int16_t* out,int frames) {
    if(!out||frames<=0) return;
    const uint32_t humInc=(uint32_t)(55.0*(4294967296.0/(double)SR));
    for(int i=0;i<frames;i++){
        const int musicSlot=g_epilogue ? 3 : g_relay;
        const float base=g_music[musicSlot] ? g_music[musicSlot][g_songFrame]*(1.0f/32768.0f) : 0.0f;
        float seasonal=0.0f;
        if(g_epilogue){
            float t=g_seasonMorphFrame>=SEASON_MORPH_FRAMES
                ? 1.0f : static_cast<float>(g_seasonMorphFrame)/static_cast<float>(SEASON_MORPH_FRAMES);
            t=t*t*(3.0f-2.0f*t);
            const float a=g_seasonFrom<0 ? 0.0f :
                AcruxEpilogue_Sample(static_cast<AcruxSeason>(g_seasonFrom),g_songFrame)*(1.0f/32768.0f);
            const float b=AcruxEpilogue_Sample(static_cast<AcruxSeason>(g_seasonTo),g_songFrame)*(1.0f/32768.0f);
            seasonal=(a+(b-a)*t)*EPILOGUE_OVERLAY_GAIN;
        }
        float m=(base+seasonal)*g_musicGain;
        float l=m,r=m;
        if(g_pathHum>0){ g_humPhase+=humInc; float h=g_sine[g_humPhase>>24]*(.055f*g_pathHum); l+=h;r+=h; }
        for(int v=0;v<MAX_SFX_VOICES;v++){
            Voice& q=g_voice[v]; if(q.id<0) continue; SfxData& s=g_sfx[q.id];
            if(q.pos>=s.frames){q.id=-1;continue;}
            float x=s.pcm[q.pos++]*(1.0f/32768.0f)*.72f;
            float lg=q.pan>0?1.0f-q.pan:1.0f, rg=q.pan<0?1.0f+q.pan:1.0f;
            l+=x*lg; r+=x*rg;
        }
        l=clampf(l,-1,1); r=clampf(r,-1,1);
        out[i*2+0]=(int16_t)(l*32767.0f); out[i*2+1]=(int16_t)(r*32767.0f);
        if(g_epilogue && g_seasonMorphFrame<SEASON_MORPH_FRAMES){
            if(++g_seasonMorphFrame>=SEASON_MORPH_FRAMES){
                g_seasonMorphFrame=SEASON_MORPH_FRAMES;
                g_seasonFrom=g_seasonTo;
            }
        }
        if(++g_songFrame>=LOOP_FRAMES) g_songFrame=0;
    }
}
