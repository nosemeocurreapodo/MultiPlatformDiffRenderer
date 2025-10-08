#pragma once

/**
 * @file render_constants.h
 * @brief Centralized rendering constants to eliminate magic numbers
 */

namespace RenderConstants {
    // Camera projection parameters
    constexpr float NEAR_PLANE = 0.001f;  ///< Near clipping plane distance
    constexpr float FAR_PLANE = 1000.0f;  ///< Far clipping plane distance
    
    // Numerical precision constants
    constexpr float EPSILON = 1e-8f;      ///< Small value for floating point comparisons
    constexpr float DEPTH_EPSILON = 1e-6f; ///< Epsilon for depth comparisons
    
    // Rendering thresholds
    constexpr float MIN_TRIANGLE_AREA = 1e-8f; ///< Minimum triangle area to avoid degenerate triangles
    
    // Pixel sampling constants
    constexpr float PIXEL_CENTER_OFFSET = 0.5f; ///< Offset to pixel center for sampling
}
