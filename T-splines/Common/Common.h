#ifndef COMMON_H
#define COMMON_H

#define _USE_MATH_DEFINES

#include <Fl/Fl.H>
#include <FL/gl.h>

#include <algorithm>
#include <map>
#include <vector>
#include <sstream>
#include <string>
#include <cmath>

#include "Matrix.h"
#include "Rendering/ArcBall.h"

#ifndef FINF32
#define FINF32 1e9f
#endif

#ifndef DINF
#define DINF 1e9
#endif

#define REFRESH_RATE .01

using namespace std;

const Fl_Color WIN_COLOR = fl_rgb_color(244, 247, 251);


namespace StringUtil {
	double parseDouble(const string& str);
	int parseInt(const string& str);
	bool isLetter(char c);
	bool isNumber(char c);
	vector<string> split(const string& s, char c);
}
using namespace StringUtil;


namespace MatrixUtil {
	void convertMat(const ArcBall::Matrix3f_t& mi, Mat4& mout);
	void mglLoadMatrix(const Mat4& mat);
	void mglReadMatrix(GLenum glmat, Mat4& mat);
	void mLoadMatrix(const Mat4& mat, GLdouble m[16]);
	void mReadMatrix(GLdouble m[16], Mat4& mat);
}
using namespace MatrixUtil;


namespace Util {
	Vec3 readVec3(istream &stream);
	Pt3 readPt3(istream &stream);
	double readDouble(istream &stream);
	int readInt(istream &stream);
	string readString(istream &stream);
	void writeVec3(ostream &stream, const Vec3& v);
	void writePt3(ostream &stream, const Pt3& v);
	void writeDouble(ostream &stream, double d);
	void writeInt(ostream &stream, int d);
	void writeString(ostream &stream, const string& s);

	double rand1();
};
using namespace Util;



// Short-cut macros
#define FOR(i,a,b) for(int _b=(b),i=(a);i<_b;++i)
#define _1 first
#define _2 second
#define SZ(x) int((x).size())

// Shortened type names for convenience
typedef vector<Pt3> VP3;
typedef vector<VP3> VVP3;

typedef pair<Pt3,Pt3> PT3;

typedef vector<int> VI;
typedef pair<VI,VI> PVI;

#endif // COMMON_H