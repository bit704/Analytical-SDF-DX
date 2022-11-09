#include "SDFRenderer.h"
#include "d3dUtil.h"
#include "DXTrace.h"
#include "HardCode.h"
#include "DDSTextureLoader11.h"
#include "WICTextureLoader11.h"
#include <iostream>
#include <fstream>

using namespace DirectX;

const D3D11_INPUT_ELEMENT_DESC Plane::VertexPosTex::inputLayout[2] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};


SDFRenderer::SDFRenderer(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight)
    : D3DApp(hInstance, windowName, initWidth, initHeight),
    m_CBuffer()
{
}

SDFRenderer::~SDFRenderer()
{
}

bool SDFRenderer::Init()
{
    if (!D3DApp::Init())
        return false;

    if (!InitPipeline())
        return false;

    return true;
}

void SDFRenderer::OnResize()
{
    D3DApp::OnResize();
}

void SDFRenderer::UpdateScene(float dt)
{
    //更新frame
    frame += 1;
    m_CBuffer.frame_1 = XMINT4(frame,0,0,0);

    //计时
    if (animation)
    {
        time += dt;
        m_CBuffer.time_1 = XMFLOAT4(time, 0.f, 0.f, 0.f);
    }


    //调试
    //std::ofstream ofs;
    //ofs.open("test.txt", std::ios::out);
    //ofs << frame << ","  << std::endl;
    //ofs.close();
    
    
    if (ImGui::Begin("ImGui"))
    {

   //     const char* PS_strs[] = {
   //         "Simple",
   //         "My work",
   //     };
   //     if (ImGui::Combo("Choose PS", &curr_PS, PS_strs, ARRAYSIZE(PS_strs)))
   //     {
			//switch (curr_PS)
			//{
			//case 0: m_pd3dImmediateContext->PSSetShader(m_pPixelShader0.Get(), nullptr, 0);
   //             animation = true;
			//	break;
			//case 1: m_pd3dImmediateContext->PSSetShader(m_pPixelShader1.Get(), nullptr, 0);
   //             animation = true;
			//	break;
   //         default:
   //             break;
			//}
   //     }
        ImGui::Checkbox("Animation", &animation);
        if (curr_PS == 1)
        {
            ImGui::Checkbox("Base Color", &s1);
            ImGui::Checkbox("Direct Light", &s2);
            ImGui::Checkbox("Sky Light", &s3);
            ImGui::Checkbox("Ambient Occlusion", &s4);
            m_CBuffer.switch_4 = XMINT4(s1, s2, s3, s4);
            ImGui::SliderInt("Max March Num", &mnum, 0, 256);
            ImGui::SliderFloat("Softshadow Coef", &ss, 1, 100);
            m_CBuffer.factor_4 = XMFLOAT4((float)mnum, ss, 0, 0);
        }
    }

    ImGui::End();
    ImGui::Render();


    // 获取IO事件
    ImGuiIO& io = ImGui::GetIO();


    //WASD 平面位移
    static float dWS = 0.f, dAD = 0.f;
    //ZX 上下移动
    static float dZX = 0.f;
    //QE相机滚筒旋转
    static float rollQE = 0.f;
    //鼠标左键 第三人称视点方向
    static float flyDirXT = 0.f, flyDirYT = 0.f;
    //鼠标右键 第一人称视点方向
    static float flyDirXF = 0.f, flyDirYF = 0.f;

    // 不允许在操作UI时漫游
    // IO逻辑
    if (!ImGui::IsAnyItemActive())
    {
        //WASD 平面移动
        if (ImGui::IsKeyDown(ImGuiKey_W))
            dWS += dt * 2;
        if (ImGui::IsKeyDown(ImGuiKey_S))
            dWS -= dt * 2;
        if (ImGui::IsKeyDown(ImGuiKey_A))
            dAD += dt * 2;
        if (ImGui::IsKeyDown(ImGuiKey_D))
            dAD -= dt * 2;
        //ZX 上下移动
        if (ImGui::IsKeyDown(ImGuiKey_Z))
            dZX += dt * 2;
        if (ImGui::IsKeyDown(ImGuiKey_X))
            dZX -= dt * 2;
        //QE滚筒旋转
        if (ImGui::IsKeyDown(ImGuiKey_Q))
            rollQE += dt * 2;
        if (ImGui::IsKeyDown(ImGuiKey_E))
            rollQE -= dt * 2;
        rollQE = XMScalarModAngle(rollQE);
        //按下鼠标左键调整第三人称视点方向
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            flyDirXT += io.MouseDelta.y * 0.0005f;
            flyDirYT += io.MouseDelta.x * 0.0005f;
            flyDirXT = XMScalarModAngle(flyDirXT);
            flyDirYT = XMScalarModAngle(flyDirYT);
        }
        //按下鼠标右键调整第一人称视点方向
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            flyDirXF += io.MouseDelta.y * 0.0005f;
            flyDirYF += io.MouseDelta.x * 0.0005f;
            flyDirXF = XMScalarModAngle(flyDirXF);
            flyDirYF = XMScalarModAngle(flyDirYF);
        }
        
    }

    m_CBuffer.camera = 
        //WS前后移动
       XMMatrixLookAtLH(
       XMVectorSet(0.f, 4.f, 5.0f, 0.f), // 相机位置 ray-origin   世界坐标系下 x轴向右，Z轴向屏幕外，Y轴向上
	   XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), // 目标位置 look-at-target  世界坐标系的原点
	   XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) *  //up vector
	   XMMatrixRotationX(flyDirXF) *
	   XMMatrixRotationY(flyDirYF);  //鼠标调整第一人称视角方向
	;

    //求逆，因为着色器里用的是相机到世界变换矩阵
    m_CBuffer.camera = XMMatrixInverse(nullptr, m_CBuffer.camera);

    m_CBuffer.camera *= XMMatrixTranslation(dAD, dZX, -dWS) * //WS前后移动  AD左右移动
        XMMatrixRotationZ(-rollQE) * //QE滚筒旋转
        XMMatrixRotationX(-flyDirXT) *
        XMMatrixRotationY(flyDirYT);  //鼠标调整第三人称视角方向

    //调试
    //XMFLOAT4X4 test;
    //XMStoreFloat4x4(&test, m_CBuffer.camera);
    //std::ofstream ofs;
    //ofs.open("test.txt", std::ios::out);
    //ofs << test._11 << "," << test._12 << "," << test._13 << "," << test._14 << std::endl;
    //ofs << test._21 << "," << test._22 << "," << test._23 << "," << test._24 << std::endl;
    //ofs << test._31 << "," << test._32 << "," << test._33 << "," << test._34 << std::endl;
    //ofs << test._41 << "," << test._42 << "," << test._43 << "," << test._44 << std::endl;
    //ofs.close();
    //exit(0);


    // 更新常量缓冲区
    D3D11_MAPPED_SUBRESOURCE mappedData;
    HR(m_pd3dImmediateContext->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
    memcpy_s(mappedData.pData, sizeof(m_CBuffer), &m_CBuffer, sizeof(m_CBuffer));
    m_pd3dImmediateContext->Unmap(m_pConstantBuffer.Get(), 0);

}

void SDFRenderer::DrawScene()
{
    assert(m_pd3dImmediateContext);
    assert(m_pSwapChain);

    static float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };	// RGBA = (0,0,0,255)
    m_pd3dImmediateContext->ClearRenderTargetView(m_pRenderTargetView.Get(), black);
    m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 绘制平面
    m_pd3dImmediateContext->DrawIndexed(m_IndexCount, 0, 0);

    // 下面这句话会触发ImGui在Direct3D的绘制
    // 因此需要在此之前将后备缓冲区绑定到渲染管线上
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HR(m_pSwapChain->Present(0, 0));
}


bool SDFRenderer::InitPipeline()
{
    s1 = s2 = s3 = s4 = animation = true;
    curr_PS = 1;

    ComPtr<ID3DBlob> blob;
    
    //****************
    //设置顶点缓冲区和索引缓冲区 

    // 设置顶点
    Plane::VertexPosTex* vertices = nullptr;
    Plane::CreateVertices(&vertices, m_VertexCount);

    // 设置顶点缓冲区描述
    D3D11_BUFFER_DESC vbd;
    ZeroMemory(&vbd, sizeof(vbd));
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.ByteWidth = m_VertexCount * sizeof Plane::VertexPosTex;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;
    // 新建顶点缓冲区
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices;
    HR(m_pd3dDevice->CreateBuffer(&vbd, &InitData, m_pVertexBuffer.GetAddressOf()));
    // 输入装配阶段的顶点缓冲区设置
    UINT stride = sizeof(Plane::VertexPosTex);	// 跨越字节数
    UINT offset = 0;						// 起始偏移量

    m_pd3dImmediateContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);

    // 创建Plane索引
    DWORD* indices = nullptr;
    Plane::CreateIndices(&indices, m_IndexCount);

    // 设置索引缓冲区描述
    D3D11_BUFFER_DESC ibd;
    ZeroMemory(&ibd, sizeof(ibd));
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.ByteWidth = m_IndexCount * sizeof(DWORD);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    // 新建索引缓冲区
    InitData.pSysMem = indices;
    HR(m_pd3dDevice->CreateBuffer(&ibd, &InitData, m_pIndexBuffer.GetAddressOf()));
    // 输入装配阶段的索引缓冲区设置
    m_pd3dImmediateContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    // 设置调试对象名
    D3D11SetDebugObjectName(m_pVertexBuffer.Get(), "VertexBuffer");
    D3D11SetDebugObjectName(m_pIndexBuffer.Get(), "IndexBuffer");

    //释放堆内存
    delete[] vertices;
    delete[] indices;



    // ******************
    // 设置常量缓冲区描述
    //
    D3D11_BUFFER_DESC cbd;
    ZeroMemory(&cbd, sizeof(cbd));
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    // 新建用于VS和PS的常量缓冲区
    cbd.ByteWidth = sizeof(ConstantBuffer);
    HR(m_pd3dDevice->CreateBuffer(&cbd, nullptr, m_pConstantBuffer.GetAddressOf()));

    //初始化常量缓冲区
    m_CBuffer.time_1 = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
    m_CBuffer.resolution_2 = XMFLOAT4(1280.f, 720.f, 0.f, 0.f);
    m_CBuffer.frame_1 = XMINT4(0, 0, 0, 0);

    // 更新常量缓冲区
    D3D11_MAPPED_SUBRESOURCE mappedData;
    HR(m_pd3dImmediateContext->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
    memcpy_s(mappedData.pData, sizeof(m_CBuffer), &m_CBuffer, sizeof(m_CBuffer));
    m_pd3dImmediateContext->Unmap(m_pConstantBuffer.Get(), 0);


    //****************
    //创建着色器

    // 创建顶点着色器
    HR(CreateShaderFromFile(L"HLSL\\Plane_VS.cso", L"HLSL\\Plane_VS.hlsl", "VS", "vs_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pVertexShader.GetAddressOf()));

    // 创建顶点布局
    HR(m_pd3dDevice->CreateInputLayout(Plane::VertexPosTex::inputLayout, ARRAYSIZE(Plane::VertexPosTex::inputLayout),
        blob->GetBufferPointer(), blob->GetBufferSize(), m_pVertexLayout.GetAddressOf()));

    // 设置图元类型，设定输入布局
    m_pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pd3dImmediateContext->IASetInputLayout(m_pVertexLayout.Get());

    // 创建像素着色器
    HR(CreateShaderFromFile(L"HLSL\\Plane_PS0.cso", L"HLSL\\Plane_PS0.hlsl", "PS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pPixelShader0.GetAddressOf()));

    HR(CreateShaderFromFile(L"HLSL\\Plane_PS1.cso", L"HLSL\\Plane_PS1.hlsl", "PS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pPixelShader1.GetAddressOf()));

    // ******************
    // 给渲染管线各个阶段绑定好所需资源
    //

    // 将着色器绑定到渲染管线
    m_pd3dImmediateContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);
    m_pd3dImmediateContext->PSSetShader(m_pPixelShader1.Get(), nullptr, 0);
    // PS常量缓冲区对应HLSL寄存于b0的常量缓冲区
    m_pd3dImmediateContext->PSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());

    // 初始化纹理和采样器状态绑定到PS
    HR(CreateWICTextureFromFile(m_pd3dDevice.Get(), L"Texture\\noise0.jpg", nullptr, m_pTexture.GetAddressOf()));

    // 初始化采样器状态
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HR(m_pd3dDevice->CreateSamplerState(&sampDesc, m_pSamplerState.GetAddressOf()));

    // PS设置采样器和纹理
    m_pd3dImmediateContext->PSSetSamplers(0, 1, m_pSamplerState.GetAddressOf());
    m_pd3dImmediateContext->PSSetShaderResources(0, 1, m_pTexture.GetAddressOf());



    // ******************
    // 设置调试对象名
    //
    D3D11SetDebugObjectName(m_pVertexLayout.Get(), "VertexPosColorLayout");
    D3D11SetDebugObjectName(m_pVertexBuffer.Get(), "VertexBuffer");
    D3D11SetDebugObjectName(m_pVertexShader.Get(), "Plane_VS");
    D3D11SetDebugObjectName(m_pPixelShader1.Get(), "Plane_PS");
    
    

    return true;
}
