#pragma once

#include <opencv2/opencv.hpp>

inline cv::Mat ReadMat(const std::string &filename, bool to_float)
{
    cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);
    if (to_float)
        image.convertTo(image, CV_32FC1);
    return image;
}

// Save debug images
inline void SaveDebugImage(const cv::Mat &image, const std::string &filename)
{
    cv::Mat normalized;
    cv::normalize(image, normalized, 0, 255, cv::NORM_MINMAX);
    normalized.convertTo(normalized, CV_8UC1);
    cv::imwrite(filename, normalized);
}

inline void SaveDebugImageColor(const cv::Mat &image, const std::string &filename)
{
    cv::Mat normalized;
    cv::normalize(image, normalized, 0, 255, cv::NORM_MINMAX);
    normalized.convertTo(normalized, CV_8UC3);
    cv::imwrite(filename, normalized);
}

// Helper functions for texture operations
template <typename Texture>
inline void UploadMatToTexture(Texture &tex, int lvl, const cv::Mat &mat)
{
    assert(tex.width(lvl) == mat.cols && tex.height(lvl) == mat.rows);
    {
        auto mapped = tex.MapWrite(lvl);
        std::memcpy(mapped.data(), mat.ptr(), mat.total() * tex.type_size());
    }
    tex.generate_mipmaps(lvl);
}

template <typename Texture>
inline cv::Mat DownloadTextureToMat(const Texture &tex, int lvl, int cv_type)
{
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

        auto diff = val1 - val2;
        total_error += diff * diff;

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

// Error computation
template <typename T>
inline double ComputeL2ErrorScalar(const cv::Mat &mat1, const cv::Mat &mat2, T nodata_value)
{
    assert(mat1.size() == mat2.size());
    assert(mat1.type() == mat2.type());

    double total_error = 0.0;
    int valid_pixels = 0;

    for (int y = 0; y < mat1.rows; ++y)
    {
        for (int x = 0; x < mat1.cols; ++x)
        {
            const T val1 = mat1.at<T>(y, x);
            const T val2 = mat2.at<T>(y, x);

            if (val1 != nodata_value && val2 != nodata_value)
            {
                double diff = static_cast<double>(val1) - static_cast<double>(val2);

                total_error += diff * diff;
                valid_pixels++;

                // if (diff != 0.0f)
                //  {
                //     std::cout << "error " << std::endl;
                // }
            }
        }
    }

    return valid_pixels > 0 ? std::sqrt(total_error / valid_pixels) : 0.0;
}

inline double ComputeL2ErrorVector(const cv::Mat &mat1, const cv::Mat &mat2, cv::Vec3f nodata_value)
{
    assert(mat1.size() == mat2.size());
    assert(mat1.type() == mat2.type());

    double total_error = 0.0;
    int valid_pixels = 0;

    for (int y = 0; y < mat1.rows; ++y)
    {
        for (int x = 0; x < mat1.cols; ++x)
        {
            const cv::Vec3f val1 = mat1.at<cv::Vec3f>(y, x);
            const cv::Vec3f val2 = mat2.at<cv::Vec3f>(y, x);

            if (val1 != nodata_value && val2 != nodata_value)
            {
                // Handle vector types like cv::Vec3f
                auto diff = val1 - val2;
                double l2_error = 0.0;
                for (int i = 0; i < diff.channels; ++i)
                {
                    double d = static_cast<double>(diff[i]);
                    l2_error += d * d;
                }
                l2_error = std::sqrt(l2_error);
                total_error += l2_error;
                valid_pixels++;
            }
        }
    }

    return valid_pixels > 0 ? std::sqrt(total_error / valid_pixels) : 0.0;
}