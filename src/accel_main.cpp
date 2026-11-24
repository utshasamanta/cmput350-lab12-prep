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

static sf::Vector2f randomPosition(PCG& rng, float radius) {
    const float span = WORLD_WIDTH - 2.f * radius;
    return {radius + rng.rand01() * span, radius + rng.rand01() * span};
}

struct Ball {
    explicit Ball(float radius = 1, const sf::Vector2f& pos = {}, const sf::Vector2f& vel = {})
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

        pushIntoBounds();
        other.pushIntoBounds();
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
                            "Collisions with acceleration", sf::State::Windowed, settings);
    window.setFramerateLimit(1500);
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

    // Query the settings we actually received (maybe different):
    const auto& actualSettings = window.getSettings();
    std::cout << "OpenGL version " << actualSettings.majorVersion << "."
              << actualSettings.minorVersion << " flags: " << actualSettings.attributeFlags << '\n';
    std::cout << "Maximum texture width (or height): " << sf::Texture::getMaximumSize() << '\n';

    PCG rng;
    std::vector<Ball> balls;
    balls.reserve(NUM_BALLS);
    QuadTree<Ball> ballsAccel(
        AABB{.minPoint = sf::Vector2f(0, 0), .maxPoint = sf::Vector2f(WORLD_WIDTH, WORLD_WIDTH)});
    std::vector<uint32_t> candidates;
    candidates.reserve(NUM_BALLS);
    for (size_t i = 0; i < NUM_BALLS; ++i) {
        const float radius = rng.rand01() * RADIUS_MULTIPLIER + MIN_RADIUS;
        Ball newBall(radius, randomPosition(rng, radius),
                     sf::Vector2f{rng.rand01(), rng.rand01()} * AVG_SPEED * 2.f - AVG_SPEED);
        while (true) {
            candidates.clear();
            ballsAccel.queryIntersecting(newBall.bounds(), candidates);
            if (std::ranges::none_of(
                    candidates, [&](const uint32_t j) { return newBall.intersects(balls[j]); })) {
                break;
            }
            newBall.position = randomPosition(rng, radius);
        }
        ballsAccel.insert(newBall, newBall.bounds());
        balls.push_back(newBall);
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
        QuadTree<Ball> newQuadTree(AABB{.minPoint = sf::Vector2f(0, 0),
                                        .maxPoint = sf::Vector2f(WORLD_WIDTH, WORLD_WIDTH)});

        for (int i = 0; i < NUM_BALLS; ++i) {
            auto bounds = balls[i].bounds();
            newQuadTree.insert(balls[i], bounds);
        }
        ballsAccel = std::move(newQuadTree);

        for (int i = 0; i < ballsAccel.containerSize(); ++i) {
            if (!ballsAccel.valid(i)) {
                continue;
            }
            candidates.clear();
            if (ballsAccel.queryIntersecting(ballsAccel[i].bounds(), candidates) > 1) {
                for (auto j : candidates) {
                    if (j == static_cast<uint32_t>(i)) {
                        continue;
                    }
                    if (balls[i].intersects(balls[j])) {
                        balls[i].collisionEnter(balls[j]);
                    }
                }
            }
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
