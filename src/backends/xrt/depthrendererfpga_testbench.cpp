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
    LoadDatasetIclNuim dataset(std::string(TEST_DATA_DIR));
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<cpu::SE3> poses = dataset.GetPoses();
    std::vector<double> timestamps = dataset.GetTimestamps();
    cpu::Camera cam = dataset.GetCamera();
    unsigned int w = dataset.GetWidth();
    unsigned int h = dataset.GetHeight();

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
    cpu::SE3 pose_src = poses[src].inverse();

    cv::Mat image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
    // cv::morphologyEx(depth_dst_CV, depth_dst_CV, operation, element);
    cpu::SE3 pose_dst = poses[dst].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV /= dataset.GetDepthFactor();
    depth_dst_CV *= 100.0;

    std::vector<cpu::Vec2> tex_coords = UniformTexCoords(32, 32);

    DelaunayTriangulation triangulator_;
    triangulator_.LoadPoints(tex_coords);
    triangulator_.Triangulate();
    std::vector<cpu::Vec3i> tris = triangulator_.GetTriangles();

    cpu::SE3 pose = pose_dst * pose_src.inverse();

    std::vector<fpga::Scalar> texcoords;
    std::vector<fpga::Scalar> vertices;
    std::vector<fpga::Scalar> weights;
    for (cpu::Vec2 tex_coord : tex_coords)
    {
        cpu::Vec2 img_coord = cpu::Vec2(tex_coord(0) * w, tex_coord(1) * h);
        float depth = depth_src_CV.at<float>(int(img_coord(1)), int(img_coord(0)));
        // float depth = VerticallySmoothDepth(tex_coord, 0.1f, 10.0f);
        if (depth <= 0.0f)
            continue;
        cpu::Vec3 ray = cam.PixToRay(tex_coord);
        cpu::Vec3 vertex = ray * depth;

        fpga::Vec3 fpga_vertex = fpga::Vec3(vertex(0), vertex(1), vertex(2));
        fpga::Vec2 fpga_tex_coord = fpga::Vec2(tex_coord(0), tex_coord(1));

        vertices.push_back(fpga_vertex(0));
        vertices.push_back(fpga_vertex(1));
        vertices.push_back(fpga_vertex(2));
        texcoords.push_back(fpga_tex_coord(0));
        texcoords.push_back(fpga_tex_coord(1));
        weights.push_back(fpga::Scalar(1.0f));
    }

    std::vector<fpga::ImageType> input_image;
    input_image.reserve(w * h);
    std::vector<fpga::Scalar> output_image;
    output_image.reserve(w * h);

    std::vector<fpga::UInt> tris_f;
    for (size_t i = 0; i < tris.size(); i++)
    {
        tris_f.push_back(tris[i](0));
        tris_f.push_back(tris[i](1));
        tris_f.push_back(tris[i](2));
    }

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float value = image_src_CV.at<float>(y, x);
            fpga::ImageType fpga_value = fpga::ImageType(value);
            input_image[y * w + x] = fpga_value;
        }
    }

    // MeshFPGA mesh(vertices, texcoords, weights);
    // DepthRendererFPGA renderer;
    // TextureFPGA<ImageType> image(w, h, 1, 0);
    // TextureFPGA<float> depth(w, h, 1, -1);
    // image.FromCPU((ImageType *)image_src_CV.data);
    // renderer.Render(mesh, pose, cam, image, depth, 0);
    // depth.ToCPU((float *)output_depthCV.data);

    DepthRenderFPGA((fpga::Scalar *)vertices.data(),
                    (fpga::Scalar *)texcoords.data(),
                    (fpga::Scalar *)weights.data(),
                    (fpga::UInt *)tris_f.data(),
                    (fpga::ImageType *)input_image.data(),
                    (fpga::Scalar *)output_image.data(),
                    fpga::UInt(vertices.size()), fpga::UInt(texcoords.size()), fpga::UInt(weights.size()), fpga::UInt(tris_f.size()),
                    fpga::UInt(w), fpga::UInt(h), fpga::UInt(1), fpga::ImageType(0),
                    fpga::UInt(w), fpga::UInt(h), fpga::UInt(1), fpga::Scalar(-1.0f),
                    fpga::Scalar(pose.so3().unit_quaternion().x()), fpga::Scalar(pose.so3().unit_quaternion().y()), fpga::Scalar(pose.so3().unit_quaternion().z()), fpga::Scalar(pose.so3().unit_quaternion().w()),
                    fpga::Scalar(pose.translation()(0)), fpga::Scalar(pose.translation()(1)), fpga::Scalar(pose.translation()(2)),
                    fpga::Scalar(cam.GetParams()(0)), fpga::Scalar(cam.GetParams()(1)), fpga::Scalar(cam.GetParams()(2)), fpga::Scalar(cam.GetParams()(3)));

    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            fpga::Scalar fpga_value = output_image[y * w + x];
            float value = float(fpga_value);
            output_depthCV.at<float>(y, x) = value;
        }
    }

    double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, -1.0f);

    cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    // cv::threshold(output_depthCV, output_depthCV, 2.0, 2.0, cv::THRESH_TRUNC);
    cv::normalize(output_depthCV, output_depthCV, 0, 255, cv::NORM_MINMAX);
    // output_depthCV = output_depthCV * 255.0f;
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_depthCV.convertTo(output_depthCV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("depthrenderfpga_input.png", depth_dst_CV);
    cv::imwrite("depthrenderfpga_output.png", output_depthCV);

    std::cout << "Depth error: " << depthError << std::endl;
}
