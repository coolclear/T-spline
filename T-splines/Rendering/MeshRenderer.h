#ifndef MESH_RENDERER_H
#define MESH_RENDERER_H

#include "TMesh.h"

class MeshRenderer
{
protected:
	TriMeshScene* _scene;
	int _shadingModel;

public:
	bool drawWire;
	bool useNormal;

	MeshRenderer()
	{
		drawWire = true;
		useNormal = true;
		_scene = NULL;
		_shadingModel = SHADE_FLAT;
	}

	virtual void setTriMeshScene(TriMeshScene* TMeshScene) { _scene = TMeshScene; initLights(); }
	virtual void setShadingModel(int m) { _shadingModel = m; }
	virtual  int getShadingModel() { return _shadingModel; }

	virtual void draw();

protected:
	virtual void initLights();
};

#endif // MESH_RENDERER_H