#include "TopologyWindow.h"
#include "Common/Common.h"

#include <iostream>

using namespace std;

const int WIN_LOWER_SPACE = 30;

static void renderButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if (topology)
		topology->prepRenderer();
}

static void loadButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if (topology)
		topology->loadMesh();
}

static void saveButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if (topology)
		topology->saveMesh();
}

TopologyWindow::TopologyWindow(int x, int y, int w, int h, const char* l)
	: Fl_Window(x,y,w,h+WIN_LOWER_SPACE,l)
{
	tv = new TopologyViewer(5,5,300,300,"tv");
	tv->box(FL_FLAT_BOX);
	tv->color(FL_BLACK);
	tv->end();

	_mesh = new TMesh(2, 4, 1, 1);
	tv->setMesh(_mesh);

	renderButton = new Button(320, 10, 60, 20, "Render");
	renderButton->callback(renderButtonCallback, this);

	loadButton = new Button(320, 50, 60, 20, "Load");
	loadButton->callback(loadButtonCallback, this);

	saveButton = new Button(390, 50, 60, 20, "Save");
	saveButton->callback(saveButtonCallback, this);

	show();
	resize(x, y, w, h);

	this->callback(escapeButtonCb,this);
	this->color(WIN_COLOR);
}

TopologyWindow::~TopologyWindow()
{
	delete tv;
	delete renderButton;
	delete loadButton;
	delete saveButton;
}

void TopologyWindow::prepGeometry()
{
	if(_mesh && _geometry)
		_geometry->setupControlPoints(_mesh);
}

void TopologyWindow::prepRenderer()
{
	if(_mesh && _renderer)
	{
		_mesh->lock.lock();
		_renderer->createScene(_mesh);
		_mesh->lock.unlock();
	}
}

void TopologyWindow::loadMesh()
{
	char* filePath = fl_file_chooser("Open T-Mesh", ".txt (*.txt)", "./files", 0);
	if(!filePath)
	{
		printf("Canceled loading T-mesh\n");
		return;
	}

	if(!_mesh)
		_mesh = new TMesh(1, 1, 1, 1);
	if(_mesh->meshFromFile(filePath))
	{
		prepGeometry();
		prepRenderer();
	}
}

void TopologyWindow::saveMesh()
{
	printf("not yet implemented\n");
}