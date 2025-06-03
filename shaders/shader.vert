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
    mat4 camera_matrices[2];
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

    vec4 worldPos = vec4(
        mod_pos + quadVertices[gl_VertexIndex],
        0.0,
        1.0
    ); 

    if(constants.cam_mode <= 1){
        gl_Position = ubo.camera_matrices[constants.cam_mode] * worldPos;
    }else{
        vec4 ortho_offset = 
            ubo.camera_matrices[1] * vec4( mod_pos + vec2(0.5, 0.5), 0.0, 1.0);
        gl_Position = (ubo.camera_matrices[0] * vec4(quadVertices[gl_VertexIndex], 0.0, 1.0)) + ortho_offset;
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
