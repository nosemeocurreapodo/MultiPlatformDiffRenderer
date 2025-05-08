#pragma once

#include <sstream>
#include <fstream>
#include <dirent.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <iostream>
#include <chrono>

// inline std::string &ltrim(std::string &s)
//{
//     s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not_fn([](int ch)
//                                                                     { return std::isspace(ch); })));
//     return s;
// }

inline std::string &ltrim(std::string &s)
{
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](unsigned char ch)
                         { return !std::isspace(ch); }));
    return s;
}

// inline std::string &rtrim(std::string &s)
//{
//     s.erase(std::find_if(s.rbegin(), s.rend(), std::not_fn([](int ch)
//                                                            { return std::isspace(ch); }))
//                 .base(),
//             s.end());
//     return s;
// }

inline std::string &rtrim(std::string &s)
{
    s.erase(
        std::find_if(s.rbegin(), s.rend(),
                     [](unsigned char ch)
                     { return !std::isspace(ch); })
            .base(),
        s.end());
    return s;
}

inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

inline int getdir(std::string dir, std::vector<std::string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(dir.c_str())) == NULL)
    {
        return -1;
    }

    while ((dirp = readdir(dp)) != NULL)
    {
        std::string name = std::string(dirp->d_name);

        if (name != "." && name != "..")
            files.push_back(name);
    }
    closedir(dp);

    std::sort(files.begin(), files.end());

    if (dir.at(dir.length() - 1) != '/')
        dir = dir + "/";
    for (unsigned int i = 0; i < files.size(); i++)
    {
        if (files[i].at(0) != '/')
            files[i] = dir + files[i];
    }

    return files.size();
}

inline int getFile(std::string source, std::vector<std::string> &files)
{
    std::ifstream f(source.c_str());

    if (f.good() && f.is_open())
    {
        while (!f.eof())
        {
            std::string l;
            std::getline(f, l);

            l = trim(l);

            if (l == "" || l[0] == '#')
                continue;

            files.push_back(l);
        }

        f.close();

        size_t sp = source.find_last_of('/');
        std::string prefix;
        if (sp == std::string::npos)
            prefix = "";
        else
            prefix = source.substr(0, sp);

        for (unsigned int i = 0; i < files.size(); i++)
        {
            if (files[i].at(0) != '/')
                files[i] = prefix + "/" + files[i];
        }

        return (int)files.size();
    }
    else
    {
        f.close();
        return -1;
    }
}

template <typename Type>
inline float ComputeImageError(const cv::Mat &image_est, const cv::Mat &image_gt)
{
    assert(image_est.cols == image_gt.cols && image_est.rows == image_gt.rows);

    float error = 0.0;
    int count = 0;
    for (int y = 0; y < image_est.rows; y++)
    {
        for (int x = 0; x < image_est.cols; x++)
        {
            float est = image_est.at<Type>(y, x);
            float gt = image_gt.at<Type>(y, x);

            error += (est - gt) * (est - gt);
            count += 1;
        }
    }
    return error / count;
}

// Function to compute error between two SE3 poses
template <typename SE3>
inline std::array<float, 2> ComputeSE3Error(const SE3 &pose_est, const SE3 &pose_gt)
{
    // Compute the relative transformation: error transformation T_error
    SE3 T_error = pose_est.inverse() * pose_gt;

    float translation_error = T_error.translation().norm();
    float rotation_error = 0.0; // T_error.so3().log().norm();

    std::array<float, 2> error = {translation_error, rotation_error};

    return error;

    // Convert T_error to a 6D vector (Lie algebra) representing the error
    // vec6f error_vector = T_error.log();

    // Return the norm of the error vector
    // return error_vector.norm();
}

template <typename SE3, typename Camera>
class LoadDatasetBase
{
public:
    LoadDatasetBase(float fx, float fy, float cx, float cy, int w, int h)
        : w_(w), h_(h), cam_(fx, fy, cx, cy, w, h)
    {
    }

    std::vector<std::string> GetImageFiles()
    {
        return image_files_;
    }

    std::vector<std::string> GetDepthFiles()
    {
        return depth_files_;
    }

    std::vector<SE3> GetPoses()
    {
        return poses_;
    }

    std::vector<double> GetTimestamps()
    {
        return time_stamps_;
    }

    Camera GetCamera()
    {
        return cam_;
    }

    int GetWidth()
    {
        return w_;
    }

    int GetHeight()
    {
        return h_;
    }

    float GetDepthFactor()
    {
        return depth_factor_;
    }

protected:
    SE3 GetClosestPose(const std::vector<SE3> &poses, const std::vector<double> &time_stamps, double target_timestamp)
    {
        SE3 closest_pose;
        double closest_diff = 10000000000.0;
        for (size_t i = 0; i < poses.size(); i++)
        {
            double time_stamp = time_stamps[i];
            SE3 pose = poses[i];

            double diff = std::abs(time_stamp - target_timestamp);
            if (diff < closest_diff)
            {
                closest_pose = pose;
                closest_diff = diff;
            }
        }
        return closest_pose;
    }

    int w_, h_;
    float depth_factor_;

    std::vector<std::string> image_files_;
    std::vector<std::string> depth_files_;
    std::vector<SE3> poses_;
    std::vector<double> time_stamps_;
    Camera cam_;
};

template <typename Vec3, typename Quaternion, typename SE3, typename Camera>
class LoadDatasetTumRgbd : public LoadDatasetBase<SE3, Camera>
{
public:
    LoadDatasetTumRgbd()
        : LoadDatasetBase<SE3, Camera>(525.0, 525.0, 319.5, 239.5, 640, 480) // for tum rgbd dataset
    {
        std::string dataset_path = std::string(TEST_DATA_DIR) + "/rgbd_dataset_freiburg1_floor_part";
        std::string image_path = "/rgb.txt";
        std::string depth_path = "/depth.txt";
        std::string pose_path = "/groundtruth.txt";

        this->depth_factor_ = 5000.0;

        // std::string poses_path = std::string(TEST_DATA_DIR) + dataset_path + pose_path;
        // poses = getPosesFromFile(poses_path);

        std::vector<std::string> image_file_paths;
        std::vector<double> image_timestamps;
        GetFilesAndTimestamps(dataset_path, image_path, image_file_paths, image_timestamps);

        std::vector<std::string> depth_file_paths;
        std::vector<double> depth_timestamps;
        GetFilesAndTimestamps(dataset_path, depth_path, depth_file_paths, depth_timestamps);

        std::vector<SE3> poses_list;
        std::vector<double> pose_timestamps;
        GetPosesAndTimestamps(dataset_path, pose_path, poses_list, pose_timestamps);

        std::vector<SE3> sync_poses;
        for (double timestamp : image_timestamps)
        {
            sync_poses.push_back(GetClosestPose(poses_list, pose_timestamps, timestamp));
        }

        this->image_files_ = image_file_paths;
        this->depth_files_ = depth_file_paths;
        this->time_stamps_ = image_timestamps;
        this->poses_ = sync_poses;
    }

private:
    inline int GetFilesAndTimestamps(std::string dir, std::string file, std::vector<std::string> &files, std::vector<double> &timestamps)
    {
        std::ifstream f((dir + file).c_str());

        if (f.good() && f.is_open())
        {
            std::vector<std::string> lines;

            while (!f.eof())
            {
                std::string l;
                std::getline(f, l);

                l = trim(l);

                if (l == "" || l[0] == '#')
                    continue;

                lines.push_back(l);
            }

            f.close();

            for (std::string line : lines)
            {
                std::stringstream ss(line);
                std::string timestamp_token;
                std::string filepath_token;
                std::getline(ss, timestamp_token, ' ');
                std::getline(ss, filepath_token, ' ');

                std::string file_path = dir + "/" + filepath_token;

                std::ifstream _f(file_path.c_str());

                if (_f.good())
                {
                    files.push_back(file_path);
                    timestamps.push_back(std::stod(timestamp_token));
                }
            }

            return files.size();
        }
        else
        {
            throw std::runtime_error("Failed to open file");
        }
    }

    int GetPosesAndTimestamps(std::string dir, std::string file, std::vector<SE3> &poses, std::vector<double> &timestamps)
    {
        std::ifstream f((dir + file).c_str());

        if (f.good() && f.is_open())
        {
            std::vector<std::string> lines;

            while (!f.eof())
            {
                std::string l;
                std::getline(f, l);

                l = trim(l);

                if (l == "" || l[0] == '#')
                    continue;

                lines.push_back(l);
            }

            f.close();

            for (std::string line : lines)
            {
                std::stringstream ss(line);
                std::string token;
                std::vector<double> values;
                while (std::getline(ss, token, ' '))
                {
                    values.push_back(std::stod(token));
                }

                SE3 pose;
                pose.setQuaternion(Quaternion(values[7], values[4], values[5], values[6]));
                pose.translation() = Vec3(values[1], values[2], values[3]);

                poses.push_back(pose);
                timestamps.push_back(values[0]);
            }

            return poses.size();
        }
        else
        {
            throw std::runtime_error("Failed to open file");
        }
    }
};

template <typename Vec3, typename Quaternion, typename SE3, typename Camera>
class LoadDatasetIclNuim : public LoadDatasetBase<SE3, Camera>
{
public:
    LoadDatasetIclNuim()
        : LoadDatasetBase<SE3, Camera>(481.20, -480.0, 319.5, 239.5, 640, 480)
    {
        std::string dataset_path = std::string(TEST_DATA_DIR) + "/traj3_frei_png_part";
        std::string assosiations_path = "/associations.txt";
        std::string pose_path = "/traj3.gt.freiburg";

        this->depth_factor_ = 5000.0;

        ReadAssociationsFile(dataset_path, assosiations_path, this->image_files_, this->depth_files_, this->time_stamps_);

        std::vector<SE3> pose_list;
        std::vector<double> poses_timestamps;
        GetPosesAndTimestamps(dataset_path, pose_path, pose_list, poses_timestamps);

        std::vector<SE3> sync_poses;
        for (double time_stamp : this->time_stamps_)
        {
            sync_poses.push_back(this->GetClosestPose(pose_list, poses_timestamps, time_stamp));
        }

        this->poses_ = sync_poses;
    }

private:
    int ReadAssociationsFile(std::string dir, std::string file, std::vector<std::string> &image_files, std::vector<std::string> &depth_files, std::vector<double> &timestamps)
    {
        std::ifstream f((dir + file).c_str());

        if (f.good() && f.is_open())
        {
            std::vector<std::string> lines;

            while (!f.eof())
            {
                std::string l;
                std::getline(f, l);

                l = trim(l);

                if (l == "" || l[0] == '#')
                    continue;

                lines.push_back(l);
            }

            f.close();

            for (std::string line : lines)
            {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> tokens;
                while (std::getline(ss, token, ' '))
                {
                    tokens.push_back(token);
                }

                double image_index = std::stod(tokens[0]);
                // double depth_index = std::stod(tokens[2]);
                std::string image_path = dir + "/" + tokens[3];
                std::string depth_path = dir + "/" + tokens[1];

                std::ifstream _f(image_path.c_str());
                std::ifstream __f(depth_path.c_str());

                if (_f.good() && __f.good())
                {
                    image_files.push_back(image_path);
                    depth_files.push_back(depth_path);
                    timestamps.push_back(image_index);
                }
            }

            return image_files.size();
        }
        else
        {
            throw std::runtime_error("Failed to open associations file");
        }
    }

    int GetPosesAndTimestamps(std::string dir, std::string file, std::vector<SE3> &poses, std::vector<double> &timestamps)
    {
        std::ifstream f((dir + file).c_str());

        if (f.good() && f.is_open())
        {
            std::vector<std::string> lines;

            while (!f.eof())
            {
                std::string l;
                std::getline(f, l);

                l = trim(l);

                if (l == "" || l[0] == '#')
                    continue;

                lines.push_back(l);
            }

            f.close();

            for (std::string line : lines)
            {
                std::stringstream ss(line);
                std::string token;
                std::vector<double> values;
                while (std::getline(ss, token, ' '))
                {
                    values.push_back(std::stod(token));
                }

                SE3 pose;
                pose.setQuaternion(Quaternion(values[7], values[4], values[5], values[6]));
                pose.translation() = Vec3(values[1], values[2], values[3]);

                poses.push_back(pose);
                timestamps.push_back(values[0]);
            }

            return poses.size();
        }
        else
        {
            throw std::runtime_error("Failed to open poses and timestamps file");
        }
    }
};

/*
void use_test_dataset()
{
    dataset_path = "/test";
    image_path = "/rgb";
    depth_path = "/depth";
    pose_path = "/poses";

    w = 640;
    h = 480;
    depthFactor = 10.0;

    float fx = 481.20;
    float fy = 480.0;
    float cx = 319.5;
    float cy = 239.5;

    cam = cameraType(fx, fy, cx, cy, w, h);

    std::string poses_path = std::string(TEST_DATA_DIR) + dataset_path + pose_path;
    std::vector<std::string> pose_files;
    getdir(poses_path, pose_files);
    for (std::string pose_file : pose_files)
    {
        SE3f pose = readPovRaypose(pose_file);
        poses.push_back(pose);
    }
}
*/
