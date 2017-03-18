#include <FL/Fl.H>
#include <iostream>

#include "GUI/TopologyWindow.h"
#include "GUI/GeometryWindow.h"
#include "GUI/RenderingWindow.h"
#include "Rendering/RenderingPrimitives.h"

using namespace std;

int main(int argc, char** argv)
{
	TopologyWindow topology(20, 600, 650, 310, "T-Mesh Topology");
	GeometryWindow geometry(680, 40, 500, 500, "T-Mesh Geometry");
	RenderingWindow renderer(1190, 40, 500, 500, "T-Mesh Renderer");

	topology.setup(&geometry, &renderer);

	return Fl::run();
}