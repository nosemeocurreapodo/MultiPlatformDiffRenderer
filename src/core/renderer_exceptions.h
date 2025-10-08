#pragma once

#include <stdexcept>
#include <string>

/**
 * @file renderer_exceptions.h
 * @brief Standardized exception classes for consistent error handling across all renderer backends
 */

namespace RendererExceptions {

    /**
     * @brief Base exception class for all renderer-related errors
     */
    class RendererException : public std::runtime_error {
    public:
        explicit RendererException(const std::string& message) 
            : std::runtime_error("Renderer Error: " + message) {}
    };

    /**
     * @brief Exception for invalid input parameters
     */
    class InvalidParameterException : public RendererException {
    public:
        explicit InvalidParameterException(const std::string& parameter_name, const std::string& details = "") 
            : RendererException("Invalid parameter '" + parameter_name + "'" + 
                              (details.empty() ? "" : ": " + details)) {}
    };

    /**
     * @brief Exception for resource allocation failures
     */
    class ResourceException : public RendererException {
    public:
        explicit ResourceException(const std::string& resource_type, const std::string& details = "") 
            : RendererException("Resource error with " + resource_type + 
                              (details.empty() ? "" : ": " + details)) {}
    };

    /**
     * @brief Exception for OpenGL-specific errors
     */
    class OpenGLException : public RendererException {
    public:
        explicit OpenGLException(const std::string& operation, int gl_error_code = 0) 
            : RendererException("OpenGL error in " + operation + 
                              (gl_error_code != 0 ? " (code: " + std::to_string(gl_error_code) + ")" : "")) {}
    };

    /**
     * @brief Exception for XRT/FPGA-specific errors
     */
    class XRTException : public RendererException {
    public:
        explicit XRTException(const std::string& operation, const std::string& details = "") 
            : RendererException("XRT error in " + operation + 
                              (details.empty() ? "" : ": " + details)) {}
    };

    /**
     * @brief Exception for mesh-related errors (invalid geometry, etc.)
     */
    class MeshException : public RendererException {
    public:
        explicit MeshException(const std::string& details) 
            : RendererException("Mesh error: " + details) {}
    };

    /**
     * @brief Exception for texture-related errors
     */
    class TextureException : public RendererException {
    public:
        explicit TextureException(const std::string& details) 
            : RendererException("Texture error: " + details) {}
    };

    /**
     * @brief Exception for camera/projection matrix errors
     */
    class CameraException : public RendererException {
    public:
        explicit CameraException(const std::string& details) 
            : RendererException("Camera error: " + details) {}
    };

} // namespace RendererExceptions
