#include <string>
#include <sstream>
#include <iostream>
#include "Common/Common.h"

using namespace std;

double Str::parseDouble(const string& str) {
	double ret;
	stringstream sst(str);
	sst>>ret;
	return ret;
}

int Str::parseInt(const string& str) {
	int ret;
	stringstream sst(str);
	sst>>ret;
	return ret;
}

bool Str::isLetter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Str::isNumber(char c) {
	return (c >= '0' && c <= '9');
}

vector<string> Str::split(const string& s, char c) {
	vector<string> ret;
	string cur = "";
	for(int j = 0;j<s.length();j++) {
		if(s[j] == c) {
			ret.push_back(cur);
			cur = "";
		}
		else {
			cur += s[j];
		}
	}
	if(cur.length() > 0)
		ret.push_back(cur);

	vector<string> ret2;
	for(int j = 0; j < ret.size(); j++)
		if(ret[j].length() > 0)
			ret2.push_back(ret[j]);

	return move(ret2);
}

void Util::convertMat(const ArcBall::Matrix3f_t& mi, Mat4& mout) {
	mout[0][0] = mi.s.M00;
	mout[1][0] = mi.s.M10;
	mout[2][0] = mi.s.M20;
	mout[3][0] = 0;
	mout[0][1] = mi.s.M01;
	mout[1][1] = mi.s.M11;
	mout[2][1] = mi.s.M21;
	mout[3][1] = 0;
	mout[0][2] = mi.s.M02;
	mout[1][2] = mi.s.M12;
	mout[2][2] = mi.s.M22;
	mout[3][2] = 0;
	mout[3][0] = 0;
	mout[3][1] = 0;
	mout[3][2] = 0;
	mout[3][3] = 1;
}

void Util::mglLoadMatrix(const Mat4& mat) {
	GLfloat m[16];
	for(int i = 0; i < 16; i++)
		m[i] = mat[i>>2][i&3];
	glLoadMatrixf(m);
}

void Util::mglReadMatrix(GLenum glmat, Mat4& mat) {
	GLfloat m[16];
	glGetFloatv(glmat, m);
	for(int i = 0; i < 16; i++)
		mat[i>>2][i&3] = m[i];
}

void Util::mLoadMatrix(const Mat4& mat, GLdouble m[16]) {
	for(int i = 0; i < 16; i++)
		m[i] = mat[i>>2][i&3];
}

void Util::mReadMatrix(GLdouble m[16], Mat4& mat) {
	for(int i = 0; i < 16; i++)
		mat[i>>2][i&3] = m[i];
}