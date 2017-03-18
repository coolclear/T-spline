#include "GUI/PropertyWindow.h"
#include <FL/Fl_Color_Chooser.h>
#include <FL/Fl_Menu_Bar.H>
#include "Common/Common.h"

PropertyWindow* PropertyWindow::_singleton = NULL;
Operator* PropertyWindow::_op = NULL;

#define PW_X 700
#define PW_Y 600
#define PW_WIDTH 250
#define PW_HEIGHT 100
#define MW_HEIGHT 80

/*
MaterialColorButton::MaterialColorButton(int x, int y, const char* name)
: Fl_Button(x, y, 20, 20, name) {
	this->align(Fl_Align::FL_ALIGN_LEFT);
	this->box(FL_BORDER_BOX);
}

void MaterialColorButton::setColor(const Color& c) {
	_color = c;
	this->color(fl_rgb_color((uchar)(c[0]*255), (uchar)(c[1]*255), (uchar)(c[2]*255)));
	this->redraw();
}

MaterialWindow::MaterialWindow(int x, int y)
: Fl_Window(x, y, PW_WIDTH-10, MW_HEIGHT) {
	int bskip = 60;
	int bwidth = 20;
	int bspace = 5;
	int sty = 5;

	int iskip = 65;
	int iwidth = 80;
	int iheight = 20;
	int ispace = 5;

	begin();
	_diffuse = new MaterialColorButton(bskip, sty, "Diffuse");
	_specular = new MaterialColorButton(bskip*2 + bwidth + bspace, sty, "Specular");
	_ambient = new MaterialColorButton(bskip*3 + bwidth*2 + bspace*2, sty, "Ambient");

	_diffuse->callback(colorButton, this);
	_specular->callback(colorButton, this);
	_ambient->callback(colorButton, this);

	sty += 25;
	_specExp = new Fl_Float_Input(iskip, sty, iwidth, iheight, "SpecExp");
	_refl = new Fl_Float_Input(iskip*2 + iwidth + ispace, sty, iwidth, iheight, "Reflect");

	sty += 25;
	_trans = new Fl_Float_Input(iskip, sty, iwidth, iheight, "Transp");
	_refr = new Fl_Float_Input(iskip*2 + (iwidth + ispace), sty, iwidth, iheight, "Refract");

	_specExp->callback(valueChanged, this);
	_refl->callback(valueChanged, this);
	_trans->callback(valueChanged, this);
	_refr->callback(valueChanged, this);

	_specExp->box(FL_BORDER_BOX);
	_refl->box(FL_BORDER_BOX);
	_trans->box(FL_BORDER_BOX);
	_refr->box(FL_BORDER_BOX);


	end();

	box(FL_BORDER_BOX);
	this->color(WIN_COLOR);
}

void MaterialWindow::setMaterial(Material* mat) {
	if(mat) {
		_mat = mat;
		_diffuse->setColor(_mat->getDiffuse());
		_specular->setColor(_mat->getSpecular());
		_ambient->setColor(_mat->getAmbient());
		_specExp->value(to_string(_mat->getSpecExponent()).c_str());
		_refl->value(to_string(_mat->getReflective()).c_str());
		_trans->value(to_string(_mat->getTransparency()).c_str());
		_refr->value(to_string(_mat->getRefractIndex()).c_str());
	}
}

void MaterialWindow::colorButton(Fl_Widget* widget, void* p) {
	MaterialWindow* win = (MaterialWindow*) p;

	MaterialColorButton* button = (MaterialColorButton*) widget;
	Color color = button->getColor();
	double r = color[0];
	double g = color[1];
	double b = color[2];

	if(fl_color_chooser("Choose Color", r, g, b)) {
		color = Color(r, g, b);
		button->setColor(color);

		if(button == win->_diffuse)
			win->_mat->setDiffuse(color);
		else if(button == win->_specular)
			win->_mat->setSpecular(color);
		else if(button == win->_ambient)
			win->_mat->setAmbient(color);
	}
}

void MaterialWindow::valueChanged(Fl_Widget* widget, void* p) {
	MaterialWindow* win = (MaterialWindow*) p;

	Fl_Float_Input* input = (Fl_Float_Input*) widget;

	double nv = atof(input->value());
	if(input == win->_specExp)
		win->_mat->setSpecExponent(nv);
	else if(input == win->_refl)
		win->_mat->setReflective(nv);
	else if(input == win->_trans)
		win->_mat->setTransparency(nv);
	else if(input == win->_refr)
		win->_mat->setRefractIndex(nv);
}
*/

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

	{
//		_matWindow = new MaterialWindow(5, sty);
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
		//_singleton->getMaterialWindow()->setMaterial(mat);
	}
}

void PropertyWindow::draw() {
	Fl_Window::draw();
}

Pt3 PropertyWindow::getCenter() { return Pt3(); }

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
// (See spheres and boxes for examples)
//========================================================================

const int dirs[3] = {PROP_X, PROP_Y, PROP_Z};

// Here is the sphere example
void WidgetUpdater::visit(Sphere* sphere, void* ret) {
	Pt3 center = sphere->getCenter();
	double radius = sphere->getRadius();

	for(int j = 0; j < 3; j++) {
		Fl_Float_Input* c = _window->getPos(dirs[j]);

		c->activate();
		c->value(to_string(center[j]).c_str());
	}
}