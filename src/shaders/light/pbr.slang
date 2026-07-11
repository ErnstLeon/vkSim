import common.types;

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float a)
{
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float nom = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
  float nom = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float k)
{
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx1 = GeometrySchlickGGX(NdotV, k);
  float ggx2 = GeometrySchlickGGX(NdotL, k);

  return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 simplePBR(float3 normal, float3 viewDir, float3 lightDir, float3 albedo, float metallic,
                 float roughness)
{
  // Simple pbr lighting model
  float3 N = normalize(normal);
  float3 V = normalize(viewDir);
  float3 L = normalize(lightDir);
  float3 H = normalize(V + L);

  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);

  float3 F0 = float3(0.04);
  F0 = lerp(F0, albedo, metallic);

  float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

  float a = roughness * roughness;
  float D = DistributionGGX(N, H, a);

  float k = ((roughness + 1.0) * (roughness + 1.0)) / 8.0;
  float G = GeometrySmith(N, V, L, k);

  float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);

  float3 kD = (1.0 - F) * (1.0 - metallic);
  float3 diffuse = kD * albedo / PI;

  float3 finalColor = (diffuse + specular) * NdotL * 10.0; // Multiply by light intensity
  return finalColor;
}
