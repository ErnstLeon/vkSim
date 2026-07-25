#pragma once

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

  std::vector<glm::vec3> c = {{{0.0F, 0.0F, 0.0F},
                               {1.0F, 0.0F, 0.0F},
                               {-1.0F, 0.0F, 0.0F},
                               {0.0F, 1.0F, 0.0F},
                               {0.0F, -1.0F, 0.0F},
                               {0.0F, 0.0F, 1.0F},
                               {0.0F, 0.0F, -1.0F},
                               {1.0F, 1.0F, 1.0F},
                               {-1.0F, -1.0F, -1.0F},
                               {1.0F, -1.0F, 1.0F},
                               {-1.0F, 1.0F, -1.0F},
                               {1.0F, 1.0F, -1.0F},
                               {-1.0F, -1.0F, 1.0F},
                               {1.0F, -1.0F, -1.0F},
                               {-1.0F, 1.0F, 1.0F}}};

  std::vector<float> w = {2.0F / 9.0F,  1.0F / 9.0F,  1.0F / 9.0F,  1.0F / 9.0F,  1.0F / 9.0F,
                          1.0F / 9.0F,  1.0F / 9.0F,  1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F,
                          1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F};
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

  std::vector<glm::vec3> c = {{{0.0F, 0.0F, 0.0F},
                               {1.0F, 0.0F, 0.0F},
                               {-1.0F, 0.0F, 0.0F},
                               {0.0F, 1.0F, 0.0F},
                               {0.0F, -1.0F, 0.0F},
                               {0.0F, 0.0F, 1.0F},
                               {0.0F, 0.0F, -1.0F},
                               {1.0F, 1.0F, 1.0F},
                               {-1.0F, -1.0F, -1.0F},
                               {1.0F, -1.0F, 1.0F},
                               {-1.0F, 1.0F, -1.0F},
                               {1.0F, 1.0F, -1.0F},
                               {-1.0F, -1.0F, 1.0F},
                               {1.0F, -1.0F, -1.0F},
                               {-1.0F, 1.0F, 1.0F},
                               {2.5f / cs, 2.5f / cs, 2.5f / cs},
                               {-2.5f / cs, -2.5f / cs, -2.5f / cs},
                               {2.5f / cs, -2.5f / cs, 2.5f / cs},
                               {-2.5f / cs, 2.5f / cs, -2.5f / cs}}};

  std::vector<float> w = {1.0F / 3.0F,  1.0F / 18.0F, 1.0F / 18.0F, 1.0F / 18.0F, 1.0F / 18.0F,
                          1.0F / 18.0F, 1.0F / 18.0F, 1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F,
                          1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F, 1.0F / 36.0F,
                          1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F, 1.0F / 72.0F};
};
} // namespace vksim::physics