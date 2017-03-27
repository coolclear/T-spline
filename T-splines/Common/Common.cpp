#include <string>
#include <sstream>
#include <iostream>
#include "Common/Common.h"

using namespace std;

double StringUtil::parseDouble(const string& str) {
	double ret;
	stringstream sst(str);
	sst>>ret;
	return ret;
}

// Parse doubles into vector<double>. Return true if success, false otherwise
bool StringUtil::parseDoubles(const string &str, vector<double> &res)
{
	res.clear();
	stringstream ss(str);
	while(true)
	{
		double val;
		if(!(ss >> val)) break;
		res.push_back(val);
	}
	if(!ss.eof()) // parsing failed: some character was unexpected
	{
		res.clear();
		return false;
	}
	return true;
}

int StringUtil::parseInt(const string& str) {
	int ret;
	stringstream sst(str);
	sst>>ret;
	return ret;
}

bool StringUtil::isLetter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool StringUtil::isNumber(char c) {
	return (c >= '0' && c <= '9');
}

vector<string> StringUtil::split(const string& s, char c) {
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

void MatrixUtil::convertMat(const ArcBall::Matrix3f_t& mi, Mat4& mout) {
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

void MatrixUtil::mglLoadMatrix(const Mat4& mat) {
	GLfloat m[16];
	for(int i = 0; i < 16; i++)
		m[i] = mat[i>>2][i&3];
	glLoadMatrixf(m);
}

void MatrixUtil::mglReadMatrix(GLenum glmat, Mat4& mat) {
	GLfloat m[16];
	glGetFloatv(glmat, m);
	for(int i = 0; i < 16; i++)
		mat[i>>2][i&3] = m[i];
}

void MatrixUtil::mLoadMatrix(const Mat4& mat, GLdouble m[16]) {
	for(int i = 0; i < 16; i++)
		m[i] = mat[i>>2][i&3];
}

void MatrixUtil::mReadMatrix(GLdouble m[16], Mat4& mat) {
	for(int i = 0; i < 16; i++)
		mat[i>>2][i&3] = m[i];
}




Vec3 Util::readVec3(istream &stream) {
	Vec3 ret(0, 0, 0, 0);
	stream>>ret[0]>>ret[1]>>ret[2];
	return ret;
}

Pt3 Util::readPt3(istream &stream) {
	Pt3 ret(0, 0, 0);
	stream>>ret[0]>>ret[1]>>ret[2];
	return ret;
}

double Util::readDouble(istream &stream) {
	double ret;
	stream>>ret;
	return ret;
}

int Util::readInt(istream &stream) {
	int ret;
	stream>>ret;
	return ret;
}

string Util::readString(istream &stream) {
	string ret;
	stream>>ret;
	return ret;
}

void Util::writeVec3(ostream &stream, const Vec3& v) {
	stream << v[0] << '\t' << v[1] << '\t' << v[2];
}

void Util::writePt3(ostream &stream, const Pt3& v) {
	stream << v[0] << '\t' << v[1] << '\t' << v[2];
}

void Util::writeDouble(ostream &stream, double d) {
	stream << d;
}

void Util::writeInt(ostream &stream, int d) {
	stream << d;
}

void Util::writeString(ostream &stream, const string& s) {
	stream << s;
}


double Util::rand1()
{
	const int x = 32700;
	return ((rand() % x) * x + rand() % x) * 1.0 / (x*x);
}