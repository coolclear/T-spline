#undef NDEBUG
#include <cassert>

#include "Rendering/RenderingPrimitives.h"
#include "ShadeAndShapes.h"

#include <fstream>
#include <iostream>



using namespace std;

Material* RenderingUtils::readMaterial(std::istream& str) {
	Color amb,diff,spec;
	double spece;
	amb.read(str,3);
	diff.read(str,3);
	spec.read(str,3);
	str >> spece;

	Material* ret = new Material();
	ret->setAmbient(amb);
	ret->setDiffuse(diff);
	ret->setSpecular(spec);
	ret->setSpecExponent(spece);
	return ret;
}

Light* RenderingUtils::readLight(std::istream& str) {
	Pt3 pos;
	Color c;
	pos.read(str,3);
	c.read(str,3);
	Light* ret = new Light();
	ret->setColor(c);
	ret->setPos(pos);
	return ret;
}

Vec3Array* RenderingUtils::perVertexNormals(Pt3Array* pts, TriIndArray* tris) {
	int ntris = tris->size();
	int nverts = pts->size();

	Vec3Array* norms = new Vec3Array();
	norms->resize(nverts);
	for(int i = 0; i < nverts; i++)
		norms->get(i).zero();

	for(int i = 0; i < ntris; i++) {
		TriInd& tri = tris->get(i);
		Pt3& a = pts->get(tri[0]);
		Pt3& b = pts->get(tri[1]);
		Pt3& c = pts->get(tri[2]);
		Vec3 n = triFaceNormal(a,b,c);

		for(int j = 0; j < 3; j++)
			norms->get(tri[j]) += n;
	}

	for(int i = 0; i < nverts; i++) {
		norms->get(i)[3] = 0;
		norms->get(i).normalize();
	}

	return norms;
}

Vec3Array* RenderingUtils::perFaceNormals(Pt3Array* pts, TriIndArray* tris) {
	Vec3Array* norms = new Vec3Array();
	int ntris = tris->size();

	for(int j = 0; j < ntris; j++) {
		TriInd& tri = tris->get(j);
		Pt3& a = pts->get(tri[0]);
		Pt3& b = pts->get(tri[1]);
		Pt3& c = pts->get(tri[2]);
		norms->add(triFaceNormal(a,b,c));
	}

	return norms;
}


Vec3 readVec3(istream* stream) {
	Vec3 ret(0, 0, 0, 0);
	(*stream)>>ret[0]>>ret[1]>>ret[2];
	return ret;
}

Pt3 readPt3(istream* stream) {
	Pt3 ret(0, 0, 0);
	(*stream)>>ret[0]>>ret[1]>>ret[2];
	return ret;
}

double readDouble(istream* stream) {
	double ret;
	(*stream)>>ret;
	return ret;
}

int readInt(istream* stream) {
	int ret;
	(*stream)>>ret;
	return ret;
}

string readString(istream* stream) {
	string ret;
	(*stream)>>ret;
	return ret;
}

void writeVec3(ostream* stream, const Vec3& v) {
	(*stream) << v[0] << " " << v[1] << " " << v[2];
}

void writePt3(ostream* stream, const Pt3& v) {
	(*stream) << v[0] << " " << v[1] << " " << v[2];
}

void writeDouble(ostream* stream, double d) {
	(*stream) << d;
}

void writeInt(ostream* stream, int d) {
	(*stream) << d;
}

void writeString(ostream* stream, const string& s) {
	(*stream) << s;
}


bool TMeshUtils::writeScene(const std::string& fname, TMeshScene* TMeshScene) {
	std::ofstream fout(fname.c_str());

	if(!fout.good())
	{
		fprintf(stderr, "Cannot write the TMeshScene to a file\n");
		return false;
	}

	fout << setprecision(12); // up to 12 decimal places

	if(TMeshScene) {
		/*
		WriteSceneObjectVisitor writer;
		writer.setStream(&fout);

		writeInt(&fout, TMeshScene->getNumObjects());
		fout << endl << endl;
		for(int j = 0; j < TMeshScene->getNumObjects(); j++) {
		Geometry* geom = TMeshScene->getObject(j);
		geom->accept(&writer, NULL);
		fout << endl;
		}*/
	}
	fout << endl;

	Mat4* rot = TMeshScene->getRotate();
	Mat4* trans = TMeshScene->getTranslate();

	for(int j = 0; j < 4; j++)
		for(int k = 0; k < 4; k++)
			fout << (*rot)[j][k] << " ";

	for(int i = 0; i < 3; i++) fout << (*trans)[3][i] << ' ';
	fout << endl;

	return true;
}

// no error handling at all.  pretty unsafe.  please make sure your file is well-formed.
TMeshScene* TMeshUtils::readScene(const std::string& fname) {
	TMeshScene* ret = NULL;

	std::fstream fin(fname.c_str());
	std::stringstream ss;
	std::string line;

	if(fin.good()) {
		ret = new TMeshScene();

		while(!fin.eof()) {
			getline(fin, line);
			size_t pos = line.find("//");
			if(pos == string::npos)
				ss << line;
			else
				ss << line.substr(0, pos);
			ss << endl;
		}

		/*
		ReadSceneObjectVisitor reader;
		reader.setStream(&ss);

		int numObjs = readInt(&ss);
		for(int j = 0; j < numObjs; j++) {
		string type = readString(&ss);
		Geometry* geom = new Sphere();
		geom->accept(&reader, NULL);
		ret->addObject(geom);
		}
		*/

		Mat4* rot = ret->getRotate();
		Mat4* trans = ret->getTranslate();

		for(int j = 0; j < 4; j++)
			for(int k = 0; k < 4; k++)
				(*rot)[j][k] = readDouble(&ss);

		for(int j = 0; j < 3; j++)
			(*trans)[3][j] = readDouble(&ss);
	}

	return ret;
}



inline bool TMesh::validateDimensionsAndDegrees(int r, int c, int rd, int cd)
{
	const int rcLimit = 1e4; // Limit up to 10^4 cells
	if(!(r >= 0 && c >= 0 && r + c >= 1 && r * c <= rcLimit))
		return false;
	if((r > 0 && (rd < 1 || rd > r)) || (r == 0 && rd != 0))
		return false;
	if((c > 0 && (cd < 1 || cd > c)) || (c == 0 && cd != 0))
		return false;
	return true;
}

// Knot values must be non-decreasing (sorted)
bool TMesh::validateKnots(const vector<double> &knots)
{
	if(knots.size() <= 1)
		return true;

	for(int i = 1; i < (int) knots.size(); ++i)
		if(knots[i-1] > knots[i]) // decreasing -> invalid
			return false;
	return true;
}

void TMesh::freeGridPoints()
{
//	printf("%p freeGridPoints\n", this);
	for(auto &spheres: gridPoints)
	{
		for(auto &s: spheres)
		{
			delete s;
			s = NULL;
		}
	}
}


TMesh::TMesh(int r, int c, int rd, int cd, bool autoFill)
{
	assert(validateDimensionsAndDegrees(r, c, rd, cd));
	rows = r;
	cols = c;
	rowDeg = rd;
	colDeg = cd;

	// Assign some uniform knot values
	if(cols > 0)
	{
		knotsH.resize(cols + colDeg);
		if(autoFill)
		{
			for(int i = 0; i < cols + colDeg; ++i)
				knotsH[i] = i;
			assert(validateKnots(knotsH));
		}
	}
	if(rows > 0)
	{
		knotsV.resize(rows + rowDeg);
		if(autoFill)
		{
			for(int i = 0; i < rows + rowDeg; ++i)
				knotsV[i] = i;
			assert(validateKnots(knotsV));
		}
	}

	// Make the full grid initially
	gridH.assign(rows + 1, vector<bool>(cols, true));
	gridV.assign(cols, vector<bool>(cols + 1, true));

	// Assign some uniform coordinates initially
	gridPoints.resize(rows + 1, vector<Sphere*>(cols + 1, NULL));

	if(autoFill)
	{
		for(int i = 0; i <= rows; ++i)
			for(int j = 0; j <= cols; ++j)
				gridPoints[i][j] = new Sphere(Pt3((j+1)*0.5, (i+1)*0.5, 0.5), radius);
	}
}

TMesh::~TMesh()
{
	lock.lock();
	freeGridPoints();
	lock.unlock();
}

void TMesh::assign(TMesh &T)
{
	this->lock.lock();

	this->rows = T.rows;
	this->cols = T.cols;
	this->rowDeg = T.rowDeg;
	this->colDeg = T.colDeg;
	this->knotsH = move(T.knotsH);
	this->knotsV = move(T.knotsV);
	this->gridH = move(T.gridH);
	this->gridV = move(T.gridV);

	this->freeGridPoints();
	this->gridPoints = move(T.gridPoints);
	T.gridPoints.clear();

	this->lock.unlock();
}

bool TMesh::meshFromFile(const string &path)
{
	// Try to open the file an
	ifstream fs(path);
	if(!fs.is_open()) // File is not found or cannot be opened
	{
		fprintf(stderr, "Failed to open a T-mesh file\n");
		return false;
	}

	// Read in dimensions and degrees, and validate them
	int rows1 = -1, cols1 = -1;
	int rowDeg1 = -1, colDeg1 = -1;
	{
		fs >> rows1 >> cols1;
		if(!fs.good())
		{
			fprintf(stderr, "Failed to read T-mesh dimensions (R x C)\n");
			return false;
		}
		fs >> rowDeg1 >> colDeg1;
		if(!fs.good())
		{
			fprintf(stderr, "Failed to read degrees\n");
			return false;
		}
		if(!validateDimensionsAndDegrees(rows1, cols1, rowDeg1, colDeg1))
		{
			fprintf(stderr, "Invalid T-mesh dimensions (%d x %d) or degrees %d & %d\n",
				rows1, cols1, rowDeg1, colDeg1);
			return false;
		}
	}

	TMesh T(rows1, cols1, rowDeg1, colDeg1, false);

	// Read grid informati1on
	{
		// - Horizontal: (R+1) x C bools
		for(int r = 0; r <= rows1; ++r)
		{
			for(int c = 0; c < cols1; ++c)
			{
				int bit = -1;
				fs >> bit;
				if(!fs.good() || bit < 0 || bit > 1)
				{
					fprintf(stderr, "Failed to read horizontal grid info\n");
					return false;
				}
				T.gridH[r][c] = bit;
			}
		}
		// - Vertical: R x (C+1) bools
		for(int r = 0; r < rows1; ++r)
		{
			for(int c = 0; c <= cols1; ++c)
			{
				int bit = -1;
				fs >> bit;
				if(!fs.good() || bit < 0 || bit > 1)
				{
					fprintf(stderr, "Failed to read vertical grid info\n");
					return false;
				}
				T.gridV[r][c] = bit;
			}
		}
	}

	// Read knot values (monotonically increasing)
	{
		// - Horizontal: C + C_deg doubles
		for(int i = 0; i < cols1 + colDeg1; ++i)
		{
			if(!(fs >> T.knotsH[i]))
			{
				fprintf(stderr, "Failed to read horizontal knot values\n");
				return false;
			}
		}
		if(!validateKnots(knotsH))
		{
			fprintf(stderr, "Horizontal knot values are not non-decreasing\n");
			return false;
		}
		// - Vertical: R + R_deg doubles
		for(int i = 0; i < rows1 + rowDeg1; ++i)
		{
			if(!(fs >> T.knotsV[i]))
			{
				fprintf(stderr, "Failed to read vertical knot values\n");
				return false;
			}
		}
		if(!validateKnots(knotsV))
		{
			fprintf(stderr, "Vertical knot values are not non-decreasing\n");
			return false;
		}
	}

	// Read control point coordinates: (R+1) x (C+1) x 3 doubles
	for(int r = 0; r <= rows1; ++r)
	{
		for(int c = 0; c <= cols1; ++c)
		{
			Pt3 p;
			for(int i = 0; i < 3; ++i)
			{
				if(!(fs >> p[i]))
				{
					fprintf(stderr, "Failed to open a T-mesh file\n");
					return false;
				}
			}
			p[3] = 1;
			T.gridPoints[r][c] = new Sphere(p, radius);
		}
	}

	// Check if the file ends with "END"
	string end;
	fs >> end;
	if(end != "END")
	{
		fprintf(stderr, "Bad ending format\n");
		return false;
	}

	// The file is read successfully, so we can replace the mesh now
	assign(T);
	return true;
}