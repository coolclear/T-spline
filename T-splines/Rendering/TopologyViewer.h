#ifndef TOPOLOGY_VIEWER_H
#define TOPOLOGY_VIEWER_H

#include <FL/Fl_Gl_Window.H>

#include "GUI/Button.h"
#include "TMesh.h"

using namespace std;

class TopologyWindow;
class TopologyViewer : public Fl_Gl_Window{
protected:
	int _w, _h;
	TMesh *_mesh;
	TopologyWindow *_parent;

public:
	TopologyViewer(int x, int y, int w, int h, const char* l=0);
	~TopologyViewer();
	void draw();
	int handle(int flag);

	int getWidth() { return _w; }
	int getHeight() { return _h; }

	Pt3 win2Screen(int x, int y);
	void set2DProjection();

	void setMesh(TMesh *mesh) { _mesh = mesh; }
	void setParent(TopologyWindow *tw) { _parent = tw; }

	static void updateCb(void* userdata) {
		TopologyViewer* viewer = (TopologyViewer*) userdata;
		viewer->redraw();
		Fl::repeat_timeout(REFRESH_RATE, TopologyViewer::updateCb,userdata);
	}
};


#endif 