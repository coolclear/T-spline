#include "Common/Common.h"
#include "Rendering/ShadeAndShapes.h"

// Returns the rotation matrix for rotation around the "axis" by "d" radians
Mat4 axisRotationMatrix(double d, int axis) {
	int id0, id1;
	switch(axis) {
		case OP_XAXIS: id0 = 1; id1 = 2; break; // X -> Y,Z
		case OP_YAXIS: id0 = 2; id1 = 0; break; // Y -> Z,X
		case OP_ZAXIS: id0 = 0; id1 = 1; break; // Z -> X,Y
		default: assert(false);
	}

	Mat4 m; // identity
	double c = cos(d);
	double s = sin(d);
	m[id0][id0] = c;
	m[id0][id1] = -s;
	m[id1][id0] = s;
	m[id1][id1] = c;
	return move(m);
}

//========================================================================
// translate() and rotate()
//========================================================================

void Sphere::translate(const Vec3& trans) {
	_center += trans;
	// No need to update the matrices for spheres
}
// Rotation doesn't affect a sphere
void Sphere::rotate(double r, int axis) {}




//========================================================================
// updateTransform() and Intersector::visit()
//========================================================================


// The operator is the widget that allows you to translate and rotate a geometric object
// It is colored as red/green/blue.  When one of the axis is highlighted, it becomes yellow.
void Intersector::visit(Operator* op, void* ret) {
	IsectAxisData* iret = (IsectAxisData*) ret;
	Pt3 center = op->getPrimaryOp()->getCenter();
	iret->hit = false;
	const int axes[3] = { OP_XAXIS, OP_YAXIS, OP_ZAXIS };

	if(op->getState() == OP_TRANSLATE) {
		const Ray rays[3] = {
			Ray(center, op->getDirX()), // X
			Ray(center, op->getDirY()), // Y
			Ray(center, op->getDirZ()) // Z
		};

		double bestTime = 0.1;
		for(int i = 0; i < 3; i++) {
			double hitTime = GeometryUtils::pointRayDist(rays[i].p + 0.5*rays[i].dir, _ray);
			if(bestTime > hitTime) {
				bestTime = hitTime;
				iret->hit = true;
				iret->axis = axes[i];
			}
		}
	} else if(op->getState() == OP_ROTATE) {
		const Plane planes[3] = {
			Plane(center, Vec3(1, 0, 0, 0)), // X: YZ-plane
			Plane(center, Vec3(0, 1, 0, 0)), // Y: XZ-plane
			Plane(center, Vec3(0, 0, 1, 0)) // Z: XY-plane
		};

		double bestTime = 0.1;
		for(int i = 0; i < 3; i++) {
			double timeTemp = GeometryUtils::planeRay(planes[i], _ray);
			double hitTime = abs(mag(_ray.at(timeTemp)-center) - OP_STEP);
			if(bestTime > hitTime) {
				bestTime = hitTime;
				iret->hit = true;
				iret->axis = axes[i];
			}
		}
	}
}


// The definition of a sphere can be pretty sparse,
// so you don't need to define the transform associated with a sphere.
void Sphere::updateTransform() {}

// TODO: rewrite this function as described in the textbook
void Intersector::visit(Sphere* sphere, void* ret) {
	IsectData* iret = (IsectData*) ret;

	Pt3 center = sphere->getCenter();
	double radius = sphere->getRadius();

	double closest = GeometryUtils::pointRayClosest(center, _ray);
	Vec3 C2P = _ray.at(closest) - center;
	assert(abs(C2P[3]) < 1e-20);

	double r2 = radius*radius;
	double d2 = C2P*C2P;

	if(d2 > r2 + EPS) {
		iret->hit = false;
		iret->t = 0;
	} else {
		double h = sqrt(r2 - d2);
		iret->t = closest-h;
		iret->hit = true;
		iret->normal = (_ray.at(iret->t) - center);
		iret->normal.normalize();
	}
}