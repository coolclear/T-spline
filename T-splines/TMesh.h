#ifndef T_MESH_H
#define T_MESH_H

#include "Rendering/Operator.h"
#include "Rendering/RenderingPrimitives.h"
#include "Rendering/ShadeAndShapes.h"

#include <mutex>

typedef pair<Sphere*,Operator*> PSO;

struct VertexInfo
{
	// Explicit info (input)
	Pt3 position;
	// Implicit info (computed)
	int valenceBits;
	int valenceType; // -1: invalid, 0/2: unused, 3: T-junction, 4: "full" vertex
	int extendFlag; // whether part of H(0) or V(1) T-junction extensions
	VertexInfo() {}
	VertexInfo(Pt3 p, int vb, int t)
	{
		position = move(p);
		valenceBits = vb;
		valenceType = t;
	}
};

struct EdgeInfo
{
	// Explicit info (input)
	bool on;
	// Implicit info (computed)
	bool valid;
	EdgeInfo() {}
	EdgeInfo(bool o, bool v)
	{
		on = o;
		valid = v;
	}
};

class TMesh
{
public:
	mutex lock; // For allowing only one thread to access the mesh at a time
	int rows, cols;
	int degH, degV; // Horizontal: for each row, Vertical: for each column
	vector<double> knotsH, knotsV;
	vector<vector<EdgeInfo>> gridH, gridV;
	vector<vector<VertexInfo>> gridPoints; // explicit and implicit vertex info

	// Implicit (computed) information
	bool validVertices; // true if all active vertices have valences 3-4 or 2 (straight)
	bool isAD;
	bool isAS;

	TMesh(int r, int c, int dv, int dh, bool autoFill = true);
	~TMesh();

	void assign(TMesh &tmesh);
	bool meshFromFile(const string &path);
	bool meshToFile(const string &path);
	inline bool useVertex(int r, int c) const;

	static bool validateDimensionsAndDegrees(int r, int c, int rd, int cd);
	static bool validateKnots(const vector<double> &knots, int n, int deg);
	static bool checkDuplicateAtKnotEnds(const vector<double> &knots, int n, int deg);

	void updateMeshInfo();
};

class TMeshScene : public SceneInfo
{
protected:
	TMesh *mesh;
	map<Sphere*, pair<int, int>> sphereIndices;

public:
	const double radius = 0.05;
	int rows, cols; // internal dimensions for updating 'gridSpheres'
	vector<vector<PSO>> gridSpheres;

	TMeshScene() : mesh(NULL), rows(0), cols(0) {}
	~TMeshScene() { freeGridSpheres(); }

	void setup(TMesh *tmesh);
	void updateScene();
	void updateSphere(Sphere *sphere);
	void freeGridSpheres();

	bool useSphere(int r, int c) const;
	const vector<vector<EdgeInfo>> &getGridH() const { return mesh->gridH; }
	const vector<vector<EdgeInfo>> &getGridV() const { return mesh->gridV; }
};

class TriMesh {
protected:
	Pt3Array* _pts;
	Vec3Array* _vnormals;
	Vec3Array* _fnormals;
	TriIndArray* _tinds; // mesh is made of triangles
public:

	TriMesh() {
		_pts = NULL;
		_vnormals = NULL;
		_fnormals = NULL;
		_tinds = NULL;
	}
	TriMesh(Pt3Array* pts, TriIndArray* inds) {
		_pts = pts;
		_tinds = inds;
	}
	~TriMesh() { del(); }

	void setPoints(Pt3Array* p) {
		_pts = p;
	}

	void setInds(TriIndArray* ti) {
		_tinds = ti;
	}

	void setVNormals(Vec3Array* n) {
		_vnormals = n;
	}

	void setFNormals(Vec3Array* n) {
		_fnormals = n;
	}

	Pt3Array* getPoints() const { return _pts; }
	TriIndArray* getInds() const { return _tinds; }
	Vec3Array* getVNormals() const { return _vnormals; }
	Vec3Array* getFNormals() const { return _fnormals; }

	// use these delete functions carefully
	void del() {
		delPts();
		delInds();
		delVNormals();
		delFNormals();
	}

protected:
	void delInds() {
		if(_tinds)
			delete _tinds;
		_tinds = NULL;
	}

	void delPts() {
		if(_pts)
			delete _pts;
		_pts = NULL;
	}

	void delVNormals() {
		if(_vnormals)
			delete _vnormals;
		_vnormals = NULL;
	}

	void delFNormals() {
		if(_fnormals)
			delete _fnormals;
		_fnormals = NULL;
	}
};

class TriMeshScene : public SceneInfo {
protected:
	Material* _mat;
	vector<Light*> _lights;
	TriMesh* _mesh;
	vector<pair<Pt3, int>> curvePoints;
	bool useCurve;

	void setCurve(vector<pair<Pt3, int>> points);
	void setMesh(const VVP3 &S);

public:
	TriMeshScene();

	~TriMeshScene() {
		for(Light *l: _lights) delete l;
		_lights.clear();
		if(_mesh)
		{
			_mesh->del(); // have to be sure that no one shares this data
			delete _mesh;
		}
		if(_mat) delete _mat;
	}

	// Set data (curve/surface) for drawing
	void setScene(const TMesh *T);

	void setMaterial(Material* m) { _mat = m; }
	void addLight(Light* l) { _lights.push_back(l); }

	bool willDrawCurve() const { return useCurve; }
	vector<pair<Pt3, int>> &getCurve() { return curvePoints; }
	TriMesh* getMesh() { return _mesh; }
	Material* getMaterial() { return _mat; }
	int getNumLights() { return (int)_lights.size(); }
	Light* getLight(int i) { return _lights[i]; }
};

#endif // T_MESH_H