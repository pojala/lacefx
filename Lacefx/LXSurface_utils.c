
#include "LXSurface.h"
#include "LXBasicTypes.h"


void LXSetQuadVerticesXYUV(LXVertexXYUV *vertices, LXRect rect, LXRect uvRect)
{
    if ( !vertices) return;
    vertices[0].x = rect.x;
    vertices[0].y = rect.y;

    vertices[1].x = rect.x + rect.w;
    vertices[1].y = rect.y;

    vertices[2].x = rect.x + rect.w;
    vertices[2].y = rect.y + rect.h;

    vertices[3].x = rect.x;
    vertices[3].y = rect.y + rect.h;


    vertices[0].u = uvRect.x;
    vertices[0].v = uvRect.y;

    vertices[1].u = uvRect.x + uvRect.w;
    vertices[1].v = uvRect.y;

    vertices[2].u = uvRect.x + uvRect.w;
    vertices[2].v = uvRect.y + uvRect.h;

    vertices[3].u = uvRect.x;
    vertices[3].v = uvRect.y + uvRect.h;
}

