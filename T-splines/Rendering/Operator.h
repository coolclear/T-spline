#ifndef OPERATOR_H
#define OPERATOR_H

#include "Rendering/Geometry.h"

class Operand {
public:
	virtual Pt3 getCenter() = 0;
	virtual void translate(const Vec3& trans) = 0;
};

#define OP_NONE 0
#define OP_TRANSLATE 1
#define OP_ROTATE 2
#define OP_XAXIS 4
#define OP_YAXIS 8
#define OP_ZAXIS 16
#define OP_STEP .25

class Operator : public Geometry {
protected:
	Operand* _primary;
	Operand* _secondary;

	int _usageSt;
	Vec3 _dirx, _diry, _dirz;
public:
	Operator(Operand* op) {
		_primary = op;
		_usageSt = OP_NONE;

		_secondary = NULL;

		_dirx = Vec3(OP_STEP, 0, 0, 0);
		_diry = Vec3(0, OP_STEP, 0, 0);
		_dirz = Vec3(0, 0, OP_STEP, 0);
	}

	virtual void accept(GeometryVisitor* visitor, void* ret) {
		visitor->visit(this, ret);
	}

	Operand* getPrimaryOp() { return _primary; }
	Operand* getSecondaryOp() { return _secondary; }
	void setPrimaryOp(Operand* op) { _primary = op; }
	void setSecondaryOp(Operand* op) { _secondary = op; }

	const Vec3& getDirX() { return _dirx; }
	const Vec3& getDirY() { return _diry; }
	const Vec3& getDirZ() { return _dirz; }

	void setState(int st) { _usageSt = st; }
	int getState() { return _usageSt; }
	void translate(const Vec3& v) {
		_primary->translate(v);
		if(_secondary)
			_secondary->translate(v);
	}

	void updateTransform() {}
};

#endif