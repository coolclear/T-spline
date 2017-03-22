#include <FL/Fl.H>
#include <iostream>

#include "GUI/TopologyWindow.h"
#include "GUI/GeometryWindow.h"
#include "Rendering/RenderingPrimitives.h"

using namespace std;

int main(int argc, char** argv)
{
	SceneInfo::initScene();

	TopologyWindow topology(20, 565, 650, 310, "T-Mesh Topology");
	GeometryWindow geometry(680, 40, 700, 700, "T-Mesh Geometry");

	topology.setup(&geometry);

	return Fl::run();
}