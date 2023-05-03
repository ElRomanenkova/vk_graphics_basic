#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out float out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D gPosition;
layout (binding = 2) uniform sampler2D gNormal;
layout (binding = 3) buffer ssaoSamples
{
    vec4 samples[];
};
layout (binding = 4) buffer ssaoNoise
{
    vec4 ssaoNoiseVector[];
};

vec2 noiseScale = vec2(1024 / Params.ssaoNoiseSize, 1024 / Params.ssaoNoiseSize);


void main()
{
  vec3 vPos   = texture(gPosition, vOut.texCoord).xyz;
  vec3 vNorm    = texture(gNormal, vOut.texCoord).xyz;

  vec2 noiseCoord = vOut.texCoord - vOut.texCoord * noiseScale;
  vec3 randomVec = ssaoNoiseVector[uint(noiseCoord.x * Params.ssaoNoiseSize + noiseCoord.y)].xyz;

  vec3 tangent   = normalize(randomVec  - vNorm * dot(randomVec , vNorm));
  vec3 bitangent = cross(vNorm, tangent);
  mat3 TBN = mat3(tangent, bitangent, vNorm);

  float occlusion = 0.f;
  for (int i = 0; i < Params.ssaoKernelSize; ++i)
  {
    vec3 ssaoSample = TBN * samples[i].xyz;
    ssaoSample = vPos + ssaoSample * Params.ssaoRadius;

    vec4 offset = Params.projMat * vec4(ssaoSample, 1.f);
    offset.xyz /= offset.w;
    offset.xyz = offset.xyz * 0.5 + 0.5;

    float sampleDepth = texture(gPosition, offset.xy).z;

    float rangeCheck = smoothstep(0.0, 1.0, Params.ssaoRadius / abs(vPos.z - sampleDepth));
    occlusion += (sampleDepth >= ssaoSample.z + 0.025 ? 1.0 : 0.0) * rangeCheck;
  }

  occlusion = 1.0 - (occlusion / Params.ssaoKernelSize);
  out_fragColor = occlusion;
}