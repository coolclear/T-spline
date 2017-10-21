#include "TMesh.h"

#include <iomanip>
#include <fstream>
#include <set>

#undef assert
#define assert(x) {if(not (x)) cout << "\n****** ASSERTION FAILED : " << (#x) << '\n' << endl;}

bool TMesh::validateDimensionsAndDegrees(int r, int c, int degV, int degH)
{
	const int rcLimit = 10000; // Limit up to 10^4 cells
	// Check for invalid dimensions
	if(not (r >= 0 and c >= 0 and r + c >= 1 and r * c <= rcLimit))
		return false;

	// Degrees: must be 0 if dim = 0, and must be within [1,dim] if dim > 0
	// Check for invalid vertical degrees (with row numbers)
	if(not ((r > 0 and 1 <= degV and degV <= r) or (r == 0 and degV == 0)))
		return false;

	// Check for invalid horizontal degrees (with column numbers)
	if(not ((c > 0 and 1 <= degH and degH <= c) or (c == 0 and degH == 0)))
		return false;

	// (For now) Restrict the surface degrees to be (3,3)
	if(degH != 3 or degV != 3)
		return false;

	return true;
}

// Knot values must have the right counts and be non-decreasing (sorted)
bool TMesh::validateKnots(const vector<double> &knots, int n, int deg)
{
	if(SZ(knots) != n + deg)
		return false;

	for(int i = 1; i < SZ(knots); ++i)
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
				gridPoints[i][j] = VertexInfo(Pt3((j+1)*0.5, (rows-i+1)*0.5, 0), 0, 0, -1, -1);
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
	if(not fs.is_open()) // File is not found or cannot be opened
	{
		fprintf(stderr, "Failed to open a T-mesh file for reading\n");
		return false;
	}

	// Read in dimensions and degrees, and validate them
	int rows1 = -1, cols1 = -1;
	int degV1 = -1, degH1 = -1;
	{
		fs >> rows1 >> cols1;
		if(not fs.good())
		{
			fprintf(stderr, "Failed to read T-mesh dimensions (R x C)\n");
			return false;
		}
		fs >> degV1 >> degH1;
		if(not fs.good())
		{
			fprintf(stderr, "Failed to read degrees\n");
			return false;
		}
		if(not validateDimensionsAndDegrees(rows1, cols1, degV1, degH1))
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
				if(not fs.good() or bit < 0 or bit > 1)
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
				if(not fs.good() or bit < 0 or bit > 1)
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
			if(not fs.good() or dupBit < 0 or dupBit > 1)
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
				if(not (fs >> T.knotsH[i]))
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
			if(not validateKnots(T.knotsH, cols1, degH1))
			{
				fprintf(stderr, "Non-decreasing horizontal knot values or incorrect counts\n");
				return false;
			}
		}

		// - Vertical: R + deg_V doubles (R - deg_V + 2 in the middle)
		{
			int dupBit = -1, lb, ub;
			fs >> dupBit;
			if(not fs.good() or dupBit < 0 or dupBit > 1)
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
				if(not (fs >> T.knotsV[i]))
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
			if(not validateKnots(T.knotsV, rows1, degV1))
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
				if(not (fs >> p[i]))
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
	if(not fs.is_open()) // File is not found or cannot be opened
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
		(not isVert and bits == 0b1100) or
		gridPoints[r][c].valenceType == 0;
}

void TMesh::cap(int& r, int& c) const
{
	r = max(0, min(rows, r));
	c = max(0, min(cols, c));
}

// Mark vertices along the extension line (degrees - 1 steps forward, 1 step backward)
void TMesh::markExtension(int r0, int c0, int dr, int dc, bool isVert, int& minRes, int& maxRes)
{
	const int val {isVert ? EXTENSION_VERTICAL : EXTENSION_HORIZONTAL};
	int fwSteps {2};
	int r {r0};
	int c {c0};
	minRes = isVert ? rows : cols;
	maxRes = 0;

	// Mark from the T-junction (e.g., up for T, left for |-)
	while(fwSteps >= 0 and isWithinGrid(r, c))
	{
		int t;
		gridPoints[r][c].extendFlag |= val;
		if(isVert)
			gridV[t = r - max(dr, 0)][c].extend = true;
		else
			gridH[r][t = c - max(dc, 0)].extend = true;
		minRes = min(minRes, t);
		maxRes = max(maxRes, t);

		if(not isSkipped(r, c, isVert))
			--fwSteps;
		r += dr;
		c += dc;
	}

	// Mark back one step (e.g., down for T, right for |-)
	r = r0 - dr;
	c = c0 - dc;
	while(isWithinGrid(r, c))
	{
		int t;
		gridPoints[r][c].extendFlag |= val;
		if(isVert)
			gridV[t = r + min(dr, 0)][c].extend = true;
		else
			gridH[r][t = c + min(dc, 0)].extend = true;
		minRes = min(minRes, t);
		maxRes = max(maxRes, t);

		if(not isSkipped(r, c, isVert))
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
 *  4) whether the T-mesh is de Boor-suitable (DS)
 * The calling thread should lock the mutex before calling
 */
void TMesh::updateMeshInfo()
{
	validVertices = true;

	FOR(r,0,rows + 1) FOR(c,0,cols + 1)
	{
		int& valenceBits = gridPoints[r][c].valenceBits; // 0-3: directions UDLR
		int& valenceType = gridPoints[r][c].valenceType; // 0:don't draw, 2-4:valence
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
			{
				valenceType = 4;
				valenceBits = 0b1111;
			}
			else valenceType = 2;
		}
		else // boundaryCount == 2, corner vertices
		{
			valenceType = 4; // Always draw the corners
			valenceBits = 0b1111;
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
	if(not validVertices)
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
						not gridH[r][c-1].on)
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
						not gridV[r-1][c].on)
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
	if(not isAD)
		isAS = false;
	else
	{
		isAS = true; // may turn out to be false after checking

		// Compute vertical index vectors (full)
		knotsCols.resize(cols+1);
		FOR(c,0,cols+1)
		{
			VI& K {knotsCols[c]};
			K.clear();
			K.push_back(-1);
			FOR(r,0,rows+1)
			{
				int& v {gridPoints[r][c].vId};
				if(isSkipped(r, c, true)) v = -1;
				else
				{
					v = SZ(K);
					K.push_back(r);
				}
			}
			K.push_back(rows + 1);
		}

		// Compute horizontal index vectors (full)
		knotsRows.resize(rows+1);
		FOR(r,0,rows+1)
		{
			VI& K {knotsRows[r]};
			K.clear();
			K.push_back(-1);
			FOR(c,0,cols+1)
			{
				int& h {gridPoints[r][c].hId};
				if(isSkipped(r, c, false)) h = -1;
				else
				{
					h = SZ(K);
					K.push_back(c);
				}
			}
			K.push_back(cols + 1);
		}

		blendDir.assign(rows, VI(cols, DIR_BOTH));

		// Compute T-junction extensions (ignore boundary vertices)
		FOR(r,1,rows) FOR(c,1,cols)
		{
			// Only consider T-junctions
			if(gridPoints[r][c].valenceType != 3)
				continue;

			// range for marking blending direction
			int minRes {-1};
			int maxRes {-1};
			int isVert {-1};

			switch(gridPoints[r][c].valenceBits)
			{
			case 0b1110: // T
				markExtension(r, c, -1, 0, true, minRes, maxRes);
				isVert = 1;
				break;
			case 0b1101: // _|_
				markExtension(r, c, 1, 0, true, minRes, maxRes);
				isVert = 1;
				break;
			case 0b1011: // |-
				markExtension(r, c, 0, -1, false, minRes, maxRes);
				isVert = 0;
				break;
			case 0b0111: // -|
				markExtension(r, c, 0, 1, false, minRes, maxRes);
				isVert = 0;
				break;
			}

			if(isVert != -1)
			{
				if(isVert)
				{
					const int h {gridPoints[r][c].hId};
					assert(h != -1);
					int c_min {knotsRows[r][max(h-2, 1)]}; // 1 to exclude the left frame region
					int c_max {knotsRows[r][min(h+2, SZ(knotsRows[r])-2)]}; // -2 to exclude the right frame region
					FOR(r,minRes,maxRes+1) FOR(c,c_min,c_max)
					{
						// Mark unit element "cannot blend first by column"
						blendDir[r][c] &= ~DIR_COLUMN;
					}
				}
				else
				{
					const int v {gridPoints[r][c].vId};
					assert(v != -1);
					int r_min {knotsCols[c][max(v-2, 1)]}; // 1 to exclude the top frame region
					int r_max {knotsCols[c][min(v+2, SZ(knotsCols[c])-2)]}; // -2 to exclude the bottom frame region
					FOR(r,r_min,r_max) FOR(c,minRes,maxRes+1)
					{
						// Mark unit element "cannot blend first by row"
						blendDir[r][c] &= ~DIR_ROW;
					}
				}
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

	// Check whether the mesh is DS (must also be AS)
	if(not isAS)
		isDS = false;
	else
	{
		isDS = true; // may turn out to be false after checking

		FOR(r,0,rows) FOR(c,0,cols)
		{
			if(blendDir[r][c] == 0)
			{
				isDS = false;
				goto doneDS;
			}
		}

		doneDS:;
	}
}

void TMesh::getTiledFloorRange(const int r, const int c, int& r_min, int& r_max, int& c_min, int& c_max) const
{
	int r_cap {r};
	int c_cap {c};
	cap(r_cap, c_cap);

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
	FOR(r,-1,rows+2) FOR(c,-1,cols+2)
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

// As described in the paper
void TMesh::get16PointsFast(int ur, int uc, vector<pair<int,int>>& blendP, bool& row_n_4, bool& col_n_4) const
{
	using P2 = pair<int,int>;

	set<P2> found_anchors;
	map<int,int> rowQ[2][2], colQ[2][2]; // check for each quadrant of unit(P), at most 2 in each row/col

	auto hasVLine = [&](int r, int c)
	{
		if(c <= 0 or c >= cols) return true;
		r = max(0, min(rows, r));
		return (gridPoints[r][c].valenceBits & VALENCE_BITS_UPDOWN) != 0;
	};
	auto hasHLine = [&](int r, int c)
	{
		if(r <= 0 or r >= rows) return true;
		c = max(0, min(cols, c));
		return (gridPoints[r][c].valenceBits & VALENCE_BITS_LEFTRIGHT) != 0;
	};

/*  expanded rows and columns: not used right now

	// rows/columns that are not void of H-/V-lines
	VI good_rows(4), good_cols(4);
	{
		auto find_rows = [&](int r, int dr, int i, int di, int i_stop)
		{
			while(i != i_stop)
			{
				// use 4 default rows
				FOR(c,uc-1,uc+3) if(hasHLine(r, c))
				{
					good_rows[i] = r;
					i += di;
					break;
				}
				r += dr;
			}
		};
		auto find_columns = [&](int c, int dc, int i, int di, int i_stop)
		{
			while(i != i_stop)
			{
				// use 4 default rows
				FOR(r,ur-1,ur+3) if(hasVLine(r, c))
				{
					good_cols[i] = c;
					i += di;
					break;
				}
				c += dc;
			}
		};

		// Find the two nearest non-empty rows above unit(P)
		find_rows(ur, -1, 1, -1, -1);
		// Find the two nearest non-empty rows below unit(P)
		find_rows(ur+1, 1, 2, 1, 4);

		// Find the two nearest non-empty columns to the left of unit(P)
		find_columns(uc, -1, 1, -1, -1);
		// Find the two nearest non-empty columns to the right of unit(P)
		find_columns(uc+1, 1, 2, 1, 4);
	}
*/

#define debug_print(x) (x) // (0) for nothing, (x) for printing

	auto find_missing = [&](VI core_rows, VI core_cols)
	{
		assert(SZ(core_rows) == 4 and SZ(core_cols) == 4);

		auto checkQ = [&](const map<int,int>& Q, int x) -> bool
		{
			auto qi = Q.find(x);
			return qi == Q.end() or qi->_2 < 2; // have found only 0-1 anchor
		};
		auto vacant_quadrant_col = [&](int ar, int ac, int c) -> bool
		{
			return checkQ(colQ[ar >> 1][ac >> 1], c);
		};
		auto vacant_quadrant_row = [&](int ar, int ac, int r) -> bool
		{
			return checkQ(rowQ[ar >> 1][ac >> 1], r);
		};
		auto vacant_quadrant = [&](int ar, int ac, int r, int c) -> bool
		{
			return vacant_quadrant_col(ar, ac, c) and vacant_quadrant_row(ar, ac, r);
		};

		auto insert_anchor = [&](int ar, int ac, int r, int c)
		{
			assert(vacant_quadrant(ar, ac, r, c)); // this row and column must not be full (2 anchors already found)
			assert(found_anchors.find({r,c}) == found_anchors.end()); // not yet found
			const int qr {ar >> 1};
			const int qc {ac >> 1};
			++rowQ[qr][qc][r];
			++colQ[qr][qc][c];
			found_anchors.emplace(r, c);
		};

		enum anchors_result {GOOD_ANCHOR, OUTSIDE_ANCHOR, BAD_ANCHOR};

		// Check if the anchor is good and not yet found, and if so collect it
		auto check = [&](int ar, int ac, int r, int c) -> anchors_result
		{
			int r_cap {r};
			int c_cap {c};
			cap(r_cap, c_cap);

			// Ignore non-vertex
			if(not useVertex(r_cap, c_cap)) return BAD_ANCHOR;

			int r_min, r_max, c_min, c_max;
			getTiledFloorRange(r, c, r_min, r_max, c_min, c_max);

			if(r_min <= ur and ur < r_max and c_min <= uc and uc < c_max)
			{
				if(found_anchors.find({r,c}) == found_anchors.end() and vacant_quadrant(ar, ac, r, c))
				{
					insert_anchor(ar, ac, r, c);
					return GOOD_ANCHOR;
				}
				else return BAD_ANCHOR;
			}
			return OUTSIDE_ANCHOR;
		};

		// Walk through the mesh vertically/horizontally to find a good anchor or a point to switch direction
		// will only return true if do_check == true and a good anchor is found
		auto walk_vert = [&](int ar, int ac, int& r, int c, bool do_check) -> bool
		{
			int h_seen {0};
			int dr {(ar < 2) ? -1: 1}; // top 2 or bottom 2

			while(h_seen < 2) // will consider at at most two rows with H-lines
			{
				if(hasHLine(r, c))
				{
					++h_seen;
					if(vacant_quadrant_row(ar, ac, r)) // will visit this row only if not full
					{
						if(do_check) // check point now
						{
							auto res = check(ar, ac, r, c);
							if(res != BAD_ANCHOR) return res == GOOD_ANCHOR;
						}
						else break; // not checking now, but planning to switch direction
					}
				}
				r += dr;
			}
			return false;
		};
		auto walk_horz = [&](int ar, int ac, int r, int& c, bool do_check) -> bool
		{
			int v_seen {0};
			int dc {(ac < 2) ? -1: 1}; // top 2 or bottom 2

			while(v_seen < 2) // will consider at most two columns with V-lines
			{
				if(hasVLine(r, c))
				{
					++v_seen;
					if(vacant_quadrant_col(ar, ac, c)) // will visit this column only if not full
					{
						if(do_check) // check point now
						{
							auto res = check(ar, ac, r, c);
							if(res != BAD_ANCHOR) return res == GOOD_ANCHOR;
						}
						else break; // not checking now, but planning to switch direction
					}
				}
				c += dc;
			}
			return false;
		};

		/*
		 * Steps for finding all 16 anchors
		 *  1) pick all existing anchors within the default surrounding box of unit(P)
		 *  2) find all horizontal/vertical missing anchors
		 *  3) find the remaining ill-missing anchors
		 *    a) replace missing V-lines with solid lines, find a temporary vertex v (as in (2) above),
		 *       revert the V-lines back, and do the same for H-lines (2) starting at v.
		 *       Use the tiled floors to check if the anchor is the correct replacement.
		 *       Repeat but with H-lines first, then V-lines.
		 *
		 *  - we assume that the area unit(P) is de-Boor-suitable
		 *
		 *  - for each quadrant of unit(P), there must be 4 anchors, and at most 2 per row/column
		 *    (we will use this restriction to skip row/column that is full)
		 *
		 *  - depending on the allowed blending directions (determined by T-junction boxes),
		 *    we may be able to determine which four rows and/or columns the blending anchors
		 *    will reside in after we have collected some good anchors. For example:
		 *     + if both directions, then we only need to know which four rows and columns
		 *     + if row-first only, find which four rows only contain good anchors
		 *
		 *  - we can possibly terminate search
		 *
		 *  - aggressive search: treat each missing vertex as an ill-missing vertex
		 *    and keep trying for all directions even if a replacement for it has been found
		 */

		// Collect all non-missing vertices (Case #1)
		bool missing[4][4];
		FOR(ar,0,4) FOR(ac,0,4)
		{
			int rr {core_rows[ar]};
			int cc {core_cols[ac]};
			missing[ar][ac] = (check(ar, ac, rr, cc) != GOOD_ANCHOR);
		}

		// For each missing vertex, find (all of) its replacement(s), aggressively
		FOR(ar,0,4) FOR(ac,0,4) if(missing[ar][ac])
		{
			const int r {core_rows[ar]};
			const int c {core_cols[ac]};
			int r1, c1;

			// move vertically only (case #2)
			r1 = r;
			walk_vert(ar, ac, r1, c, true);

			// move horizontally only (case #3)
			c1 = c;
			walk_horz(ar, ac, r, c1, true);

			// move horizontally and then vertically (case #4a)
			r1 = r;
			c1 = c;
			walk_horz(ar, ac, r1, c1, false);
			walk_vert(ar, ac, r1, c1, true);
			// move vertically and then horizontally (case #4b)
			r1 = r;
			c1 = c;
			walk_vert(ar, ac, r1, c1, false);
			walk_horz(ar, ac, r1, c1, true);
		}
	};
#undef debug_print

	VI normal_rows {ur-1, ur, ur+1, ur+2};
	VI normal_cols {uc-1, uc, uc+1, uc+2};

	// Try many combinations of good and normal rows/columns (only the default for now)
//	find_missing(good_rows, good_cols);
//	find_missing(normal_rows, good_cols);
//	find_missing(good_rows, normal_cols);
	find_missing(normal_rows, normal_cols);

	map<int,int> rowCounts, colCounts;
	blendP.clear();
	blendP.reserve(16);
	for(auto& p: found_anchors)
	{
		blendP.emplace_back(p);
		++rowCounts[p._1];
		++colCounts[p._2];
	}

	assert(SZ(blendP) == 16);
	row_n_4 = SZ(rowCounts) == 4;
	col_n_4 = SZ(colCounts) == 4;
}


void TMesh::test1(int ur, int uc,
	vector<pair<int,int>>& blendP, vector<pair<int,int>>& blendP2,
	vector<pair<int,int>>& missing, vector<pair<int,int>>& extra,
	bool& row_n_4, bool& col_n_4) const
{
	bool t;
	get16Points(ur, uc, blendP, row_n_4, col_n_4);
	get16PointsFast(ur, uc, blendP2, t, t);
	sort(begin(blendP), end(blendP));
	sort(begin(blendP2), end(blendP2));
	set_difference(begin(blendP), end(blendP), begin(blendP2), end(blendP2), back_inserter(missing));
	set_difference(begin(blendP2), end(blendP2), begin(blendP2), end(blendP2), back_inserter(extra));
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
	node.knotR = (idR >= SZ(knots)) ? 0 : knots[idR];
}

void TriMeshScene::setScene(const TMesh* T)
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
	else // B-spline
	{
		cout << "A" << endl;
		VVP3 S;
		const int RN {2};
		const int CN {2};

		S.assign(RN+1, VP3(CN+1));

		const double s0 {T->knotsV[2]};
		const double s1 {T->knotsV[T->rows]};
		const double t0 {T->knotsH[2]};
		const double t1 {T->knotsH[T->cols]};

		auto populateKnotsH5 = [&](vector<double>& K, int p_r, int p_c)
		{
			K.clear();
			K.reserve(5);
			p_r = max(0, min(T->rows, p_r));
			const int h {(p_c < 0) ? 0 : (p_c > T->cols) ? SZ(T->knotsRows[p_r]) - 1 : T->gridPoints[p_r][p_c].hId};
			FOR(dh,-2,3)
			{
				const int hh {max(0, min(SZ(T->knotsRows[p_r]) - 1, h + dh))};
//				cout << "hh " << hh << "  dh " << dh << "  h " << h << "   - " << SZ(T->knotsRows[p_r]) << endl;
				K.emplace_back(T->knotsH[T->knotsRows[p_r][hh] + 1]);
			}
		};

		auto populateKnotsV5 = [&](vector<double>& K, int p_r, int p_c, bool run = false)
		{
			K.clear();
			K.reserve(5);
			p_c = max(0, min(T->cols, p_c));
			const int v {(p_r < 0) ? 0 : (p_r > T->rows) ? SZ(T->knotsCols[p_c]) - 1 : T->gridPoints[p_r][p_c].vId};
			FOR(dv,-2,3)
			{
				const int vv {max(0, min(SZ(T->knotsCols[p_c]) - 1, v + dv))};
				if(run) cout << "   " << vv << "_" << T->knotsCols[p_c][vv];
				K.emplace_back(T->knotsV[T->knotsCols[p_c][vv] + 1]);
			}
			if(run) cout << endl;
		};

		auto B_s = [&](double s, vector<double>& knots)
		{
			assert(SZ(knots) == 5);
			static double dp[4][4];
			FOR(j,0,4) dp[0][j] = (knots[j] <= s and s < knots[j+1]) ? 1.0 : 0.0;
			FOR(i,1,4) FOR(j,0,4-i)
			{
				dp[i][j] = 0;
				auto a {dp[i-1][j] * (s - knots[j])};
				if(abs(a) > 1e-20) dp[i][j] += a / (knots[j + i] - knots[j]);
				auto b {dp[i-1][j+1] * (knots[j + i + 1] - s)};
				if(abs(b) > 1e-20) dp[i][j] += b / (knots[j + i + 1] - knots[j + 1]);
			}
			return dp[3][0];
		};
		//cout << "B" << endl;

		vector<double> X, Y(11);
		FOR(i,-1,5) if(i != 1 and i != 2)
		{
			populateKnotsH5(X, 2, i);
			for(auto a: X) cout << a << ' ';
			cout << endl;
			cout << fixed << setprecision(5);
			FOR(j,0,11) cout << j/10.0 << " : " << B_s(j/10.0, X) << endl;
			FOR(j,0,11) Y[j] += B_s(j/10.0, X);
		}
		FOR(i,0,11) cout << Y[i] << endl;

		Y.assign(11,0);
		for(auto a: T->knotsV)
		{
			cout << a << ' ';
		}
		cout << endl;
		for(auto a: T->knotsCols[0])
		{
			cout << a << " - " << T->knotsV[a+1] <<  "      ";
		}
		cout << endl;

		cout << "\n\n VVV \n";
		FOR(i,-1,6) if(i != 1 and i != 3)
		{
			cout << i << " ---\n";
			populateKnotsV5(X, i, 0, true);
			for(auto a: X) cout << a << ' ';
			cout << endl;
			cout << fixed << setprecision(5);
			FOR(j,0,11) cout << j/5.0 << " : " << B_s(j/5.0, X) << endl;
			FOR(j,0,11) Y[j] += B_s(j/5.0, X);
			cout << endl;
		}
		FOR(i,0,11) cout << Y[i] << endl;
		cout << endl;
		//exit(0);

		//return;

		FOR(ri,0,RN+1) FOR(ci,0,CN+1)
		{
			const double s {s0 + ri * (s1-s0) / RN};
			const double t {t0 + ci * (t1-t0) / CN};

			Pt3& res {S[ri][ci]};
			res = Pt3(0,0,0,0);

			FOR(ar,-1,T->rows+2) FOR(ac,-1,T->cols+2)
			{
				int ar_cap {ar};
				int ac_cap {ac};
				T->cap(ar_cap, ac_cap);
				if(not T->useVertex(ar_cap, ac_cap)) continue;

				const auto& P = T->gridPoints[ar_cap][ac_cap];

				vector<double> knotsV, knotsH;
				populateKnotsH5(knotsH, ar, ac);
				populateKnotsV5(knotsV, ar, ac);
				double x {B_s(s, knotsV)};
				double y {B_s(t, knotsH)};
				double c {x * y};
				res += P.position * c;

				if(false and ri == 0 and ci == 1)
				{
					cout << "s " << s << "  t " << t << endl;
					cout << "a " << ar << ' ' << ac << endl;
					cout << P.position << "      c " << c << endl;
					cout << x << ' ' << y << endl;
					cout << "knots H ";
					for(auto a: knotsH) cout << ' ' << a;
					cout << endl;
					cout << "knots V ";
					for(auto a: knotsV) cout << ' ' << a;
					cout << endl << endl;
				}
			}

			if(abs(res[3] - 1) > 1e-6) cout << "too much error  " << ri << ' ' << ci << endl;
		}
		cout << "C" << endl;

		cout << SZ(S) << endl;

		//FOR(i,0,RN+1) FOR(j,0,CN+1) cout << i << "  " << j << "  :  " << S[i][j] << endl;

		setMesh(S);
	}

	if(false) // de Boor
	{
		vector<VVP3> Ss;
		VVP3 S;
		FOR(ur,1,T->rows-1) FOR(uc,1,T->cols-1)
		{
			// Skip dead areas
			if(T->blendDir[ur][uc] == DIR_NEITHER) continue;

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

			if(true)
			{
				//T->get16Points(ur, uc, blendP, row_n_4, col_n_4);
				T->get16PointsFast(ur, uc, blendP, row_n_4, col_n_4);
			}
			else // Testing...
			{
				vector<pair<int,int>> blendP2, missing, extra;
				T->test1(ur, uc, blendP, blendP2, missing, extra, row_n_4, col_n_4);
				if(blendP != blendP2)
				{
					cout << "\n\nBAD\n";
					cout << "missing : ";
					for(auto p: missing) cout << p._1 << ' ' << p._2 << "    ";
					cout << endl;
					cout << "extra   : ";
					for(auto p: extra) cout << p._1 << ' ' << p._2 << "    ";
					cout << endl;
				}
			}

			const int RN {20};
			const int CN {20};
			const double ds {(s1 - s0) / RN};
			const double dt {(t1 - t0) / CN};

			auto populateKnotsH = [&](vector<double>& K, pair<int,int> p_r_c1)
			{
				K.clear();
				K.reserve(6);
				int p_r, p_c;
				tie(p_r, p_c) = p_r_c1;
				const int h {T->gridPoints[p_r][p_c].hId};
				FOR(dh,-2,4)
				{
					const int hh {max(0, min(SZ(T->knotsRows[p_r]) - 1, h + dh))};
					K.emplace_back(T->knotsH[T->knotsRows[p_r][hh] + 1]);
				}
			};

			auto populateKnotsV = [&](vector<double>& K, pair<int,int> p_r1_c)
			{
				K.clear();
				K.reserve(6);
				int p_r, p_c;
				tie(p_r, p_c) = p_r1_c;
				const int v {T->gridPoints[p_r][p_c].vId};
				FOR(dv,-2,4)
				{
					const int vv {max(0, min(SZ(T->knotsCols[p_c]) - 1, v + dv))};
					K.emplace_back(T->knotsV[T->knotsCols[p_c][vv] + 1]);
				}
			};

			if(row_n_4) // can process row-then-column
			{
				// blendP: row-major by default

				// Restrict the vertices to within the active region
				for(auto& p: blendP) T->cap(p._1, p._2);

				// Horizontal knot vectors, one per row
				vector<double> kH[4];
				FOR(r,0,4) populateKnotsH(kH[r], blendP[r * 4 + 1]); // P[0..3][1]

				// Vertical knot vector
				vector<double> kV;
				populateKnotsV(kV, blendP[5]); // P[1][1]

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
				sort(begin(blendP), end(blendP), [&](const auto& p, const auto& q)
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
				FOR(c,0,4) populateKnotsV(kV[c], blendP[4 + c]); // P[1][0..3]

				// Horizontal knot vector
				vector<double> kH;
				populateKnotsH(kH, blendP[5]); // P[1][1]

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

			if(ready) Ss.emplace_back(move(S));
		}

		setMesh2(Ss);
	}
}
