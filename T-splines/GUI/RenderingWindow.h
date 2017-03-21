#ifndef RENDERING_WINDOW_H
#define RENDERING_WINDOW_H

#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Window.H>
#include <FL/Fl.H>
#include <FL/gl.h>
#include <FL/glut.h>
#include <FL/glu.h>

#include "Rendering/ArcBall.h"
#include "Rendering/Operator.h"
#include "Rendering/RenderingPrimitives.h"
#include "GeometryWindow.h"

#include "Common/Matrix.h"
#include "Common/Common.h"

#include <vector>
#include <map>
#include <mutex>

class GeometryWindow;

class MeshRenderer
{
protected:
	TriMeshScene* _scene;
	View* _view;
	int _shadingModel;

public:
	bool drawWire;
	bool useNormal;

	MeshRenderer()
	{
		drawWire = true;
		useNormal = true;
		_scene = NULL;
		_view = NULL;
		_shadingModel = SHADE_FLAT;
	}

	virtual void setTriMeshScene(TriMeshScene* TMeshScene) { _scene = TMeshScene; }
	virtual void setShadingModel(int m) { _shadingModel = m; }
	virtual  int getShadingModel() { return _shadingModel; }

	virtual void setView(View* view);
	virtual void draw();

protected:
	virtual void initLights();
};

class RenderingWindow : public Fl_Gl_Window {
protected:
	static int _w, _h;
	static int _frames;
	static double _lastTime;
	static double _fps;

	static int _prevMx;
	static int _prevMy;
	static Pt3 _prevMPt;
	static double _prevRot;

	static ArcBall::ArcBall_t* _arcBall;

	static Mat4 _lastRot;
	static Mat4 _proj;

	static Vec3 _zoomVec;
	static Vec3 _panVec;

	static TriMeshScene* _scene;
	static RenderingWindow* _singleton;

	MeshRenderer _renderer;
	mutex sceneLock;

public:
	RenderingWindow(int x, int y, int w, int h, const char* l);
	~RenderingWindow();

	void draw();
	int handle(int flag);
	void init();

//	static TriMeshScene* getTriMeshScene() { return _scene; }
	void createScene(const TMesh *T);

protected:
	void createCurveScene(const vector<pair<Pt3, int>> &P);
	void createTriScene(const VVP3 &S);

	static inline int getWidth() { return _w; }
	static inline int getHeight() { return _h; }

	static void updateModelView();
	static void handleZoom(int x, int y, bool b);
	static void handleRot(int x, int y, bool b);
	static void handlePan(int x, int y, bool b);

	static Ray getMouseRay(int x, int y);
	static Pt3 getMousePoint(int x, int y);

	static void exitButtonCb(Fl_Widget* widget, void* win) { exit(0); }
	static void escapeButtonCb(Fl_Widget* widget, void* win) {}
	static void updateCb(void* userdata) {
		RenderingWindow* viewer = (RenderingWindow*) userdata;
		viewer->redraw();
		Fl::repeat_timeout(REFRESH_RATE, RenderingWindow::updateCb, userdata);
	}

public:
	void resize(int x, int y, int w, int h);

};


#endif