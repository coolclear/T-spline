#ifndef T_MESH_H
#define T_MESH_H

#include "Rendering/RenderingPrimitives.h"
#include "Rendering/ShadeAndShapes.h"

#include <mutex>

class TMesh
{
private:
	static inline bool validateDimensionsAndDegrees(int r, int c, int rd, int cd);
	static bool validateKnots(const vector<double> &knots);
	void freeGridPoints();

public:
	const double radius = 0.05;
	mutex lock; // For allowing only one thread to access the mesh at a time
	int rows, cols;
	int rowDeg, colDeg;
	vector<double> knotsH, knotsV;
	vector<vector<bool>> gridH, gridV;
	vector<vector<Sphere*>> gridPoints;


	TMesh(int r, int c, int rd, int cd, bool autoFill = true);
	~TMesh();

	void assign(TMesh &tmesh);
	bool meshFromFile(const string &path);
	bool meshToFile(const string &path);
};

class TMeshScene : public SceneInfo
{
protected:
	vector<vector<Sphere*>> gridSpheres;

public:
	TMeshScene() {}

	void setup(TMesh *tmesh);
	const vector<vector<Sphere*>> &getSpheres() { return gridSpheres; }
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

	Pt3Array* getPoints() { return _pts; }
	TriIndArray* getInds() { return _tinds; }
	Vec3Array* getVNormals() { return _vnormals; }
	Vec3Array* getFNormals() { return _fnormals; }

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