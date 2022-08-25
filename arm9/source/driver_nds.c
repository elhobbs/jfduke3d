/*
 Copyright (C) 2009 Jonathon Fowler <jf@jonof.id.au>
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */

/**
 * Stub driver for no output
 */
#include <nds.h>
#include "midifuncs.h"
#include <string.h>

static volatile char *MixBuffer = 0;
static volatile int MixBufferSize = 0;
static volatile int MixBufferCount = 0;
static volatile int MixBufferCurrent = 0;
static volatile int MixBufferUsed = 0;
static volatile void ( *MixCallBack )( void ) = 0;
static volatile int MixBufferChannel = -1;
static volatile uint64_t MixBufferStartTime = 0;
static volatile uint64_t MixBufferLastTime = 0;
static volatile uint32_t MixBufferSizeMask = 0;
static volatile uint8_t *mix_buffer = 0;
static volatile uint64_t mix_filled = 0;

#define MIX_LENGTH (16*1024)
#define MIX_MASK (MIX_LENGTH-1)

static volatile int Initialised = 0;

uint64_t ds_time();
void NdsSoundDrv_PCM_StopPlayback(void);

static void FillBufferPortion(int remaining)
{
    int len;
	uint8_t *sptr;

	while (remaining > 0) {
		if (MixBufferUsed == MixBufferSize) {
			MixCallBack();
            //printf("%08x + ", MixCallBack);
			
			MixBufferUsed = 0;
			MixBufferCurrent++;
			if (MixBufferCurrent >= MixBufferCount) {
				MixBufferCurrent -= MixBufferCount;
			}
		}
		
		while (remaining > 0 && MixBufferUsed < MixBufferSize) {
			sptr = MixBuffer + (MixBufferCurrent * MixBufferSize) + MixBufferUsed;
			
			len = MixBufferSize - MixBufferUsed;
			if (remaining < len) {
				len = remaining;
			}
            int lenx = len;
            int pos = mix_filled & MIX_MASK;
            if((pos + len) > MIX_LENGTH) {
                lenx = MIX_LENGTH - pos;
                //if(mix_buffer == 0 || pos < 0 || pos >= MIX_LENGTH || (pos + lenx) > MIX_LENGTH) {
                //    printf("m1 %08x %6d %6d\n", mix_buffer, pos, lenx);
                //    exit(-1);
                //}
                uint8_t *buf = &mix_buffer[pos];
                for(int i=0;i<lenx;i++) {
                    *buf++ = sptr[i] ^ 0x80;
                }
                //memcpy(&mix_buffer[pos],sptr,lenx);
                DC_FlushRange(&mix_buffer[pos],lenx);
                mix_filled += lenx;
                sptr += lenx;
                lenx = len - lenx;
                pos = 0;
            }
            if(lenx) {
                //if(mix_buffer == 0 || pos < 0 || pos >= MIX_LENGTH || (pos + lenx) > MIX_LENGTH) {
                //    printf("m2 %08x %6d %6d\n", mix_buffer, pos, lenx);
                //    exit(-1);
                //}
                uint8_t *buf = &mix_buffer[pos];
                for(int i=0;i<lenx;i++) {
                    *buf++ = sptr[i] ^ 0x80;
                }
                //memcpy(&mix_buffer[pos],sptr,lenx);
                DC_FlushRange(&mix_buffer[pos],lenx);
                mix_filled += lenx;
                sptr += lenx;
            }
			
			//ptr += len;
			MixBufferUsed += len;
			remaining -= len;
		}
	}
}

volatile int in_interrupt = 0;
static void NdsSoundDrv_FillBuffer()
{
    if(in_interrupt != 0) {
        return;
    }
    in_interrupt = 1;
    //HRESULT err;

    uint64_t current = ds_time() - MixBufferStartTime;
    if(mix_filled < current) {
        mix_filled = current;
    }
	//if (current > 0x40000000) {
	//	current -= 0x40000000;
	//	filled -= 0x40000000;
	//}

    uint64_t end = current + (256);
    uint32_t count = end - mix_filled;

    if(count > 0) {
        FillBufferPortion(count);
    }

    in_interrupt = 0;
}


int NdsSoundDrv_GetError(void)
{
	return 0;
}

const char *NdsSoundDrv_ErrorString( int ErrorNumber )
{
	(void)ErrorNumber;
	return "No sound, Ok.";
}

int NdsSoundDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata)
{
    printf("NdsSoundDrv_PCM_Init %d %d %d %08x\n",*mixrate, *numchannels, *samplebits, initdata);
    if (Initialised) {
        NdsSoundDrv_PCM_Shutdown();
    }
	(void)mixrate; (void)numchannels; (void)samplebits; (void)initdata;
    Initialised = 1;
	return 0;
}

void NdsSoundDrv_PCM_Shutdown(void)
{
    printf("NdsSoundDrv_PCM_Shutdown\n");
    if (!Initialised) {
        return;
    }
    
    NdsSoundDrv_PCM_StopPlayback();
    
    Initialised = 0;
}

int NdsSoundDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
						int NumDivisions, void ( *CallBackFunc )( void ) )
{
    printf("NdsSoundDrv_PCM_BeginPlayback %08x %d %d %08x\n",BufferStart, BufferSize, NumDivisions, CallBackFunc);
	(void)BufferStart; (void)BufferSize; (void)NumDivisions; (void)CallBackFunc;
    if (!Initialised) {
        return 1;
    }

    NdsSoundDrv_PCM_StopPlayback();
    
	MixBuffer = BufferStart;
	MixBufferSize = BufferSize;
	MixBufferCount = NumDivisions;
	MixBufferCurrent = 0;
	MixBufferUsed = 0;
	MixCallBack = CallBackFunc;
    MixBufferSizeMask = (BufferSize * NumDivisions) - 1;

    mix_buffer = malloc(MIX_LENGTH);
    if(mix_buffer == 0) {
        printf("mix_buffer == 0");
        exit(-1);
    }
    memset(mix_buffer,0,MIX_LENGTH);
    //MixBufferChannel = soundPlaySample(MixBuffer, SoundFormat_8Bit, MixBufferSize * MixBufferCount, 11025, 127, 64, true, 0);
    MixBufferChannel = soundPlaySample(mix_buffer, SoundFormat_8Bit, MIX_LENGTH, 11025, 127, 64, true, 0);
    MixBufferLastTime = MixBufferStartTime = ds_time();

	// prime the buffer
	//NdsSoundDrv_FillBuffer();


	//timerStart( 3, ClockDivider_1024, TIMER_FREQ_1024( 60 ), FillBuffer );
	irqSet(IRQ_VBLANK, NdsSoundDrv_FillBuffer);
	irqEnable( IRQ_VBLANK);

	return 0;
}

void NdsSoundDrv_PCM_StopPlayback(void)
{
    printf("NdsSoundDrv_PCM_StopPlayback\n");
    if(MixBufferChannel != -1) {
        soundKill(MixBufferChannel);
        MixBufferChannel = -1;
    }
}

static int oldIME;
void NdsSoundDrv_PCM_Lock(void)
{
    oldIME = enterCriticalSection();
}

void NdsSoundDrv_PCM_Unlock(void)
{
    leaveCriticalSection(oldIME);
}


int NdsSoundDrv_CD_Init(void)
{
    return 0;
}

void NdsSoundDrv_CD_Shutdown(void)
{
}

int NdsSoundDrv_CD_Play(int track, int loop)
{
    (void)track; (void)loop;
    return 0;
}

void NdsSoundDrv_CD_Stop(void)
{
}

void NdsSoundDrv_CD_Pause(int pauseon)
{
    (void)pauseon;
}

int NdsSoundDrv_CD_IsPlaying(void)
{
    return 0;
}

void NdsSoundDrv_CD_SetVolume(int volume)
{
    (void)volume;
}

int NdsSoundDrv_MIDI_Init(midifuncs *funcs, const char *params)
{
    (void)params;
    memset(funcs, 0, sizeof(midifuncs));
    return 0;
}

void NdsSoundDrv_MIDI_Shutdown(void)
{
}

int  NdsSoundDrv_MIDI_StartPlayback(void (*service)(void))
{
    (void)service;
    return 0;
}

void NdsSoundDrv_MIDI_HaltPlayback(void)
{
}

unsigned int NdsSoundDrv_MIDI_GetTick(void)
{
    return 0;
}

void NdsSoundDrv_MIDI_SetTempo(int tempo, int division)
{
    (void)tempo; (void)division;
}

void NdsSoundDrv_MIDI_Lock(void)
{
}

void NdsSoundDrv_MIDI_Unlock(void)
{
}

