#include "GUI/PropertyWindow.h"
#include <FL/Fl_Color_Chooser.h>
#include <FL/Fl_Menu_Bar.H>
#include "Common/Common.h"

PropertyWindow* PropertyWindow::_singleton = NULL;
Operator* PropertyWindow::_op = NULL;

#define PW_X 680
#define PW_Y 780
#define PW_WIDTH 220
#define PW_HEIGHT 100
#define MW_HEIGHT 80

PropertyWindow::PropertyWindow() : Fl_Window(PW_X, PW_Y, PW_WIDTH, PW_HEIGHT, "Property") {
	begin();

	int sty = 5;
	{
		int stSpace = PW_WIDTH/4;
		int intSpace = stSpace;
		int compSpace = PW_WIDTH/2;
		int compH = 20;

		_posX = new Fl_Float_Input(stSpace, sty, compSpace, compH, "Pos X");
		_posY = new Fl_Float_Input(stSpace, sty + 30, compSpace, compH, "Pos Y");
		_posZ = new Fl_Float_Input(stSpace, sty + 60, compSpace, compH, "Pos Z");

		_posX->callback(handleAllCb, this);
		_posY->callback(handleAllCb, this);
		_posZ->callback(handleAllCb, this);

		_posX->box(FL_BORDER_BOX);
		_posY->box(FL_BORDER_BOX);
		_posZ->box(FL_BORDER_BOX);

		sty += compH + 10;
	}

	this->callback(escapeButtonCb, this);

	end();

	_geom = NULL;
	_geomUpdater = new GeometryUpdater(this);
	_widgetUpdater = new WidgetUpdater(this);

	this->color(WIN_COLOR);
}

void PropertyWindow::openPropertyWindow(Geometry* geom, Operator* op) {

	if(!_singleton)
		_singleton = new PropertyWindow();

	int x1 = _singleton->x();
	int y1 = _singleton->y();
	int w1 = _singleton->w();
	int h1 = _singleton->h();
	_singleton->show();
	_singleton->resize(x1, y1, w1, h1);
	if(geom && op) {
		_op = op;
		_singleton->setGeometry(geom);
		_op->setSecondaryOp(_singleton);
		_singleton->getGeometry()->accept(_singleton->getWidgetUpdater(), NULL);
	}
}

void PropertyWindow::draw() {
	Fl_Window::draw();
}

Pt3 PropertyWindow::getCenter() const { return Pt3(); }

void PropertyWindow::translate(const Vec3& trans) {
	if(_geom)
		_geom->accept(_widgetUpdater, NULL);
}

void PropertyWindow::closePropertyWindow() {
	if(_singleton) {
		_singleton->hide();
		if(_op) {
			_op->setSecondaryOp(NULL);
			_op = NULL;
		}

		if(_singleton->getGeometry())
			_singleton->setGeometry(NULL);
	}
}

void PropertyWindow::handleAllCb(Fl_Widget* widget, void* w) {
	PropertyWindow* win = (PropertyWindow*) w;
	win->getGeometry()->accept(win->getGeometryUpdater(), NULL);
}

void PropertyWindow::escapeButtonCb(Fl_Widget* widget, void* win) {
	closePropertyWindow();
}



//========================================================================
// GeometryUpdater::visit(shapes)
//
// For updating object properties from the Property Window
// (See spheres and boxes for examples)
//========================================================================


void GeometryUpdater::visit(Sphere* sphere, void* ret) {
	Fl_Float_Input* cx = _window->getPos(PROP_X);
	Fl_Float_Input* cy = _window->getPos(PROP_Y);
	Fl_Float_Input* cz = _window->getPos(PROP_Z);
	Pt3 ncenter = Pt3(atof(cx->value()), atof(cy->value()), atof(cz->value()));
	sphere->setCenter(ncenter);
}




//========================================================================
// WidgetUpdater::visit(shapes)
//
// For showing certain object properties on the Property Window
// and allowing changes to such properties
//========================================================================

const int dirs[3] = {PROP_X, PROP_Y, PROP_Z};

void WidgetUpdater::visit(Sphere* sphere, void* ret) {
	Pt3 center = sphere->getCenter();
	double radius = sphere->getRadius();

	for(int j = 0; j < 3; j++) {
		Fl_Float_Input* c = _window->getPos(dirs[j]);

		c->activate();
		c->value(to_string(center[j]).c_str());
	}
}