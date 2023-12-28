#pragma once

#include "vertex.h"
#include <vector>

class Triangle
{
public:
	vector<Vertex> vert{
    Vertex{ Vec2{-0.5f, -0.5f }, Vec3{ 0.0, 0.0, 1.0 } },
    Vertex{ Vec2{ 0.5f,  0.5f }, Vec3{ 0.0, 1.0, 0.0 } },
    Vertex{ Vec2{-0.5f,  0.5f }, Vec3{ 1.0, 0.0, 0.0 } },
    Vertex{ Vec2{ 0.5f, -0.5f }, Vec3{ 1.0, 1.0, 1.0 } },
	};

    vector<uint32_t> indices = { 0, 1, 2, 1, 0, 3 };
};