#include "MeshRenderer.h"

void MeshRenderer::draw()
{
	if(_scene)
	{
		Material* mat = _scene->getMaterial();

		Color amb = mat->getAmbient();
		Color spec = mat->getSpecular();
		Color diffuse = mat->getDiffuse();
		float ambv[4] = {(float)amb[0], (float)amb[1], (float)amb[2], 1.f};
		float specv[4] = {(float)spec[0], (float)spec[1], (float)spec[2], 1.f};
		float diffv[4] = {(float)diffuse[0], (float)diffuse[1], (float)diffuse[2], 1.f};
		float sxp = (float) mat->getSpecExponent();

		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffv);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specv);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambv);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, sxp);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_CULL_FACE);

		// Draw the interpolated curve
		if(_scene->willDrawCurve())
		{
			vector<pair<Pt3, int>> &P = _scene->getCurve();
			glShadeModel(GL_FLAT);
			glColor4d(1,1,1,1);
			glDisable(GL_LIGHTING);
			glDisable(GL_COLOR_MATERIAL);

			for(int j = 0; j < 2; ++j)
			{
				if(j == 0) // First draw lines
				{
					glLineWidth(2);
					glBegin(GL_LINE_STRIP);
				}
				else // Then draw points
				{
					glPointSize(2);
					glBegin(GL_POINTS);
				}

				for(int i = 0; i < (int) P.size(); ++i)
				{
					int a = P[i].second % 3;
					glColor3d(a == 0, a == 1, a == 2);
					glVertex3dv(&P[i].first[0]);
				}
				glEnd();
			}
		}
		else // Draw the interpolated surface
		{
			const TriMesh* m = _scene->getMesh();
			Pt3Array* pts = m->getPoints();
			const TriIndArray* inds = m->getInds();
			Vec3Array* vnorms = m->getVNormals();
			Vec3Array* fnorms = m->getFNormals();

			auto normColor = [&](const Pt3 &norm)
			{
				double color[3];

				// Scheme 1
				/*
				double sum = 0;
				FOR(i,0,3)
				{
					double a = abs(norm[i]);
					sum += color[i] = a;
				}
				FOR(i,0,3) color[i] /= sum;
				//*/

				// Scheme 2
				FOR(i,0,3) color[i] = ((norm[i] + 1) * 0.5) * (0.5 * abs(norm[i]) + 0.5);

				glColor3dv(color);
			};

			// Use normals, no lighting
			if(useNormal)
			{
				glDisable(GL_LIGHTING);
				glDisable(GL_COLOR_MATERIAL);
			}
			else // Use lighting and material
			{
				glEnable(GL_LIGHTING);
				glEnable(GL_COLOR_MATERIAL);
				glColor3d(1, 1, 1);
			}

			if(getShadingModel() == SHADE_GOURAUD)
			{
				glShadeModel(GL_SMOOTH);
				glBegin(GL_TRIANGLES);
				FOR(j,0,inds->size())
				{
					const TriInd& ti = inds->get(j);
					const Vec3 &fn = fnorms->get(j);
					FOR(k,0,3)
					{
						const Vec3 vn = vnorms->get(ti[k]);
						/*
						 * Don't use the vertex normal if it's too different
						 * from the face normal.
						 */
						const double threshold = 0.7;
						Vec3 norm = ((vn * fn) > threshold) ? vn : fn;
						if(useNormal)
							normColor(norm);
						else
							glNormal3dv(&norm[0]);

						glVertex3dv(&pts->get(ti[k])[0]);
					}
				}
				glEnd();
			}
			else
			{
				glShadeModel(GL_FLAT);
				glBegin(GL_TRIANGLES);
				FOR(j,0,inds->size())
				{
					Vec3 fn = fnorms->get(j);
					if(useNormal)
						normColor(fn);
					else
						glNormal3dv(&fn[0]);

					const TriInd& ti = inds->get(j);
					FOR(k,0,3) glVertex3dv(&pts->get(ti[k])[0]);
				}
				glEnd();
			}

			// Wireframes
			if(drawWire)
			{
				glDisable(GL_LIGHTING);
				glDisable(GL_COLOR_MATERIAL);
				glDisable(GL_CULL_FACE);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				glLineWidth(1);
				glColor3d(0.1, 0.1, 0.1);
				glBegin(GL_TRIANGLES);
				FOR(j,0,inds->size())
				{
					const TriInd& ti = inds->get(j);
					FOR(k,0,3) glVertex3dv(&pts->get(ti[k])[0]);
				}
				glEnd();
			}
		}
	}
}

void MeshRenderer::initLights()
{
	// 8 is the max # of light sources
	int nlights = min<int>(8, _scene->getNumLights());
	if(nlights > 0) {
		Color amb = _scene->getLight(0)->getAmbient();
		float ambv[4] = {(float)amb[0], (float)amb[1], (float)amb[2], 1.f};

		glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambv);
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

		for(int j = 0; j < 8; j++) {
			int lid = GL_LIGHT0 + j;
			glDisable(lid);
		}

		for(int j = 0; j < nlights; j++) {
			Light* l = _scene->getLight(j);

			int lid = GL_LIGHT0 + j;
			glEnable(lid);

			Color c = l->getColor();
			Pt3 p = l->getPos();
			float cv[4] = {(float)c[0], (float)c[1], (float)c[2], 1.f};
			float pv[4] = {(float)p[0], (float)p[1], (float)p[2], 1.f};

			glLightfv(lid, GL_DIFFUSE, cv);
			glLightfv(lid, GL_SPECULAR, cv);
			glLightfv(lid, GL_POSITION, pv);
		}
	}
}