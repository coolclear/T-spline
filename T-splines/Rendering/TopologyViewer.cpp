#include "Rendering/TopologyViewer.h"
#include "GUI/TopologyWindow.h"

#include <GL/glu.h>

using namespace std;

static int highlightDir = 0;
static int highlightRow = 0;
static int highlightCol = 0;

static const double canvasMargin = 0.03;
static const double canvasLen = 1 - canvasMargin * 2;

TopologyViewer::TopologyViewer(int x, int y, int w, int h, const char* l)
: Fl_Gl_Window(x,y,w,h,l) {
	_w = w;
	_h = h;
	_mesh = NULL;
	this->border(5);

	Fl::repeat_timeout(REFRESH_RATE,TopologyViewer::updateCb,this);
}

TopologyViewer::~TopologyViewer() {
	Fl::remove_timeout(TopologyViewer::updateCb,this);
}

void TopologyViewer::set2DProjection()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0,1,1,0); // Y-axis points downward

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
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

	static Color colorActive(1,1,1); // Active line (white)
	static Color colorInactive(0.2,0.2,0.2); // Inactive line (dark gray)
	static Color colorMark(0.5,0.5,0.5); // Marks for 1D-grid (gray)

	// Draw grid lines (1D or 2D)
	const double sx = (_mesh->cols > 0) ? canvasLen / _mesh->cols : 0;
	const double sy = (_mesh->rows > 0) ? canvasLen / _mesh->rows : 0;
	const double mx = (_mesh->cols > 0) ? canvasMargin : 0.5;
	const double my = (_mesh->rows > 0) ? canvasMargin : 0.5;

	glLineWidth(1);
	glBegin(GL_LINES);
	// Draw marks for 1D-grid
	glColor3dv(&colorMark[0]);
	if(_mesh->rows == 0)
	{
		// Marks for H-lines
		const double sx = canvasLen / _mesh->cols;
		FOR(c,0,_mesh->cols + 1)
		{
			glVertex2d(c * sx + mx, 0.48);
			glVertex2d(c * sx + mx, 0.52);
		}
	}
	if(_mesh->cols == 0)
	{
		// Marks for V-lines
		const double sy = canvasLen / _mesh->rows;
		FOR(r,0,_mesh->rows + 1)
		{
			glVertex2d(0.48, r * sy + my);
			glVertex2d(0.52, r * sy + my);
		}
	}
	glEnd();


	// Draw H-lines
	FOR(r,0,_mesh->rows + 1) FOR(c,0,_mesh->cols)
	{
		// Thicken the highlighted line
		if(highlightDir == 1 && r == highlightRow && c == highlightCol)
			glLineWidth(3);
		else
			glLineWidth(1);

		if(_mesh->gridH[r][c].on) // H-line active?
			glColor3dv(&colorActive[0]);
		else
			glColor3dv(&colorInactive[0]);

		glBegin(GL_LINES);
		glVertex2d(c * sx + mx, r * sy + my);
		glVertex2d((c + 1) * sx + mx, r * sy + my);
		glEnd();
	}

	// Draw V-lines
	FOR(r,0,_mesh->rows) FOR(c,0,_mesh->cols + 1)
	{
		// Thicken the highlighted line
		if(highlightDir == 2 && r == highlightRow && c == highlightCol)
			glLineWidth(3);
		else
			glLineWidth(1);

		if(_mesh->gridV[r][c].on) // V-line active?
			glColor3dv(&colorActive[0]);
		else
			glColor3dv(&colorInactive[0]);

		glBegin(GL_LINES);
		glVertex2d(c * sx + mx, r * sy + my);
		glVertex2d(c * sx + mx, (r + 1) * sy + my);
		glEnd();
	}


	if(_mesh->rows * _mesh->cols > 0)
	{
		static Color colorBad(1, 0, 0); // Bad line (red)
		static Color colorExtendH(0, 0.7, 0); // H-extension (green)
		static Color colorExtendV(1, 0.5, 0); // V-extension (orange)

		// Draw bad edges (red) and T-junction extensions (H:green, V:orange)
		if(_mesh->validVertices)
		{
			// Make the line dashed
			glPushAttrib(GL_ENABLE_BIT);
			glLineStipple(5, 0xAAAA);
			glEnable(GL_LINE_STIPPLE);

			glLineWidth(2);
			glBegin(GL_LINES);

			// Draw a (dashed) line of an edge, isVert: H(0) or V(1)
			auto drawLine = [&](EdgeInfo ei, int r, int c, bool isVert)
			{
				bool doDraw = false;
				if(!ei.valid)
				{
					doDraw = true;
					glColor3dv(&colorBad[0]);
				}
				else if(_mesh->isAD && ei.extend) // draw T-j.e. only if AD
				{
					doDraw = true;
					if(isVert)
						glColor3dv(&colorExtendV[0]);
					else
						glColor3dv(&colorExtendH[0]);
				}

				if(doDraw)
				{
					glVertex2d(c * sx + mx, r * sy + my);
					glVertex2d((c + !isVert) * sx + mx, (r + isVert) * sy + my);
				}
			};

			// Draw bad H-edges or H-T-junction extensions
			FOR(r,0,_mesh->rows + 1) FOR(c,0,_mesh->cols)
				drawLine(_mesh->gridH[r][c], r, c, false);

			// Draw bad V-edges or V-T-junction extensions
			FOR(r,0,_mesh->rows) FOR(c,0,_mesh->cols + 1)
				drawLine(_mesh->gridV[r][c], r, c, true);

			glEnd();
			glPopAttrib();
		}


		// Draw vertices
		Color v3Color(0.2, 1, 0.2); // green for valence 3
		Color v4Color(0.2, 0.4, 1); // blue for valence 4
		Color vBadColor(1, 0.2, 0.2); // red for bad vertices

		glPointSize(6);
		glBegin(GL_POINTS);
		FOR(r,0,_mesh->rows + 1) FOR(c,0,_mesh->cols + 1)
		{
			int vertexType = _mesh->gridPoints[r][c].valenceType;

			if(vertexType == VALENCE_INVALID || vertexType >= 3)
			{
				if(vertexType == 3)
					glColor3dv(&v3Color[0]);
				else if(vertexType == 4)
					glColor3dv(&v4Color[0]);
				else // vertexType == VALENCE_INVALID
					glColor3dv(&vBadColor[0]);

				glVertex2d(c * sx + mx, r * sy + my);
			}
		}
		glEnd();


		// Mark points that are intersections of V-H T-junction extensions
		if(_mesh->validVertices && _mesh->isAD && !_mesh->isAS)
		{
			glLineWidth(2);
			glColor3d(1, 0, 0); // red
			glBegin(GL_LINES);
			FOR(r,0,_mesh->rows + 1) FOR(c,0,_mesh->cols + 1)
			{
				if(_mesh->gridPoints[r][c].extendFlag == EXTENSION_BOTH)
				{
					const double x = c * sx + mx;
					const double y = r * sy + my;
					glVertex2d(x - 0.02, y);
					glVertex2d(x + 0.02, y);
					glVertex2d(x, y - 0.02);
					glVertex2d(x, y + 0.02);
				}
			}
			glEnd();
		}
	}

	swap_buffers();
}

Pt3 TopologyViewer::win2Screen(int x, int y) {
	return Pt3(double(x) / _w, double(y) / _h, 0);
}

int TopologyViewer::handle(int ev)
{
	if(ev==FL_PUSH)
	{
		if(Fl::event_button() == FL_LEFT_MOUSE && highlightDir != 0)
		{
			if(highlightDir == 1) // toggle the H-line
			{
				_mesh->gridH[highlightRow][highlightCol].on =
					!_mesh->gridH[highlightRow][highlightCol].on;
			}
			else // highlightDir == 2, toggle the V-line
			{
				_mesh->gridV[highlightRow][highlightCol].on =
					!_mesh->gridV[highlightRow][highlightCol].on;
			}
			_mesh->updateMeshInfo();

			// Reflect changes to the rendered scene
			if(_parent)
				_parent->updateControlPoints();
		}
		return 1;  // must return 1 here to ensure FL_PUSH? is sent
	}
	else if(ev==FL_DRAG) {}
	else if(ev==FL_RELEASE) {}
	else if(ev==FL_MOVE)
	{
		// Will highlight only when rows > 0 and cols > 0
		if(_mesh->rows * _mesh->cols > 0)
		{
			highlightDir = 0; // no highlighted point

			Pt3 cursorPoint = win2Screen(Fl::event_x(), Fl::event_y());
			double cursorRow = _mesh->rows * (cursorPoint[1] - canvasMargin) / canvasLen;
			double cursorCol = _mesh->cols * (cursorPoint[0] - canvasMargin) / canvasLen;
			int roundedRow = (int) round(cursorRow);
			int roundedCol = (int) round(cursorCol);
			double rowY = (canvasLen * roundedRow) / _mesh->rows + canvasMargin;
			double rowX = (canvasLen * roundedCol) / _mesh->cols + canvasMargin;

			Pt3 roundedPoint(rowX, rowY, 0);
			double pointDist2 = mag2(roundedPoint - cursorPoint);

			// Consider highlighting only when the cursor is not at a gridpoint
			if(pointDist2 > 0.0001) // note that squared distance
			{
				// Find the closest H-grid line
				double distH = 10;
				if(roundedRow > 0 && roundedRow < _mesh->rows && // ignore topmost and bottommost H-lines
					cursorCol > 0 && cursorCol < _mesh->cols) // ignore lines outside T-mesh
					distH = abs(rowY - cursorPoint[1]);

				// Find the closest V-grid line
				double distV = 10;
				if(roundedCol > 0 && roundedCol < _mesh->cols && // ignore leftmost and rightmost V-lines
					cursorRow > 0 && cursorRow < _mesh->rows) // ignore lines outside T-mesh
					distV = abs(rowX - cursorPoint[0]);

				if(min<double>(distH,distV) < 0.02)
				{
					if(distH <= distV) // cursor closest to some H-line
					{
						highlightDir = 1;
						highlightRow = roundedRow;
						highlightCol = (int) floor(cursorCol);
					}
					else // cursor closest to some V-line
					{
						highlightDir = 2;
						highlightRow = (int) floor(cursorRow);
						highlightCol = roundedCol;
					}
				}
			}
		}
	}
	else if(ev==FL_KEYDOWN) {}
	else if(ev==FL_KEYUP) {}

	return Fl_Gl_Window::handle(ev);
}