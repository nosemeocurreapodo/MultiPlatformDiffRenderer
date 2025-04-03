#include <gtest/gtest.h>

#include <sstream>
#include <fstream>
#include <dirent.h>
#include <algorithm>
#include <iostream>
#include <chrono>

#include "opencv2/opencv.hpp"

#include "tests/common.h"
#include "src/common/dataMulti.h"

TEST(DataMulti, dataCPU)
{
    // load_dataset_tum_rgbd dataset;
    load_dataset_icl_nuim dataset;

    std::vector<std::string> image_files = dataset.getImageFiles();
    std::vector<std::string> depth_files = dataset.getDepthFiles();
    std::vector<SE3f> poses = dataset.getPoses();
    std::vector<double> timestamps = dataset.getTimestamps();
    cameraType cam = dataset.getCamera();
    int w = dataset.getWidth();
    int h = dataset.getHeight();

    for (unsigned int i = 0; i < image_files.size(); i++)
    {
        cv::Mat image = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
        cv::Mat gtDepth = cv::imread(depth_files[i], cv::IMREAD_GRAYSCALE);
        SE3f gtPose = poses[i].inverse();

        if (std::is_same<imageType, uchar>::value)
            image.convertTo(image, CV_8UC1);
        else if (std::is_same<imageType, int>::value)
            image.convertTo(image, CV_32SC1);
        else if (std::is_same<imageType, float>::value)
            image.convertTo(image, CV_32FC1);

        gtDepth.convertTo(gtDepth, CV_32FC1);
        gtDepth /= dataset.getDepthFactor();
        gtDepth *= 100.0;

        data<imageType> imageData(w, h, 0);
        imageData.set((imageType *)image.data);

        data<float> gtDepthData(w, h, 0);
        gtDepthData.set((float *)gtDepth.data);

        dataMipMap<float> gtMipMapDepthData(gtDepthData);

        int lvl = 0;

        if (i == 0)
        {
            kframe = keyFrameCPU(imageData, vec2f(0.0, 0.0), gtPose, 1.0);
            kframe.initGeometryFromDepth(gtDepthData, dataCPU<float>(w, h, 1.0 / mesh_vo::mapping_param_initial_var), cam);
            // kframe.initGeometryVerticallySmooth(cam);
        }
        else
        {
            SE3f gtLocalPose = kframe.globalPoseToLocal(gtPose);
            dataMipMap<float> estMipMapDepthData(w, h, 0);

            auto startTime = std::chrono::high_resolution_clock::now();
            renderer.render(kframe, gtLocalPose, estMipMapDepthData, cam, lvl);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

            float error = computeImageError(estMipMapDepthData.get(lvl), gtMipMapDepthData.get(lvl));

            accProcessingTime += std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            accError += error;
            framesProcessedCounter++;

            // show(gtMipMapDepthData.get(lvl), "gt depth");
            // show(estMipMapDepthData.get(lvl), "est depth");
        }
    }

    auto meanDuration = accProcessingTime.count() / framesProcessedCounter;
    float meanError = accError / framesProcessedCounter;
    std::cout << "Mean processing time " << meanDuration << " ms" << std::endl;
    std::cout << "Mean error " << meanError << " ms" << std::endl;

    // The test passes if the error is below the threshold
    EXPECT_LT(meanError, errorThreshold)
        << "mean error (" << meanError
        << ") exceeds the acceptable threshold (" << errorThreshold << ").";

    // EXPECT_LE(durationMs, acceptableTimeMs)
    //     << "mean time " << meanDuration << "ms, which exceeds the acceptable threshold of "
    //     << acceptableTimeMs << "ms.";
}