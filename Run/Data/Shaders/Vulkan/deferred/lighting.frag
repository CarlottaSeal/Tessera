#version 450

// Input attachments resolve to tile-memory reads on Adreno/Mali/PowerVR — this is the
// bandwidth payoff of co-locating the G-buffer fill and lighting in one render pass.
// subpassLoad samples only the current pixel of an input attachment, which is exactly
// what tilers can satisfy from on-chip SRAM.
layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput uAlbedo;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput uNormal;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput uDepth;

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
layout(set = 1, binding = 3) uniform LightConstants {
    PointLight lights[8];
    int        numLights;
    int        _pad0;
    int        _pad1;
    int        _pad2;
} lc;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

vec3 ReconstructWorldPos(vec2 uv, float depthNDC)
{
    // lighting.vert and gbuffer.vert both go through the same negative-height viewport,
    // so vUv from lighting.vert at a given fragment corresponds to the same NDC.xy that
    // gbuffer.vert produced for that fragment. No extra Y flip needed.
    vec4 clip = vec4(uv * 2.0 - 1.0, depthNDC, 1.0);

    mat4 invClipToRender    = inverse(cam.RenderToClipTransform);
    mat4 invRenderToCamera  = inverse(cam.CameraToRenderTransform);
    mat4 invWorldToCamera   = inverse(cam.WorldToCameraTransform);

    vec4 renderPos = invClipToRender * clip;
    renderPos    /= renderPos.w;
    vec4 cameraPos = invRenderToCamera * renderPos;
    vec4 worldPos  = invWorldToCamera * cameraPos;
    return worldPos.xyz;
}

void main()
{
    vec4 gAlb = subpassLoad(uAlbedo);
    vec4 gN   = subpassLoad(uNormal);
    float depth = subpassLoad(uDepth).r;

    if (depth >= 0.99999)
    {
        outColor = vec4(0.05, 0.05, 0.07, 1.0);
        return;
    }

    vec3 albedo   = gAlb.rgb;
    float shin    = max(1.0, gAlb.a * 256.0); // shininess from gAlbedo.a (8-bit precision)
    float specInt = 0.6;                       // constant — matches forward_lit.frag
    vec3 N        = normalize(gN.rgb * 2.0 - 1.0);

    vec3 worldPos = ReconstructWorldPos(vUv, depth);
    vec3 V = normalize(cam.CameraWorldPosition - worldPos);

    vec3 lit = albedo * 0.1; // ambient

    for (int i = 0; i < lc.numLights; ++i)
    {
        vec3  toLight = lc.lights[i].posAndRadius.xyz - worldPos;
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
