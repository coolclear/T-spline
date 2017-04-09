#include "TMesh.h"

#include <iomanip>
#include <fstream>


inline bool TMesh::validateDimensionsAndDegrees(int r, int c, int degV, int degH)
{
	const int rcLimit = 10000; // Limit up to 10^4 cells
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
		if(autoFill) // some default values: 0, 1, ... (duplicated at ends)
		{
			for(int i = 0; i < cols + degH; ++i)
				knotsH[i] = min(cols, max(degH - 1, i)) - (degH - 1);
			assert(validateKnots(knotsH, cols, degH));
		}
	}
	if(rows > 0)
	{
		knotsV.resize(rows + degV);
		if(autoFill) // some default values: 0, 1, ... (duplicated at ends)
		{
			for(int i = 0; i < rows + degV; ++i)
				knotsV[i] = min(rows, max(degV - 1, i)) - (degV - 1);
			assert(validateKnots(knotsV, rows, degV));
		}
	}

	// Make the full grid initially
	gridH.assign(rows + 1, vector<EdgeInfo>(cols, EdgeInfo(true)));
	gridV.assign(cols, vector<EdgeInfo>(cols + 1, EdgeInfo(true)));

	// Assign some uniform coordinates initially
	gridPoints.resize(rows + 1, vector<VertexInfo>(cols + 1));

	if(autoFill) // a default mesh on the XY-plane
	{
		for(int i = 0; i <= rows; ++i)
			for(int j = 0; j <= cols; ++j)
				gridPoints[i][j] = VertexInfo(Pt3((j+1)*0.5, (rows-i+1)*0.5, 0), 0, 0);
	}

	updateMeshInfo();
}

TMesh::~TMesh() {}


/*
* Replaces the current content with a given T-mesh T using C++ move(),
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
	this->gridPoints = move(T.gridPoints);
	this->updateMeshInfo();

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
		// - Horizontal: (R-1) x C bools
		for(int r = 1; r < rows1; ++r) // boundary H-lines are 1 by default
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
				T.gridH[r][c].on = bit;
			}
		}
		// - Vertical: R x (C-1) bools
		for(int r = 0; r < rows1; ++r)
		{
			for(int c = 1; c < cols1; ++c) // boundary V-lines are 1 by default
			{
				int bit = -1;
				fs >> bit;
				if(!fs.good() || bit < 0 || bit > 1)
				{
					fprintf(stderr, "Failed to read vertical grid info\n");
					return false;
				}
				T.gridV[r][c].on = bit;
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
			T.gridPoints[r][c].position = p;
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
		// - Horizontal: (R-1) x C bools
		if(this->cols > 0)
		{
			fs << '\n';
			for(int r = 1; r < this->rows; ++r)
				for(int c = 0; c < this->cols; ++c)
					fs << separator(c) << (int)this->gridH[r][c].on;
		}
		// - Vertical: R x (C-1) bools
		if(this->rows > 0)
		{
			fs << '\n';
			for(int r = 0; r < this->rows; ++r)
				for(int c = 1; c < this->cols; ++c)
					fs << separator(c-1) << (int)this->gridV[r][c].on;
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
				writePt3(fs, this->gridPoints[r][c].position);
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



inline bool TMesh::isWithinGrid(int r, int c) const
{
	return r >= 0 && r <= rows && c >= 0 && c <= cols;
}

inline bool TMesh::useVertex(int r, int c) const
{
	return isWithinGrid(r, c) && gridPoints[r][c].valenceType >= 3;
}

// Whether the current vertex (r,c) is skipped (depending on 'isVert')
inline bool TMesh::isSkipped(int r, int c, bool isVert) const
{
	const int bits = gridPoints[r][c].valenceBits;
	return (isVert && bits == 0b0011) ||
		(!isVert && bits == 0b1100) ||
		gridPoints[r][c].valenceType == 0;
}

// Mark vertices along the extension line (degrees - 1 steps forward, 1 step backward)
void TMesh::markExtension(int r0, int c0, int dr, int dc, int fwSteps, bool isVert)
{
	const int val = isVert ? EXTENSION_VERTICAL : EXTENSION_HORIZONTAL;
	int r = r0;
	int c = c0;

	// Mark from the T-junction (e.g., up for T, left for |-)
	while(fwSteps >= 0 && isWithinGrid(r, c))
	{
		gridPoints[r][c].extendFlag |= val;
		if(isVert)
			gridV[r - max(dr, 0)][c].extend = true;
		else
			gridH[r][c - max(dc, 0)].extend = true;

		if(!isSkipped(r, c, isVert))
			--fwSteps;
		r += dr;
		c += dc;
	}

	// Mark back one step (e.g., down for T, right for |-)
	r = r0 - dr;
	c = c0 - dc;
	while(isWithinGrid(r, c))
	{
		gridPoints[r][c].extendFlag |= val;
		if(isVert)
			gridV[r + min(dr, 0)][c].extend = true;
		else
			gridH[r][c + min(dc, 0)].extend = true;

		if(!isSkipped(r, c, isVert))
			break;
		r -= dr;
		c -= dc;
	}
}

/*
 * Update implicit mesh information that is computed but not input
 * and also verify if
 *  1) vertices and edges are in good shape
 *  2) whether the T-mesh is admissible (AD)
 *  3) whether the T-mesh is analysis-suitable (AS)
 * The calling thread should lock the mutex before calling
 */
void TMesh::updateMeshInfo()
{
	validVertices = true;

	FOR(r,0,rows + 1) FOR(c,0,cols + 1)
	{
		int &valenceBits = gridPoints[r][c].valenceBits; // 0-3: directions UDLR
		int &valenceType = gridPoints[r][c].valenceType; // 0:don't draw, 2-4:valence
		valenceBits = 0;
		valenceType = 0;
		gridPoints[r][c].extendFlag = 0;

		int boundaryCount = 0;
		boundaryCount += (r == 0); // top row?
		boundaryCount += (r == rows); // bottom row?
		boundaryCount += (c == 0); // leftmost column?
		boundaryCount += (c == cols); // rightmost column?

		int valenceCount = 0;
		auto addBit = [&](int b, int val)
		{
			if(val)
			{
				++valenceCount;
				valenceBits |= b;
			}
		};
		addBit(VALENCE_BIT_UP, r > 0 && gridV[r-1][c].on); // up
		addBit(VALENCE_BIT_DOWN, r < rows && gridV[r][c].on); // down
		addBit(VALENCE_BIT_LEFT, c > 0 && gridH[r][c-1].on); // left
		addBit(VALENCE_BIT_RIGHT, c < cols && gridH[r][c].on); // right

		if(boundaryCount == 0) // inner vertices
		{
			if(valenceCount >= 3)
				valenceType = valenceCount;
			else if(valenceCount == 0)
				valenceType = 0; // no line
			else if(valenceCount == 2 && (valenceBits == 3 || valenceBits == 12))
				valenceType = 2; // vertical or horizontal lines
			else
			{
				valenceType = VALENCE_INVALID;
				validVertices = false; // no longer consider AD or AS
			}
		}
		else if(boundaryCount == 1) // side vertices (not corners)
		{
			if(valenceCount == 3)
				valenceType = 4;
		}
		else // boundaryCount == 2, corner vertices
		{
			valenceType = 4; // Always draw the corners
		}
	}


	// Validate edges (only if vertices are fine)
	FOR(r,0,rows + 1) FOR(c,0,cols)
	{
		gridH[r][c].valid = true;
		gridH[r][c].extend = false;
	}
	FOR(r,0,rows) FOR(c,0,cols + 1)
	{
		gridV[r][c].valid = true;
		gridV[r][c].extend = false;
	}

	// Don't check if doing curves (1D)
	if(rows * cols == 0)
	{
		isAD = isAS = true;
		return;
	}

	// Check whether the mesh is AD (before checking AS)
	if(!validVertices)
	{
		isAD = false;
		// Since vertices aren't even valid, just ignore issues with edges
	}
	else
	{
		isAD = true; // may turn out to be false after checking

		// Draw H-links
		FOR(r,0,rows + 1)
		{
			int lastC = -1;
			FOR(c,0,cols + 1)
			{
				int type = gridPoints[r][c].valenceType;
				if(type <= 0) // only consider full (4), T (3), horizontal (2), or vertical (2)
					continue;
				if(lastC >= 0)
				{
					if(type == 3 && gridPoints[r][lastC].valenceType == 3 &&
						!gridH[r][c-1].on)
					{
						FOR(i,lastC,c)
							gridH[r][i].valid = false;
						isAD = false;
					}
				}
				lastC = c;
			}
		}

		// Draw V-links
		FOR(c,0,cols + 1)
		{
			int lastR = -1;
			FOR(r,0,rows + 1)
			{
				int type = gridPoints[r][c].valenceType;
				if(type <= 0) // only consider full (4), T (3), horizontal (2), or vertical (2)
					continue;
				if(lastR >= 0)
				{
					if(type == 3 && gridPoints[lastR][c].valenceType == 3 &&
						!gridV[r-1][c].on)
					{
						FOR(i,lastR,r)
							gridV[i][c].valid = false;
						isAD = false;
					}
				}
				lastR = r;
			}
		}
	}

	// Check whether the mesh is AS (must also be AD)
	if(!isAD)
		isAS = false;
	else
	{
		isAS = true; // may turn out to be false after checking

		// Compute T-junction extensions (ignore boundary vertices)
		FOR(r,1,rows) FOR(c,1,cols)
		{
			// Only consider T-junctions
			if(gridPoints[r][c].valenceType != 3)
				continue;

			switch(gridPoints[r][c].valenceBits)
			{
			case 0b1110: // T
				markExtension(r, c, -1, 0, degV - 1, true);
				break;
			case 0b1101: // _|_
				markExtension(r, c, 1, 0, degV - 1, true);
				break;
			case 0b1011: // |-
				markExtension(r, c, 0, -1, degH - 1, false);
				break;
			case 0b0111: // -|
				markExtension(r, c, 0, 1, degH - 1, false);
				break;
			}
		}

		FOR(r,0,rows + 1) FOR(c,0,cols + 1)
		{
			if(gridPoints[r][c].extendFlag == EXTENSION_BOTH)
			{
				isAS = false;
				goto doneAS;
			}
		}

		doneAS:;
	}
}


// Initialize mesh information for the scene
void TMeshScene::setup(TMesh *tmesh)
{
	mesh = tmesh;
	updateScene();
}

// Update the grid spheres for the scene, including coordinates
void TMeshScene::updateScene()
{
	// Update dynamic objects in 'gridSpheres' if dimensions change
	if(rows != mesh->rows || cols != mesh->cols)
	{
		freeGridSpheres();
		sphereIndices.clear();

		rows = mesh->rows;
		cols = mesh->cols;
		gridSpheres.assign(rows + 1, vector<PSO>(cols + 1, PSO(NULL, NULL)));

		FOR(r,0,rows + 1) FOR(c,0,cols + 1)
		{
			Sphere *sphere = new Sphere(Pt3(), radius);
			sphereIndices[sphere] = {r, c};
			gridSpheres[r][c] = PSO(sphere, new Operator(sphere));
		}
	}

	FOR(r,0,rows + 1) FOR(c,0,cols + 1)
		gridSpheres[r][c].first->setCenter(mesh->gridPoints[r][c].position);
}

void TMeshScene::updateSphere(Sphere *sphere)
{
	auto it = sphereIndices.find(sphere);
	if(it != sphereIndices.end())
	{
		int r = it->second.first;
		int c = it->second.second;
		mesh->gridPoints[r][c].position = sphere->getCenter();
	}
}

// Free sphere-operator objects in the 2D array 'gridSpheres'
void TMeshScene::freeGridSpheres()
{
	for(auto &spheres: gridSpheres)
	{
		for(auto &s: spheres)
		{
			delete s.first;
			delete s.second;
		}
	}
	gridSpheres.clear();
}

bool TMeshScene::useSphere(int r, int c) const
{
	return mesh->useVertex(r, c);
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
						baseLayer[j].point = T->gridPoints[r1][c1].position;
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
					P[i] = {T->gridPoints[0][i].position, 0};
			}
		}
		else // (R+1) x 1 grid
		{
			// TODO: make this the same as 1 x (C+1) grid (though, it's redundant)
			P.resize(T->rows + 1);
			for(int i = 0; i <= T->rows; ++i)
				P[i] = {T->gridPoints[i][0].position, 0};
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
						pointsV[r].point = T->gridPoints[r1][c1].position;
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