#include <chrono>

#include <opencv2/opencv.hpp>

#include "loaddataset.h"
#include "core/format_converters.h"
#include "backends/cpu/typescpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/xrt/devicexrt.h"
#include "backends/xrt/rendererxrt.h"

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 4)
    {
        std::cout << "please provide: xclbin_file device_id dataset_path" << std::endl;
        return 1;
    }

    // Read settings
    std::string xclbin_file = argv[1];
    int device_index = atoi(argv[2]);
    std::string dataset_path = argv[3];

    LoadDatasetIclNuim dataset(dataset_path);
    // LoadDatasetTumRgbd dataset;

    std::vector<std::string> image_files = dataset.GetImageFiles();
    std::vector<std::string> depth_files = dataset.GetDepthFiles();
    std::vector<cpu::SE3> poses = dataset.GetPoses();
    std::vector<double> timestamps = dataset.GetTimestamps();
    cpu::Camera cam = dataset.GetCamera();
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
    cpu::SE3 pose_src = poses[src].inverse();

    image_src_CV.convertTo(image_src_CV, GetOpenCVFormat(GetTypeIndex<cpu::ImageType>(), 1));
    depth_src_CV.convertTo(depth_src_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_src_CV /= dataset.GetDepthFactor();
    depth_src_CV *= 100.0;

    image_dst_CV.convertTo(image_dst_CV, GetOpenCVFormat(GetTypeIndex<cpu::ImageType>(), 1));
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<float>(), 1));
    depth_dst_CV /= dataset.GetDepthFactor();
    depth_dst_CV *= 100.0;

    std::vector<cpu::Vec2> tex_coords = UniformTexCoords<cpu::Vec2>(32, 32);
    std::vector<float> texcoords;
    std::vector<float> vertices;
    std::vector<float> weights;
    for (cpu::Vec2 tex_coord : tex_coords)
    {
        cpu::Vec2 img_coord = cpu::Vec2(tex_coord(0) * (w - 1), tex_coord(1) * (h - 1));
        float depth = depth_src_CV.at<float>(int(img_coord(1)), int(img_coord(0)));
        // float depth = VerticallySmoothDepth(tex_coord, 0.1f, 10.0f);
        if (depth <= 0.0f)
            continue;
        cpu::Vec3 ray = cam.PixToRay(tex_coord);
        cpu::Vec3 vertex = ray * depth;
        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(tex_coord(0));
        texcoords.push_back(tex_coord(1));
        weights.push_back(1.0f);
    }

    cv::Mat output_depthCV = cv::Mat(h, w, GetOpenCVFormat(GetTypeIndex<float>(), 1));

    if (!InitXRT(xclbin_file, device_index))
    {
        std::cout << "Error initializing xrt backend!" << std::endl;
        return 1;
    }

    DepthRendererXRT renderer;

    MeshCPU mesh(vertices, texcoords, weights);
    TextureCPU<cpu::ImageType> image(w, h, 1, 0);
    TextureCPU<float> depth(w, h, 1, -1);
    image.FromCPU((cpu::ImageType *)image_src_CV.data);
    renderer.WriteMesh(mesh);
    renderer.WriteInTexture(image);
    renderer.PrepareOutTexture(depth);

    for (int i = 0; i < dst; i++)
    {
        cpu::SE3 pose = (poses[i].inverse()) * pose_src.inverse();

        auto start = std::chrono::steady_clock::now();
        renderer.Render(pose, cam, 0);
        auto end = std::chrono::steady_clock::now();
        std::cout << "Render time (ms)=" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;

        start = std::chrono::steady_clock::now();
        renderer.ReadOutTexture(depth);
        end = std::chrono::steady_clock::now();
        std::cout << "Read frame time (ms)=" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
    }

    depth.ToCPU((float *)output_depthCV.data);

    cv::Mat image_dst_CV = cv::imread(image_files[dst], cv::IMREAD_GRAYSCALE);
    cv::Mat depth_dst_CV = cv::imread(depth_files[dst], cv::IMREAD_GRAYSCALE);
    // cv::morphologyEx(depth_dst_CV, depth_dst_CV, operation, element);
    cpu::SE3 pose_dst = poses[dst].inverse();

    double depthError = ComputeImageError<float>(depth_dst_CV, output_depthCV, depth.nodata());

    cv::normalize(depth_dst_CV, depth_dst_CV, 0, 255, cv::NORM_MINMAX);
    // cv::threshold(output_depthCV, output_depthCV, 2.0, 2.0, cv::THRESH_TRUNC);
    cv::normalize(output_depthCV, output_depthCV, 0, 255, cv::NORM_MINMAX);
    // output_depthCV = output_depthCV * 255.0f;
    depth_dst_CV.convertTo(depth_dst_CV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    output_depthCV.convertTo(output_depthCV, GetOpenCVFormat(GetTypeIndex<uchar>(), 1));
    cv::imwrite("depthrenderxrt_input.png", depth_dst_CV);
    cv::imwrite("depthrenderxrt_output.png", output_depthCV);

    std::cout << "Depth error: " << depthError << std::endl;

    return 0;
}
