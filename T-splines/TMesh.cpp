#include "TMesh.h"

#include <iomanip>
#include <fstream>


inline bool TMesh::validateDimensionsAndDegrees(int r, int c, int degV, int degH)
{
	const int rcLimit = 1e4; // Limit up to 10^4 cells
	// Check for invalid dimensions
	if(!(r >= 0 && c >= 0 && r + c >= 1 && r * c <= rcLimit))
		return false;

	// Degrees: must be 0 if dim = 0, and must be within [1,dim] if dim > 0
	// Check for invalid vertical degrees (with row numbers)
	if(!((r > 0 && 1 <= degV && degV <= r) || (r == 0 && degV == 0)))
		return false;

	// Check for invalid horizontal degrees (with column numbers)
	if(!((c > 0 && 1 <= degH && degH <= c) || (c == 0 && degH == 0)))
		return false;
	return true;
}

// Knot values must have the right counts and be non-decreasing (sorted)
bool TMesh::validateKnots(const vector<double> &knots, int n, int deg)
{
	if((int)knots.size() != n + deg)
		return false;

	for(int i = 1; i < (int)knots.size(); ++i)
		if(knots[i-1] > knots[i]) // decreasing -> invalid
			return false;
	return true;
}

/*
 * Check if the knots at both ends are duplicates
 * with multiplicities at least the degrees.
 * The knots, dimension, and degree must be valid.
 */
bool TMesh::checkDuplicateAtKnotEnds(const vector<double> &knots, int n, int deg)
{
	if(deg <= 1) // no need to check if empty knots or using linear T-spline
		return true;
	else
	{
		// Deal with precision errors and assume monotonicity
		return knots[0] + 1e-9 > knots[deg - 1] && knots[n] + 1e-9 > knots[n + deg - 1];
	}
}

// Free sphere objects in the 2D array 'gridPoints'
void TMesh::freeGridPoints()
{
	for(auto &spheres: gridPoints)
	{
		for(auto &s: spheres)
		{
			delete s;
			s = NULL;
		}
	}
	gridPoints.clear();
}


TMesh::TMesh(int r, int c, int dv, int dh, bool autoFill)
{
	assert(validateDimensionsAndDegrees(r, c, dv, dh));
	rows = r;
	cols = c;
	degV = dv;
	degH = dh;

	// Assign some uniform knot values
	if(cols > 0)
	{
		knotsH.resize(cols + degH);
		if(autoFill) // some default values: 0, 1, ...
		{
			for(int i = 0; i < cols + degH; ++i)
				knotsH[i] = i;
			assert(validateKnots(knotsH, cols, degH));
		}
	}
	if(rows > 0)
	{
		knotsV.resize(rows + degV);
		if(autoFill) // some default values: 0, 1, ...
		{
			for(int i = 0; i < rows + degV; ++i)
				knotsV[i] = i;
			assert(validateKnots(knotsV, rows, degV));
		}
	}

	// Make the full grid initially
	gridH.assign(rows + 1, vector<bool>(cols, true));
	gridV.assign(cols, vector<bool>(cols + 1, true));

	// Assign some uniform coordinates initially
	gridPoints.resize(rows + 1, vector<Sphere*>(cols + 1, NULL));

	if(autoFill) // a default mesh on the XY-plane
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
	this->degH = T.degH;
	this->degV = T.degV;
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
	int degV1 = -1, degH1 = -1;
	{
		fs >> rows1 >> cols1;
		if(!fs.good())
		{
			fprintf(stderr, "Failed to read T-mesh dimensions (R x C)\n");
			return false;
		}
		fs >> degV1 >> degH1;
		if(!fs.good())
		{
			fprintf(stderr, "Failed to read degrees\n");
			return false;
		}
		if(!validateDimensionsAndDegrees(rows1, cols1, degV1, degH1))
		{
			fprintf(stderr, "Invalid T-mesh dimensions (%d x %d) or degrees V %d H %d\n",
				rows1, cols1, degV1, degH1);
			return false;
		}
	}

	TMesh T(rows1, cols1, degV1, degH1, false);

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
		// - Horizontal: C + deg_H doubles (C - deg_H + 2 in the middle)
		{
			int dupBit = -1, lb, ub;
			fs >> dupBit;
			if(!fs.good() || dupBit < 0 || dupBit > 1)
			{
				fprintf(stderr, "Bad flag for horizontal knot values\n");
				return false;
			}

			if(dupBit == 0) // All knots are provided in the file
			{
				lb = 0;
				ub = cols1 + degH1 - 1;
			}
			else // Need to duplicate knots at both ends
			{
				lb = degH1 - 1;
				ub = cols1;
			}

			for(int i = lb; i <= ub; ++i)
			{
				if(!(fs >> T.knotsH[i]))
				{
					fprintf(stderr, "Failed to read horizontal knot values\n");
					return false;
				}
			}
			if(dupBit) // Duplicate knots at ends
			{
				for(int i = 0; i < lb; ++i)
					T.knotsH[i] = T.knotsH[lb];
				for(int i = ub + 1; i < cols1 + degH1; ++i)
					T.knotsH[i] = T.knotsH[ub];
			}
			if(!validateKnots(T.knotsH, cols1, degH1))
			{
				fprintf(stderr, "Non-decreasing horizontal knot values or incorrect counts\n");
				return false;
			}
		}

		// - Vertical: R + deg_V doubles (R - deg_V + 2 in the middle)
		{
			int dupBit = -1, lb, ub;
			fs >> dupBit;
			if(!fs.good() || dupBit < 0 || dupBit > 1)
			{
				fprintf(stderr, "Bad flag for vertical knot values\n");
				return false;
			}

			if(dupBit == 0) // All knots are provided in the file
			{
				lb = 0;
				ub = rows1 + degV1 - 1;
			}
			else // Need to duplicate knots at both ends
			{
				lb = degV1 - 1;
				ub = rows1;
			}

			for(int i = lb; i <= ub; ++i)
			{
				if(!(fs >> T.knotsV[i]))
				{
					fprintf(stderr, "Failed to read vertical knot values\n");
					return false;
				}
			}
			if(dupBit) // Duplicate knots at ends
			{
				for(int i = 0; i < lb; ++i)
					T.knotsV[i] = T.knotsV[lb];
				for(int i = ub + 1; i < rows1 + degV1; ++i)
					T.knotsV[i] = T.knotsV[ub];
			}
			if(!validateKnots(T.knotsV, rows1, degV1))
			{
				fprintf(stderr, "Non-decreasing vertical knot values or incorrect counts\n");
				return false;
			}
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
		fprintf(stderr, "Bad ending format: missing the END tag\n");
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
		fs << this->degV << ' ' << this->degH;
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

		// - Horizontal: C + deg_H doubles
		fs << "\n0 "; // Provide all by default
		if(this->cols > 0)
		{
			for(int i = 0; i < this->cols + this->degH; ++i)
				fs << ' ' << this->knotsH[i];
		}
		// - Vertical: R + deg_V doubles
		fs << "\n0 "; // Provide all by default
		if(this->rows > 0)
		{
			for(int i = 0; i < this->rows + this->degV; ++i)
				fs << ' ' << this->knotsV[i];
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
	const Color C[6] = {Color(1,0,0),Color(0,1,0),Color(0,0,1),Color(1,0,0),Color(0,1,0),Color(0,0,1)};

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
				double t0 = T->knotsH[T->degH - 1];
				double t1 = T->knotsH[T->cols];
				double dt = (t1 - t0) / N;

				int p = T->degH - 1;
				for(int i = 0; i <= N; ++i)
				{
					double t = t0 + dt * i;

					// Find the correct segment for this parameter t
					updateSegmentIndex(p, T->cols, t, T->knotsH);

					// Collect the initial control points for this segment
					vector<PyramidNode> baseLayer(T->degH + 1);
					for(int j = 0; j <= T->degH; ++j)
					{
						int r1 = 0;
						int c1 = j + p - T->degH + 1;
						baseLayer[j].point = T->gridPoints[r1][c1]->getCenter();
						populateKnotLR(baseLayer[j], j + p, T->degH, T->knotsH);
					}

					/*
					* Evaluate at t - run the local de Boor Algorithm on this segment
					* Also store the index p for segment verification (for visualization)
					*/
					P[i] = {localDeBoor(T->degH, t, move(baseLayer)), p};
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

		double s0 = T->knotsV[T->degV - 1];
		double s1 = T->knotsV[T->rows];
		double ds = (s1 - s0) / RN;

		double t0 = T->knotsH[T->degH - 1];
		double t1 = T->knotsH[T->cols];
		double dt = (t1 - t0) / CN;

		int rp = T->degV - 1;
		for(int ri = 0; ri <= RN; ++ri)
		{
			double s = s0 + ds * ri;

			// Find the correct segment for this parameter s
			updateSegmentIndex(rp, T->rows, s, T->knotsV);

			int cp = T->degH - 1;
			for(int ci = 0; ci <= CN; ++ci)
			{
				double t = t0 + dt * ci;

				// Find the correct segment for this parameter t
				updateSegmentIndex(cp, T->cols, t, T->knotsH);

				// ** Process vertical (s) then horizontal (t) directions

				// Collect the initial control points for this horizontal segment
				vector<PyramidNode> pointsH(T->degH + 1);
				for(int c = 0; c <= T->degH; ++c)
				{
					// Collect the initial control points for each vertical segment
					vector<PyramidNode> pointsV(T->degV + 1);
					const int c1 = c + cp - T->degH + 1;
					for(int r = 0; r <= T->degV; ++r)
					{
						const int r1 = r + rp - T->degV + 1;
						pointsV[r].point = T->gridPoints[r1][c1]->getCenter();
						populateKnotLR(pointsV[r], r + rp, T->degV, T->knotsV);
					}

					// Run the local de Boor Algorithm on this vertical segment
					pointsH[c].point = localDeBoor(T->degV, s, move(pointsV));
					populateKnotLR(pointsH[c], c + cp, T->degH, T->knotsH);
				}

				// Run the local de Boor Algorithm on this horizontal segment
				S[ri][ci] = localDeBoor(T->degH, t, move(pointsH));
			}
		}

		setMesh(S);
	}
}