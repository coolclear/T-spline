#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "Common/Common.h"

#define EPS 1e-9

class Ray {
public:
	Pt3 p;
	Vec3 dir;
	Pt3 at(double t) const { return p+(dir*t); }

	Ray() : p(0, 0, 0), dir(1, 0, 0, 0) {};

	Ray(const Pt3& p, const Vec3& d) {
		this->p = p;
		this->dir = d;
	}
};

class Plane {
public:
	Pt3 p;
	Vec3 n;

	Plane() : p(0, 0, 0), n(1, 0, 0, 0) {}
	Plane(const Vec3& p, const Vec3& n) {
		this->p = p;
		this->n = n;
	}
};

class Sphere;
class Operator;

class GeometryVisitor {
public:
	virtual void visit(Sphere* sphere, void* ret) = 0;
	virtual void visit(Operator* op, void* ret) = 0;
};

class Geometry
{
public:
	Geometry() {}
	virtual void accept(GeometryVisitor* visitor, void* ret) = 0;
};

namespace GeometryUtils {
	double pointRayClosest(const Pt3& pt, const Ray& ray);
	double pointRayDist(const Pt3& pt, const Ray& ray);
	double rayRayDist(const Ray& r1, const Ray& r2);
	double lineSegRayDist(const Pt3& p0, const Pt3& p1, const Ray& r);
	double planeRay(const Plane& pl, const Ray& r);
	double planeRayDeg(const Plane& pl, const Vec3& xa, const Ray& r);
};

#endif