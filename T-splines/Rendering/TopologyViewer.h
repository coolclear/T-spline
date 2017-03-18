#ifndef TOPOLOGY_VIEWER_H
#define TOPOLOGY_VIEWER_H

#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>

#include "GUI/Button.h"
#include "RenderingPrimitives.h"

#include <list>
#include <map>
#include <fstream>

using namespace std;

class TopologyViewer : public Fl_Gl_Window{
protected:
	int _w, _h;
	TMesh *_mesh;

public:
	TopologyViewer(int x, int y, int w, int h, const char* l=0);
	~TopologyViewer();
	void draw();
	int handle(int flag);
	void init();
	void resize(int x, int y, int width, int height);

	inline int getWidth() { return _w; }
	inline int getHeight() { return _h; }

	Pt3 win2Screen(int x, int y);
	void set2DProjection();

	void setMesh(TMesh *mesh) { _mesh = mesh; }

	static void updateCb(void* userdata) {
		TopologyViewer* viewer = (TopologyViewer*) userdata;
		viewer->redraw();
		Fl::repeat_timeout(REFRESH_RATE, TopologyViewer::updateCb,userdata);
	}
};


#endif 