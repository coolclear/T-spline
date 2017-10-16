#include "Rendering/TopologyViewer.h"
#include "GUI/TopologyWindow.h"

#include <GL/glu.h>

using namespace std;

static int highlightDir = 0;
static int highlightRow = 0;
static int highlightCol = 0;

static const double canvasMargin = 0.1;
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

	auto gridVertex2d = [&](double r, double c)
	{
		r = max(0.01, min(0.99, r * sy + my));
		c = max(0.01, min(0.99, c * sx + mx));
		glVertex2d(c, r);
	};

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
		if(highlightDir == 1 and r == highlightRow and c == highlightCol)
			glLineWidth(3);
		else
			glLineWidth(1);

		if(_mesh->gridH[r][c].on) // H-line active?
			glColor3dv(&colorActive[0]);
		else
			glColor3dv(&colorInactive[0]);

		glBegin(GL_LINES);
		gridVertex2d(r, c);
		gridVertex2d(r, c + 1);
		glEnd();
	}

	// Draw V-lines
	FOR(r,0,_mesh->rows) FOR(c,0,_mesh->cols + 1)
	{
		// Thicken the highlighted line
		if(highlightDir == 2 and r == highlightRow and c == highlightCol)
			glLineWidth(3);
		else
			glLineWidth(1);

		if(_mesh->gridV[r][c].on) // V-line active?
			glColor3dv(&colorActive[0]);
		else
			glColor3dv(&colorInactive[0]);

		glBegin(GL_LINES);
		gridVertex2d(r, c);
		gridVertex2d(r + 1, c);
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
			glLineStipple(6, 0xAAAA);
			glEnable(GL_LINE_STIPPLE);

			glLineWidth(1);
			glBegin(GL_LINES);

			// Draw a (dashed) line of an edge, isVert: H(0) or V(1)
			auto drawLine = [&](EdgeInfo ei, int r, int c, bool isVert)
			{
				bool doDraw = false;
				if(not ei.valid)
				{
					doDraw = true;
					glColor3dv(&colorBad[0]);
				}
				else if(_mesh->isAD and ei.extend) // draw T-j.e. only if AD
				{
					doDraw = true;
					if(isVert)
						glColor3dv(&colorExtendV[0]);
					else
						glColor3dv(&colorExtendH[0]);
				}

				if(doDraw)
				{
					gridVertex2d(r, c);
					gridVertex2d(r + isVert, c + not isVert);
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

			if(vertexType == VALENCE_INVALID or vertexType >= 3)
			{
				if(vertexType == 3)
					glColor3dv(&v3Color[0]);
				else if(vertexType == 4)
					glColor3dv(&v4Color[0]);
				else // vertexType == VALENCE_INVALID
					glColor3dv(&vBadColor[0]);

				gridVertex2d(r, c);
			}
		}
		glEnd();


		// Mark points that are intersections of V-H T-junction extensions
		if(_mesh->validVertices and _mesh->isAD and not _mesh->isAS)
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

		// Mark non-de-Boor unit elements
		if(_mesh->isAS and not _mesh->isDS)
		{
			FOR(r,0,_mesh->rows) FOR(c,0,_mesh->cols) if(_mesh->blendDir[r][c] == DIR_NEITHER)
			{
				const double margin_small {0.1};
				const double margin_large {0.4};

				double r0 {r + margin_small};
				double c0 {c + margin_small};
				double r1 {r + 1 - margin_small};
				double c1 {c + 1 - margin_small};

				glColor3d(0.3,0,0); // dark red
				glBegin(GL_QUADS);
				gridVertex2d(r0, c0);
				gridVertex2d(r1, c0);
				gridVertex2d(r1, c1);
				gridVertex2d(r0, c1);
				glEnd();
			}
		}

		// Display the tiled floor of an anchor or the blending points for a unit element
		if(_mesh->isAS and highlightDir >= 3)
		{
			auto getTiledFloorRange = [&](const int r, const int c, int& r_min, int& r_max, int& c_min, int& c_max)
			{
				auto onVSkel = [&](int r0)
				{
					return _mesh->gridPoints[r0][c].valenceType >= 3 or _mesh->gridPoints[r0][c].valenceBits == 12;
				};
				auto onHSkel = [&](int c0)
				{
					return _mesh->gridPoints[r][c0].valenceType >= 3 or _mesh->gridPoints[r][c0].valenceBits == 3;
				};
				r_min = r_max = r;
				c_min = c_max = c;
				FOR(k,0,2)
				{
					while(--r_min >= 0 and not onVSkel(r_min));
					while(++r_max <= _mesh->rows and not onVSkel(r_max));
					while(--c_min >= 0 and not onHSkel(c_min));
					while(++c_max <= _mesh->cols and not onHSkel(c_max));
				}
				r_min = max(r_min, 0);
				r_max = min(r_max, _mesh->rows);
				c_min = max(c_min, 0);
				c_max = min(c_max, _mesh->cols);
			};

			// Mark the point at which the cursor is pointing
			if(highlightDir == 3)
			{
				glPointSize(6);
				glBegin(GL_POINTS);
				glColor3d(1,0,1);
				gridVertex2d(highlightRow, highlightCol);
				glEnd();

				int r_min, r_max, c_min, c_max;
				getTiledFloorRange(highlightRow, highlightCol, r_min, r_max, c_min, c_max);

				glColor3d(1,0,1);
				glLineWidth(2);
				glBegin(GL_LINE_LOOP);
				gridVertex2d(r_min, c_min);
				gridVertex2d(r_min, c_max);
				gridVertex2d(r_max, c_max);
				gridVertex2d(r_max, c_min);
				glEnd();
			}


			// Mark the unit element at which the cursor is pointing
			if(highlightDir == 4)
			{
				vector<pair<int,int>> blendP, blendP2, missing, extra;
				bool row_n_4, col_n_4;
				//_mesh->get16Points(highlightRow, highlightCol, blendP, row_n_4, col_n_4);
				_mesh->test1(highlightRow, highlightCol, blendP, blendP2, missing, extra, row_n_4, col_n_4);

				// Mark found or missing blending points (for testing the algorithms in the paper)
				glBegin(GL_POINTS);
				glPointSize(8);
				// all good blending points
				glColor3d(0, 1, 1);
				for(auto& p: blendP) gridVertex2d(p._1, p._2);
				// missing points
				glColor3d(1, 0.2, 0);
				for(auto& p: missing) gridVertex2d(p._1, p._2);
				// extra points
				glColor3d(0.5, 1, 0);
				for(auto& p: extra) gridVertex2d(p._1, p._2);
				glEnd();


				double r0, r1, c0, c1;
				const double margin_small {0.1};
				const double margin_large {0.4};

				// Color green if the unit element is renderable, red otherwise
				if(row_n_4 or col_n_4) glColor3d(0,0.4,0);
				else glColor3d(0.5,0,0);

				// Show which directions the unit element can be rendered first
				if(row_n_4 == col_n_4)
				{
					r0 = highlightRow + margin_small;
					c0 = highlightCol + margin_small;
					r1 = highlightRow + 1 - margin_small;
					c1 = highlightCol + 1 - margin_small;
				}
				else if(row_n_4)
				{
					r0 = highlightRow + margin_large;
					c0 = highlightCol + margin_small;
					r1 = highlightRow + 1 - margin_large;
					c1 = highlightCol + 1 - margin_small;
				}
				else // if(col_n_4)
				{
					r0 = highlightRow + margin_small;
					c0 = highlightCol + margin_large;
					r1 = highlightRow + 1 - margin_small;
					c1 = highlightCol + 1 - margin_large;
				}

				glBegin(GL_QUADS);
				gridVertex2d(r0, c0);
				gridVertex2d(r1, c0);
				gridVertex2d(r1, c1);
				gridVertex2d(r0, c1);
				glEnd();

/*
				// Simplest: only if all 16 points are present
				bool simplestOk {true};
				FOR(dr,-1,3) FOR(dc,-1,3)
				{
					int r {highlightRow + dr};
					int c {highlightCol + dc};
					if(r >= 0 and r <= _mesh->rows and c >= 0 and c <= _mesh->cols)
					{
						int vertexType = _mesh->gridPoints[r][c].valenceType;
						if(vertexType < 3) simplestOk = false;
					}
					else simplestOk = false;
				}
				if(simplestOk)
				{
					glPointSize(10);
					glBegin(GL_POINTS);
					glColor3d(1,0.5,0);
					FOR(r,-1,3) FOR(c,-1,3)
					{
						gridVertex2d(highlightRow + r, highlightCol + c);
					}
					glEnd();
				}
				else
				{
					// Test: use tiled floors
					glPointSize(10);
					glBegin(GL_POINTS);
					glColor3d(1,0.1,0);
					FOR(r,0,_mesh->rows+1) FOR(c,0,_mesh->cols+1)
					{
						// ignore non-vertex
						if(_mesh->gridPoints[r][c].valenceType < 3) continue;

						int r_min, r_max, c_min, c_max;
						getTiledFloorRange(r, c, r_min, r_max, c_min, c_max);

						if(r_min <= highlightRow and highlightRow < r_max and
								c_min <= highlightCol and highlightCol < c_max)
						{
							gridVertex2d(r, c);
						}
					}
					glEnd();
				}

				glBegin(GL_QUADS);
				glColor3d(0,0.4,0);
				gridVertex2d(highlightRow + 0.1, highlightCol + 0.1);
				gridVertex2d(highlightRow + 0.9, highlightCol + 0.1);
				gridVertex2d(highlightRow + 0.9, highlightCol + 0.9);
				gridVertex2d(highlightRow + 0.1, highlightCol + 0.9);
				glEnd();
//*/
			}
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
		if(Fl::event_button() == FL_LEFT_MOUSE and (highlightDir == 1 or highlightDir == 2))
		{
			if(highlightDir == 1) // toggle the H-line
			{
				_mesh->gridH[highlightRow][highlightCol].on =
					not _mesh->gridH[highlightRow][highlightCol].on;
			}
			else if(highlightDir == 2) // toggle the V-line
			{
				_mesh->gridV[highlightRow][highlightCol].on =
					not _mesh->gridV[highlightRow][highlightCol].on;
			}
			_mesh->updateMeshInfo();

			// Reflect changes to the rendered scene
			if(_parent)
			{
				_parent->updateControlPoints();
				if(_mesh->isAS)
					_parent->updateSurface();
			}
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
				// ignore topmost and bottommost H-lines
				bool roundedRowIn {roundedRow > 0 and roundedRow < _mesh->rows};
				// ignore leftmost and rightmost V-lines
				bool roundedColIn {roundedCol > 0 and roundedCol < _mesh->cols};
				// ignore lines outside T-mesh
				bool cursorRowIn {cursorRow > 0 and cursorRow < _mesh->rows};
				// ignore lines outside T-mesh
				bool cursorColIn {cursorCol > 0 and cursorCol < _mesh->cols};

				// Find the closest H-grid line
				double distH = 10;
				if(roundedRowIn and cursorColIn) distH = abs(rowY - cursorPoint[1]);

				// Find the closest V-grid line
				double distV = 10;
				if(roundedColIn and cursorRowIn) distV = abs(rowX - cursorPoint[0]);

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
				else if(cursorRowIn and cursorColIn)
				{
					highlightDir = 4;
					highlightRow = (int) floor(cursorRow);
					highlightCol = (int) floor(cursorCol);
//					printf("pointing at (%d, %d)\n", highlightRow, highlightCol);
//					printf("rows %d cols %d\n", _mesh->rows, _mesh->cols);
				}
			}
			else if(0 <= roundedRow and roundedRow <= _mesh->rows and
					0 <= roundedCol and roundedCol <= _mesh->cols and
					_mesh->gridPoints[roundedRow][roundedCol].valenceType >= 3)
			{
//				printf("pointing at (%d, %d)\n", roundedRow, roundedCol);
				highlightDir = 3;
				highlightRow = roundedRow;
				highlightCol = roundedCol;
			}
		}
	}
	else if(ev==FL_KEYDOWN) {}
	else if(ev==FL_KEYUP) {}

	return Fl_Gl_Window::handle(ev);
}