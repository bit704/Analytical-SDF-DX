#ifndef GAMEAPP_H
#define GAMEAPP_H

#include "d3dApp.h"

class SDFRenderer : public D3DApp
{
public:
    SDFRenderer(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight);
    ~SDFRenderer();

    bool Init();
    void OnResize();
    void UpdateScene(float dt);
    void DrawScene();

    
private:
    bool InitPipeline();



private:
    ComPtr<ID3D11InputLayout> m_pVertexLayout;	// 顶点输入布局
    ComPtr<ID3D11Buffer> m_pVertexBuffer;		// 顶点缓冲区
    ComPtr<ID3D11Buffer> m_pIndexBuffer;	    // 索引缓冲区

    struct ConstantBuffer
    {
        DirectX::XMFLOAT4 time_1;
        DirectX::XMFLOAT4 resolution_2;
        DirectX::XMINT4 frame_1;
        DirectX::XMMATRIX camera;
        DirectX::XMINT4 switch_4;
        DirectX::XMFLOAT4 factor_4;
    };
    ConstantBuffer m_CBuffer;                       // 用于修改GPU常量缓冲区的变量
    ComPtr<ID3D11Buffer> m_pConstantBuffer;         // 常量缓冲区

    int curr_PS = 2;   //PS编号
    //以下传入着色器
    bool s1, s2, s3, s4; //效果开关
    bool animation; //时间开关
    float time = 0.f;  //计时
    int frame = 0; //帧数
    int mnum = 256; //光线行进步数
    float ss = 20; //软阴影因子


    ComPtr<ID3D11VertexShader> m_pVertexShader;	// 顶点着色器

    // 像素着色器
    ComPtr<ID3D11PixelShader> m_pPixelShader0;	
    ComPtr<ID3D11PixelShader> m_pPixelShader1;
    ComPtr<ID3D11PixelShader> m_pPixelShader2;

    const static int m_VertexCount = 4;
    const static int m_IndexCount = 6;

    ComPtr<ID3D11ShaderResourceView> m_pTexture;			    // 纹理
    ComPtr<ID3D11SamplerState> m_pSamplerState;				// 采样器状态

    
};


#endif