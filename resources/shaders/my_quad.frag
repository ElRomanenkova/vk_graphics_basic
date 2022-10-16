#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

const float SIGMA  = 0.5;
const float H      = 0.05;
const int WIND_RAD = 2;

void main()
{
  vec4 unchanged_color = textureLod(colorTex, surf.texCoord, 0);
  ivec2 tex_size = textureSize(colorTex, 0);

  float sum_of_k = 0.0;
  vec4 sum_of_col = vec4(0.0);

  for (int i = -WIND_RAD; i <= WIND_RAD; ++i)
    for (int j = -WIND_RAD; j <= WIND_RAD; ++j)
    {
      vec4 changed_color = textureLod(colorTex, surf.texCoord + vec2(i, j) / tex_size, 0);

      float dist = length(vec2(i * i, j * j));
      float photo_dist = length(changed_color*changed_color - unchanged_color*unchanged_color);

      float k_ij = exp(- dist / (2*SIGMA*SIGMA) - photo_dist / (2*H*H));

      sum_of_col += k_ij * changed_color;
      sum_of_k += k_ij;
    }

  color = sum_of_col / sum_of_k;
}
