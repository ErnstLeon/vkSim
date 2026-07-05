#pragma once

#include <expected>
#include <slang/slang-com-helper.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>
#include <string_view>
#include <vector>

#include "vksim/utility/Error.hpp"

namespace vksim::compiler
{

/**
 * @brief A wrapper around the Slang compiler to compile shader source code
 * to SPIR-V.
 */
class SlangCompiler
{
public:
  SlangCompiler();

  /** @brief Compiles the given shader source code to SPIR-V binary format.
   *
   * @param filename The name of the .slang file to compile.
   * @param moduleName The name of the shader module.
   * @param entryPointName The name of the entry point function in the
   * shader (default is "main").
   * @return A std::expected containing the compiled SPIR-V code as a
   * vector of char on success, or an Error::Error on failure.
   */
  [[nodiscard]] auto compileToSpirv(std::string_view filename, std::string_view moduleName,
                                    std::string_view entryPointName = "main")
      -> std::expected<std::vector<char>, error::Error>;

private:
  /** @brief The global Slang session */
  Slang::ComPtr<slang::IGlobalSession> globalSession;
  /** @brief The Slang session for this compiler instance */
  Slang::ComPtr<slang::ISession> session;
};

} // namespace vksim::compiler