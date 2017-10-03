#include "TMesh.h"

#include <iomanip>
#include <fstream>


inline bool TMesh::validateDimensionsAndDegrees(int r, int c, int degV, int degH)
{
	const int rcLimit = 10000; // Limit up to 10^4 cells
	// Check for invalid dimensions
	if(!(r >= 0 and c >= 0 and r + c >= 1 and r * c <= rcLimit))
		return false;

	// Degrees: must be 0 if dim = 0, and must be within [1,dim] if dim > 0
	// Check for invalid vertical degrees (with row numbers)
	if(!((r > 0 and 1 <= degV and degV <= r) or (r == 0 and degV == 0)))
		return false;

	// Check for invalid horizontal degrees (with column numbers)
	if(!((c > 0 and 1 <= degH and degH <= c) or (c == 0 and degH == 0)))
		return false;

	// (For now) Restrict the surface degrees to be (3,3)
	if(degH != 3 or degV != 3)
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
		return knots[0] + 1e-9 > knots[deg - 1] and knots[n] + 1e-9 > knots[n + deg - 1];
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
	gridV.assign(rows, vector<EdgeInfo>(cols + 1, EdgeInfo(true)));

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
				if(!fs.good() or bit < 0 or bit > 1)
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
				if(!fs.good() or bit < 0 or bit > 1)
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
			if(!fs.good() or dupBit < 0 or dupBit > 1)
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
			if(!fs.good() or dupBit < 0 or dupBit > 1)
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



bool TMesh::isWithinGrid(int r, int c) const
{
	return r >= 0 and r <= rows and c >= 0 and c <= cols;
}

bool TMesh::useVertex(int r, int c) const
{
	return isWithinGrid(r, c) and gridPoints[r][c].valenceType >= 3;
}

// Whether the current vertex (r,c) is skipped (depending on 'isVert')
bool TMesh::isSkipped(int r, int c, bool isVert) const
{
	const int bits = gridPoints[r][c].valenceBits;
	return (isVert and bits == 0b0011) or
		(!isVert and bits == 0b1100) or
		gridPoints[r][c].valenceType == 0;
}

void TMesh::cap(int& r, int& c) const
{
	r = max(0, min(rows, r));
	c = max(0, min(cols, c));
}

// Mark vertices along the extension line (degrees - 1 steps forward, 1 step backward)
void TMesh::markExtension(int r0, int c0, int dr, int dc, int fwSteps, bool isVert)
{
	const int val = isVert ? EXTENSION_VERTICAL : EXTENSION_HORIZONTAL;
	int r = r0;
	int c = c0;

	// Mark from the T-junction (e.g., up for T, left for |-)
	while(fwSteps >= 0 and isWithinGrid(r, c))
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
		addBit(VALENCE_BIT_UP, r > 0 and gridV[r-1][c].on); // up
		addBit(VALENCE_BIT_DOWN, r < rows and gridV[r][c].on); // down
		addBit(VALENCE_BIT_LEFT, c > 0 and gridH[r][c-1].on); // left
		addBit(VALENCE_BIT_RIGHT, c < cols and gridH[r][c].on); // right

		if(boundaryCount == 0) // inner vertices
		{
			if(valenceCount >= 3)
				valenceType = valenceCount;
			else if(valenceCount == 0)
				valenceType = 0; // no line
			else if(valenceCount == 2 and (valenceBits == 3 or valenceBits == 12))
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
					if(type == 3 and gridPoints[r][lastC].valenceType == 3 and
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
					if(type == 3 and gridPoints[lastR][c].valenceType == 3 and
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

void TMesh::getTiledFloorRange(const int r, const int c, int& r_min, int& r_max, int& c_min, int& c_max) const
{
	int r_cap {max(0, min(rows, r))};
	int c_cap {max(0, min(cols, c))};

	auto onVSkel = [&](int r0)
	{
		r0 = max(0, min(rows, r0));
		return useVertex(r0, c_cap) or gridPoints[r0][c_cap].valenceBits == 12;
	};
	auto onHSkel = [&](int c0)
	{
		c0 = max(0, min(cols, c0));
		return useVertex(r_cap, c0) or gridPoints[r_cap][c0].valenceBits == 3;
	};
	r_min = r_max = r;
	c_min = c_max = c;
	FOR(k,0,2)
	{
		while(--r_min >= 0 and not onVSkel(r_min));
		while(++r_max <= rows and not onVSkel(r_max));
		while(--c_min >= 0 and not onHSkel(c_min));
		while(++c_max <= cols and not onHSkel(c_max));
	}
	r_min = max(r_min, 0);
	r_max = min(r_max, rows);
	c_min = max(c_min, 0);
	c_max = min(c_max, cols);
};

void TMesh::get16Points(int ur, int uc, vector<pair<int,int>>& blendP, bool& row_n_4, bool& col_n_4) const
{
	map<int,int> rowCounts, colCounts;
	blendP.clear();
	blendP.reserve(16);

	// Loop over all vertices of the frame region (including inactive ones)
	FOR(r,-2,rows+3) FOR(c,-2,cols+3)
	{
		int r_cap {r};
		int c_cap {c};
		cap(r_cap, c_cap);

		// Ignore non-vertex
		if(not useVertex(r_cap, c_cap)) continue;

		int r_min, r_max, c_min, c_max;
		getTiledFloorRange(r, c, r_min, r_max, c_min, c_max);

		if(r_min <= ur and ur < r_max and c_min <= uc and uc < c_max)
		{
			blendP.emplace_back(r, c);
			++rowCounts[r];
			++colCounts[c];
		}
	}

	assert(SZ(blendP) == 16);

	row_n_4 = SZ(rowCounts) == 4;
	col_n_4 = SZ(colCounts) == 4;
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
	if(rows != mesh->rows or cols != mesh->cols)
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

static TriMesh* createTriMesh2(const vector<VVP3>& Ss)
{
	// Count the number of vertices and triangles to allocate just enough memory
	int nverts {0};
	int ntris {0};
	for(auto& S: Ss)
	{
		int R {SZ(S) - 1};
		assert(R >= 0);
		int C {SZ(S[0]) - 1};
		assert(C >= 0);

		nverts += (R + 1) * (C + 1);
		ntris += R * C * 2;
	}

	Pt3Array* pts = new Pt3Array();
	TriIndArray* inds = new TriIndArray();
	pts->recap(nverts);
	inds->recap(ntris);
	int id0 = 0; // running vertex index

	// Build a tri-mesh from each unit element
	for(auto& S: Ss)
	{
		int R {SZ(S) - 1};
		int C {SZ(S[0]) - 1};

		vector<VI> ID(R+1,VI(C+1));
		FOR(r,0,R+1) FOR(c,0,C+1)
		{
			ID[r][c] = id0++;
			pts->add(S[r][c]);
		}

		FOR(r,0,R) FOR(c,0,C)
		{
			// wz : w  | wz
			// xy : xy |  y
			int w = ID[r][c];
			int x = ID[r+1][c];
			int y = ID[r+1][c+1];
			int z = ID[r][c+1];
			inds->add(TriInd(w,x,y));
			inds->add(TriInd(w,y,z));
		}
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

void TriMeshScene::setMesh(const VVP3& S)
{
	_mesh = createTriMesh(S);
	useCurve = false;
}

void TriMeshScene::setMesh2(const vector<VVP3>& S)
{
	_mesh = createTriMesh2(S);
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
	while(p + 1 < n and (t > knots[p + 1] + 1e-9 or abs(knots[p] - knots[p + 1]) < 1e-9))
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
/*
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
*/

		vector<VVP3> Ss;
		VVP3 S;
		FOR(ur,1,T->rows-1) FOR(uc,1,T->cols-1)
		{
			const double s0 {T->knotsV[ur + 1]};
			const double s1 {T->knotsV[ur + 2]};
			const double t0 {T->knotsH[uc + 1]};
			const double t1 {T->knotsH[uc + 2]};
			// Skip unit elements with zero-area parameter space (s,t)
			if(s0 + 1e-9 > s1 or t0 + 1e-9 > t1) continue;

			bool ready {false};
			auto populateS = [&](double r_margin, double c_margin)
			{
				S.assign(2, VP3(2));
				int r {ur};
				int c {uc};
				FOR(i,0,2) FOR(j,0,2)
				{
					Pt3 p(0,0,0,0);
					FOR(a,0,2) FOR(b,0,2)
					{
						double wa {(a == i) ? 1 - r_margin : r_margin};
						double wb {(b == j) ? 1 - c_margin : c_margin};
						p += T->gridPoints[r+a][c+b].position * wa * wb;
					}
					S[i][j] = p;
				}

				ready = true;
			};

			// Retrieve the 16 blending points for the unit element (ur, uc)
			bool row_n_4, col_n_4;
			vector<pair<int,int>> blendP;
			T->get16Points(ur, uc, blendP, row_n_4, col_n_4);

			auto findVertex = [&](int& r, int& c, int dr, int dc)
			{
				do
				{
					r += dr;
					c += dc;
					if(r < 0)
					{
						r = 0;
						break;
					}
					else if(r > T->rows)
					{
						r = T->rows;
						break;
					}
					if(c < 0)
					{
						c = 0;
						break;
					}
					else if(c > T->cols)
					{
						c = T->cols;
						break;
					}
				}
				while(not T->useVertex(r,c));
			};

			if(row_n_4) // can process row-then-column
			{
				// blendP: row-major by default

				// Restrict the vertices to within the active region
				for(auto& p: blendP) T->cap(p._1, p._2);

				auto findHEdge = [&](int& r, int dr)
				{
					do
					{
						r += dr;
						if(r < 0)
						{
							r = 0;
							break;
						}
						else if(r > T->rows)
						{
							r = T->rows;
							break;
						}
					}
					while(not T->gridH[r][uc].on);
				};

				// Horizontal knot vectors, one per row
				vector<double> kH[4];
				for(int r = 0; r <= 3; ++r)
				{
					kH[r].reserve(6);
					// Get knot value from the previous point to the left of P[r][0]
					{
						int r1, c1;
						tie(r1, c1) = blendP[r * 4];
						findVertex(r1, c1, 0, -1);
						kH[r].emplace_back(T->knotsH[c1 + 1]);
					}
					// Get knot values from P[r][0..3]
					for(int c = 0; c <= 3; ++c) kH[r].emplace_back(T->knotsH[blendP[r * 4 + c]._2 + 1]);
					// Get knot value from the next point to the right of P[r][3]
					{
						int r1, c1;
						tie(r1, c1) = blendP[r * 4 + 3];
						findVertex(r1, c1, 0, +1);
						kH[r].emplace_back(T->knotsH[c1 + 1]);
					}
				}

				// Vertical knot vector
				vector<double> kV;
				kV.reserve(6);
				// Get knot value from the previous point upward from P[0][0]
				{
					int r1 {blendP[0]._1};
					findHEdge(r1, -1);
					kV.emplace_back(T->knotsV[r1 + 1]);
				}
				// Get knot values from P[0..3][0]
				for(int r = 0; r <= 3; ++r) kV.emplace_back(T->knotsV[blendP[r * 4]._1 + 1]);
				// Get knot value from the next point down from P[3][0]
				{
					int r1 {blendP[12]._1};
					findHEdge(r1, +1);
					kV.emplace_back(T->knotsV[r1 + 1]);
				}

				const int RN {10};
				const int CN {10};
				const double ds {(s1 - s0) / RN};
				const double dt {(t1 - t0) / CN};

				S.assign(RN + 1, VP3(CN + 1));
				FOR(ri,0,RN+1) FOR(ci,0,CN+1)
				{
					const double s {s0 + ds * ri};
					const double t {t0 + dt * ci};

					// Collect the initial control points for this vertical segment
					vector<PyramidNode> pointsV(4);
					for(int r = 0; r <= 3; ++r)
					{
						// Collect the initial control points for each horizontal segment
						vector<PyramidNode> pointsH(4);
						for(int c = 0; c <= 3; ++c)
						{
							int bp_r, bp_c;
							tie(bp_r, bp_c) = blendP[r * 4 + c];

							pointsH[c].point = T->gridPoints[bp_r][bp_c].position;
							populateKnotLR(pointsH[c], c + 2, 3, kH[r]);
						}

						// Run the local de Boor algorithm on this horizontal segment
						populateKnotLR(pointsV[r], r + 2, 3, kV);
						pointsV[r].point = localDeBoor(3, t, move(pointsH));
					}

					// Run the local de Boor Algorithm on this vertical segment
					S[ri][ci] = localDeBoor(3, s, move(pointsV));
				}

				ready = true;
			}
			else if(col_n_4) // can process column-then-row
			{
				// Make blendP column-major
				sort(begin(blendP), end(blendP), [&](auto& p, auto& q)
				{
					if(p._2 != q._2) return p._2 < q._2;
					else return p._1 < q._1;
				});
				// Restrict the vertices to within the active region
				for(auto& p: blendP) T->cap(p._1, p._2);
				// Make blendP row-major again (now sorted)
				FOR(i,0,4) FOR(j,0,i) swap(blendP[i*4 + j], blendP[j*4 + i]);

				auto findVEdge = [&](int& c, int dc)
				{
					do
					{
						c += dc;
						if(c < 0)
						{
							c = 0;
							break;
						}
						else if(c > T->cols)
						{
							c = T->cols;
							break;
						}
					}
					while(not T->gridV[ur][c].on);
				};

				// Vertical knot vectors, one per column
				vector<double> kV[4];
				for(int c = 0; c <= 3; ++c)
				{
					kV[c].reserve(6);
					// Get knot value from the previous point upward from P[0][c]
					{
						int r1, c1;
						tie(r1, c1) = blendP[c];
						findVertex(r1, c1, -1, 0);
						kV[c].emplace_back(T->knotsV[r1 + 1]);
					}
					// Get knot values from P[r][0..3]
					for(int r = 0; r <= 3; ++r) kV[c].emplace_back(T->knotsV[blendP[r * 4 + c]._1 + 1]);
					// Get knot value from the next point downward from P[3][c]
					{
						int r1, c1;
						tie(r1, c1) = blendP[12 + c];
						findVertex(r1, c1, +1, 0);
						kV[c].emplace_back(T->knotsV[r1 + 1]);
					}
				}

				// Horizontal knot vector
				vector<double> kH;
				kH.reserve(6);
				// Get knot value from the previous point to the left of P[0][0]
				{
					int c1 {blendP[0]._2};
					findVEdge(c1, -1);
					kH.emplace_back(T->knotsH[c1 + 1]);
				}
				// Get knot values from P[0][0..3]
				for(int c = 0; c <= 3; ++c) kH.emplace_back(T->knotsH[blendP[c]._2 + 1]);
				// Get knot value from the next point to the right of P[0][3]
				{
					int c1 {blendP[3]._2};
					findVEdge(c1, +1);
					kH.emplace_back(T->knotsH[c1 + 1]);
				}

				const int RN {10};
				const int CN {10};
				const double ds {(s1 - s0) / RN};
				const double dt {(t1 - t0) / CN};

				S.assign(RN + 1, VP3(CN + 1));
				FOR(ri,0,RN+1) FOR(ci,0,CN+1)
				{
					const double s {s0 + ds * ri};
					const double t {t0 + dt * ci};

					// Collect the initial control points for this horizontal segment
					vector<PyramidNode> pointsH(4);
					for(int c = 0; c <= 3; ++c)
					{
						// Collect the initial control points for each vertical segment
						vector<PyramidNode> pointsV(4);
						for(int r = 0; r <= 3; ++r)
						{
							int bp_r, bp_c;
							tie(bp_r, bp_c) = blendP[r * 4 + c];

							pointsV[r].point = T->gridPoints[bp_r][bp_c].position;
							populateKnotLR(pointsV[r], r + 2, 3, kV[c]);
						}

						// Run the local de Boor algorithm on this vertical segment
						populateKnotLR(pointsH[c], c + 2, 3, kH);
						pointsH[c].point = localDeBoor(3, s, move(pointsV));
					}

					// Run the local de Boor Algorithm on this horizontal segment
					S[ri][ci] = localDeBoor(3, t, move(pointsH));
				}

				ready = true;
			}


			// Testing: show which direction each unit element can be computed first (de Boor)
			if(false)
			{
				if(SZ(blendP) == 16)
				{
					double r_margin {0.3};
					double c_margin {0.3};
					bool renderable {false};
					// Check if there are only four rows (each with 4 vertices)
					if(row_n_4)
					{
						c_margin = 0.05;
						renderable = true;
					}
					// Check if there are only four columns (each with 4 vertices)
					if(col_n_4)
					{
						r_margin = 0.05;
						renderable = true;
					}

					if(renderable) populateS(r_margin, c_margin);
				}
				else
				{
					cout << "SZ(blendP) = " << SZ(blendP) << endl;
				}
			}

			if(ready) Ss.emplace_back(move(S));
		}

		printf("set scene2\n");
		setMesh2(Ss);
	}
}