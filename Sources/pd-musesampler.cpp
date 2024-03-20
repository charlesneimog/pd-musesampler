#include "pd-musampler.h"

#include <cstdlib>
#include <unistd.h>

// #ifdef DEBUG_MODE
#define DEBUG_PRINT(message) printf("[DEBUG] %s\n", message)
// #else
// #define DEBUG_PRINT(message) // Define como vazio em builds sem debug
// #endif

static t_class *MuseSampler;
#define MAX_VOICE 64

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
typedef struct _Synth {
    t_object xObj;

    MuseSamplerLibHandler *m_samplerLib;
    ms_MuseSampler m_sampler = nullptr;
    InstrumentInfo m_instrument;
    ms_Track Track;

    // TrackList is a vector of tracks
    std::vector<ms_Track> trackList;
    std::vector<TrackNote> TrackNotes;
    unsigned int trackIndex = 0;
    uint64_t articulation;

    // MuseSamplerSequencer m_sequencer;
    InstrumentInfo m_instrumentInfo;
    ms_OutputBuffer m_bus;

    unsigned int m_sampleRate;
    bool m_isPlaying = false;
    bool instrumentAdded = true;
    std::array<float *, 2> m_internalBuffer;

    bool sucess = false;
    bool started = false;
    float m_currentPosition = 0;
    int outIndex;
    int renderStep;

    std::vector<t_sample> m_leftChannel;
    std::vector<t_sample> m_rightChannel;

    t_outlet *lSig;
    t_outlet *rSig;

    t_outlet *aux;
} t_MuseSampler;

std::string getMuseSoundsPath() {
#ifdef __linux__
    const char *xdgDataHome = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    std::string xdgMuse = std::string(xdgDataHome) + "/MuseSampler/lib/libMuseSamplerCoreLib.so";
    std::string homeMuse = std::string(home) + "/.local/share/MuseSampler/lib/libMuseSamplerCoreLib.so";
    std::string genericMuse = "/usr/share/MuseSampler/lib/libMuseSamplerCoreLib.so";

    if (access(xdgMuse.c_str(), F_OK) != -1) {
        return xdgMuse;
    } else if (access(homeMuse.c_str(), F_OK) != -1) {
        return homeMuse;
    } else if (access(genericMuse.c_str(), F_OK) != -1) {
        return genericMuse;
    } else {
        pd_error(NULL, "MuseSamplerCoreLib not found");
        return "";
    }
#elif __APPLE__
    return ""
#elif _WIN32
    return return ""
#else
    pd_error(NULL, "Not able to recognize the OS");
    return ""
#endif
}

// ╭─────────────────────────────────────╮
// │          PureData Methods           │
// ╰─────────────────────────────────────╯
// ==============================================
void AllNotesOff(t_MuseSampler *x, t_float note, t_float velocity, t_float cents) {
    DEBUG_PRINT("NoteOn");

    if (!x->started) {
        pd_error(x, "Sampler not initialized");
        return;
    }

    if (!x->m_instrumentInfo.isValid()) {
        pd_error(x, "No instrument set");
        return;
    }

    x->m_samplerLib->allNotesOff(x->m_sampler);
}

// ==============================================
void NoteOff(t_MuseSampler *x, t_float note, t_float velocity) {
    DEBUG_PRINT("NoteOff");

    if (!x->started) {
        pd_error(x, "Sampler not initialized");
        return;
    }

    ms_AuditionStopNoteEvent event = ms_AuditionStopNoteEvent();
    event._pitch = note;

    ms_Track track;
    for (int i = 0; i < x->trackList.size(); i++) {
        if (x->TrackNotes[i].midiNote == note) {
            x->m_samplerLib->stopAuditionNote(x->m_sampler, x->trackList[x->TrackNotes[i].trackIndex], event);
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
        pd_error(x, "Sampler not initialized");
        return;
    }

    if (!x->m_instrumentInfo.isValid()) {
        pd_error(x, "No instrument set");
        return;
    }

    // ms_LivePlayStartNoteEvent_2 event = ms_LivePlayStartNoteEvent_2();
    ms_AuditionStartNoteEvent_2 event = ms_AuditionStartNoteEvent_2();
    event._pitch = note;
    event._dynamics = velocity / 127.0;
    event._articulation = (ms_NoteArticulation)x->articulation;
    event._offset_cents = cents;

    x->m_samplerLib->startAuditionNote(x->m_sampler, x->trackList[0], event);
    x->TrackNotes[x->trackIndex].midiNote = note;
    x->TrackNotes[x->trackIndex].trackIndex = x->trackIndex;

    x->trackIndex++;
    if (x->trackIndex >= x->trackList.size()) {
        x->trackIndex = 0;
    }
    // post("track index: %d", x->trackIndex);

    x->m_currentPosition = 0;
    x->m_isPlaying = true;
}

// ==============================================
void Articulation(t_MuseSampler *x, t_float art) {
    DEBUG_PRINT("Articulation");

    if (!x->started) {
        pd_error(x, "Sampler not initialized");
        return;
    }
    x->articulation = 1LL << (int)art;
}

// ==============================================
void Reverb(t_MuseSampler *x, t_float reverb) {
    DEBUG_PRINT("Reverb");
    if (!x->started) {
        pd_error(x, "Sampler not initialized");
        return;
    }
    if (x->m_samplerLib->disableReverb() != ms_Result_OK) {
        pd_error(x, "Reverb not enabled");
        return;
    }
}

// ==============================================
void getInstrumentList(t_MuseSampler *x) {
    DEBUG_PRINT("getInstrumentList");
    auto instrumentList = x->m_samplerLib->getInstrumentList();
    while (auto instrument = x->m_samplerLib->getNextInstrument(instrumentList)) {
        int instrumentId = x->m_samplerLib->getInstrumentId(instrument);
        std::string internalName = x->m_samplerLib->getInstrumentName(instrument);
        std::string internalCategory = x->m_samplerLib->getInstrumentCategory(instrument);
        std::string instrumentSoundId = x->m_samplerLib->getMpeSoundId(instrument);
        t_atom argv[2];
        SETFLOAT(argv, instrumentId);
        SETSYMBOL(argv + 1, gensym(internalName.c_str()));
        outlet_anything(x->aux, gensym("inst"), 2, argv);
    }
}

// ==============================================
/*
 *
 */
void setInstrument(t_MuseSampler *x, t_float id) {
    DEBUG_PRINT("setInstrument");
    auto instrumentList = x->m_samplerLib->getInstrumentList();
    bool instrumentFound = false;
    while (auto instrument = x->m_samplerLib->getNextInstrument(instrumentList)) {
        int instrumentId = x->m_samplerLib->getInstrumentId(instrument);
        std::string internalName = x->m_samplerLib->getInstrumentName(instrument);
        std::string internalCategory = x->m_samplerLib->getInstrumentCategory(instrument);
        std::string instrumentSoundId = x->m_samplerLib->getMpeSoundId(instrument);
        if (instrumentId == id) {
            post("Loading instrument %s", internalName.c_str());
            x->m_instrumentInfo.instrumentId = instrumentId;
            x->m_instrumentInfo.msInstrument = instrument;
            for (size_t i = 0; i < MAX_VOICE; ++i) {
                ms_Track track = x->m_samplerLib->addTrack(x->m_sampler, instrumentId);
                x->trackList.push_back(track);
            }
            //

            // x->m_samplerLib->getTextArticulations(instrument, x->m_articulations);

            instrumentFound = true;
            post("Instrument loaded");
        }
    }
    if (!instrumentFound) {
        pd_error(x, "Instrument not found, send 'insts' message to list all IDs for available instruments");
    }
}

// ==============================================
static bool startMuseSampler(t_MuseSampler *x) {
    if (x->m_samplerLib->initSampler(x->m_sampler, x->m_sampleRate, x->renderStep, 2) != ms_Result_OK) {
        pd_error(x, "[musesampler~] Unable to init MuseSampler");
        x->started = false;
        return false;
    }

    x->started = true;
    x->m_leftChannel.resize(x->renderStep);
    x->m_rightChannel.resize(x->renderStep);

    x->m_bus._num_channels = 2;
    x->m_bus._num_data_pts = x->renderStep;

    x->m_internalBuffer[0] = x->m_leftChannel.data();
    x->m_internalBuffer[1] = x->m_rightChannel.data();
    x->m_bus._channels = x->m_internalBuffer.data();

    x->m_samplerLib->startLivePlayMode(x->m_sampler);

    post("Core Musampler Version %s", x->m_samplerLib->getVersionString());

    return true;
}
// ==============================================
static t_int *MuseSamplerPerform(t_int *w) {
    t_MuseSampler *x = (t_MuseSampler *)(w[1]);
    t_sample *lSig = (t_sample *)(w[2]);
    t_sample *rSig = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    int i;

    if (!x->m_instrumentInfo.isValid()) {
        x->sucess = false;
        return (w + 5); // Pointer to this function, x, lSig, rSig, n,
    }

    if (!x->m_samplerLib || !x->m_sampler) {
        return (w + 5);
    }

    if (x->m_isPlaying) {
        if (x->m_samplerLib->process(x->m_sampler, x->m_bus, x->m_currentPosition) != ms_Result_OK) {
            x->sucess = false;
            return (w + 5);
        }
        x->sucess = true;
    }

    // copy buffer
    if (!x->sucess) {
        for (i = 0; i < n; i++) {
            lSig[i] = 0;
            rSig[i] = 0;
        }
        return (w + 5);
    } else {
        for (i = 0; i < n; i++) {
            lSig[i] = x->m_bus._channels[0][i];
            rSig[i] = x->m_bus._channels[1][i];
        }
    }

    return (w + 5);
}

// ==============================================
static void MuseSamplerAddDsp(t_MuseSampler *x, t_signal **sp) {
    x->renderStep = sp[0]->s_n;
    x->outIndex = 0;
    x->m_currentPosition = 0;
    x->sucess = false;

    if (!x->started) {
        bool ok = startMuseSampler(x);
        if (!ok) {
            return;
        }
    }
    dsp_add(MuseSamplerPerform, 4, x, sp[0]->s_vec, sp[0]->s_vec, sp[0]->s_n);
}

// ==============================================
inline void *getLibFunc(void *libHandle, const char *funcName) {
#if defined(Q_OS_WIN) && !defined(__MINGW64__)
    return GetProcAddress((HINSTANCE)libHandle, funcName);
#else
    return dlsym(libHandle, funcName);
#endif
}

// ==============================================
static void *NewMuseSampler(t_symbol *s, int argc, t_atom *argv) {
    DEBUG_PRINT("NewMuseSampler");
    t_MuseSampler *x = (t_MuseSampler *)pd_new(MuseSampler);
    x->m_samplerLib = new MuseSamplerLibHandler();
    x->m_samplerLib->initLib();
    x->m_sampler = x->m_samplerLib->create();
    x->lSig = outlet_new(&x->xObj, &s_signal);
    x->rSig = outlet_new(&x->xObj, &s_signal);
    x->aux = outlet_new(&x->xObj, &s_anything);
    x->m_sampleRate = sys_getsr();

    x->TrackNotes = std::vector<TrackNote>(MAX_VOICE);

    // check if first argv is A_SYMBOL
    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL) {
            std::string arg = atom_getsymbol(argv + i)->s_name;
            if (arg == "-inst") {
                if (argv[i + 1].a_type == A_FLOAT) {
                    setInstrument(x, atom_getint(argv + i + 1));
                    post("Loading instrument %f", atom_getfloat(argv + i + 1));

                } else {
                    post("Invalid argument");
                }
            } else {
                post("Invalid argument");
            }
        }
    }

    return x;
}

// ==============================================
#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus)
extern "C" {
void musesampler_tilde_setup(void);
}
#endif

// ==============================================
void musesampler_tilde_setup(void) {
    MuseSampler = class_new(gensym("musesampler~"), (t_newmethod)NewMuseSampler, NULL, sizeof(t_MuseSampler),
                            CLASS_DEFAULT, A_GIMME, 0);

    class_addmethod(MuseSampler, (t_method)MuseSamplerAddDsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(MuseSampler, (t_method)getInstrumentList, gensym("insts"), A_NULL, 0);

    class_addmethod(MuseSampler, (t_method)AllNotesOff, gensym("reset"), A_NULL, 0);
    class_addmethod(MuseSampler, (t_method)NoteOn, gensym("noteon"), A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(MuseSampler, (t_method)NoteOff, gensym("noteoff"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(MuseSampler, (t_method)Articulation, gensym("art"), A_FLOAT, 0);
    class_addmethod(MuseSampler, (t_method)Reverb, gensym("reverb"), A_FLOAT, 0);

    class_addmethod(MuseSampler, (t_method)setInstrument, gensym("set"), A_FLOAT, 0);
}
