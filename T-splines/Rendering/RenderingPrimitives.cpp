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


inline Vec3 readVec3(istream* stream) {
	Vec3 ret(0, 0, 0, 0);
	(*stream)>>ret[0]>>ret[1]>>ret[2];
	return ret;
}

inline Pt3 readPt3(istream* stream) {
	Pt3 ret(0, 0, 0);
	(*stream)>>ret[0]>>ret[1]>>ret[2];
	return ret;
}

inline double readDouble(istream* stream) {
	double ret;
	(*stream)>>ret;
	return ret;
}

inline int readInt(istream* stream) {
	int ret;
	(*stream)>>ret;
	return ret;
}

inline string readString(istream* stream) {
	string ret;
	(*stream)>>ret;
	return ret;
}

inline void writeVec3(ostream* stream, const Vec3& v) {
	(*stream) << v[0] << '\t' << v[1] << '\t' << v[2];
}

inline void writePt3(ostream* stream, const Pt3& v) {
	(*stream) << v[0] << '\t' << v[1] << '\t' << v[2];
}

inline void writeDouble(ostream* stream, double d) {
	(*stream) << d;
}

inline void writeInt(ostream* stream, int d) {
	(*stream) << d;
}

inline void writeString(ostream* stream, const string& s) {
	(*stream) << s;
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


/*
 * Replaces the current content with a given T-mesh T using C++ move,
 * and also destroys the content in T (T becomes unusable after this function).
 */
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

/*
* Loads T-mesh information from the file specified by a given path.
* Returns 1 on success, 0 on failure.
*/
bool TMesh::meshFromFile(const string &path)
{
	// Try to open the file
	ifstream fs(path);
	if(!fs.is_open()) // File is not found or cannot be opened
	{
		fprintf(stderr, "Failed to open a T-mesh file for reading\n");
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

	// Read grid information
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


/*
* Saves T-mesh information to the file specified by a given path.
* Returns 1 on success, 0 on failure.
*/
bool TMesh::meshToFile(const string &path)
{
	// Try to open the file
	ofstream fs(path);
	if(!fs.is_open()) // File is not found or cannot be opened
	{
		fprintf(stderr, "Failed to open a T-mesh file for writing\n");
		return false;
	}

	// Lock to prevent changes while saving the T-mesh
	this->lock.lock();

	// Save dimensions and degrees
	{
		fs << this->rows << ' ' << this->cols << '\n';
		fs << this->rowDeg << ' ' << this->colDeg;
	}

// Returns ' ' if x > 0 and '\n' otherwise
#define separator(x) ("\n "[(x) > 0])

	// Save grid information
	{
		// - Horizontal: (R+1) x C bools
		if(this->cols > 0)
		{
			fs << '\n';
			for(int r = 0; r <= this->rows; ++r)
				for(int c = 0; c < this->cols; ++c)
					fs << separator(c) << (int)this->gridH[r][c];
		}
		// - Vertical: R x (C+1) bools
		if(this->rows > 0)
		{
			fs << '\n';
			for(int r = 0; r < this->rows; ++r)
				for(int c = 0; c <= this->cols; ++c)
					fs << separator(c) << (int)this->gridV[r][c];
		}
	}

	fs << setprecision(12);

	// Save knot values
	{
		fs << '\n';

		// - Horizontal: C + C_deg doubles
		if(this->cols > 0)
		{
			for(int i = 0; i < this->cols + this->colDeg; ++i)
				fs << separator(i) << this->knotsH[i];
		}
		// - Vertical: R + R_deg doubles
		if(this->rows > 0)
		{
			for(int i = 0; i < this->rows + this->rowDeg; ++i)
				fs << separator(i) << this->knotsV[i];
		}
	}

	// Save control point coordinates: (R+1) x (C+1) x 3 doubles
	{
		for(int r = 0; r <= this->rows; ++r)
		{
			fs << '\n';
			for(int c = 0; c <= this->cols; ++c)
			{
				fs << '\n';
				writePt3(&fs, this->gridPoints[r][c]->getCenter());
			}
		}
	}

	// Finish saving the T-mesh
	this->lock.unlock();

	// Signal the ending in the file
	fs << "\n\nEND" << endl;

	// The file is written successfully if it's still in good state.
	return fs.good();

#undef separator
}



void TMeshScene::setup(TMesh *tmesh)
{
	tmesh->lock.lock();
	gridSpheres = tmesh->gridPoints;
	for(int r = 0; r <= tmesh->rows; ++r)
	{
		for(int c = 0; c <= tmesh->cols; ++c)
		{
			bool doDraw = false;

			if(r == 0 || r == tmesh->rows || c == 0 || c == tmesh->cols)
				doDraw = true; // always draw boundary points (for now)
			else
			{
				bool bu = tmesh->gridV[r-1][c];
				bool bd = tmesh->gridV[r][c];
				bool bl = tmesh->gridH[r][c-1];
				bool br = tmesh->gridH[r][c];
				if((bu && bd && bl && br) || (bu != bd) || (bl != br))
					doDraw = true; // draw the point if it has an edge but is not an I-junction
			}

			if(!doDraw) gridSpheres[r][c] = NULL; // don't draw this sphere (gridpoint)
		}
	}
	tmesh->lock.unlock();
}