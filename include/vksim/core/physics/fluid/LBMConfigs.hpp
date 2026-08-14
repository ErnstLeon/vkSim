#pragma once

#include "glm/ext/vector_float4_precision.hpp"
#include <cstdint>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/hash.hpp>
#include <numbers>

namespace vksim::physics
{

/** @brief Structure representing the configuration for a D3Q15 Lattice Boltzmann Method (LBM)
 * simulation. This structure encapsulates the properties and behaviors of a D3Q15 LBM simulation,
 * including the lattice dimensions, number of discrete velocity directions, and the discrete
 * velocity vectors and weights.
 */
struct D3Q15
{
  uint32_t Dims = 3;
  uint32_t Q = 15;

  // Discrete velocity vectors and weights for D3Q15 LBM simulation.
  // The discrete velocity vectors are defined in a 4D vector format (x, y, z, w), where w
  // represents the weight associated with each velocity direction.
  std::vector<glm::vec4> cw = {{{0.0F, 0.0F, 0.0F, 2.0F / 9.0F},
                                {1.0F, 0.0F, 0.0F, 1.0F / 9.0F},
                                {-1.0F, 0.0F, 0.0F, 1.0F / 9.0F},
                                {0.0F, 1.0F, 0.0F, 1.0F / 9.0F},
                                {0.0F, -1.0F, 0.0F, 1.0F / 9.0F},
                                {0.0F, 0.0F, 1.0F, 1.0F / 9.0F},
                                {0.0F, 0.0F, -1.0F, 1.0F / 9.0F},
                                {1.0F, 1.0F, 1.0F, 1.0F / 72.0F},
                                {-1.0F, -1.0F, -1.0F, 1.0F / 72.0F},
                                {1.0F, -1.0F, 1.0F, 1.0F / 72.0F},
                                {-1.0F, 1.0F, -1.0F, 1.0F / 72.0F},
                                {1.0F, 1.0F, -1.0F, 1.0F / 72.0F},
                                {-1.0F, -1.0F, 1.0F, 1.0F / 72.0F},
                                {1.0F, -1.0F, -1.0F, 1.0F / 72.0F},
                                {-1.0F, 1.0F, 1.0F, 1.0F / 72.0F}}};

  std::vector<uint32_t> opposite = {0, 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13};
};

/** @brief Structure representing the configuration for a D3Q19 Lattice Boltzmann Method (LBM)
 * simulation. This structure encapsulates the properties and behaviors of a D3Q19 LBM simulation,
 * including the lattice dimensions, number of discrete velocity directions, and the discrete
 * velocity vectors and weights.
 */
struct D3Q19
{
  uint32_t Dims = 3;
  uint32_t Q = 19;

  float cs = std::numbers::inv_sqrt3_v<float>;

  // Discrete velocity vectors and weights for D3Q19 LBM simulation. The first three components of
  // each glm::vec4 represent the discrete velocity vector, and the fourth component represents the
  // corresponding weight for that velocity direction.
  std::vector<glm::vec4> cw = {{{0.0F, 0.0F, 0.0F, 1.0F / 3.0F},
                                {1.0F, 0.0F, 0.0F, 1.0F / 18.0F},
                                {-1.0F, 0.0F, 0.0F, 1.0F / 18.0F},
                                {0.0F, 1.0F, 0.0F, 1.0F / 18.0F},
                                {0.0F, -1.0F, 0.0F, 1.0F / 18.0F},
                                {0.0F, 0.0F, 1.0F, 1.0F / 18.0F},
                                {0.0F, 0.0F, -1.0F, 1.0F / 18.0F},
                                {1.0F, 1.0F, 1.0F, 1.0F / 36.0F},
                                {-1.0F, -1.0F, -1.0F, 1.0F / 36.0F},
                                {1.0F, -1.0F, 1.0F, 1.0F / 36.0F},
                                {-1.0F, 1.0F, -1.0F, 1.0F / 36.0F},
                                {1.0F, 1.0F, -1.0F, 1.0F / 36.0F},
                                {-1.0F, -1.0F, 1.0F, 1.0F / 36.0F},
                                {1.0F, -1.0F, -1.0F, 1.0F / 36.0F},
                                {-1.0F, 1.0F, 1.0F, 1.0F / 36.0F},
                                {2.5f / cs, 2.5f / cs, 2.5f / cs, 1.0F / 72.0F},
                                {-2.5f / cs, -2.5f / cs, -2.5f / cs, 1.0F / 72.0F},
                                {2.5f / cs, -2.5f / cs, 2.5f / cs, 1.0F / 72.0F},
                                {-2.5f / cs, 2.5f / cs, -2.5f / cs, 1.0F / 72.0F}}};

  std::vector<uint32_t> opposite = {0, 2,  1,  4,  3,  6,  5,  8,  7, 10,
                                    9, 12, 11, 14, 13, 16, 15, 18, 17};
};
} // namespace vksim::physics