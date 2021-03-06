#undef NDEBUG
#include "GUI/TopologyWindow.h"
#include "Common/Common.h"

#include <iostream>
#include <iomanip>

using namespace std;

const int WIN_LOWER_SPACE = 30;

TMesh TopologyWindow::_mesh(7, 7, 3, 3);

TopologyWindow::TopologyWindow(int x, int y, int w, int h, const char* l)
	: Fl_Window(x,y,w,h+WIN_LOWER_SPACE,l)
{
	// User interfaces
	begin();
	{
		_viewer = new TopologyViewer(5,5,300,300,"_viewer");
		_viewer->box(FL_FLAT_BOX);
		_viewer->color(FL_BLACK);
		_viewer->end();
		_viewer->setMesh(&_mesh);
		_viewer->setParent(this);

		fileGroup = new Fl_Group(310, 20, 150, 40, "T-Mesh File");
		fileGroup->color(WIN_COLOR);
		fileGroup->box(FL_BORDER_BOX);
		fileGroup->begin();
		{
			loadButton = new Button(320, 30, 60, 20, "Load");
			loadButton->callback(loadButtonCallback, this);
			saveButton = new Button(390, 30, 60, 20, "Save");
			saveButton->callback(saveButtonCallback, this);
		}
		fileGroup->end();

		knotsHInput = new Fl_Input(370, 70, 200, 20, "H knots: ");
		knotsHButton = new Button(580, 70, 60, 20, "Update");
		knotsHButton->callback(knotsHButtonCallback, this);
		knotsVInput = new Fl_Input(370, 95, 200, 20, "V knots: ");
		knotsVButton = new Button(580, 95, 60, 20, "Update");
		knotsVButton->callback(knotsVButtonCallback, this);

		topStatLabel = new Fl_Box(310, 125, 330, 22);
		topStatLabel->box(FL_FRAME_BOX);
		//topStatLabel->
	}
	end();

	show();
	resize(x, y, w, h);

	this->callback(escapeButtonCb, this);
	this->color(WIN_COLOR);

	//loadMesh("files/default.txt");
	updatePanel();

	Fl::repeat_timeout(REFRESH_RATE, TopologyWindow::updateTopologyStatus, this);
}

TopologyWindow::~TopologyWindow()
{
	delete _viewer;

	delete loadButton;
	delete saveButton;
	delete fileGroup;

	delete topStatLabel;
	delete knotsHInput;
	delete knotsVInput;
	delete knotsHButton;
	delete knotsVButton;
}

void TopologyWindow::updatePanel()
{
	// Load knots from the mesh to text boxes
	stringstream ss;
	ss << setprecision(6);
	// Horizontal knots
	for(int i = 0; i < SZ(_mesh.knotsH); ++i)
	{
		if(i > 0) ss << ' ';
		ss << _mesh.knotsH[i];
	}
	knotsHInput->value(ss.str().c_str());

	ss = stringstream();
	// Vertical knots
	for(int i = 0; i < SZ(_mesh.knotsV); ++i)
	{
		if(i > 0) ss << ' ';
		ss << _mesh.knotsV[i];
	}
	knotsVInput->value(ss.str().c_str());
}

void TopologyWindow::updateControlPoints()
{
	if(_geometry)
		_geometry->setupControlPoints(&_mesh);
}

void TopologyWindow::updateSurface()
{
	if(_geometry)
		_geometry->setupSurface(&_mesh);
}

void TopologyWindow::loadMesh(char *filePath)
{
	// If filePath is not supplied, then we'll use the file chooser
	if(not filePath)
		filePath = fl_file_chooser("Open T-Mesh", ".txt (*.txt)", "./files/", 0);

	if(not filePath)
	{
		fprintf(stderr, "Canceled loading T-mesh\n");
		return;
	}

	if(_mesh.meshFromFile(filePath))
	{
		printf("---------------------------------------------------------\n");
		printf("Loaded [%s] successfully\n", filePath);
		printf("Dimensions: %d x %d\n", _mesh.rows, _mesh.cols);
		printf("Degree: V %d x H %d\n", _mesh.degV, _mesh.degH);
		printf("\n");

		if(not TMesh::checkDuplicateAtKnotEnds(_mesh.knotsH, _mesh.cols, _mesh.degH))
			printf("* Warning: Horizontal knot values are not repeated at end points\n");
		if(not TMesh::checkDuplicateAtKnotEnds(_mesh.knotsV, _mesh.rows, _mesh.degV))
			printf("* Warning: Vertical knot values are not repeated at end points\n");

		updatePanel();
		updateControlPoints();
		updateSurface();
	}
	else
		printf("Failed to load T-mesh [%s]\n", filePath);
}

void TopologyWindow::saveMesh()
{
	char* filePath = fl_file_chooser("Save T-Mesh", ".txt (*.txt)", "./files/", 0);
	if(not filePath)
	{
		fprintf(stderr, "Canceled saving T-mesh\n");
		return;
	}
	if(_mesh.meshToFile(filePath))
		printf("Saved [%s] successfully\n", filePath);
}

void TopologyWindow::loadButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if(topology)
		topology->loadMesh();
}

void TopologyWindow::saveButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if(topology)
		topology->saveMesh();
}

void TopologyWindow::knotsHButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if(topology and topology->knotsHInput)
	{
		vector<double> knotsH;
		if(not parseDoubles(topology->knotsHInput->value(), knotsH))
		{
			printf("Malformed input - horizontal knots\n");
			return;
		}

		int cols = _mesh.cols;
		int degH = _mesh.degH;

		if(not TMesh::validateKnots(knotsH, cols, degH))
		{
			fprintf(stderr, "Non-decreasing horizontal knot values or incorrect counts\n");
			return;
		}
		if(not TMesh::checkDuplicateAtKnotEnds(knotsH, cols, degH))
			printf("\n* Warning: Horizontal knot values are not repeated at end points\n");

		_mesh.lock.lock();
		assert(SZ(_mesh.knotsH) == SZ(knotsH));
		_mesh.knotsH = move(knotsH);
		_mesh.lock.unlock();

		topology->updateControlPoints();
		topology->updateSurface();

		printf("\nUpdated horizontal knots\n");
	}
}

void TopologyWindow::knotsVButtonCallback(Fl_Widget* widget, void* userdata)
{
	TopologyWindow *topology = (TopologyWindow*)userdata;
	if(topology and topology->knotsVInput)
	{
		vector<double> knotsV;
		if(not parseDoubles(topology->knotsVInput->value(), knotsV))
		{
			printf("Malformed input - vertical knots\n");
			return;
		}

		int rows = _mesh.rows;
		int degV = _mesh.degV;

		if(not TMesh::validateKnots(knotsV, rows, degV))
		{
			fprintf(stderr, "Non-decreasing vertical knot values or incorrect counts\n");
			return;
		}
		if(not TMesh::checkDuplicateAtKnotEnds(knotsV, rows, degV))
			printf("\n* Warning: Vertical knot values are not repeated at end points\n");

		_mesh.lock.lock();
		assert(SZ(_mesh.knotsV) == SZ(knotsV));
		_mesh.knotsV = move(knotsV);
		_mesh.lock.unlock();

		topology->updateControlPoints();
		topology->updateSurface();

		printf("\nUpdated vertical knots\n");
	}
}

void TopologyWindow::updateTopologyStatus(void* userdata)
{
	TopologyWindow *tw = (TopologyWindow *)userdata;
	if(not tw or not tw->topStatLabel) return;

	if(not tw->_mesh.validVertices)
		tw->topStatLabel->label("Invalid Vertices");
	else if(not tw->_mesh.isAD)
		tw->topStatLabel->label("T-Mesh is not Admissible (AD)");
	else if(not tw->_mesh.isAS)
		tw->topStatLabel->label("T-Mesh is not Analysis-Suitable (AS)");
	else if(not tw->_mesh.isDS)
		tw->topStatLabel->label("T-Mesh is not de Boor-Suitable (DS)");
	else
		tw->topStatLabel->label("T-Mesh OK");

	Fl::repeat_timeout(REFRESH_RATE, TopologyWindow::updateTopologyStatus, userdata);
}