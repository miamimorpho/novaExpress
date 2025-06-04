#version 460
#define ASCII_SCR_WIDTH 32
#define ATLAS_WIDTH 32
#define PALETTE_SIZE 16.0

layout(location = 0) in vec2 in_position; // Only used in sprite mode
layout(location = 1) in uint unicode_atlas_and_colors;

layout(push_constant) uniform PushConstants {
    int cam_mode;
    int sprite_mode;
} constants;

layout(set = 0, binding = 0) uniform UniformBufferObject {
  mat4 cam_flat_view;
  mat4 cam_flat_proj;
  mat4 cam_seven_view;
  mat4 cam_seven_proj;
} ubo;

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

// Maps the UV coordinates around a quad from a texture encoding
vec2 encodingToUV(vec2 quadUV, uint encoding) {
    uint tex_col = uint(mod(float(encoding), ATLAS_WIDTH));
    uint tex_row = encoding / ATLAS_WIDTH;
    vec2 uv_offset = vec2(tex_col, tex_row);
    return (quadUV + uv_offset);
}

void main() {

    // POSITIONING
    vec2 mod_pos = vec2(0, 0);
    if(constants.sprite_mode == 0){
    
        // BUG: the spec says gl_InstanceIndex should 
        // return an int relative to vkDraw.firstInstance
        // on intel graphics, this does not happen so we subtract gl_BaseInstance    
    
        int i = gl_InstanceIndex - gl_BaseInstance;
        mod_pos = vec2(i % ASCII_SCR_WIDTH,
                            i / ASCII_SCR_WIDTH);
    }else{ // sprite mode
        mod_pos = in_position;
    }


    if(constants.cam_mode == 1){
        vec4 worldPos = vec4(mod_pos + quadVertices[gl_VertexIndex], 0.0, 1.0 ); 
        gl_Position = ubo.cam_flat_proj * ubo.cam_flat_view * worldPos;
    }else if(constants.cam_mode == 2){
        vec4 worldPos = vec4(mod_pos + quadVertices[gl_VertexIndex], 0.0, 1.0 ); 
        gl_Position = ubo.cam_seven_proj * ubo.cam_seven_view * worldPos;
    }else if(constants.cam_mode == 3){
        // Billboard mode
        mat3 viewMatrix3x3 = mat3(ubo.cam_seven_view);
        vec3 cameraRight = normalize(viewMatrix3x3[0]);
        vec3 cameraUp = normalize(viewMatrix3x3[1]);
        
        // Center the quad around mod_pos (sprite center)
        vec2 quadOffset = quadVertices[gl_VertexIndex] - vec2(0.5, 0.5);
        
        // Create billboard position
        vec3 spriteCenter = vec3(mod_pos.x, mod_pos.y, 1.0);
        vec3 billboardPos = spriteCenter + (cameraRight * quadOffset.x + cameraUp * quadOffset.y);
        
        gl_Position = ubo.cam_seven_proj * ubo.cam_seven_view * vec4(billboardPos, 1.0);
    } else {
        // Fallback
        vec4 worldPos = vec4(mod_pos + quadVertices[gl_VertexIndex], 0.0, 1.0);
        gl_Position = ubo.cam_seven_proj * ubo.cam_seven_view * worldPos;
    }

    // Pass through texture coordinates and color data
    uint unicode = bitfieldExtract(unicode_atlas_and_colors, 22, 10);
    unicodeUV = encodingToUV(quadVertices[gl_VertexIndex], unicode);
    atlas_index = bitfieldExtract(unicode_atlas_and_colors, 16, 6);

    // COLOURS
    uint fg_index = bitfieldExtract(unicode_atlas_and_colors, 8, 8);
    uint bg_index = bitfieldExtract(unicode_atlas_and_colors, 0, 8);
    fgUV = vec2(float(fg_index) / PALETTE_SIZE, 0.5);
    bgUV = vec2(float(bg_index) / PALETTE_SIZE, 0.5);
}
