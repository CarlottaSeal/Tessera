#version 450

layout(set = 0, binding = 1) uniform sampler2D g_textures[256];

layout(set = 0, binding = 0) uniform CameraConstants {
    mat4 WorldToCameraTransform;
    mat4 CameraToRenderTransform;
    mat4 RenderToClipTransform;
    vec3 CameraWorldPosition;
    float padding;
} cam;

struct PointLight {
    vec4 posAndRadius;
    vec4 colorAndIntensity;
};
layout(set = 1, binding = 0) uniform LightConstants {
    PointLight lights[8];
    int numLights;
    int _pad0;
    int _pad1;
    int _pad2;
} lc;

layout(push_constant) uniform MaterialConstantsPC {
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

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 sampleUV = vec2(vTexCoord.x, 1.0 - vTexCoord.y);
    vec4 tex = texture(g_textures[mat.DiffuseId], sampleUV);
    if (tex.a < 0.1) discard;

    vec3 nT = texture(g_textures[mat.NormalId], sampleUV).rgb * 2.0 - 1.0;
    vec3 T = normalize(vWorldTangent);
    vec3 B = normalize(vWorldBitangent);
    vec3 Nv = normalize(vWorldNormal);
    mat3 TBN = mat3(T, B, Nv);
    vec3 N = normalize(TBN * nT);

    vec3 albedo   = tex.rgb * vColor.rgb;
    float specInt = 0.6;
    float shin    = 32.0;

    vec3 V = normalize(cam.CameraWorldPosition - vWorldPos);

    vec3 lit = albedo * 0.1;
    for (int i = 0; i < lc.numLights; ++i)
    {
        vec3  toLight = lc.lights[i].posAndRadius.xyz - vWorldPos;
        float d       = length(toLight);
        float radius  = lc.lights[i].posAndRadius.w;
        float atten   = max(0.0, 1.0 - d / radius);
        atten        *= atten;
        vec3  L       = toLight / max(d, 1e-4);
        vec3  H       = normalize(L + V);
        vec3  lcol    = lc.lights[i].colorAndIntensity.rgb;
        float linten  = lc.lights[i].colorAndIntensity.a;

        float NdotL   = max(0.0, dot(N, L));
        float NdotH   = max(0.0, dot(N, H));
        float specF   = pow(NdotH, shin) * specInt;

        vec3 diffuse  = albedo * NdotL;
        vec3 specular = vec3(specF);
        lit += (diffuse + specular) * lcol * (atten * linten);
    }

    outColor = vec4(lit, 1.0);
}
