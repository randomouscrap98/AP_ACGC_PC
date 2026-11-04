/* pc_audio.c - SDL3 audio backend with dedicated producer thread.
 *
 * Architecture (matches GC):
 *   Game thread:  Na_GameFrame() queues commands via message queues
 *   Audio thread: pc_audio_process_frame() produces samples into ring buffer
 *   SDL stream callback: reads ring buffer → speakers
 *
 * The audio thread decouples sample production from the game frame,
 * so OS preemption of the game thread doesn't cause audio dropouts.
 */
#include "pc_platform.h"
#include "pc_settings.h"
#include "jaudio_NES/audiothread.h"

#define PC_AUDIO_SAMPLE_RATE 32000

/* lock-free SPSC ring buffer (producer=audio thread, consumer=SDL stream callback) */
#define RING_BUF_SAMPLES (32768) /* ~512ms at 32kHz stereo */
#define RING_BUF_MASK    (RING_BUF_SAMPLES - 1)

/* Produce more samples when buffer drops below this level.
 * ~4 audio frames ahead = ~70ms of buffer at 32kHz stereo. */
#define AUDIO_PRODUCE_THRESHOLD 4480

static s16 ring_buffer[RING_BUF_SAMPLES];
static SDL_AtomicInt ring_write_pos; /* written by audio producer thread */
static SDL_AtomicInt ring_read_pos;  /* written by SDL audio callback */
static SDL_AudioStream* audio_stream = NULL; // the default output device's stream

typedef void (*AIDMACallback)(void);
static AIDMACallback ai_dma_callback = NULL;
static u32 ai_dsp_sample_rate = PC_AUDIO_SAMPLE_RATE;

/* --- Audio producer thread --- */
static SDL_Thread* audio_producer_thread = NULL;
static SDL_AtomicInt audio_thread_running;

// Frame-driven mode (pc_audio_set_frame_driven): samples made so far, only counted
static int s_frame_driven;
static u32 s_frame_samples;

void pc_audio_set_frame_driven(void) {
    s_frame_driven = 1;
}

static int pc_audio_producer_func(void* data) {
    (void)data;
    while (SDL_GetAtomicInt(&audio_thread_running)) {
        int fill = pc_audio_get_buffer_fill();
        if (fill < AUDIO_PRODUCE_THRESHOLD) {
            pc_audio_process_frame();
        } else {
            SDL_Delay(1);
        }
    }
    return 0;
}

void pc_audio_start_producer_thread(void) {
    if (audio_producer_thread) return;
    if (s_frame_driven) return; // pc_audio_frame_tick instead
    SDL_SetAtomicInt(&audio_thread_running, 1);
    audio_producer_thread = SDL_CreateThread(pc_audio_producer_func, "AudioProducer", NULL);
    if (audio_producer_thread) {
        printf("[AUDIO] Producer thread started\n");
    } else {
        printf("[AUDIO] Failed to create producer thread: %s\n", SDL_GetError());
    }
}

/* --- SDL audio callback (runs on SDL's audio device thread) --- */
// len bytes of stereo s16 from the ring, silence where it ran dry
static void pc_audio_fill(s16* out, int len) {
    int total_samples = len / sizeof(s16);
    u32 wp = (u32)SDL_GetAtomicInt(&ring_write_pos);
    SDL_MemoryBarrierAcquire();
    u32 rp = (u32)SDL_GetAtomicInt(&ring_read_pos);
    u32 used = wp - rp;

    /* overrun: producer lapped us */
    if (used > RING_BUF_SAMPLES) {
        rp = wp - RING_BUF_SAMPLES;
        rp &= ~1u; /* stereo-align */
        used = wp - rp;
    }

    int avail = (int)used;
    avail &= ~1; /* whole stereo frames only */
    int copy = (avail < total_samples) ? avail : total_samples;
    copy &= ~1;

    for (int i = 0; i < copy; i++) {
        out[i] = ring_buffer[(rp + i) & RING_BUF_MASK];
    }
    if (copy < total_samples) {
        memset(&out[copy], 0, (total_samples - copy) * sizeof(s16));
    }

    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&ring_read_pos, (int)(rp + copy));
}

// The device wants additional_amount more bytes: always give exactly that (silence on underrun)
static void pc_audio_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    s16 out[2048];

    (void)userdata;
    (void)total_amount;
    additional_amount &= ~3; // whole stereo frames
    while (additional_amount > 0) {
        int len = additional_amount < (int)sizeof(out) ? additional_amount : (int)sizeof(out);
        pc_audio_fill(out, len);
        SDL_PutAudioStreamData(stream, out, len);
        additional_amount -= len;
    }
}

/* --- AI (Audio Interface) --- */

void pc_audio_frame_tick(void) {
    extern u32 pc_frame_counter;
    if (!s_frame_driven) {
        return;
    }
    // Stereo samples due by this frame
    u32 target = (u32)((u64)pc_frame_counter * PC_AUDIO_SAMPLE_RATE * 2 / 60);
    while (s_frame_samples < target) {
        u32 before = s_frame_samples;
        pc_audio_process_frame();
        if (s_frame_samples == before) {
            break; // nothing made (not started yet)
        }
    }
}

void AIInit(u8* stack) {
    (void)stack;
    if (audio_stream != NULL) return;
    if (s_frame_driven) return; // no device: AIInitDMA only counts

    SDL_AudioSpec want = { SDL_AUDIO_S16, 2, PC_AUDIO_SAMPLE_RATE };
    SDL_AudioSpec have;
    int frames = 0;

    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512"); // device buffer, as SDL2 had it
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, pc_audio_callback, NULL);
    if (audio_stream == NULL) {
        // No usable output device: the ring would never drain and the game would stall
        // on a black screen. SDL's dummy driver drains it in real time (silent).
        printf("[AUDIO] Failed to open: %s; using silent dummy audio\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, pc_audio_callback,
                                                     NULL);
        }
    }
    if (audio_stream != NULL && SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio_stream), &have, &frames)) {
        printf("[AUDIO] Opened: freq=%d fmt=0x%04X ch=%d samples=%d (requested: freq=%d)\n",
               have.freq, have.format, have.channels, frames, want.freq);
    } else if (audio_stream == NULL) {
        printf("[AUDIO] Failed to open: %s\n", SDL_GetError());
    }
}

void AIInitDMA(u32 addr, u32 size) {
    s16* src = (s16*)(uintptr_t)addr;
    u32 n_samples = size / sizeof(s16);
    n_samples &= ~1u; /* whole stereo frames */
    if (s_frame_driven) {
        s_frame_samples += n_samples;
        return;
    }

    u32 wp = (u32)SDL_GetAtomicInt(&ring_write_pos);
    u32 rp = (u32)SDL_GetAtomicInt(&ring_read_pos);
    SDL_MemoryBarrierAcquire();
    u32 used = wp - rp;
    u32 free = RING_BUF_SAMPLES - used;

    if (n_samples > free) {
        n_samples = free & ~1u;
    }

    int vol = g_pc_settings.master_volume;
    if (vol < 0)   vol = 0;
    if (vol > 100) vol = 100;

    for (u32 i = 0; i < n_samples; i++) {
        int s = ((int)src[i] * vol) / 100;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        ring_buffer[(wp + i) & RING_BUF_MASK] = (s16)s;
    }

    SDL_MemoryBarrierRelease();
    SDL_SetAtomicInt(&ring_write_pos, (int)(wp + n_samples));
}

// Device streams start paused
void AIStartDMA(void) {
    if (audio_stream != NULL) SDL_ResumeAudioStreamDevice(audio_stream);
}

void AIStopDMA(void) {
    if (audio_stream != NULL) SDL_PauseAudioStreamDevice(audio_stream);
}

void pc_audio_set_paused(int paused) {
    /* Currently unused */
    if (audio_stream == NULL) return;
    if (paused) {
        SDL_PauseAudioStreamDevice(audio_stream);
    } else {
        SDL_ResumeAudioStreamDevice(audio_stream);
    }
}

u32  AIGetDMAStartAddr(void) { return 0; }
u16  AIGetDMALength(void) { return 0; }
u32  AIGetStreamTrigger(void) { return 0; }
u32  AIGetStreamSampleCount(void) { return 0; }
void AISetStreamPlayState(u32 state) { (void)state; }
u32  AIGetStreamPlayState(void) { return 0; }
void AISetStreamSampleRate(u32 rate) { (void)rate; }
u32  AIGetStreamSampleRate(void) { return PC_AUDIO_SAMPLE_RATE; }
void AISetStreamVolLeft(u8 vol) { (void)vol; }
void AISetStreamVolRight(u8 vol) { (void)vol; }
u8   AIGetStreamVolLeft(void) { return 0; }
u8   AIGetStreamVolRight(void) { return 0; }
void AIResetStreamSampleCount(void) {}
void AISetDSPSampleRate(u32 rate) { ai_dsp_sample_rate = rate; }
u32  AIGetDSPSampleRate(void) { return ai_dsp_sample_rate; }

void* AIRegisterDMACallback(void* callback) {
    void* old = (void*)ai_dma_callback;
    ai_dma_callback = (AIDMACallback)callback;
    return old;
}

/* --- DSP stubs (rspsim does everything in software) --- */

void DSPInit(void) {}
BOOL DSPCheckMailToDSP(void) { return FALSE; }
BOOL DSPCheckMailFromDSP(void) { return FALSE; }
u32  DSPReadMailFromDSP(void) { return 0; }
void DSPSendMailToDSP(u32 mail) { (void)mail; }
void DSPAssertInt(void) {}
void* DSPAddTask(void* task) { return task; }

/* --- ring buffer queries for frame pacing (pc_vi.c) --- */

int pc_audio_get_buffer_fill(void) {
    return SDL_GetAtomicInt(&ring_write_pos) - SDL_GetAtomicInt(&ring_read_pos);
}

int pc_audio_is_active(void) {
    return audio_stream != NULL;
}

void pc_audio_shutdown(void) {
    /* Stop producer thread first */
    SDL_SetAtomicInt(&audio_thread_running, 0);
    if (audio_producer_thread) {
        SDL_WaitThread(audio_producer_thread, NULL);
        audio_producer_thread = NULL;
    }
    if (audio_stream != NULL) {
        SDL_DestroyAudioStream(audio_stream); // closes its device too
        audio_stream = NULL;
    }
}
