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

#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/Image.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <Urho3D/DebugNew.h>

using namespace Urho3D;

int main(int argc, char** argv);
void Run(const Vector<String>& arguments);

void Help()
{
    ErrorExit("Usage: TextureCompressor <input file> <output file> [options]\n"
        "\n"
        "Options:\n"
        "-h Shows this help message.\n"
        "-f Compression format BC1 (DXT1), BC3 (DXT5), ETC1, ASTC. Default is DXT1\n"
        "-b Block size NxN (ASTC only). Default is 4x4\n");
}

int main(int argc, char** argv)
{
    Vector<String> arguments;

    #ifdef WIN32
    arguments = ParseArguments(GetCommandLineW());
    #else
    arguments = ParseArguments(argc, argv);
    #endif

    Run(arguments);
    return 0;
}

void Run(const Vector<String>& arguments)
{
    if (arguments.Size() < 2)
        Help();

    SharedPtr<Context> context(new Context());
    context->RegisterSubsystem(new FileSystem(context));
    context->RegisterSubsystem(new Log(context));
    //FileSystem* fileSystem = context->GetSubsystem<FileSystem>();

    const String& input = arguments[0];
    const String& output = arguments[1];
    String format;
    String blockSize;
    CompressedFormat compressedFormat = CF_DXT1;
    bool debug = false;

    if (arguments.Size() > 2)
    {
        for (unsigned i = 2; i < arguments.Size(); ++i)
        {
            if (arguments[i][0] == '-')
            {
                String argument = arguments[i].Substring(1).ToLower();
                String value = i + 1 < arguments.Size() ? arguments[i + 1] : String::EMPTY;

                if (argument == "h")
                    Help();
                else if (argument == "f" && !value.Empty())
                {
                    format = value;
                    ++i;
                }
                else if (argument == "b" && !value.Empty())
                {
                    blockSize = value;
                    ++i;
                }
            }
        }
    }

    //String outputExt = GetExtension(output);

    if (format.Empty()) { compressedFormat = CF_DXT1; }
    else if (format == "dxt1") { compressedFormat = CF_DXT1; }
    else if (format == "bc1") { compressedFormat = CF_DXT1; }
    else if (format == "dxt5") { compressedFormat = CF_DXT5; }
    else if (format == "bc3") { compressedFormat = CF_DXT5; }
    else if (format == "etc1") { compressedFormat = CF_ETC1; }
    else if (format == "astc")
    {
        if (!blockSize.Empty())
        {
            if (blockSize == "4x4") { compressedFormat = CF_ASTC_RGBA_4x4; }
            else if (blockSize == "5x4") { compressedFormat = CF_ASTC_RGBA_5x4; }
            else if (blockSize == "5x5") { compressedFormat = CF_ASTC_RGBA_5x5; }
            else if (blockSize == "6x5") { compressedFormat = CF_ASTC_RGBA_6x5; }
            else if (blockSize == "6x6") { compressedFormat = CF_ASTC_RGBA_6x6; }
            else if (blockSize == "8x5") { compressedFormat = CF_ASTC_RGBA_8x5; }
            else if (blockSize == "8x6") { compressedFormat = CF_ASTC_RGBA_8x6; }
            else if (blockSize == "8x8") { compressedFormat = CF_ASTC_RGBA_8x8; }
        }
        else
            { compressedFormat = CF_ASTC_RGBA_4x4; }
    }
    else
        ErrorExit("Format not supported.");

    File inputFile(context, input);
    File outputFile(context, output, FILE_WRITE);
    Image inputImage(context);
    if (inputImage.BeginLoad(inputFile))
    {
        SharedPtr<Image> compressedImage = inputImage.ConvertToCompressedFormat(compressedFormat);
        URHO3D_LOGINFO("Saving output image.");
        bool success = compressedImage->Save(outputFile);
        if (!success)
            ErrorExit("Failed to write file.");
    }
}
