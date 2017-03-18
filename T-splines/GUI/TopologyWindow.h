#ifndef TOPOLOGY_WINDOW_H
#define TOPOLOGY_WINDOW_H

#include <FL/Fl_Window.H>
#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <FL/glut.h>
#include <FL/glu.h>

#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Button.H>

#include "Common/Common.h"
#include "Rendering/TopologyViewer.h"
#include "GeometryWindow.h"
#include "RenderingWindow.h"

#include <vector>

class TopologyWindow : public Fl_Window
{
protected:
	TopologyViewer *tv;
	TMesh *_mesh;
	GeometryWindow *_geometry;
	RenderingWindow *_renderer;

	Button *renderButton;
	Button *loadButton;
	Button *saveButton;

public:
	TopologyWindow(int x, int y, int w, int h, const char* l);
	~TopologyWindow();
	void prepGeometry();
	void prepRenderer();
	void loadMesh();
	void saveMesh();
	void setup(GeometryWindow *geometry, RenderingWindow *renderer)
	{
		_geometry = geometry;
		_renderer = renderer;
		prepGeometry();
		prepRenderer();
	}

protected:
	static void exitButtonCb(Fl_Widget* widget, void* win) { exit(0); }
	static void escapeButtonCb(Fl_Widget* widget, void* win) {}
};

#endif