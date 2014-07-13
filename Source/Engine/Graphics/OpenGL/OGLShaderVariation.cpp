//
// Copyright (c) 2008-2014 the Urho3D project.
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
#include "Graphics.h"
#include "GraphicsImpl.h"
#include "Log.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include "ShaderVariation.h"

#include "DebugNew.h"

namespace Urho3D
{

ShaderVariation::ShaderVariation(Shader* owner, ShaderType type) :
    GPUObject(owner->GetSubsystem<Graphics>()),
    owner_(owner),
    type_(type)
{
}

ShaderVariation::~ShaderVariation()
{
    Release();
}

void ShaderVariation::OnDeviceLost()
{
    GPUObject::OnDeviceLost();

    compilerOutput_.Clear();
    
    if (graphics_)
        graphics_->CleanupShaderPrograms();
}

void ShaderVariation::Release()
{
    if (object_)
    {
        if (!graphics_)
            return;
        
        if (!graphics_->IsDeviceLost())
        {
            /*
            if (type_ == VS)
            {
                if (graphics_->GetVertexShader() == this)
                    graphics_->SetShaders(0, 0);
            }
            else
            {
                if (graphics_->GetPixelShader() == this)
                    graphics_->SetShaders(0, 0);
            }
            */
            switch (type_)
            {
                case VS:
                {
                    if(graphics_->GetVertexShader() == this)
                        graphics_->SetShaders(0, 0, 0, 0, 0, 0);
                    break;
                }
                case HS:
                {
                    if(graphics_->GetHullShader() == this)
                        graphics_->SetShaders(0, 0, 0, 0, 0, 0);
                    break;
                }
                case DS:
                {
                    if(graphics_->GetDomainShader() == this)
                        graphics_->SetShaders(0, 0, 0, 0, 0, 0);
                    break;
                }
                case GS:
                {
                    if(graphics_->GetGeometryShader() == this)
                        graphics_->SetShaders(0, 0, 0, 0, 0, 0);
                    break;
                }
                case PS:
                {
                    if(graphics_->GetPixelShader() == this)
                        graphics_->SetShaders(0, 0, 0, 0, 0, 0);
                    break;
                }
                case CS:
                {
                    if(graphics_->GetComputeShader() == this)
                        graphics_->SetShaders(0, 0, 0, 0, 0, 0);
                    break;
                }
                default:
                    break;
            }
            
            glDeleteShader(object_);
        }
        
        object_ = 0;
        graphics_->CleanupShaderPrograms();
    }
    
    compilerOutput_.Clear();
}

bool ShaderVariation::Create()
{
    Release();

    if (!owner_)
    {
        compilerOutput_ = "Owner shader has expired";
        return false;
    }
    
//    object_ = glCreateShader(type_ == VS ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    switch (type_)
    {
        case VS:
        {
            object_ = glCreateShader(GL_VERTEX_SHADER);
            break;
        }
        case HS:
        {
            object_ = glCreateShader(GL_TESS_CONTROL_SHADER);
            break;
        }
        case DS:
        {
            object_ = glCreateShader(GL_TESS_EVALUATION_SHADER);
            break;
        }
        case GS:
        {
            object_ = glCreateShader(GL_GEOMETRY_SHADER);
            break;
        }
        case PS:
        {
            object_ = glCreateShader(GL_FRAGMENT_SHADER);
            break;
        }
        case CS:
        {
            object_ = glCreateShader(GL_COMPUTE_SHADER);
            break;
        }
        default:
            break;
    }

    if (!object_)
    {
        compilerOutput_ = "Could not create shader object";
        return false;
    }
    
    const String& originalShaderCode = owner_->GetSourceCode(type_);
    String shaderCode;

    // Check if the shader code contains a version define
    unsigned verStart = originalShaderCode.Find('#');
    unsigned verEnd = 0;
    if (verStart != String::NPOS)
    {
        if (originalShaderCode.Substring(verStart + 1, 7) == "version")
        {
            verEnd = verStart + 9;
            while (verEnd < originalShaderCode.Length())
            {
                if (IsDigit(originalShaderCode[verEnd]))
                    ++verEnd;
                else
                    break;
            }
            // If version define found, insert it first
            String versionDefine = originalShaderCode.Substring(verStart, verEnd - verStart);
            shaderCode += versionDefine + "\n";
        }
    }

    // Distinguish between shader type compile in case the shader code wants to include/omit different things
    //shaderCode += type_ == VS ? "#define COMPILEVS\n" : "#define COMPILEPS\n";
    switch (type_)
    {
        case VS:
        {
            shaderCode += "#define COMPILEVS\n";
            break;
        }
        case HS:
        {
            shaderCode += "#define COMPILEHS\n";
            break;
        }
        case DS:
        {
            shaderCode += "#define COMPILEDS\n";
            break;
        }
        case GS:
        {
            shaderCode += "#define COMPILEGS\n";
            break;
        }
        case PS:
        {
            shaderCode += "#define COMPILEPS\n";
            break;
        }
        case CS:
        {
            shaderCode += "#define COMPILECS\n";
            break;
        }
        default:
            break;
    }

    // Prepend the defines to the shader code
    Vector<String> defineVec = defines_.Split(' ');
    for (unsigned i = 0; i < defineVec.Size(); ++i)
    {
        // Add extra space for the checking code below
        String defineString = "#define " + defineVec[i].Replaced('=', ' ') + " \n";
        shaderCode += defineString;
        
        // In debug mode, check that all defines are referenced by the shader code
        #ifdef _DEBUG
        String defineCheck = defineString.Substring(8, defineString.Find(' ', 8) - 8);
        if (originalShaderCode.Find(defineCheck) == String::NPOS)
            LOGWARNING("Shader " + GetFullName() + " does not use the define " + defineCheck);
        #endif
    }
    
    #ifdef RASPI
    if (type_ == VS)
        shaderCode += "#define RASPI\n";
    #endif

    // When version define found, do not insert it a second time
    if (verEnd > 0)
        shaderCode += (originalShaderCode.CString() + verEnd);
    else
        shaderCode += originalShaderCode;
    
    const char* shaderCStr = shaderCode.CString();
    glShaderSource(object_, 1, &shaderCStr, 0);
    glCompileShader(object_);
    
    int compiled, length;
    glGetShaderiv(object_, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        glGetShaderiv(object_, GL_INFO_LOG_LENGTH, &length);
        compilerOutput_.Resize(length);
        int outLength;
        glGetShaderInfoLog(object_, length, &outLength, &compilerOutput_[0]);
        glDeleteShader(object_);
        object_ = 0;
    }
    else
        compilerOutput_.Clear();
    
    return object_ != 0;
}

void ShaderVariation::SetName(const String& name)
{
    name_ = name;
}

void ShaderVariation::SetDefines(const String& defines)
{
    defines_ = defines;
}

Shader* ShaderVariation::GetOwner() const
{
    return owner_;
}

}
