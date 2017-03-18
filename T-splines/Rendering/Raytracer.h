#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "Rendering/ShadeAndShapes.h"
#include "Rendering/Renderer.h"
#include <FL/gl.h>

struct TraceResult {
	// NOTE: You can add more data here for your own recursive ray tracing
	Color color;
};

class Raytracer : public Renderer {
protected:
	Intersector _intersector;

	float*  _pixels;
	int _width;
	int _height;

	GLdouble _modelview[16], _proj[16];
	GLint _view[4];

	Mat4 _final;
	Mat4 _invFinal;

	int _last;

public:
	Raytracer();
	virtual void draw() {}
	virtual void drawInit(GLdouble modelview[16], GLdouble proj[16], GLint view[4]);
	virtual bool draw(int step);

	Pt3 unproject(const Pt3& p);

	TraceResult trace(const Ray& ray, int depth);

	int getWidth() { return _width; }
	int getHeight() { return _height; }
	float* getPixels() { return _pixels; }
};

#endif