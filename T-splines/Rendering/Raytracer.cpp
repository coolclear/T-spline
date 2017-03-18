#include "Rendering/Raytracer.h"
#include "Rendering/Shading.h"
#include <FL/glu.h>
#include "Common/Common.h"

Raytracer::Raytracer() {
	_pixels = NULL;
}

void Raytracer::drawInit(GLdouble modelview[16], GLdouble proj[16], GLint view[4]) {
	_width = view[2];
	_height = view[3];

	if(_pixels) delete [] _pixels;
	_pixels = new float[_width*_height*4];

	memcpy(_modelview, modelview, 16*sizeof(modelview[0]));
	memcpy(_proj, proj, 16*sizeof(proj[0]));
	memcpy(_view, view, 4*sizeof(view[0]));

	Mat4 mv, pr;
	for(int j = 0; j < 16; j++) {
		mv[j/4][j%4] = modelview[j];
		pr[j/4][j%4] = proj[j];
	}

	_final = mv*pr;
	_invFinal = !_final;
	_last = 0;
}

Pt3 Raytracer::unproject(const Pt3& p) {
	Pt3	np = p;
	np[0] = (p[0]-_view[0])/_view[2];
	np[1] = (p[1]-_view[1])/_view[3];

	np[0] = np[0]*2-1;
	np[1] = np[1]*2-1;
	np[2] = np[2]*2-1;

	Pt3 ret = np*_invFinal;
	ret[0] /= ret[3];
	ret[1] /= ret[3];
	ret[2] /= ret[3];
	ret[3] = 1;
	return move(ret);
}

bool Raytracer::draw(int step) {
	int size = _width*_height;

	int j;
	for(j = _last; j < size && j < _last+step; j++) {
		int x = j % _width;
		int y = j / _width;

		Pt3 rst = unproject(Pt3(x, y, 0));
		Pt3 red = unproject(Pt3(x, y, 1));

		Ray r(rst, red-rst);
		r.dir.normalize();

		TraceResult res;
		if(_scene)
			res = trace(r, 0);

		res.color[3] = 1;
		int offset = (x + y*_width) * 4;
		for(int i = 0; i < 4; i++)
			_pixels[offset + i] = res.color[i];
	}

	_last = j;
	return (_last >= size);
}

TraceResult Raytracer::trace(const Ray& ray, int depth) {
	// TODO: perform recursive tracing here
	// TODO: perform proper illumination, shadowing, etc...

	const Color ambientI = _scene->getLight(0)->getAmbient();

	_intersector.setRay(ray);
	TraceResult res;

	double bestTime = DINF;
	Material* bestMat = NULL;

	for(int j = 0; j < _scene->getNumObjects(); j++) {
		IsectData data;
		data.hit = false;

		Geometry* geom = _scene->getObject(j);
		Material* mat = _scene->getMaterial(geom);
		geom->accept(&_intersector, &data);

		if(data.hit) {
			if(bestTime > data.t) {
				bestTime = data.t;
				bestMat = mat;
			}
		}
	}

	if(bestTime < DINF) { // hit
		// NOTE: Modify as needed
		res.color = bestMat->getDiffuse();
	} else { // miss
		// Default background color if missing all objects (default)
		res.color = Color(0,0,0);
	}

	return res;
}
