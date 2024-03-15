#include "musampler.h"

static t_class *Synth;

// ==============================================
typedef struct _Synth {
    t_object xObj;

    MuseSamplerLibHandler *museHandler;

    t_outlet *outlet;
} t_MuseSampler;

// ==============================================
static t_int *MuseSamplerPerform(t_int *w) {
    t_MuseSampler *x = (t_MuseSampler *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);
    int i;

    for (i = 0; i < n; i++) {
        out[i] = 0;
    }

    return (w + 4);
}

// ==============================================
static void SynthAddDsp(t_MuseSampler *x, t_signal **sp) {
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
static void OpenMuseSampler(t_MuseSampler *x, std::string path) {
    void *handle = dlopen(path.c_str(), RTLD_LAZY);
    x->museHandler->initLib = (ms_init)getLibFunc(handle, "ms_init");
    if (x->museHandler->initLib) {
        ms_Result res = x->museHandler->initLib();
        if (res != ms_Result_OK) {
            pd_error(x, "Failed to initialize MuseSampler");
            return;
        } else {
            post("MuseSampler initialized");
        }
    } else {
        pd_error(x, "Failed to find init function");
        return;
    }

    //     getVersionMajor =
    //     (ms_get_version_major)getLibFunc(m_lib, "ms_get_version_major");
    // getVersionMinor =
    //     (ms_get_version_minor)getLibFunc(m_lib, "ms_get_version_minor");
    // getVersionRevision =
    //     (ms_get_version_revision)getLibFunc(m_lib,
    //     "ms_get_version_revision");
    // getVersionString =
    //     (ms_get_version_string)getLibFunc(m_lib, "ms_get_version_string");

    x->museHandler->getVersionString = (ms_get_version_string)getLibFunc(handle, "ms_get_version_string");

    post("MuseSampler version major: %s", x->museHandler->getVersionString());

    x->museHandler->getInstrumentList = (ms_get_instrument_list)getLibFunc(handle, "ms_get_instrument_list");
}

// ==============================================
static void *NewMuseSampler(t_symbol *synth) {
    t_MuseSampler *x = (t_MuseSampler *)pd_new(Synth);
    x->museHandler = new MuseSamplerLibHandler();

    post("Version of MuseSampler: %s", x->museHandler->getVersionString());

    x->outlet = outlet_new(&x->xObj, &s_signal);

    auto instrumentList = x->museHandler->getInstrumentList();

    while (auto instrument = x->museHandler->getNextInstrument(instrumentList)) {
        int instrumentId = x->museHandler->getInstrumentId(instrument);
        std::string internalName = x->museHandler->getInstrumentName(instrument);
        std::string internalCategory = x->museHandler->getInstrumentCategory(instrument);
        std::string instrumentSoundId = x->museHandler->getMpeSoundId(instrument);

        post("Instrument: %s, %s, %s", internalName.c_str(), internalCategory.c_str(),
             instrumentSoundId.c_str());
    }

    // ======
    //
    // : AbstractSynthesizer(params), m_samplerLib(samplerLib), m_instrument(instrument)

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
    Synth = class_new(gensym("musesampler~"), (t_newmethod)NewMuseSampler, NULL, sizeof(t_MuseSampler),
                      CLASS_DEFAULT, A_DEFSYMBOL, 0);
}
