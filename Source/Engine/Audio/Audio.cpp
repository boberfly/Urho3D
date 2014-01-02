//
// Copyright (c) 2008-2013 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Precompiled.h"
#include "Audio.h"
#include "Context.h"
#include "CoreEvents.h"
#include "Log.h"
#include "Mutex.h"
#include "ProcessUtils.h"
#include "Profiler.h"
#include "Sound.h"
#include "SoundListener.h"
#include "SoundSource3D.h"

#if !defined(USE_OPENAL)
#include <SDL.h>
#else
#if defined(IOS)
#include <OpenAL/AL.h>
#include <OpenAL/ALC.h>
#include "AudioListenerIOS.h"
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#endif

#include "DebugNew.h"

namespace Urho3D
{

const char* AUDIO_CATEGORY = "Audio";

static const int MIN_BUFFERLENGTH = 20;
static const int MIN_MIXRATE = 11025;
static const int MAX_MIXRATE = 48000;
static const int AUDIO_FPS = 100;

#if !defined(USE_OPENAL)
static void SDLAudioCallback(void *userdata, Uint8 *stream, int len);
#endif

#if defined(ANDROID)
//extern "C " void Android_JNI_StartCapture();
//extern "C " void Android_JNI_StopCapture();
//extern "C" float Android_JNI_GetCaptureLevel();
#endif

Audio::Audio(Context* context) :
    Object(context),
    deviceID_(0),
    sampleSize_(0),
    playing_(false),
    outputLevel_(0.0f),
    captureDataSize_(0),
    captureLevel_(0.0f),
    capturing_(false)
{
    for (unsigned i = 0; i < MAX_SOUND_TYPES; ++i)
        masterGain_[i] = 1.0f;
    #if defined(USE_OPENAL)
    lastErrorALC_ = "ALC_NO_ERROR";
    lastErrorAL_ = "AL_NO_ERROR";
    #endif
    captureSound_ = new Sound(context);

    #if defined(IOS)
    IOSAudioFixOpenALCapture();
    #endif
    // Register Audio library object factories
    RegisterAudioLibrary(context_);
    
    SubscribeToEvent(E_RENDERUPDATE, HANDLER(Audio, HandleRenderUpdate));
}

Audio::~Audio()
{
    Release();
}

bool Audio::SetMode(int bufferLengthMSec, int mixRate, bool stereo, bool interpolation)
{
    #if !defined(USE_OPENAL)
    Release();
    
    bufferLengthMSec = Max(bufferLengthMSec, MIN_BUFFERLENGTH);
    mixRate = Clamp(mixRate, MIN_MIXRATE, MAX_MIXRATE);
    
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    
    desired.freq = mixRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = stereo ? 2 : 1;
    
    // For SDL, do not actually use the buffer length, but calculate a suitable power-of-two size from the mixrate
    if (desired.freq <= 11025)
        desired.samples = 512;
    else if (desired.freq <= 22050)
        desired.samples = 1024;
    else if (desired.freq <= 44100)
        desired.samples = 2048;
    else
        desired.samples = 4096;
    
    desired.callback = SDLAudioCallback;
    desired.userdata = this;
    
    deviceID_ = SDL_OpenAudioDevice(0, SDL_FALSE, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!deviceID_)
    {
        LOGERROR("Could not initialize audio output");
        return false;
    }
    
    if (obtained.format != AUDIO_S16SYS && obtained.format != AUDIO_S16LSB && obtained.format != AUDIO_S16MSB)
    {
        LOGERROR("Could not initialize audio output, 16-bit buffer format not supported");
        SDL_CloseAudioDevice(deviceID_);
        deviceID_ = 0;
        return false;
    }
    
    stereo_ = obtained.channels == 2;
    sampleSize_ = stereo_ ? sizeof(int) : sizeof(short);
    // Guarantee a fragment size that is low enough so that Vorbis decoding buffers do not wrap
    fragmentSize_ = Min((int)NextPowerOfTwo(mixRate >> 6), (int)obtained.samples);
    mixRate_ = mixRate;
    interpolation_ = interpolation;
    clipBuffer_ = new int[stereo ? fragmentSize_ << 1 : fragmentSize_];
    
    LOGINFO("Set audio mode " + String(mixRate_) + " Hz " + (stereo_ ? "stereo" : "mono") + " " +
        (interpolation_ ? "interpolated" : ""));
    
    return Play();
    #else
    Release();

    fragmentSize_ = NextPowerOfTwo(mixRate >> 6);

    int attlist[] = {ALC_FREQUENCY, mixRate, ALC_INVALID};

    alDevice_ = alcOpenDevice(NULL); // select the "preferred device"
    if (alDevice_)
    {
        // use the device to make a context
        alContext_ = alcCreateContext(alDevice_, attlist);
        // set my context to the currently active one
        alcMakeContextCurrent(alContext_);
    }
    else
    {
        if (!checkALCError())
	{
            LOGERROR("OpenAL Error: "+GetErrorALC()+", Failed to open a device");
            LOGERROR("Could not initialize audio output");
            return false;
	}
    }

    stereo_ = 2;
    sampleSize_ = stereo_ ? sizeof(int) : sizeof(short);
    //fragmentSize_ = obtained.samples;
    mixRate_ = mixRate;
    interpolation_ = interpolation;

    LOGINFO("Set audio mode " + String(mixRate_) + " Hz " + (stereo_ ? "stereo" : "mono") + " " +
        (interpolation_ ? "interpolated" : ""));

    return Play();
    #endif
}

void Audio::Update(float timeStep)
{
    PROFILE(UpdateAudio);
    
    // Update in reverse order, because sound sources might remove themselves
    for (unsigned i = soundSources_.Size() - 1; i < soundSources_.Size(); --i)
        soundSources_[i]->Update(timeStep);

    #if defined(IOS)
    captureLevel_ = IOSAudioListenerGetMeter();
    outputLevel_ = IOSAudioOutputMeter();
    #elif defined(ANDROID)
    captureLevel_ = Android_JNI_GetCaptureLevel();
    #endif

    #if defined(USE_OPENAL)
    if (capturing_)
        CaptureToBuffer();
    #endif
}

bool Audio::Play()
{
    if (playing_)
        return true;

    #if defined(USE_OPENAL)
    if (!alContext_)
    {
        LOGERROR("No audio mode set, can not start playback");
        return false;
    }
    #else
    if (!deviceID_)
    {
        LOGERROR("No audio mode set, can not start playback");
        return false;
    }
    
    SDL_PauseAudioDevice(deviceID_, 0);
    #endif

    playing_ = true;
    return true;
}

void Audio::Stop()
{
    playing_ = false;
}

void Audio::SetMasterGain(SoundType type, float gain)
{
    if (type >= MAX_SOUND_TYPES)
        return;
    
    masterGain_[type] = Clamp(gain, 0.0f, 1.0f);
    #if defined(USE_OPENAL)
    if (type == SOUND_MASTER)
        alListenerf(AL_GAIN, masterGain_[SOUND_MASTER]);
    #endif
}

void Audio::SetListener(SoundListener* listener)
{
    listener_ = listener;
}

void Audio::StopSound(Sound* soundClip)
{
    for (PODVector<SoundSource*>::Iterator i = soundSources_.Begin(); i != soundSources_.End(); ++i)
    {
        if ((*i)->GetSound() == soundClip)
            (*i)->Stop();
    }
}

float Audio::GetMasterGain(SoundType type) const
{
    if (type >= MAX_SOUND_TYPES)
        return 0.0f;
    
    return masterGain_[type];
}

SoundListener* Audio::GetListener() const
{
    return listener_;
}

void Audio::AddSoundSource(SoundSource* channel)
{
    MutexLock lock(audioMutex_);
    soundSources_.Push(channel);
}

void Audio::RemoveSoundSource(SoundSource* channel)
{
    PODVector<SoundSource*>::Iterator i = soundSources_.Find(channel);
    if (i != soundSources_.End())
    {
        MutexLock lock(audioMutex_);
        soundSources_.Erase(i);
    }
}

bool Audio::SetCaptureMode(int bufferLengthMSec, int mixRate, bool stereo)
{
    if (captureData_.NotNull())
        captureData_.Reset();

    bufferLengthMSec = Max(bufferLengthMSec, MIN_BUFFERLENGTH);
    mixRate = Clamp(mixRate, MIN_MIXRATE, MAX_MIXRATE);

    unsigned bufferSize = 0;

    if (mixRate <= 11025)
        bufferSize = 4096;
    else if (mixRate <= 22050)
        bufferSize = 8192;
    else if (mixRate <= 44100)
        bufferSize = 16384;
    else
        bufferSize = 32768;

    bufferSize *= stereo ? 2 : 1;

    captureDataSize_ = mixRate * (stereo ? 2 : 1) * (16 / 8) * (bufferLengthMSec / 1000); // I think this is right?

    #if defined(IOS)
    IOSAudioListenerInit();
    #endif
    #if defined(USE_OPENAL)
    if (alContext_)
    {
        //const ALCchar* devices = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);

        alCaptureDevice_ = alcCaptureOpenDevice(NULL, mixRate, stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, bufferSize); // 800
        if (!alCaptureDevice_)
        {
            if (!checkALCError())
                LOGERROR("OpenAL Error: "+GetErrorALC()+" Cannot open OpenAL Capture device.");
            else
                LOGERROR("No OpenAL context created, cannot open capture device.");
            return false;
        }
        captureData_ = new unsigned[captureDataSize_ + 4]; // (IP_SAFETY = 4)
        captureSound_->SetFormat(mixRate, true, stereo);
        return true;
    }
    else
    {
        LOGERROR("No OpenAL context created, cannot open capture device.");
        return false;
    }

    #else
    return false;
    #endif
}

void Audio::StartCapture()
{
    #if defined(USE_OPENAL)
    //capturePosition_ = captureData_.Get(); // Set the pointer
    if (alCaptureDevice_)
    {
        alcCaptureStart(alCaptureDevice_);
        capturePosition_ = captureData_.Get();
        samplesCapturedSize_ = 0;
    }
    capturing_ = true;
    #elif defined(ANDROID)
    //Android_JNI_StartCapture();
    #else
    return;
    #endif
}

void Audio::CaptureToBuffer()
{
    #if defined(USE_OPENAL)
    if (samplesCapturedSize_ >= captureDataSize_)
        StopCapture();
    ALint samplesAvailable = 0;
    alcGetIntegerv(alCaptureDevice_, ALC_CAPTURE_SAMPLES, 1, &samplesAvailable);
    if (samplesAvailable > 0)
    {
       alcCaptureSamples(alCaptureDevice_, (ALCvoid*)capturePosition_, samplesAvailable);
       samplesCapturedSize_ += samplesAvailable * 2;
       capturePosition_ += samplesAvailable / 2;
    }
    #endif
}

void Audio::StopCapture()
{
    #if defined(USE_OPENAL)
    capturing_ = false;
    if (alCaptureDevice_)
    {
        alcCaptureStop(alCaptureDevice_);
        capturePosition_ = captureData_.Get();
    }

    captureSound_->UploadBufferToAL(captureData_.Get(), samplesCapturedSize_);
    samplesCapturedSize_ = 0;
    #elif defined(ANDROID)
    //Android_JNI_StopCapture();
    #else
    return;
    #endif
}

#if !defined(USE_OPENAL)
void SDLAudioCallback(void *userdata, Uint8* stream, int len)
{
    Audio* audio = static_cast<Audio*>(userdata);
    
    {
        MutexLock Lock(audio->GetMutex());
        audio->MixOutput(stream, len / audio->GetSampleSize());
    }
}

void Audio::MixOutput(void *dest, unsigned samples)
{
    if (!playing_ || !clipBuffer_)
    {
        memset(dest, 0, samples * sampleSize_);
        return;
    }
    
    while (samples)
    {
        // If sample count exceeds the fragment (clip buffer) size, split the work
        unsigned workSamples = Min((int)samples, (int)fragmentSize_);
        unsigned clipSamples = workSamples;
        if (stereo_)
            clipSamples <<= 1;
        
        // Clear clip buffer
        int* clipPtr = clipBuffer_.Get();
        memset(clipPtr, 0, clipSamples * sizeof(int));
        
        // Mix samples to clip buffer
        for (PODVector<SoundSource*>::Iterator i = soundSources_.Begin(); i != soundSources_.End(); ++i)
            (*i)->Mix(clipPtr, workSamples, mixRate_, stereo_, interpolation_);
        
        // Copy output from clip buffer to destination
        short* destPtr = (short*)dest;
        while (clipSamples--)
            *destPtr++ = Clamp(*clipPtr++, -32768, 32767);
        
        samples -= workSamples;
        ((unsigned char*&)dest) += sampleSize_ * workSamples;
    }
}
#endif

void Audio::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace RenderUpdate;
    
    Update(eventData[P_TIMESTEP].GetFloat());
}

void Audio::Release()
{
    Stop();

    if (captureData_.NotNull())
        captureData_.Reset();

    #if defined(IOS)
    IOSAudioListenerRelease();
    #endif

    #if !defined(USE_OPENAL)
    if (deviceID_)
    {
        SDL_CloseAudioDevice(deviceID_);
        deviceID_ = 0;
        clipBuffer_.Reset();
    }
    #else
    if (alDevice_)
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(alContext_);
        alcCloseDevice(alDevice_);
        if (alCaptureDevice_)
            alcCaptureCloseDevice(alCaptureDevice_);
    }
    #endif
}

void RegisterAudioLibrary(Context* context)
{
    Sound::RegisterObject(context);
    SoundSource::RegisterObject(context);
    SoundSource3D::RegisterObject(context);
    SoundListener::RegisterObject(context);
}


#if defined(USE_OPENAL)
bool Audio::checkALCError()
{
    ALenum error = ALC_NO_ERROR;

    if( (error = alcGetError( alDevice_ )) != ALC_NO_ERROR )
    {
        switch( error )
        {
        case ALC_INVALID_DEVICE:
            lastErrorALC_ = "ALC_INVALID_DEVICE";
            break;
        case ALC_INVALID_CONTEXT:
            lastErrorALC_ = "ALC_INVALID_CONTEXT";
            break;
        case ALC_INVALID_ENUM:
            lastErrorALC_ = "ALC_INVALID_ENUM";
            break;
        case ALC_INVALID_VALUE:
            lastErrorALC_ = "ALC_INVALID_VALUE";
            break;
        case ALC_OUT_OF_MEMORY:
            lastErrorALC_ = "ALC_OUT_OF_MEMORY";
            break;
        default:
            lastErrorALC_ = "unknown error";
            break;
        }

        return false;
    }

    return true;
}

bool Audio::checkALError()
{
    ALenum error = AL_NO_ERROR;

    if( (error = alGetError()) != AL_NO_ERROR )
    {
        switch( error )
        {
        case AL_INVALID_NAME:
            lastErrorAL_ = "AL_INVALID_NAME";
            break;
        case AL_INVALID_ENUM:
            lastErrorAL_ = "AL_INVALID_ENUM";
            break;
        case AL_INVALID_VALUE:
            lastErrorAL_ = "AL_INVALID_VALUE";
            break;
        case AL_INVALID_OPERATION:
            lastErrorAL_ = "AL_INVALID_OPERATION";
            break;
        case AL_OUT_OF_MEMORY:
            lastErrorAL_ = "AL_OUT_OF_MEMORY";
            break;
        default:
            lastErrorAL_ = "unknown error";
            break;
        }

        return false;
    }

    return true;
}
#endif

}
