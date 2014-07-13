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

#pragma once

#include "GraphicsDefs.h"
#include "HashTable.h"
#include "Resource.h"

namespace Urho3D
{

class ShaderVariation;

/// Lighting mode of a pass.
enum PassLightingMode
{
    LIGHTING_UNLIT,
    LIGHTING_PERVERTEX,
    LIGHTING_PERPIXEL
};

/// %Material rendering pass, which defines shaders and render state.
class URHO3D_API Pass : public RefCounted
{
public:
    /// Construct.
    Pass(StringHash type);
    /// Destruct.
    ~Pass();
    
    /// Set blend mode.
    void SetBlendMode(BlendMode mode);
    /// Set depth compare mode.
    void SetDepthTestMode(CompareMode mode);
    /// Set pass lighting mode, affects what shader variations will be attempted to be loaded.
    void SetLightingMode(PassLightingMode mode);
    /// Set depth write on/off.
    void SetDepthWrite(bool enable);
    /// Set alpha masking hint. Completely opaque draw calls will be performed before alpha masked.
    void SetAlphaMask(bool enable);
//    /// Set whether requires %Shader %Model 3.
//    void SetIsSM3(bool enable);
    /// Set %Shader %Model.
    void SetShaderModel(unsigned value);
    /// Set Uses Compute.
    void SetUsesCompute(bool enable);
    /// Set vertex shader name.
    void SetVertexShader(const String& name);
    /// Set hull shader name.
    void SetHullShader(const String& name);
    /// Set domain shader name.
    void SetDomainShader(const String& name);
    /// Set geometry shader name.
    void SetGeometryShader(const String& name);
    /// Set pixel shader name.
    void SetPixelShader(const String& name);
    /// Set compute shader name.
    void SetComputeShader(const String& name);
    /// Set vertex shader defines.
    void SetVertexShaderDefines(const String& defines);
    /// Set hull shader defines.
    void SetHullShaderDefines(const String& defines);
    /// Set domain shader defines.
    void SetDomainShaderDefines(const String& defines);
    /// Set geometry shader defines.
    void SetGeometryShaderDefines(const String& defines);
    /// Set pixel shader defines.
    void SetPixelShaderDefines(const String& defines);
    /// Set compute shader defines.
    void SetComputeShaderDefines(const String& defines);
    /// Reset shader pointers.
    void ReleaseShaders();
    /// Mark shaders loaded this frame.
    void MarkShadersLoaded(unsigned frameNumber);
    
    /// Return pass type.
    const StringHash& GetType() const { return type_; }
    /// Return blend mode.
    BlendMode GetBlendMode() const { return blendMode_; }
    /// Return depth compare mode.
    CompareMode GetDepthTestMode() const { return depthTestMode_; }
    /// Return pass lighting mode.
    PassLightingMode GetLightingMode() const { return lightingMode_; }
    /// Return last shaders loaded frame number.
    unsigned GetShadersLoadedFrameNumber() const { return shadersLoadedFrameNumber_; }
    /// Return depth write mode.
    bool GetDepthWrite() const { return depthWrite_; }
    /// Return alpha masking hint.
    bool GetAlphaMask() const { return alphaMask_; }
    /// Return compute shader usage.
    bool GetUsesCompute() const { return usesCompute_; }
//    /// Return whether requires %Shader %Model 3.
//    bool IsSM3() const { return isSM3_; }
    /// Return what %Shader %Model is required.
    unsigned GetShaderModel() const { return shaderModel_; }
    /// Return vertex shader name.
    const String& GetVertexShader() const { return vertexShaderName_; }
    /// Return hull shader name.
    const String& GetHullShader() const { return hullShaderName_; }
    /// Return domain shader name.
    const String& GetDomainShader() const { return domainShaderName_; }
    /// Return geometry shader name.
    const String& GetGeometryShader() const { return geometryShaderName_; }
    /// Return pixel shader name.
    const String& GetPixelShader() const { return pixelShaderName_; }
    /// Return compute shader name.
    const String& GetComputeShader() const { return computeShaderName_; }
    /// Return vertex shader defines.
    const String& GetVertexShaderDefines() const { return vertexShaderDefines_; }
    /// Return hull shader defines.
    const String& GetHullShaderDefines() const { return hullShaderDefines_; }
    /// Return domain shader defines.
    const String& GetDomainShaderDefines() const { return domainShaderDefines_; }
    /// Return geometry shader defines.
    const String& GetGeometryShaderDefines() const { return vertexShaderDefines_; }
    /// Return pixel shader defines.
    const String& GetPixelShaderDefines() const { return pixelShaderDefines_; }
    /// Return compute shader defines.
    const String& GetComputeShaderDefines() const { return pixelShaderDefines_; }
    /// Return vertex shaders.
    Vector<SharedPtr<ShaderVariation> >& GetVertexShaders() { return vertexShaders_; }
    /// Return hull shaders.
    Vector<SharedPtr<ShaderVariation> >& GetHullShaders() { return hullShaders_; }
    /// Return domain shaders.
    Vector<SharedPtr<ShaderVariation> >& GetDomainShaders() { return domainShaders_; }
    /// Return geometry shaders.
    Vector<SharedPtr<ShaderVariation> >& GetGeometryShaders() { return geometryShaders_; }
    /// Return pixel shaders.
    Vector<SharedPtr<ShaderVariation> >& GetPixelShaders() { return pixelShaders_; }
    /// Return compute shaders.
    Vector<SharedPtr<ShaderVariation> >& GetComputeShaders() { return computeShaders_; }
    
private:
    /// Pass type.
    StringHash type_;
    /// Blend mode.
    BlendMode blendMode_;
    /// Depth compare mode.
    CompareMode depthTestMode_;
    /// Lighting mode.
    PassLightingMode lightingMode_;
    /// Last shaders loaded frame number.
    unsigned shadersLoadedFrameNumber_;
    /// Depth write mode.
    bool depthWrite_;
    /// Alpha masking hint.
    bool alphaMask_;
    /// Require Compute support.
    bool usesCompute_;
//    /// Require %Shader %Model 3 flag.
//    bool isSM3_;
    /// %Shader %Model.
    unsigned shaderModel_;
    /// Vertex shader name.
    String vertexShaderName_;
    /// Hull shader name.
    String hullShaderName_;
    /// Domain shader name_;
    String domainShaderName_;
    /// Geometry shader name_;
    String geometryShaderName_;
    /// Pixel shader name.
    String pixelShaderName_;
    /// Compute shader name.
    String computeShaderName_;
    /// Vertex shader defines.
    String vertexShaderDefines_;
    /// Hull shader defines.
    String hullShaderDefines_;
    /// Domain shader defines.
    String domainShaderDefines_;
    /// Geometry shader defines.
    String geometryShaderDefines_;
    /// Pixel shader defines.
    String pixelShaderDefines_;
    /// Compute shader defines.
    String computeShaderDefines_;
    /// Vertex shaders.
    Vector<SharedPtr<ShaderVariation> > vertexShaders_;
    /// Hull shaders.
    Vector<SharedPtr<ShaderVariation> > hullShaders_;
    /// Domain shaders.
    Vector<SharedPtr<ShaderVariation> > domainShaders_;
    /// Geometry shaders.
    Vector<SharedPtr<ShaderVariation> > geometryShaders_;
    /// Pixel shaders.
    Vector<SharedPtr<ShaderVariation> > pixelShaders_;
    /// Compute shaders.
    Vector<SharedPtr<ShaderVariation> > computeShaders_;
};

/// %Material technique. Consists of several passes.
class URHO3D_API Technique : public Resource
{
    OBJECT(Technique);
    
    friend class Renderer;
    
public:
    /// Construct.
    Technique(Context* context);
    /// Destruct.
    ~Technique();
    /// Register object factory.
    static void RegisterObject(Context* context);
    
    /// Load resource. Return true if successful.
    virtual bool Load(Deserializer& source);
    
//    /// Set whether requires %Shader %Model 3.
//    void SetIsSM3(bool enable);
    /// Set %Shader %Model that is required.
    void SetShaderModel(unsigned value);
    /// Set uses Compute.
    void SetUsesCompute(bool enable);
    /// Create a new pass.
    Pass* CreatePass(StringHash type);
    /// Remove a pass.
    void RemovePass(StringHash type);
    /// Reset shader pointers in all passes.
    void ReleaseShaders();
    
//    /// Return whether requires %Shader %Model 3.
//    bool IsSM3() const { return isSM3_; }
    /// Return what %Shader %Model is required.
    unsigned GetShaderModel() const { return shaderModel_; }
    /// Return if compute shader is used.
    bool GetUsesCompute() const { return usesCompute_; }
    /// Return whether has a pass.
    bool HasPass(StringHash type) const { return  passes_.Find(type.Value()) != 0; }
    
    /// Return a pass, or null if not found.
    Pass* GetPass(StringHash type) const
    {
        SharedPtr<Pass>* passPtr = passes_.Find(type.Value());
        return passPtr ? passPtr->Get() : 0;
    }
    
    /// Return a pass that is supported for rendering, or null if not found.
    Pass* GetSupportedPass(StringHash type) const
    {
        SharedPtr<Pass>* passPtr = passes_.Find(type.Value());
        Pass* pass = passPtr ? passPtr->Get() : 0;
//        return pass && (!pass->IsSM3() || sm3Support_) ? pass : 0;
        if (pass)
        {
            if ((pass->GetShaderModel() <= smSupport_) && (pass->GetUsesCompute() <= computeSupport_))
                return pass;
            else
                return 0;
        }
        else
            return 0;
    }
    
private:
    /*
    /// Require %Shader %Model 3 flag.
    bool isSM3_;
    /// Cached %Shader %Model 3 support flag.
    bool sm3Support_;
    */
    /// %Shader %Model.
    unsigned shaderModel_;
    /// Uses Compute.
    bool usesCompute_;
    /// Cached %Shader %Model support.
    unsigned smSupport_;
    /// Cached Compute support.
    bool computeSupport_;
    /// Passes.
    HashTable<SharedPtr<Pass>, 16> passes_;
};

}
