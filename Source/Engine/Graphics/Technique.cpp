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
#include "Context.h"
#include "Graphics.h"
#include "Log.h"
#include "Technique.h"
#include "Profiler.h"
#include "ResourceCache.h"
#include "ShaderVariation.h"
#include "XMLFile.h"

#include "DebugNew.h"

namespace Urho3D
{

const char* blendModeNames[] =
{
    "replace",
    "add",
    "multiply",
    "alpha",
    "addalpha",
    "premulalpha",
    "invdestalpha",
    "subtract",
    "subtractalpha",
    0
};

static const char* compareModeNames[] =
{
    "always",
    "equal",
    "notequal",
    "less",
    "lessequal",
    "greater",
    "greaterequal",
    0
};

static const char* lightingModeNames[] =
{
    "unlit",
    "pervertex",
    "perpixel",
    0
};

Pass::Pass(StringHash type) :
    type_(type),
    blendMode_(BLEND_REPLACE),
    depthTestMode_(CMP_LESSEQUAL),
    lightingMode_(LIGHTING_UNLIT),
    shadersLoadedFrameNumber_(0),
    depthWrite_(true),
    alphaMask_(false),
    usesCompute_(false),
//    isSM3_(false),
    shaderModel_(2)
{
    // Guess default lighting mode from pass name
    if (type == PASS_BASE || type == PASS_ALPHA || type == PASS_MATERIAL || type == PASS_DEFERRED)
        lightingMode_ = LIGHTING_PERVERTEX;
    else if (type == PASS_LIGHT || type == PASS_LITBASE || type == PASS_LITALPHA)
        lightingMode_ = LIGHTING_PERPIXEL;
}

Pass::~Pass()
{
}

void Pass::SetBlendMode(BlendMode mode)
{
    blendMode_ = mode;
}

void Pass::SetDepthTestMode(CompareMode mode)
{
    depthTestMode_ = mode;
}

void Pass::SetLightingMode(PassLightingMode mode)
{
    lightingMode_ = mode;
}

void Pass::SetDepthWrite(bool enable)
{
    depthWrite_ = enable;
}

void Pass::SetAlphaMask(bool enable)
{
    alphaMask_ = enable;
}

void Pass::SetUsesCompute(bool enable)
{
    usesCompute_ = enable;
}

//void Pass::SetIsSM3(bool enable)
//{
//    isSM3_ = enable;
//    if (enable)
//        shaderModel_ = 3;
//}

void Pass::SetShaderModel(unsigned value)
{
    shaderModel_ = value;
}

void Pass::SetVertexShader(const String& name)
{
    vertexShaderName_ = name;
    ReleaseShaders();
}

void Pass::SetHullShader(const String& name)
{
    hullShaderName_ = name;
    ReleaseShaders();
}

void Pass::SetDomainShader(const String& name)
{
    domainShaderName_ = name;
    ReleaseShaders();
}

void Pass::SetGeometryShader(const String& name)
{
    geometryShaderName_ = name;
    ReleaseShaders();
}

void Pass::SetPixelShader(const String& name)
{
    pixelShaderName_ = name;
    ReleaseShaders();
}

void Pass::SetComputeShader(const String& name)
{
    computeShaderName_ = name;
    ReleaseShaders();
}

void Pass::SetVertexShaderDefines(const String& defines)
{
    vertexShaderDefines_ = defines;
    ReleaseShaders();
}

void Pass::SetHullShaderDefines(const String& defines)
{
    hullShaderDefines_ = defines;
    ReleaseShaders();
}

void Pass::SetDomainShaderDefines(const String& defines)
{
    domainShaderDefines_ = defines;
    ReleaseShaders();
}

void Pass::SetGeometryShaderDefines(const String& defines)
{
    geometryShaderDefines_ = defines;
    ReleaseShaders();
}

void Pass::SetPixelShaderDefines(const String& defines)
{
    pixelShaderDefines_ = defines;
    ReleaseShaders();
}

void Pass::SetComputeShaderDefines(const String& defines)
{
    computeShaderDefines_ = defines;
    ReleaseShaders();
}

void Pass::ReleaseShaders()
{
    vertexShaders_.Clear();
    hullShaders_.Clear();
    domainShaders_.Clear();
    geometryShaders_.Clear();
    pixelShaders_.Clear();
    computeShaders_.Clear();
}

void Pass::MarkShadersLoaded(unsigned frameNumber)
{
    shadersLoadedFrameNumber_ = frameNumber;
}

Technique::Technique(Context* context) :
    Resource(context),
    //isSM3_(false)
    shaderModel_(2),
    usesCompute_(false)
{
    Graphics* graphics = GetSubsystem<Graphics>();
    //sm3Support_ = graphics ? graphics->GetSM3Support() : true;
    smSupport_ = graphics->GetSMSupport();
    computeSupport_ = graphics->GetComputeSupport();
}

Technique::~Technique()
{
}

void Technique::RegisterObject(Context* context)
{
    context->RegisterFactory<Technique>();
}

bool Technique::Load(Deserializer& source)
{
    PROFILE(LoadTechnique);
    
    passes_.Clear();
    SetMemoryUse(sizeof(Technique));
    
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(source))
        return false;
    
    XMLElement rootElem = xml->GetRoot();
    if (rootElem.HasAttribute("sm3"))
        if (rootElem.GetBool("sm3"))
            shaderModel_ = 3;
    if (rootElem.HasAttribute("sm"))
        shaderModel_ = rootElem.GetUInt("sm");
/*
    String globalVS = rootElem.GetAttribute("vs");
    String globalPS = rootElem.GetAttribute("ps");
    String globalVSDefines = rootElem.GetAttribute("vsdefines");
    String globalPSDefines = rootElem.GetAttribute("psdefines");
*/
    String globalVS;
    String globalVSDefines;
    if (rootElem.HasAttribute("vs"))
    {
        globalVS = rootElem.GetAttribute("vs");
        if (!globalVSDefines.Empty())
            globalVSDefines += ' ';
    }
    String globalHS;
    String globalHSDefines;
    if (rootElem.HasAttribute("hs"))
    {
        globalHS = rootElem.GetAttribute("hs");
        if (!globalHSDefines.Empty())
            globalHSDefines += ' ';
    }
    String globalDS;
    String globalDSDefines;
    if (rootElem.HasAttribute("ds"))
    {
        globalDS = rootElem.GetAttribute("ds");
        if (!globalDSDefines.Empty())
            globalDSDefines += ' ';
    }
    String globalGS;
    String globalGSDefines;
    if (rootElem.HasAttribute("gs"))
    {
        globalGS = rootElem.GetAttribute("gs");
        if (!globalGSDefines.Empty())
            globalGSDefines += ' ';
    }
    String globalPS;
    String globalPSDefines;
    if (rootElem.HasAttribute("ps"))
    {
        globalPS = rootElem.GetAttribute("ps");
        if (!globalPSDefines.Empty())
            globalPSDefines += ' ';
    }
    String globalCS;
    String globalCSDefines;
    if (rootElem.HasAttribute("cs"))
    {
        usesCompute_ = true;
        globalCS = rootElem.GetAttribute("cs");
        if (!globalCSDefines.Empty())
            globalCSDefines += ' ';
    }
    /*
    // End with space so that the pass-specific defines can be appended
    if (!globalVSDefines.Empty())
        globalVSDefines += ' ';
    if (!globalPSDefines.Empty())
        globalPSDefines += ' ';
    */
    bool globalAlphaMask = false;
    if (rootElem.HasAttribute("alphamask"))
        globalAlphaMask = rootElem.GetBool("alphamask");
    
    unsigned numPasses = 0;
    
    XMLElement passElem = rootElem.GetChild("pass");
    while (passElem)
    {
        if (passElem.HasAttribute("name"))
        {
            StringHash nameHash(passElem.GetAttribute("name"));
            
            Pass* newPass = CreatePass(nameHash);
            ++numPasses;
            
            if (passElem.HasAttribute("sm3"))
                //newPass->SetIsSM3(passElem.GetBool("sm3"));
                if (passElem.GetBool("sm3"))
                    newPass->SetShaderModel(3);
            if (passElem.HasAttribute("sm"))
                newPass->SetShaderModel(passElem.GetUInt("sm"));
            // Append global defines only when pass does not redefine the shader
            if (passElem.HasAttribute("vs"))
            {
                newPass->SetVertexShader(passElem.GetAttribute("vs"));
                newPass->SetVertexShaderDefines(passElem.GetAttribute("vsdefines"));
            }
            else
            {
                newPass->SetVertexShader(globalVS);
                newPass->SetVertexShaderDefines(globalVSDefines + passElem.GetAttribute("vsdefines"));
            }

            if (passElem.HasAttribute("hs"))
            {
                newPass->SetHullShader(passElem.GetAttribute("hs"));
                newPass->SetHullShaderDefines(passElem.GetAttribute("hsdefines"));
            }
            else
            {
                newPass->SetHullShader(globalHS);
                newPass->SetHullShaderDefines(globalVSDefines + passElem.GetAttribute("hsdefines"));
            }

            if (passElem.HasAttribute("ds"))
            {
                newPass->SetDomainShader(passElem.GetAttribute("ds"));
                newPass->SetDomainShaderDefines(passElem.GetAttribute("dsdefines"));
            }
            else
            {
                newPass->SetDomainShader(globalDS);
                newPass->SetDomainShaderDefines(globalDSDefines + passElem.GetAttribute("dsdefines"));
            }

            if (passElem.HasAttribute("gs"))
            {
                newPass->SetGeometryShader(passElem.GetAttribute("gs"));
                newPass->SetGeometryShaderDefines(passElem.GetAttribute("gsdefines"));
            }
            else
            {
                newPass->SetGeometryShader(globalGS);
                newPass->SetGeometryShaderDefines(globalGSDefines + passElem.GetAttribute("gsdefines"));
            }

            if (passElem.HasAttribute("ps"))
            {
                newPass->SetPixelShader(passElem.GetAttribute("ps"));
                newPass->SetPixelShaderDefines(passElem.GetAttribute("psdefines"));
            }
            else
            {
                newPass->SetPixelShader(globalPS);
                newPass->SetPixelShaderDefines(globalPSDefines + passElem.GetAttribute("psdefines"));
            }

            if (passElem.HasAttribute("cs"))
            {
                newPass->SetUsesCompute(true);
                newPass->SetComputeShader(passElem.GetAttribute("cs"));
                newPass->SetComputeShaderDefines(passElem.GetAttribute("csdefines"));
            }
            else
            {
                newPass->SetUsesCompute(true);
                newPass->SetComputeShader(globalCS);
                newPass->SetComputeShaderDefines(globalCSDefines + passElem.GetAttribute("csdefines"));
            }
            
            if (passElem.HasAttribute("lighting"))
            {
                String lighting = passElem.GetAttributeLower("lighting");
                newPass->SetLightingMode((PassLightingMode)GetStringListIndex(lighting.CString(), lightingModeNames,
                    LIGHTING_UNLIT));
            }
            
            if (passElem.HasAttribute("blend"))
            {
                String blend = passElem.GetAttributeLower("blend");
                newPass->SetBlendMode((BlendMode)GetStringListIndex(blend.CString(), blendModeNames, BLEND_REPLACE));
            }
            
            if (passElem.HasAttribute("depthtest"))
            {
                String depthTest = passElem.GetAttributeLower("depthtest");
                if (depthTest == "false")
                    newPass->SetDepthTestMode(CMP_ALWAYS);
                else
                    newPass->SetDepthTestMode((CompareMode)GetStringListIndex(depthTest.CString(), compareModeNames, CMP_LESS));
            }
            
            if (passElem.HasAttribute("depthwrite"))
                newPass->SetDepthWrite(passElem.GetBool("depthwrite"));
            
            if (passElem.HasAttribute("alphamask"))
                newPass->SetAlphaMask(passElem.GetBool("alphamask"));
            else
                newPass->SetAlphaMask(globalAlphaMask);
        }
        else
            LOGERROR("Missing pass name");
        
        passElem = passElem.GetNext("pass");
    }
    
    // Calculate memory use now
    SetMemoryUse(sizeof(Technique) + numPasses * sizeof(Pass));
    return true;
}

/*
void Technique::SetIsSM3(bool enable)
{
    isSM3_ = enable;
}
*/

void Technique::SetShaderModel(unsigned value)
{
    shaderModel_ = value;
//    if (value > 2)
//        isSM3_ = true;
}

void Technique::SetUsesCompute(bool enable)
{
    usesCompute_ = enable;
}

void Technique::ReleaseShaders()
{
    PODVector<SharedPtr<Pass>*> allPasses = passes_.Values();
    
    for (unsigned i = 0; i < allPasses.Size(); ++i)
        allPasses[i]->Get()->ReleaseShaders();
}

Pass* Technique::CreatePass(StringHash type)
{
    /// \todo Memory use is not tracked when creating passes programmatically due to HashTable not returning the element count
    Pass* oldPass = GetPass(type);
    if (oldPass)
        return oldPass;
    
    SharedPtr<Pass> newPass(new Pass(type));
    passes_.Insert(type.Value(), newPass);
    
    return newPass;
}

void Technique::RemovePass(StringHash type)
{
    passes_.Erase(type.Value());
}

}
