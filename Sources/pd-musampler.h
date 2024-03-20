#include <m_pd.h>

#include "apitypes.h"

#include <dlfcn.h>
#include <functional>
#include <string>

#if defined(Q_OS_WIN) && !defined(__MINGW64__)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

void *getLibFunc(void *libHandle, const char *funcName);

struct MuseSamplerLibHandler {
    void *m_lib = nullptr;
    ms_init initLib = nullptr;
    ms_disable_reverb disableReverb = nullptr;
    ms_get_version_major getVersionMajor = nullptr;
    ms_get_version_minor getVersionMinor = nullptr;
    ms_get_version_revision getVersionRevision = nullptr;
    ms_get_version_string getVersionString = nullptr;

    ms_contains_instrument containsInstrument = nullptr;

    ms_get_matching_instrument_id getMatchingInstrumentId = nullptr;
    ms_get_instrument_list getInstrumentList = nullptr;
    ms_get_matching_instrument_list getMatchingInstrumentList = nullptr;
    ms_InstrumentList_get_next getNextInstrument = nullptr;
    ms_Instrument_get_id getInstrumentId = nullptr;
    ms_Instrument_get_name getInstrumentName = nullptr;
    ms_Instrument_get_category getInstrumentCategory = nullptr;
    ms_Instrument_get_package getInstrumentPackage = nullptr;
    ms_Instrument_get_musicxml_sound getMusicXmlSoundId = nullptr;
    ms_Instrument_get_mpe_sound getMpeSoundId = nullptr;
    ms_Instrument_get_reverb_level getReverbLevel = nullptr;

    ms_Instrument_get_preset_list getPresetList = nullptr;
    ms_PresetList_get_next getNextPreset = nullptr;

    ms_MuseSampler_create create = nullptr;
    ms_MuseSampler_destroy destroy = nullptr;
    ms_MuseSampler_init initSampler = nullptr;

    ms_MuseSampler_clear_score clearScore = nullptr;
    ms_MuseSampler_add_track addTrack = nullptr;
    ms_MuseSampler_finalize_track finalizeTrack = nullptr;
    ms_MuseSampler_clear_track clearTrack = nullptr;

    std::function<bool(ms_MuseSampler ms, ms_Track track, long long timestamp, float value)>
        addDynamicsEvent = nullptr;
    std::function<bool(ms_MuseSampler ms, ms_Track track, long long timestamp, float value)> addPedalEvent =
        nullptr;
    std::function<bool(ms_MuseSampler ms, ms_Track track, int voice, long long location_us,
                       long long duration_us, int pitch, double tempo, int offset_cents,
                       ms_NoteArticulation articulation, long long &event_id)>
        addNoteEvent = nullptr;
    ms_MuseSampler_is_ranged_articulation isRangedArticulation = nullptr;
    ms_MuseSampler_add_track_event_range_start addTrackEventRangeStart = nullptr;
    ms_MuseSampler_add_track_event_range_end addTrackEventRangeEnd = nullptr;

    ms_MuseSampler_add_pitch_bend addPitchBend = nullptr;
    ms_MuseSampler_add_vibrato addVibrato = nullptr;

    std::function<bool(ms_MuseSampler ms, ms_Track track, ms_AuditionStartNoteEvent_2)> startAuditionNote =
        nullptr;
    ms_MuseSampler_stop_audition_note stopAuditionNote = nullptr;

    ms_MuseSampler_start_liveplay_mode startLivePlayMode = nullptr;
    ms_MuseSampler_stop_liveplay_mode stopLivePlayMode = nullptr;
    std::function<bool(ms_MuseSampler ms, ms_Track track, ms_LivePlayStartNoteEvent_2)> startLivePlayNote =
        nullptr;
    ms_MuseSampler_stop_liveplay_note stopLivePlayNote = nullptr;

    ms_MuseSampler_start_offline_mode startOfflineMode = nullptr;
    ms_MuseSampler_stop_offline_mode stopOfflineMode = nullptr;
    ms_MuseSampler_process_offline processOffline = nullptr;

    ms_MuseSampler_set_position setPosition = nullptr;
    ms_MuseSampler_set_playing setPlaying = nullptr;
    ms_MuseSampler_process process = nullptr;
    ms_MuseSampler_all_notes_off allNotesOff = nullptr;

  private:
    ms_MuseSampler_add_track_dynamics_event addDynamicsEventInternal = nullptr;
    ms_MuseSampler_add_track_dynamics_event_2 addDynamicsEventInternal2 = nullptr;
    ms_MuseSampler_add_track_pedal_event addPedalEventInternal = nullptr;
    ms_MuseSampler_add_track_pedal_event_2 addPedalEventInternal2 = nullptr;
    ms_MuseSampler_add_track_note_event addNoteEventInternal = nullptr;
    ms_MuseSampler_add_track_note_event_2 addNoteEventInternal2 = nullptr;
    ms_MuseSampler_add_track_note_event_3 addNoteEventInternal3 = nullptr;
    ms_MuseSampler_add_track_note_event_4 addNoteEventInternal4 = nullptr;
    ms_MuseSampler_start_audition_note startAuditionNoteInternal = nullptr;
    ms_MuseSampler_start_audition_note_2 startAuditionNoteInternal2 = nullptr;
    ms_MuseSampler_start_liveplay_note startLivePlayNoteInternal = nullptr;
    ms_MuseSampler_start_liveplay_note_2 startLivePlayNoteInternal2 = nullptr;

  public:
    MuseSamplerLibHandler() {

        // TODO: Fix this for Windows and Max
        std::string path = "/home/neimog/.local/share/MuseSampler/lib/"
                           "libMuseSamplerCoreLib.so";

        void *m_lib = dlopen(path.c_str(), RTLD_LAZY);
        if (m_lib) {
            initLib = (ms_init)getLibFunc(m_lib, "ms_init");
            if (initLib) {
                ms_Result res = initLib();
                if (res != ms_Result_OK) {
                    pd_error(NULL, "Failed to initialize MuseSampler");
                    return;
                } else {
                    post("MuseSampler initialized");
                }
            } else {
                pd_error(NULL, "Failed to find init function");
                return;
            }
        }

        getVersionMajor = (ms_get_version_major)getLibFunc(m_lib, "ms_get_version_major");
        getVersionMinor = (ms_get_version_minor)getLibFunc(m_lib, "ms_get_version_minor");
        getVersionRevision = (ms_get_version_revision)getLibFunc(m_lib, "ms_get_version_revision");
        getVersionString = (ms_get_version_string)getLibFunc(m_lib, "ms_get_version_string");

        // Invalid...
        if (!getVersionMajor || !getVersionMinor || !getVersionRevision) {
            return;
        }

        int versionMajor = getVersionMajor();
        int versionMinor = getVersionMinor();

        bool at_least_v_0_3 = (versionMajor == 0 && versionMinor >= 3) || versionMajor > 0;
        bool at_least_v_0_4 = (versionMajor == 0 && versionMinor >= 4) || versionMajor > 0;
        bool at_least_v_0_5 = (versionMajor == 0 && versionMinor >= 5) || versionMajor > 0;

        containsInstrument = (ms_contains_instrument)getLibFunc(m_lib, "ms_contains_instrument");
        getMatchingInstrumentId =
            (ms_get_matching_instrument_id)getLibFunc(m_lib, "ms_get_matching_instrument_id");
        getInstrumentList = (ms_get_instrument_list)getLibFunc(m_lib, "ms_get_instrument_list");
        getMatchingInstrumentList =
            (ms_get_matching_instrument_list)getLibFunc(m_lib, "ms_get_matching_instrument_list");
        getNextInstrument = (ms_InstrumentList_get_next)getLibFunc(m_lib, "ms_InstrumentList_get_next");
        getInstrumentId = (ms_Instrument_get_id)getLibFunc(m_lib, "ms_Instrument_get_id");
        getInstrumentName = (ms_Instrument_get_name)getLibFunc(m_lib, "ms_Instrument_get_name");
        getInstrumentCategory = (ms_Instrument_get_category)getLibFunc(m_lib, "ms_Instrument_get_category");
        getInstrumentPackage = (ms_Instrument_get_package)getLibFunc(m_lib, "ms_Instrument_get_package");
        getMusicXmlSoundId =
            (ms_Instrument_get_musicxml_sound)getLibFunc(m_lib, "ms_Instrument_get_musicxml_sound");
        getMpeSoundId = (ms_Instrument_get_mpe_sound)getLibFunc(m_lib, "ms_Instrument_get_mpe_sound");

        getPresetList = (ms_Instrument_get_preset_list)getLibFunc(m_lib, "ms_Instrument_get_preset_list");
        getNextPreset = (ms_PresetList_get_next)getLibFunc(m_lib, "ms_PresetList_get_next");

        create = (ms_MuseSampler_create)getLibFunc(m_lib, "ms_MuseSampler_create");
        destroy = (ms_MuseSampler_destroy)getLibFunc(m_lib, "ms_MuseSampler_destroy");
        initSampler = (ms_MuseSampler_init)getLibFunc(m_lib, "ms_MuseSampler_init");

        clearScore = (ms_MuseSampler_clear_score)getLibFunc(m_lib, "ms_MuseSampler_clear_score");
        addTrack = (ms_MuseSampler_add_track)getLibFunc(m_lib, "ms_MuseSampler_add_track");
        finalizeTrack = (ms_MuseSampler_finalize_track)getLibFunc(m_lib, "ms_MuseSampler_finalize_track");
        clearTrack = (ms_MuseSampler_clear_track)getLibFunc(m_lib, "ms_MuseSampler_clear_track");

        if (at_least_v_0_4) {
            if (addDynamicsEventInternal2 = (ms_MuseSampler_add_track_dynamics_event_2)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_dynamics_event_2");
                addDynamicsEventInternal2 != nullptr) {
                addDynamicsEvent = [this](ms_MuseSampler ms, ms_Track track, long long timestamp,
                                          float value) {
                    ms_DynamicsEvent_2 evt{timestamp, value};
                    return addDynamicsEventInternal2(ms, track, evt) == ms_Result_OK;
                };
            }
            disableReverb = (ms_disable_reverb)getLibFunc(m_lib, "ms_disable_reverb");
            getReverbLevel =
                (ms_Instrument_get_reverb_level)getLibFunc(m_lib, "ms_Instrument_get_reverb_level");
        } else {
            if (addDynamicsEventInternal = (ms_MuseSampler_add_track_dynamics_event)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_dynamics_event");
                addDynamicsEventInternal != nullptr) {
                addDynamicsEvent = [this](ms_MuseSampler ms, ms_Track track, long long timestamp,
                                          float value) {
                    ms_DynamicsEvent evt{static_cast<long>(timestamp), value};
                    return addDynamicsEventInternal(ms, track, evt) == ms_Result_OK;
                };
            }
        }

        if (at_least_v_0_4) {
            if (addPedalEventInternal2 = (ms_MuseSampler_add_track_pedal_event_2)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_pedal_event_2");
                addPedalEventInternal2 != nullptr) {
                addPedalEvent = [this](ms_MuseSampler ms, ms_Track track, long long timestamp, float value) {
                    ms_PedalEvent_2 evt{timestamp, value};
                    return addPedalEventInternal2(ms, track, evt) == ms_Result_OK;
                };
            }
        } else {
            if (addPedalEventInternal = (ms_MuseSampler_add_track_pedal_event)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_pedal_event");
                addPedalEventInternal != nullptr) {
                addPedalEvent = [this](ms_MuseSampler ms, ms_Track track, long long timestamp, float value) {
                    // TODO: down cast of long long? trim?
                    ms_PedalEvent evt{static_cast<long>(timestamp), value};
                    return addPedalEventInternal(ms, track, evt) == ms_Result_OK;
                };
            }
        }

        if (at_least_v_0_5) {
            if (addNoteEventInternal4 = (ms_MuseSampler_add_track_note_event_4)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_note_event_4");
                addNoteEventInternal4 != nullptr) {
                addNoteEvent = [this](ms_MuseSampler ms, ms_Track track, int voice, long long location_us,
                                      long long duration_us, int pitch, double tempo, int offset_cents,
                                      ms_NoteArticulation articulation, long long &event_id) {
                    ms_NoteEvent_3 evt{voice, location_us,  duration_us, pitch,
                                       tempo, offset_cents, articulation};
                    return addNoteEventInternal4(ms, track, evt, event_id) == ms_Result_OK;
                };
            }

            addPitchBend = (ms_MuseSampler_add_pitch_bend)getLibFunc(m_lib, "ms_MuseSampler_add_pitch_bend");
            addVibrato = (ms_MuseSampler_add_vibrato)getLibFunc(m_lib, "ms_MuseSampler_add_vibrato");
        } else if (at_least_v_0_4) {
            if (addNoteEventInternal3 = (ms_MuseSampler_add_track_note_event_3)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_note_event_3");
                addNoteEventInternal3 != nullptr) {
                addNoteEvent = [this](ms_MuseSampler ms, ms_Track track, int voice, long long location_us,
                                      long long duration_us, int pitch, double tempo, int offset_cents,
                                      ms_NoteArticulation articulation, long long &event_id) {
                    event_id = 0;
                    ms_NoteEvent_3 evt{voice, location_us,  duration_us, pitch,
                                       tempo, offset_cents, articulation};
                    return addNoteEventInternal3(ms, track, evt) == ms_Result_OK;
                };
            }
        } else if (at_least_v_0_3) {
            if (addNoteEventInternal2 = (ms_MuseSampler_add_track_note_event_2)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_note_event_2");
                addNoteEventInternal2 != nullptr) {
                addNoteEvent = [this](ms_MuseSampler ms, ms_Track track, int voice, long long location_us,
                                      long long duration_us, int pitch, double tempo, int offset_cents,
                                      ms_NoteArticulation articulation, long long &event_id) {
                    event_id = 0;
                    ms_NoteEvent_2 evt{voice,
                                       static_cast<long>(location_us),
                                       static_cast<long>(duration_us),
                                       pitch,
                                       tempo,
                                       offset_cents,
                                       articulation};
                    return addNoteEventInternal2(ms, track, evt) == ms_Result_OK;
                };
            }
        } else {
            if (addNoteEventInternal = (ms_MuseSampler_add_track_note_event)getLibFunc(
                    m_lib, "ms_MuseSampler_add_track_note_event");
                addNoteEventInternal != nullptr) {
                addNoteEvent = [this](ms_MuseSampler ms, ms_Track track, int voice, long long location_us,
                                      long long duration_us, int pitch, double tempo,
                                      int
                                      /*offset_cents*/,
                                      ms_NoteArticulation articulation, long long &event_id) {
                    event_id = 0;
                    ms_NoteEvent evt{voice,
                                     static_cast<long>(location_us),
                                     static_cast<long>(duration_us),
                                     pitch,
                                     tempo,
                                     articulation};
                    return addNoteEventInternal(ms, track, evt) == ms_Result_OK;
                };
            }
        }

        isRangedArticulation =
            (ms_MuseSampler_is_ranged_articulation)getLibFunc(m_lib, "ms_MuseSampler_is_ranged_articulation");
        addTrackEventRangeStart = (ms_MuseSampler_add_track_event_range_start)getLibFunc(
            m_lib, "ms_MuseSampler_add_track_event_range_start");
        addTrackEventRangeEnd = (ms_MuseSampler_add_track_event_range_end)getLibFunc(
            m_lib, "ms_MuseSampler_add_track_event_range_end");

        if (at_least_v_0_3) {
            if (startAuditionNoteInternal2 = (ms_MuseSampler_start_audition_note_2)getLibFunc(
                    m_lib, "ms_MuseSampler_start_audition_note_2");
                startAuditionNoteInternal2 != nullptr) {
                startAuditionNote = [this](ms_MuseSampler ms, ms_Track track,
                                           ms_AuditionStartNoteEvent_2 evt) {
                    return startAuditionNoteInternal2(ms, track, evt) == ms_Result_OK;
                };
            }
        } else {
            if (startAuditionNoteInternal = (ms_MuseSampler_start_audition_note)getLibFunc(
                    m_lib, "ms_MuseSampler_start_audition_note");
                startAuditionNoteInternal != nullptr) {
                startAuditionNote = [this](ms_MuseSampler ms, ms_Track track,
                                           ms_AuditionStartNoteEvent_2 evt2) {
                    ms_AuditionStartNoteEvent evt{evt2._pitch, evt2._articulation, evt2._dynamics};
                    return startAuditionNoteInternal(ms, track, evt) == ms_Result_OK;
                };
            }
        }
        stopAuditionNote =
            (ms_MuseSampler_stop_audition_note)getLibFunc(m_lib, "ms_MuseSampler_stop_audition_note");

        startLivePlayMode =
            (ms_MuseSampler_start_liveplay_mode)getLibFunc(m_lib, "ms_MuseSampler_start_liveplay_mode");
        stopLivePlayMode =
            (ms_MuseSampler_stop_liveplay_mode)getLibFunc(m_lib, "ms_MuseSampler_stop_liveplay_mode");
        if (at_least_v_0_3) {
            if (startLivePlayNoteInternal2 = (ms_MuseSampler_start_liveplay_note_2)getLibFunc(
                    m_lib, "ms_MuseSampler_start_liveplay_note_2");
                startLivePlayNoteInternal2 != nullptr) {
                startLivePlayNote = [this](ms_MuseSampler ms, ms_Track track,
                                           ms_LivePlayStartNoteEvent_2 evt) {
                    return startLivePlayNoteInternal2(ms, track, evt) == ms_Result_OK;
                };
            }
        } else {
            if (startLivePlayNoteInternal = (ms_MuseSampler_start_liveplay_note)getLibFunc(
                    m_lib, "ms_MuseSampler_start_liveplay_note");
                startLivePlayNoteInternal != nullptr) {
                startLivePlayNote = [this](ms_MuseSampler ms, ms_Track track,
                                           ms_LivePlayStartNoteEvent_2 evt2) {
                    ms_LivePlayStartNoteEvent evt{evt2._pitch, evt2._dynamics};
                    return startLivePlayNoteInternal(ms, track, evt) == ms_Result_OK;
                };
            }
        }
        stopLivePlayNote =
            (ms_MuseSampler_stop_liveplay_note)getLibFunc(m_lib, "ms_MuseSampler_stop_liveplay_note");

        startOfflineMode =
            (ms_MuseSampler_start_offline_mode)getLibFunc(m_lib, "ms_MuseSampler_start_offline_mode");
        stopOfflineMode =
            (ms_MuseSampler_stop_offline_mode)getLibFunc(m_lib, "ms_MuseSampler_stop_offline_mode");
        processOffline = (ms_MuseSampler_process_offline)getLibFunc(m_lib, "ms_MuseSampler_process_offline");

        setPosition = (ms_MuseSampler_set_position)getLibFunc(m_lib, "ms_MuseSampler_set_position");
        setPlaying = (ms_MuseSampler_set_playing)getLibFunc(m_lib, "ms_MuseSampler_set_playing");
        process = (ms_MuseSampler_process)getLibFunc(m_lib, "ms_MuseSampler_process");
        allNotesOff = (ms_MuseSampler_all_notes_off)getLibFunc(m_lib, "ms_MuseSampler_all_notes_off");

        if (initLib) {
            initLib();

            if (disableReverb) {
                disableReverb();
            }
        }
    }
};
