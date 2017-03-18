#ifndef COMMON_H
#define COMMON_H

#include <Fl/Fl.H>
#include <FL/gl.h>
#include <string>
#include <vector>
#include <sstream>

#include "Matrix.h"
#include "Rendering/ArcBall.h"
#include "Rendering/RenderingPrimitives.h"

#ifndef M_PI
#define M_PI 3.14159265358979
#endif

#ifndef FINF32
#define FINF32 1e9f
#endif

#ifndef DINF
#define DINF 1e9
#endif

#define REFRESH_RATE .01

using namespace std;

const Fl_Color WIN_COLOR = fl_rgb_color(244, 247, 251);

namespace Str {
	double parseDouble(const string& str);
	int parseInt(const string& str);
	bool isLetter(char c);
	bool isNumber(char c);
	vector<string> split(const string& s, char c);
}

namespace Util {
	void convertMat(const ArcBall::Matrix3f_t& mi, Mat4& mout);
	void mglLoadMatrix(const Mat4& mat);
	void mglReadMatrix(GLenum glmat, Mat4& mat);
	void mLoadMatrix(const Mat4& mat, GLdouble m[16]);
	void mReadMatrix(GLdouble m[16], Mat4& mat);
}
using namespace Util;

#define FOR(i,a,b) for(int _b=(b),i=(a);i<_b;++i)
#define ROF(i,b,a) for(int _a=(a),i=(b);i>_a;--i)
#define REP(n) for(int _n=(n);_n--;)
#define _1 first
#define _2 second
#define PB(x) push_back(x)
#define SZ(x) int((x).size())
#define ALL(x) (x).begin(),(x).end()

inline double rand1()
{
	return (rand()%32000 * 32000 + rand()%32000) / 32000.0 / 32000.0;
}

typedef vector<Pt3> VP3;
typedef vector<VP3> VVP3;

typedef pair<Pt3,Pt3> PT3;

typedef vector<int> VI;
typedef pair<VI,VI> PV;


#endif