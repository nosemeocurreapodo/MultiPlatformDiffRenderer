#include <opencv2/opencv.hpp>

#define TEST_DATA_DIR "/home/emanuel/workspace/MultiPlatformRenderer/tests/data"

#include "loaddataset.h"
#include "core/format_converters.h"
#include "core/delaunaytriangulation.h"
#include "backends/xrt/typesfpga.h"
#include "backends/xrt/bufferfpga.h"
#include "backends/xrt/texturefpga.h"
#include "backends/xrt/meshfpga.h"
#include "backends/xrt/depthrendererfpga.h"

int main()
{
    LoadDatasetIclNuim<fpga::Vec3, fpga::Quaternion, fpga::SE3, fpga::Camera> dataset;
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<fpga::SE3> poses = dataset.GetPoses();
    std::vector<double> timestamps = dataset.GetTimestamps();
    fpga::Camera cam = dataset.GetCamera();
    int w = dataset.GetWidth();
    int h = dataset.GetHeight();

    int src = 0;
    int dst = 50;

    // int operation = cv::MORPH_CLOSE;
    // int morph_size = 5;
    // int morph_elem = cv::MORPH_ELLIPSE;

    // cv::Mat element = cv::getStructuringElement(morph_elem,
    //                                             cv::Size(2 * morph_size + 1, 2 * morph_size + 1),
    //                                             cv::Point(morph_size, morph_size));

    cv::Mat image_src_CV = cv::imread(image_files[src], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_src_CV = cv::imread(depth_files[src], cv::IMREAD_GRAYSCALE);
    // cv::morphologyEx(depth_src_CV, depth_src_CV, operation, element);
    fpga::SE3 pose_src = poses[src].inverse();

    cv::Mat image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
    // cv::morphologyEx(depth_dst_CV, depth_dst_CV, operation, element);
    fpga::SE3 pose_dst = poses[dst].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<fpga::ImageType>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<fpga::ImageType>(), 1));
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV /= dataset.GetDepthFactor();
    depth_dst_CV *= 100.0;

    std::vector<fpga::Vec2> tex_coords = UniformTexCoords<fpga::Vec2>(32, 32);
    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
    for (fpga::Vec2 tex_coord : tex_coords)
    {
        fpga::Vec2 img_coord = fpga::Vec2(tex_coord(0) * w, tex_coord(1) * h);
        float depth = depth_src_CV.at<float>(int(img_coord(1)), int(img_coord(0)));
        // float depth = VerticallySmoothDepth(tex_coord, 0.1f, 10.0f);
        if (depth <= 0.0f)
            continue;
        fpga::Vec3 ray = cam.PixToRay(tex_coord);
        fpga::Vec3 vertex = ray * depth;
        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(tex_coord(0));
        texcoords.push_back(tex_coord(1));
        weights.push_back(1.0f);
    }

    DelaunayTriangulation<fpga::Vec2, fpga::Vec2i, fpga::Vec3i> triangulator_;
    std::vector<fpga::Vec2> tex_coords_2d;
    for (size_t i = 0; i < texcoords.size(); i += 2)
    {
        tex_coords_2d.push_back(fpga::Vec2(texcoords[i], texcoords[i + 1]));
    }
    triangulator_.LoadPoints(tex_coords_2d);
    triangulator_.Triangulate();
    std::vector<fpga::Vec3i> tris = triangulator_.GetTriangles();
    std::vector<unsigned int> tris_f;
    for (size_t i = 0; i < tris.size(); i++)
    {
        tris_f.push_back(tris[i](0));
        tris_f.push_back(tris[i](1));
        tris_f.push_back(tris[i](2));
    }

    fpga::SE3 pose = pose_dst * pose_src.inverse();
    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    // MeshFPGA mesh(vertices, texcoords, weights);
    // DepthRendererFPGA renderer;
    // TextureFPGA<ImageType> image(w, h, 1, 0);
    // TextureFPGA<float> depth(w, h, 1, -1);
    // image.FromCPU((ImageType *)image_src_CV.data);
    // renderer.Render(mesh, pose, cam, image, depth, 0);
    // depth.ToCPU((float *)output_depthCV.data);

    DepthRenderFPGA((float *)vertices.data(), vertices.size(),
                    (float *)texcoords.data(), texcoords.size(),
                    (float *)weights.data(), weights.size(),
                    (unsigned int *)tris_f.data(), tris_f.size(),
                    pose.rotation().getQuaterion().x_, pose.rotation().getQuaterion().y_, pose.rotation().getQuaterion().z_, pose.rotation().getQuaterion().w_,
                    pose.translation()(0), pose.translation()(1), pose.translation()(2),
                    cam.GetParams()(0), cam.GetParams()(1), cam.GetParams()(2), cam.GetParams()(3),
                    (fpga::ImageType *)image_src_CV.data,
                    w, h, 1,
                    (float *)output_depthCV.data,
                    w, h, 1);

    float depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV);

    cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    // cv::threshold(output_depthCV, output_depthCV, 2.0, 2.0, cv::THRESH_TRUNC);
    cv::normalize(output_depthCV, output_depthCV, 0, 255, cv::NORM_MINMAX);
    // output_depthCV = output_depthCV * 255.0f;
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_depthCV.convertTo(output_depthCV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("depthrenderfpga_input.png", depth_dst_CV);
    cv::imwrite("depthrenderfpga_output.png", output_depthCV);
}