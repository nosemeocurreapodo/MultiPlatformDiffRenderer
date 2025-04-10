#pragma once

class Renderer {
    public:
        virtual ~Renderer() = default;
        virtual void setSceneData(const Scene& scene) = 0; // Possibly upload data
        virtual void render(Image& output) = 0;
    };