#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>

#include "aabb.h"
#include "glad/glad.h"
#include "pcg.h"
#include "quadtree.h"
#include "timer.h"
#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION

constexpr uint32_t WORLD_WIDTH = 800;  // also height, for a square view.
constexpr size_t NUM_BALLS = 5000;
constexpr float RADIUS_MULTIPLIER = 5.f;
constexpr float MIN_RADIUS = 1.f;
constexpr float AVG_SPEED = 0.3f;

static sf::Vector2f randomPosition(PCG& rng, const float radius) {
    const float span = WORLD_WIDTH - 2.f * radius;
    return {radius + rng.rand01() * span, radius + rng.rand01() * span};
}

struct Ball {
    Ball(float radius, const sf::Vector2f& pos, const sf::Vector2f& vel)
        : radius{radius}, position{pos}, velocity{vel} {
        geometry.setRadius(radius);
        geometry.setOrigin({radius, radius});
        geometry.setPosition(pos);
        geometry.setFillColor(sf::Color::White);
    }

    void pushIntoBounds() {
        const auto [minPoint, maxPoint] = bounds();
        if (minPoint.x < 0) {
            position.x = radius;
            velocity.x = -velocity.x;
        }
        if (minPoint.y < 0) {
            position.y = radius;
            velocity.y = -velocity.y;
        }
        if (maxPoint.x > WORLD_WIDTH) {
            position.x = WORLD_WIDTH - radius;
            velocity.x = -velocity.x;
        }
        if (maxPoint.y > WORLD_WIDTH) {
            position.y = WORLD_WIDTH - radius;
            velocity.y = -velocity.y;
        }
    }

    void update(const float deltaTime) {
        position += deltaTime * velocity;
        pushIntoBounds();
    }

    void collisionEnter(Ball& other) {
        assert(intersects(other));
        // compute tangent line of collision
        const sf::Vector2f normal = position - other.position;
        const float distance = normal.length();
        const sf::Vector2f unitNormal =
            (distance != 0.f) ? normal / distance : sf::Vector2f{1.f, 0};
        const sf::Vector2f unitTangent = {-unitNormal.y, unitNormal.x};
        // compute distance of intersection
        const float toAdjust = (radius + other.radius - distance) * 0.5f;
        assert(toAdjust >= 0);
        // force-fix our positions
        position += toAdjust * unitNormal;
        other.position -= toAdjust * unitNormal;
        // assuming same mass, swap normal components of velocities
        sf::Vector2f velUV = {velocity.dot(unitTangent), velocity.dot(unitNormal)};
        sf::Vector2f velOtherUV = {other.velocity.dot(unitTangent), other.velocity.dot(unitNormal)};
        std::swap(velUV.y, velOtherUV.y);
        // revert back to our coords
        velocity = velUV.x * unitTangent + velUV.y * unitNormal;
        other.velocity = velOtherUV.x * unitTangent + velOtherUV.y * unitNormal;
    }

    void lateUpdate([[maybe_unused]] float deltaTime) {
        // for simplicity, we'll just handle wall collisions here
        pushIntoBounds();
    }

    constexpr bool intersects(const Ball& other) const {
        return (position - other.position).lengthSquared() <
               (radius + other.radius) * (radius + other.radius);
    }

    constexpr AABB<float> bounds() const {
        return {.minPoint = position - radius, .maxPoint = position + radius};
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        geometry.setPosition(position);
        target.draw(geometry, states);
    }

    float radius;
    sf::Vector2f position;
    sf::Vector2f velocity;
    mutable sf::CircleShape geometry;
};

int main() {
    // OpenGL settings we request for:
    sf::ContextSettings settings;
    settings.depthBits = 24;   // 24-bit depth buffer
    settings.stencilBits = 8;  // 8-bit stencil buffer
    // Request OpenGL 2.1 for compatibility with both SFML/Graphics AND macOS:
    //  - SFML's Graphics module implementation relies on legacy fixed-function pipeline features
    //     of OpenGL, which means only up to OpenGL 2.1. Any newer version requires Compatibility
    //     profile (i.e., maintaining legacy features).
    //  - macOS has deprecated OpenGL support, meaning for anything newer than 2.1 (i.e., 3.0 and
    //      newer), only Core profile is supported, i.e., REMOVING legacy fixed-function features.
    // As a result, if you want to both support macOS AND use SFML's Graphics features (e.g.,
    // sf::RenderWindow), you can use at latest OpenGL 2.1, which is a nightmare.
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    sf::RenderWindow window(sf::VideoMode({WORLD_WIDTH, WORLD_WIDTH}),
                            "Collisions without acceleration", sf::State::Windowed, settings);
    window.setFramerateLimit(512);
    (void)window.setActive();  // set window as OpenGL target
    // window.setMouseCursorVisible(false); // hide cursor

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        // Handle GLAD initialization error
        return -1;
    }
    if (GLAD_GL_ARB_texture_float) {
        std::cout << "Textures with 32-bit float channels supported\n";
    } else {
        std::cout << "textures with 32-bit float channels unsupported\n";
        abort();
    }

    // Query the settings we actually received (may be different):
    const auto& actualSettings = window.getSettings();
    std::cout << "OpenGL version " << actualSettings.majorVersion << "."
              << actualSettings.minorVersion << " flags: " << actualSettings.attributeFlags << '\n';
    std::cout << "Maximum texture width (or height): " << sf::Texture::getMaximumSize() << '\n';

    PCG rng;
    std::vector<Ball> balls;
    balls.reserve(NUM_BALLS);

    for (size_t i = 0; i < NUM_BALLS; ++i) {
        const float radius = rng.rand01() * RADIUS_MULTIPLIER + MIN_RADIUS;
        Ball newBall(radius, randomPosition(rng, radius),
                     sf::Vector2f{rng.rand01(), rng.rand01()} * AVG_SPEED * 2.f - AVG_SPEED);
        while (std::ranges::any_of(balls,
                                   [&](const Ball& other) { return newBall.intersects(other); })) {
            newBall.position = randomPosition(rng, radius);
        }
        balls.emplace_back(std::move(newBall));
    }
    Timer timer;
    float frameTimeMillisEMA = -1;
    size_t f = 0;
    while (window.isOpen()) {
        while (auto e = window.pollEvent()) {
            if (e->is<sf::Event::Closed>()) {
                return 0;
            }
        }
        auto dt = static_cast<float>(timer.click<Timer::Millis>());
        if (frameTimeMillisEMA == -1) [[unlikely]] {
            frameTimeMillisEMA = dt;
        } else {
            frameTimeMillisEMA += 0.05f * (dt - frameTimeMillisEMA);
        }
        if (f % 128 == 1) {
            std::cout << "FPS: " << 1000.f / frameTimeMillisEMA << std::endl;
        }

        for (auto& ball : balls) {
            ball.update(dt);
        }

        for (int i = 0; i < balls.size(); ++i) {
            for (int j = i + 1; j < balls.size(); ++j) {
                if (balls[i].intersects(balls[j])) {
                    balls[i].collisionEnter(balls[j]);
                }
            }
        }
        for (auto& ball : balls) {
            ball.lateUpdate(dt);
        }

        window.clear(sf::Color::Black);
        sf::RenderStates states{};
        for (size_t i = 0; i < NUM_BALLS; ++i) {
            balls[i].draw(window, states);
        }
        window.display();
        ++f;
    }
    return 0;
}
