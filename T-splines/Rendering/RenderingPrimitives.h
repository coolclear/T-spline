#ifndef RENDERING_PRIMITIVES_H
#define RENDERING_PRIMITIVES_H

#include "Common/Matrix.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

#include <algorithm>
#include <mutex>
#include <vector>
#include <string>

using namespace std;

// using a self-made dynamic array might make things a little faster
template<typename T>
class DynArray {
protected:
	T* _data;
	int _size, _cap;
public:
	DynArray() {
		_data = NULL;
		_size = 0;
		_cap = 0;
		recap(10);
	}

	~DynArray() {
		if(_data) delete [] _data;
	}

	void resize(int s) {
		if(s > _cap)
			recap(s);
		_size = s;
	}

	int size() { return _size; }

	void clear() {
		_size = 0;
		if(data) {
			delete [] _data;
			_data = NULL;
		}
		recap(10);
	}

	void add(const T& p) {
		if(_size >= _cap)
			recap(max<int>(_size + 1, _cap * 1.5));
		_data[_size++] = p;
	}

	T& get(int i) {
		assert(0 <= i && i < _size);
		return _data[i];
	}
	const T get(int i) const {
		assert(0 <= i && i < _size);
		return _data[i];
	}

	T* getData() { return _T; }

	void recap(int r) {
		_cap = r;
		T* ndata = new T[_cap];
		if(_data != NULL) {
			for(int j = 0; j < _size; j++)
				ndata[j] = move(_data[j]);
			delete [] _data;
		}
		_data = ndata;
	}
};

typedef DynArray<Pt3> Pt3Array;
typedef DynArray<Vec3> Vec3Array;
typedef DynArray<Color> ColorArray;

typedef Vector<int,3> TriInd;

typedef DynArray<int> IndArray;
typedef DynArray<TriInd> TriIndArray;

class Material {
protected:
	Color _ambient, _diffuse, _specular;
	float _specExp;
public:
	inline Material() {};

	inline const Color& getAmbient() const { return _ambient; }
	inline const Color& getDiffuse() const { return _diffuse; }
	inline const Color& getSpecular() const { return _specular; }
	inline float getSpecExponent() const { return _specExp; }

	inline void setAmbient(const Color& amb) { _ambient = amb; }
	inline void setDiffuse(const Color& diff) { _diffuse = diff; }
	inline void setSpecular(const Color& spec) { _specular = spec; }
	inline void setSpecExponent(float s) { _specExp = s; }
};

class Light {
protected:
	unsigned int _id;
	Pt3 _pos;
	Color _color;
	Color _ambient; // keep ambient is easier to code than using a global ambient
public:
	inline Light() {
		_color = Color(1,1,1);
	}

	inline Light(const Pt3& pos, const Color& color)
		: _pos(pos), _color(color) {}

	inline const Pt3& getPos() const { return _pos; }
	inline const Color& getColor() const { return _color; }
	inline const Color& getAmbient() const { return _ambient; }
	inline unsigned int getId() { return _id; }

	inline void setPos(const Pt3& p) { _pos = p; }
	inline void setColor(const Color& c) { _color = c; }
	inline void setAmbient(const Color& c) { _ambient = c; }
	inline void setId(unsigned int id) { _id = id; }
};


class View {
protected:
	Mat4 _modelview;
	Mat4 _proj;
	double _viewport[4];

public:
	View() {
		_viewport[0] = _viewport[1] = 0;
		_viewport[2] = _viewport[3] = 1;
	}

	Mat4* getModelView() { return &_modelview; }
	Mat4* getProjection() { return &_proj; }
	double* getViewport() { return _viewport; }
};

// For storing camera information and mutex locks for separate viewers
class SceneInfo
{
protected:
	Matrix<double, 4> _modelview;
	Matrix<double, 4> _translate;
	Matrix<double, 4> _rotate;

public:
	Matrix<double, 4>* getModelview() { return &_modelview; }
	Matrix<double, 4>* getTranslate() { return &_translate; }
	Matrix<double, 4>* getRotate() { return &_rotate; }

	inline void updateModelView() {_modelview = _translate * _rotate;}
};

class Sphere; // for reference

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
	vector<Pt3> curvePoints;
	bool useCurve;

public:
	TriMeshScene() {
		_mat = NULL;
		_mesh = NULL;
		useCurve = false;
	}

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

	void setCurve(vector<Pt3> points)
	{
		curvePoints = move(points);
		useCurve = true;
	}
	void setMesh(TriMesh* m)
	{
		_mesh = m;
		useCurve = false;
	}
	void setMaterial(Material* m) { _mat = m; }
	void addLight(Light* l) { _lights.push_back(l); }

	bool willDrawCurve() const { return useCurve; }
	vector<Pt3> &getCurve() { return curvePoints; }
	TriMesh* getMesh() { return _mesh; }
	Material* getMaterial() { return _mat; }
	int getNumLights() { return (int)_lights.size(); }
	Light* getLight(int i) { return _lights[i]; }
};




class RenderingUtils {
public:
	static Material* readMaterial(istream& mat);
	static Light* readLight(istream& str);

	static Vec3Array* perVertexNormals(Pt3Array* pts, TriIndArray* tris);
	static Vec3Array* perFaceNormals(Pt3Array* pts, TriIndArray* tris);
};

#define SHADE_FLAT 0
#define SHADE_GOURAUD 1
#define SHADE_NAIVE_PHONG 2
#define SHADE_SPHERICAL_PHONG 2


// Newell's method
inline Vec3 triFaceNormal(const Pt3 &a, const Pt3 &b, const Pt3 &c, bool doNorm = true) {
	Vec3 res = cross(b-a, c-a) + cross(c-b, a-b) + cross(a-c, b-c);
	if(doNorm) res.normalize();
	return res;
}

#endif