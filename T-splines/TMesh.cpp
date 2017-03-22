#include "TMesh.h"

#include <iomanip>
#include <fstream>


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
				writePt3(fs, this->gridPoints[r][c]->getCenter());
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
}


static Material* createMaterial() {
	Color amb,diff,spec;
	double spece;
	amb = Color(0.2,0.2,0.2,1);
	diff = Color(0.6,0.6,0.6,1);
	spec = Color(1,1,1,1);
	spece = 20;

	Material* ret = new Material();
	ret->setAmbient(amb);
	ret->setDiffuse(diff);
	ret->setSpecular(spec);
	ret->setSpecExponent(spece);
	return ret;
}

static Light* createLight(int id) {
	Pt3 pos;
	Color c;

	// x+,y+,z+,x-,y-,z-
	const Pt3 P[6] = {Pt3(100,0,0),Pt3(0,100,0),Pt3(0,0,100),Pt3(-100,0,0),Pt3(0,-100,0),Pt3(0,0,-100)};
	// RGB RGB
	const Color C[6] = {Color(0.5,0,0),Color(0,0.5,0),Color(0,0,0.5),Color(0.5,0,0),Color(0,0.5,0),Color(0,0,0.5)};

	pos = P[id];
	c = C[id];
	Light* ret = new Light();
	ret->setColor(c);
	ret->setPos(pos);
	return ret;
}

static TriMesh* createTriMesh(const VVP3 &S)
{
	int nverts, ntris;
	Pt3Array* pts = new Pt3Array();
	TriIndArray* inds = new TriIndArray();

	int R = SZ(S) - 1;
	int C = SZ(S[0]) - 1;

	nverts = (R + 1) * (C + 1);
	pts->recap(nverts);

	vector<VI> ID(R+1,VI(C+1));
	int id0 = 0;
	FOR(r,0,R+1) FOR(c,0,C+1)
	{
		ID[r][c] = id0++;
		pts->add(S[r][c]);
	}

	ntris = R * C * 2;
	inds->recap(ntris);
	FOR(r,0,R) FOR(c,0,C)
	{
		int w = ID[r][c];
		int x = ID[r+1][c];
		int y = ID[r+1][c+1];
		int z = ID[r][c+1];
		inds->add(TriInd(w,x,y));
		inds->add(TriInd(w,y,z));
	}

	TriMesh* ret = new TriMesh(pts,inds);

	// compute the normals
	ret->setFNormals(RenderingUtils::perFaceNormals(pts,inds));
	ret->setVNormals(RenderingUtils::perVertexNormals(pts,inds));

	return ret;
}

TriMeshScene::TriMeshScene()
{
	_mat = NULL;
	_mesh = NULL;
	useCurve = false;

	this->setMaterial(createMaterial());
	Color amb(0.1,0.1,0.1,1);

	int nlights = 6;
	for(int j = 0; j < nlights; j++)
	{
		Light* l = createLight(j);
		l->setAmbient(amb);
		this->addLight(l);
	}
}

void TriMeshScene::setCurve(vector<pair<Pt3, int>> points)
{
	curvePoints = move(points);
	useCurve = true;
}

void TriMeshScene::setMesh(const VVP3 &S)
{
	_mesh = createTriMesh(S);
	useCurve = false;
}




// Describes each node in the pyramid in the de Boor Algorithm
struct PyramidNode
{
	// Denote parameters t_l or t_r along the up-left (t-t_l) or up-right (t_r-t) arrow
	double knotL, knotR;
	// Coordinates of the point
	Pt3 point;
};

static Pt3 localDeBoor(int deg, double t, vector<PyramidNode> layer)
{
	// Run the local de Boor Algorithm on this segment
	for(int i = deg; i >= 1; --i)
	{
		// At this moment, we have i+1 points in the vector 'layer'.

		// Compute the new points
		vector<PyramidNode> newLayer(i);
		for(int j = 0; j < i; ++j)
		{
			double ta = layer[j + 1].knotL;
			double tb = layer[j].knotR;
			newLayer[j].knotL = ta;
			newLayer[j].knotR = tb;
			newLayer[j].point =
				layer[j].point * ((tb-t) / (tb-ta)) +
				layer[j+1].point * ((t-ta) / (tb-ta));
		}
		// Replace the current layer (i+1 points) with the new layer (i points)
		layer = move(newLayer);
	}

	return move(layer[0].point); // the top of the de Boor pyramid
}

// Update the index 'p' to cover the appropriate knot values given a particular parameter t
static void updateSegmentIndex(int &p, int n, double t, const vector<double> &knots)
{
	while(p + 1 < n && (t > knots[p + 1] + 1e-9 || abs(knots[p] - knots[p + 1]) < 1e-9))
		++p;
}

// Retrieve parameters t_l or t_r along the up-left (t-t_l) or up-right (t_r-t) arrow
static void populateKnotLR(PyramidNode &node, int p, int deg, const vector<double> &knots)
{
	int idL = p - deg;
	int idR = p + 1;
	node.knotL = (idL < 0) ? 0 : knots[idL];
	node.knotR = (idR >= (int) knots.size()) ? 0 : knots[idR];
}

void TriMeshScene::setScene(const TMesh *T)
{
	if(T->rows * T->cols == 0)
	{
		vector<pair<Pt3, int>> P;

		if(T->rows == 0) // 1 x (C+1) grid
		{
			if(1) // Interpolate points
			{
				const int N = 1000; // fixed for now
				P.resize(N + 1);
				double t0 = T->knotsH[T->colDeg - 1];
				double t1 = T->knotsH[T->cols];
				double dt = (t1 - t0) / N;

				int p = T->colDeg - 1;
				for(int i = 0; i <= N; ++i)
				{
					double t = t0 + dt * i;

					// Find the correct segment for this parameter t
					updateSegmentIndex(p, T->cols, t, T->knotsH);

					// Collect the initial control points for this segment
					vector<PyramidNode> baseLayer(T->colDeg + 1);
					for(int j = 0; j <= T->colDeg; ++j)
					{
						int r1 = 0;
						int c1 = j + p - T->colDeg + 1;
						baseLayer[j].point = T->gridPoints[r1][c1]->getCenter();
						populateKnotLR(baseLayer[j], j + p, T->colDeg, T->knotsH);
					}

					/*
					* Evaluate at t - run the local de Boor Algorithm on this segment
					* Also store the index p for segment verification (for visualization)
					*/
					P[i] = {localDeBoor(T->colDeg, t, move(baseLayer)), p};
				}
			}
			if(0) // Use the control points directly
			{
				P.resize(T->cols + 1);
				for(int i = 0; i <= T->cols; ++i)
					P[i] = {T->gridPoints[0][i]->getCenter(), 0};
			}
		}
		else // (R+1) x 1 grid
		{
			// TODO: make this the same as 1 x (C+1) grid (though, it's redundant)
			P.resize(T->rows + 1);
			for(int i = 0; i <= T->rows; ++i)
				P[i] = {T->gridPoints[i][0]->getCenter(), 0};
		}
		setCurve(P);
	}
	else
	{
		VVP3 S;
		// Interpolate points on the B-spline surface
		const int RN = 40;
		const int CN = 40;
		S.resize(RN + 1, VP3(CN + 1));

		double s0 = T->knotsV[T->rowDeg - 1];
		double s1 = T->knotsV[T->rows];
		double ds = (s1 - s0) / RN;

		double t0 = T->knotsH[T->colDeg - 1];
		double t1 = T->knotsH[T->cols];
		double dt = (t1 - t0) / CN;

		int rp = T->rowDeg - 1;
		for(int ri = 0; ri <= RN; ++ri)
		{
			double s = s0 + ds * ri;

			// Find the correct segment for this parameter s
			updateSegmentIndex(rp, T->rows, s, T->knotsV);

			int cp = T->colDeg - 1;
			for(int ci = 0; ci <= CN; ++ci)
			{
				double t = t0 + dt * ci;

				// Find the correct segment for this parameter t
				updateSegmentIndex(cp, T->cols, t, T->knotsH);

				// ** Process vertical (s) then horizontal (t) directions

				// Collect the initial control points for this horizontal segment
				vector<PyramidNode> pointsH(T->colDeg + 1);
				for(int c = 0; c <= T->colDeg; ++c)
				{
					// Collect the initial control points for each vertical segment
					vector<PyramidNode> pointsV(T->rowDeg + 1);
					const int c1 = c + cp - T->colDeg + 1;
					for(int r = 0; r <= T->rowDeg; ++r)
					{
						const int r1 = r + rp - T->rowDeg + 1;
						pointsV[r].point = T->gridPoints[r1][c1]->getCenter();
						populateKnotLR(pointsV[r], r + rp, T->rowDeg, T->knotsV);
					}

					// Run the local de Boor Algorithm on this vertical segment
					pointsH[c].point = localDeBoor(T->rowDeg, s, move(pointsV));
					populateKnotLR(pointsH[c], c + cp, T->colDeg, T->knotsH);
				}

				// Run the local de Boor Algorithm on this horizontal segment
				S[ri][ci] = localDeBoor(T->colDeg, t, move(pointsH));
			}
		}

		setMesh(S);
	}
}