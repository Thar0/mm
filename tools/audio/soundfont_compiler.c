#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "xml.h"
#include "aifc.h"
#include "samplebank.h"
#include "soundfont.h"
#include "util.h"

static_assert(sizeof(float) == sizeof(uint32_t), "Float is assumed to be 32-bit");

static float
i2f(uint32_t i)
{
    union {
        float f;
        uint32_t i;
    } fi;

    fi.i = i;
    return fi.f;
}

static uint32_t
f2i(float f)
{
    union {
        float f;
        uint32_t i;
    } fi;

    fi.f = f;
    return fi.i;
}

static int
midinote_to_z64note(int note)
{
    // Converts from MIDI note number (middle C = 60) to Z64 note number (middle C = 39)
    int z64note = note - 21;
    if (z64note < 0) // % 128
        z64note += 128;
    return z64note;
}

/**
 * Calculate the tuning value from a given samplerate and basenote.
 *
 * Uses a lookup table (gPitchFrequencies from the audio driver source) to compute the result of `2^(basenote / 12)`
 * (with appropriate shifting such that the index for C4 results in 1.0)
 */
static float
calc_tuning(float sample_rate, int basenote)
{
    static const float playback_sample_rate = 32000.0f; // Target samplerate in-game is 32KHz
    static const float pitch_frequencies[] = {
        // gPitchFrequencies in audio driver source
        /* 0x00 */ 0.105112f,   // PITCH_A0
        /* 0x01 */ 0.111362f,   // PITCH_BF0
        /* 0x02 */ 0.117984f,   // PITCH_B0
        /* 0x03 */ 0.125f,      // PITCH_C1
        /* 0x04 */ 0.132433f,   // PITCH_DF1
        /* 0x05 */ 0.140308f,   // PITCH_D1
        /* 0x06 */ 0.148651f,   // PITCH_EF1
        /* 0x07 */ 0.15749f,    // PITCH_E1
        /* 0x08 */ 0.166855f,   // PITCH_F1
        /* 0x09 */ 0.176777f,   // PITCH_GF1
        /* 0x0A */ 0.187288f,   // PITCH_G1
        /* 0x0B */ 0.198425f,   // PITCH_AF1
        /* 0x0C */ 0.210224f,   // PITCH_A1
        /* 0x0D */ 0.222725f,   // PITCH_BF1
        /* 0x0E */ 0.235969f,   // PITCH_B1
        /* 0x0F */ 0.25f,       // PITCH_C2
        /* 0x10 */ 0.264866f,   // PITCH_DF2
        /* 0x11 */ 0.280616f,   // PITCH_D2
        /* 0x12 */ 0.297302f,   // PITCH_EF2
        /* 0x13 */ 0.31498f,    // PITCH_E2
        /* 0x14 */ 0.33371f,    // PITCH_F2
        /* 0x15 */ 0.353553f,   // PITCH_GF2
        /* 0x16 */ 0.374577f,   // PITCH_G2
        /* 0x17 */ 0.39685f,    // PITCH_AF2
        /* 0x18 */ 0.420448f,   // PITCH_A2
        /* 0x19 */ 0.445449f,   // PITCH_BF2
        /* 0x1A */ 0.471937f,   // PITCH_B2
        /* 0x1B */ 0.5f,        // PITCH_C3
        /* 0x1C */ 0.529732f,   // PITCH_DF3
        /* 0x1D */ 0.561231f,   // PITCH_D3
        /* 0x1E */ 0.594604f,   // PITCH_EF3
        /* 0x1F */ 0.629961f,   // PITCH_E3
        /* 0x20 */ 0.66742f,    // PITCH_F3
        /* 0x21 */ 0.707107f,   // PITCH_GF3
        /* 0x22 */ 0.749154f,   // PITCH_G3
        /* 0x23 */ 0.793701f,   // PITCH_AF3
        /* 0x24 */ 0.840897f,   // PITCH_A3
        /* 0x25 */ 0.890899f,   // PITCH_BF3
        /* 0x26 */ 0.943875f,   // PITCH_B3
        /* 0x27 */ 1.0f,        // PITCH_C4 (Middle C)
        /* 0x28 */ 1.059463f,   // PITCH_DF4
        /* 0x29 */ 1.122462f,   // PITCH_D4
        /* 0x2A */ 1.189207f,   // PITCH_EF4
        /* 0x2B */ 1.259921f,   // PITCH_E4
        /* 0x2C */ 1.33484f,    // PITCH_F4
        /* 0x2D */ 1.414214f,   // PITCH_GF4
        /* 0x2E */ 1.498307f,   // PITCH_G4
        /* 0x2F */ 1.587401f,   // PITCH_AF4
        /* 0x30 */ 1.681793f,   // PITCH_A4
        /* 0x31 */ 1.781798f,   // PITCH_BF4
        /* 0x32 */ 1.887749f,   // PITCH_B4
        /* 0x33 */ 2.0f,        // PITCH_C5
        /* 0x34 */ 2.118926f,   // PITCH_DF5
        /* 0x35 */ 2.244924f,   // PITCH_D5
        /* 0x36 */ 2.378414f,   // PITCH_EF5
        /* 0x37 */ 2.519842f,   // PITCH_E5
        /* 0x38 */ 2.66968f,    // PITCH_F5
        /* 0x39 */ 2.828428f,   // PITCH_GF5
        /* 0x3A */ 2.996615f,   // PITCH_G5
        /* 0x3B */ 3.174803f,   // PITCH_AF5
        /* 0x3C */ 3.363586f,   // PITCH_A5
        /* 0x3D */ 3.563596f,   // PITCH_BF5
        /* 0x3E */ 3.775498f,   // PITCH_B5
        /* 0x3F */ 4.0f,        // PITCH_C6
        /* 0x40 */ 4.237853f,   // PITCH_DF6
        /* 0x41 */ 4.489849f,   // PITCH_D6
        /* 0x42 */ 4.756829f,   // PITCH_EF6
        /* 0x43 */ 5.039685f,   // PITCH_E6
        /* 0x44 */ 5.33936f,    // PITCH_F6
        /* 0x45 */ 5.656855f,   // PITCH_GF6
        /* 0x46 */ 5.993229f,   // PITCH_G6
        /* 0x47 */ 6.349606f,   // PITCH_AF6
        /* 0x48 */ 6.727173f,   // PITCH_A6
        /* 0x49 */ 7.127192f,   // PITCH_BF6
        /* 0x4A */ 7.550996f,   // PITCH_B6
        /* 0x4B */ 8.0f,        // PITCH_C7
        /* 0x4C */ 8.475705f,   // PITCH_DF7
        /* 0x4D */ 8.979697f,   // PITCH_D7
        /* 0x4E */ 9.513658f,   // PITCH_EF7
        /* 0x4F */ 10.07937f,   // PITCH_E7
        /* 0x50 */ 10.6787205f, // PITCH_F7
        /* 0x51 */ 11.31371f,   // PITCH_GF7
        /* 0x52 */ 11.986459f,  // PITCH_G7
        /* 0x53 */ 12.699211f,  // PITCH_AF7
        /* 0x54 */ 13.454346f,  // PITCH_A7
        /* 0x55 */ 14.254383f,  // PITCH_BF7
        /* 0x56 */ 15.101993f,  // PITCH_B7
        /* 0x57 */ 16.0f,       // PITCH_C8
        /* 0x58 */ 16.95141f,   // PITCH_DF8
        /* 0x59 */ 17.959395f,  // PITCH_D8
        /* 0x5A */ 19.027315f,  // PITCH_EF8
        /* 0x5B */ 20.15874f,   // PITCH_E8
        /* 0x5C */ 21.35744f,   // PITCH_F8
        /* 0x5D */ 22.62742f,   // PITCH_GF8
        /* 0x5E */ 23.972918f,  // PITCH_G8
        /* 0x5F */ 25.398422f,  // PITCH_AF8
        /* 0x60 */ 26.908691f,  // PITCH_A8
        /* 0x61 */ 28.508766f,  // PITCH_BF8
        /* 0x62 */ 30.203985f,  // PITCH_B8
        /* 0x63 */ 32.0f,       // PITCH_C9
        /* 0x64 */ 33.90282f,   // PITCH_DF9
        /* 0x65 */ 35.91879f,   // PITCH_D9
        /* 0x66 */ 38.05463f,   // PITCH_EF9
        /* 0x67 */ 40.31748f,   // PITCH_E9
        /* 0x68 */ 42.71488f,   // PITCH_F9
        /* 0x69 */ 45.25484f,   // PITCH_GF9
        /* 0x6A */ 47.945835f,  // PITCH_G9
        /* 0x6B */ 50.796845f,  // PITCH_AF9
        /* 0x6C */ 53.817383f,  // PITCH_A9
        /* 0x6D */ 57.017532f,  // PITCH_BF9
        /* 0x6E */ 60.40797f,   // PITCH_B9
        /* 0x6F */ 64.0f,       // PITCH_C10
        /* 0x70 */ 67.80564f,   // PITCH_DF10
        /* 0x71 */ 71.83758f,   // PITCH_D10
        /* 0x72 */ 76.10926f,   // PITCH_EF10
        /* 0x73 */ 80.63496f,   // PITCH_E10
        /* 0x74 */ 85.42976f,   // PITCH_F10
        /* 0x75 */ 0.055681f,   // PITCH_BFNEG1
        /* 0x76 */ 0.058992f,   // PITCH_BNEG1
        /* 0x77 */ 0.0625f,     // PITCH_C0
        /* 0x78 */ 0.066216f,   // PITCH_DF0
        /* 0x79 */ 0.070154f,   // PITCH_D0
        /* 0x7A */ 0.074325f,   // PITCH_EF0
        /* 0x7B */ 0.078745f,   // PITCH_E0
        /* 0x7C */ 0.083427f,   // PITCH_F0
        /* 0x7D */ 0.088388f,   // PITCH_GF0
        /* 0x7E */ 0.093644f,   // PITCH_G0
        /* 0x7F */ 0.099213f,   // PITCH_AF0
    };

    return (sample_rate / playback_sample_rate) * pitch_frequencies[basenote];
}

void
read_envelopes_info(soundfont *sf, xmlNodePtr envelopes)
{
    static const xml_attr_spec spec_env = {
        {"Name",     false, xml_parse_c_identifier, offsetof(envelope_data, name)   },
        { "Release", false, xml_parse_u8,           offsetof(envelope_data, release)},
    };
    static const xml_attr_spec spec_env_pt = {
        {"Delay", false, xml_parse_s16, offsetof(envelope_point, delay)},
        { "Arg",  false, xml_parse_s16, offsetof(envelope_point, arg)  },
    };
    static const xml_attr_spec spec_env_goto = {
        {"Index", false, xml_parse_s16, offsetof(envelope_point, arg)},
    };

    LL_FOREACH(xmlNodePtr, env, envelopes->children) {
        if (env->type != XML_ELEMENT_NODE)
            continue;

        const char *name = XMLSTR_TO_STR(env->name);
        if (!strequ(name, "Envelope"))
            error("Unexpected element node %s in envelopes list (line %d)", name, env->line);

        envelope_data *envdata;

        if (env->children == NULL) {
            // empty envelopes for mm
            envdata = (envelope_data *)malloc(sizeof(envelope_data));
            envdata->name = NULL;
            envdata->points = NULL;
            envdata->release = 0;
            envdata->n_points = 0;
        } else {
            size_t points_cap = 4;
            size_t points_num = 0;

            void *envelopes_data = malloc(sizeof(envelope_data) + points_cap * sizeof(envelope_point));
            envdata = (envelope_data *)envelopes_data;

            xml_parse_node_by_spec(envdata, env, spec_env, ARRAY_COUNT(spec_env));

            // printf("Got Name=\"%s\" Release=(%d)\n", envdata->name, envdata->release);

            // ensure name is unique
            LL_FOREACH(envelope_data *, envdata2, sf->envelopes) {
                if (envdata2->name != NULL && strequ(envdata->name, envdata2->name))
                    error("Duplicate envelope name %s\n", envdata->name);
            }

            envelope_point *pts = (envelope_point *)(envdata + 1);

            LL_FOREACH(xmlNodePtr, env_pt, env->children) {
                if (points_num >= points_cap) {
                    points_cap *= 2;
                    envelopes_data =
                        realloc(envelopes_data, sizeof(envelope_data) + points_cap * sizeof(envelope_point));
                    envdata = (envelope_data *)envelopes_data;
                    pts = (envelope_point *)(envdata + 1);
                }

                envelope_point *pt = &pts[points_num];

                if (env_pt->type != XML_ELEMENT_NODE)
                    continue;

                const char *pt_name = XMLSTR_TO_STR(env_pt->name);

                if (strequ(pt_name, "Point")) {
                    xml_parse_node_by_spec(pt, env_pt, spec_env_pt, ARRAY_COUNT(spec_env_pt));
                } else if (strequ(pt_name, "Disable")) {
                    pt->delay = ADSR_DISABLE;
                    pt->arg = 0;
                } else if (strequ(pt_name, "Goto")) {
                    pt->delay = ADSR_GOTO;
                    xml_parse_node_by_spec(pt, env_pt, spec_env_goto, ARRAY_COUNT(spec_env_goto));
                } else if (strequ(pt_name, "Restart")) {
                    pt->delay = ADSR_RESTART;
                    pt->arg = 0;
                } else if (strequ(pt_name, "Hang")) {
                    pt->delay = ADSR_HANG;
                    pt->arg = 0;
                    // TODO force end here and don't emit an extra hang
                } else {
                    error("Unexpected element node %s in envelope definition (line %d)", name, env->line);
                }

                // printf("    POINT : Delay=(%d) Arg=(%d)\n", pt->delay, pt->arg);

                points_num++;
            }
            envdata->points = pts;
            envdata->n_points = points_num;
        }

        envdata->used = false;

        // link
        if (sf->envelopes == NULL) {
            sf->envelopes = envdata;
            sf->envelope_last = envdata;
        } else {
            sf->envelope_last->next = envdata;
            sf->envelope_last = envdata;
        }
        envdata->next = NULL;
    }
}

void
read_instrs_info(soundfont *sf, xmlNodePtr instrs)
{
    // <!-- Specifying a release value in the instrument/drum is optional, if not specified it will take the release
    // from the envelope -->
    // <!-- Specifying a base note is optional, it can be read from the sample INST chunk (but only if it exists) -->
    // <Instrument Name="STEP_GROUND" Envelope="Env0" Release="" BaseNote="" Sample="step_ground.aifc"/>
    // <Instrument Name="STEP_SAND" Envelope="" Sample=""/>

    // <!-- TODO make RangeLo/RangeHi note values instead of raw numbers (but also allow raw numbers) -->

    // <Instrument Name="FIRE_WIND" Envelope="Env0" RangeHi="62">
    //     <Sample Mid="generic_ice_crackle.aifc"/>
    //     <Sample High="amb_sandstorm.aifc"/>
    // </Instrument>

    // <Instrument Name="FIRE_WIND" Envelope="Env0" RangeLo="" RangeHi="62">
    //     <Sample Low=""/>
    //     <Sample Mid="generic_ice_crackle.aifc"/>
    //     <Sample High="amb_sandstorm.aifc"/>
    // </Instrument>

    static const xml_attr_spec instr_spec = {
        {"Name",          true,  xml_parse_c_identifier, offsetof(instr_data, name)             },
        { "MatchOrder",   true,  xml_parse_int,          offsetof(instr_data, struct_index)     },
        { "Envelope",     false, xml_parse_c_identifier, offsetof(instr_data, envelope_name)    },
        { "Release",      true,  xml_parse_u8,           offsetof(instr_data, release)          },

        { "Sample",       true,  xml_parse_c_identifier, offsetof(instr_data, sample_name_mid)  },
        { "BaseNote",     true,  xml_parse_note_number,  offsetof(instr_data, base_note_mid)    },
        { "SampleRate",   true,  xml_parse_double,       offsetof(instr_data, sample_rate_mid)  },

        { "RangeLo",      true,  xml_parse_note_number,  offsetof(instr_data, sample_low_end)   },
        { "SampleLo",     true,  xml_parse_c_identifier, offsetof(instr_data, sample_name_low)  },
        { "BaseNoteLo",   true,  xml_parse_note_number,  offsetof(instr_data, base_note_lo)     },
        { "SampleRateLo", true,  xml_parse_double,       offsetof(instr_data, sample_rate_lo)   },

        { "RangeHi",      true,  xml_parse_note_number,  offsetof(instr_data, sample_high_start)},
        { "SampleHi",     true,  xml_parse_c_identifier, offsetof(instr_data, sample_name_high) },
        { "BaseNoteHi",   true,  xml_parse_note_number,  offsetof(instr_data, base_note_hi)     },
        { "SampleRateHi", true,  xml_parse_double,       offsetof(instr_data, sample_rate_hi)   },
    };

    int last_struct_index = 0;

    LL_FOREACH(xmlNodePtr, instr_node, instrs->children) {
        if (instr_node->type != XML_ELEMENT_NODE)
            continue;

        const char *name = XMLSTR_TO_STR(instr_node->name);

        bool is_instr = strequ(name, "Instrument");
        bool is_instr_unused = strequ(name, "InstrumentUnused");

        if (!is_instr && !is_instr_unused)
            error("Unexpected element node %s in instrument list (line %d)", name, instr_node->line);

        if (!is_instr_unused)
            sf->num_instruments++;

        instr_data *instr = malloc(sizeof(instr_data));

        instr->name = NULL;
        instr->sample_name_low = NULL;
        instr->sample_name_mid = NULL;
        instr->sample_name_high = NULL;
        instr->sample_low_end = INSTR_LO_NONE;
        instr->sample_low = NULL;
        instr->sample_high_start = INSTR_HI_NONE;
        instr->sample_high = NULL;
        instr->base_note_mid = NOTE_UNSET;
        instr->base_note_lo = NOTE_UNSET;
        instr->base_note_hi = NOTE_UNSET;
        instr->sample_rate_mid = -1.0;
        instr->sample_rate_lo = -1.0;
        instr->sample_rate_hi = -1.0;
        instr->release = RELEASE_UNSET;
        instr->struct_index = last_struct_index;
        instr->unused = is_instr_unused;

        if (instr_node->properties == NULL && !is_instr_unused) {
            // printf("NULL Instrument\n");
            instr->struct_index = -1;
            instr->next_struct = NULL;
            instr->prev_struct = NULL;
            // <Instrument/>
            goto link_instr;
        }

        xml_parse_node_by_spec(instr, instr_node, instr_spec, ARRAY_COUNT(instr_spec));

        // check name
        if (!is_instr_unused && instr->name == NULL)
            error("Instrument must be named");

        last_struct_index = instr->struct_index + 1;

        // check envelope
        instr->envelope = sf_get_envelope(sf, instr->envelope_name);
        if (instr->envelope == NULL)
            error("Bad envelope name %s (line %d)\n", instr->envelope_name, instr_node->line);

        // validate optionals
        if (instr->release == RELEASE_UNSET)
            instr->release = instr->envelope->release;

        if (instr->sample_name_mid == NULL) {
            // for a used instrument to have no sample path, it must have sample children and have specified at least
            // one of RangeLo or RangeHi

            if (instr->sample_low_end == INSTR_LO_NONE && instr->sample_high_start == INSTR_HI_NONE)
                error("epic fail");

            if (instr_node->children == NULL)
                error("Sample list is empty\n");

            bool seen_low = false;
            bool seen_mid = false;
            bool seen_high = false;

            LL_FOREACH(xmlNodePtr, instr_sample_node, instr_node->children) {
                if (instr_sample_node->type != XML_ELEMENT_NODE)
                    continue;

                const char *name = XMLSTR_TO_STR(instr_sample_node->name);
                if (!strequ(name, "Sample"))
                    error("Unexpected element node %s in instrument sample list (line %d)", name,
                          instr_sample_node->line);

                if (instr_sample_node->properties == NULL)
                    error("Expected a Low/Mid/High sample path (line %d)", instr_sample_node->line);

                xmlAttrPtr attr = instr_sample_node->properties;
                if (attr->next != NULL)
                    error("Instrument sample should have exactly one attribute (line %d)", instr_sample_node->line);

                const char *attr_name = XMLSTR_TO_STR(attr->name);

                bool *seen;
                const char **name_ptr;

                if (strequ(attr_name, "Low")) {
                    seen = &seen_low;
                    name_ptr = &instr->sample_name_low;

                    if (instr->sample_low_end == INSTR_LO_NONE)
                        error("Useless Low sample specified (RangeLo is 0)");
                } else if (strequ(attr_name, "Mid")) {
                    seen = &seen_mid;
                    name_ptr = &instr->sample_name_mid;
                } else if (strequ(attr_name, "High")) {
                    seen = &seen_high;
                    name_ptr = &instr->sample_name_high;

                    if (instr->sample_high_start == INSTR_HI_NONE)
                        error("Useless High sample specified (RangeHi is 0)");
                } else
                    error("Unexpected attribute name for instrument sample (line %d)", instr_sample_node->line);

                if (*seen)
                    error("Duplicate \"%s\" sample specifier in instrument sample (line %d)", attr_name,
                          instr_sample_node->line);
                *seen = true;

                xmlChar *xvalue = xmlNodeListGetString(instr_sample_node->doc, attr->children, 1);
                const char *value = XMLSTR_TO_STR(xvalue);
                xml_parse_c_identifier(value, name_ptr);
            }

            if (!seen_mid && instr->sample_low_end != instr->sample_high_start)
                error("Unset-but-used Mid sample");
            if (!seen_low && instr->sample_low_end != 0)
                error("Unset-but-used Low sample");
            if (!seen_high && instr->sample_high_start != 0)
                error("Unset-but-used High sample");
        }

        // printf("INSTRUMENT [%s, %s, %s]\n", instr->sample_name_low, instr->sample_name_mid, instr->sample_name_high);

        // TODO select sb or sbdd based on whether IsDD is true/false for this sample

        if (instr->sample_name_low != NULL) {
            instr->sample_low = sample_data_forname(sf, instr->sample_name_low);
            if (instr->sample_low == NULL)
                error("Bad sample name for LOW sample");

            if (instr->base_note_lo == NOTE_UNSET)
                instr->base_note_lo = instr->sample_low->base_note;

            if (instr->sample_rate_lo < 0.0)
                instr->sample_rate_lo = instr->sample_low->sample_rate;

            instr->sample_low_tuning = calc_tuning(instr->sample_rate_lo, instr->base_note_lo);
            // printf("Tuning(L): (%f, %d) -> %f(%08X) \n", (float)instr->sample_rate_lo, instr->base_note_lo,
            //        instr->sample_low_tuning, f2i(instr->sample_low_tuning));
        }

        instr->sample_mid = sample_data_forname(sf, instr->sample_name_mid);
        if (instr->sample_mid == NULL)
            error("Bad sample name for MID sample");

        if (instr->base_note_mid == NOTE_UNSET)
            instr->base_note_mid = instr->sample_mid->base_note;

        if (instr->sample_rate_mid < 0.0)
            instr->sample_rate_mid = instr->sample_mid->sample_rate;

        instr->sample_mid_tuning = calc_tuning(instr->sample_rate_mid, instr->base_note_mid);
        // printf("Tuning(M): (%f, %d) -> %f(%08X) \n", (float)instr->sample_rate_mid, instr->base_note_mid,
        //        instr->sample_mid_tuning, f2i(instr->sample_mid_tuning));

        // The nastiest hack (tm)
        if (f2i(instr->sample_mid_tuning) == 0x3E7319DF /* 0.23740337789058685 */)
            instr->sample_mid_tuning = i2f(0x3E7319E3 /* 0.23740343749523163 */);

        if (instr->sample_name_high != NULL) {
            instr->sample_high = sample_data_forname(sf, instr->sample_name_high);
            if (instr->sample_high == NULL)
                error("Bad sample name for HIGH sample");

            if (instr->base_note_hi == NOTE_UNSET)
                instr->base_note_hi = instr->sample_high->base_note;

            if (instr->sample_rate_hi < 0.0)
                instr->sample_rate_hi = instr->sample_high->sample_rate;

            instr->sample_high_tuning = calc_tuning(instr->sample_rate_hi, instr->base_note_hi);
            // printf("Tuning(H): (%f, %d) -> %f(%08X) \n", (float)instr->sample_rate_hi, instr->base_note_hi,
            //        instr->sample_high_tuning, *(uint32_t*)&instr->sample_high_tuning);
        }

        // printf("LINK: %d\n", instr->struct_index);

        // link (struct), insertion sort by struct_index from highest to lowest
        if (sf->instruments_struct == NULL) {
            // printf("FIRST [%d]\n", instr->struct_index);

            // first entry, immediately insert
            sf->instruments_struct = instr;
            sf->instruments_struct_last = instr;
            instr->next_struct = NULL;
            instr->prev_struct = NULL;
        } else if (instr->struct_index >= sf->instruments_struct->struct_index) {
            instr_data *next = sf->instruments_struct;

            // printf("REPLACE FIRST [%d][%d]\n", instr->struct_index, next->struct_index);

            instr->prev_struct = NULL;
            next->prev_struct = instr;

            instr->next_struct = next;

            sf->instruments_struct = instr;
            if (next->next_struct == NULL)
                sf->instruments_struct_last = next;
        } else {
            instr_data *instr2 = sf->instruments_struct;

            // not first entry, walk the list until the next struct has a larger index
            for (instr2 = sf->instruments_struct; instr2->next_struct != NULL; instr2 = instr2->next_struct) {
                if (instr->struct_index >= instr2->next_struct->struct_index)
                    break;
            }

            instr_data *next = instr2->next_struct;
            instr_data *prev = instr2;

            prev->next_struct = instr;
            instr->prev_struct = prev;

            instr->next_struct = next;

            if (next == NULL) {
                sf->instruments_struct_last = instr;
                // printf("INSERT LAST [%d][%d]\n", prev->struct_index, instr->struct_index);
            } else {
                next->prev_struct = instr;
                // printf("INSERT BETWEEN [%d][%d][%d]\n", prev->struct_index, instr->struct_index, next->struct_index);
            }
        }

    link_instr:
        // link
        if (sf->instruments == NULL) {
            sf->instruments = instr;
            sf->instrument_last = instr;
        } else {
            sf->instrument_last->next = instr;
            sf->instrument_last = instr;
        }
        instr->next = NULL;
    }
}

void
read_drums_info(soundfont *sf, xmlNodePtr drums)
{
    // <!-- TODO how much of this can come from the aifc -->
    // <Drum Name="TAMBO" SemitoneStart="" SemitoneEnd="" Pan="64" Envelope="Env1" Sample="tambo.aifc"/>
    // <Drum Name="TAMBO_H" Semitone="" Pan="64" Envelope="Env1" Sample="tambo_h.aifc"/>

    static const xml_attr_spec drum_spec = {
        {"Name",           false, xml_parse_c_identifier, offsetof(drum_data, name)          },
        { "Semitone",      true,  xml_parse_note_number,  offsetof(drum_data, semitone)      },
        { "SemitoneStart", true,  xml_parse_note_number,  offsetof(drum_data, semitone_start)},
        { "SemitoneEnd",   true,  xml_parse_note_number,  offsetof(drum_data, semitone_end)  },
        { "Pan",           false, xml_parse_int,          offsetof(drum_data, pan)           },
        { "Envelope",      false, xml_parse_c_identifier, offsetof(drum_data, envelope_name) },
        { "Sample",        false, xml_parse_c_identifier, offsetof(drum_data, sample_name)   },
        { "SampleRate",    true,  xml_parse_double,       offsetof(drum_data, sample_rate)   },
        { "BaseNote",      true,  xml_parse_note_number,  offsetof(drum_data, base_note)     },
    };

    LL_FOREACH(xmlNodePtr, drum_node, drums->children) {
        if (drum_node->type != XML_ELEMENT_NODE)
            continue;

        const char *name = XMLSTR_TO_STR(drum_node->name);
        if (!strequ(name, "Drum"))
            error("Unexpected element node %s in drums list (line %d)", name, drum_node->line);

        drum_data *drum = malloc(sizeof(drum_data));
        drum->semitone = NOTE_UNSET;
        drum->semitone_start = NOTE_UNSET;
        drum->semitone_end = NOTE_UNSET;
        drum->sample_rate = -1;
        drum->base_note = NOTE_UNSET;

        if (drum_node->properties == NULL) {
            // <Drum/>
            drum->name = NULL;
            drum->envelope = NULL;
            drum->sample_name = NULL;
            drum->sample = NULL;
            goto link_drum;
        }

        xml_parse_node_by_spec(drum, drum_node, drum_spec, ARRAY_COUNT(drum_spec));

        drum->envelope = sf_get_envelope(sf, drum->envelope_name);
        if (drum->envelope == NULL)
            error("Bad envelope name %s (line %d)\n", drum->envelope_name, drum_node->line);

        // validate optionals
        if (drum->semitone == NOTE_UNSET) {
            if (drum->semitone_start == NOTE_UNSET || drum->semitone_end == NOTE_UNSET)
                error("Incomplete semitone range specification\n");
        } else {
            if (drum->semitone_start != NOTE_UNSET || drum->semitone_end != NOTE_UNSET)
                error("Overspecified semitone range\n");

            drum->semitone_start = drum->semitone_end = drum->semitone;
        }

        if (drum->semitone_end < drum->semitone_start)
            error("Invalid drum semitone range: %d - %d", drum->semitone_start, drum->semitone_end);

        drum->sample = sample_data_forname(sf, drum->sample_name);
        if (drum->sample == NULL)
            error("Bad sample name");

        // set final samplerate if not overridden
        if (drum->sample_rate == -1) {
            // printf("Read samplerate %s %f\n", drum->sample->name, drum->sample->sample_rate);
            drum->sample_rate = drum->sample->sample_rate;
        }

        // set basenote if not overridden
        if (drum->base_note == NOTE_UNSET) {
            if (drum->sample->aifc.has_inst) {
                // printf("Read basenote %d\n", drum->sample->base_note);

                drum->base_note = drum->sample->base_note;
            } else {
                error("No basenote for drum");
            }
        }

        // printf("DRUM [%2d:%2d] samplerate=%f basenote=%d\n", drum->semitone_start, drum->semitone_end,
        //        drum->sample_rate, drum->sample->base_note);

        // link
    link_drum:
        if (sf->drums == NULL) {
            sf->drums = drum;
            sf->drums_last = drum;
        } else {
            sf->drums_last->next = drum;
            sf->drums_last = drum;
        }
        drum->next = NULL;
    }
}

void
read_sfx_info(soundfont *sf, xmlNodePtr effects)
{
    // <Effect Name="foo" Sample="bar.aifc"/>
    // <Effect/> (don't emit a struct and place a NULL in the array at this location)
    // sample may be "NONE" in which case tuning is 0

    static const xml_attr_spec effect_spec = {
        {"Name",        false, xml_parse_c_identifier, offsetof(sfx_data, name)       },
        { "Sample",     false, xml_parse_c_identifier, offsetof(sfx_data, sample_name)},
        { "SampleRate", true,  xml_parse_double,       offsetof(sfx_data, sample_rate)},
        { "BaseNote",   true,  xml_parse_note_number,  offsetof(sfx_data, base_note)  },
    };

    LL_FOREACH(xmlNodePtr, eff, effects->children) {
        if (eff->type != XML_ELEMENT_NODE)
            continue;

        const char *name = XMLSTR_TO_STR(eff->name);
        if (!strequ(name, "Effect"))
            error("Unexpected element node %s in effects list (line %d)", name, eff->line);

        sf->num_effects++;

        sfx_data *sfx = malloc(sizeof(sfx_data));

        if (eff->properties == NULL) {
            // printf("GOT SFX (NULL)\n");
            sfx->sample = NULL;
        } else {
            sfx->sample_rate = -1;
            sfx->base_note = NOTE_UNSET;
            xml_parse_node_by_spec(sfx, eff, effect_spec, ARRAY_COUNT(effect_spec));
            // printf("GOT SFX Name=\"%s\" Sample=\"%s\"\n", sfx->name, sfx->sample_name);

            sfx->sample = sample_data_forname(sf, sfx->sample_name);

            if (sfx->base_note == NOTE_UNSET)
                sfx->base_note = sfx->sample->base_note;

            if (sfx->sample_rate == -1)
                sfx->sample_rate = sfx->sample->sample_rate;

            sfx->tuning = calc_tuning(sfx->sample_rate, sfx->base_note);
        }

        // link
        if (sf->sfx == NULL) {
            sf->sfx = sfx;
            sf->sfx_last = sfx;
        } else {
            sf->sfx_last->next = sfx;
            sf->sfx_last = sfx;
        }
        sfx->next = NULL;
    }
}

typedef struct {
    bool is_dd;
    bool cached;
} sample_data_defaults;

void
read_samples_info(soundfont *sf, xmlNodePtr samples)
{
    //  <Samples IsDD="false" Cached="false">
    //      <Sample Name="STEP_GROUND" SampleRate="32000" BaseNote="C4" IsDD="false" Cached="false">
    //  </Samples>

    static const xml_attr_spec samples_spec = {
        {"IsDD",    true, xml_parse_bool, offsetof(sample_data_defaults, is_dd) },
        { "Cached", true, xml_parse_bool, offsetof(sample_data_defaults, cached)},
    };
    static const xml_attr_spec sample_spec = {
        {"Name",        false, xml_parse_c_identifier, offsetof(sample_data, name)       },
        { "SampleRate", true,  xml_parse_double,       offsetof(sample_data, sample_rate)},
        { "BaseNote",   true,  xml_parse_int,          offsetof(sample_data, base_note)  },
        { "IsDD",       true,  xml_parse_bool,         offsetof(sample_data, is_dd)      },
        { "Cached",     true,  xml_parse_bool,         offsetof(sample_data, cached)     },
    };

    sample_data_defaults defaults;
    defaults.is_dd = false;
    defaults.cached = false;
    xml_parse_node_by_spec(&defaults, samples, samples_spec, ARRAY_COUNT(samples_spec));

    LL_FOREACH(xmlNodePtr, sample_node, samples->children) {
        if (sample_node->type != XML_ELEMENT_NODE)
            continue;

        const char *name = XMLSTR_TO_STR(sample_node->name);
        if (!strequ(name, "Sample"))
            error("Unexpected element node %s in samples list (line %d)", name, sample_node->line);

        sample_data *sample = malloc(sizeof(sample_data));

        sample->sample_rate = -1.0;
        sample->base_note = NOTE_UNSET;
        sample->is_dd = defaults.is_dd;
        sample->cached = defaults.cached;

        xml_parse_node_by_spec(sample, sample_node, sample_spec, ARRAY_COUNT(sample_spec));

        // printf("GOT SAMPLE Name=\"%s\" SampleRate=\"%f\" BaseNote=\"%d\" IsDD=\"%s\" Cached=\"%s\"\n",
        //        sample->name, sample->sample_rate, sample->base_note, BOOL_STR(sample->is_dd),
        //        BOOL_STR(sample->cached));

        const char *sample_path = samplebank_path_forname(&sf->sb, sample->name);
        if (sample_path == NULL)
            error("Bad sample name"); // TODO

        // printf("    Path = \"%s\"\n", sample_path);
        aifc_read(&sample->aifc, sample_path, NULL, NULL);

        if (sample->sample_rate == -1.0)
            sample->sample_rate = sample->aifc.sample_rate;

        if (sample->base_note == NOTE_UNSET) {
            if (sample->aifc.has_inst)
                sample->base_note = midinote_to_z64note(sample->aifc.basenote);
            else
                error("No basenote"); // TODO
        }

        if (!sample->aifc.has_book)
            error("No book"); // TODO

        // printf("    SampleRate = %f\n"
        //        "    BaseNote   = %d\n"
        //        "    SSNDSize   = 0x%lX\n",
        //        sample->sample_rate,
        //        sample->base_note,
        //        sample->aifc.ssnd_size);

        // link
        if (sf->samples == NULL) {
            sf->samples = sample;
            sf->sample_last = sample;
        } else {
            sf->sample_last->next = sample;
            sf->sample_last = sample;
        }
        sample->next = NULL;
    }
}

static bool
is_hex(char c)
{
    return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F');
}

static int
from_hex(char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    assert(false);
    return -0xABABABAB;
}

void
read_match_padding(soundfont *sf, xmlNodePtr padding_decl)
{
    if (padding_decl->properties != NULL)
        error("Unexpected attributes line %d\n", padding_decl->line);

    if (padding_decl->children == NULL || padding_decl->children->content == NULL)
        error("No data\n");

    if (padding_decl->children->next != NULL)
        error("Malformed padding data\n");

    const char *data_str = XMLSTR_TO_STR(padding_decl->children->content);
    size_t data_len = strlen(data_str);

    // We expect padding to be bytes like 0xAB separated by comma or whitespace, so string length / 5 is the upper bound
    uint8_t *padding = malloc(data_len / 5);

    size_t k = 0;
    bool must_be_delimiter = false;

    for (size_t i = 0; i < data_len - 4; i++) {
        if (isspace(data_str[i]) || data_str[i] == ',') {
            must_be_delimiter = false;
            continue;
        }

        if (must_be_delimiter)
            error("Malformed padding data (1, %ld)\n", i);

        if (data_str[i + 0] != '0' || data_str[i + 1] != 'x')
            error("Malformed padding data (2, %ld)\n", i);

        char c1 = toupper(data_str[i + 2]);
        char c2 = toupper(data_str[i + 3]);

        if (!is_hex(c1) || !is_hex(c2))
            error("Malformed padding data (3, %ld)\n", i);

        padding[k++] = (from_hex(c1) << 4) | from_hex(c2);
        must_be_delimiter = true;
        i += 3;
    }

    sf->match_padding = padding;
    sf->match_padding_num = k;

    // for (size_t i = 0; i < k; i++)
    //     printf("0x%02X ", padding[i]);
    // printf("\n");
}

/**
 * Emit a padding statement that pads to the next 0x10 byte boundary. Assumes that `pos` measures from an 0x10-byte
 * aligned location.
 */
static void
emit_padding_stmt(FILE *out, unsigned pos)
{
    switch (ALIGN16(pos) - pos) {
        case 0:
            // Already aligned, pass silently
            break;
        case 4:
            fprintf(out, "SF_PAD4();\n");
            break;
        case 8:
            fprintf(out, "SF_PAD8();\n");
            break;
        case 0xC:
            fprintf(out, "SF_PADC();\n");
            break;
        default:
            // We don't expect to need to support alignment from anything less than word-aligned.
            error("[Internal] Bad alignment generated");
            break;
    }
}

size_t
emit_c_header(FILE *out, soundfont *sf)
{
    size_t size = 0;

    fprintf(out, "// HEADER\n\n");

    // Generate externs for use in the header.

    if (sf->drums != NULL)
        fprintf(out, "extern Drum* SF%d_DRUMS_PTR_LIST[];\n\n", sf->info.index);

    if (sf->sfx != NULL)
        fprintf(out, "extern SoundEffect SF%d_SFX_LIST[];\n\n", sf->info.index);

    if (sf->instruments != NULL) {
        LL_FOREACH(instr_data *, instr, sf->instruments) {
            if (instr->name == NULL)
                continue;
            fprintf(out, "extern Instrument %s;\n", instr->name);
        }
        fprintf(out, "\n");
    }

    // Generate the header itself: drums -> sfx -> instruments.

    // We always need to write pointers for drums and sfx even if they are NULL.

    uint32_t pos = 0;

    if (sf->drums != NULL)
        fprintf(out, "NO_REORDER DATA Drum** SF%d_DRUMS_PTR_LIST_PTR = SF%d_DRUMS_PTR_LIST;\n", sf->info.index,
                sf->info.index);
    else
        fprintf(out, "NO_REORDER DATA Drum** SF%d_DRUMS_PTR_LIST_PTR = NULL;\n", sf->info.index);

    pos += 4;
    size += 4;

    if (sf->sfx != NULL)
        fprintf(out, "NO_REORDER DATA SoundEffect* SF%d_SFX_LIST_PTR = SF%d_SFX_LIST;\n", sf->info.index,
                sf->info.index);
    else
        fprintf(out, "NO_REORDER DATA SoundEffect* SF%d_SFX_LIST_PTR = NULL;\n", sf->info.index);

    pos += 4;
    size += 4;

    if (sf->instruments != NULL) {
        fprintf(out, "NO_REORDER DATA Instrument* SF%d_INSTRUMENT_PTR_LIST[] = {\n", sf->info.index);

        LL_FOREACH(instr_data *, instr, sf->instruments) {
            if (instr->unused)
                continue; // Don't increment list size as nothing was written

            if (instr->name == NULL)
                fprintf(out, "    NULL,\n");
            else
                fprintf(out, "    &%s,\n", instr->name);
            pos += 4;
            size += 4;
        }

        fprintf(out, "};\n");
    }

    // Pad the header to the next 0x10-byte boundary.
    emit_padding_stmt(out, pos);
    fprintf(out, "\n");

    return ALIGN16(size);
}

/**
 * Convert the compression type as indicated in the AIFC to the correspoding SampleCodec enum value.
 * These must be kept in sync with the SampleCodec definition!
 */
static const char *
codec_enum(uint32_t compression_type, const char *origin_file)
{
    switch (compression_type) {
        case CC4('A', 'D', 'P', '9'):
            return "CODEC_ADPCM";

        case CC4('H', 'P', 'C', 'M'):
            return "CODEC_S8";

        case CC4('A', 'D', 'P', '5'):
            return "CODEC_SMALL_ADPCM";

        case CC4('R', 'V', 'R', 'B'):
            return "CODEC_REVERB";

        case CC4('N', 'O', 'N', 'E'):
            return "CODEC_S16";
    }
    error("Bad compression type in aifc file %s", origin_file);
    __builtin_unreachable();
}

static unsigned int
codec_frame_size(uint32_t compression_type)
{
    switch (compression_type) {
        case CC4('A', 'D', 'P', '9'):
            return 9;

        case CC4('A', 'D', 'P', '5'):
            return 5;

        default: // TODO should any others not use 16?
            return 16;
    }
}

/**
 * Compare the codebooks of two samples. Returns true if they are identical.
 */
static bool
samples_books_equal(sample_data *s1, sample_data *s2)
{
    int32_t s1_order = s1->aifc.book.order;
    int32_t s1_npredictors = s1->aifc.book.npredictors;
    int32_t s2_order = s1->aifc.book.order;
    int32_t s2_npredictors = s1->aifc.book.npredictors;

    if (s1_order != s2_order || s1_npredictors != s2_npredictors)
        return false;
    return !memcmp(*s1->aifc.book_state, *s2->aifc.book_state, 8 * (unsigned)s1_order * (unsigned)s1_npredictors);
}

/**
 * Writes all samples, their codebooks and their loops to C structures.
 */
size_t
emit_c_samples(FILE *out, soundfont *sf)
{
    size_t size = 0;

    if (sf->samples == NULL)
        return size;

    int i = 0;
    LL_FOREACH(sample_data *, sample, sf->samples) {
        // Determine if we need to write a new book structure. If we've already emitted a book structure with the
        // same contents we use that instead.

        bool new_book = true;
        const char *bookname = sample->name;

        LL_FOREACH(sample_data *, sample2, sf->samples) {
            if (sample2 == sample)
                // Caught up to our current position, we need to write a new book.
                break;

            if (samples_books_equal(sample, sample2)) {
                // A book that we've already seen is the same as this one. Since the book we are comparing to here is
                // the first such book, this is guaranteed to have already been written and we move the reference to
                // this one.
                new_book = false;
                bookname = sample2->name;
                break;
            }
        }

        fprintf(out, "// SAMPLE %d\n\n", i);

        // Write the sample header

        fprintf(out,
                // clang-format off
               "extern u8 %s_%s_Off[];"         "\n"
               "extern AdpcmBook SF%d_%s_BOOK;" "\n"    // TODO we could skip writing this extern if new_book is false
               "extern AdpcmLoop SF%d_%s_LOOP;" "\n"
                                                "\n",
                // clang-format on
                sf->sb.name, sample->name, sf->info.index, bookname, sf->info.index, sample->name);

        const char *codec_name = codec_enum(sample->aifc.compression_type, sample->aifc.path);

        fprintf(out,
                // clang-format off
               "NO_REORDER DATA Sample SF%d_%s_HEADER = {"  "\n"
               "    %d, %s, %d, %s, %s,"                    "\n"
               "    0x%06lX,"                               "\n"
               "    %s_%s_Off,"                             "\n"
               "    &SF%d_%s_LOOP,"                         "\n"
               "    &SF%d_%s_BOOK,"                         "\n"
               "};"                                         "\n"
                                                            "\n",
                // clang-format on
                sf->info.index, sample->name, 0, codec_name, sample->is_dd, BOOL_STR(sample->cached), BOOL_STR(false),
                sample->aifc.ssnd_size, sf->sb.name, sample->name, sf->info.index, sample->name, sf->info.index,
                bookname);
        size += 0x10;

        // Write the book if it hasn't been deduplicated.

        if (new_book) {
            // Since books are variable-size structures and we want to support a C89 compiler, we first write the
            // header as one structure and the book state as an array. We then declare a weak symbol for the book
            // header to alias it to the correct type without casts, avoiding potential type conflicts with externs.
            size_t book_size = 0;

            fprintf(out,
                    // clang-format off
                   "NO_REORDER DATA ALIGNED(16) AdpcmBookHeader SF%d_%s_BOOK_HEADER = {"    "\n"
                   "    %d, %d,"                                                            "\n"
                   "};"                                                                     "\n"
                   "NO_REORDER DATA AdpcmBookData SF%d_%s_BOOK_DATA = {"                    "\n",
                    // clang-format on
                    sf->info.index, bookname, sample->aifc.book.order, sample->aifc.book.npredictors, sf->info.index,
                    bookname);
            book_size += 8;

            for (size_t j = 0; j < (unsigned)sample->aifc.book.order * (unsigned)sample->aifc.book.npredictors; j++) {
                fprintf(
                    out,
                    // clang-format off
                       "    (s16)0x%04X, (s16)0x%04X, (s16)0x%04X, (s16)0x%04X, "
                           "(s16)0x%04X, (s16)0x%04X, (s16)0x%04X, (s16)0x%04X,\n",
                    // clang-format on
                    (uint16_t)(*sample->aifc.book_state)[j * 8 + 0], (uint16_t)(*sample->aifc.book_state)[j * 8 + 1],
                    (uint16_t)(*sample->aifc.book_state)[j * 8 + 2], (uint16_t)(*sample->aifc.book_state)[j * 8 + 3],
                    (uint16_t)(*sample->aifc.book_state)[j * 8 + 4], (uint16_t)(*sample->aifc.book_state)[j * 8 + 5],
                    (uint16_t)(*sample->aifc.book_state)[j * 8 + 6], (uint16_t)(*sample->aifc.book_state)[j * 8 + 7]);
            }

            fprintf(out,
                    // clang-format off
                   "};"                                                 "\n"
                   "#pragma weak SF%d_%s_BOOK = SF%d_%s_BOOK_HEADER"    "\n",
                    // clang-format on
                    sf->info.index, bookname, sf->info.index, bookname);

            // We assume here that book structures begin on 0x10-byte boundaries. Book structures are always
            // `4 + 4 + 8 * order * npredictors` large, emit a padding statement to the next 0x10-byte boundary.
            book_size += 2 * 8 * (unsigned)sample->aifc.book.order * (unsigned)sample->aifc.book.npredictors;
            emit_padding_stmt(out, book_size);
            fprintf(out, "\n");

            size += ALIGN16(book_size);
        }

        // Write the loop

        // Can't use sample->aifc.num_frames directly, the original vadpcm_enc tool occasionally got the number
        // of frames wrong (off-by-1) which we must reproduce here for matching (rather than reproducing it in the
        // aifc and wav/aiff files themselves)
        uint32_t frame_count = (sample->aifc.ssnd_size * 16) / codec_frame_size(sample->aifc.compression_type);

        // We cannot deduplicate or skip writing loops in general as the audio driver assumes that at least a loop
        // header exists for every sample. We could deduplicate on the special case that two samples have the same
        // frame count? TODO

        if (!sample->aifc.has_loop || sample->aifc.loop.count == 0) {
            // No loop present, or a loop with a count of 0 was explicitly written into the aifc.
            // Write a header only, using the same weak symbol trick as with books.

            uint32_t start;
            uint32_t end;
            uint32_t count;

            if (!sample->aifc.has_loop) {
                // No loop, write a loop header that spans the entire sample with a count of 0.
                // The audio driver expects that a loop structure always exists for a sample.
                start = 0;
                end = frame_count;
                count = 0;
            } else {
                // There is a count=0 loop in the aifc file, trust it.
                start = sample->aifc.loop.start;
                end = sample->aifc.loop.end;
                count = sample->aifc.loop.count;
            }

            fprintf(out,
                    // clang-format off
                   "NO_REORDER DATA ALIGNED(16) AdpcmLoopHeader SF%d_%s_LOOP_HEADER = {"    "\n"
                   "    %u, %u, %u, 0,"                                                     "\n"
                   "};"                                                                     "\n"
                   "#pragma weak SF%d_%s_LOOP = SF%d_%s_LOOP_HEADER"                        "\n"
                                                                                            "\n",
                    // clang-format on
                    sf->info.index, sample->name, start, end, count, sf->info.index, sample->name, sf->info.index,
                    sample->name);
            size += 0x10;
        } else {
            // With state, since loop states are a fixed size there is no need for a weak alias.

            // Some soundfonts include the total frame count of the sample, but not all of them.
            // Set the frame count to 0 here to inhibit writing it into the loop structure if this is
            // a soundfont that does not include it.
            if (!sf->info.loops_have_frames)
                frame_count = 0;

            char count_str[12];

            if (sample->aifc.loop.count == 0xFFFFFFFF)
                snprintf(count_str, sizeof(count_str), "0x%08X", sample->aifc.loop.count);
            else
                snprintf(count_str, sizeof(count_str), "%u", sample->aifc.loop.count);

            fprintf(out,
                    // clang-format off
                   "NO_REORDER DATA ALIGNED(16) AdpcmLoop SF%d_%s_LOOP = {"         "\n"
                   "    { %u, %u, %s, %u },"                                        "\n"
                   "    {"                                                          "\n"
                   "        (s16)0x%04X, (s16)0x%04X, (s16)0x%04X, (s16)0x%04X,"    "\n"
                   "        (s16)0x%04X, (s16)0x%04X, (s16)0x%04X, (s16)0x%04X,"    "\n"
                   "        (s16)0x%04X, (s16)0x%04X, (s16)0x%04X, (s16)0x%04X,"    "\n"
                   "        (s16)0x%04X, (s16)0x%04X, (s16)0x%04X, (s16)0x%04X,"    "\n"
                   "    },"                                                         "\n"
                   "};"                                                             "\n"
                                                                                    "\n",
                    // clang-format on
                    sf->info.index, sample->name, sample->aifc.loop.start, sample->aifc.loop.end, count_str,
                    frame_count, (uint16_t)sample->aifc.loop.state[0], (uint16_t)sample->aifc.loop.state[1],
                    (uint16_t)sample->aifc.loop.state[2], (uint16_t)sample->aifc.loop.state[3],
                    (uint16_t)sample->aifc.loop.state[4], (uint16_t)sample->aifc.loop.state[5],
                    (uint16_t)sample->aifc.loop.state[6], (uint16_t)sample->aifc.loop.state[7],
                    (uint16_t)sample->aifc.loop.state[8], (uint16_t)sample->aifc.loop.state[9],
                    (uint16_t)sample->aifc.loop.state[10], (uint16_t)sample->aifc.loop.state[11],
                    (uint16_t)sample->aifc.loop.state[12], (uint16_t)sample->aifc.loop.state[13],
                    (uint16_t)sample->aifc.loop.state[14], (uint16_t)sample->aifc.loop.state[15]);
            size += 0x30;
        }
        i++;
    }
    return size;
}

/**
 * Write envelope structures.
 */
size_t
emit_c_envelopes(FILE *out, soundfont *sf)
{
    size_t size = 0;

    if (sf->envelopes == NULL)
        return size;

    fprintf(out, "// ENVELOPES\n\n");

    size_t empty_num = 0;

    LL_FOREACH(envelope_data *, envdata, sf->envelopes) {
        if (envdata->name == NULL) {
            // For MM: write 16 bytes of 0
            // TODO ignore when nonmatching

            fprintf(out,
                    // clang-format off
                   "NO_REORDER DATA EnvelopePoint SF%d_ENV_EMPTY_%lu[] = {"     "\n"
                   "    { 0, 0, },"                                             "\n"
                   "    { 0, 0, },"                                             "\n"
                   "    { 0, 0, },"                                             "\n"
                   "    { 0, 0, },"                                             "\n"
                   "};"                                                         "\n"
                                                                                "\n",
                    // clang-format on
                    sf->info.index, empty_num);

            empty_num++;
            size += 0x10;
        } else {
            fprintf(out, "NO_REORDER DATA EnvelopePoint SF%d_%s[] = {\n", sf->info.index, envdata->name);

            // Write all points
            for (size_t j = 0; j < envdata->n_points; j++) {
                envelope_point *pt = &envdata->points[j];

                switch (pt->delay) {
                    case ADSR_DISABLE:
                        fprintf(out, "    ENVELOPE_DISABLE(),\n");
                        break;
                    case ADSR_GOTO:
                        fprintf(out, "    ENVELOPE_GOTO(%d),\n", pt->arg);
                        break;
                    case ADSR_HANG:
                        fprintf(out, "    ENVELOPE_HANG(),\n");
                        break;
                    case ADSR_RESTART:
                        fprintf(out, "    ENVELOPE_RESTART(),\n");
                        break;
                    default:
                        fprintf(out, "    ENVELOPE_POINT(%5d, %5d),\n", pt->delay, pt->arg);
                        break;
                }
            }

            // Automatically add a HANG command at the end
            fprintf(out, "    ENVELOPE_HANG(),\n"
                         "};\n");

            // Pad to 0x10-byte boundary
            size_t env_size = 4 * (envdata->n_points + 1);
            emit_padding_stmt(out, env_size);
            fprintf(out, "\n");

            size += ALIGN16(env_size);
        }
    }
    return size;
}

#define F32_FMT "%.22f"

size_t
emit_c_instruments(FILE *out, soundfont *sf)
{
    size_t size = 0;

    if (sf->instruments_struct_last == NULL)
        return size;

    fprintf(out, "// INSTRUMENTS\n\n");

    size_t unused_instr_num = 0;

    // For matching reasons we need to emit instrument structures in a possibly different order to the order we emit
    // the instrument pointers in the header. When reading the instrument information before, we created a linked list
    // sorted by highest "struct index". We now walk backwards through this list to emit structures in order of lowest
    // to highest "struct index".

    for (instr_data *instr = sf->instruments_struct_last; instr != NULL; instr = instr->prev_struct) {
        if (instr->name == NULL && !instr->unused)
            // This corresponds to <Instrument/> entries, these have no associated data and only correspond to a NULL in
            // the instrument pointer list. Ignores these.
            continue;

        if (instr->unused) {
            fprintf(out, "NO_REORDER DATA Instrument _INSTR_UNUSED_%lu = {\n", unused_instr_num);
            unused_instr_num++;
        } else {
            fprintf(out, "NO_REORDER DATA Instrument %s = {\n", instr->name);
        }

        char nlo[5];
        snprintf(nlo, sizeof(nlo), "%3d", instr->sample_low_end);
        char nhi[5];
        snprintf(nhi, sizeof(nhi), "%3d", instr->sample_high_start);

        fprintf(out,
                // clang-format off
               "    false,"         "\n"
               "    %s,"            "\n"
               "    %s,"            "\n"
               "    %d,"            "\n"
               "    SF%d_%s,"       "\n",
                // clang-format on
                (instr->sample_low_end == INSTR_LO_NONE) ? "INSTR_SAMPLE_LO_NONE" : nlo,
                (instr->sample_high_start == INSTR_HI_NONE) ? "INSTR_SAMPLE_HI_NONE" : nhi, instr->release,
                sf->info.index, instr->envelope_name);

        if (instr->sample_low != NULL)
            fprintf(out, "    { &SF%d_%s_HEADER, " F32_FMT "f },\n", sf->info.index, instr->sample_name_low,
                    instr->sample_low_tuning);
        else
            fprintf(out, "    INSTR_SAMPLE_NONE,\n");

        fprintf(out, "    { &SF%d_%s_HEADER, " F32_FMT "f },\n", sf->info.index, instr->sample_name_mid,
                instr->sample_mid_tuning);

        if (instr->sample_high != NULL)
            fprintf(out, "    { &SF%d_%s_HEADER, " F32_FMT "f },\n", sf->info.index, instr->sample_name_high,
                    instr->sample_high_tuning);
        else
            fprintf(out, "    INSTR_SAMPLE_NONE,\n");

        fprintf(out, "};\n\n");

        size += 0x20;
    }
    return size;
}

size_t
emit_c_drums(FILE *out, soundfont *sf)
{
    size_t size = 0;

    if (sf->drums == NULL)
        return size;

    fprintf(out, "// DRUMS\n\n");

    // Prepare pointer table data to be filled in while writing the drum structures. Init to 0 so if any low semitones
    // are not covered by any drum group the name will be NULL.
    struct {
        const char *name;
        int n;
    } ptr_table[64];
    memset(ptr_table, 0, sizeof(ptr_table));

    // While writing the drum structures we record the maximum semitone covered by this soundfont. Some "oddball"
    // soundfonts like soundfont 0 do not have an array entry for all 64 semitones. We use this to know when to stop
    // writing entries in the pointer table.
    int max_semitone = -1;

    LL_FOREACH(drum_data *, drum, sf->drums) {
        if (drum->name == NULL) {
            max_semitone++;
            continue;
        }

        if (drum->semitone_end > max_semitone)
            max_semitone = drum->semitone_end;

        size_t length = drum->semitone_end - drum->semitone_start + 1;

        // Drum structures are duplicated for each semitone in the range they cover, the basenote for each is
        // incremented by one but the data is otherwise identical. We write a preprocessor definition to make the
        // resulting source more compact for easier inspection.

        fprintf(out,
                // clang-format off
               "#define %s_ENTRY(tuning) \\"                "\n"
               "    { \\"                                   "\n"
               "        %d, \\"                             "\n"
               "        %d, \\"                             "\n"
               "        false, \\"                          "\n"
               "        { &SF%d_%s_HEADER, (tuning) }, \\"  "\n"
               "        SF%d_%s, \\"                        "\n"
               "    }"                                      "\n"
               "NO_REORDER DATA Drum %s[%lu] = {"           "\n",
                // clang-format on
                drum->name,
                drum->envelope->release, // TODO expose override
                drum->pan, sf->info.index, drum->sample->name, sf->info.index, drum->envelope->name, drum->name,
                length);

        // Write each structure while building the drum pointer table

        if (drum->semitone_end + 1 > 64)
            error("Bad drum range");

        for (size_t note_offset = 0; note_offset < length; note_offset++) {
            size_t ptr_offset = drum->semitone_start + note_offset;

            ptr_table[ptr_offset].name = drum->name;
            ptr_table[ptr_offset].n = note_offset;

            // wrap note on overflow
            int note = drum->base_note + note_offset;
            if (note > 127)
                note -= 128;

            float tuning = calc_tuning(drum->sample_rate, note);

            // printf("Tuning(Drum Index %lu): (%f, %d) -> %f(%08X) \n",
            //        note_offset,
            //        drum->sample_rate,
            //        note,
            //        tuning,
            //        f2i(tuning));

            fprintf(out, "    %s_ENTRY(" F32_FMT "f),\n", drum->name, tuning);
        }

        fprintf(out, "};\n\n");
        size += 0x10 * length;
    }

    // Write the drum pointer table. Always start at 0 and end at the maximum used semitone. If any low semitones are
    // not used, NULL is written into the array.

    size_t table_len = max_semitone + 1;
    if (table_len > 64)
        error("Bad drum pointer table length");

    fprintf(out, "NO_REORDER DATA Drum* SF%d_DRUMS_PTR_LIST[%lu] = {\n", sf->info.index, table_len);

    for (size_t i = 0; i < table_len; i++) {
        if (ptr_table[i].name == NULL) {
            fprintf(out, "    NULL,\n");
            continue;
        }

        if (i != 0 && ptr_table[i].n == 0) // Add some space between different drum groups
            fprintf(out, "\n");
        fprintf(out, "    &%s[%d],\n", ptr_table[i].name, ptr_table[i].n);
    }

    sf->num_drums = table_len;

    fprintf(out, "};\n");
    emit_padding_stmt(out, table_len * 4);
    fprintf(out, "\n");

    size += ALIGN16(table_len * 4);
    return size;
}

size_t
emit_c_effects(FILE *out, soundfont *sf)
{
    size_t size = 0;

    if (sf->sfx == NULL)
        return size;

    fprintf(out, "// EFFECTS\n\n");

    // Effects are all contained in the same array. We write empty <Effect/> entries as NULL entries in this array.

    fprintf(out, "NO_REORDER DATA SoundEffect SF%d_SFX_LIST[] = {\n", sf->info.index);

    LL_FOREACH(sfx_data *, sfx, sf->sfx) {
        if (sfx->sample != NULL)
            fprintf(out, "    { { &SF%d_%s_HEADER, " F32_FMT "f } },\n", sf->info.index, sfx->sample->name,
                    sfx->tuning);
        else
            fprintf(out, "    { { NULL, 0.0f } },\n");

        size += 8;
    }

    fprintf(out, "};\n\n");

    return size;
}

void
emit_c_match_padding(FILE *out, soundfont *sf, size_t size)
{
    if (sf->match_padding != NULL && sf->match_padding_num != 0) {
        // Sometimes a soundfont will have non-zero padding at the end, add these values manually
        size_t expected = sf->match_padding_num;

        // Don't pad any further than the next 0x10 byte boundary
        size_t remaining = ALIGN16(size) - size;
        size_t amount = (expected > remaining) ? remaining : expected;

        fprintf(out, "// MATCH PADDING\n\n");

        fprintf(out, "NO_REORDER DATA u8 SF%d_MATCH_PADDING[] = {\n", sf->info.index);
        for (size_t i = 0; i < amount; i++)
            fprintf(out, "    0x%02X,\n", sf->match_padding[i]);
        fprintf(out, "};\n\n");

        size += amount;
    }

    if (sf->info.pad_to_size != 0) {
        if (sf->info.pad_to_size <= size) {
            warning("PadToSize directive ignored.");
        } else {
            fprintf(out, "// MATCH SIZE PADDING\n\n");

            // pad to given size
            size_t amount = sf->info.pad_to_size - size;
            fprintf(out, "NO_REORDER DATA u8 SF%d_MATCH_PADDING_TO_SIZE[%lu] = { 0 };\n", sf->info.index, amount);
        }
    }
}

void
emit_h_instruments(FILE *out, soundfont *sf)
{
    if (sf->instruments == NULL)
        return;

    // #define FONT{Index}_INSTR_{EnumName} {EnumValue}

    int i = 0;
    LL_FOREACH(instr_data *, instr, sf->instruments) {
        if (instr->name != NULL) {
            fprintf(out, "#define %s %d\n", instr->name, i);
        } else {
            // fprintf(out, "\n");
        }
        i++;
    }
    fprintf(out, "\n");
}

static const char *
z64_note_name(int note_num)
{
    static const char *const note_names[] = {
        "A0",  "BF0", "B0",  "C1",  "DF1", "D1",  "EF1", "E1",   "F1",  "GF1",  "G1",  "AF1", "A1",     "BF1",   "B1",
        "C2",  "DF2", "D2",  "EF2", "E2",  "F2",  "GF2", "G2",   "AF2", "A2",   "BF2", "B2",  "C3",     "DF3",   "D3",
        "EF3", "E3",  "F3",  "GF3", "G3",  "AF3", "A3",  "BF3",  "B3",  "C4",   "DF4", "D4",  "EF4",    "E4",    "F4",
        "GF4", "G4",  "AF4", "A4",  "BF4", "B4",  "C5",  "DF5",  "D5",  "EF5",  "E5",  "F5",  "GF5",    "G5",    "AF5",
        "A5",  "BF5", "B5",  "C6",  "DF6", "D6",  "EF6", "E6",   "F6",  "GF6",  "G6",  "AF6", "A6",     "BF6",   "B6",
        "C7",  "DF7", "D7",  "EF7", "E7",  "F7",  "GF7", "G7",   "AF7", "A7",   "BF7", "B7",  "C8",     "DF8",   "D8",
        "EF8", "E8",  "F8",  "GF8", "G8",  "AF8", "A8",  "BF8",  "B8",  "C9",   "DF9", "D9",  "EF9",    "E9",    "F9",
        "GF9", "G9",  "AF9", "A9",  "BF9", "B9",  "C10", "DF10", "D10", "EF10", "E10", "F10", "BFNEG1", "BNEG1", "C0",
        "DF0", "D0",  "EF0", "E0",  "F0",  "GF0", "G0",  "AF0",
    };
    return note_names[note_num];
}

void
emit_h_drums(FILE *out, soundfont *sf)
{
    if (sf->drums == NULL)
        return;

    // Emit drum defines in groups, named like [DrumName]_[NoteName]
    // e.g. a drum called "MY_DRUM" with a sample basenote of C4 covering a semitone range of 0..3 looks like
    // #define MY_DRUM_C4  0
    // #define MY_DRUM_DF4 1
    // #define MY_DRUM_D4  2
    // #define MY_DRUM_EF4 3

    LL_FOREACH(drum_data *, drum, sf->drums) {
        if (drum->name == NULL)
            continue;

        int length = drum->semitone_end - drum->semitone_start + 1;

        for (int note_offset = 0; note_offset < length; note_offset++) {
            // wrap note on overflow
            int note = drum->base_note + note_offset;
            if (note > 127)
                note -= 128;

            fprintf(out, "#define %s_%s %d\n", drum->name, z64_note_name(note), drum->semitone_start + note_offset);
        }

        fprintf(out, "\n");
    }
}

void
emit_h_effects(FILE *out, soundfont *sf)
{
    if (sf->sfx == NULL)
        return;

    int i = 0;
    LL_FOREACH(sfx_data *, sfx, sf->sfx) {
        if (sfx->sample != NULL)
            fprintf(out, "#define %s %d\n", sfx->name, i);
        i++;
    }
    fprintf(out, "\n");
}

NORETURN static void
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [--matching] <filename.xml> <out.c> <out.h>\n", progname);
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
#define NUM_REQUIRED_ARGS 4
#define MAX_OPTIONAL_ARGS 1
    char *filename_in = NULL;
    char *filename_out_c = NULL;
    char *filename_out_h = NULL;
    char *filename_out_name = NULL;
    xmlDocPtr document;
    soundfont sf;

    if (argc != 1 + NUM_REQUIRED_ARGS && argc != 1 + NUM_REQUIRED_ARGS + MAX_OPTIONAL_ARGS)
        usage(argv[0]);

    int argn = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strequ(argv[i], "--matching"))
                sf.matching = true;
        } else {
            switch (argn) {
                case 0:
                    filename_in = argv[i];
                    break;
                case 1:
                    filename_out_c = argv[i];
                    break;
                case 2:
                    filename_out_h = argv[i];
                    break;
                case 3:
                    filename_out_name = argv[i];
                    break;
                default:
                    usage(argv[0]);
            }
            argn++;
        }
    }
    if (argn != NUM_REQUIRED_ARGS)
        usage(argv[0]);

    document = xmlReadFile(filename_in, NULL, XML_PARSE_NONET);
    if (document == NULL)
        return EXIT_FAILURE;

    // xml_print_tree(document);

    xmlNodePtr root = xmlDocGetRootElement(document);
    if (!strequ(XMLSTR_TO_STR(root->name), "Soundfont"))
        error("Root node must be <Soundfont>");
    read_soundfont_info(&sf, root);

    // printf("Wow %s %d [%s] [%s]\n", sf.info.name, sf.info.index, sf.info.bank_path, sf.info.bank_path_dd);

    sf.envelopes = sf.envelope_last = NULL;

    // read all envelopes first irrespective of their positioning in the xml
    LL_FOREACH(xmlNodePtr, node, root->children) {
        const char *name = XMLSTR_TO_STR(node->name);

        if (strequ(name, "Envelopes"))
            read_envelopes_info(&sf, node);
    }

// DEBUG: print envelopes
#if 0
    LL_FOREACH(envelope_data *, envdata, sf.envelopes) {
        printf("ENVELOPE \"%s\" (%d) has %lu points\n", envdata->name, envdata->release, envdata->n_points);

        for (size_t i = 0; i < envdata->n_points; i++) {
            envelope_point *pt = &envdata->points[i];
            printf("  { %5d, %5d }\n", pt->delay, pt->arg);
        }
    }
#endif

    // read all samples
    sf.samples = NULL;
    LL_FOREACH(xmlNodePtr, node, root->children) {
        const char *name = XMLSTR_TO_STR(node->name);

        if (strequ(name, "Samples"))
            read_samples_info(&sf, node);
    }

    // read all instruments
    sf.num_instruments = 0;
    sf.num_drums = 0;
    sf.num_effects = 0;
    sf.instruments = NULL;
    sf.instruments_struct = NULL;
    sf.instruments_struct_last = NULL;
    sf.drums = NULL;
    sf.sfx = NULL;
    LL_FOREACH(xmlNodePtr, node, root->children) {
        const char *name = XMLSTR_TO_STR(node->name);

        if (strequ(name, "Instruments"))
            read_instrs_info(&sf, node);
        if (strequ(name, "Drums"))
            read_drums_info(&sf, node);
        if (strequ(name, "Effects"))
            read_sfx_info(&sf, node);
    }

    // read match padding if it exists
    sf.match_padding = NULL;
    LL_FOREACH(xmlNodePtr, node, root->children) {
        const char *name = XMLSTR_TO_STR(node->name);

        if (strequ(name, "MatchPadding"))
            read_match_padding(&sf, node);
    }

    // emit C source

    FILE *out_c = fopen(filename_out_c, "w");
    fprintf(out_c, "#include \"soundfont_file.h\"\n\n");

    size_t size = 0;
    size += emit_c_header(out_c, &sf);
    size += emit_c_samples(out_c, &sf);
    size += emit_c_envelopes(out_c, &sf);
    size += emit_c_instruments(out_c, &sf);
    size += emit_c_drums(out_c, &sf);
    size += emit_c_effects(out_c, &sf);
    emit_c_match_padding(out_c, &sf, size);

    fclose(out_c);

    // emit C header

    FILE *out_h = fopen(filename_out_h, "w");
    fprintf(out_h,
            // clang-format off
           "#ifndef SOUNDFONT_%d_H_"            "\n"
           "#define SOUNDFONT_%d_H_"            "\n"
        //                                      "\n"
        // "#include \"z64audio.h\""            "\n"
                                                "\n",
            // clang-format on
            sf.info.index, sf.info.index);

    fprintf(out_h,
            // clang-format off
           "#ifdef _LANGUAGE_ASEQ"              "\n"
           ".pushsection .fonts, \"\", @note"   "\n"
           "    .byte %d /*sf id*/"             "\n"
           ".popsection"                        "\n"
           "#endif"                             "\n"
                                                "\n",
            // clang-format on
            sf.info.index);

    fprintf(out_h,
            // clang-format off
           "#define %s_ID %d"                   "\n"
                                                "\n"
           "#define SF%d_NUM_INSTRUMENTS %d"    "\n"
           "#define SF%d_NUM_DRUMS       %d"    "\n"
           "#define SF%d_NUM_SFX         %d"    "\n"
                                                "\n",
            // clang-format on
            sf.info.name, sf.info.index, sf.info.index, sf.num_instruments, sf.info.index, sf.num_drums, sf.info.index,
            sf.num_effects);

    emit_h_instruments(out_h, &sf);
    emit_h_drums(out_h, &sf);
    emit_h_effects(out_h, &sf);

    fprintf(out_h, "#endif\n");
    fclose(out_h);

    // emit name marker

    FILE *out_name = fopen(filename_out_name, "w");
    fprintf(out_name, "%s", sf.info.name);
    fclose(out_name);

    // done

    xmlFreeDoc(document);
    return EXIT_SUCCESS;
}
