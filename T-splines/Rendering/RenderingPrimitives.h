#ifndef RENDERING_PRIMITIVES_H
#define RENDERING_PRIMITIVES_H

#include "Common/Common.h"

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

	int size() const { return _size; }

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
			recap(max<int>(_size + 1, int(_cap * 1.5)));
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
	double _specExp;
public:
	inline Material() {};

	inline const Color& getAmbient() const { return _ambient; }
	inline const Color& getDiffuse() const { return _diffuse; }
	inline const Color& getSpecular() const { return _specular; }
	inline double getSpecExponent() const { return _specExp; }

	inline void setAmbient(const Color& amb) { _ambient = amb; }
	inline void setDiffuse(const Color& diff) { _diffuse = diff; }
	inline void setSpecular(const Color& spec) { _specular = spec; }
	inline void setSpecExponent(double s) { _specExp = s; }
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

// For storing shared camera information
class SceneInfo
{
// Shared singleton information
private:
	static Mat4 modelview0;
	static Mat4 translate0;
	static Mat4 rotate0;

public:
	Mat4 *getModelview() { return &modelview0; }
	Mat4 *getTranslate() { return &translate0; }
	Mat4 *getRotate() { return &rotate0; }

	static void initScene();
	static inline void updateModelView() { modelview0 = translate0 * rotate0; }
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

#endif