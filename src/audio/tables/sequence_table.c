#include "z64audio.h"

// TODO move
#ifndef NO_REORDER
#ifdef __GNUC__
#define NO_REORDER __attribute__((no_reorder))
#else
#define NO_REORDER
#endif
#endif

extern AudioTable gSequenceTable;
#pragma weak gSequenceTable = sSequenceTableHeader

#define DEFINE_SEQUENCE(name, seqId, storageMedium, cachePolicy, seqFlags, seqPlayer) \
    extern u8 name##_Start[]; extern u8 name##_Size[];
#define DEFINE_SEQUENCE_PTR(seqIdReal, seqId, storageMediumReal, cachePolicyReal, seqFlags, seqPlayer) \
    /*empty*/

#include "sequence_table.h"

#undef DEFINE_SEQUENCE
#undef DEFINE_SEQUENCE_PTR

NO_REORDER AudioTableHeader sSequenceTableHeader = {
#define DEFINE_SEQUENCE(name, seqId, storageMedium, cachePolicy, seqFlags, seqPlayer) \
    1+
#define DEFINE_SEQUENCE_PTR(seqIdReal, seqId, storageMediumReal, cachePolicyReal, seqFlags, seqPlayer) \
    1+

#include "sequence_table.h"

#undef DEFINE_SEQUENCE
#undef DEFINE_SEQUENCE_PTR
    0,

    0,
    0x00000000,
    { 0, 0, 0, 0, 0, 0, 0, 0 },
};

NO_REORDER AudioTableEntry sSequenceTableEntries[] = {
#define DEFINE_SEQUENCE(name, seqId, storageMedium, cachePolicy, seqFlags, seqPlayer) \
    { (u32)name##_Start, (u32)name##_Size, (storageMedium), (cachePolicy), 0, 0, 0 },
#define DEFINE_SEQUENCE_PTR(seqIdReal, seqId, storageMediumReal, cachePolicyReal, seqFlags, seqPlayer) \
    { (seqIdReal), 0, (storageMediumReal), (cachePolicyReal), 0, 0, 0 },

#include "sequence_table.h"

#undef DEFINE_SEQUENCE
#undef DEFINE_SEQUENCE_PTR
};
