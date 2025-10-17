#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include <opencv2/opencv.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/common.h"
#include "common/test_helpers.h"
#include "core/format_converters.h"
// #include "core/types.h"

// #include "backends/cpu/devicecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"
#include "backends/cpu/renderercpu.h"

// #ifdef COMPILE_GL
#include "backends/gl/devicegl_glad.h"
#include "backends/gl/texturegl.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/renderergl.h"
// #endif

#include <iostream>

static bool LoadAssimpMesh(const std::string &path,
                           std::vector<Eigen::Vector3f> &vertices,
                           std::vector<Eigen::Vector3f> &normals,
                           std::vector<Eigen::Vector2f> &texcoords,
                           std::vector<unsigned int> &indices)
{
    Assimp::Importer imp;
    const aiScene *scene = imp.ReadFile(
        path,
        aiProcess_Triangulate |
            // aiProcess_JoinIdenticalVertices |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            // aiProcess_ImproveCacheLocality |
            // aiProcess_OptimizeMeshes |
            aiProcess_FlipUVs);
    // if (!scene || !scene->HasMeshes())
    //{
    //     std::cerr << "Assimp error: " << imp.GetErrorString() << std::endl;
    //     return false;
    // }

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
    {
        std::cout << "ERROR::ASSIMP:: " << imp.GetErrorString() << std::endl;
        return false;
    }

    vertices.clear();
    normals.clear();
    texcoords.clear();
    indices.clear();

    size_t baseVertex = 0;
    Eigen::Vector3f minB(std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max());
    Eigen::Vector3f maxB(-std::numeric_limits<float>::max(),
                         -std::numeric_limits<float>::max(),
                         -std::numeric_limits<float>::max());

    // aiNode *root = scene->mRootNode;

    // for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    unsigned m = 0;
    {
        const aiMesh *mesh = scene->mMeshes[m];
        // vertices & uvs
        for (unsigned v = 0; v < mesh->mNumVertices; ++v)
        {
            aiVector3D p = mesh->mVertices[v];
            aiVector3D n = mesh->mNormals[v];

            Eigen::Vector3f vertice(p.x, p.y, -p.z);
            Eigen::Vector3f normal(n.x, n.y, n.z);

            // p.z = -p.z;
            vertices.push_back(vertice);
            normals.push_back(normal);

            minB = minB.cwiseMin(vertice);
            maxB = maxB.cwiseMax(vertice);

            if (mesh->HasTextureCoords(0))
            {
                aiVector3D t = mesh->mTextureCoords[0][v];
                texcoords.push_back(Eigen::Vector2f(t.x, t.y));
            }
            else
            {
                texcoords.push_back(Eigen::Vector2f(0.0f, 0.0f));
            }
        }
        // indices
        for (unsigned f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace &face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue;
            indices.push_back(static_cast<unsigned>(baseVertex) + face.mIndices[0]);
            indices.push_back(static_cast<unsigned>(baseVertex) + face.mIndices[1]);
            indices.push_back(static_cast<unsigned>(baseVertex) + face.mIndices[2]);
        }
        baseVertex += mesh->mNumVertices;
    }

    Eigen::Vector3f center = 0.5f * (minB + maxB);
    float radius = (maxB - center).norm();

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        vertices[i] -= center;
        vertices[i] /= radius;
    }

    return true;
}

static cv::Mat MakeCheckerTex(int w = 512, int h = 512, int checker = 32)
{
    cv::Mat tex(h, w, CV_8UC3);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            bool c = ((x / checker) + (y / checker)) & 1;
            tex.at<cv::Vec3b>(y, x) = c ? cv::Vec3b(40, 40, 40) : cv::Vec3b(220, 220, 220);
        }
    }
    return tex;
}

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 2.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;

// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // euler Angles
    float Yaw;
    float Pitch;
    // camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    {
        Position = glm::vec3(posX, posY, posZ);
        WorldUp = glm::vec3(upX, upY, upZ);
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;
        if (direction == FORWARD)
            Position += Front * velocity;
        if (direction == BACKWARD)
            Position -= Front * velocity;
        if (direction == LEFT)
            Position -= Right * velocity;
        if (direction == RIGHT)
            Position += Right * velocity;
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        // update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= (float)yoffset;
        if (Zoom < 1.0f)
            Zoom = 1.0f;
        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp)); // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

linalg::Mat4<float> GlmToLinalg(const glm::mat4 &m)
{
    linalg::Mat4<float> lm;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            lm(r, c) = m(r, c);
    return lm;
};

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

int main()
{
    if (!InitEGL())
    {
        std::cout << "Failed to initialize EGL" << std::endl;
        return -1;
    }

    std::vector<Eigen::Vector3f> vertices;
    std::vector<Eigen::Vector3f> normals;
    std::vector<Eigen::Vector2f> texcoords;
    std::vector<unsigned int> indices;

    if (!LoadAssimpMesh("/home/emanuel/workspace/",
                        vertices,
                        normals,
                        texcoords,
                        indices))
    {
        std::cout << "Failed to load model" << std::endl;
        return -1;
    }

    int width = 640;
    int height = 480;

    TextureGL<float> in_texture(width, height, 0);
    TextureGL<float> out_texture(width, height, 0);

    cv::Mat checker = MakeCheckerTex(width, height, 32);
    UploadMatToTexture(in_texture, 0, checker);

    MeshGL mesh(vertices, indices);

    SimpleExampleRendererGL renderer;

    cv::namedWindow("Simple Example", cv::WINDOW_AUTOSIZE);

    // render loop
    // -----------
    while (true)
    {
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)width / (float)height, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));     // it's a bit too big for our scene, so scale it down

        renderer.Render(mesh, GlmToLinalg(projection), GlmToLinalg(view), GlmToLinalg(model), 0, 0, in_texture, out_texture);

        cv::Mat out_f = DownloadTextureToMat(out_texture, 0, CV_32FC1);

        // Pretty up the single-channel output
        cv::Mat out_u8, out_color;
        out_f.convertTo(out_u8, CV_8U, 255.0);
        cv::applyColorMap(out_u8, out_color, cv::COLORMAP_TURBO);

        cv::imshow("Simple Example", out_color);
        cv::waitKey(30);
    }

    return 0;
}
