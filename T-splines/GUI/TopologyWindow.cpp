#include "TopologyWindow.h"
#include "Common/Common.h"

#include <iostream>

using namespace std;

const int WIN_LOWER_SPACE = 30;

TMesh TopologyWindow::_mesh(1, 1, 1, 1);

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
	_viewer = new TopologyViewer(5,5,300,300,"_viewer");
	_viewer->box(FL_FLAT_BOX);
	_viewer->color(FL_BLACK);
	_viewer->end();

	_viewer->setMesh(&_mesh);

	loadButton = new Button(320, 20, 60, 20, "Load");
	loadButton->callback(loadButtonCallback, this);

	saveButton = new Button(390, 20, 60, 20, "Save");
	saveButton->callback(saveButtonCallback, this);

	show();
	resize(x, y, w, h);

	this->callback(escapeButtonCb,this);
	this->color(WIN_COLOR);
}

TopologyWindow::~TopologyWindow()
{
	delete _viewer;
	delete loadButton;
	delete saveButton;
}

void TopologyWindow::updateControlPoints()
{
	if(_geometry)
	{
		_mesh.lock.lock();
		_geometry->setupControlPoints(&_mesh);
		_mesh.lock.unlock();
	}
}

void TopologyWindow::updateSurface()
{
	if(_geometry)
	{
		_mesh.lock.lock();
		_geometry->setupSurface(&_mesh);
		_mesh.lock.unlock();
	}
}

void TopologyWindow::loadMesh()
{
	char* filePath = fl_file_chooser("Open T-Mesh", ".txt (*.txt)", "./files/", 0);
	if(!filePath)
	{
		fprintf(stderr, "Canceled loading T-mesh\n");
		return;
	}

	if(_mesh.meshFromFile(filePath))
	{
		printf("Loaded [%s] successfully\n", filePath);
		printf("Dimensions: %d x %d\n", _mesh.rows, _mesh.cols);
		printf("Degree: %d x %d\n", _mesh.rowDeg, _mesh.colDeg);

		updateControlPoints();
		updateSurface();
	}
}

void TopologyWindow::saveMesh()
{
	char* filePath = fl_file_chooser("Save T-Mesh", ".txt (*.txt)", "./files/", 0);
	if(!filePath)
	{
		fprintf(stderr, "Canceled saving T-mesh\n");
		return;
	}
	if(_mesh.meshToFile(filePath))
		printf("Saved [%s] successfully\n", filePath);
}