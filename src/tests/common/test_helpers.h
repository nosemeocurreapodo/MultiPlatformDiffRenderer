#pragma once

#include <opencv2/opencv.hpp>
#include "core/cv_converters.h"

// Save debug images
inline void SaveDebugImage(const cv::Mat &image, const std::string &filename)
{
    cv::Mat normalized;
    cv::normalize(image, normalized, 0, 255, cv::NORM_MINMAX);
    normalized.convertTo(normalized, CV_8U);
    cv::imwrite(filename, normalized);
}

// Helper functions for texture operations
template <typename Texture>
inline void UploadMatToTexture(Texture &tex, int lvl, cv::Mat &mat)
{
    //int cv_type = GetOpenCVFormat(tex.get_type_index());
    int cv_type = tex.getOpenCVType();
    mat.convertTo(mat, cv_type, 1.0); // / 255.0);

    assert(tex.width(lvl) == mat.cols && tex.height(lvl) == mat.rows);
    
    {
        auto mapped = tex.MapWrite(lvl);
        std::memcpy(mapped.data(), mat.ptr(), mat.total() * tex.type_size());
    }

    tex.generate_mipmaps(lvl);
}

template <typename Texture>
inline cv::Mat DownloadTextureToMat(const Texture &tex, int lvl)
{
    //int cv_type = GetOpenCVFormat(tex.get_type_index());
    int cv_type = tex.getOpenCVType();

    cv::Mat result(tex.height(lvl), tex.width(lvl), cv_type);

    {
        auto mapped = tex.MapRead(lvl);
        std::memcpy(result.ptr(), mapped.data(), tex.height(lvl) * tex.width(lvl) * tex.type_size());
        /*
        for (int y = 0; y < tex.height(lvl); y++)
        {
            for (int x = 0; x < tex.width(lvl); x++)
            {
                    unsigned char data = mapped[y * tex.width(lvl) + x];
                    result.at<unsigned char>(y, x) = data;
            }
        }
        */
    }

    return result;
}

// Mipmap level dimension calculation
inline int GetLevelDim(int base_dim, int level)
{
    return std::max(1, base_dim >> level);
}

// Error computation
template <typename Texture1, typename Texture2>
inline double RMSE(const Texture1 &tex1, const Texture2 &tex2, int lvl)
{
    assert(tex1.width(lvl) == tex2.width(lvl) && tex1.height(lvl) == tex2.height(lvl));

    double total_error = 0.0;
    int valid_pixels = 0;

    auto tx1_map = tex1.MapRead(lvl);
    auto tx2_map = tex2.MapRead(lvl);

    for (int i = 0; i < tx1_map.size(); ++i)
    {
        auto val1 = tx1_map[i];
        auto val2 = tx2_map[i];

        if (val1 == tex1.nodata() || val2 == tex2.nodata())
        {
            continue;
        }

        double diff = double(val1) - double(val2);
        double diff_sq = diff * diff;
        total_error += diff_sq;

        valid_pixels++;
    }

    return valid_pixels > 0 ? std::sqrt(total_error / valid_pixels) : 0.0;
}

template <typename Texture1, typename Texture2>
inline double RMSEV(const Texture1 &tex1, const Texture2 &tex2, int lvl)
{
    assert(tex1.width(lvl) == tex2.width(lvl) && tex1.height(lvl) == tex2.height(lvl));

    double total_error = 0.0;
    int valid_pixels = 0;

    auto tx1_map = tex1.MapRead(lvl);
    auto tx2_map = tex2.MapRead(lvl);

    for (int i = 0; i < tx1_map.size(); ++i)
    {
        auto val1 = tx1_map[i];
        auto val2 = tx2_map[i];

        if (val1 == tex1.nodata() || val2 == tex2.nodata())
        {
            continue;
        }

        auto diff = val1 - val2;
        auto diff_sq = diff.transpose() * diff;
        total_error += std::sqrt(diff_sq(0, 0));

        valid_pixels++;
    }

    return valid_pixels > 0 ? std::sqrt(total_error / valid_pixels) : 0.0;
}

template <typename Texture>
inline int CountValid(const Texture &tex, int lvl)
{
    int valid_pixels = 0;
    auto tx_map = tex.MapRead(lvl);

    for (int i = 0; i < tx_map.size(); ++i)
    {
        auto val = tx_map[i];
        if (val == tex.nodata())
        {
            continue;
        }

        valid_pixels++;
    }

    return valid_pixels;
}
