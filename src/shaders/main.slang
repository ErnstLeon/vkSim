import common.types;
import stages.vertex;
import stages.fragment;

[[vk::binding(0, 0)]]
ConstantBuffer<SceneInfo> sceneInfo;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraBuffer> camera;

[[vk::binding(2, 0)]]
StructuredBuffer<DirectionalLight> directionalLights;

[[vk::binding(3, 0)]]
StructuredBuffer<PointLight> pointLights;

[[vk::binding(4, 0)]]
StructuredBuffer<SpotLight> spotLights;

[[vk::binding(0, 1)]]
Sampler2D texture[];

[[vk::binding(1, 1)]]
ConstantBuffer<MaterialBuffer> material[];

[[vk::push_constant]]
PushConstants pushConstants;

[shader("vertex")]
VSOutput vertMain(VSInput input, )
{
  return vertexShader(input, pushConstants.model, pushConstants.normalMatrix, camera.view,
                      camera.proj);
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
  return fragmentShader(vertIn, pushConstants.modelIndex, pushConstants.textureIndex, sceneInfo,
                        directionalLights, pointLights, spotLights, material, texture, camera);
}
