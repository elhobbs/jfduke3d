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


int NdsSoundDrv_GetError(void);
const char *NdsSoundDrv_ErrorString( int ErrorNumber );

int  NdsSoundDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata);
void NdsSoundDrv_PCM_Shutdown(void);
int  NdsSoundDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
              int NumDivisions, void ( *CallBackFunc )( void ) );
void NdsSoundDrv_PCM_StopPlayback(void);
void NdsSoundDrv_PCM_Lock(void);
void NdsSoundDrv_PCM_Unlock(void);

int  NdsSoundDrv_CD_Init(void);
void NdsSoundDrv_CD_Shutdown(void);
int  NdsSoundDrv_CD_Play(int track, int loop);
void NdsSoundDrv_CD_Stop(void);
void NdsSoundDrv_CD_Pause(int pauseon);
int  NdsSoundDrv_CD_IsPlaying(void);
void NdsSoundDrv_CD_SetVolume(int volume);

int  NdsSoundDrv_MIDI_Init(midifuncs *, const char *);
void NdsSoundDrv_MIDI_Shutdown(void);
int  NdsSoundDrv_MIDI_StartPlayback(void (*service)(void));
void NdsSoundDrv_MIDI_HaltPlayback(void);
unsigned int NdsSoundDrv_MIDI_GetTick(void);
void NdsSoundDrv_MIDI_SetTempo(int tempo, int division);
void NdsSoundDrv_MIDI_Lock(void);
void NdsSoundDrv_MIDI_Unlock(void);

