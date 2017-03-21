#include "GUI/RenderingWindow.h"
#include "GUI/PropertyWindow.h"
#include <time.h>
#include <iostream>
#include <sstream>
#include <FL/gl.h>
#include <FL/glu.h>

#include <FL/Fl_Color_Chooser.H>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.141592653589793
#endif

#include "TriMeshScene.h"

using namespace std;

#define frand() (rand()/(float)RAND_MAX)

#define MENU_OPTION_COLOR 0
#define MENU_OPTION_DELETE 1

#define INPUT_VIEWING 0
#define INPUT_SELECTING 1
#define INPUT_EDITING 2
#define INPUT_TRANS 3
#define INPUT_ROT 4

int RenderingWindow::_frames = 0;
int RenderingWindow::_w = 0;
int RenderingWindow::_h = 0;
double RenderingWindow::_lastTime = 0.f;
double RenderingWindow::_fps = 0.f;
Mat4 RenderingWindow::_proj;
Mat4 RenderingWindow::_lastRot;

int RenderingWindow::_prevMx = 0;
int RenderingWindow::_prevMy = 0;
double RenderingWindow::_prevRot = 0;
Pt3 RenderingWindow::_prevMPt;
ArcBall::ArcBall_t* RenderingWindow::_arcBall = NULL;

Vec3 RenderingWindow::_panVec = Vec3(0, 0, 0, 0);
Vec3 RenderingWindow::_zoomVec = Vec3(0, 0, 0, 0);

TriMeshScene* RenderingWindow::_scene = NULL;
RenderingWindow* RenderingWindow::_singleton = NULL;

const int WIN_LOWER_SPACE = 0;
const int MENU_SPACE = 0;
const double MOUSE_TOL = 8.f;

Mat4 _mv;

// this is the function to call if you need to update your TMeshScene view after some mouse or keyboard input
void RenderingWindow::updateModelView() {
	if(_scene) _scene->updateModelView();
}

Matrix<double, 4> _modelview;
Matrix<double, 4> _translate;
Matrix<double, 4> _rotate;

void RenderingWindow::createCurveScene(const vector<pair<Pt3, int>>& P)
{
	sceneLock.lock();
	if(_scene)
	{
		_modelview = *_scene->getModelview();
		_rotate = *_scene->getRotate();
		_translate = *_scene->getTranslate();
		delete _scene;
	}

	_scene = ::createCurveScene(P);
	*_scene->getModelview() = _modelview;
	*_scene->getRotate() = _rotate;
	*_scene->getTranslate() = _translate;

	_renderer.setTriMeshScene(_scene);
	sceneLock.unlock();
}

void RenderingWindow::createTriScene(const VVP3 &S)
{
	sceneLock.lock();
	if(_scene)
	{
		_modelview = *_scene->getModelview();
		_rotate = *_scene->getRotate();
		_translate = *_scene->getTranslate();
		delete _scene;
	}

	_scene = ::createTriMeshScene(S);
	*_scene->getModelview() = _modelview;
	*_scene->getRotate() = _rotate;
	*_scene->getTranslate() = _translate;

	_renderer.setTriMeshScene(_scene);
	sceneLock.unlock();
}


RenderingWindow::RenderingWindow(int x, int y, int w, int h, const char* l)
	: Fl_Gl_Window(x, y, w, h+WIN_LOWER_SPACE, l)
{
	show();
	resize(x, y, w, h);

	begin();
	// This is where the glut rendering window is added to the FLTK window
	glutInitWindowSize(0,0);
	glutInitWindowPosition(-1,-1); // place it inside parent window
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glutCreateWindow("");
	end();

	_w = w;
	_h = h;
	_lastTime = clock()/((double)CLOCKS_PER_SEC);
	_arcBall = new ArcBall::ArcBall_t((float)_w, (float)_h);

	_singleton = this;
	this->callback(escapeButtonCb,this);
	Fl::repeat_timeout(REFRESH_RATE, RenderingWindow::updateCb, this);

	{
		const double A[19] = {
			0.909375070178, -0.0740295117531, 0.409336197201, 0,
			-0.0373269545854, 0.965544756015, 0.257545774263, 0,
			-0.414298441997, -0.249485712232, 0.875279623743, 0,
			0, 0, 0, 1,
			-2.1925264022, -1.6757205209, -3.18389057922};
		int ai = 0;

		FOR(j,0,4) FOR(k,0,4) _rotate[j][k] = A[ai++];
		FOR(j,0,3) _translate[3][j] = A[ai++];
		_modelview = _translate * _rotate;
	}
}


void RenderingWindow::init() {
	glClearColor(0,0,0,1);
	glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POINT_SMOOTH);

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(45.f, 1.f, .1f, 200.f);
	mglReadMatrix(GL_PROJECTION_MATRIX, _proj);
	glPopMatrix();
}


// Describes each node in the pyramid in the de Boor Algorithm
struct PyramidNode
{
	// Denote parameters t_l or t_r along the up-left (t-t_l) or up-right (t_r-t) arrow
	double knotL, knotR;
	// Coordinates of the point
	Pt3 point;
};

static Pt3 localDeBoor(int deg, double t, vector<PyramidNode> layer)
{
	// Run the local de Boor Algorithm on this segment
	for(int i = deg; i >= 1; --i)
	{
		// At this moment, we have i+1 points in the vector 'points'.

		// Compute the new points
		vector<PyramidNode> newLayer(i);
		for(int j = 0; j < i; ++j)
		{
			double ta = layer[j + 1].knotL;
			double tb = layer[j].knotR;
			newLayer[j].knotL = ta;
			newLayer[j].knotR = tb;
			newLayer[j].point =
				layer[j].point * ((tb-t) / (tb-ta)) +
				layer[j+1].point * ((t-ta) / (tb-ta));
		}
		// Replace the current layer (i+1 points) with the new layer (i points)
		layer = move(newLayer);
	}

	return move(layer[0].point); // the top of the de Boor pyramid
}

// Update the index 'p' to cover the appropriate knot values given a particular parameter t
static void updateSegmentIndex(int &p, int n, double t, const vector<double> &knots)
{
	while(p + 1 < n && (t > knots[p + 1] + 1e-9 || abs(knots[p] - knots[p + 1]) < 1e-9))
		++p;
}

// Retrieve parameters t_l or t_r along the up-left (t-t_l) or up-right (t_r-t) arrow
static void populateKnotLR(PyramidNode &node, int p, int deg, const vector<double> &knots)
{
	int idL = p - deg;
	int idR = p + 1;
	node.knotL = (idL < 0) ? 0 : knots[idL];
	node.knotR = (idR >= (int) knots.size()) ? 0 : knots[idR];
}

void RenderingWindow::createScene(const TMesh *T)
{
	if(T->rows * T->cols == 0)
	{
		vector<pair<Pt3, int>> P;

		if(T->rows == 0) // 1 x (C+1) grid
		{
			if(1) // Interpolate points
			{
				const int N = 1000; // fixed for now
				P.resize(N + 1);
				double t0 = T->knotsH[T->colDeg - 1];
				double t1 = T->knotsH[T->cols];
				double dt = (t1 - t0) / N;

				int p = T->colDeg - 1;
				for(int i = 0; i <= N; ++i)
				{
					double t = t0 + dt * i;

					// Find the correct segment for this parameter t
					updateSegmentIndex(p, T->cols, t, T->knotsH);

					// Collect the initial control points for this segment
					vector<PyramidNode> baseLayer(T->colDeg + 1);
					for(int j = 0; j <= T->colDeg; ++j)
					{
						int r1 = 0;
						int c1 = j + p - T->colDeg + 1;
						baseLayer[j].point = T->gridPoints[r1][c1]->getCenter();
						populateKnotLR(baseLayer[j], j + p, T->colDeg, T->knotsH);
					}

					/*
					 * Evaluate at t - run the local de Boor Algorithm on this segment
					 * Also store the index p for segment verification (for visualization)
					 */
					P[i] = {localDeBoor(T->colDeg, t, move(baseLayer)), p};
				}
			}
			if(0) // Use the control points directly
			{
				P.resize(T->cols + 1);
				for(int i = 0; i <= T->cols; ++i)
					P[i] = {T->gridPoints[0][i]->getCenter(), 0};
			}
		}
		else // (R+1) x 1 grid
		{
			// TODO: make this the same as 1 x (C+1) grid (though, it's redundant)
			P.resize(T->rows + 1);
			for(int i = 0; i <= T->rows; ++i)
				P[i] = {T->gridPoints[i][0]->getCenter(), 0};
		}
		createCurveScene(P);
	}
	else
	{
		VVP3 S;
		// Interpolate points on the B-spline surface
		const int RN = 100;
		const int CN = 100;
		S.resize(RN + 1, VP3(CN + 1));

		double s0 = T->knotsV[T->rowDeg - 1];
		double s1 = T->knotsV[T->rows];
		double ds = (s1 - s0) / RN;

		double t0 = T->knotsH[T->colDeg - 1];
		double t1 = T->knotsH[T->cols];
		double dt = (t1 - t0) / CN;

		int rp = T->rowDeg - 1;
		for(int ri = 0; ri <= RN; ++ri)
		{
			double s = s0 + ds * ri;

			// Find the correct segment for this parameter s
			updateSegmentIndex(rp, T->rows, s, T->knotsV);

			int cp = T->colDeg - 1;
			for(int ci = 0; ci <= CN; ++ci)
			{
				double t = t0 + dt * ci;

				// Find the correct segment for this parameter t
				updateSegmentIndex(cp, T->cols, t, T->knotsH);

				// ** Process vertical (s) then horizontal (t) directions

				// Collect the initial control points for this horizontal segment
				vector<PyramidNode> pointsH(T->colDeg + 1);
				for(int c = 0; c <= T->colDeg; ++c)
				{
					// Collect the initial control points for each vertical segment
					vector<PyramidNode> pointsV(T->rowDeg + 1);
					const int c1 = c + cp - T->colDeg + 1;
					for(int r = 0; r <= T->rowDeg; ++r)
					{
						const int r1 = r + rp - T->rowDeg + 1;
						pointsV[r].point = T->gridPoints[r1][c1]->getCenter();
						populateKnotLR(pointsV[r], r + rp, T->rowDeg, T->knotsV);
					}

					// Run the local de Boor Algorithm on this vertical segment
					pointsH[c].point = localDeBoor(T->rowDeg, s, move(pointsV));
					populateKnotLR(pointsH[c], c + cp, T->colDeg, T->knotsH);
				}

				// Run the local de Boor Algorithm on this horizontal segment
				S[ri][ci] = localDeBoor(T->colDeg, t, move(pointsH));
			}
		}
		createTriScene(S);
	}
}

void RenderingWindow::draw()
{
	if(!valid()) init();

	sceneLock.lock();
	if(_scene) {

		glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		mglLoadMatrix(_proj);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		mglLoadMatrix(*_scene->getModelview());

		_renderer.draw();

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		swap_buffers();
	}
	sceneLock.unlock();
}

int RenderingWindow::handle(int ev)
{
	if(ev == FL_MOVE || ev == FL_DRAG || ev == FL_PUSH || ev == FL_RELEASE) // mouse
	{
		int x = Fl::event_x();
		int y = Fl::event_y();

		if(ev == FL_DRAG)
		{
			if(Fl::event_button1()) {
				if(Fl::event_alt())
					handlePan(x, y, false);
				else
					handleRot(x, y, false);
			}
			else if(Fl::event_button3())
				handleZoom(x, y, false);
		}
		else if(ev == FL_PUSH)
		{
			if(Fl::event_button1()) {
				if(Fl::event_alt())
					handlePan(x, y, true);
				else
					handleRot(x, y, true);
			}
			else if(Fl::event_button3())
				handleZoom(x, y, true);
		}

		_prevMx = x;
		_prevMy = y;
	}
	else if(ev == FL_KEYUP || ev == FL_KEYDOWN)
	{
		if(ev == FL_KEYDOWN)
		{
			int key = Fl::event_key();
			int step = 15;

			if(key >= 'A' && key <= 'Z') // UPPERCASE -> lowercase
				key = key-'A'+'a';

			// e and q rotates the camera with respect to the forward looking axis
			if(key == 'e' || key == 'q') {
				Mat4 mat;

				// using gl matrices out of laziness
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadIdentity();
				if(key == 'q') // CCW roll
					glRotatef(1, 0, 0, -1);
				else // CW roll
					glRotatef(-1, 0, 0, -1);
				mglReadMatrix(GL_MODELVIEW_MATRIX, mat);
				glPopMatrix();

				Mat4* rot = _scene->getRotate();
				(*rot) = (*rot) * mat;
				updateModelView();
			}
			// pans left or right,
			else if(key == 'd' || key == 'a') {
				Ray r0 = getMouseRay(_w/2, _h/2);
				_panVec = r0.p + r0.dir*3.f;

				Ray r1;
				if(key == 'd')
					r1 = getMouseRay(_w/2-step, _h/2);
				else
					r1 = getMouseRay(_w/2+step, _h/2);

				Pt3 p = r1.p + r1.dir*3.f;
				Vec3 v = (p-_panVec);
				Mat4* trans = _scene->getTranslate();
				(*trans)[3][0] += v[0];
				(*trans)[3][1] += v[1];
				(*trans)[3][2] += v[2];
				updateModelView();
			}
			// pans up or down
			else if(key == 'w' || key == 's') {
				Ray r0 = getMouseRay(_w/2, _h/2);
				_panVec = r0.p + r0.dir*3.f;

				Ray r1;
				if(key == 'w')
					r1 = getMouseRay(_w/2, _h/2+step);
				else
					r1 = getMouseRay(_w/2, _h/2-step);

				Pt3 p = r1.p + r1.dir*3.f;
				Vec3 v = (p-_panVec);
				Mat4* trans = _scene->getTranslate();
				(*trans)[3][0] += v[0];
				(*trans)[3][1] += v[1];
				(*trans)[3][2] += v[2];
				updateModelView();
			}
			// toggle wireframes
			else if(key == 'f')
			{
				_renderer.drawWire ^= 1;
			}
			// toggle shading mode
			else if(key == 'g')
			{
				_renderer.setShadingModel(_renderer.getShadingModel() ^ SHADE_FLAT ^ SHADE_GOURAUD);
			}
			// toggle lighting mode (use normal as color, or use lighting)
			else if(key == 'v')
			{
				_renderer.useNormal ^= 1;
			}
		}
	}

	return 0;
	//	return Fl_Gl_Window::handle(ev);
}

RenderingWindow::~RenderingWindow() {}

void RenderingWindow::resize(int x0, int y0, int w, int h) {
	Fl_Window::resize(x0, y0, w, h);
	_w = w;
	_h = h-(WIN_LOWER_SPACE+MENU_SPACE);

	glViewport(0, 0, _w, _h);
	if(_arcBall)
		_arcBall->setBounds((float)w, (float)h);
}

// handles the camera rotation (arcball)
void RenderingWindow::handleRot(int x, int y, bool beginRot)
{
	if(beginRot) {
		ArcBall::Tuple2f_t mousePt;
		mousePt.s.X = (float) x/2.0;
		mousePt.s.Y = (float) y/2.0;

		_lastRot = (*_scene->getRotate());
		_arcBall->click(&mousePt);
	}
	else {
		ArcBall::Tuple2f_t mousePt;
		mousePt.s.X = (float)x/2.0;
		mousePt.s.Y = (float)y/2.0;

		ArcBall::Quat4fT thisQuat;
		ArcBall::Matrix3f_t thisRot;
		_arcBall->drag(&mousePt, &thisQuat);
		ArcBall::Matrix3fSetRotationFromQuat4f(&thisRot, &thisQuat);

		Mat4* rot = _scene->getRotate();
		convertMat(thisRot, *rot);
		(*rot) = _lastRot * (*rot);
		updateModelView();
	}
}

// handles the camera zoom
void RenderingWindow::handleZoom(int x, int y, bool beginZoom)
{
	int dy = y - _prevMy;

	if(beginZoom) {
		Ray r = getMouseRay(_w/2, _h/2);
		_zoomVec = -r.dir;
	}
	else {
		Mat4* trans = _scene->getTranslate();
		(*trans)[3][0] += _zoomVec[0]*dy*.03f;
		(*trans)[3][1] += _zoomVec[1]*dy*.03f;
		(*trans)[3][2] += _zoomVec[2]*dy*.03f;
		updateModelView();
	}
}

// handles camera pan
void RenderingWindow::handlePan(int x, int y, bool beginPan)
{
	int dy = y - _prevMy;

	if(beginPan) {
		Ray r = getMouseRay(x, y);
		_panVec = r.p + r.dir*3.f;
	}
	else {
		Ray r = getMouseRay(x, y);
		Pt3 p = r.p + r.dir*3.f;
		Vec3 v = (p-_panVec);
		Mat4* trans = _scene->getTranslate();
		(*trans)[3][0] += v[0];
		(*trans)[3][1] += v[1];
		(*trans)[3][2] += v[2];
		updateModelView();
	}
}

// gets the ray shooting into the TMeshScene corresponding to a position on the screen
Ray RenderingWindow::getMouseRay(int mx, int my) {
	if(_scene) {
		GLdouble mv[16], proj[16];
		GLint viewport[4] = {0, 0, _w, _h};

		mLoadMatrix(*_scene->getModelview(), mv);
		mLoadMatrix(_proj, proj);
		GLdouble ax, ay, az;
		GLdouble bx, by, bz;
		gluUnProject(mx, (_h-my), 0.f, mv, proj, viewport, &ax, &ay, &az);
		gluUnProject(mx, (_h-my), .5f, mv, proj, viewport, &bx, &by, &bz);

		Ray ret(
			Pt3(ax, ay, az),
			Vec3((bx-ax), (by-ay), (bz-az), 0));

		ret.dir.normalize();

		return ret;
	}
	return Ray();
}

Pt3 RenderingWindow::getMousePoint(int mx, int my) {
	if(_scene) {
		GLdouble mv[16], proj[16];
		GLint viewport[4] = {0, 0, _w, _h};

		mLoadMatrix(*_scene->getModelview(), mv);
		mLoadMatrix(_proj, proj);
		GLdouble ax, ay, az;
		gluUnProject(mx, (_h-my), 0.f, mv, proj, viewport, &ax, &ay, &az);

		return Pt3(ax, ay, az);
	}
	return Pt3(0, 0, 0);
}



void MeshRenderer::setView(View* view) {
	_view = view;
	initLights();
}

void MeshRenderer::draw() {
	if(_scene)
	{
//		printf("opengl draw\n");

		initLights();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);

		Material* mat = _scene->getMaterial();

		Color amb = mat->getAmbient();
		Color spec = mat->getSpecular();
		Color diffuse = mat->getDiffuse();
		float ambv[4] = {(float)amb[0], (float)amb[1], (float)amb[2], 1.f};
		float specv[4] = {(float)spec[0], (float)spec[1], (float)spec[2], 1.f};
		float diffv[4] = {(float)diffuse[0], (float)diffuse[1], (float)diffuse[2], 1.f};
		float sxp = (float) mat->getSpecExponent();

		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffv);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specv);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambv);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, sxp);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// Draw the interpolated curve
		if(_scene->willDrawCurve())
		{
			vector<pair<Pt3, int>> &P = _scene->getCurve();
			glShadeModel(GL_FLAT);
			glColor4d(1,1,1,1);
			glDisable(GL_LIGHTING);
			glDisable(GL_COLOR_MATERIAL);

			for(int j = 0; j < 2; ++j)
			{
				if(j == 0) // First draw lines
				{
					glLineWidth(2);
					glBegin(GL_LINE_STRIP);
				}
				else // Then draw points
				{
					glPointSize(2);
					glBegin(GL_POINTS);
				}

				for(int i = 0; i < (int) P.size(); ++i)
				{
					int a = P[i].second % 3;
					glColor3d(a == 0, a == 1, a == 2);
					glVertex3dv(&P[i].first[0]);
				}
				glEnd();
			}
		}
		else // Draw the interpolated surface
		{
			TriMesh* m = _scene->getMesh();
			Pt3Array* pts = m->getPoints();
			TriIndArray* inds = m->getInds();
			Vec3Array* vnorms = m->getVNormals();
			Vec3Array* fnorms = m->getFNormals();

			auto normColor = [&](const Pt3 &norm)
			{
				double color[3];
				double sum = 0;
				FOR(i,0,3)
				{
					double a = abs(norm[i]);
					a *= a;
					sum += color[i] = a;
				}
				FOR(i,0,3)
					color[i] /= sum;
				glColor3dv(color);
			};

			// Use normals, no lighting
			if(useNormal)
			{
				glDisable(GL_LIGHTING);
				glDisable(GL_COLOR_MATERIAL);
				if(getShadingModel() == SHADE_GOURAUD)
				{
					glShadeModel(GL_SMOOTH);
					glBegin(GL_TRIANGLES);
					FOR(j,0,inds->size())
					{
						TriInd& ti = inds->get(j);
						FOR(k,0,3)
						{
							normColor(vnorms->get(ti[k]));
							glVertex3dv(&pts->get(ti[k])[0]);
						}
					}
					glEnd();
				}
				else
				{
					glShadeModel(GL_FLAT);
					glBegin(GL_TRIANGLES);
					FOR(j,0,inds->size())
					{
						normColor(fnorms->get(j));
						TriInd& ti = inds->get(j);
						FOR(k,0,3) glVertex3dv(&pts->get(ti[k])[0]);
					}
					glEnd();
				}
			}
			else // Use lighting and material
			{
				glEnable(GL_LIGHTING);
				glEnable(GL_COLOR_MATERIAL);
				glColor3d(1.2, 1.2, 1.2);
				if(getShadingModel() == SHADE_GOURAUD)
				{
					glShadeModel(GL_SMOOTH);
					glBegin(GL_TRIANGLES);
					FOR(j,0,inds->size())
					{
						TriInd& ti = inds->get(j);
						FOR(k,0,3)
						{
							glNormal3dv(&vnorms->get(ti[k])[0]);
							glVertex3dv(&pts->get(ti[k])[0]);
						}
					}
					glEnd();
				}
				else
				{
					glShadeModel(GL_FLAT);
					glBegin(GL_TRIANGLES);
					FOR(j,0,inds->size())
					{
						glNormal3dv(&fnorms->get(j)[0]);
						TriInd& ti = inds->get(j);
						FOR(k,0,3) glVertex3dv(&pts->get(ti[k])[0]);
					}
					glEnd();
				}
			}

			// Wireframes
			if(drawWire)
			{
				glDisable(GL_LIGHTING);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				glLineWidth(1);
				glColor3d(0.1, 0.1, 0.1);
				glBegin(GL_TRIANGLES);
				FOR(j,0,inds->size())
				{
					TriInd& ti = inds->get(j);
					FOR(k,0,3) glVertex3dv(&pts->get(ti[k])[0]);
				}
				glEnd();
			}
		}
	}
}

void MeshRenderer::initLights()
{
	glEnable(GL_LIGHTING);

	// 8 is the max # of light sources
	int nlights = min<int>(8, _scene->getNumLights());
	if(nlights > 0) {
		Color amb = _scene->getLight(0)->getAmbient();
		float ambv[4] = {(float)amb[0], (float)amb[1], (float)amb[2], 1.f};

		glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambv);
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

		for(int j = 0; j < 8; j++) {
			int lid = GL_LIGHT0 + j;
			glDisable(lid);
		}

		for(int j = 0; j < nlights; j++) {
			Light* l = _scene->getLight(j);

			int lid = GL_LIGHT0 + j;
			glEnable(lid);

			Color c = l->getColor();
			Pt3 p = l->getPos();
			float cv[4] = {(float)c[0], (float)c[1], (float)c[2], 1.f};
			float pv[4] = {(float)p[0], (float)p[1], (float)p[2], 1.f};

			glLightfv(lid, GL_DIFFUSE, cv);
			glLightfv(lid, GL_SPECULAR, cv);
			glLightfv(lid, GL_POSITION, pv);
		}
		glDisable(GL_COLOR_MATERIAL);
	}
}