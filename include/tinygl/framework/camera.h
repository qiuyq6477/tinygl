#pragma once

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <tinygl/base/tmath.h>

namespace tinygl {

struct CameraCreateInfo {
    Vec4 position = Vec4(0.0f, 0.0f, 3.0f, 1.0f);
    Vec4 up = Vec4(0.0f, 1.0f, 0.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    
    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;
    float zoomSpeed = 1.0f;
    float fov = 45.0f;
    float aspect = 1.33f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float pivotDistance = 3.0f;
};

class Camera {
public:
    // Camera Attributes
    Vec4 position;
    Vec4 front;
    Vec4 up;
    Vec4 right;
    Vec4 worldUp;

    // Euler Angles
    float yaw;
    float pitch;

    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoomSpeed;
    float fov;
    float aspect;
    float zNear;
    float zFar;

    // Orbit/Pivot
    float pivotDistance;

    // Input State
    bool isRMBDown = false;
    bool isMMBDown = false;
    bool isLMBDown = false;
    bool isAltDown = false;
    bool isShiftDown = false;

    // Movement Keys
    bool k_w = false, k_s = false;
    bool k_a = false, k_d = false;
    bool k_q = false, k_e = false;

    explicit Camera(const CameraCreateInfo& info = {}) 
        : front(Vec4(0.0f, 0.0f, -1.0f, 0.0f)),
          movementSpeed(info.movementSpeed),
          mouseSensitivity(info.mouseSensitivity),
          zoomSpeed(info.zoomSpeed),
          fov(info.fov),
          aspect(info.aspect),
          zNear(info.zNear),
          zFar(info.zFar),
          pivotDistance(info.pivotDistance)
    {
        this->position = info.position;
        this->worldUp = info.up;
        this->yaw = info.yaw;
        this->pitch = info.pitch;
        updateCameraVectors();
    }

    Mat4 GetViewMatrix() {
        return Mat4::LookAt(position, position + front, up);
    }

    Mat4 GetProjectionMatrix() {
        return Mat4::Perspective(fov, aspect, zNear, zFar);
    }

    void ProcessEvent(const SDL_Event& event) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool down = (event.type == SDL_KEYDOWN);
            switch(event.key.keysym.sym) {
                case SDLK_w: k_w = down; break;
                case SDLK_s: k_s = down; break;
                case SDLK_a: k_a = down; break;
                case SDLK_d: k_d = down; break;
                case SDLK_q: k_q = down; break;
                case SDLK_e: k_e = down; break;
                case SDLK_LSHIFT: case SDLK_RSHIFT: isShiftDown = down; break;
                case SDLK_LALT: case SDLK_RALT: isAltDown = down; break;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) isLMBDown = true;
            if (event.button.button == SDL_BUTTON_RIGHT) { 
                isRMBDown = true; 
                SDL_SetRelativeMouseMode(SDL_TRUE); 
            }
            if (event.button.button == SDL_BUTTON_MIDDLE) isMMBDown = true;
        }
        else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) isLMBDown = false;
            if (event.button.button == SDL_BUTTON_RIGHT) { 
                isRMBDown = false; 
                SDL_SetRelativeMouseMode(SDL_FALSE); 
            }
            if (event.button.button == SDL_BUTTON_MIDDLE) isMMBDown = false;
        }
        else if (event.type == SDL_MOUSEMOTION) {
            float xoffset = (float)event.motion.xrel;
            float yoffset = (float)event.motion.yrel;
            
            float speedFactor = isShiftDown ? 2.5f : 1.0f;

            // 2. Orbit (Alt + Left)
            if (isAltDown && isLMBDown) {
                // Determine pivot point
                Vec4 currentPivot = position + front * pivotDistance;
                
                // Orbit logic: invert x control for intuitive orbit (drag left -> rotate camera left -> decrease yaw)
                yaw -= xoffset * mouseSensitivity; 
                pitch += yoffset * mouseSensitivity;
                
                if (pitch > 89.0f) pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                updateCameraVectors();
                
                // Re-calculate position
                position = currentPivot - front * pivotDistance;
            }
            // 4. Fly Look (RMB)
            else if (isRMBDown) {
                yaw += xoffset * mouseSensitivity;
                pitch -= yoffset * mouseSensitivity;
                
                if (pitch > 89.0f) pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;
                
                updateCameraVectors();
            }
            // 1. Pan (Middle Mouse)
            else if (isMMBDown) {
                // Move in camera plane
                float panScale = 0.005f * speedFactor * pivotDistance; 
                position = position - right * xoffset * panScale + up * yoffset * panScale;
            }
            // 3. Zoom (Alt + Right)
            else if (isAltDown && isRMBDown) {
                float zoomSensitivity = 0.05f * speedFactor;
                // Drag down to zoom in? or up?
                // Usually Up -> Zoom In.
                float zoomDelta = -yoffset * zoomSensitivity; 
                processZoom(zoomDelta);
            }
        }
        else if (event.type == SDL_MOUSEWHEEL) {
            float speedFactor = isShiftDown ? 5.0f : 1.0f;
            processZoom(event.wheel.y * 0.5f * speedFactor);
        }
        else if (event.type == SDL_WINDOWEVENT) {
             if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                 int w = event.window.data1;
                 int h = event.window.data2;
                 if (h > 0) aspect = (float)w / (float)h;
             }
        }
    }

    void Update(float dt) {
        // 4. Fly Move: RMB + WASDQE
        if (isRMBDown) {
            float velocity = movementSpeed * dt;
            if (isShiftDown) velocity *= 4.0f;

            if (k_w) position = position + front * velocity;
            if (k_s) position = position - front * velocity;
            if (k_a) position = position - right * velocity;
            if (k_d) position = position + right * velocity;
            if (k_q) position = position - worldUp * velocity;
            if (k_e) position = position + worldUp * velocity;
            
            // While flying, we are essentially moving the pivot along with us if we switch back to orbit
            // So we don't explicitly update pivotDistance, but the conceptual pivot moves.
        }
    }

private:
    void processZoom(float delta) {
        // Zoom in/out
        float dist = pivotDistance;
        dist -= delta;
        if (dist < 0.1f) dist = 0.1f;
        
        // Move position
        position = position + front * (pivotDistance - dist);
        pivotDistance = dist;
    }

    void updateCameraVectors() {
        Vec4 f;
        f.x = std::cos(radians(yaw)) * std::cos(radians(pitch));
        f.y = std::sin(radians(pitch));
        f.z = std::sin(radians(yaw)) * std::cos(radians(pitch));
        front = normalize(f);
        right = normalize(cross(front, worldUp));
        up = normalize(cross(right, front));
    }
};

} // namespace tinygl
