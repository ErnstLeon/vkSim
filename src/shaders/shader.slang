struct VSInput
{
  float3 inPos;
  float3 inColor;
  float2 inTexCoord;
};

struct UniformBuffer
{
  float4x4 view;
  float4x4 proj;
};

struct PushConstants
{
  float4x4 model;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBuffer> camera;

[[vk::binding(0, 1)]]
Sampler2D texture;

[[vk::push_constant]]
PushConstants pushConstants;

struct VSOutput
{
  float4 pos : SV_Position;
  float3 fragColor;
  float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
  VSOutput output;
  output.pos =
      mul(camera.proj, mul(camera.view, mul(pushConstants.model, float4(input.inPos, 1.0))));
  output.fragColor = input.inColor;
  output.fragTexCoord = input.inTexCoord;
  return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET { return texture.Sample(vertIn.fragTexCoord); }
