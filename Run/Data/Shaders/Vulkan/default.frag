#version 450

layout(set = 0, binding = 1) uniform sampler2D g_textures[256];

layout(push_constant) uniform MaterialConstantsPC
{
    int DiffuseId;
    int NormalId;
    int SpecularId;
    float _pad;
} mat;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec4 outFragColor;

void main()
{
    // V flip: engine UV is authored Y-up (DX convention); stb_image loads Y-down.
    vec2 sampleUV = vec2(vTexCoord.x, 1.0 - vTexCoord.y);
    vec4 texColor = texture(g_textures[mat.DiffuseId], sampleUV);
    if (texColor.a < 0.1) discard;

    outFragColor = texColor * vColor;
}
