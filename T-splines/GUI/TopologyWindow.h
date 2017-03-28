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
#include "GUI/GeometryWindow.h"
#include "Rendering/TopologyViewer.h"

class TopologyWindow : public Fl_Window
{
protected:
	static TMesh _mesh;

	TopologyViewer *_viewer;
	GeometryWindow *_geometry;

	Fl_Group *fileGroup;
	Button *loadButton;
	Button *saveButton;

	Fl_Input *knotsHInput;
	Fl_Input *knotsVInput;
	Button *knotsHButton;
	Button *knotsVButton;

public:
	TopologyWindow(int x, int y, int w, int h, const char* l);
	~TopologyWindow();
	void updatePanel();
	void updateControlPoints();
	void updateSurface();
	void loadMesh(char *filePath = NULL);
	void saveMesh();
	void setup(GeometryWindow *geometry)
	{
		_geometry = geometry;
		updateControlPoints();
		updateSurface();
	}

protected:
	static void loadButtonCallback(Fl_Widget* widget, void* userdata);
	static void saveButtonCallback(Fl_Widget* widget, void* userdata);
	static void knotsHButtonCallback(Fl_Widget* widget, void* userdata);
	static void knotsVButtonCallback(Fl_Widget* widget, void* userdata);

	static void exitButtonCb(Fl_Widget* widget, void* win) { exit(0); }
	static void escapeButtonCb(Fl_Widget* widget, void* win) {}
};

#endif