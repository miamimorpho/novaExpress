#version 450

#define ASCII_SCR_WIDTH 32
#define ASCII_SCR_HEIGHT 24

#define ATLAS_WIDTH 32
#define PALETTE_SIZE 16.0
#define TILE_SIZE 8

layout( push_constant ) uniform PushConstants {
  vec2 screen_size;
} constants;

layout(set = 0, binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
} ubo;

layout(location = 1) in uint unicode_atlas_and_colors;

layout(location = 0) out vec2 unicodeUV;
layout(location = 1) flat out uint atlas_index;
layout(location = 2) flat out vec2 fgUV;
layout(location = 3) flat out vec2 bgUV;

out gl_PerVertex {
  vec4 gl_Position;
};

/* Counter clockwise direction */
vec2 quadVertices[6] = {
  vec2(0, 0),
  vec2(0, 1),
  vec2(1, 0),
  vec2(0, 1),
  vec2(1, 1),
  vec2(1, 0)
};

vec2 encodingToUV(vec2 quadUV, uint encoding){
  uint tex_col = uint(mod(float(encoding), ATLAS_WIDTH));
  uint tex_row = encoding / ATLAS_WIDTH;
  vec2 uv_offset = vec2(tex_col, tex_row);

  return (quadUV + uv_offset);
}

void main() {

  vec2 mod_pos = vec2(gl_InstanceIndex % ASCII_SCR_WIDTH,
    gl_InstanceIndex / ASCII_SCR_WIDTH);

  // Position
  vec2 synthPos = mod_pos + quadVertices[gl_VertexIndex];
  //vec2 asciiToScreen = 1 / vec2( ASCII_SCR_WIDTH, ASCII_SCR_HEIGHT);
  //vec2 ndcPos = synthPos * asciiToScreen -1;
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(synthPos, 0, 1.0);
  //gl_Position = ubo.view * vec4(synthPos, 0, 1.0);
  //gl_Position = vec4(ndcPos, 0, 1.0);

  // Glyph Selection
  uint unicode = (unicode_atlas_and_colors >> 22) & 0x3FFu;
  unicodeUV = encodingToUV(quadVertices[gl_VertexIndex], unicode);
  atlas_index = (unicode_atlas_and_colors >> 16) & 0x3Fu;

  // Color
  uint fg_index = (unicode_atlas_and_colors >> 8) & 0xFFu;
  uint bg_index =  unicode_atlas_and_colors & 0xFFu;
  fgUV = vec2(float(fg_index) / PALETTE_SIZE, 0.5);
  bgUV = vec2(float(bg_index) / PALETTE_SIZE, 0.5);

}
