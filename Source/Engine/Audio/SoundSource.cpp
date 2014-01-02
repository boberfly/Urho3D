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
#include "ResourceCache.h"
#include "Sound.h"
#include "SoundSource.h"
#if defined(USE_OPENAL)
#include "Log.h"
#endif

#include <cstring>

#include "DebugNew.h"

namespace Urho3D
{
#if !defined( USE_OPENAL)
#define INC_POS_LOOPED() \
    pos += intAdd; \
    fractPos += fractAdd; \
    if (fractPos > 65535) \
    { \
        fractPos &= 65535; \
        ++pos; \
    } \
    while (pos >= end) \
        pos -= (end - repeat); \

#define INC_POS_ONESHOT() \
    pos += intAdd; \
    fractPos += fractAdd; \
    if (fractPos > 65535) \
    { \
        fractPos &= 65535; \
        ++pos; \
    } \
    if (pos >= end) \
    { \
        pos = 0; \
        break; \
    } \

#define INC_POS_STEREO_LOOPED() \
    pos += (intAdd << 1); \
    fractPos += fractAdd; \
    if (fractPos > 65535) \
    { \
        fractPos &= 65535; \
        pos += 2; \
    } \
    while (pos >= end) \
        pos -= (end - repeat); \

#define INC_POS_STEREO_ONESHOT() \
    pos += (intAdd << 1); \
    fractPos += fractAdd; \
    if (fractPos > 65535) \
    { \
        fractPos &= 65535; \
        pos += 2; \
    } \
    if (pos >= end) \
    { \
        pos = 0; \
        break; \
    } \

#define GET_IP_SAMPLE() (((((int)pos[1] - (int)pos[0]) * fractPos) / 65536) + (int)pos[0])

#define GET_IP_SAMPLE_LEFT() (((((int)pos[2] - (int)pos[0]) * fractPos) / 65536) + (int)pos[0])

#define GET_IP_SAMPLE_RIGHT() (((((int)pos[3] - (int)pos[1]) * fractPos) / 65536) + (int)pos[1])

#else

#if defined(ENABLE_LOGGING)
#define IF_AL_ERROR(message); if(!audio_->checkALError()){message;}
#else
#define IF_AL_ERROR(message);
#endif

#endif

static const char* typeNames[] =
{
    "Effect",
    "Ambient",
    "Voice",
    "Music",
    0
};

static const float AUTOREMOVE_DELAY = 0.25f;

extern const char* AUDIO_CATEGORY;

SoundSource::SoundSource(Context* context) :
    Component(context),
    soundType_(SOUND_EFFECT),
    frequency_(0.0f),
    gain_(1.0f),
    attenuation_(1.0f),
    panning_(0.0f),
    pitch_(1.0f),
    autoRemoveTimer_(0.0f),
    autoRemove_(false),
    position_(0),
#if !defined(USE_OPENAL)
    fractPosition_(0),
#endif
    timePosition_(0.0f),
    decoder_(0),
    decodePosition_(0)
{

    audio_ = GetSubsystem<Audio>();

#if defined(USE_OPENAL)
    alGenSources(1, &alSource_);
    IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", alGenSources(1, &alSource_)"));

    alSourcef(alSource_, AL_PITCH, pitch_);
    alSourcef(alSource_, AL_VELOCITY, 10.0f );
    alSourcef(alSource_, AL_GAIN, gain_);
    alSourcef(alSource_, AL_ROLLOFF_FACTOR, attenuation_); // Is this correct?
    alSource3f(alSource_, AL_POSITION, panning_, 0.0f, 0.0f); // Is this correct?
    alSourcef(alSource_, AL_SEC_OFFSET, timePosition_);
    alSourcef(alSource_, AL_MIN_GAIN, 0.0f);
    alSourcef(alSource_, AL_MAX_GAIN, 1.0f);
    alSourcei(alSource_, AL_SOURCE_RELATIVE, AL_TRUE); // 2D sound (will cancel out 3D)
#endif
    
    if (audio_)
        audio_->AddSoundSource(this);
}

SoundSource::~SoundSource()
{
    if (audio_)
        audio_->RemoveSoundSource(this);

    FreeDecoder();

#if defined(USE_OPENAL)
    alSourceStop(alSource_);
    alDeleteSources(1, &alSource_);
#endif
}

void SoundSource::RegisterObject(Context* context)
{
    context->RegisterFactory<SoundSource>(AUDIO_CATEGORY);

    ACCESSOR_ATTRIBUTE(SoundSource, VAR_BOOL, "Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(SoundSource, VAR_RESOURCEREF, "Sound", GetSoundAttr, SetSoundAttr, ResourceRef, ResourceRef(Sound::GetTypeStatic()), AM_DEFAULT);
    ENUM_ATTRIBUTE(SoundSource, "Sound Type", soundType_, typeNames, SOUND_EFFECT, AM_DEFAULT);
    ATTRIBUTE(SoundSource, VAR_FLOAT, "Frequency", frequency_, 0.0f, AM_DEFAULT);
    ATTRIBUTE(SoundSource, VAR_FLOAT, "Gain", gain_, 1.0f, AM_DEFAULT);
    ATTRIBUTE(SoundSource, VAR_FLOAT, "Attenuation", attenuation_, 1.0f, AM_DEFAULT);
    ATTRIBUTE(SoundSource, VAR_FLOAT, "Panning", panning_, 0.0f, AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(SoundSource, VAR_BOOL, "Is Playing", IsPlaying, SetPlayingAttr, bool, false, AM_DEFAULT);
    ATTRIBUTE(SoundSource, VAR_BOOL, "Autoremove on Stop", autoRemove_, false, AM_FILE);
    ACCESSOR_ATTRIBUTE(SoundSource, VAR_RESOURCEREF, "Sound", GetSoundAttr, SetSoundAttr, ResourceRef, ResourceRef(Sound::GetTypeStatic()), AM_DEFAULT);
    ACCESSOR_ATTRIBUTE(SoundSource, VAR_INT, "Play Position", GetPositionAttr, SetPositionAttr, int, 0, AM_FILE);
}

void SoundSource::Play(Sound* sound)
{
    if (!audio_)
        return;

    // If no frequency set yet, set from the sound's default
    if (frequency_ == 0.0f && sound)
        SetFrequency(sound->GetFrequency());
    
    #if defined(USE_OPENAL)
    PlayOpenAL(sound);
    #else
    // If sound source is currently playing, have to lock the audio mutex
    if (position_)
    {
        MutexLock lock(audio_->GetMutex());
        PlayLockless(sound);
    }
    else
        PlayLockless(sound);
    #endif
    
    MarkNetworkUpdate();
}

void SoundSource::Play(Sound* sound, float frequency)
{
    SetFrequency(frequency);
    Play(sound);
}

void SoundSource::Play(Sound* sound, float frequency, float gain)
{
    SetFrequency(frequency);
    SetGain(gain);
    Play(sound);
}

void SoundSource::Play(Sound* sound, float frequency, float gain, float panning)
{
    SetFrequency(frequency);
    SetGain(gain);
    SetPanning(panning);
    Play(sound);
}

void SoundSource::Stop()
{
    if (!audio_)
        return;
    #if defined(USE_OPENAL)
    alSourceStop(alSource_);
    position_ = 0;
    timePosition_ = 0.0f;
    alSourcei(alSource_, AL_BUFFER, 0);
    #else
    // If sound source is currently playing, have to lock the audio mutex
    if (position_)
    {
        MutexLock lock(audio_->GetMutex());
        StopLockless();
    }

    // Free the compressed sound decoder now if any
    FreeDecoder();
    #endif
    
    MarkNetworkUpdate();
}

void SoundSource::SetSoundType(SoundType type)
{
    if (type == SOUND_MASTER || type >= MAX_SOUND_TYPES)
        return;

    soundType_ = type;
    MarkNetworkUpdate();
}

void SoundSource::SetFrequency(float frequency)
{
    frequency_ = Clamp(frequency, 0.0f, 535232.0f);

    MarkNetworkUpdate();
}

void SoundSource::SetGain(float gain)
{
    gain_ = Max(gain, 0.0f);
    MarkNetworkUpdate();
}

void SoundSource::SetAttenuation(float attenuation)
{
    attenuation_ = Clamp(attenuation, 0.0f, 1.0f);
    MarkNetworkUpdate();
}

void SoundSource::SetPanning(float panning)
{
    panning_ = Clamp(panning, -1.0f, 1.0f);
    MarkNetworkUpdate();
}

void SoundSource::SetPitch(float pitch)
{
    pitch_ = Clamp(pitch, 0.0f, 2.0f);
    MarkNetworkUpdate();
}

void SoundSource::SetAutoRemove(bool enable)
{
    autoRemove_ = enable;
}

bool SoundSource::IsPlaying() const
{
#if defined(USE_OPENAL)
    int state;
    alGetSourcei(alSource_, AL_SOURCE_STATE, &state );

    return state == AL_PLAYING;
#else
    return sound_ != 0 && position_ != 0;
#endif
}

#if !defined (USE_OPENAL)
void SoundSource::SetPlayPosition(signed char* pos)
{
    if (!audio_ || !sound_)
        return;

    MutexLock lock(audio_->GetMutex());
    SetPlayPositionLockless(pos);
}

void SoundSource::PlayLockless(Sound* sound)
{
    // Reset the time position in any case
    timePosition_ = 0.0f;

    if (sound)
    {
        if (!sound->IsCompressed())
        {
            // Uncompressed sound start
            signed char* start = sound->GetStart();
            if (start)
            {
                // Free decoder in case previous sound was compressed
                FreeDecoder();
                sound_ = sound;
                position_ = start;
                fractPosition_ = 0;
                return;
            }
        }
        else
        {
            // Compressed sound start
            if (sound == sound_ && decoder_)
            {
                // If same compressed sound is already playing, rewind the decoder
                sound_->RewindDecoder(decoder_);
                return;
            }
            else
            {
                // Else just set the new sound with a dummy start position. The mixing routine will allocate the new decoder
                FreeDecoder();
                sound_ = sound;
                position_ = sound->GetStart();
                return;
            }
        }
    }

    // If sound pointer is null or if sound has no data, stop playback
    FreeDecoder();
    sound_.Reset();
    position_ = 0;
}

void SoundSource::StopLockless()
{
    position_ = 0;
    timePosition_ = 0.0f;
}

void SoundSource::SetPlayPositionLockless(signed char* pos)
{
    // Setting position on a compressed sound is not supported
    if (!sound_ || sound_->IsCompressed())
        return;

    signed char* start = sound_->GetStart();
    signed char* end = sound_->GetEnd();
    if (pos < start)
        pos = start;
    if (sound_->IsSixteenBit() && (pos - start) & 1)
        ++pos;
    if (pos > end)
        pos = end;

    position_ = pos;
    timePosition_ = ((float)(int)(size_t)(pos - sound_->GetStart())) / (sound_->GetSampleSize() * sound_->GetFrequency());
}

#else // USE_OPENAL

void SoundSource::PlayOpenAL(Sound* sound)
{
    if (sound)
    {
        if (!sound->IsCompressed())
        {
            if (sound == sound_)
            {
                if (!IsPlaying())
                {
                    alSourcei(alSource_, AL_BUFFER, sound->GetALBuffer());
                    alSourcePlay(alSource_);
                    IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot play alSource"));
                }
                return;
            }
            else
            {
            	// Stop the current sound
            	Stop();
                // Free Decoder in case previous sound was compressed
                FreeDecoder();
                // Load in buffer
                alSourcei(alSource_, AL_BUFFER, sound->GetALBuffer());
                IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot set alSource to AL_BUFFER"));

                // Set OpenAL buffer and if it's looping or not
                // (need to use the member directly otherwise iOS won't get the value from ->IsLooped())
                alSourcei(alSource_, AL_LOOPING, sound->looped_);
                // Set proper volume
                float soundTypeGain = audio_->GetMasterGain(soundType_);
                alSourcef(alSource_, AL_GAIN, gain_ * soundTypeGain);
                // Set OpenAL source Attributes
                alSourcef(alSource_, AL_ROLLOFF_FACTOR, attenuation_);
                alSource3f(alSource_, AL_POSITION, panning_, 0.0f, 0.0f);
                alSourcef(alSource_, AL_PITCH, pitch_);

                sound_ = sound;
                position_ = 0;

                alSourcePlay(alSource_);
                IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot play alSource"));
                return;
            }
        }
        else
        {
            // Compressed sound start
            if (sound == sound_)
            {
                // We stop the existing compressed sound
            	Stop();
                // If same compressed sound is already playing, rewind the Decoder
                sound_->RewindDecoder(decoder_);
                //alSourceQueueBuffers(alSource_, 2, sound_->GetALBuffer());
                alSourcePlay(alSource_);
                IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot play alSource"));
                return;
            }
            else
            {
                // We stop the existing compressed sound
            	Stop();
                FreeDecoder(); // This will alSourceUnqueueBuffers

                // Streaming audio is never looped at the AL-level
                alSourcei(alSource_, AL_LOOPING, AL_FALSE );
                // Set proper volume
                float soundTypeGain = audio_->GetMasterGain(soundType_);
                alSourcef(alSource_, AL_GAIN, gain_ * soundTypeGain);

                // Set OpenAL source Attributes
                alSourcef(alSource_, AL_ROLLOFF_FACTOR, attenuation_);
                alSource3f(alSource_, AL_POSITION, panning_, 0.0f, 0.0f);

                sound_ = sound;
                position_ = 0;

                // Source Queue the buffers
                alSourceQueueBuffers(alSource_, 2, sound_->GetALBufferPointer());
                IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot queue new buffer"));
                // Setup the decoder and decode buffer
                decoder_ = sound_->AllocateDecoder();
                unsigned sampleSize = sound_->GetSampleSize();
                unsigned DecodeBufferSize = sampleSize * sound_->GetIntFrequency() * DECODE_BUFFER_LENGTH / 1000;
                sound_->DecodeOpenAL(decoder_, sound_->GetALBuffer(), DecodeBufferSize);
                
                // Start playing the decode buffer
                alSourcePlay(alSource_);
                IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot play streaming alSource"));
                return;
            }
        }
    }
    
    // If sound pointer is null or if sound has no data, stop playback
    FreeDecoder();
    sound_.Reset();
    position_ = 0;
    timePosition_ = 0;
}

void SoundSource::SetPlayPositionOpenAL(int pos)
{
    // Setting position on a compressed sound is not supported
    if (!sound_ || sound_->IsCompressed())
        return;
    
    // If the sound is playing, stop it first
    if(IsPlaying())
        Stop();

    // Set offset
    alSourcei(alSource_, AL_BYTE_OFFSET, pos);

    // Get offset and update position_ and timePosition_
    ALint position = 0;
    ALfloat timePosition = 0.0f;
    alGetSourcei(alSource_, AL_BYTE_OFFSET, &position);
    alGetSourcef(alSource_, AL_SEC_OFFSET, &timePosition);

    position_ = position;
    timePosition_ = timePosition;

    // Play the sound again
    PlayOpenAL(sound_);
}

void SoundSource::UpdateOpenAL(float timeStep)
{
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(alSource_, AL_POSITION, panning_, 0.0f, 0.0f);

    float soundTypeGain = audio_->GetMasterGain(soundType_);
    alSourcef(alSource_, AL_GAIN, gain_ * soundTypeGain);

    // Update position_ and timePosition_ from OpenAL
    ALint position = 0;
    ALfloat timePosition = 0.0f;
    // Get position data
    alGetSourcei(alSource_, AL_BYTE_OFFSET, &position);
    IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", error getting AL_BYTE_OFFSET from alSource"));
    alGetSourcef(alSource_, AL_SEC_OFFSET, &timePosition);
    IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", error getting AL_SEC_OFFSET from alSource"));

    position_ = position;
    timePosition_ = timePosition;

    if(sound_)
    {
        if (sound_->IsCompressed())
        {
            if (!StreamOpenAL(timeStep))
            	LOGERROR("Cannot stream " + sound_->GetName());
        }
        else
            // Need to use the member directly otherwise iOS won't pick up the value.
            alSourcei(alSource_, AL_LOOPING, sound_->looped_);
    }
}

bool SoundSource::StreamOpenAL(float timeStep)
{
    int processed;
    bool active = true;

    alGetSourcei(alSource_, AL_BUFFERS_PROCESSED, &processed);

    while(processed--)
    {
        ALuint buffer;

        unsigned sampleSize = sound_->GetSampleSize();
        unsigned DecodeBufferSize = sampleSize * sound_->GetIntFrequency() * DECODE_BUFFER_LENGTH / 1000;
        
        alSourceUnqueueBuffers(alSource_, 1, &buffer);
        IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot unqueue buffer."));

        active = sound_->DecodeOpenAL(decoder_, buffer, DecodeBufferSize);

        if (!active && sound_->IsLooped())
        {
            sound_->RewindDecoder(decoder_);
            timePosition_ = 0.0f;
            position_ = 0;
            active = sound_->DecodeOpenAL(decoder_, buffer, DecodeBufferSize);
        }
        else
        	return false;

        alSourceQueueBuffers(alSource_, 1, &buffer);
        IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot queue buffer."));
    }

    return (bool)active;
}
#endif  // USE_OPENAL

void SoundSource::Update(float timeStep)
{
    if (!audio_ || !IsEnabledEffective())
        return;
#if defined(USE_OPENAL)
    UpdateOpenAL(timeStep);
#endif
    // If there is no actual audio output, perform fake mixing into a nonexistent buffer to check stopping/looping
    if (!audio_->IsInitialized())
        MixNull(timeStep);

    // Free the decoder if playback has stopped
#if !defined(USE_OPENAL)
    if (!position_ && decoder_)
        FreeDecoder();
#endif
    
    // Check for autoremove
    if (autoRemove_)
    {
        if (!IsPlaying())
        {
            autoRemoveTimer_ += timeStep;
            if (autoRemoveTimer_ > AUTOREMOVE_DELAY)
            {
                Remove();
                // Note: this object is now deleted, so only returning immediately is safe
                return;
            }
        }
        else
            autoRemoveTimer_ = 0.0f;
    }
}

#if !defined (USE_OPENAL)
void SoundSource::Mix(int* dest, unsigned samples, int mixRate, bool stereo, bool interpolation)
{
    if (!position_ || !sound_ || !IsEnabledEffective())
        return;

    if (sound_->IsCompressed())
    {
        if (decoder_)
        {
            // If Decoder already exists, decode new compressed audio
            bool eof = false;
            unsigned currentPos = position_ - decodeBuffer_->GetStart();
            if (currentPos != decodePosition_)
            {
                // If buffer has wrapped, decode first to the end
                if (currentPos < decodePosition_)
                {
                    unsigned bytes = decodeBuffer_->GetDataSize() - decodePosition_;
                    unsigned outBytes = sound_->Decode(decoder_, decodeBuffer_->GetStart() + decodePosition_, bytes);
                    // If produced less output, end of sound encountered. Fill rest with zero
                    if (outBytes < bytes)
                    {
                        memset(decodeBuffer_->GetStart() + decodePosition_ + outBytes, 0, bytes - outBytes);
                        eof = true;
                    }
                    decodePosition_ = 0;
                }
                if (currentPos > decodePosition_)
                {
                    unsigned bytes = currentPos - decodePosition_;
                    unsigned outBytes = sound_->Decode(decoder_, decodeBuffer_->GetStart() + decodePosition_, bytes);
                    // If produced less output, end of sound encountered. Fill rest with zero
                    if (outBytes < bytes)
                    {
                        memset(decodeBuffer_->GetStart() + decodePosition_ + outBytes, 0, bytes - outBytes);
                        if (sound_->IsLooped())
                            eof = true;
                    }

                    // If wrote to buffer start, correct interpolation wraparound
                    if (!decodePosition_)
                        decodeBuffer_->FixInterpolation();
                }
            }

            // If end of stream encountered, check whether we should rewind or stop
            if (eof)
            {
                if (sound_->IsLooped())
                {
                    sound_->RewindDecoder(decoder_);
                    timePosition_ = 0.0f;
                }
                else
                    decodeBuffer_->SetLooped(false); // Stop after the current decode buffer has been played
            }

            decodePosition_ = currentPos;
        }
        else
        {
            // Setup the decoder and decode buffer
            decoder_ = sound_->AllocateDecoder();
            unsigned sampleSize = sound_->GetSampleSize();
            unsigned DecodeBufferSize = sampleSize * sound_->GetIntFrequency() * DECODE_BUFFER_LENGTH / 1000;
            decodeBuffer_ = new Sound(context_);
            decodeBuffer_->SetSize(DecodeBufferSize);
            decodeBuffer_->SetFormat(sound_->GetIntFrequency(), true, sound_->IsStereo());

            // Clear the decode buffer, then fill with initial audio data and set it to loop
            memset(decodeBuffer_->GetStart(), 0, DecodeBufferSize);
            sound_->Decode(decoder_, decodeBuffer_->GetStart(), DecodeBufferSize);
            decodeBuffer_->SetLooped(true);
            decodePosition_ = 0;

            // Start playing the decode buffer
            position_ = decodeBuffer_->GetStart();
            fractPosition_ = 0;
        }
    }

    // If compressed, play the decode buffer. Otherwise play the original sound
    Sound* sound = sound_->IsCompressed() ? decodeBuffer_ : sound_;
    if (!sound)
        return;

    // Choose the correct mixing routine
    if (!sound->IsStereo())
    {
        if (interpolation)
        {
            if (stereo)
                MixMonoToStereoIP(sound, dest, samples, mixRate);
            else
                MixMonoToMonoIP(sound, dest, samples, mixRate);
        }
        else
        {
            if (stereo)
                MixMonoToStereo(sound, dest, samples, mixRate);
            else
                MixMonoToMono(sound, dest, samples, mixRate);
        }
    }
    else
    {
        if (interpolation)
        {
            if (stereo)
                MixStereoToStereoIP(sound, dest, samples, mixRate);
            else
                MixStereoToMonoIP(sound, dest, samples, mixRate);
        }
        else
        {
            if (stereo)
                MixStereoToStereo(sound, dest, samples, mixRate);
            else
                MixStereoToMono(sound, dest, samples, mixRate);
        }
    }

    // Update the time position
    if (!sound_->IsCompressed())
        timePosition_ = ((float)(int)(size_t)(position_ - sound_->GetStart())) / (sound_->GetSampleSize() * sound_->GetFrequency());
    else
        timePosition_ += ((float)samples / (float)mixRate) * frequency_ / sound_->GetFrequency();
}
#endif // !defined (USE_OPENAL)

void SoundSource::SetSoundAttr(ResourceRef value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Sound* newSound = cache->GetResource<Sound>(value.name_);
    if (IsPlaying())
        Play(newSound);
    else
    {
        // When changing the sound and not playing, make sure the old decoder (if any) is freed
        FreeDecoder();
        sound_ = newSound;
    }
}

void SoundSource::SetPlayingAttr(bool value)
{
    if (value)
    {
        if (!IsPlaying())
            Play(sound_);
    }
    else
        Stop();
}

void SoundSource::SetPositionAttr(int value)
{
    if (sound_)
    {
        #if defined(USE_OPENAL)
        SetPlayPositionOpenAL(value);
        #else
        SetPlayPosition(sound_->GetStart() + value);
        #endif
    }
}

ResourceRef SoundSource::GetSoundAttr() const
{
    return GetResourceRef(sound_, Sound::GetTypeStatic());
}

int SoundSource::GetPositionAttr() const
{
    if (sound_ && position_)
        #if defined(USE_OPENAL)
        return GetPlayPosition();
        #else
        return (int)(GetPlayPosition() - sound_->GetStart());
        #endif
    else
        return 0;
}
#if !defined(USE_OPENAL)
void SoundSource::MixMonoToMono(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int vol = (int)(256.0f * totalGain + 0.5f);
    if (!vol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + (*pos * vol) / 256;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + (*pos * vol) / 256;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + *pos * vol;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + *pos * vol;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixMonoToStereo(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int leftVol = (int)((-panning_ + 1.0f) * (256.0f * totalGain + 0.5f));
    int rightVol = (int)((panning_ + 1.0f) * (256.0f * totalGain + 0.5f));
    if (!leftVol && !rightVol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + (*pos * leftVol) / 256;
                ++dest;
                *dest = *dest + (*pos * rightVol) / 256;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + (*pos * leftVol) / 256;
                ++dest;
                *dest = *dest + (*pos * rightVol) / 256;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + *pos * leftVol;
                ++dest;
                *dest = *dest + *pos * rightVol;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + *pos * leftVol;
                ++dest;
                *dest = *dest + *pos * rightVol;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixMonoToMonoIP(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int vol = (int)(256.0f * totalGain + 0.5f);
    if (!vol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + (GET_IP_SAMPLE() * vol) / 256;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + (GET_IP_SAMPLE() * vol) / 256;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + GET_IP_SAMPLE() * vol;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + GET_IP_SAMPLE() * vol;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixMonoToStereoIP(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int leftVol = (int)((-panning_ + 1.0f) * (256.0f * totalGain + 0.5f));
    int rightVol = (int)((panning_ + 1.0f) * (256.0f * totalGain + 0.5f));
    if (!leftVol && !rightVol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                int s = GET_IP_SAMPLE();
                *dest = *dest + (s * leftVol) / 256;
                ++dest;
                *dest = *dest + (s * rightVol) / 256;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                int s = GET_IP_SAMPLE();
                *dest = *dest + (s * leftVol) / 256;
                ++dest;
                *dest = *dest + (s * rightVol) / 256;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                int s = GET_IP_SAMPLE();
                *dest = *dest + s * leftVol;
                ++dest;
                *dest = *dest + s * rightVol;
                ++dest;
                INC_POS_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                int s = GET_IP_SAMPLE();
                *dest = *dest + s * leftVol;
                ++dest;
                *dest = *dest + s * rightVol;
                ++dest;
                INC_POS_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixStereoToMono(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int vol = (int)(256.0f * totalGain + 0.5f);
    if (!vol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                int s = ((int)pos[0] + (int)pos[1]) / 2;
                *dest = *dest + (s * vol) / 256;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                int s = ((int)pos[0] + (int)pos[1]) / 2;
                *dest = *dest + (s * vol) / 256;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                int s = ((int)pos[0] + (int)pos[1]) / 2;
                *dest = *dest + s * vol;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                int s = ((int)pos[0] + (int)pos[1]) / 2;
                *dest = *dest + s * vol;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixStereoToStereo(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int vol = (int)(256.0f * totalGain + 0.5f);
    if (!vol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + (pos[0] * vol) / 256;
                ++dest;
                *dest = *dest + (pos[1] * vol) / 256;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + (pos[0] * vol) / 256;
                ++dest;
                *dest = *dest + (pos[1] * vol) / 256;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + pos[0] * vol;
                ++dest;
                *dest = *dest + pos[1] * vol;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + pos[0] * vol;
                ++dest;
                *dest = *dest + pos[1] * vol;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixStereoToMonoIP(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int vol = (int)(256.0f * totalGain + 0.5f);
    if (!vol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                int s = (GET_IP_SAMPLE_LEFT() + GET_IP_SAMPLE_RIGHT()) / 2;
                *dest = *dest + (s * vol) / 256;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                int s = (GET_IP_SAMPLE_LEFT() + GET_IP_SAMPLE_RIGHT()) / 2;
                *dest = *dest + (s * vol) / 256;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                int s = (GET_IP_SAMPLE_LEFT() + GET_IP_SAMPLE_RIGHT()) / 2;
                *dest = *dest + s * vol;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                int s = (GET_IP_SAMPLE_LEFT() + GET_IP_SAMPLE_RIGHT()) / 2;
                *dest = *dest + s * vol;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixStereoToStereoIP(Sound* sound, int* dest, unsigned samples, int mixRate)
{
    float totalGain = audio_->GetSoundSourceMasterGain(soundType_) * attenuation_ * gain_;
    int vol = (int)(256.0f * totalGain + 0.5f);
    if (!vol)
    {
        MixZeroVolume(sound, samples, mixRate);
        return;
    }

    float add = frequency_ / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    int fractPos = fractPosition_;

    if (sound->IsSixteenBit())
    {
        short* pos = (short*)position_;
        short* end = (short*)sound->GetEnd();
        short* repeat = (short*)sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + (GET_IP_SAMPLE_LEFT() * vol) / 256;
                ++dest;
                *dest = *dest + (GET_IP_SAMPLE_RIGHT() * vol) / 256;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = (signed char*)pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + (GET_IP_SAMPLE_LEFT() * vol) / 256;
                ++dest;
                *dest = *dest + (GET_IP_SAMPLE_RIGHT() * vol) / 256;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = (signed char*)pos;
        }
    }
    else
    {
        signed char* pos = (signed char*)position_;
        signed char* end = sound->GetEnd();
        signed char* repeat = sound->GetRepeat();

        if (sound->IsLooped())
        {
            while (--samples)
            {
                *dest = *dest + GET_IP_SAMPLE_LEFT() * vol;
                ++dest;
                *dest = *dest + GET_IP_SAMPLE_RIGHT() * vol;
                ++dest;
                INC_POS_STEREO_LOOPED();
            }
            position_ = pos;
        }
        else
        {
            while (--samples)
            {
                *dest = *dest + GET_IP_SAMPLE_LEFT() * vol;
                ++dest;
                *dest = *dest + GET_IP_SAMPLE_RIGHT() * vol;
                ++dest;
                INC_POS_STEREO_ONESHOT();
            }
            position_ = pos;
        }
    }

    fractPosition_ = fractPos;
}

void SoundSource::MixZeroVolume(Sound* sound, unsigned samples, int mixRate)
{
    float add = frequency_ * (float)samples / (float)mixRate;
    int intAdd = (int)add;
    int fractAdd = (int)((add - floorf(add)) * 65536.0f);
    unsigned sampleSize = sound->GetSampleSize();

    fractPosition_ += fractAdd;
    if (fractPosition_ > 65535)
    {
        fractPosition_ &= 65535;
        position_ += sampleSize;
    }
    position_ += intAdd * sampleSize;

    if (position_ > sound->GetEnd())
    {
        if (sound->IsLooped())
        {
            while (position_ >= sound->GetEnd())
            {
                position_ -= (sound->GetEnd() - sound->GetRepeat());
            }
        }
        else
            position_ = 0;
    }
}
#endif // !defined(USE_OPENAL)

void SoundSource::MixNull(float timeStep)
{
    if (!position_ || !sound_ || !IsEnabledEffective())
        return;

    // Advance only the time position
    timePosition_ += timeStep * frequency_ / sound_->GetFrequency();

    if (sound_->IsLooped())
    {
        // For simulated playback, simply reset the time position to zero when the sound loops
        if (timePosition_ >= sound_->GetLength())
            timePosition_ -= sound_->GetLength();
    }
    else
    {
        if (timePosition_ >= sound_->GetLength())
        {
            position_ = 0;
            timePosition_ = 0.0f;
        }
    }
}

void SoundSource::FreeDecoder()
{
    if (sound_ && decoder_)
    {
        sound_->FreeDecoder(decoder_);
        decoder_ = 0;
    }

    #if defined(USE_OPENAL)
    if(sound_)
    {
        if (sound_->IsCompressed())
        {
            alSourceUnqueueBuffers(alSource_, 2, sound_->GetALBufferPointer());
            IF_AL_ERROR(LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot unqueue buffer."));
        }
    }
    else
        alSourcei(alSource_, AL_BUFFER, 0);
    #else
    decodeBuffer_.Reset();
    #endif
}

}
