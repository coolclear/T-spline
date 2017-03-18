#include "TriMeshScene.h"

static Material* readMaterial() {
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

static Light* readLight(int id) {
	Pt3 pos;
	Color c;

	const Pt3 P[6] = {Pt3(100,0,0),Pt3(0,100,0),Pt3(0,0,100),Pt3(-100,0,0),Pt3(0,-100,0),Pt3(0,0,-100)};
	//const Pt3 P[6] = {Pt3(10,0,0),Pt3(0,10,0),Pt3(0,0,10),Pt3(-10,0,0),Pt3(0,-10,0),Pt3(0,0,-10)};
	//const Pt3 P[6] = {Pt3(2,0,0),Pt3(0,2,0),Pt3(0,0,2),Pt3(-2,0,0),Pt3(0,-2,0),Pt3(0,0,-2)};

	// RGB
	const Color C[6] = {Color(0.5,0,0),Color(0,0.5,0),Color(0,0,0.5),Color(0.5,0,0),Color(0,0.5,0),Color(0,0,0.5)};
	// BRG
	//const Color C[6] = {Color(0,0,0.5),Color(0.5,0,0),Color(0,0.5,0),Color(0,0,0.5),Color(0.5,0,0),Color(0,0.5,0)};

	pos = P[id];
	c = C[id];
	Light* ret = new Light();
	ret->setColor(c);
	ret->setPos(pos);
	return ret;
}

static TriMesh* readMesh(const VVP3 &S) {
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

	cout << "#Vertices  : " << nverts << endl;
	cout << "#Triangles : " << ntris << endl;

	return ret;
}


static TriMeshScene* newTriMeshScene()
{
	TriMeshScene* ret = new TriMeshScene();

	ret->setMaterial(readMaterial());
	Color amb(0.1,0.1,0.1,1);

	int nlights = 6;
	for(int j = 0; j < nlights; j++) {
		Light* l = readLight(j);
		l->setAmbient(amb);
		ret->addLight(l);
	}

	return ret;
}

TriMeshScene* createTriMeshScene(const VVP3 &S)
{
	TriMeshScene* ret = newTriMeshScene();
	ret->setMesh(readMesh(S));
	return ret;
}

TriMeshScene* createCurveScene(const vector<Pt3> &P)
{
	TriMeshScene* ret = newTriMeshScene();
	ret->setCurve(P);
	return ret;
}