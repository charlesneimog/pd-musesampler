#include <m_pd.h>

#include "Musescore/apitypes.h" // Downloaded by Cmake

#include <array>
#include <string>
#include <unistd.h>
#include <vector>

#if _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <chrono>
#include <iostream>

// #ifdef DEBUG
// #define DEBUG_PRINT(message) printf("[DEBUG] %s\n", message)
// #else
#define DEBUG_PRINT(message) // Define como vazio em builds sem debug
// #endif

static t_class *MuseSampler;
#define MAX_VOICE 64

struct MuseSamplerLibFunctions {
    ms_MuseSampler_init initSampler = nullptr;
    ms_get_version_string getVersionString = nullptr;

    ms_init initLib = nullptr;
    ms_MuseSampler_create create = nullptr;
    ms_MuseSampler_destroy destroy = nullptr;

    ms_get_instrument_list getInstrumentList = nullptr;
    ms_InstrumentList_get_next getNextInstrument = nullptr;
    ms_Instrument_get_id getInstrumentId = nullptr;
    ms_Instrument_get_name getInstrumentName = nullptr;
    ms_Instrument_get_category getInstrumentCategory = nullptr;
    ms_Instrument_get_mpe_sound getMpeSoundId = nullptr;

    ms_MuseSampler_start_audition_note_2 startAuditionNote = nullptr;
    ms_MuseSampler_stop_audition_note stopAuditionNote = nullptr;
    ms_MuseSampler_all_notes_off allNotesOff = nullptr;

    ms_MuseSampler_start_liveplay_mode startLivePlayMode = nullptr;
    ms_MuseSampler_stop_liveplay_mode stopLivePlayMode = nullptr;
    ms_MuseSampler_start_liveplay_note_2 startLivePlayNote = nullptr;

    ms_disable_reverb disableReverb = nullptr; // TODO: REMOVE
    ms_MuseSampler_add_track addTrack = nullptr;
    ms_MuseSampler_clear_track finalizeTrack = nullptr;
    ms_MuseSampler_clear_track clearTrack = nullptr;
    ms_MuseSampler_process process = nullptr;

    // playback
    ms_MuseSampler_set_playing setPlaying = nullptr;
    ms_MuseSampler_set_position setPosition = nullptr;
};

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

    MuseSamplerLibFunctions *museLib = nullptr;
    ms_MuseSampler m_sampler = nullptr;
    InstrumentInfo m_instrument;
    ms_Track Track;

    // ==============================================
    ms_LivePlayStartNoteEvent_2 eventList[MAX_VOICE];
    int eventIndex = 0;

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
    bool starting = false;
    float m_currentPosition = 0;
    int outIndex;
    int renderStep;
    int settedStep;

    std::vector<t_sample> m_leftChannel;
    std::vector<t_sample> m_rightChannel;

    t_outlet *lSig;
    t_outlet *rSig;

    t_outlet *aux;
} t_MuseSampler;

static bool startMuseSampler(t_MuseSampler *x);

// ╭─────────────────────────────────────╮
// │      Helpers to Load MuseSound      │
// ╰─────────────────────────────────────╯
//

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
 * This function validates if the MuseSampler lib has all the necessary functions
 */
bool ValidateMuseLib(const MuseSamplerLibFunctions *Funcs) {
    return (Funcs->initSampler && Funcs->getVersionString && Funcs->initLib && Funcs->create &&
            Funcs->destroy && Funcs->getInstrumentList && Funcs->getNextInstrument &&
            Funcs->getInstrumentId && Funcs->clearTrack && Funcs->getInstrumentName &&
            Funcs->getInstrumentCategory && Funcs->getMpeSoundId && Funcs->startAuditionNote &&
            Funcs->finalizeTrack && Funcs->stopAuditionNote && Funcs->allNotesOff &&
            Funcs->startLivePlayMode && Funcs->stopLivePlayMode && Funcs->disableReverb && Funcs->addTrack &&
            Funcs->process);
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
#ifdef _WIN32
    void *m_lib = LoadLibrary(path.c_str());
#else
    void *m_lib = dlopen(path.c_str(), RTLD_LAZY);
#endif
    if (m_lib) {
        ms_init initLib = (ms_init)getLibFunc(m_lib, "ms_init");
        if (initLib) {
            ms_Result res = initLib();
            if (res != ms_Result_OK) {
                pd_error(x, "[musesampler~] Failed to initialize MuseSampler");
                return false;
            }
        } else {
            pd_error(x, "[musesampler~] Failed to find init function");
            return false;
        }
    }

    x->museLib->getVersionString = (ms_get_version_string)getLibFunc(m_lib, "ms_get_version_string");

    ms_get_version_major getVersionMajor = (ms_get_version_major)getLibFunc(m_lib, "ms_get_version_major");
    ms_get_version_minor getVersionMinor = (ms_get_version_minor)getLibFunc(m_lib, "ms_get_version_minor");

    int major = getVersionMajor();
    int minor = getVersionMinor();

    bool at_least_v_0_5 = (major == 0 && minor >= 5) || major > 0;

    if (!at_least_v_0_5) {
        pd_error(x, "[musesampler~] MuseSampler version is too old, please update");
        return false;
    }

    x->museLib->initSampler = (ms_MuseSampler_init)getLibFunc(m_lib, "ms_MuseSampler_init");
    x->museLib->getVersionString = (ms_get_version_string)getLibFunc(m_lib, "ms_get_version_string");

    x->museLib->initLib = (ms_init)getLibFunc(m_lib, "ms_init");
    x->museLib->create = (ms_MuseSampler_create)getLibFunc(m_lib, "ms_MuseSampler_create");
    x->museLib->destroy = (ms_MuseSampler_destroy)getLibFunc(m_lib, "ms_MuseSampler_destroy");

    x->museLib->getInstrumentList = (ms_get_instrument_list)getLibFunc(m_lib, "ms_get_instrument_list");
    x->museLib->getNextInstrument =
        (ms_InstrumentList_get_next)getLibFunc(m_lib, "ms_InstrumentList_get_next");
    x->museLib->getInstrumentId = (ms_Instrument_get_id)getLibFunc(m_lib, "ms_Instrument_get_id");
    x->museLib->getInstrumentName = (ms_Instrument_get_name)getLibFunc(m_lib, "ms_Instrument_get_name");
    x->museLib->getInstrumentCategory =
        (ms_Instrument_get_category)getLibFunc(m_lib, "ms_Instrument_get_category");
    x->museLib->getMpeSoundId = (ms_Instrument_get_mpe_sound)getLibFunc(m_lib, "ms_Instrument_get_mpe_sound");

    x->museLib->startAuditionNote =
        (ms_MuseSampler_start_audition_note_2)getLibFunc(m_lib, "ms_MuseSampler_start_audition_note_2");
    x->museLib->stopAuditionNote =
        (ms_MuseSampler_stop_audition_note)getLibFunc(m_lib, "ms_MuseSampler_stop_audition_note");
    x->museLib->allNotesOff = (ms_MuseSampler_all_notes_off)getLibFunc(m_lib, "ms_MuseSampler_all_notes_off");

    x->museLib->startLivePlayMode =
        (ms_MuseSampler_start_liveplay_mode)getLibFunc(m_lib, "ms_MuseSampler_start_liveplay_mode");
    x->museLib->stopLivePlayMode =
        (ms_MuseSampler_stop_liveplay_mode)getLibFunc(m_lib, "ms_MuseSampler_stop_liveplay_mode");

    x->museLib->startLivePlayNote =
        (ms_MuseSampler_start_liveplay_note_2)getLibFunc(m_lib, "ms_MuseSampler_start_liveplay_note_2");

    x->museLib->disableReverb = (ms_disable_reverb)getLibFunc(m_lib, "ms_disable_reverb");
    x->museLib->addTrack = (ms_MuseSampler_add_track)getLibFunc(m_lib, "ms_MuseSampler_add_track");
    x->museLib->clearTrack = (ms_MuseSampler_clear_track)getLibFunc(m_lib, "ms_MuseSampler_clear_track");
    x->museLib->finalizeTrack =
        (ms_MuseSampler_finalize_track)getLibFunc(m_lib, "ms_MuseSampler_finalize_track");
    x->museLib->process = (ms_MuseSampler_process)getLibFunc(m_lib, "ms_MuseSampler_process");

    x->museLib->setPlaying = (ms_MuseSampler_set_playing)getLibFunc(m_lib, "ms_MuseSampler_set_playing");
    x->museLib->setPosition = (ms_MuseSampler_set_position)getLibFunc(m_lib, "ms_MuseSampler_set_position");

    if (!ValidateMuseLib(x->museLib)) {
        pd_error(x, "[musesampler~] MuseSampler lib is not valid");
        return false;
    }
    x->museLib->initLib();
    x->m_sampler = x->museLib->create();

    return true;
}

// ╭─────────────────────────────────────╮
// │          PureData Methods           │
// ╰─────────────────────────────────────╯
// ==============================================
/*
 * Stop all notes playing
 */
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
/*
 * Note Off
 */
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
/*
 * Note On
 */
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
/*
 * Articulation seems to be possible in version 0.6
 */
void Articulation(t_MuseSampler *x, t_float art) {
    DEBUG_PRINT("Articulation");

    if (!x->started) {
        pd_error(x, "[musesampler~] Sampler not initialized");
        return;
    }
    x->articulation = 1LL << (int)art;
}

// ==============================================
/*
 * Just possible to disable reverb
 */
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
/*
 * This will print all the installed instruments
 */
void Get(t_MuseSampler *x, t_symbol *s) {
    std::string method = s->s_name;
    if (method == "instruments") {
        DEBUG_PRINT("getInstrumentList");
        auto instrumentList = x->museLib->getInstrumentList();
        while (auto instrument = x->museLib->getNextInstrument(instrumentList)) {
            int instrumentId = x->museLib->getInstrumentId(instrument);
            std::string internalName = x->museLib->getInstrumentName(instrument);
            std::string internalCategory = x->museLib->getInstrumentCategory(instrument);
            std::string instrumentSoundId = x->museLib->getMpeSoundId(instrument);
            t_atom argv[2];
            SETFLOAT(argv, instrumentId);
            SETSYMBOL(argv + 1, gensym(internalName.c_str()));
            outlet_anything(x->aux, gensym("inst"), 2, argv);
        }
    }
}

// ==============================================
/*
 * This will set the instrument to the sampler, BUG: just possible once
 */
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
    bool instrumentFound = false;
    while (auto instrument = x->museLib->getNextInstrument(instrumentList)) {
        int instrumentId = x->museLib->getInstrumentId(instrument);
        std::string internalName = x->museLib->getInstrumentName(instrument);
        std::string internalCategory = x->museLib->getInstrumentCategory(instrument);
        std::string instrumentSoundId = x->museLib->getMpeSoundId(instrument);
        if (instrumentId == id) {
            x->m_instrumentInfo.instrumentId = instrumentId;
            x->m_instrumentInfo.msInstrument = instrument;
            for (size_t i = 0; i < MAX_VOICE; ++i) {
                // BUG: MuseSounds Just support 1 track (version < 0.6)
                ms_Track track = x->museLib->addTrack(x->m_sampler, instrumentId);
                x->trackList.push_back(track);
            }
            startMuseSampler(x);
            instrumentFound = true;
            post("[musesampler~] %s Loaded!", internalName.c_str());
        }
    }

    if (!instrumentFound) {
        pd_error(x, "[musesampler~] Instrument not found, send 'insts' message to list all IDs for available "
                    "instruments");
        return;
    }
}

// ==============================================
/*
 * This will start the MuseSampler
 */
static bool startMuseSampler(t_MuseSampler *x) {
    if (x->renderStep == 0) {
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

    x->museLib->setPosition(x->m_sampler, 0);
    x->museLib->startLivePlayMode(x->m_sampler);

    x->started = true;

    return true;
}
// ==============================================
/* Perform Method */
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

    if (!x->museLib || !x->m_sampler) {
        return (w + 5);
    }

    for (int i = 0; i < x->eventIndex; i++) {
        x->museLib->startLivePlayNote(x->m_sampler, x->Track, x->eventList[i]);
    }
    x->eventIndex = 0;

    if (x->m_isPlaying) {
        if (x->museLib->process(x->m_sampler, x->m_bus, x->renderStep) != ms_Result_OK) {
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
/* Add DSP Method */
static void MuseSamplerAddDsp(t_MuseSampler *x, t_signal **sp) {
    x->renderStep = sp[0]->s_n;
    x->m_currentPosition = 0;
    x->sucess = false;

    if (!x->started) {
        startMuseSampler(x);
        x->museLib->setPlaying(x->m_sampler, false);
    }

    dsp_add(MuseSamplerPerform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

// ==============================================
/* Create PureData Object */
static void *NewMuseSampler(t_symbol *s, int argc, t_atom *argv) {
    DEBUG_PRINT("NewMuseSampler");
    t_MuseSampler *x = (t_MuseSampler *)pd_new(MuseSampler);

    x->museLib = new MuseSamplerLibFunctions();
    if (!LoadMuseLib(x)) { // TODO: Thing about multithreading
        return nullptr;
    }
    post("[musesampler~] Musampler Version %s", x->museLib->getVersionString());

    x->lSig = outlet_new(&x->xObj, &s_signal);
    x->rSig = outlet_new(&x->xObj, &s_signal);
    x->aux = outlet_new(&x->xObj, &s_anything);
    x->m_sampleRate = sys_getsr();

    x->TrackNotes = std::vector<TrackNote>(MAX_VOICE);

    // check if first argv is A_SYMBOL
    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL) {
            std::string arg = atom_getsymbol(argv + i)->s_name;
            if (arg == "-inst" || arg == "-i") {
                if (argv[i + 1].a_type == A_FLOAT) {
                    setInstrument(x, atom_getint(argv + i + 1));

                } else {
                    post("[musesampler~] Invalid argument");
                }
            } else {
                post("[musesampler~] Invalid argument");
            }
        }
    }

    return x;
}

// ==============================================
/* When the object is deleted */
static void *FreeMuseSampler(t_MuseSampler *x) {
    x->museLib->destroy(x->m_sampler);
    delete x->museLib;
    return nullptr;
}

// ==============================================
#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus)
extern "C" {
void musesampler_tilde_setup(void);
}
#endif

// ==============================================
/* Setup the object */
void musesampler_tilde_setup(void) {
    MuseSampler = class_new(gensym("musesampler~"), (t_newmethod)NewMuseSampler, NULL, sizeof(t_MuseSampler),
                            CLASS_DEFAULT, A_GIMME, 0);

    class_addmethod(MuseSampler, (t_method)MuseSamplerAddDsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(MuseSampler, (t_method)Get, gensym("get"), A_SYMBOL, 0);

    class_addmethod(MuseSampler, (t_method)AllNotesOff, gensym("reset"), A_NULL, 0);
    class_addmethod(MuseSampler, (t_method)NoteOn, gensym("noteon"), A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(MuseSampler, (t_method)NoteOff, gensym("noteoff"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(MuseSampler, (t_method)Articulation, gensym("art"), A_FLOAT, 0);
    class_addmethod(MuseSampler, (t_method)Reverb, gensym("reverb"), A_FLOAT, 0);

    class_addmethod(MuseSampler, (t_method)setInstrument, gensym("set"), A_FLOAT, 0);
    post("[musesampler~] Pd version by Charles K. Neimog");
    post("[musesampler~] version 0.0.1");
}
