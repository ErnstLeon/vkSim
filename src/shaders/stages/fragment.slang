import common.types;
import light.pbr;

static const uint INVALID_INDEX = 0xffffffff;

float4 fragmentShader(VSOutput vertIn, uint32_t modelIndex, uint32_t textureIndex,
                      ConstantBuffer<SceneInfo> sceneInfo,
                      StructuredBuffer<DirectionalLight> directionalLights,
                      StructuredBuffer<PointLight> pointLights,
                      StructuredBuffer<SpotLight> spotLights,
                      ConstantBuffer<MaterialBuffer> material[], Sampler2D texture[],
                      CameraBuffer camera)
{
  // Default albedo color is white
  float3 albedo = float3(1.0, 1.0, 1.0);
  MaterialBuffer mat;

  if (modelIndex != INVALID_INDEX)
  {
    mat = material[modelIndex];
    albedo = mat.color;
  }

  if (textureIndex != INVALID_INDEX)
  {
    float4 texColor = texture[textureIndex].Sample(vertIn.outTexCoord);
    albedo *= texColor.rgb;
  }

  float3 finalColor = albedo;

  if (modelIndex != INVALID_INDEX)
  {
    finalColor = float3(0.0, 0.0, 0.0);
    float3 viewDir = normalize(camera.cameraPos - vertIn.outWorldPos);

    for (uint i = 0; i < sceneInfo.numDirectionalLights; ++i)
    {
      DirectionalLight light = directionalLights[i];
      float3 lightDir = normalize(-light.direction.xyz);

      finalColor +=
          simplePBR(vertIn.outNormal, viewDir, lightDir, albedo, mat.metallic, mat.roughness) *
          light.color.rgb * light.color.w;
    }

    for (uint i = 0; i < sceneInfo.numPointLights; ++i)
    {
      PointLight light = pointLights[i];
      float3 L = light.position.xyz - vertIn.outWorldPos;
      float distance = length(L);
      float3 lightDir = L / distance;

      float attenuation = 1.0 / (distance * distance);

      finalColor +=
          simplePBR(vertIn.outNormal, viewDir, lightDir, albedo, mat.metallic, mat.roughness) *
          light.color.rgb * light.color.w * attenuation;
    }

    for (uint i = 0; i < sceneInfo.numSpotLights; ++i)
    {
      SpotLight light = spotLights[i];
      float3 L = light.position.xyz - vertIn.outWorldPos;
      float distance = length(L);
      float3 lightDir = L / distance;

      float attenuation = 1.0 / (distance * distance);

      float theta = dot(-lightDir, normalize(light.direction.xyz));

      float intensity = smoothstep(light.direction.w, light.position.w, theta);

      finalColor +=
          simplePBR(vertIn.outNormal, viewDir, lightDir, albedo, mat.metallic, mat.roughness) *
          light.color.rgb * light.color.w * attenuation * intensity;
    }
  }

  return float4(finalColor, 1.0);
}
