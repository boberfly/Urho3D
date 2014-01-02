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
#include "Context.h"
#include "FileSystem.h"
#include "Log.h"
#include "Profiler.h"
#include "ResourceCache.h"
#include "Sound.h"
#include "XMLFile.h"

#include <cstring>
#include <stb_vorbis.h>

#include "DebugNew.h"

namespace Urho3D
{

/// WAV format header.
struct WavHeader
{
    unsigned char riffText_[4];
    unsigned totalLength_;
    unsigned char waveText_[4];
    unsigned char formatText_[4];
    unsigned formatLength_;
    unsigned short format_;
    unsigned short channels_;
    unsigned frequency_;
    unsigned avgBytes_;
    unsigned short blockAlign_;
    unsigned short bits_;
    unsigned char dataText_[4];
    unsigned dataLength_;
};

static const unsigned IP_SAFETY = 4;

OBJECTTYPESTATIC(Sound);

Sound::Sound(Context* context) :
    Resource(context),
    repeat_(0),
    end_(0),
    dataSize_(0),
    frequency_(44100),
    looped_(false),
    sixteenBit_(false),
    stereo_(false),
    compressed_(false),
#if defined(USE_OPENAL)
    samplesLength_(0),
    samplesRemaining_(0),
#endif
    compressedLength_(0.0f)
{
    #if defined(USE_OPENAL)
	//alGenBuffers( 1, &alBuffer_ ); // deferred, we might need 2 buffers
    //audio_ = GetSubsystem<Audio>();
    #endif
}

Sound::~Sound()
{
    #if defined(USE_OPENAL)
    if(alBuffer_)
    {
	    alDeleteBuffers( compressed_ ? 2 : 1, &alBuffer_ );
        //if(audio_)
        //{
        //    if (!audio_->checkALCError())
        //        LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot delete buffers for "+GetName());
        //}
    }
    #endif
}

void Sound::RegisterObject(Context* context)
{
    context->RegisterFactory<Sound>();
}

bool Sound::Load(Deserializer& source)
{
    PROFILE(LoadSound);
    
    bool success = false;
    if (GetExtension(source.GetName()) == ".ogg")
        success = LoadOggVorbis(source);
    else if (GetExtension(source.GetName()) == ".wav")
        success = LoadWav(source);
    else
        success = LoadRaw(source);
    
    // Load optional parameters
    if (success)
        LoadParameters();
    
    return success;
}

bool Sound::LoadOggVorbis(Deserializer& source)
{
    unsigned dataSize = source.GetSize();
    SharedArrayPtr<signed char> data(new signed char[dataSize]);
    source.Read(data.Get(), dataSize);
    
    // Check for validity of data
    int error;
    stb_vorbis* vorbis = stb_vorbis_open_memory((unsigned char*)data.Get(), dataSize, &error, 0);
    if (!vorbis)
    {
        LOGERROR("Could not read Ogg Vorbis data from " + source.GetName());
        return false;
    }
    
    // Store length, frequency and stereo flag
    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    compressedLength_ = stb_vorbis_stream_length_in_seconds(vorbis);
    #if defined(USE_OPENAL)
    samplesLength_ = stb_vorbis_stream_length_in_samples(vorbis) * info.channels;
    samplesRemaining_ = samplesLength_; // For OpenAL streaming
    #endif
    frequency_ = info.sample_rate;
    stereo_ = info.channels > 1;
    stb_vorbis_close(vorbis);
    
    data_ = data;
    dataSize_ = dataSize;
    sixteenBit_ = true;
    compressed_ = true;

    #if defined(USE_OPENAL)
    alGenBuffers(2, &alBuffer_); // 2 = front and back-buffer as we will stream

    //if (audio_)
    //{
    //    if (!audio_->checkALCError())
    //        LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot allocate buffers for "+GetName()+" from sound file "+source.GetName());
    //}
    #endif
    
    SetMemoryUse(dataSize);
    return true;
}

bool Sound::LoadWav(Deserializer& source)
{
    WavHeader header;
    
    // Try to open
    memset(&header, 0, sizeof header);
    source.Read(&header.riffText_, 4);
    header.totalLength_ = source.ReadUInt();
    source.Read(&header.waveText_, 4);
    
    if (memcmp("RIFF", header.riffText_, 4) || memcmp("WAVE", header.waveText_, 4))
    {
        LOGERROR("Could not read WAV data from " + source.GetName());
        return false;
    }
    
    // Search for the FORMAT chunk
    for (;;)
    {
        source.Read(&header.formatText_, 4);
        header.formatLength_ = source.ReadUInt();
        if (!memcmp("fmt ", &header.formatText_, 4))
            break;
        
        source.Seek(source.GetPosition() + header.formatLength_);
        if (!header.formatLength_ || source.GetPosition() >= source.GetSize())
        {
            LOGERROR("Could not read WAV data from " + source.GetName());
            return false;
        }
    }
    
    // Read the FORMAT chunk
    header.format_ = source.ReadUShort();
    header.channels_ = source.ReadUShort();
    header.frequency_ = source.ReadUInt();
    header.avgBytes_ = source.ReadUInt();
    header.blockAlign_ = source.ReadUShort();
    header.bits_ = source.ReadUShort();
    
    // Skip data if the format chunk was bigger than what we use
    source.Seek(source.GetPosition() + header.formatLength_ - 16);
    
    // Check for correct format
    if (header.format_ != 1)
    {
        LOGERROR("Could not read WAV data from " + source.GetName());
        return false;
    }
    
    // Search for the DATA chunk
    for (;;)
    {
        source.Read(&header.dataText_, 4);
        header.dataLength_ = source.ReadUInt();
        if (!memcmp("data", &header.dataText_, 4))
            break;
        
        source.Seek(source.GetPosition() + header.dataLength_);
        if (!header.dataLength_ || source.GetPosition() >= source.GetSize())
        {
            LOGERROR("Could not read WAV data from " + source.GetName());
            return false;
        }
    }
    
    // Allocate sound and load audio data
    unsigned length = header.dataLength_;
    SetSize(length);
    SetFormat(header.frequency_, header.bits_ == 16, header.channels_ == 2);
    source.Read(data_.Get(), length);
    
    // Convert 8-bit audio to signed
    if (!sixteenBit_)
    {
        for (unsigned i = 0; i < length; ++i)
            data_[i] -= 128;
    }

    #if defined(USE_OPENAL)
    ALenum format = 0;

    if( header.bits_ == 8 )
	{
        switch( header.channels_ )
        {
        case 1:
            format = AL_FORMAT_MONO8;
            break;
        case 2:
            format = AL_FORMAT_STEREO8;
            break;
        case 4:
            format = alGetEnumValue( "AL_FORMAT_QUAD8" );
            break;
        case 6:
            format = alGetEnumValue( "AL_FORMAT_51CHN8" );
            break;
        case 7:
            format = alGetEnumValue( "AL_FORMAT_61CHN8" );
            break;
        case 8:
            format = alGetEnumValue( "AL_FORMAT_71CHN8" );
            break;
        }
    }
    else if( header.bits_ == 16 )
    {
        switch( header.channels_ )
        {
        case 1:
            format = AL_FORMAT_MONO16;
            break;
        case 2:
            format = AL_FORMAT_STEREO16;
            break;
        case 4:
            format = alGetEnumValue( "AL_FORMAT_QUAD16" );
            break;
        case 6:
            format = alGetEnumValue( "AL_FORMAT_51CHN16" );
            break;
        case 7:
            format = alGetEnumValue( "AL_FORMAT_61CHN16" );
            break;
        case 8:
            format = alGetEnumValue( "AL_FORMAT_71CHN16" );
            break;
        }
    }
    alGenBuffers( 1, &alBuffer_ );
    /*if (audio_)
    {
        if (!audio_->checkALCError())
            LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot allocate buffers for "+GetName()+" from sound file "+source.GetName());
    }*/
    alBufferData(alBuffer_, format, data_.Get(), (ALsizei)length, (ALsizei)header.frequency_);

    /*if (audio_)
    {
        if (!audio_->checkALCError())
            LOGERROR("OpenAL Error: "+audio_->GetErrorAL()+", cannot upload buffer for "+GetName()+" from sound file "+source.GetName());
    }*/
    data_.Reset();
    #endif

    return true;
}

bool Sound::LoadRaw(Deserializer& source)
{
    unsigned dataSize = source.GetSize();
    SetSize(dataSize);
    return source.Read(data_.Get(), dataSize) == dataSize;
}

void Sound::SetSize(unsigned dataSize)
{
    if (!dataSize)
        return;
    
    data_ = new signed char[dataSize + IP_SAFETY];
    dataSize_ = dataSize;
    compressed_ = false;
    SetLooped(false);
    
    SetMemoryUse(dataSize + IP_SAFETY);
}

void Sound::SetData(const void* data, unsigned dataSize)
{
    if (!dataSize)
        return;
    
    SetSize(dataSize);
    memcpy(data_.Get(), data, dataSize);
}

void Sound::UploadBufferToAL(const void* data, unsigned dataSize)
{
    if (!dataSize)
        return;
    if (alBuffer_)
    {
        alDeleteBuffers(1, &alBuffer_);
        if (alGetError() != AL_NO_ERROR)
            LOGERROR("ERROR DELETING BUFFER.");
    }
    alGenBuffers(1, &alBuffer_);
    if (alGetError() != AL_NO_ERROR)
        LOGERROR("ERROR GENNING BUFFER.");
    alBufferData(alBuffer_, stereo_ ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, data, (ALsizei)dataSize, (ALsizei)frequency_);
    if (alGetError() != AL_NO_ERROR)
        LOGERROR("ERROR UPLOADING BUFFER.");
}

void Sound::SetFormat(unsigned frequency, bool sixteenBit, bool stereo)
{
    frequency_ = frequency;
    sixteenBit_ = sixteenBit;
    stereo_ = stereo;
    compressed_ = false;
}

void Sound::SetLooped(bool enable)
{
#if defined(USE_OPENAL)
    looped_ = enable;
#else
    if (enable)
        SetLoop(0, dataSize_);
    else
    {
        if (!compressed_)
        {
            end_ = data_.Get() + dataSize_;
            looped_ = false;
            
            FixInterpolation();
        }
        else
            looped_ = false;
    }
#endif
}

void Sound::SetLoop(unsigned repeatOffset, unsigned endOffset)
{
#if !defined(USE_OPENAL)
    if (!compressed_)
    {
        if (repeatOffset > dataSize_)
            repeatOffset = dataSize_;
        if (endOffset > dataSize_)
            endOffset = dataSize_;
        
        // Align repeat and end on sample boundaries
        int sampleSize = GetSampleSize();
        repeatOffset &= -sampleSize;
        endOffset &= -sampleSize;
        
        repeat_ = data_.Get() + repeatOffset;
        end_ = data_.Get() + endOffset;
        looped_ = true;
        
        FixInterpolation();
    }
    else
#endif
        looped_ = true;
}

#if !defined(USE_OPENAL)
void Sound::FixInterpolation()
{
    if (!data_)
        return;
    
    // If looped, copy loop start to loop end. If oneshot, insert silence to end
    if (looped_)
    {
        for (unsigned i = 0; i < IP_SAFETY; ++i)
            end_[i] = repeat_[i];
    }
    else
    {
        for (unsigned i = 0; i < IP_SAFETY; ++i)
            end_[i] = 0;
    }
}
#endif

void* Sound::AllocateDecoder()
{
    if (!compressed_)
        return 0;
    
    int error;
    stb_vorbis* vorbis = stb_vorbis_open_memory((unsigned char*)data_.Get(), dataSize_, &error, 0);
    return vorbis;
}

#if defined(USE_OPENAL)
unsigned Sound::DecodeOpenAL(void* decoder, ALuint buffer, unsigned bytes)
{
    if (!decoder)
        return 0;

    ALenum format = 0;
    format = stereo_ ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    ALshort dest[bytes];
    int  size = 0;
    int  result = 0;
    unsigned soundSources = stereo_ ? 2 : 1;

    stb_vorbis* vorbis = static_cast<stb_vorbis*>(decoder);

    while(size < bytes)
    {
        unsigned outSamples = stb_vorbis_get_samples_short_interleaved(vorbis, soundSources, dest + size, bytes - size);
        if(outSamples > 0)
            size += outSamples * soundSources;
        else
            break;
    }

    if(size == 0)
        return 0;

    alBufferData(buffer, format, dest, size*sizeof(ALshort), (ALsizei)GetIntFrequency());

    //if(audio_)
    //{
    //    if (!audio_->checkALError())
    //        LOGERROR("OpenAL Error (SoundClass): "+audio_->GetErrorAL()+", cannot put streamed data in "+GetName());
    //}

    return samplesRemaining_ -= size;
}
#else
unsigned Sound::Decode(void* decoder, signed char* dest, unsigned bytes)
{
    if (!decoder)
        return 0;
    
    unsigned soundSources = stereo_ ? 2 : 1;
    stb_vorbis* vorbis = static_cast<stb_vorbis*>(decoder);
    unsigned outSamples = stb_vorbis_get_samples_short_interleaved(vorbis, soundSources, (short*)dest, bytes >> 1);
    return (outSamples * soundSources) << 1;
}
#endif

void Sound::RewindDecoder(void* decoder)
{
    if (!decoder)
        return;
    
    stb_vorbis* vorbis = static_cast<stb_vorbis*>(decoder);
    stb_vorbis_seek_start(vorbis);
    #if defined(USE_OPENAL)
    samplesRemaining_ = samplesLength_;
    #endif
}

void Sound::FreeDecoder(void* decoder)
{
    if (!decoder)
        return;
    
    stb_vorbis* vorbis = static_cast<stb_vorbis*>(decoder);
    stb_vorbis_close(vorbis);
}

float Sound::GetLength() const
{
    if (!compressed_)
    {
        if (!frequency_)
            return 0.0f;
        else
            return ((float)dataSize_) / GetSampleSize() / frequency_;
    }
    else
        return compressedLength_;
}

unsigned Sound::GetSampleSize() const
{
    unsigned size = 1;
    if (sixteenBit_)
        size <<= 1;
    if (stereo_)
        size <<= 1;
    return size;
}

void Sound::LoadParameters()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    String xmlName = ReplaceExtension(GetName(), ".xml");
    
    if (!cache->Exists(xmlName))
        return;
    
    XMLFile* file = cache->GetResource<XMLFile>(xmlName);
    if (!file)
        return;
    
    XMLElement rootElem = file->GetRoot();
    XMLElement paramElem = rootElem.GetChild();
    
    while (paramElem)
    {
        String name = paramElem.GetName();
        
        if (name == "format" && !compressed_)
        {
            if (paramElem.HasAttribute("frequency"))
                frequency_ = paramElem.GetInt("frequency");
            if (paramElem.HasAttribute("sixteenbit"))
                sixteenBit_ = paramElem.GetBool("sixteenbit");
            if (paramElem.HasAttribute("16bit"))
                sixteenBit_ = paramElem.GetBool("16bit");
            if (paramElem.HasAttribute("stereo"))
                stereo_ = paramElem.GetBool("stereo");
        }
        
        if (name == "loop")
        {
            if (paramElem.HasAttribute("enable"))
                SetLooped(paramElem.GetBool("enable"));
            if (paramElem.HasAttribute("start") && paramElem.HasAttribute("end"))
                SetLoop(paramElem.GetInt("start"), paramElem.GetInt("end"));
        }
        
        paramElem = paramElem.GetNext();
    }
}

}
