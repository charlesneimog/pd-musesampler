#include "musampler.h"

static t_class *MuseSampler;

#define MAX_POLYPHONY 64

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

    // MuseSamplerSequencer m_sequencer;
    InstrumentInfo m_instrumentInfo;
    ms_OutputBuffer m_bus;

    unsigned int m_sampleRate;
    bool m_isPlaying = false;
    bool instrumentAdded = true;
    std::array<float *, 2> m_internalBuffer;

    bool sucess = false;
    float m_currentPosition = 0;
    int outIndex;
    int renderStep;

    std::vector<t_sample> m_leftChannel;
    std::vector<t_sample> m_rightChannel;

    ms_LivePlayStartNoteEvent_2 eventArray[MAX_POLYPHONY];
    int eventIndex = 0;

    t_outlet *signal;
    t_outlet *aux;
} t_MuseSampler;

// ==============================================
void NoteOff(t_MuseSampler *x, t_float note, t_float velocity) {
    printf("NoteOff\n");
    ms_LivePlayStopNoteEvent event = ms_LivePlayStopNoteEvent();
    event._pitch = note;

    if (x->m_samplerLib->stopLivePlayNote(x->m_sampler, x->Track, event) != ms_Result_OK) {
        pd_error(x, "Start Live Play Note Failed");
    }

    x->m_currentPosition = 0;
    x->m_isPlaying = true;
}

// ==============================================
void NoteOn(t_MuseSampler *x, t_float note, t_float velocity) {

    printf("NoteOn\n");
    ms_LivePlayStartNoteEvent_2 event = ms_LivePlayStartNoteEvent_2();
    event._pitch = note;
    event._dynamics = 0.5;
    event._offset_cents = 0;
    x->m_samplerLib->startLivePlayNote(x->m_sampler, x->Track, event);

    x->eventArray[x->eventIndex] = event;
    x->eventIndex++;

    if (x->eventIndex >= MAX_POLYPHONY) {
        x->eventIndex = 0;
    }

    x->m_currentPosition = 0;
    x->m_isPlaying = true;
}

// ==============================================
void getInstrumentList(t_MuseSampler *x) {
    printf("getInstrumentList\n");
    auto instrumentList = x->m_samplerLib->getInstrumentList();
    while (auto instrument = x->m_samplerLib->getNextInstrument(instrumentList)) {
        int instrumentId = x->m_samplerLib->getInstrumentId(instrument);
        std::string internalName = x->m_samplerLib->getInstrumentName(instrument);
        std::string internalCategory = x->m_samplerLib->getInstrumentCategory(instrument);
        std::string instrumentSoundId = x->m_samplerLib->getMpeSoundId(instrument);
        t_atom t_atom[2];
        SETSYMBOL(&t_atom[0], gensym(internalName.c_str()));
        SETFLOAT(&t_atom[1], instrumentId);
        outlet_anything(x->aux, gensym("instrument"), 2, t_atom);
    }
}

// ==============================================
void setInstrument(t_MuseSampler *x, t_float id) {
    printf("setInstrument\n");
    auto instrumentList = x->m_samplerLib->getInstrumentList();
    while (auto instrument = x->m_samplerLib->getNextInstrument(instrumentList)) {
        int instrumentId = x->m_samplerLib->getInstrumentId(instrument);
        std::string internalName = x->m_samplerLib->getInstrumentName(instrument);
        std::string internalCategory = x->m_samplerLib->getInstrumentCategory(instrument);
        std::string instrumentSoundId = x->m_samplerLib->getMpeSoundId(instrument);

        if (instrumentId == id) {
            x->m_instrumentInfo.instrumentId = instrumentId;
            x->m_instrumentInfo.msInstrument = instrument;
            ms_Track track = x->m_samplerLib->addTrack(x->m_sampler, instrumentId);
        }
    }
}

// ==============================================
t_sample *process(t_MuseSampler *x) {
    printf("precess\n");
    if (!x->m_samplerLib || !x->m_sampler) {
        return nullptr;
    }

    if (!x->m_instrumentInfo.isValid()) {
        pd_error(x, "Instrument not set");
        x->sucess = false;
        return nullptr;
    }

    if (x->m_isPlaying) {
        if (x->m_samplerLib->process(x->m_sampler, x->m_bus, x->m_currentPosition) != ms_Result_OK) {
            x->sucess = false;
            return nullptr;
        }
        x->sucess = true;
    }
    return nullptr;
}

// ==============================================
static t_int *MuseSamplerPerform(t_int *w) {
    printf("NewMuseSamplerPerform\n");
    t_MuseSampler *x = (t_MuseSampler *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    int i;

    process(x);

    if (!x->sucess) {
        for (i = 0; i < n; i++) {
            out[i] = 0;
            x->outIndex += 1;
        }
        return (w + 4);
    } else {
        for (i = 0; i < n; i++) {
            out[i] = x->m_bus._channels[0][i];
            x->outIndex += 1;
        }
    }

    return (w + 4);
}

// ==============================================
static void SynthAddDsp(t_MuseSampler *x, t_signal **sp) {
    x->renderStep = sp[0]->s_n;
    x->outIndex = 0;
    x->m_currentPosition = 0;
    x->sucess = false;

    // TODO: APOS O INSTRUMENTO SER VÁLIDO
    if (x->m_samplerLib->initSampler(x->m_sampler, x->m_sampleRate, x->renderStep, 2) != ms_Result_OK) {
        pd_error(x, "[musesampler~] Unable to init MuseSampler");
        return;
    }

    x->m_leftChannel.resize(x->renderStep);
    x->m_rightChannel.resize(x->renderStep);

    x->m_bus._num_channels = 2;
    x->m_bus._num_data_pts = x->renderStep;

    x->m_internalBuffer[0] = x->m_leftChannel.data();
    x->m_internalBuffer[1] = x->m_rightChannel.data();
    x->m_bus._channels = x->m_internalBuffer.data();

    x->m_samplerLib->startLivePlayMode(x->m_sampler);

    dsp_add(MuseSamplerPerform, 3, x, sp[0]->s_vec, sp[0]->s_n);
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
static void *NewMuseSampler(t_float inst) {
    printf("NewMuseSampler\n");
    t_MuseSampler *x = (t_MuseSampler *)pd_new(MuseSampler);
    x->m_samplerLib = new MuseSamplerLibHandler();
    x->m_samplerLib->initLib();

    x->m_sampler = x->m_samplerLib->create();

    x->signal = outlet_new(&x->xObj, &s_signal);
    x->aux = outlet_new(&x->xObj, &s_anything);

    x->m_sampleRate = sys_getsr();

    if (inst > 0) {
        setInstrument(x, inst);
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
                            CLASS_DEFAULT, A_DEFFLOAT, 0);

    class_addmethod(MuseSampler, (t_method)SynthAddDsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(MuseSampler, (t_method)getInstrumentList, gensym("instrument"), A_NULL, 0);
    class_addmethod(MuseSampler, (t_method)NoteOn, gensym("noteon"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(MuseSampler, (t_method)NoteOff, gensym("noteoff"), A_FLOAT, A_FLOAT, 0);

    class_addmethod(MuseSampler, (t_method)setInstrument, gensym("set"), A_FLOAT, 0);
}
