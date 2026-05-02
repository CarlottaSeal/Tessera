#version 450

layout(set = 0, binding = 1) uniform sampler2D g_textures[256];

layout(push_constant) uniform MaterialConstantsPC
{
    int DiffuseId;
    int NormalId;
    int SpecularId;
    float _pad;
} mat;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec2 vTexCoord;
layout(location = 4) in vec3 vWorldTangent;
layout(location = 5) in vec3 vWorldBitangent;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;

void main()
{
    vec2 sampleUV = vec2(vTexCoord.x, 1.0 - vTexCoord.y);
    vec4 tex = texture(g_textures[mat.DiffuseId], sampleUV);
    if (tex.a < 0.1) discard;

    vec3 nT = texture(g_textures[mat.NormalId], sampleUV).rgb * 2.0 - 1.0;

    vec3 T = normalize(vWorldTangent);
    vec3 B = normalize(vWorldBitangent);
    vec3 N = normalize(vWorldNormal);
    mat3 TBN = mat3(T, B, N);
    vec3 worldN = normalize(TBN * nT);

    gAlbedo.rgb = tex.rgb * vColor.rgb;
    gAlbedo.a   = 32.0 / 256.0;
    gNormal.rgb = worldN * 0.5 + 0.5;
    gNormal.a   = 1.0;
}
