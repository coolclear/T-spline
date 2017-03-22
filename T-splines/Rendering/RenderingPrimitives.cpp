#include "Rendering/RenderingPrimitives.h"

using namespace std;

Mat4 SceneInfo::modelview0;
Mat4 SceneInfo::rotate0;
Mat4 SceneInfo::translate0;

// Newell's method
inline Vec3 triFaceNormal(const Pt3 &a, const Pt3 &b, const Pt3 &c, bool doNorm = true)
{
	Vec3 res = cross(b-a, c-a) + cross(c-b, a-b) + cross(a-c, b-c);
	if(doNorm) res.normalize();
	return res;
}

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


// Initialize modelview matrices
void SceneInfo::initScene()
{
	const double A[19] = {
		0.909375070178, -0.0740295117531, 0.409336197201, 0,
		-0.0373269545854, 0.965544756015, 0.257545774263, 0,
		-0.414298441997, -0.249485712232, 0.875279623743, 0,
		0, 0, 0, 1,
		-2.1925264022, -1.6757205209, -3.18389057922};
	int ai = 0;
	FOR(j,0,4) FOR(k,0,4) rotate0[j][k] = A[ai++];
	FOR(j,0,3) translate0[3][j] = A[ai++];
	updateModelView();
}