#ifndef T_MESH_H
#define T_MESH_H

#include "Rendering/Operator.h"
#include "Rendering/RenderingPrimitives.h"
#include "Rendering/ShadeAndShapes.h"

#include <mutex>

typedef pair<Sphere*,Operator*> PSO;

enum ValenceType {VALENCE_INVALID = -1};
enum ValenceBits
{
	VALENCE_BIT_UP = 1,
	VALENCE_BIT_DOWN = 2,
	VALENCE_BIT_LEFT = 4,
	VALENCE_BIT_RIGHT = 8,
	VALENCE_BITS_UPDOWN = 3,
	VALENCE_BITS_LEFTRIGHT = 12,
	VALENCE_BITS_ALL = 15
};
enum ExtensionBits
{
	EXTENSION_NEITHER = 0,
	EXTENSION_HORIZONTAL = 1,
	EXTENSION_VERTICAL = 2,
	// Denotes an intersection of V-H T-junction extensions
	EXTENSION_BOTH = 3
};
enum DirectionBits
{
	DIR_NEITHER = 0,
	DIR_ROW = 1,
	DIR_COLUMN = 2,
	// Denotes an intersection of V-H T-junction extensions
	DIR_BOTH = 3
};

struct VertexInfo
{
	// Explicit info (input)
	Pt3 position;
	// Implicit info (computed)
	int valenceBits;
	int valenceType; // -1: invalid, 0/2: unused, 3: T-junction, 4: "full" vertex
	int extendFlag; // whether part of H(0) or V(1) T-junction extensions
	int vId, hId; // vertical/horizontal indices of the 3rd of 5 elements in the
	              // corresponding index vector instead of two 5-element vectors
	VertexInfo() {}
	VertexInfo(Pt3 p, int vb, int t, int v, int h)
	{
		position = move(p);
		valenceBits = vb;
		valenceType = t;
		vId = v;
		hId = h;
	}
};

struct EdgeInfo
{
	// Explicit info (input)
	bool on;
	// Implicit info (computed)
	bool valid;
	bool extend; // whether part of H(0) or V(1) T-junction extensions
	EdgeInfo() {}
	EdgeInfo(bool o)
	{
		on = o;
		valid = true;
		extend = false;
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
	bool isAD; // admissible?
	bool isAS; // analysis-suitable?
	bool isDS; // de Boor-suitable?
	vector<VI> knotsCols, knotsRows; // indices, per column/row, discarding unused ones
	vector<VI> blendDir; // for each unit element whether it is allowed to blend
	                     // by row (0-bit) and/or column (1-bit) first
	vector<VI> sb2, sb4, bad;

	TMesh(int r, int c, int dv, int dh, bool autoFill = true);
	~TMesh();

	void assign(TMesh &tmesh);
	bool meshFromFile(const string &path);
	bool meshToFile(const string &path);
	bool useVertex(int r, int c) const;
	void cap(int& r, int& c) const;

	static bool validateDimensionsAndDegrees(int r, int c, int rd, int cd);
	static bool validateKnots(const vector<double> &knots, int n, int deg);
	static bool checkDuplicateAtKnotEnds(const vector<double> &knots, int n, int deg);

	void updateMeshInfo();
	void getTiledFloorRange(const int r, const int c, int& r_min, int& r_max, int& c_min, int& c_max) const;
	void get16Points(int ur, int uc, vector<pair<int,int>>& blendP, bool& row_n_4, bool& col_n_4) const;
	void get16PointsFast(int ur, int uc, vector<pair<int,int>>& blendP, bool& row_n_4, bool& col_n_4) const;
	void test1(int ur, int uc,
		vector<pair<int,int>>& blend1, vector<pair<int,int>>& blend2,
		vector<pair<int,int>>& missing, vector<pair<int,int>>& extra,
		bool& row_n_4, bool& col_n_4) const;

private:
	void markExtension(int r0, int c0, int dr, int dc, bool isVert, int& minRes, int& maxRes);
	bool isWithinGrid(int r, int c) const;
	bool isSkipped(int r, int c, bool isVert) const;
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
	void setMesh(const VVP3& S);
	void setMesh2(const vector<VVP3>& S);

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
	int getNumLights() { return SZ(_lights); }
	Light* getLight(int i) { return _lights[i]; }
};

#endif // T_MESH_H