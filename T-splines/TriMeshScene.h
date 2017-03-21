#ifndef TRI_MESH_SCENE_H
#define TRI_MESH_SCENE_H

#include "Rendering/RenderingPrimitives.h"
#include "Common/Common.h"

TriMeshScene* createTriMeshScene(const VVP3 &S);
TriMeshScene* createCurveScene(const vector<pair<Pt3, int>> &P);

#endif //__SUB_H