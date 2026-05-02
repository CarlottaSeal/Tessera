struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 modelTangent : TANGENT;
	float3 modelBitangent : BITANGENT;
	float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition : SV_Position;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;	// View transform
	float4x4 CameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 RenderToClipTransform;		// Projection transform
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelToWorldTransform;		// Model transform
	float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
cbuffer ShadowConstants : register(b6)
{
	float4x4 LightViewProjMatrix;        // World to Light's Clip Space
};

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelPosition = float4(input.modelPosition, 1.0f);
	float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
	float4 lightSpacePosition = mul(LightViewProjMatrix, worldPosition);

	v2p_t v2p;
	v2p.clipPosition = lightSpacePosition;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float PixelMain(v2p_t input) : SV_Depth
{
	// Nothing to compute; GPU will automatically output depth from clipPosition.z
	return 0.0f;
}