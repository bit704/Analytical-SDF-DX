#ifndef HARDCODE_H
#define HARDCODE_H

#include "d3dapp.h"
using namespace DirectX;

class Plane
{
public:
    struct VertexPosTex
    {
        XMFLOAT3 pos;
        XMFLOAT2 tex;
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
    };

    //创建顶点
    static void CreateVertices(VertexPosTex** vertices, UINT m_VertexCount)
    {
        *vertices = new VertexPosTex[m_VertexCount]
        {
            //两个三角形组成一个平面
            //保证纹理坐标与shadertoy一致
            { XMFLOAT3(-1.f,  1.f, 0.f), XMFLOAT2(0.f, 1.f) },
            { XMFLOAT3( 1.f,  1.f, 0.f), XMFLOAT2(1.f, 1.f) },
            { XMFLOAT3(-1.f, -1.f, 0.f), XMFLOAT2(0.f, 0.f) },
            { XMFLOAT3( 1.f, -1.f, 0.f), XMFLOAT2(1.f, 0.f) },

        };
    }

    static void CreateIndices(DWORD** indices, UINT m_IndexCount)
    {
        *indices = new DWORD[m_IndexCount]
        {
            0, 1, 3,
            0, 3, 2,
        };
    }

};


#endif