#include <gtest/gtest.h>

#include <sstream>
#include <fstream>
#include <dirent.h>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "opencv2/opencv.hpp"

#include "common/types.h"
#include "common/dataCPU.h"
//#include "common/dataGL.h"
#include "common.h"
// #include "src/common/dataMulti.h"

template <typename T>
std::type_index get_type_index()
{
    return std::type_index(typeid(T));
}

class DataLoader : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::vector<std::string> image_files = dataset.getImageFiles();
        std::vector<std::string> depth_files = dataset.getDepthFiles();
        std::vector<SE3f> poses = dataset.getPoses();
        std::vector<double> timestamps = dataset.getTimestamps();
        cameraType cam = dataset.getCamera();
        int w = dataset.getWidth();
        int h = dataset.getHeight();

        /*
        opencvTypeMap[get_type_index<float>()] = CV_32F;
        opencvTypeMap[get_type_index<int>()] = CV_32S;
        opencvTypeMap[get_type_index<unsigned char>()] = CV_8U;

        opencvTypeChannelMap[std::make_pair(CV_32F, 1)] = CV_32FC1;
        opencvTypeChannelMap[std::make_pair(CV_32F, 2)] = CV_32FC2;
        opencvTypeChannelMap[std::make_pair(CV_32F, 3)] = CV_32FC3;
        opencvTypeChannelMap[std::make_pair(CV_32F, 4)] = CV_32FC4;

        opencvTypeChannelMap[std::make_pair(CV_32S, 1)] = CV_32SC1;
        opencvTypeChannelMap[std::make_pair(CV_32S, 2)] = CV_32SC2;
        opencvTypeChannelMap[std::make_pair(CV_32S, 3)] = CV_32SC3;
        opencvTypeChannelMap[std::make_pair(CV_32S, 4)] = CV_32SC4;

        opencvTypeChannelMap[std::make_pair(CV_8U, 1)] = CV_8UC1;
        opencvTypeChannelMap[std::make_pair(CV_8U, 2)] = CV_8UC2;
        opencvTypeChannelMap[std::make_pair(CV_8U, 3)] = CV_8UC3;
        opencvTypeChannelMap[std::make_pair(CV_8U, 4)] = CV_8UC4;
        */

        opencvFormatMap[std::make_pair(get_type_index<float>(), 1)] = CV_32FC1;
        opencvFormatMap[std::make_pair(get_type_index<float>(), 2)] = CV_32FC2;
        opencvFormatMap[std::make_pair(get_type_index<float>(), 3)] = CV_32FC3;
        opencvFormatMap[std::make_pair(get_type_index<float>(), 4)] = CV_32FC4;

        opencvFormatMap[std::make_pair(get_type_index<int>(), 1)] = CV_32SC1;
        opencvFormatMap[std::make_pair(get_type_index<int>(), 2)] = CV_32SC2;
        opencvFormatMap[std::make_pair(get_type_index<int>(), 3)] = CV_32SC3;
        opencvFormatMap[std::make_pair(get_type_index<int>(), 4)] = CV_32SC4;

        opencvFormatMap[std::make_pair(get_type_index<unsigned char>(), 1)] = CV_8UC1;
        opencvFormatMap[std::make_pair(get_type_index<unsigned char>(), 2)] = CV_8UC2;
        opencvFormatMap[std::make_pair(get_type_index<unsigned char>(), 3)] = CV_8UC3;
        opencvFormatMap[std::make_pair(get_type_index<unsigned char>(), 4)] = CV_8UC4;
    }

    // load_dataset_tum_rgbd dataset;
    load_dataset_icl_nuim dataset;

    std::vector<std::string> image_files;
    std::vector<std::string> depth_files;
    std::vector<SE3f> poses;
    std::vector<double> timestamps;
    cameraType cam;
    int w;
    int h;

    //std::map<std::type_index, int> opencvTypeMap;
    //std::map<std::pair<int, int>, int> opencvTypeChannelMap;
    std::map<std::pair<std::type_index, int>, int> opencvFormatMap;
};

TEST_F(DataLoader, TestToFromOpenCV)
{
    int i = 0;
    cv::Mat imageCV = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
    cv::Mat depthCV = cv::imread(depth_files[i], cv::IMREAD_GRAYSCALE);
    // SE3f gtPose = poses[i].inverse();

    imageCV.convertTo(imageCV, opencvFormatMap[std::make_pair(get_type_index<imageType>(), 1)]);
    depthCV.convertTo(depthCV, opencvFormatMap[std::make_pair(get_type_index<float>(), 1)]);
    depthCV /= dataset.getDepthFactor();
    depthCV *= 100.0;

    dataCPU<imageType> imageCPU(w, h, 1, 0);
    imageCPU.set((imageType *)imageCV.data);

    dataCPU<float> depthCPU(w, h, 1, 0);
    depthCPU.set((float *)depthCV.data);

    cv::Mat output_imageCV = cv::Mat(h, w, opencvFormatMap[std::make_pair(get_type_index<imageType>(), 1)]);
    cv::Mat output_depthCV = cv::Mat(h, w, opencvFormatMap[std::make_pair(get_type_index<float>(), 1)]);

    imageCPU.toCPU((imageType *)output_imageCV.data);
    depthCPU.toCPU((float *)output_depthCV.data);

    float imageError = computeImageError<imageType>(imageCV, output_imageCV);
    float depthError = computeImageError<float>(depthCV, output_depthCV);

    ASSERT_EQ(imageError, 0.0f);
    ASSERT_EQ(depthError, 0.0f);
}
