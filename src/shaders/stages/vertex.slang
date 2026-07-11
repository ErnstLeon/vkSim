import common.types;

VSOutput vertexShader(VSInput input, float4x4 model, float4x4 normalMatrix, float4x4 view,
                      float4x4 proj)
{
  VSOutput output;

  float4 worldPosition = mul(model, float4(input.inPos, 1.0));
  float3x3 model3x3 = float3x3(model);

  output.outPos = mul(proj, mul(view, worldPosition));
  output.outNormal = mul(model3x3, input.inNormal);
  output.outWorldPos = worldPosition.xyz;
  output.outTexCoord = input.inTexCoord;
  return output;
}
