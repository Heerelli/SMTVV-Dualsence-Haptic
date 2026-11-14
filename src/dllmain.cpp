// SMT5V DualSense Haptic Bridge
// ---------------------------------------------------------------------------
// Sits in front of "SMT5V Haptics" (renamed SMT5VHapticsCore.dll) and turns
// its XInput rumble into DualSense voice-coil haptics (USB audio channels 3/4).
//
//  game ──► xinput1_3.dll (this, export forwarders) ──► SMT5VHapticsCore.dll
//                                                        │  XInputSetState ─► Hook_SetState ─► motors→haptics
//                                                        │  log "EVENT kind=" ─► Hook_WriteFile ─► event waveform
//                                                        ▼
//                                              System32\xinput1_3.dll
//
// No offsets of the core are hard-coded: the core's IAT (GetProcAddress,
// WriteFile) is patched, and its writable sections are scanned for an
// already-resolved XInputSetState pointer.
// ---------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <initguid.h>   // defines PKEY_Device_FriendlyName in this TU
#include <xinput.h>       // types only, no xinput.lib
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ks.h>
#include <ksmedia.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

#define BRIDGE_VERSION "0.2"

// ───────────────────────────── globals / config ─────────────────────────────
static HMODULE g_self, g_core, g_sysXInput;
static wchar_t g_dir[MAX_PATH], g_ini[MAX_PATH], g_logPath[MAX_PATH];

typedef DWORD(WINAPI* PFN_SetState)(DWORD, XINPUT_VIBRATION*);
typedef FARPROC(WINAPI* PFN_GetProcAddress)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* PFN_WriteFile)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static PFN_SetState       g_realSetState;
static PFN_GetProcAddress g_realGetProcAddress;
static PFN_WriteFile      g_realWriteFile;

struct Config {
    int    enabled = 1;
    wchar_t deviceName[128] = L"Wireless Controller";
    double gain = 1.0;
    int    passThroughRumble = 0;  // 1 = also drive the (emulated) motors
    int    motorToHaptics = 0;     // raw motor values → haptics (game has no native rumble)
    double motorLowHz = 55.0, motorHighHz = 160.0;
    int    motorDelayMs = 12;      // wait this long for an EVENT line before using raw motors
    int    bufferMs = 10;
    int    leftChannel = 2, rightChannel = 3;  // 0-based; DualSense: 3rd/4th channel
    int    muteWhenUnfocused = 1;
    int    testHotkey = 1;         // F11 = cycle event test patterns
} g_cfg;

static std::atomic<bool> g_quit{ false };
static std::atomic<bool> g_hapticsActive{ false };
static LARGE_INTEGER g_qpf;

static void Log(const char* fmt, ...) {
    char msg[768];
    va_list ap; va_start(ap, fmt); _vsnprintf_s(msg, sizeof msg, _TRUNCATE, fmt, ap); va_end(ap);
    SYSTEMTIME st; GetLocalTime(&st);
    char line[900];
    int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%04u-%02u-%02u %02u:%02u:%02u.%03u [DualSense] %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
    HANDLE h = CreateFileW(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD w; WriteFile(h, line, (DWORD)(n > 0 ? n : 0), &w, nullptr);  // own IAT, not hooked
        CloseHandle(h);
    }
}

static double IniDouble(const wchar_t* key, double def) {
    wchar_t buf[64], d[64];
    swprintf_s(d, L"%g", def);
    GetPrivateProfileStringW(L"DualSense", key, d, buf, 64, g_ini);
    return _wtof(buf);
}
static int IniInt(const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(L"DualSense", key, def, g_ini); }

static void LoadConfig() {
    g_cfg.enabled           = IniInt(L"Enabled", 1);
    GetPrivateProfileStringW(L"DualSense", L"DeviceName", L"Wireless Controller", g_cfg.deviceName, 128, g_ini);
    g_cfg.gain              = IniDouble(L"Gain", 1.0);
    g_cfg.passThroughRumble = IniInt(L"PassThroughRumble", 0);
    g_cfg.motorToHaptics    = IniInt(L"MotorToHaptics", 0);
    g_cfg.motorLowHz        = IniDouble(L"MotorLowHz", 55.0);
    g_cfg.motorHighHz       = IniDouble(L"MotorHighHz", 160.0);
    g_cfg.motorDelayMs      = IniInt(L"MotorDelayMs", 12);
    g_cfg.bufferMs          = IniInt(L"BufferMs", 10);
    g_cfg.leftChannel       = IniInt(L"LeftChannel", 2);
    g_cfg.rightChannel      = IniInt(L"RightChannel", 3);
    g_cfg.muteWhenUnfocused = IniInt(L"MuteWhenUnfocused", 1);
    g_cfg.testHotkey        = IniInt(L"TestHotkey", 1);
    if (g_cfg.bufferMs < 3) g_cfg.bufferMs = 3;
    if (g_cfg.bufferMs > 50) g_cfg.bufferMs = 50;
    if (g_cfg.gain < 0) g_cfg.gain = 0;
    if (g_cfg.gain > 2) g_cfg.gain = 2;
}

static long long QpcNow() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
static long long MsToQpc(double ms) { return (long long)(ms * g_qpf.QuadPart / 1000.0); }

// ───────────────────────────── haptic patterns ──────────────────────────────
// A pattern is a set of short sine layers with a fast attack and exponential decay.
struct Layer { float f0, f1, startMs, lenMs, ampL, ampR; };
struct Pattern { const char* name; int n; Layer layers[4]; };

enum Kind { K_NORMAL, K_CRITICAL, K_WEAK, K_GUARD, K_SOUND, K_HEAL, K_COUNT };

static const Pattern kPatterns[K_COUNT] = {
    // name        layers: f0   f1  start  len   L     R
    { "normal",   2, { { 70,  60,   0,   90, 0.80f, 0.80f }, { 180, 180,  0,  25, 0.40f, 0.40f } } },
    { "critical", 3, { { 220, 200,  0,   20, 0.90f, 0.90f }, {  45,  40,  0,  80, 1.00f, 1.00f },
                       { 65,  50,  60,  170, 0.70f, 0.70f } } },
    { "weak",     3, { { 130, 130,  0,   35, 0.90f, 0.30f }, { 130, 130, 70,  35, 0.30f, 0.90f },
                       { 60,  55,   0,  130, 0.50f, 0.50f } } },
    { "guard",    2, { { 260, 240,  0,   18, 0.70f, 0.70f }, {  40,  40, 10,  60, 0.35f, 0.35f } } },
    { "sound",    2, { { 90,  80,   0,  120, 0.60f, 0.60f }, {  50,  45,  0, 180, 0.40f, 0.40f } } },
    { "heal",     1, { { 160, 240,  0,  240, 0.25f, 0.25f } } },
};

static int KindFromName(const char* s, size_t n) {
    for (int k = 0; k < K_COUNT; ++k) {
        size_t m = strlen(kPatterns[k].name);
        if (m == n && memcmp(s, kPatterns[k].name, n) == 0) return k;
    }
    return -1;
}

struct Voice { int kind; double t; double scale; double gain; bool live; };

// Shared state between hooks (game/core threads) and the render thread.
static SRWLOCK g_lock = SRWLOCK_INIT;
static Voice   g_pendingVoices[16]; static int g_pendingCount;
static float   g_motorLow, g_motorHigh;     // latest SetState target, 0..1
static long long g_motorOnset;              // QPC when motors went from silent to non-zero
static long long g_motorMuteUntil;          // QPC: raw motors ignored (event owns the actuators)

static void TriggerEvent(int kind, unsigned durationMs, int intensity) {
    if (kind < 0 || kind >= K_COUNT) return;
    double scale = 1.0;                       // mirror core v0.6: 8/9/10 → 120/125/130 %
    if (intensity >= 10) scale = 1.30; else if (intensity == 9) scale = 1.25; else if (intensity == 8) scale = 1.20;
    double gain = intensity <= 0 ? 0.0 : (intensity <= 5 ? intensity / 5.0 : 1.0 + (intensity - 5) * 0.12);
    AcquireSRWLockExclusive(&g_lock);
    if (g_pendingCount < 16) g_pendingVoices[g_pendingCount++] = Voice{ kind, 0.0, scale, gain, true };
    long long until = QpcNow() + MsToQpc((durationMs ? durationMs : 150) + 30.0);
    if (until > g_motorMuteUntil) g_motorMuteUntil = until;
    ReleaseSRWLockExclusive(&g_lock);
}

// ───────────────────────────── hooks ────────────────────────────────────────
static DWORD WINAPI Hook_SetState(DWORD idx, XINPUT_VIBRATION* v) {
    if (!g_realSetState) return ERROR_DEVICE_NOT_CONNECTED;
    if (!v || !g_cfg.enabled) return g_realSetState(idx, v);

    AcquireSRWLockExclusive(&g_lock);
    bool wasSilent = g_motorLow == 0 && g_motorHigh == 0;
    g_motorLow  = v->wLeftMotorSpeed  / 65535.0f;
    g_motorHigh = v->wRightMotorSpeed / 65535.0f;
    if (wasSilent && (g_motorLow > 0 || g_motorHigh > 0)) g_motorOnset = QpcNow();
    ReleaseSRWLockExclusive(&g_lock);

    if (g_hapticsActive.load() && !g_cfg.passThroughRumble) {
        XINPUT_VIBRATION zero{ 0, 0 };
        return g_realSetState(idx, &zero);
    }
    return g_realSetState(idx, v);
}

static const char* FindBytes(const char* hay, size_t n, const char* needle) {
    size_t m = strlen(needle);
    if (m > n) return nullptr;
    for (size_t i = 0; i + m <= n; ++i) if (memcmp(hay + i, needle, m) == 0) return hay + i;
    return nullptr;
}

// Core log line: "... EVENT kind=%s controller=%d delay_ms=%llu duration_ms=%u intensity=%ld/10 result=%lu"
static void SniffLog(const char* buf, size_t n) {
    if (n > 2048) return;
    const char* p = FindBytes(buf, n, "] EVENT kind=");
    if (!p) return;
    const char* end = buf + n;
    p += 13;
    const char* q = p;
    while (q < end && *q > ' ') ++q;
    int kind = KindFromName(p, (size_t)(q - p));
    unsigned dur = 0; int inten = 5;
    char tmp[512]; size_t rest = (size_t)(end - q); if (rest > sizeof tmp - 1) rest = sizeof tmp - 1;
    memcpy(tmp, q, rest); tmp[rest] = 0;
    if (const char* d = strstr(tmp, "duration_ms=")) dur = (unsigned)strtoul(d + 12, nullptr, 10);
    if (const char* i = strstr(tmp, "intensity="))   inten = (int)strtol(i + 10, nullptr, 10);
    if (kind >= 0) TriggerEvent(kind, dur, inten);
}

static BOOL WINAPI Hook_WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD w, LPOVERLAPPED ov) {
    if (g_cfg.enabled && buf && n) {
        __try { SniffLog((const char*)buf, n); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return g_realWriteFile(h, buf, n, w, ov);
}

static FARPROC WINAPI Hook_GetProcAddress(HMODULE m, LPCSTR name) {
    FARPROC r = g_realGetProcAddress(m, name);
    if (r && m == g_sysXInput && ((ULONG_PTR)name >> 16) && strcmp(name, "XInputSetState") == 0) {
        g_realSetState = (PFN_SetState)r;
        Log("core resolved XInputSetState via GetProcAddress -> hooked");
        return (FARPROC)&Hook_SetState;
    }
    return r;
}

// ───────────────────────────── PE patching ──────────────────────────────────
static bool PatchPtr(void** slot, void* value) {
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *slot = value;
    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}

static int PatchIat(HMODULE mod, void* target, void* replacement) {
    auto base = (BYTE*)mod;
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    int count = 0;
    for (auto imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; ++imp) {
        for (auto th = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk); th->u1.Function; ++th) {
            if ((void*)th->u1.Function == target && PatchPtr((void**)&th->u1.Function, replacement)) ++count;
        }
    }
    return count;
}

// If the core already resolved XInputSetState (e.g. in its DllMain), its pointer sits in a
// writable section. Replace every exact match with our hook.
static int PatchResolvedPointers(HMODULE mod, void* target, void* replacement) {
    auto base = (BYTE*)mod;
    auto nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    int count = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
        auto p = (void**)(base + sec->VirtualAddress);
        size_t n = sec->Misc.VirtualSize / sizeof(void*);
        for (size_t k = 0; k < n; ++k) if (p[k] == target) { p[k] = replacement; ++count; }
    }
    return count;
}

// ───────────────────────────── WASAPI haptic engine ─────────────────────────
static bool NameMatches(const wchar_t* name) {
    if (!name) return false;
    if (wcsstr(name, g_cfg.deviceName)) return true;
    return wcsstr(name, L"DualSense") != nullptr;
}

static IMMDevice* FindDualSense(IMMDeviceEnumerator* en) {
    IMMDeviceCollection* col = nullptr;
    if (FAILED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col))) return nullptr;
    UINT n = 0; col->GetCount(&n);
    IMMDevice* found = nullptr;
    for (UINT i = 0; i < n && !found; ++i) {
        IMMDevice* d = nullptr;
        if (FAILED(col->Item(i, &d))) continue;
        IPropertyStore* ps = nullptr;
        if (SUCCEEDED(d->OpenPropertyStore(STGM_READ, &ps))) {
            PROPVARIANT v; PropVariantInit(&v);
            if (SUCCEEDED(ps->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR && NameMatches(v.pwszVal)) {
                char nm[256]; WideCharToMultiByte(CP_UTF8, 0, v.pwszVal, -1, nm, 256, nullptr, nullptr);
                Log("audio endpoint found: %s", nm);
                found = d; d = nullptr;
            }
            PropVariantClear(&v);
            ps->Release();
        }
        if (d) d->Release();
    }
    col->Release();
    return found;
}

static bool GameHasFocus() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

struct Synth {
    double sr = 48000;
    Voice voices[16]{};
    double lowPh = 0, highPh = 0;
    float curLow = 0, curHigh = 0;   // smoothed motor amplitudes
    int   testKind = 0;

    void Pull() {
        AcquireSRWLockExclusive(&g_lock);
        for (int i = 0; i < g_pendingCount; ++i)
            for (auto& v : voices) if (!v.live) { v = g_pendingVoices[i]; break; }
        g_pendingCount = 0;
        ReleaseSRWLockExclusive(&g_lock);
    }

    // Motor target for this block, honoring the event mute window and the confirmation delay.
    void MotorTarget(float& lo, float& hi) {
        AcquireSRWLockShared(&g_lock);
        float l = g_motorLow, h = g_motorHigh; long long onset = g_motorOnset, mute = g_motorMuteUntil;
        ReleaseSRWLockShared(&g_lock);
        long long now = QpcNow();
        bool fresh = (l > 0 || h > 0) && (now - onset) < MsToQpc(g_cfg.motorDelayMs);
        if (!g_cfg.motorToHaptics || now < mute || fresh) { lo = hi = 0; return; }
        lo = l; hi = h;
    }

    static float Env(double tMs, double lenMs) {
        if (tMs < 0 || tMs >= lenMs) return 0.f;
        const double atk = 2.0;
        if (tMs < atk) return (float)(tMs / atk);
        double x = (tMs - atk) / (lenMs - atk);
        double e = std::exp(-4.6 * x);                    // ≈ -40 dB at the end
        if (lenMs - tMs < 3.0) e *= (lenMs - tMs) / 3.0;  // tiny release, no click
        return (float)e;
    }

    void Render(float* outL, float* outR, UINT frames, bool silent) {
        float tl, th; MotorTarget(tl, th);
        if (silent) { tl = th = 0; for (auto& v : voices) v.live = false; }
        const double a = 1.0 - std::exp(-1.0 / (sr * 0.008));  // 8 ms smoothing
        const double dLow = 2 * 3.14159265358979 * g_cfg.motorLowHz / sr;
        const double dHigh = 2 * 3.14159265358979 * g_cfg.motorHighHz / sr;
        const float g = (float)g_cfg.gain;
        for (UINT i = 0; i < frames; ++i) {
            curLow += (float)((tl - curLow) * a);
            curHigh += (float)((th - curHigh) * a);
            lowPh += dLow; if (lowPh > 6.283185307) lowPh -= 6.283185307;
            highPh += dHigh; if (highPh > 6.283185307) highPh -= 6.283185307;
            float sl = (float)std::sin(lowPh), sh = (float)std::sin(highPh);
            float L = curLow * sl * 0.90f + curHigh * sh * 0.35f;
            float R = curLow * sl * 0.35f + curHigh * sh * 0.90f;

            if (!silent) for (auto& v : voices) {
                if (!v.live) continue;
                const Pattern& p = kPatterns[v.kind];
                double tMs = v.t * 1000.0 / sr;
                double maxEnd = 0;
                for (int k = 0; k < p.n; ++k) {
                    const Layer& ly = p.layers[k];
                    double st = ly.startMs * v.scale, len = ly.lenMs * v.scale, lt = tMs - st;
                    if (st + len > maxEnd) maxEnd = st + len;
                    float e = Env(lt, len);
                    if (e <= 0) continue;
                    double ts = lt / 1000.0, lenS = len / 1000.0;
                    double ph = 2 * 3.14159265358979 * (ly.f0 * ts + (ly.f1 - ly.f0) * ts * ts / (2 * lenS));
                    float s = (float)std::sin(ph) * e * (float)v.gain;
                    L += s * ly.ampL; R += s * ly.ampR;
                }
                v.t += 1;
                if (tMs > maxEnd) v.live = false;
            }
            // soft clip
            L *= g; R *= g;
            outL[i] = (float)std::tanh(L * 1.2) / 0.834f * 0.98f;
            outR[i] = (float)std::tanh(R * 1.2) / 0.834f * 0.98f;
            if (outL[i] > 1) outL[i] = 1; if (outL[i] < -1) outL[i] = -1;
            if (outR[i] > 1) outR[i] = 1; if (outR[i] < -1) outR[i] = -1;
        }
    }
};

enum SampleFmt { SF_F32, SF_I16, SF_I24, SF_I32 };

static void WriteFrames(BYTE* dst, const WAVEFORMATEX* wf, SampleFmt sf, const float* L, const float* R, UINT frames) {
    const int ch = wf->nChannels, bytes = wf->wBitsPerSample / 8;
    memset(dst, 0, (size_t)frames * wf->nBlockAlign);
    for (UINT i = 0; i < frames; ++i) {
        BYTE* frame = dst + (size_t)i * wf->nBlockAlign;
        for (int side = 0; side < 2; ++side) {
            int c = side ? g_cfg.rightChannel : g_cfg.leftChannel;
            if (c < 0 || c >= ch) continue;
            float s = side ? R[i] : L[i];
            BYTE* p = frame + c * bytes;
            switch (sf) {
            case SF_F32: *(float*)p = s; break;
            case SF_I16: *(short*)p = (short)(s * 32767.0f); break;
            case SF_I24: { int v = (int)(s * 8388607.0f); p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); p[2] = (BYTE)(v >> 16); } break;
            case SF_I32: *(int*)p = (int)(s * 2147483647.0); break;
            }
        }
    }
}

static bool DetectFormat(const WAVEFORMATEX* wf, SampleFmt& sf) {
    bool isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool isPcm = wf->wFormatTag == WAVE_FORMAT_PCM;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto ext = (const WAVEFORMATEXTENSIBLE*)wf;
        isFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
        isPcm = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) != 0;
    }
    if (isFloat && wf->wBitsPerSample == 32) { sf = SF_F32; return true; }
    if (isPcm && wf->wBitsPerSample == 16) { sf = SF_I16; return true; }
    if (isPcm && wf->wBitsPerSample == 24) { sf = SF_I24; return true; }
    if (isPcm && wf->wBitsPerSample == 32) { sf = SF_I32; return true; }
    return false;
}

static void PollHotkey(Synth& syn) {
    static bool prev = false;
    bool down = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (down && !prev && GameHasFocus()) {
        int k = syn.testKind; syn.testKind = (syn.testKind + 1) % K_COUNT;
        Log("TEST pattern %s (F11)", kPatterns[k].name);
        TriggerEvent(k, 250, 5);
    }
    prev = down;
}

// One session on one device. Returns when the device goes away or on quit.
static void RunDevice(IMMDevice* dev, Synth& syn) {
    IAudioClient* ac = nullptr; IAudioRenderClient* rc = nullptr; WAVEFORMATEX* wf = nullptr;
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    float* bufL = nullptr; float* bufR = nullptr;
    SampleFmt sf;
    UINT32 bufFrames = 0;
    HRESULT hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&ac);
    if (FAILED(hr)) { Log("Activate failed 0x%08lX", hr); goto done; }
    if (FAILED(hr = ac->GetMixFormat(&wf))) { Log("GetMixFormat failed 0x%08lX", hr); goto done; }
    Log("mix format: %u ch, %lu Hz, %u bit, tag=0x%X", wf->nChannels, wf->nSamplesPerSec, wf->wBitsPerSample, wf->wFormatTag);
    if (wf->nChannels < 4) { Log("endpoint has %u channels; haptics need 4 (USB connection required)", wf->nChannels); goto done; }
    if (!DetectFormat(wf, sf)) { Log("unsupported sample format"); goto done; }
    hr = ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        (REFERENCE_TIME)g_cfg.bufferMs * 10000, 0, wf, nullptr);
    if (FAILED(hr)) { Log("IAudioClient::Initialize failed 0x%08lX", hr); goto done; }
    ac->SetEventHandle(ev);
    ac->GetBufferSize(&bufFrames);
    if (FAILED(hr = ac->GetService(__uuidof(IAudioRenderClient), (void**)&rc))) { Log("GetService failed 0x%08lX", hr); goto done; }
    bufL = new float[bufFrames]; bufR = new float[bufFrames];
    syn.sr = wf->nSamplesPerSec;
    if (FAILED(hr = ac->Start())) { Log("Start failed 0x%08lX", hr); goto done; }
    g_hapticsActive = true;
    Log("HAPTICS ACTIVE buffer=%u frames (%d ms), channels L=%d R=%d", bufFrames, g_cfg.bufferMs, g_cfg.leftChannel, g_cfg.rightChannel);

    while (!g_quit.load()) {
        WaitForSingleObject(ev, 200);
        if (g_cfg.testHotkey) PollHotkey(syn);
        UINT32 pad = 0;
        hr = ac->GetCurrentPadding(&pad);
        if (FAILED(hr)) { Log("device lost 0x%08lX", hr); break; }
        UINT32 n = bufFrames - pad;
        if (!n) continue;
        BYTE* data = nullptr;
        if (FAILED(hr = rc->GetBuffer(n, &data))) { Log("GetBuffer failed 0x%08lX", hr); break; }
        syn.Pull();
        bool silent = !g_cfg.enabled || (g_cfg.muteWhenUnfocused && !GameHasFocus());
        syn.Render(bufL, bufR, n, silent);
        WriteFrames(data, wf, sf, bufL, bufR, n);
        rc->ReleaseBuffer(n, 0);
    }
    ac->Stop();
done:
    g_hapticsActive = false;
    delete[] bufL; delete[] bufR;
    if (rc) rc->Release();
    if (wf) CoTaskMemFree(wf);
    if (ac) ac->Release();
    CloseHandle(ev);
}

static DWORD WINAPI AudioThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) {
        Log("MMDeviceEnumerator unavailable");
        return 0;
    }
    Synth syn;
    bool warned = false;
    while (!g_quit.load()) {
        IMMDevice* dev = FindDualSense(en);
        if (dev) { warned = false; RunDevice(dev, syn); dev->Release(); }
        else if (!warned) { Log("no DualSense audio endpoint (USB cable?) - rumble passes through unchanged; retrying"); warned = true; }
        for (int i = 0; i < 20 && !g_quit.load(); ++i) Sleep(100);
    }
    en->Release();
    CoUninitialize();
    return 0;
}

// ───────────────────────────── entry ────────────────────────────────────────
// Two build flavours share everything above:
//   default            : xinput1_3.dll proxy, original mod renamed to SMT5VHapticsCore.dll
//   SMT5V_RELOADED     : Reloaded-II native mod; the original mod stays installed as-is
//                        (xinput1_3.dll, or d3d11.dll on Steam Deck) and is patched in place.

static void SetPaths(const wchar_t* dir) {
    wcscpy_s(g_dir, dir);
    swprintf_s(g_ini, L"%s\\SMT5V-Haptics.ini", g_dir);
    swprintf_s(g_logPath, L"%s\\SMT5V-DualSense.log", g_dir);
}

static void DirOf(HMODULE m, wchar_t* out) {
    GetModuleFileNameW(m, out, MAX_PATH);
    if (wchar_t* s = wcsrchr(out, L'\\')) *s = 0;
}

static bool LoadSystemXInput() {
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    wcscat_s(sys, L"\\xinput1_3.dll");
    g_sysXInput = LoadLibraryW(sys);
    if (!g_sysXInput) { Log("ERROR: cannot load %ls", sys); return false; }
    g_realSetState = (PFN_SetState)GetProcAddress(g_sysXInput, "XInputSetState");
    return true;
}

static bool PatchCore(HMODULE core) {
    g_core = core;
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    g_realGetProcAddress = (PFN_GetProcAddress)GetProcAddress(k32, "GetProcAddress");
    g_realWriteFile = (PFN_WriteFile)GetProcAddress(k32, "WriteFile");
    void* realSet = (void*)GetProcAddress(g_sysXInput, "XInputSetState");
    int a = PatchIat(core, (void*)g_realGetProcAddress, (void*)&Hook_GetProcAddress);
    int b = PatchIat(core, (void*)g_realWriteFile, (void*)&Hook_WriteFile);
    int c = realSet ? PatchResolvedPointers(core, realSet, (void*)&Hook_SetState) : 0;
    Log("core patched: GetProcAddress IAT=%d WriteFile IAT=%d resolved SetState=%d", a, b, c);
    if (!b) Log("WARNING: WriteFile not hooked - event-specific patterns disabled, raw motors only");
    return a + b + c > 0;
}

static void StartEngine() {
    LoadConfig();
    Log("bridge v%s (%s) enabled=%d gain=%.2f passThrough=%d motorToHaptics=%d", BRIDGE_VERSION,
#ifdef SMT5V_RELOADED
        "Reloaded-II",
#else
        "proxy",
#endif
        g_cfg.enabled, g_cfg.gain, g_cfg.passThroughRumble, g_cfg.motorToHaptics);
    if (g_cfg.enabled) CloseHandle(CreateThread(nullptr, 0, AudioThread, nullptr, 0, nullptr));
}

#ifndef SMT5V_RELOADED
// ---- proxy flavour -------------------------------------------------------
static void Init() {
    QueryPerformanceFrequency(&g_qpf);
    wchar_t dir[MAX_PATH]; DirOf(g_self, dir); SetPaths(dir);
    LoadConfig();
    if (!LoadSystemXInput()) return;

    wchar_t corePath[MAX_PATH];
    swprintf_s(corePath, L"%s\\SMT5VHapticsCore.dll", g_dir);
    HMODULE core = LoadLibraryW(corePath);
    if (!core) { Log("ERROR: SMT5VHapticsCore.dll missing (rename the original mod's xinput1_3.dll), Win32=%lu", GetLastError()); return; }
    PatchCore(core);
    StartEngine();
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        Init();
    } else if (reason == DLL_PROCESS_DETACH) {
        g_quit = true;
    }
    return TRUE;
}

#else
// ---- Reloaded-II flavour -------------------------------------------------
// The original mod is identified by its own export, not by file name.
static HMODULE FindCoreModule() {
    static const wchar_t* names[] = { L"xinput1_3.dll", L"SMT5VHapticsCore.dll", L"d3d11.dll" };
    for (auto n : names) {
        HMODULE m = GetModuleHandleW(n);
        if (m && GetProcAddress(m, "SMT5VHapticsPoCStatus")) return m;
    }
    return nullptr;
}

static DWORD WINAPI ReloadedInitThread(LPVOID) {
    // Log next to this mod until the original mod is found.
    wchar_t dir[MAX_PATH]; DirOf(g_self, dir); SetPaths(dir);
    if (!LoadSystemXInput()) return 0;

    HMODULE core = nullptr;
    for (int i = 0; i < 1200 && !core && !g_quit.load(); ++i) {   // up to 60 s
        core = FindCoreModule();
        if (!core) Sleep(50);
    }
    if (!core) {
        Log("ERROR: SMT5V Haptics (original mod) not loaded - install it in Project\\Binaries\\Win64 first");
        return 0;
    }
    // From here on, use the original mod's folder for the INI and the log.
    DirOf(core, dir); SetPaths(dir);
    if (!PatchCore(core)) {
        // Nothing left to patch: the proxy flavour (xinput1_3.dll bridge) is already installed.
        Log("ERROR: original mod already patched - remove the proxy xinput1_3.dll bridge or this Reloaded-II mod, not both");
        return 0;
    }
    StartEngine();
    return 0;
}

extern "C" __declspec(dllexport) void ReloadedStart() {
    static std::atomic<bool> started{ false };
    if (started.exchange(true)) return;
    QueryPerformanceFrequency(&g_qpf);
    CloseHandle(CreateThread(nullptr, 0, ReloadedInitThread, nullptr, 0, nullptr));
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_quit = true;
    }
    return TRUE;
}
#endif
