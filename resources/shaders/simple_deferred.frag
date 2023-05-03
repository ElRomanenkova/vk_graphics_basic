#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform sampler2D gPosition;
layout (binding = 3) uniform sampler2D gNormals;
layout (binding = 4) uniform sampler2D gAlbedo;
layout (binding = 5) uniform sampler2D ssaoMap;

void main()
{
  const vec4 vNorm = texture(gNormals, vOut.texCoord);
  const vec3 wNorm = (Params.invViewMat * vNorm).xyz;

  const vec4 vPos = vec4(texture(gPosition, vOut.texCoord).xyz, 1.0);
  const vec3 wPos = (Params.invViewMat * vPos).xyz;

  const vec4 posLightClipSpace = Params.lightMatrix * vec4(wPos, 1.0);
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz / posLightClipSpace.w;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy * 0.5f + vec2(0.5f, 0.5f);  // from [-1,1] to [0,1]

  const bool outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow   = ((posLightSpaceNDC.z < texture(shadowMap, shadowTexCoord).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec4 lightColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  const vec3 lightDir   = normalize(Params.lightPos - wPos);

  const vec4 diffuse    = max(dot(wNorm, lightDir), 0.0f) * lightColor;
  const float occlusion = Params.isSsao ? texture(ssaoMap, vOut.texCoord).r : 1.0f;

  const vec4 baseColor = vec4(Params.baseColor, 1.0f);

  out_fragColor = (baseColor * vec4(0.2f) * occlusion + diffuse * shadow) * texture(gAlbedo, vOut.texCoord);
}