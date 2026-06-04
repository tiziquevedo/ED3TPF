#ifndef SONG_H
#define SONG_H

#include <stdint.h>

// A single sequenced event
typedef struct {
    const int16_t *sample;      // which sample to play
    uint32_t       length;      // sample length in frames
    float          pitch;       // step value (0.0 = rest)
    uint32_t       duration;    // how long to hold this slot, in audio frames
} SeqNote;

// A track: an array of notes that loops
typedef struct {
    const SeqNote *notes;
    uint32_t       count;
    uint32_t       idx;         // current position in notes[]
    uint32_t       next_tick;   // Audio_Time value when next note fires
} SeqTrack;

void Seq_Init  (SeqTrack *t, const SeqNote *notes, uint32_t count);
void Seq_Tick  (SeqTrack *t);   // call inside Audio_FillBuffer

#endif
