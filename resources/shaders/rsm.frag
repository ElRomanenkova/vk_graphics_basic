#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec3 lightViewPos;
layout (location = 1) out vec3 lightViewNorm;
layout (location = 2) out vec3 albedo;

layout (location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 texCoord;
} vsOut;

layout (push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    vec3 objColor;
} params;

layout (binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

void main()
{
  lightViewPos = vec4(vsOut.wPos, 1.0).xyz;
  lightViewNorm = vec4(vsOut.wNorm, 0.0).xyz;
  albedo = params.objColor;
}
