// Storing: float3 gridMin, float gridSize, float3 gridMax
struct VoxelizationParameters
{
  float4 gridMin; // xyz: min, w: cell size
  float4 gridMax; // xyz: max, w: unused
  uint4 gridSize; // xyz: size, w: total number of voxels
};

[[vk::binding(0, 0)]]
ConstantBuffer<VoxelizationParameters> voxelizationParams;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint8_t> voxelGrid;

[[vk::binding(2, 0)]]
StructuredBuffer<float4> positionBuffers[];

[[vk::binding(3, 0)]]
StructuredBuffer<uint> indexBuffers[];

struct PushConstants
{
  float4x4 invModel;
  uint32_t objectIndex;
  uint32_t numFaces;
  uint32_t numSamples;
};

[[vk::push_constant]]
PushConstants pushConstants;

#define EPSILON 1e-6f

// Hash function for generating pseudo-random numbers
// Robert Jenkins' 32 bit integer hash function
// https://web.archive.org/web/20071123051617/http://www.concentric.net/~Ttwang/tech/inthash.htm
uint hash(uint a)
{
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

// Generate a pseudo-random float in the range [0, 1] using a hash function
float random(inout uint state)
{
  state = hash(state);
  // Normalize to [0, 1]
  // 4294967295.0 is 2^32 - 1, the maximum value of a 32-bit unsigned integer
  return float(state) / 4294967295.0;
}

// Function to check if a ray intersects a triangle
// See https://en.wikipedia.org/wiki/Möller–Trumbore_intersection_algorithm
bool intersects(float3 v0, float3 v1, float3 v2, float3 origin, float3 dir)
{
  // Möller–Trumbore intersection algorithm
  float3 edge1 = v1 - v0;
  float3 edge2 = v2 - v0;

  // Calculate the determinant of [-dir, edge1, edge2]
  float3 dir_cross_e2 = cross(dir, edge2);
  float det = dot(edge1, dir_cross_e2);

  // Ray is parallel to the triangle
  if (abs(det) < EPSILON)
    return false;

  float inv_det = 1.0 / det;

  // We are looking to the solution of the equation: origin + t * dir = v0 + u * edge1 + v * edge2
  // or [-dir, edge1, edge2] * [t, u, v]^T = origin - v0 using Cramer's rule to solve for t, u, v.
  float3 s = origin - v0;

  // Calculate u and check if the intersection point is outside the triangle
  float u = inv_det * dot(s, dir_cross_e2);
  if (u < 0.0 || u > 1.0)
    return false;

  // Calculate v and check if the intersection point is outside the triangle
  float3 q = cross(s, edge1);
  float v = inv_det * dot(dir, q);
  if (v < 0.0 || u + v > 1.0)
    return false;

  // Calculate t to find out where the intersection point is on the line
  float t = inv_det * dot(edge2, q);
  if (t > EPSILON) // Ray intersection
    return true;

  return false; // Line intersection but not a ray intersection
}

void singleSampleVoxel(uint3 voxelId)
{
  // Get the object parameters from push constants
  uint objectIndex = pushConstants.objectIndex;
  uint numFaces = pushConstants.numFaces;
  float4x4 invModelMatrix = pushConstants.invModel;

  // Calculate the linear index of the voxel in the voxel grid with x varying fastest, then y, then
  // z.
  uint linearIndex = voxelId.z * voxelizationParams.gridSize.x * voxelizationParams.gridSize.y +
                     voxelId.y * voxelizationParams.gridSize.x + voxelId.x;

  // Calculate the world space position of the voxel center (gridMin + (voxelId + 0.5) *
  // cellSize(=gridMin.w))
  float3 voxelCenter =
      voxelizationParams.gridMin.xyz + (float3(voxelId) + 0.5) * voxelizationParams.gridMin.w;

  // pick an arbitrary sampling direction
  float3 dir = float3(1.0, 0.0001, 0.0003);

  // Transform the voxel center and direction to object space using the inverse model matrix
  dir = normalize(mul(invModelMatrix, float4(dir, 0.0)).xyz);
  voxelCenter = mul(invModelMatrix, float4(voxelCenter, 1.0)).xyz;

  // check if the voxel is inside of the mesh.
  // if it is inside, then there should be an odd number of
  // intersections with the surrounding mesh
  // TO DO: Optimize by using ray-tracing acceleration structures and vk ray tracing extensions.
  // NOTE: Parallelizing this loop, e.g. using one warp per voxel and accumulating the
  // insideCount via WaveActiveSum, does not increase performance in tests. Perhaps because this
  // worsens memory access patterns extremely. Right now, each thread in a subgroup accesses the
  // same index and face buffers, which is very cache-friendly. Instead of loading the same data
  // for each voxel, this way, there is only one load per subgroup (e.g. 32 voxels), which is
  // much more efficient. (Shared memory to share vertixes not only between subgroup threads but
  // also between workgroups did not improve performance (increases barriers).)
  uint intersections = 0;
  for (int i = 0; i < numFaces; ++i)
  {
    float3 v0 = positionBuffers[objectIndex][indexBuffers[objectIndex][i * 3]].xyz;
    float3 v1 = positionBuffers[objectIndex][indexBuffers[objectIndex][i * 3 + 1]].xyz;
    float3 v2 = positionBuffers[objectIndex][indexBuffers[objectIndex][i * 3 + 2]].xyz;

    if (intersects(v0, v1, v2, voxelCenter, dir))
    {
      intersections += 1;
    }
  }

  // If the number of intersections is odd, then the voxel is inside the mesh and we mark it as
  // occupied in the voxel grid.
  if (intersections % 2 == 1)
  {
    voxelGrid[linearIndex] = 1; // Set the corresponding voxel to 1
  }
}

void multipleSampleVoxel(uint3 voxelId, uint numSamples)
{
  // Get the object parameters from push constants
  uint objectIndex = pushConstants.objectIndex;
  uint numFaces = pushConstants.numFaces;
  float4x4 invModelMatrix = pushConstants.invModel;

  // Calculate the linear index of the voxel in the voxel grid with x varying fastest, then y, then
  // z.
  uint linearIndex = voxelId.z * voxelizationParams.gridSize.x * voxelizationParams.gridSize.y +
                     voxelId.y * voxelizationParams.gridSize.x + voxelId.x;

  // Calculate the world space position of the voxel center (gridMin + (voxelId + 0.5) *
  // cellSize(=gridMin.w))
  float3 voxelCenter =
      voxelizationParams.gridMin.xyz + (float3(voxelId) + 0.5) * voxelizationParams.gridMin.w;

  // Transform the voxel center to object space using the inverse model matrix
  voxelCenter = mul(invModelMatrix, float4(voxelCenter, 1.0)).xyz;

  // Initialize a random state based on the linear index for reproducibility
  uint randomState = linearIndex;

  // Count how many samples are inside the mesh
  uint insideCount = 0;
  for (uint sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
  {
    // Generate a random direction for each sample
    float3 dir = normalize(float3(random(randomState), random(randomState), random(randomState)));

    // check if the sample is inside of the mesh.
    // if it is inside, then there should be an odd number of
    // intersections with the surrounding mesh
    // TO DO: Optimize by using ray-tracing acceleration structures and vk ray tracing extensions.
    // NOTE: Parallelizing this loop, e.g. using one warp per voxel and accumulating the
    // insideCount via WaveActiveSum, does not increase performance in tests. Perhaps because this
    // worsens memory access patterns extremely. Right now, each thread in a subgroup accesses the
    // same index and face buffers, which is very cache-friendly. Instead of loading the same data
    // for each voxel, this way, there is only one load per subgroup (e.g. 32 voxels), which is
    // much more efficient.
    uint intersections = 0;
    for (int i = 0; i < numFaces; ++i)
    {
      float3 v0 = positionBuffers[objectIndex][indexBuffers[objectIndex][i * 3]].xyz;
      float3 v1 = positionBuffers[objectIndex][indexBuffers[objectIndex][i * 3 + 1]].xyz;
      float3 v2 = positionBuffers[objectIndex][indexBuffers[objectIndex][i * 3 + 2]].xyz;

      if (intersects(v0, v1, v2, voxelCenter, dir))
      {
        intersections += 1;
      }
    }
    // If the number of intersections is odd, then the sample is inside the mesh
    if (intersections % 2 == 1)
    {
      insideCount += 1;
    }
  }

  // If the majority of samples are inside the mesh, mark the voxel as occupied
  if (insideCount > numSamples / 2)
  {
    voxelGrid[linearIndex] = 1; // Set the corresponding voxel to 1
  }
}

[numthreads(8, 8, 8)]
void main(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID,
          uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex)
{
  if (any(dispatchThreadID >= voxelizationParams.gridSize.xyz))
    return;

  if (pushConstants.numSamples == 1)
  {
    singleSampleVoxel(dispatchThreadID);
  }
  else
  {
    multipleSampleVoxel(dispatchThreadID, pushConstants.numSamples);
  }
}
