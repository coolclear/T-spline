#ifndef ZBUFFER_RENDERER_H
#define ZBUFFER_RENDERER_H

#include <FL/gl.h>
#include <FL/glut.h>
#include <FL/glu.h>

#include "Rendering/ShadeAndShapes.h"

class ZBufferVisitor;

class ZBufferRenderer
{
protected:
	TMeshScene* _scene;
	ZBufferVisitor* _visitor;

	Geometry* _selected;
	Geometry* _highlighted;
	Operator* _op;

	bool _drawGrid;
public:
	ZBufferRenderer();

	void initScene();
	virtual void draw();
	void drawGrid(double low, double high, int steps);

	void setScene(TMeshScene* s) { _scene = s; }
	void setSelected(Geometry* geom);
	Geometry* getSelected() { return _selected; }

	void setHighlighted(Geometry* geom);

	void setOperator(Operator* op, int mode);
	Operator* getOperator() { return _op; }
	int getOperatorMode();
};

#define OP_MODE_STANDARD 0
#define OP_MODE_TRANSLATE 1
#define OP_MODE_XAXIS 4
#define OP_MODE_YAXIS 8
#define OP_MODE_ZAXIS 16

class ZBufferVisitor : public GeometryVisitor {
protected:
	GLUquadric* _quadric;
	int _opMode;
public:
	ZBufferVisitor();

	int getOpMode() { return _opMode; }
	void setOpMode(int mode) { _opMode = mode; }
	virtual void visit(Sphere* sphere, void* ret);
	virtual void visit(Operator* op, void* ret);
};

#endif