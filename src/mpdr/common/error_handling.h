#pragma once

#include "mpdr/common/renderer_exceptions.h"
#include "mpdr/common/render_constants.h"
#include <cmath>

/**
 * @file error_handling.h
 * @brief Utility functions for consistent error handling and validation across backends
 */

namespace ErrorHandling {

    /**
     * @brief Validates that a triangle area is not degenerate
     * @param area Triangle area to validate
     * @throws MeshException if triangle is degenerate
     */
    inline void ValidateTriangleArea(float area) {
        if (std::abs(area) < RenderConstants::MIN_TRIANGLE_AREA) {
            throw RendererExceptions::MeshException("Degenerate triangle detected (area too small)");
        }
    }

    /**
     * @brief Validates camera projection parameters
     * @param near_plane Near clipping plane distance
     * @param far_plane Far clipping plane distance
     * @throws CameraException if parameters are invalid
     */
    inline void ValidateCameraParameters(float near_plane, float far_plane) {
        if (near_plane <= 0.0f) {
            throw RendererExceptions::CameraException("Near plane must be positive");
        }
        if (far_plane <= near_plane) {
            throw RendererExceptions::CameraException("Far plane must be greater than near plane");
        }
    }

    /**
     * @brief Validates texture dimensions
     * @param width Texture width
     * @param height Texture height
     * @param level Mipmap level
     * @throws TextureException if dimensions are invalid
     */
    inline void ValidateTextureDimensions(int width, int height, int level = 0) {
        if (width <= 0 || height <= 0) {
            throw RendererExceptions::TextureException("Texture dimensions must be positive");
        }
        if (level < 0) {
            throw RendererExceptions::TextureException("Texture level must be non-negative");
        }
    }

    /**
     * @brief Validates viewport bounds
     * @param min_x Minimum x coordinate
     * @param max_x Maximum x coordinate
     * @param min_y Minimum y coordinate
     * @param max_y Maximum y coordinate
     * @throws InvalidParameterException if viewport is invalid
     */
    inline void ValidateViewport(int min_x, int max_x, int min_y, int max_y) {
        if (min_x >= max_x || min_y >= max_y) {
            throw RendererExceptions::InvalidParameterException("viewport", "Invalid viewport bounds");
        }
    }

    /**
     * @brief Validates array bounds access
     * @param index Index to validate
     * @param size Array size
     * @param array_name Name of the array for error reporting
     * @throws InvalidParameterException if index is out of bounds
     */
    inline void ValidateArrayBounds(size_t index, size_t size, const std::string& array_name) {
        if (index >= size) {
            throw RendererExceptions::InvalidParameterException(array_name, 
                "Index " + std::to_string(index) + " out of bounds (size: " + std::to_string(size) + ")");
        }
    }

    /**
     * @brief Validates that a pointer is not null
     * @param ptr Pointer to validate
     * @param ptr_name Name of the pointer for error reporting
     * @throws InvalidParameterException if pointer is null
     */
    template<typename T>
    inline void ValidateNotNull(const T* ptr, const std::string& ptr_name) {
        if (ptr == nullptr) {
            throw RendererExceptions::InvalidParameterException(ptr_name, "Null pointer");
        }
    }

    /**
     * @brief Validates division by zero
     * @param denominator Value to check
     * @param operation_name Name of the operation for error reporting
     * @throws InvalidParameterException if denominator is zero
     */
    inline void ValidateNonZero(float denominator, const std::string& operation_name) {
        if (std::abs(denominator) < RenderConstants::EPSILON) {
            throw RendererExceptions::InvalidParameterException(operation_name, "Division by zero");
        }
    }

} // namespace ErrorHandling
