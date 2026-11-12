// H6 trace harness: cMeshCreator over the recording low-level graphics. stdout is the GPU call trace
// plus mesh/sub-mesh lookups; OHPL_IMPL=cpp and =rust must print the same text.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "graphics/Mesh.h"
#include "graphics/MeshCreator.h"
#include "graphics/SubMesh.h"
#include "graphics/VertexBuffer.h"
#include "resources/MaterialManager.h"
#include "resources/Resources.h"
#include "trace/RecordingLowLevelGraphics.h"

using namespace hpl;

int hplMain(const std::string&) { return 0; }

namespace hpl {
	cMaterial* cMaterialManager::CreateMaterial(const tString& asName)
	{
		printf("material %s\n", asName.c_str());
		return NULL;
	}
}

static void Show(const char* what, cMesh* m)
{
	printf("%s %s\n", what, m ? "mesh" : "null");
	if(!m) return;
	printf("  subs %d tris %d main %d %d\n", m->GetSubMeshNum(), m->GetTriangleCount(), m->GetSubMeshIndex("Main"), m->GetSubMeshName("Main") == m->GetSubMesh(0));
	cSubMesh* s = m->GetSubMesh(0);
	printf("  matname '%s' vb %d\n", s->GetMaterialName().c_str(), s->GetVertexBuffer() != NULL);
	delete m;
}

int main()
{
	cRecordingLowLevelGraphics llg;
	cResources* res = (cResources*)calloc(1, 65536);
	cMeshCreator mc(&llg, res);
	Show("box", mc.CreateBox("box", cVector3f(1, 2, 3), "mat_box"));
	Show("sphere", mc.CreateSphere("sphere", 1.5f, 12, 8, "mat_sphere"));
	Show("cyl", mc.CreateCylinder("cyl", cVector2f(0.5f, 2), 10, "mat_cyl"));
	Show("capsule", mc.CreateCapsule("capsule", cVector2f(0.5f, 2), 10, 6, "mat_cap"));
	Show("cone", mc.CreateCone("cone", cVector2f(1, 2), 9, "mat_cone"));
	Show("plane", mc.CreatePlane("plane", cVector3f(0, 0, 0), cVector3f(2, 0, 3), cVector2f(0, 0), cVector2f(1, 0), cVector2f(1, 1), cVector2f(0, 1), "mat_plane"));
	Show("plane_bad", mc.CreatePlane("plane_bad", cVector3f(0, 0, 0), cVector3f(2, 1, 3), cVector2f(0, 0), cVector2f(1, 0), cVector2f(1, 1), cVector2f(0, 1), "mat_plane"));
	iVertexBuffer* sky = mc.CreateSkyBoxVertexBuffer(10);
	printf("sky %d\n", sky != NULL);
	iVertexBuffer* box = mc.CreateBoxVertexBuffer(cVector3f(2, 2, 2));
	printf("boxvb %d\n", box != NULL);
	if(box)
	{
		iVertexBuffer* wf = mc.CreateWireframeVertexBuffer(box);
		printf("wire %d\n", wf != NULL);
	}
	fputs(llg.Trace().Text().c_str(), stdout);
	return 0;
}
