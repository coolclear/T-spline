#include "Rendering/ZBufferRenderer.h"
#include "GUI/GeometryWindow.h"

ZBufferRenderer::ZBufferRenderer() {
	_visitor = new ZBufferVisitor();

	_highlighted = NULL;
	_selected = NULL;
	_op = NULL;
	_drawGrid = true;
}

void ZBufferRenderer::initScene()
{
	_highlighted = NULL;
	_selected = NULL;
	_op = NULL;

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);
}

void ZBufferRenderer::draw()
{
	glClearColor(0,0,0,1);
	glDisable(GL_LIGHTING);

	if(_drawGrid)
	{
		const double low = -3;
		const double high = 3;
		const int steps = 20;

/*
		// YZ-plane grid (red)
		glColor3d(0.4, 0.1, 0.1);
		glRotatef(90, 0, 1, 0);
		drawGrid(low, high, steps);
		glRotatef(-90, 0, 1, 0);
//*/

		// XZ-plane grid (green)
		glColor3d(0.1, 0.3, 0.1);
		drawGrid(low, high, steps);
		glRotatef(-90, 1, 0, 0);

		// XY-plane grid (blue)
		glColor3d(0.1, 0.1, 0.4);
		drawGrid(low, high, steps);
		glRotatef(90, 1, 0, 0);

		// Draw thick colorful axes
		glLineWidth(3);

		glBegin(GL_LINES);
		glColor3d(0.5, 0.1, 0.1);
		glVertex3d(low, 0, 0);
		glVertex3d(high, 0, 0);
		glColor3d(0.1, 0.4, 0.1);
		glVertex3d(0, low, 0);
		glVertex3d(0, high, 0);
		glColor3d(0.1, 0.1, 0.5);
		glVertex3d(0, 0, low);
		glVertex3d(0, 0, high);
		glEnd();
	}

	// Draw the control points
	for(const auto &row: _scene->getSpheres())
	{
		for(const auto &sphere: row)
		{
			if(sphere != NULL)
			{
				glColor3d(0.8, 0.8, 0.8);
				sphere->accept(_visitor, NULL);
			}
		}
	}

	if(1) // Draw links between adjacent control points
	{
		glLineWidth(3);
		glBegin(GL_LINES);

		auto &spheres = _scene->getSpheres();
		int rows = spheres.size();
		int cols = spheres[0].size();

		// Draw H-links
		glColor3d(0, 0.4, 0.8);
		for(int r = 0; r < rows; ++r)
		{
			Sphere *last = NULL, *curr;
			for(int c = 0; c < cols; ++c)
			{
				if(!(curr = spheres[r][c]))
					continue;
				if(last)
				{
					glVertex3dv(&last->getCenter()[0]);
					glVertex3dv(&curr->getCenter()[0]);
				}
				last = curr;
			}
		}

		// Draw V-links
		glColor3d(0.8, 0.4, 0);
		for(int c = 0; c < cols; ++c)
		{
			Sphere *last = NULL, *curr;
			for(int r = 0; r < rows; ++r)
			{
				if(!(curr = spheres[r][c]))
					continue;
				if(last)
				{
					glVertex3dv(&last->getCenter()[0]);
					glVertex3dv(&curr->getCenter()[0]);
				}
				last = curr;
			}
		}
		glEnd();
	}

	if(_highlighted) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		_highlighted->accept(_visitor, NULL);

		glDisable(GL_LIGHTING);
		glEnable(GL_COLOR_MATERIAL);
		glPolygonOffset(1, 1);
		glColor4d(1,0,0,.3);
		glLineWidth(6.0);
		glCullFace(GL_FRONT);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		_highlighted->accept(_visitor, NULL);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glPolygonOffset(0, 0);
		glLineWidth(1.f);
		glCullFace(GL_BACK);
		glEnable(GL_LIGHTING);
	}

	if(_op) {
		glEnable(GL_COLOR_MATERIAL);
		_op->accept(_visitor, NULL);
	}

	if(_selected) {
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		Color diffuse(1,1,1,0.7);
		glColor4dv(&diffuse[0]);
		_selected->accept(_visitor, NULL);
		glDisable(GL_BLEND);
	}
}

void ZBufferRenderer::drawGrid(double low, double high, int steps)
{
	double diff = (high - low) / steps;
	double z = 0;

	glLineWidth(1);
	glBegin(GL_LINES);
	for(int i = 0; i <= steps; ++i)
	{
		double x = low + i * diff;
		if(abs(x) < 1e-6) // ignore lines near the axes
			continue;
		glVertex3d(x, low, z);
		glVertex3d(x, high, z);
		glVertex3d(low, x, z);
		glVertex3d(high, x, z);
	}
	glEnd();
}

void ZBufferRenderer::setSelected(Geometry* geom) {
	_selected = geom;
}

void ZBufferRenderer::setHighlighted(Geometry* geom) {
	_highlighted = geom;
}

void ZBufferRenderer::setOperator(Operator* op, int mode) {
	_op = op;
	if(_op != NULL)
		_visitor->setOpMode(mode);
}

int ZBufferRenderer::getOperatorMode() {
	if(_visitor != NULL)
		return _visitor->getOpMode();
	return -1;
}

ZBufferVisitor::ZBufferVisitor() {
	_quadric = gluNewQuadric();
}




//========================================================================
// ZBufferVisitor::visit(shapes)
//
// Rendering in GeometryWindow using OpenGL
//========================================================================

void ZBufferVisitor::visit(Sphere* sphere, void* ret) {
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	Pt3 c = sphere->getCenter();
	glTranslatef(c[0], c[1], c[2]);
	float r = sphere->getRadius();
	gluSphere(_quadric, r, 50, 50);
	glPopMatrix();
}



//========================================================================
// END OF ZBufferVisitor::visit(shapes)
//========================================================================



void drawAxis(GLUquadric* quad) {
	glPushMatrix();
	float step = OP_STEP;
	gluCylinder(quad, .015, .015, step, 20, 20);
	glTranslatef(0.f, 0.f, step);
	gluCylinder(quad, .05, .0, step/2.5, 20, 20);
	glPopMatrix();
}

void ZBufferVisitor::visit(Operator* op, void* ret) {
	Pt3 center = op->getPrimaryOp()->getCenter();

	glDisable(GL_CULL_FACE);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(center[0], center[1], center[2]);

	if(_opMode & OP_MODE_TRANSLATE) {
		glColor3f(.7f, .7f, .7f);
		gluSphere(_quadric, .04, 20, 20);

		if(_opMode & OP_MODE_ZAXIS)
			glColor3f(1.f, 1.f, 0.f);
		else
			glColor3f(0.f, 0.f, 1.f);
		drawAxis(_quadric);

		glRotatef(90.f, 0.f, 1.f, 0.f);
		if(_opMode & OP_MODE_XAXIS)
			glColor3f(1.f, 1.f, 0.f);
		else
			glColor3f(1.f, 0.f, 0.f);
		drawAxis(_quadric);
		glRotatef(-90.f, 0.f, 1.f, 0.f);

		glRotatef(-90.f, 1.f, 0.f, 0.f);
		if(_opMode & OP_MODE_YAXIS)
			glColor3f(1.f, 1.f, 0.0f);
		else
			glColor3f(0.f, 1.f, 0.f);
		drawAxis(_quadric);
	}

	glEnable(GL_CULL_FACE);
	glPopMatrix();
}