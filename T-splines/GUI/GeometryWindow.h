#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Button.H>

#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <FL/glut.h>
#include <FL/glu.h>

#include "Rendering/ArcBall.h"
#include "Rendering/ZBufferRenderer.h"
#include "Rendering/Operator.h"

#include "GUI/RenderingWindow.h"

#include "Common/Matrix.h"
#include "Common/Common.h"

#include <vector>
#include <map>

class RenderingWindow;

class GeometryWindow : public Fl_Gl_Window {
protected:
	Fl_Button* _triButton;

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

	static int _buttonSt;
	static int _button;
	static int _inputMode;
	static int _holdAxis;

	static Vec3 _zoomVec;
	static Vec3 _panVec;

	static TMeshScene* _scene;
	static ZBufferRenderer* _zbuffer;
	static std::map<Geometry*, Operator*> _geom2op;
	static Intersector* _intersector;
	static Geometry* _selected;
	static Geometry* _highlighted;
	static bool _holdCtrl;
	static bool _holdAlt;
	static bool _holdShift;

	static TMeshScene *_meshScene;
	static GeometryWindow* _singleton;
	mutex sceneLock;

public:
	GeometryWindow(int x, int y, int w, int h, const char* l);
	~GeometryWindow();

	static TMeshScene* getScene() { return _scene; }

	void setupControlPoints(TMesh *tmesh);

	void draw();
	int handle(int flag);
	void init();

protected:
	static inline int getWidth() { return _w; }
	static inline int getHeight() { return _h; }

	static void display();

	static void updateModelView();
	static void handleZoom(int x, int y, bool b);
	static void handleRot(int x, int y, bool b);
	static void handleAxisTrans(int x, int y, bool b);
	static void handlePan(int x, int y, bool b);

	static Ray getMouseRay(int x, int y);
	static Pt3 getMousePoint(int x, int y);

	static void exitButtonCb(Fl_Widget* widget, void* win) { exit(0); }
	static void escapeButtonCb(Fl_Widget* widget, void* win) {}
	static void updateCb(void* userdata) {
		GeometryWindow* viewer = (GeometryWindow*) userdata;
		viewer->display();
		Fl::repeat_timeout(REFRESH_RATE, GeometryWindow::updateCb, userdata);
	}

	void resize(int x, int y, int w, int h);
};

#endif