#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include "windows.h"
#include "mmsystem.h"
#include "mmreg.h"

// some song information
#include "Synth.h"

// if USE_BLOBS is defined in the project (preprocessor settings) use the file based blob loading
// for that the 64k2Song.h.blob and 64k2Patch.h.blob files need to be next to this exe
#ifdef USE_BLOBS
#define MAX_SAMPLES (10*60*44100)
#define STREAM_FILEBUFFER_SIZE 1024*1024*10
#define PATCH_FILEBUFFER_SIZE 1024*1024*10
char	lpStreamBuffer[STREAM_FILEBUFFER_SIZE];
char	lpPatchBuffer[PATCH_FILEBUFFER_SIZE];
// if USE_BLOBS is NOT defined in the project use the classic way
// include file based compilation/baking of the song and patch with all optimizations
#else
#define INCLUDE_NODES
#include "64k2Patch.h"
#include "64k2Song.h"
#endif

#define SAMPLE_RATE 44100
#define SAMPLE_TYPE float
SAMPLE_TYPE lpSoundBuffer[MAX_SAMPLES*2 + 44100*60]; // add safety buffer for 60s 
HWAVEOUT hWaveOut;

/////////////////////////////////////////////////////////////////////////////////
// initialized data
/////////////////////////////////////////////////////////////////////////////////

WAVEFORMATEX WaveFMT =
{
	WAVE_FORMAT_IEEE_FLOAT,
	2, // channels
	SAMPLE_RATE, // samples per sec
	SAMPLE_RATE*sizeof(SAMPLE_TYPE)*2, // bytes per sec
	sizeof(SAMPLE_TYPE)*2, // block alignment;
	sizeof(SAMPLE_TYPE)*8, // bits per sample
	0 // extension not needed
};

WAVEHDR WaveHDR = 
{
	(LPSTR)lpSoundBuffer, 
	MAX_SAMPLES*sizeof(SAMPLE_TYPE)*2,			// MAX_SAMPLES*sizeof(float)*2(stereo)
	0, 
	0, 
	0, 
	0, 
	0, 
	0
};

MMTIME MMTime = 
{ 
	TIME_SAMPLES,
	0
};

/////////////////////////////////////////////////////////////////////////////////
// crt emulation
/////////////////////////////////////////////////////////////////////////////////

extern "C" 
{
	int _fltused = 1;
}

/////////////////////////////////////////////////////////////////////////////////
// entry point for the executable if msvcrt is not used
/////////////////////////////////////////////////////////////////////////////////

#if defined _DEBUG
void main(void)
#else
void mainCRTStartup(void)
#endif
{
	// init synth and start filling the buffer 
#ifdef USE_BLOBS
	// read song stream blob
	HANDLE hSynthStreamFile = CreateFileA("64k2Song.h.blob",               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template
	DWORD streambytes = 0;
	ReadFile(hSynthStreamFile, lpStreamBuffer, STREAM_FILEBUFFER_SIZE-1, &streambytes, NULL);
	CloseHandle(hSynthStreamFile);

	// read patch blob
	HANDLE hSynthPatchFile = CreateFileA("64k2Patch.h.blob",               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template
	DWORD patchbytes = 0;
	ReadFile(hSynthPatchFile, lpPatchBuffer, PATCH_FILEBUFFER_SIZE-1, &patchbytes, NULL);
	CloseHandle(hSynthPatchFile);

	// init with blobs
	_64klang_Init((BYTE*)lpStreamBuffer, (void*)lpPatchBuffer);
	int SongLength = *(((int*)lpStreamBuffer) + 1);
#else
	_64klang_Init(SynthStream, SynthNodes, SynthMonoConstantOffset, SynthStereoConstantOffset, SynthMaxOffset);
	int SongLength = *(((int*)SynthStream) + 1);	
#endif

#if 1
	// threaded buffer fill with 5 seconds heads up
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)_64klang_Render, lpSoundBuffer, 0, 0);
	Sleep(5000);
#else
	// sequential (blocking) buffer fill aka precalc
	_64klang_Render(lpSoundBuffer);
#endif	

	// start audio playback
	waveOutOpen(&hWaveOut, WAVE_MAPPER, &WaveFMT, NULL, 0, CALLBACK_NULL);
	waveOutPrepareHeader(hWaveOut, &WaveHDR, sizeof(WaveHDR));
	waveOutWrite(hWaveOut, &WaveHDR, sizeof(WaveHDR));
	do
	{
		waveOutGetPosition(hWaveOut, &MMTime, sizeof(MMTIME));
		Sleep(128);
	} while ((MMTime.u.sample < SongLength) && !GetAsyncKeyState(VK_ESCAPE));

	// done
	ExitProcess(0);
}
