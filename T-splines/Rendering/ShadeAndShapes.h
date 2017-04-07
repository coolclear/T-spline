#ifndef SHADE_AND_SHAPES_H
#define SHADE_AND_SHAPES_H

#include "Rendering/Operator.h"
#include "Rendering/Geometry.h"

#include "Common/Common.h"
#include "Common/Matrix.h"


//========================================================================
// Shapes
//========================================================================

class Sphere : public Geometry, public Operand {
protected:
	Pt3 _center;
	double _rad;
public:
	Sphere() {};
	Sphere(const Pt3& c, double r) : _center(c), _rad(r) {}

	// set/get methods
	void setRadius(double r) { _rad = r; }
	double getRadius() const { return _rad; }
	void setCenter(const Pt3& c) { _center = c; }
	Pt3 getCenter() const { return _center; }

	void translate(const Vec3& trans);
	void rotate(double d, int axis);
	void updateTransform();

	virtual void accept(GeometryVisitor* visitor, void* ret) { visitor->visit(this, ret); }
};


//========================================================================
// END OF Shapes
//========================================================================


struct IsectData {
	bool hit;
	double t;
	Vec3 normal;
	IsectData() : hit(false), t(DINF) {}
};

struct IsectAxisData {
	bool hit;
	int axis;
	IsectAxisData() : hit(false) {}
};

class Intersector : public GeometryVisitor {
protected:
	Ray _ray;
public:
	Intersector() : _ray(Pt3(0, 0, 0), Vec3(1, 0, 0, 0)) {}
	void setRay(const Ray& r) { _ray = r; }
	virtual void visit(Sphere* sphere, void* ret);
	virtual void visit(Operator* op, void* ret);
};

#endif