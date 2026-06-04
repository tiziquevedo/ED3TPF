#include "song.h"
#include "audio_engine.h"

void Seq_Init(SeqTrack *t, const SeqNote *notes, uint32_t count)
{
    t->notes      = notes;
    t->count      = count;
    t->idx        = 0;
    t->next_tick  = 0;
}

void Seq_Tick(SeqTrack *t)
{
    // Fire all notes whose timestamp has passed.
    // The while handles the case where FillBuffer is
    // called late and multiple notes are overdue.
    while (Audio_Time >= t->next_tick)
    {
        const SeqNote *n = &t->notes[t->idx];

        if (n->pitch > 0.0f)
            Audio_StartVoice(n->sample, n->length, n->pitch);

        t->next_tick += n->duration;
        t->idx        = (t->idx + 1) % t->count;
    }
}
