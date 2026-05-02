#version 450

layout(set = 0, binding = 0) uniform CameraConstants {
    mat4 WorldToCameraTransform;
    mat4 CameraToRenderTransform;
    mat4 RenderToClipTransform;
    vec3 CameraWorldPosition;
    float padding;
} cam;

layout(set = 0, binding = 4) uniform ModelConstants {
    mat4 ModelToWorldTransform;
    vec4 ModelColor;
} model;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vWorldNormal;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec2 vTexCoord;

void main()
{
    vec4 localPos = vec4(inPosition, 1.0);
    vec4 worldPos = model.ModelToWorldTransform * localPos;

    // PCU has no normal attribute — fake it from local position. Correct for
    // origin-centred spheres; non-perpendicular for cubes (acceptable for this demo).
    vec3 localNormal = normalize(inPosition);
    mat3 modelRot = mat3(model.ModelToWorldTransform);
    vec3 worldNormal = normalize(modelRot * localNormal);

    vWorldPos    = worldPos.xyz;
    vWorldNormal = worldNormal;
    vColor       = inColor * model.ModelColor;
    vTexCoord    = inTexCoord;

    vec4 cameraPos = cam.WorldToCameraTransform * worldPos;
    vec4 renderPos = cam.CameraToRenderTransform * cameraPos;
    gl_Position    = cam.RenderToClipTransform * renderPos;
}
