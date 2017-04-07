#include "GUI/GeometryWindow.h"
#include "GUI/PropertyWindow.h"

using namespace std;

#define frand() (rand()/(float)RAND_MAX)

#define MENU_OPTION_COLOR 0
#define MENU_OPTION_DELETE 1

#define INPUT_VIEWING 0
#define INPUT_SELECTING 1
#define INPUT_EDITING 2
#define INPUT_TRANS 3
#define INPUT_ROT 4

int GeometryWindow::_frames = 0;
int GeometryWindow::_w = 0;
int GeometryWindow::_h = 0;
double GeometryWindow::_lastTime = 0.f;
double GeometryWindow::_fps = 0.f;
Mat4 GeometryWindow::_proj;
Mat4 GeometryWindow::_lastRot;

int GeometryWindow::_prevMx = 0;
int GeometryWindow::_prevMy = 0;
double GeometryWindow::_prevRot = 0;
Pt3 GeometryWindow::_prevMPt;
int GeometryWindow::_buttonSt = -1;
int GeometryWindow::_button = -1;
static bool ctrl = false;
static bool shift = false;
int GeometryWindow::_holdAxis = -1;
ArcBall::ArcBall_t* GeometryWindow::_arcBall = NULL;

Vec3 GeometryWindow::_panVec = Vec3(0, 0, 0, 0);
Vec3 GeometryWindow::_zoomVec = Vec3(0, 0, 0, 0);

Intersector* GeometryWindow::_intersector = NULL;
std::map<Geometry*, Operator*> GeometryWindow::_geom2op;
int GeometryWindow::_inputMode = INPUT_VIEWING;
Geometry* GeometryWindow::_highlighted = NULL;

GeometryWindow* GeometryWindow::_singleton = NULL;

bool GeometryWindow::_drawGrid = true;
bool GeometryWindow::_drawControlPoints = true;
bool GeometryWindow::_drawSurface = true;

mutex GeometryWindow::sceneLock;
TMeshScene GeometryWindow::_meshScene;
TriMeshScene GeometryWindow::_scene;
MeshRenderer GeometryWindow::_renderer;
ZBufferRenderer GeometryWindow::_zbuffer;

const int WIN_LOWER_SPACE = 0;
const int MENU_SPACE = 0;
const double MOUSE_TOL = 8.f;

void GeometryWindow::updateModelView()
{
	SceneInfo::updateModelView();
}

void GeometryWindow::setupControlPoints(TMesh *tmesh)
{
	sceneLock.lock();

	tmesh->lock.lock();
	_meshScene.setup(tmesh);
	tmesh->lock.unlock();

	_geom2op.clear();
	for(const auto &row: _meshScene.gridSpheres)
		for(const auto &pso: row)
			_geom2op[pso.first] = pso.second;

	_zbuffer.setScene(&_meshScene);
	_zbuffer.initScene();

	sceneLock.unlock();
}

void GeometryWindow::setupSurface(TMesh *tmesh)
{
	static TMesh *lastT = NULL;
	if(tmesh != NULL) // update to the new non-null mesh object
		lastT = tmesh;

	sceneLock.lock();

	lastT->lock.lock();
	_scene.setScene(lastT);
	lastT->lock.unlock();

	_renderer.setTriMeshScene(&_scene);

	sceneLock.unlock();
}

GeometryWindow::GeometryWindow(int x, int y, int w, int h, const char* l)
	: Fl_Gl_Window(x, y, w, h+WIN_LOWER_SPACE, l)
{
	show();
	resize(x, y, w, h);

	begin();
	glutInitWindowSize(0, 0);
	glutInitWindowPosition(-1, -1); // place it inside parent window
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glutCreateWindow("");
	end();

	_w = w;
	_h = h;
	_lastTime = clock()/((double)CLOCKS_PER_SEC);

	_arcBall = new ArcBall::ArcBall_t((float)_w, (float)_h);
	_intersector = new Intersector();

	this->callback(escapeButtonCb,this);
	Fl::repeat_timeout(REFRESH_RATE, GeometryWindow::updateCb, this);

	GeometryWindow::init();
	_singleton = this;

	show();
}

void GeometryWindow::init()
{
	glClearColor(0, 0, 0, 1);
	glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POINT_SMOOTH);

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_COLOR_MATERIAL);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(45.f,1.f,.1f,200.f);
	mglReadMatrix(GL_PROJECTION_MATRIX,_proj);

	glViewport(0, 0, _w, _h);
}

void GeometryWindow::draw()
{
	sceneLock.lock();

	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(45.f, 1.f, .1f, 200.f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	mglLoadMatrix(*_meshScene.getModelview());
	Mat4 &m = *_meshScene.getModelview();

	glShadeModel(GL_SMOOTH);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	// Drawing starts here ----------------------------

	_zbuffer.turnOnGrid(_drawGrid);
	_zbuffer.turnOnControlPoints(_drawControlPoints);
	_zbuffer.draw();

	if(_drawSurface)
		_renderer.draw();

	// Drawing ends here ------------------------------

	sceneLock.unlock();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	swap_buffers();
}

int GeometryWindow::handle(int ev)
{
	bool ctrl1 = Fl::event_state() & FL_CTRL;
	bool shift1 = Fl::event_state() & FL_SHIFT;

	if(ev == FL_MOVE || ev == FL_DRAG || ev == FL_PUSH || ev == FL_RELEASE) // mouse
	{
		int x = Fl::event_x();
		int y = Fl::event_y();

		if(_inputMode == INPUT_VIEWING)
		{
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
		}
		else if(_inputMode == INPUT_TRANS) {
			if(_holdAxis >= 0) {
				handleAxisTrans(x, y, false);
			}
			if(ev == FL_RELEASE) {
				Operator* op = _zbuffer.getOperator();
				_inputMode = INPUT_EDITING;
				_holdAxis = -1;
				_zbuffer.setOperator(op, OP_MODE_TRANSLATE);

				Sphere *sphere = dynamic_cast<Sphere *>(op->getPrimaryOp());
				_meshScene.updateSphere(sphere);
				setupSurface(NULL);
			}
		}
		else if(_inputMode == INPUT_EDITING) {
			if(ev == FL_MOVE || ev == FL_DRAG)
			{
				Operator* op = _zbuffer.getOperator();
				if(op) {
					IsectAxisData data;
					Ray r = getMouseRay(x, y);
					_intersector->setRay(r);
					op->accept(_intersector, &data);

					int editMode = OP_MODE_TRANSLATE;

					if(data.hit) {
						if(_zbuffer.getOperatorMode() != (editMode|data.axis))
							_zbuffer.setOperator(op, editMode|data.axis);
						_holdAxis = data.axis;
					}
					else {
						if(_zbuffer.getOperatorMode() != editMode)
							_zbuffer.setOperator(op, editMode);
						_holdAxis = -1;
					}

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
				}
				else
					_inputMode = INPUT_VIEWING;
			}
			else if(ev == FL_PUSH)
			{
				if(!_highlighted)
				{
					if(Fl::event_button1())
					{
						if(_holdAxis >= 0)
						{
							_inputMode = INPUT_TRANS;
							handleAxisTrans(x, y, true);
						}
						else
						{
							if(Fl::event_alt())
								handlePan(x, y, true);
							else
								handleRot(x, y, true);
						}
					}
					else if(Fl::event_button3())
						handleZoom(x, y, true);
				}
			}
		}
		else if(_inputMode == INPUT_SELECTING) {
			if(ev == FL_MOVE || ev == FL_DRAG)
			{
				IsectData data;

				Ray r = getMouseRay(x, y);
				_intersector->setRay(r);

				double t = 1e12; // INF
				Geometry* hobj = NULL;
				sceneLock.lock();
				FOR(r,0,_meshScene.rows + 1) FOR(c,0,_meshScene.cols + 1)
				{
					if(_meshScene.useSphere(r,c))
					{
						Sphere *const sphere = _meshScene.gridSpheres[r][c].first;
						sphere->accept(_intersector, &data);
						if(data.hit && t > data.t)
						{
							t = data.t;
							hobj = sphere;
						}
					}
				}
				sceneLock.unlock();

				_highlighted = hobj;
			}
			else if(ev == FL_RELEASE)
			{
				_zbuffer.setSelected(_highlighted);
				if(_highlighted) {
					_zbuffer.setOperator(_geom2op[_highlighted], OP_MODE_TRANSLATE);
					_zbuffer.getOperator()->setState(OP_TRANSLATE);
					_inputMode = INPUT_EDITING;
					PropertyWindow::openPropertyWindow(_highlighted, _geom2op[_highlighted]);

					_singleton->show(); // Prevents the property window to steal the first focus.
				}
				else {
					if(_zbuffer.getOperator()) {
						_zbuffer.getOperator()->setState(OP_NONE);
						_zbuffer.setSelected(NULL);
						_zbuffer.setOperator(NULL, 0);
						PropertyWindow::closePropertyWindow();
					}
				}

				_highlighted = NULL;
			}
		}

		_zbuffer.setHighlighted(_highlighted);
		_prevMx = x;
		_prevMy = y;
	}
	else if(ev == FL_KEYUP || ev == FL_KEYDOWN)
	{
		if(!ctrl && ctrl1)
			_inputMode = INPUT_SELECTING;
		else if(ctrl && !ctrl1)
		{
			if(_inputMode == INPUT_SELECTING && _zbuffer.getSelected() == NULL) {
				_inputMode = INPUT_VIEWING;
			}
			else if(_zbuffer.getSelected() != NULL) {
				_inputMode = INPUT_EDITING;
			}

			_highlighted = NULL;
			_zbuffer.setHighlighted(_highlighted);
		}
		if(shift && !shift1) {
			if(_inputMode == INPUT_EDITING) {
				Operator* op = _zbuffer.getOperator();
				_zbuffer.setOperator(op, OP_MODE_TRANSLATE);
				_zbuffer.getOperator()->setState(OP_TRANSLATE);
				_holdAxis = -1;
			}
		}

		ctrl = ctrl1;
		shift = shift1;

		if(ev == FL_KEYDOWN)
		{
			int key = Fl::event_key();
			int step = 15;

			if(key >= 'A' && key <= 'Z') // UPPERCASE -> lowercase
				key = key-'A'+'a';

//			if(key == 'r') prepScene();

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

				Mat4* rot = _meshScene.getRotate();
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
				Mat4* trans = _meshScene.getTranslate();
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
				Mat4* trans = _meshScene.getTranslate();
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
			// toggle axis lines
			else if(key == 'z')
			{
				_drawGrid ^= 1;
			}
			// toggle showing control points
			else if(key == 'x')
			{
				_drawControlPoints ^= 1;
			}
			// toggle showing the surface
			else if(key == 'c')
			{
				_drawSurface ^= 1;
			}
		}
	}

	// TODO: which one to use?
	return 0;
//	return Fl_Gl_Window::handle(ev);
}

GeometryWindow::~GeometryWindow() {}

void GeometryWindow::resize(int x0, int y0, int w, int h) {
	Fl_Window::resize(x0, y0, w, h);
	_w = w;
	_h = h-(WIN_LOWER_SPACE+MENU_SPACE);

	if(_arcBall)
		_arcBall->setBounds((float)w, (float)h);
}

// handles the camera rotation (arcball)
void GeometryWindow::handleRot(int x, int y, bool beginRot) {
	if(beginRot) {
		ArcBall::Tuple2f_t mousePt;
		mousePt.s.X = (float) (x/2.0);
		mousePt.s.Y = (float) (y/2.0);

		_lastRot = (*_meshScene.getRotate());
		_arcBall->click(&mousePt);
	}
	else {
		ArcBall::Tuple2f_t mousePt;
		mousePt.s.X = (float) (x/2.0);
		mousePt.s.Y = (float) (y/2.0);

		ArcBall::Quat4fT thisQuat;
		ArcBall::Matrix3f_t thisRot;
		_arcBall->drag(&mousePt, &thisQuat);
		ArcBall::Matrix3fSetRotationFromQuat4f(&thisRot, &thisQuat);

		Mat4* rot = _meshScene.getRotate();
		convertMat(thisRot, *rot);
		(*rot) = _lastRot * (*rot);
		updateModelView();
	}
}

// handles the camera zoom
void GeometryWindow::handleZoom(int x, int y, bool beginZoom)
{
	double dy = y - _prevMy;

	if(beginZoom) {
		Ray r = getMouseRay(_w/2, _h/2);
		_zoomVec = -r.dir;
	}
	else {
		dy *= 0.03;
		Mat4* trans = _meshScene.getTranslate();
		(*trans)[3][0] += _zoomVec[0] * dy;
		(*trans)[3][1] += _zoomVec[1] * dy;
		(*trans)[3][2] += _zoomVec[2] * dy;
		updateModelView();
	}
}

// handles camera pan
void GeometryWindow::handlePan(int x, int y, bool beginPan)
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
		Mat4* trans = _meshScene.getTranslate();
		(*trans)[3][0] += v[0];
		(*trans)[3][1] += v[1];
		(*trans)[3][2] += v[2];
		updateModelView();
	}
}

// gets the ray shooting into the scene corresponding to a position on the screen
Ray GeometryWindow::getMouseRay(int mx, int my)
{
	GLdouble mv[16], proj[16];
	GLint viewport[4] = {0, 0, _w, _h};

	mLoadMatrix(*_meshScene.getModelview(), mv);
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

Pt3 GeometryWindow::getMousePoint(int mx, int my)
{
	GLdouble mv[16], proj[16];
	GLint viewport[4] = {0, 0, _w, _h};

	mLoadMatrix(*_meshScene.getModelview(), mv);
	mLoadMatrix(_proj, proj);
	GLdouble ax, ay, az;
	gluUnProject(mx, (_h-my), 0.f, mv, proj, viewport, &ax, &ay, &az);

	return Pt3(ax, ay, az);
}

// This handles the case when an operator translation is performed
void GeometryWindow::handleAxisTrans(int mx, int my, bool beginTrans) {
	Operator* op = _zbuffer.getOperator();
	Ray r = getMouseRay(mx, my);
	Pt3 np;
	switch(_holdAxis) {
		case OP_XAXIS: np = op->getDirX()+op->getPrimaryOp()->getCenter(); break;
		case OP_YAXIS: np = op->getDirY()+op->getPrimaryOp()->getCenter(); break;
		case OP_ZAXIS: np = op->getDirZ()+op->getPrimaryOp()->getCenter(); break;
	}

	Pt3 cpt = r.at(GeometryUtils::pointRayClosest(np, r));
	if(!beginTrans) {
		switch(_holdAxis) {
			case OP_XAXIS: op->translate(Vec3(cpt[0]-_prevMPt[0], 0, 0, 0)); break;
			case OP_YAXIS: op->translate(Vec3(0, cpt[1]-_prevMPt[1], 0, 0)); break;
			case OP_ZAXIS: op->translate(Vec3(0, 0, cpt[2]-_prevMPt[2], 0)); break;
		}
	}
	_prevMPt = cpt;
}