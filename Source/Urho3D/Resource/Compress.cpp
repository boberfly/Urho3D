//
// Copyright (c) 2008-2015 the Urho3D project.
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

#ifdef URHO3D_ISPC_TEXCOMP

#include "../Precompiled.h"

#include "../Resource/Compress.h"

#include <ISPCTexcomp/ispc_texcomp.h>


namespace Urho3D
{

void CompressImageDXT(void* dest, const void* src, int width, int height, int stride, CompressedFormat format)
{
    rgba_surface surf;
    memset(&surf, 0, sizeof surf);
    surf.ptr = (uint8_t*)src;
    surf.width = width;
    surf.height = height;
    surf.stride = stride;

    switch(format)
    {
        case CF_DXT1:
            CompressBlocksBC1(&surf, (uint8_t*)dest);
            break;

        case CF_DXT5:
            CompressBlocksBC3(&surf, (uint8_t*)dest);
            break;

        default:
            break;
    }
}

void CompressImageETC(void* dest, const void* src, int width, int height, int stride)
{
    rgba_surface surf;
    memset(&surf, 0, sizeof surf);
    surf.ptr = (uint8_t*)src;
    surf.width = width;
    surf.height = height;
    surf.stride = stride;

    etc_enc_settings settings;
    memset(&settings, 0, sizeof settings);
    settings.fastSkipTreshold = 6;

    CompressBlocksETC1(&surf, (uint8_t*)dest, &settings);
}

void CompressImageASTC(void* dest, const void* src, int width, int height, int stride, CompressedFormat format)
{
    rgba_surface surf;
    memset(&surf, 0, sizeof surf);
    surf.ptr = (uint8_t*)src;
    surf.width = width;
    surf.height = height;
    surf.stride = stride;

    astc_enc_settings settings;
    memset(&settings, 0, sizeof settings);

    switch(format)
    {
        case CF_ASTC_RGBA_4x4:
            settings.block_width = 4;
            settings.block_height = 4;
            break;

        case CF_ASTC_RGBA_5x4:
            settings.block_width = 5;
            settings.block_height = 4;
            break;

        case CF_ASTC_RGBA_5x5:
            settings.block_width = 5;
            settings.block_height = 5;
            break;

        case CF_ASTC_RGBA_6x5:
            settings.block_width = 6;
            settings.block_height = 5;
            break;

        case CF_ASTC_RGBA_6x6:
            settings.block_width = 6;
            settings.block_height = 6;
            break;

        case CF_ASTC_RGBA_8x5:
            settings.block_width = 8;
            settings.block_height = 5;
            break;

        case CF_ASTC_RGBA_8x6:
            settings.block_width = 8;
            settings.block_height = 6;
            break;

        case CF_ASTC_RGBA_8x8:
            settings.block_width = 8;
            settings.block_height = 8;
            break;

        default:
            settings.block_width = 4;
            settings.block_height = 4;
            break;
    }
    settings.fastSkipTreshold = 5;
    settings.refineIterations = 2;

    CompressBlocksASTC(&surf, (uint8_t*)dest, &settings);
}

}

#endif
