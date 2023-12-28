#pragma once

#include <vector>

using namespace std;

struct Vec2
{
	float x, y;
};

struct Vec3
{
	float x, y, z;
};

struct Vertex{
	Vec2 pos;
	Vec3 color;
};