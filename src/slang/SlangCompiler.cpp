#include <fstream>
#include <slang/slang-com-helper.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>
#include <string_view>
#include <utility>

#include "vksim/slang/SlangCompiler.hpp"
#include "vksim/utility/Error.hpp"
#include "vksim/utility/Logging.hpp"

namespace vksim::compiler
{

SlangCompiler::SlangCompiler(std::optional<std::string> slangPath)
    : m_slangPath(std::move(slangPath))
{
  // Create the global Slang session
  createGlobalSession(globalSession.writeRef());

  // Set up the target description for SPIR-V
  slang::TargetDesc targetDesc = {.format = SLANG_SPIRV,
                                  .profile = globalSession->findProfile("spirv_1_5")};

  // Create the session description
  slang::SessionDesc sessionDesc = {.targets = &targetDesc, .targetCount = 1};
  const auto *searchPaths = m_slangPath->c_str();

  globalSession->createSession(sessionDesc, session.writeRef());
  if (m_slangPath.has_value())
  {
    sessionDesc.searchPathCount = 1;
    sessionDesc.searchPaths = &searchPaths;
  }
  // Create the Slang session for this compiler instance
  globalSession->createSession(sessionDesc, session.writeRef());
};

auto SlangCompiler::compileToSpirv(std::string_view filename, std::string_view moduleName,
                                   std::string_view entryPointName)
    -> std::expected<std::vector<char>, error::Error>
{
  // Read the shader source code from the specified file
  std::string fullPath;
  if (m_slangPath.has_value())
  {
    fullPath = m_slangPath.value() + "/" + std::string(filename);
  }
  else
  {
    fullPath = std::string(filename);
  }

  std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

  if (!file.is_open())
  {
    return std::unexpected(
        error::Error(error::ErrorCode::SlangCompilationFailed, "Failed to open shader file"));
  }

  std::vector<char> source(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(source.data(), static_cast<std::streamsize>(source.size()));
  file.close();

  // Compile the shader source code to SPIR-V using the Slang session
  Slang::ComPtr<slang::IModule> slangModule;
  {
    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    slangModule = session->loadModuleFromSourceString(moduleName.data(), filename.data(),
                                                      source.data(), diagnosticsBlob.writeRef());

    if (slangModule == nullptr)
    {
      return std::unexpected(
          error::Error(error::ErrorCode::SlangCompilationFailed, "Failed to load shader module"));
    }
  }

  // Query for entry point
  Slang::ComPtr<slang::IEntryPoint> entryPoint;
  {
    slangModule->findEntryPointByName(entryPointName.data(), entryPoint.writeRef());

    if (entryPoint == nullptr)
    {
      return std::unexpected(
          error::Error(error::ErrorCode::SlangCompilationFailed, "Failed to find entry point"));
    }
  }

  // Compose the program by linking the module and entry points together
  std::vector<slang::IComponentType *> componentTypes = {slangModule, entryPoint};

  Slang::ComPtr<slang::IComponentType> composedProgram;
  {
    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    SlangResult result = session->createCompositeComponentType(
        componentTypes.data(), componentTypes.size(), composedProgram.writeRef(),
        diagnosticsBlob.writeRef());

    if (SLANG_FAILED(result))
    {
      return std::unexpected(
          error::Error(error::ErrorCode::SlangCompilationFailed,
                       (diagnosticsBlob != nullptr)
                           ? static_cast<const char *>(diagnosticsBlob->getBufferPointer())
                           : "Failed to compose program"));
    }
  }

  // Link the composed program to generate the final SPIR-V binary
  Slang::ComPtr<slang::IComponentType> linkedProgram;
  {
    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    SlangResult result =
        composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
    if (SLANG_FAILED(result))
    {
      return std::unexpected(
          error::Error(error::ErrorCode::SlangCompilationFailed,
                       (diagnosticsBlob != nullptr)
                           ? static_cast<const char *>(diagnosticsBlob->getBufferPointer())
                           : "Failed to link program"));
    }
  }

  // Get the compiled SPIR-V code from the linked program
  Slang::ComPtr<slang::IBlob> spirvCode;
  {
    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    SlangResult result =
        linkedProgram->getEntryPointCode(0, // entryPointIndex
                                         0, // targetIndex
                                         spirvCode.writeRef(), diagnosticsBlob.writeRef());
    if (SLANG_FAILED(result))
    {
      return std::unexpected(
          error::Error(error::ErrorCode::SlangCompilationFailed,
                       (diagnosticsBlob != nullptr)
                           ? static_cast<const char *>(diagnosticsBlob->getBufferPointer())
                           : "Failed to get SPIR-V code"));
    }
  }

  // Convert the SPIR-V code blob to a vector of char and return it
  const auto *words = static_cast<const char *>(spirvCode->getBufferPointer());
  const size_t wordCount = spirvCode->getBufferSize() / sizeof(char);

  return std::vector<char>(reinterpret_cast<const char *>(words),
                           reinterpret_cast<const char *>(words) + (wordCount * sizeof(char)));
}

} // namespace vksim::compiler