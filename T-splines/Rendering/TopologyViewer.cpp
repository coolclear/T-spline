#include "Rendering/TopologyViewer.h"

#include <GL/glu.h>

using namespace std;

TopologyViewer::TopologyViewer(int x, int y, int w, int h, const char* l)
: Fl_Gl_Window(x,y,w,h,l) {
	Fl::repeat_timeout(REFRESH_RATE,TopologyViewer::updateCb,this);
	_w = w;
	_h = h;
	_mesh = NULL;
	this->border(5);
}

TopologyViewer::~TopologyViewer() {
	Fl::remove_timeout(TopologyViewer::updateCb,this);
}

void TopologyViewer::set2DProjection()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0,1,0,1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glViewport(0, 0, 1, 1);
}

void TopologyViewer::draw()
{
	if(_mesh == NULL) return;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0,0,0,1);
	glLineWidth(1.f);
	glPointSize(2.f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);

	set2DProjection();

	const double margin = 0.05;
	const double len = 1 - margin * 2;

	static Color colorActive(1,1,1); // Active line (white)
	static Color colorInactive(0.2,0.2,0.2); // Inactive line (dark gray)
	static Color colorMark(0.5,0.5,0.5); // Marks for 1D-grid (gray)

	// Draw grid lines (1D or 2D)
	const double sx = (_mesh->cols > 0) ? len / _mesh->cols : 0;
	const double sy = (_mesh->rows > 0) ? len / _mesh->rows : 0;
	const double mx = (_mesh->cols > 0) ? margin : 0.5;
	const double my = (_mesh->rows > 0) ? margin : 0.5;

	glLineWidth(0.5);
	glBegin(GL_LINES); //-----------------------------------

	// Draw marks for 1D-grid
	glColor3dv(&colorMark[0]);
	if(_mesh->rows == 0)
	{
		// Marks for H-lines
		const double sx = len / _mesh->cols;
		for(int c = 0; c <= _mesh->cols; ++c)
		{
			glVertex2d(c * sx + mx, 0.48);
			glVertex2d(c * sx + mx, 0.52);
		}
	}
	if(_mesh->cols == 0)
	{
		// Marks for V-lines
		const double sy = len / _mesh->rows;
		for(int r = 0; r <= _mesh->rows; ++r)
		{
			glVertex2d(0.48, r * sy + my);
			glVertex2d(0.52, r * sy + my);
		}
	}

	// Draw H-lines
	for(int r = 0; r <= _mesh->rows; ++r)
	{
		for(int c = 0; c < _mesh->cols; ++c)
		{
			if(_mesh->gridH[r][c]) // H-line active?
				glColor3dv(&colorActive[0]);
			else
				glColor3dv(&colorInactive[0]);

			glVertex2d(c * sx + mx, r * sy + my);
			glVertex2d((c + 1) * sx + mx, r * sy + my);
		}
	}

	// Draw V-lines
	for(int r = 0; r < _mesh->rows; ++r)
	{
		for(int c = 0; c <= _mesh->cols; ++c)
		{
			if(_mesh->gridV[r][c]) // V-line active?
				glColor3dv(&colorActive[0]);
			else
				glColor3dv(&colorInactive[0]);

			glVertex2d(c * sx + mx, r * sy + my);
			glVertex2d(c * sx + mx, (r + 1) * sy + my);
		}
	}

	glEnd(); //---------------------------------------------

	swap_buffers();
}

Pt3 TopologyViewer::win2Screen(int x, int y) {
	return Pt3(double(x) / _w, double(_h - y) / _h, 0);
}

int TopologyViewer::handle(int ev) {
	// input in 2d mode
	if(ev==FL_PUSH)
	{
		/*if(Fl::event_button()==FL_LEFT_MOUSE) {
			_prevpos = win2Screen(Fl::event_x(),Fl::event_y());
			if(_highlighted) {
				_selected = _highlighted;
			}
			else if(_highlightedPt) {
				_selectedPt = _highlightedPt;
			}
			else
				_panning = true;
		}*/
		return 1;  // must return 1 here to ensure FL_PUSH? is sent
	}
	else if(ev==FL_DRAG) {}
	else if(ev==FL_RELEASE) {}
	else if(ev==FL_MOVE) {/*
		Pt2 mpos = win2Screen(Fl::event_x(),Fl::event_y());
		double ratio = Utils::dist2d(_dspaceLL,_dspaceUR)/600;

		// TODO: maybe need to modify for more shapes
		// check to see if the mouse is interior to some shape
		// isPtInterior only works for convex shapes.
		// if your shape is not-convex, you need to write a different function to check for interior-ness.
		_highlighted = NULL;
		for(list<pair<Geom2*,Color> >::reverse_iterator i=_geomhist.getTop()->rbegin();i!=_geomhist.getTop()->rend();i++) {
			if(Utils::isPtInterior(i->first,mpos)) {
				_highlighted = i->first;
				break;
			}
		}

		_highlightedPt = NULL;
		if(!_highlighted) {
			double bestd=10000;
			Pt2* best = NULL;
			for(map<Pt2*,Geom2*>::iterator i=_p2geom.begin();i!=_p2geom.end();i++) {
				double nd = Utils::dist2d(*(i->first),mpos);
				if(nd<bestd) {
					bestd = nd;
					best = i->first;
				}
			}
			if(best && bestd<5*ratio)
				_highlightedPt = best;
		}*/
	}
	else if(ev==FL_KEYDOWN) {}
	else if(ev==FL_KEYUP) {}

	return Fl_Gl_Window::handle(ev);
}

void TopologyViewer::resize(int x, int y, int width, int height) {
	make_current();
	_w = width;
	_h = height;
	Fl_Gl_Window::resize(x,y,width,height);
}