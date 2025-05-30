#version 450
#define ASCII_SCR_WIDTH 32
#define ASCII_SCR_HEIGHT 24
#define ATLAS_WIDTH 32
#define PALETTE_SIZE 16.0
#define TILE_SIZE 8

layout(push_constant) uniform PushConstants {
    vec2 screen_size;
} constants;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Add depth/height data per character
layout(set = 0, binding = 1) uniform sampler2D depthTexture; // Optional: for height variation

layout(location = 1) in uint unicode_atlas_and_colors;

layout(location = 0) out vec2 unicodeUV;
layout(location = 1) flat out uint atlas_index;
layout(location = 2) flat out vec2 fgUV;
layout(location = 3) flat out vec2 bgUV;

out gl_PerVertex {
    vec4 gl_Position;
};

// Counter clockwise quad vertices
vec2 quadVertices[6] = {
    vec2(0, 0), vec2(0, 1), vec2(1, 0),
    vec2(0, 1), vec2(1, 1), vec2(1, 0)
};

vec2 encodingToUV(vec2 quadUV, uint encoding) {
    uint tex_col = uint(mod(float(encoding), ATLAS_WIDTH));
    uint tex_row = encoding / ATLAS_WIDTH;
    vec2 uv_offset = vec2(tex_col, tex_row);
    return (quadUV + uv_offset);
}

void main() {
    // Calculate grid position
    vec2 mod_pos = vec2(gl_InstanceIndex % ASCII_SCR_WIDTH,
                        gl_InstanceIndex / ASCII_SCR_WIDTH);
    
    // Get base quad vertex
    vec2 quadVertex = quadVertices[gl_VertexIndex];
    
    // Create 3D position from 2D grid
    vec3 worldPos = vec3(
        mod_pos.x + quadVertex.x,   // Center X
        mod_pos.y + quadVertex.y,  // Center Z
        0.0                        // Base Y (ground level)
    );
    
    // Optional: Add height variation based on character or depth texture
    // vec2 depthUV = mod_pos / vec2(ASCII_SCR_WIDTH, ASCII_SCR_HEIGHT);
    // float height = texture(depthTexture, depthUV).r * 5.0; // Scale height
    // worldPos.y += height;
    
    // Transform to clip space
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(worldPos, 1.0);
    
    // Pass through texture coordinates and color data
    uint unicode = (unicode_atlas_and_colors >> 22) & 0x3FFu;
    unicodeUV = encodingToUV(quadVertex, unicode);
    atlas_index = (unicode_atlas_and_colors >> 16) & 0x3Fu;
    
    uint fg_index = (unicode_atlas_and_colors >> 8) & 0xFFu;
    uint bg_index = unicode_atlas_and_colors & 0xFFu;
    fgUV = vec2(float(fg_index) / PALETTE_SIZE, 0.5);
    bgUV = vec2(float(bg_index) / PALETTE_SIZE, 0.5);
}
