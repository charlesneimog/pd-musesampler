#include <m_pd.h>

#include <array>
#include <functional>
#include <stdarg.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "pd-helpers.h"

#include "Musescore/apitypes.h"   // Downloaded by Cmake
#include "Musescore/libhandler.h" // Downloaded by Cmake

#if _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static t_class *MuseSampler;

struct TrackNote {
    int trackIndex;
    int midiNote;
};

struct InstrumentInfo {
    int instrumentId = -1;
    ms_InstrumentInfo msInstrument = nullptr;
    bool isValid() const { return instrumentId != -1 && msInstrument != nullptr; }
};

// ==============================================
class t_MuseSampler {
  public:
    t_object xObj;

    mu::musesampler::MuseSamplerLibHandler *MuseSounds;
    bool MuStarted = false;
    unsigned BlockSize = 0;

    t_outlet *lSig;
    t_outlet *rSig;

    t_outlet *aux;
};

static bool startMuseSampler(t_MuseSampler *x);

// ╭─────────────────────────────────────╮
// │      Helpers to Load MuseSound      │
// ╰─────────────────────────────────────╯

// ==============================================
inline void *loadLib(const std::string path) {
#ifdef _WIN32
    void *m_lib = LoadLibrary(path.c_str());
#else
    void *m_lib = dlopen(path.c_str(), RTLD_LAZY);
#endif
    return m_lib;
}

// ==============================================
inline void closeLib(void *libHandle) {
#ifdef _WIN32
    UNUSED(libHandle);
    return;
#else
    dlclose(libHandle);
#endif
}

// ==============================================
inline void *getLibFunc(void *libHandle, const char *funcName) {
#ifdef _WIN32
    return reinterpret_cast<void *>(GetProcAddress(static_cast<HINSTANCE>(libHandle), funcName));
#else
    return dlsym(libHandle, funcName);
#endif
}

// ==============================================
std::string getMuseSoundsPath() {
#if _WIN32
    std::string museSoundsLib = "C:\\Windows\\System32\\MuseSamplerCoreLib.dll";
    if (access(museSoundsLib.c_str(), F_OK) != -1) {
        return museSoundsLib;
    } else {
        return "";
    }
#elif __linux__
    const char *xdgDataHome = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    std::string homeMuse = std::string(home) + "/.local/share/MuseSampler/lib/libMuseSamplerCoreLib.so";
    if (access(homeMuse.c_str(), F_OK) != -1) {
        return homeMuse;
    } else {
        return "";
    }
#elif __APPLE__
    std::string museSoundsLib = "/usr/local/lib/libMuseSamplerCoreLib.dylib";
    if (access(museSoundsLib.c_str(), F_OK) != -1) {
        return museSoundsLib;
    } else {
        return "";
    }
#else
    pd_error(NULL, "[musesampler~] Not able to recognize the OS");
    return ""
#endif
}

// ==============================================
/*
 * This function loads the MuseSampler lib and get the Functions
 */
bool LoadMuseLib(t_MuseSampler *x) {
    std::string path = getMuseSoundsPath();

    if (path.empty()) {
        pd_error(NULL, "MuseSounds not found, if you are sure you have it please report in "
                       "https://github.com/charlesneimog/pd-musesampler/issues.");
        return false;
    }

    return true;
}

// ╭─────────────────────────────────────╮
// │          PureData Methods           │
// ╰─────────────────────────────────────╯
// ==============================================
/*
 * Stop all notes playing
 */

/*
void AllNotesOff(t_MuseSampler *x, t_float note, t_float velocity, t_float cents) {
    DEBUG_PRINT("NoteOn");

    if (!x->started) {
        pd_error(x, "[musesampler~] Sampler not initialized");
        return;
    }

    if (!x->m_instrumentInfo.isValid()) {
        pd_error(x, "[musesampler~] No instrument set");
        return;
    }

    x->museLib->allNotesOff(x->m_sampler);
}

// ==============================================
void NoteOff(t_MuseSampler *x, t_float note, t_float velocity) {
    DEBUG_PRINT("NoteOff");

    if (!x->started) {
        pd_error(x, "[musesampler~] Sampler not initialized");
        return;
    }

    ms_AuditionStopNoteEvent event = ms_AuditionStopNoteEvent();
    event._pitch = note;

    ms_Track track;
    for (int i = 0; i < x->trackList.size(); i++) {
        if (x->TrackNotes[i].midiNote == note) {
            x->museLib->stopAuditionNote(x->m_sampler, x->trackList[x->TrackNotes[i].trackIndex], event);
            x->TrackNotes[i].midiNote = -1;
        }
    }
    x->m_currentPosition = 0;
    x->m_isPlaying = true;
}

// ==============================================
void NoteOn(t_MuseSampler *x, t_float note, t_float velocity, t_float cents) {
    DEBUG_PRINT("NoteOn");

    if (!x->started) {
        pd_error(x, "[musesampler~] Sampler not initialized");
        return;
    }

    if (!x->m_instrumentInfo.isValid()) {
        pd_error(x, "[musesampler~] No instrument set");
        return;
    }

    // check is absolute value of cents is minor than 100
    if (abs((int)cents) > 100) {
        pd_error(x, "[musesampler~] Cents must be between -100 and 100");
        return;
    }

    // ms_LivePlayStartNoteEvent_2 event = ms_LivePlayStartNoteEvent_2();
    // ms_AuditionStartNoteEvent_2 event = ms_AuditionStartNoteEvent_2();
    // event._pitch = note;
    // event._dynamics = velocity / 127.0;
    // // event._articulation = (ms_NoteArticulation)x->articulation;
    // event._offset_cents = cents;
    ms_LivePlayStartNoteEvent_2 event = ms_LivePlayStartNoteEvent_2();
    event._pitch = note;
    event._offset_cents = cents;
    event._dynamics = velocity / 127.0;

    // x->museLib->startLivePlayNote(x->m_sampler, x->trackList[0], event); // BUG: for now just track 0
    x->eventList[x->eventIndex] = event;
    x->eventIndex++;

    x->TrackNotes[x->trackIndex].midiNote = note;
    x->TrackNotes[x->trackIndex].trackIndex = x->trackIndex;

    x->trackIndex++;
    if (x->trackIndex >= x->trackList.size()) {
        x->trackIndex = 0;
    }

    x->m_currentPosition = 0;
    x->m_isPlaying = true;
}

// ==============================================
void Articulation(t_MuseSampler *x, t_float art) {
    DEBUG_PRINT("Articulation");

    if (!x->started) {
        pd_error(x, "[musesampler~] Sampler not initialized");
        return;
    }
    x->articulation = 1LL << (int)art;
}

// ==============================================
void Reverb(t_MuseSampler *x, t_float reverb) {
    DEBUG_PRINT("Reverb");
    if (!x->started) {
        pd_error(x, "[musesampler~] Sampler not initialized");
        return;
    }
    if (x->museLib->disableReverb() != ms_Result_OK) {
        pd_error(x, "[musesampler~] Reverb not enabled");
        return;
    }
}

// ==============================================
void Get(t_MuseSampler *x, t_symbol *s) {
    std::string method = s->s_name;
    if (method == "instruments") {
        auto instrumentList = x->museLib->getInstrumentList();
        while (auto instrument = x->museLib->getNextInstrument(instrumentList)) {
            int instrumentId = x->museLib->getInstrumentId(instrument);
            std::string internalName = x->museLib->getInstrumentName(instrument);
            std::string internalCategory = x->museLib->getInstrumentCategory(instrument);
            std::string instrumentSoundId = x->museLib->getMpeSoundId(instrument);
            // t_atom argv[2];
            // SETFLOAT(argv, instrumentId);
            // SETSYMBOL(argv + 1, gensym(internalName.c_str()));
            post("Id %d: %s", instrumentId, internalName.c_str());
            // outlet_anything(x->aux, gensym("inst"), 2, argv);
        }
    } else {
        pd_error(x, "[musesampler~] Invalid argument for get method");
    }
}

// ==============================================
void setInstrument(t_MuseSampler *x, t_float id) {
    if (x->m_instrumentInfo.isValid()) {
        for (size_t i = 0; i < MAX_VOICE; ++i) {
            ms_Result clearOk = x->museLib->clearTrack(x->m_sampler, x->trackList[i]);
            ms_Result finalizeOk = x->museLib->finalizeTrack(x->m_sampler, x->trackList[i]);
            if (clearOk != ms_Result_OK && finalizeOk != ms_Result_OK) {
                pd_error(x, "[musesampler~] Error clearing track");
            }
        }
        x->museLib->stopLivePlayMode(x->m_sampler);
    }

    auto instrumentList = x->museLib->getInstrumentList();

    while (auto instrument = x->museLib->getNextInstrument(instrumentList)) {
    }
}

// ==============================================
static bool startMuseSampler(t_MuseSampler *x) {
    if (x->renderStep == 0) {
        return false;
    }
    if (!x->InstrumentName.empty()) {
        post("[musesampler~] Loading %s", x->InstrumentName.c_str());
        return false;
    }

    if (x->m_leftChannel.size() != x->renderStep) {
        x->m_leftChannel.resize(x->renderStep);
        x->m_rightChannel.resize(x->renderStep);
        x->m_bus._num_channels = 2;
        x->m_bus._num_data_pts = x->renderStep;
        x->m_internalBuffer[0] = x->m_leftChannel.data();
        x->m_internalBuffer[1] = x->m_rightChannel.data();
        x->m_bus._channels = x->m_internalBuffer.data();
        x->settedStep = x->renderStep;
    }

    if (x->museLib->initSampler(x->m_sampler, x->m_sampleRate, x->renderStep, 2) != ms_Result_OK) {
        pd_error(x, "[musesampler~] Unable to init MuseSampler");
        x->started = false;
        return false;
    }

    // x->museLib->setPosition(x->m_sampler, 0);
    x->museLib->startLivePlayMode(x->m_sampler);

    x->started = true;

    return true;
}
// ==============================================
*/
static t_int *MuseSamplerPerform(t_int *w) { return (w + 5); }

// ==============================================
static void MuseSamplerAddDsp(t_MuseSampler *x, t_signal **sp) {
    x->BlockSize = sp[0]->s_n;
    if (!x->MuStarted) {
    }

    // dsp_add(MuseSamplerPerform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

// ==============================================
static void *NewMuseSampler(t_symbol *s, int argc, t_atom *argv) {
    t_MuseSampler *x = (t_MuseSampler *)pd_new(MuseSampler);

    x->lSig = outlet_new(&x->xObj, &s_signal);
    x->rSig = outlet_new(&x->xObj, &s_signal);
    x->aux = outlet_new(&x->xObj, &s_anything);

    std::string path = getMuseSoundsPath();

    mu::musesampler::MuseSamplerLibHandlerPtr m_libHandler = nullptr;
    m_libHandler = std::make_shared<mu::musesampler::MuseSamplerLibHandler>(path);
    if (!m_libHandler->isValid()) {
        pd_error(NULL, "MuseSounds not found, if you are sure you have it please report");
    }

    return x;
}

// ==============================================
static void *FreeMuseSampler(t_MuseSampler *x) { return nullptr; }

// ==============================================
/* Setup the object */
extern "C" void musesampler_tilde_setup(void) {
    MuseSampler = class_new(gensym("musesampler~"), (t_newmethod)NewMuseSampler, NULL, sizeof(t_MuseSampler),
                            CLASS_DEFAULT, A_GIMME, 0);

    class_addmethod(MuseSampler, (t_method)MuseSamplerAddDsp, gensym("dsp"), A_CANT, 0);
    // class_addmethod(MuseSampler, (t_method)Get, gensym("get"), A_SYMBOL, 0);

    // class_addmethod(MuseSampler, (t_method)AllNotesOff, gensym("reset"), A_NULL, 0);
    // class_addmethod(MuseSampler, (t_method)NoteOn, gensym("noteon"), A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    // class_addmethod(MuseSampler, (t_method)NoteOff, gensym("noteoff"), A_FLOAT, A_FLOAT, 0);
    // class_addmethod(MuseSampler, (t_method)Articulation, gensym("art"), A_FLOAT, 0);
    // class_addmethod(MuseSampler, (t_method)Reverb, gensym("reverb"), A_FLOAT, 0);

    // class_addmethod(MuseSampler, (t_method)setInstrument, gensym("set"), A_FLOAT, 0);

#ifndef NEIMOG_LIBRARY
    post("[musesampler~] by Charles K. Neimog");
    post("[musesampler~] version 0.0.1");
#endif
}
