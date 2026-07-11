// SLANG shader code for common types used in vertex and fragment shaders

// Define the input structure for the vertex shader
struct VSInput
{
  float3 inPos;
  float3 inNormal;
  float2 inTexCoord;
};

// Define the output structure for the vertex shader
struct VSOutput
{
  float4 outPos : SV_Position;
  float3 outWorldPos;
  float3 outNormal;
  float2 outTexCoord;
};

// Define the push constants structure for passing object data like model transformation and
// texture/model indices
struct PushConstants
{
  float4x4 model;
  float4x4 normalMatrix;
  uint32_t textureIndex;
  uint32_t modelIndex;
};

// Define the camera buffer structure for passing view and projection matrices and camera position
struct CameraBuffer
{
  float4x4 view;
  float4x4 proj;
  float3 cameraPos;
};

// Define the directional light buffer structure for passing directional light parameters
struct DirectionalLight
{
  float4 direction; // xyz: direction, w: unused
  float4 color;     // rgb: color, w: intensity
};

// Define the point light buffer structure for passing point light parameters
struct PointLight
{
  float4 position; // xyz: position, w: unused
  float4 color;    // rgb: color, w: intensity
};

// Define the spot light buffer structure for passing spot light parameters
struct SpotLight
{
  float4 position;  // xyz: position, w: inner cone angle as cosine of the angle
  float4 direction; // xyz: direction, w: outer cone angle as cosine of the angle
  float4 color;     // rgb: color, w: intensity
};

struct SceneInfo
{
  uint32_t numDirectionalLights;
  uint32_t numPointLights;
  uint32_t numSpotLights;
};

// Define the material buffer structure for passing material properties like color, metallic, and
// roughness
struct MaterialBuffer
{
  float3 color;
  float metallic;
  float roughness;
};
