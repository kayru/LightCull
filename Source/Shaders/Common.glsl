#ifndef COMMON_GLSL
#define COMMON_GLSL

#define FLT_MAX 3.402823466e+38F
#define SCALARIZE(v) readFirstInvocationARB(v)

uint divUp(uint n, uint d)
{
	return (n + d - 1) / d;
}

uint nextPow2(uint n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}

uint popcnt(uint x)
{
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0f0f0f0f);
	x += (x >> 8);
	x += (x >> 16);
	return x & 0x0000003f;
}

uint clz(uint x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return 32 - popcnt(x);
}

float sqr(float v)
{
	return v*v;
}

struct Box2
{
	vec2 m_min;
	vec2 m_max;
};

Box2 makeBox2(vec2 vmin, vec2 vmax)
{
	Box2 result;
	result.m_min = vmin;
	result.m_max = vmax;
	return result;
}

vec3 projectPoint(mat4 m, vec3 p)
{
	vec4 r = vec4(p, 1.0) * m;
	return vec3(r.xyz / r.w);
}

void getBoundsForAxis(vec3 axis, vec3 C, float r, float nearZ, out vec3 L, out vec3 U)
{
	// Based on http://jcgt.org/published/0002/02/05/paper.pdf

	const vec2 c = vec2(dot(axis, C), C.z);

	vec2 bounds[2]; // In the a-z reference frame

	const float radiusSquared = sqr(r);

	const float tSquared = dot(c, c) - radiusSquared;

	const bool cameraInsideSphere = (tSquared <= 0);

	// (cos, sin) of angle theta between c and a tangent vector
	vec2 v = cameraInsideSphere ? vec2(0.0f, 0.0f) : vec2(sqrt(tSquared), r) / c.length();

	// Does the near plane intersect the sphere?
	const bool clipSphere = (C.z - r <= nearZ);

	// Square root of the discriminant;
	// NaN (and unused) if the camera is in the sphere
	float k = sqrt(radiusSquared - sqr(C.z - nearZ));

	for (int i = 0; i < 2; ++i)
	{
		if (!cameraInsideSphere)
		{
			mat2 m;
			m[0] = vec2(v.x, -v.y);
			m[1] = vec2(v.y, v.x);
			bounds[i] = (m * c) * v.x;
		}

		const bool clipBound = cameraInsideSphere || (bounds[i].y < nearZ);
		if (clipSphere && clipBound)
		{
			bounds[i] = vec2(c.x + k, nearZ);
		}

		// Set up for the lower bound
		v.y = -v.y;
		k = -k;
	}

	// Transform back to camera space
	L = bounds[1].x * axis;
	L.z = bounds[1].y;

	U = bounds[0].x * axis;
	U.z = bounds[0].y;
}

Box2 computeProjectedBoundingBox(mat4 matProj, vec3 C, float r, float nearZ)
{
	// Based on http://jcgt.org/published/0002/02/05/paper.pdf

	vec3 right, left, top, bottom;

	getBoundsForAxis(vec3(1, 0, 0), C, r, nearZ, left, right);
	getBoundsForAxis(vec3(0, 1, 0), C, r, nearZ, top, bottom);

	return makeBox2(
		vec2(projectPoint(matProj, left).x, projectPoint(matProj, bottom).y),
		vec2(projectPoint(matProj, right).x, projectPoint(matProj, top).y));
}


bool testRaySphere(vec3 ray, vec4 sphere)
{
	float r2 = sphere.w * sphere.w;
	vec3 a = ray * dot(sphere.xyz, ray);
	vec3 b = sphere.xyz - a;
	float d2 = dot(b, b);
	return d2 < r2;
}

bool testUnnormRaySphere(vec3 ray, vec4 sphere)
{
	float r2 = sphere.w * sphere.w;
	vec3 a = ray * dot(sphere.xyz, ray) / dot(ray, ray);
	vec3 b = sphere.xyz - a;
	float d2 = dot(b, b);
	return d2 < r2;
}

void groupMemoryBarrierWithGroupSync()
{
	groupMemoryBarrier();
	barrier();
}

#endif // COMMON_GLSL
