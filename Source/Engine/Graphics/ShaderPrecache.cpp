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
#include "File.h"
#include "FileSystem.h"
#include "Graphics.h"
#include "GraphicsImpl.h"
#include "Log.h"
#include "ShaderPrecache.h"
#include "ShaderVariation.h"

#include "DebugNew.h"

namespace Urho3D
{

ShaderPrecache::ShaderPrecache(Context* context, const String& fileName) :
    Object(context),
    fileName_(fileName),
    xmlFile_(context)
{
    if (GetSubsystem<FileSystem>()->FileExists(fileName))
    {
        // If file exists, read the already listed combinations
        File source(context_, fileName);
        xmlFile_.Load(source);
        
        XMLElement shader = xmlFile_.GetRoot().GetChild("shader");
        while (shader)
        {
            String oldCombination = shader.GetAttribute("vs") + " " + shader.GetAttribute("vsdefines") + " " +
                shader.GetAttribute("ps") + " " + shader.GetAttribute("psdefines");
            usedCombinations_.Insert(oldCombination);
            
            shader = shader.GetNext("shader");
        }
    }
    
    // If no file yet or loading failed, create the root element now
    if (!xmlFile_.GetRoot())
        xmlFile_.CreateRoot("shaders");
    
    LOGINFO("Begin dumping shaders to " + fileName_);
}

ShaderPrecache::~ShaderPrecache()
{
    LOGINFO("End dumping shaders");
    
    if (usedCombinations_.Empty())
        return;
    
    File dest(context_, fileName_, FILE_WRITE);
    xmlFile_.Save(dest);
}

void ShaderPrecache::StoreShaders(ShaderVariation* vs, ShaderVariation* hs, ShaderVariation* ds,
                                  ShaderVariation* gs, ShaderVariation* ps, ShaderVariation* cs)
{
    // We need at least a vertex shader or a compute shader here, but not both!
    if ((!vs || !cs) || (vs && cs))
        return;

    // Compute shaders link alone.
    if (cs && (hs || ds || gs || ps))
        return;
    
    // Check for duplicate using pointers first (fast)
    Pair<ShaderVariation*, ShaderVariation*> shaderPair = MakePair(vs, ps);
    if (usedPtrCombinations_.Contains(shaderPair))
        return;
    usedPtrCombinations_.Insert(shaderPair);
    
    String newCombination;

    String vsName;
    String csName;
    String vsDefines;
    String csDefines;

    if (vs)
    {
        vsName = vs->GetName();
        vsDefines = vs->GetDefines();
        newCombination += vsName + " " + vsDefines;
        // Transform Feedback/Stream out
        if (!hs || !ds || !gs || !ps)
        {
            if (usedCombinations_.Contains(newCombination))
                return;
            usedCombinations_.Insert(newCombination);
            XMLElement shaderElem = xmlFile_.GetRoot().CreateChild("shader");
            shaderElem.SetAttribute("vs", vsName);
            shaderElem.SetAttribute("vsdefines", vsDefines);
            return;
        }
    }
    else if (cs)
    {
        csName = cs->GetName();
        csDefines = cs->GetDefines();
        newCombination += csName + " " + csDefines;
        if (usedCombinations_.Contains(newCombination))
            return;
        usedCombinations_.Insert(newCombination);
        XMLElement shaderElem = xmlFile_.GetRoot().CreateChild("shader");
        shaderElem.SetAttribute("cs", csName);
        shaderElem.SetAttribute("csdefines", csDefines);
        return;
    }

    String hsName;
    String dsName;
    String hsDefines;
    String dsDefines;

    if (hs && ds)
    {
        hsName = hs->GetName();
        dsName = ds->GetName();
        hsDefines = hs->GetDefines();
        dsDefines = ds->GetDefines();
        newCombination += " " + hsName + " " + hsDefines + " " + dsName + " " + dsDefines;
    }
    String gsName;
    String gsDefines;
    if (gs)
    {
        gsName = gs->GetName();
        gsDefines = gs->GetDefines();
        newCombination += " " + gsName + " " + gsDefines;
    }
    String psName;
    String psDefines;
    if (ps)
    {
        psName = ps->GetName();
        psDefines = ps->GetDefines();
        newCombination += " " + psName + " " + psDefines;
    }

    // Check for duplicate using strings (needed for combinations loaded from existing file)
    if (usedCombinations_.Contains(newCombination))
        return;
    usedCombinations_.Insert(newCombination);
    
    XMLElement shaderElem = xmlFile_.GetRoot().CreateChild("shader");
    shaderElem.SetAttribute("vs", vsName);
    shaderElem.SetAttribute("vsdefines", vsDefines);
    if (hs && ds)
    {
        shaderElem.SetAttribute("hs", hsName);
        shaderElem.SetAttribute("hsdefines", hsDefines);
        shaderElem.SetAttribute("ds", dsName);
        shaderElem.SetAttribute("dsdefines", dsDefines);
    }
    if (gs)
    {
        shaderElem.SetAttribute("gs", gsName);
        shaderElem.SetAttribute("gsdefines", gsDefines);
    }
    if (ps)
    {
        shaderElem.SetAttribute("ps", psName);
        shaderElem.SetAttribute("psdefines", psDefines);
    }
}

void ShaderPrecache::LoadShaders(Graphics* graphics, Deserializer& source)
{
    LOGDEBUG("Begin precaching shaders");
    
    XMLFile xmlFile(graphics->GetContext());
    xmlFile.Load(source);
    
    XMLElement shader = xmlFile.GetRoot().GetChild("shader");
    while (shader)
    {
        // OpenGL ES 2.0 doesn't do tessellation/geometry/compute shaders
        #ifdef GL_ES_VERSION_2_0
        if (shader.HasAttribute("hsdefines") || shader.HasAttribute("dsdefines") || shader.HasAttribute("gsdefines") || shader.HasAttribute("csdefines"))
        {
            shader = shader.GetNext("shader");
            continue;
        }
        #else
        unsigned sm = graphics->GetShaderModel();
        // Shader Model 3.0 just does vertex and pixel shaders
        if ((sm < 4) && (shader.HasAttribute("hs") || shader.HasAttribute("ds") || shader.HasAttribute("gs") || shader.HasAttribute("cs")))
        {
            shader = shader.GetNext("shader");
            continue;
        }
        // Shader Model 4.0 just does vertex, geometry and pixel shaders
        if ((sm < 5) && (shader.HasAttribute("hs") || shader.HasAttribute("ds") || shader.HasAttribute("cs")))
        {
            shader = shader.GetNext("shader");
            continue;
        }
        #endif

        String vsDefines = shader.GetAttribute("vsdefines");
        String hsDefines = shader.GetAttribute("hsdefines");
        String dsDefines = shader.GetAttribute("dsdefines");
        String gsDefines = shader.GetAttribute("gsdefines");
        String psDefines = shader.GetAttribute("psdefines");
        String csDefines = shader.GetAttribute("csdefines");
        // Check for illegal variations on OpenGL ES and skip them
        #ifdef GL_ES_VERSION_2_0
        if (vsDefines.Contains("INSTANCED") || (psDefines.Contains("POINTLIGHT") && psDefines.Contains("SHADOW")))
        {
            shader = shader.GetNext("shader");
            continue;
        }
        #endif

        ShaderVariation* vs = graphics->GetShader(VS, shader.GetAttribute("vs"), vsDefines);
        ShaderVariation* hs = graphics->GetShader(HS, shader.GetAttribute("hs"), hsDefines);
        ShaderVariation* ds = graphics->GetShader(DS, shader.GetAttribute("ds"), dsDefines);
        ShaderVariation* gs = graphics->GetShader(GS, shader.GetAttribute("gs"), gsDefines);
        ShaderVariation* ps = graphics->GetShader(PS, shader.GetAttribute("ps"), psDefines);
        ShaderVariation* cs = graphics->GetShader(CS, shader.GetAttribute("cs"), csDefines);
        // Set the shaders active to actually compile them
        graphics->SetShaders(vs, hs, ds, gs, ps, cs);
        
        shader = shader.GetNext("shader");
    }
    
    LOGDEBUG("End precaching shaders");
}

}
