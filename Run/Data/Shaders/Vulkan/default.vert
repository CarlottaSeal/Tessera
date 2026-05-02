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

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;

void main()
{
    vec4 localPos  = vec4(inPosition, 1.0);
    vec4 worldPos  = model.ModelToWorldTransform * localPos;
    vec4 cameraPos = cam.WorldToCameraTransform   * worldPos;
    vec4 renderPos = cam.CameraToRenderTransform  * cameraPos;
    gl_Position    = cam.RenderToClipTransform    * renderPos;

    vColor    = inColor * model.ModelColor;
    vTexCoord = inTexCoord;
}
