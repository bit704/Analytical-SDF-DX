#include "Calculate.hlsli"

// 光线投射
float2 raycast(float3 ro, float3 rd, int mnum)
{
    float2 res = float2(-1.0, -1.0);

    //最大行进距离
    float tmax = 20.0;
    
    //行进
    //初始行进距离
    float t = 1.0;
    for (int i = 0; i < mnum && t < tmax; i++)
    {
        float2 h = map(ro + rd * t);
        if (abs(h.x) < (0.0001 * t))
        {
            res = float2(t, h.y);
            break;
        }
        t += h.x;
    }
    
    return res;
}

float3 render(in float3 ro, in float3 rd)
{
    // 默认颜色，即背景颜色
    float3 col = float3(0, 0, 0);
    
    // 光线投射
    float2 res = raycast(ro, rd, (int)g_Factor.x);
    // 行进步长
    float t = res.x;
    // 材质参数
    float m = res.y;
    if (m >= 0)
    {
        float3 pos = ro + t * rd;
        float3 nor = (m < 1.5) ? float3(0.0, 1.0, 0.0) : calcNormal(pos, t);
        float3 ref = reflect(rd, nor);
        
        //base color
        if (g_Switch.x == 1)
        {
            // 材质        
            col = 0.2 + 0.2 * sin(m * 2.0 + float3(0.0, 1.0, 2.0));
        
            //地板材质
            if (m == 0)
            {
                col = float3(.3, .0, .0);
                //噪声纹理
                col = col * g_Tex.Sample(g_SamLinear, float2(pos.x, pos.z)).rgb;
            }
        }
        
        //最终颜色
        float3 lin = float3(0, 0, 0);

        // 环境光遮蔽因子
        float occ = float3(1, 1, 1);
        if (g_Switch.w == 1)
        {
            occ = calcAO(pos, nor);
        }
        
        // 方向光
        if(g_Switch.y == 1)
        {
            // lig为以当前pos为原点时光源的位置
            // 随时间移动
            // xz平面上做往复圆周运动,y轴上下移动
            float3 lig = normalize(float3(2 * sin(fmod(g_Time.x, 6.28)), 1.5 + cos(fmod(g_Time.x, 6.28)), 2 * cos(fmod(g_Time.x, 6.28))));
            
            // 求半程向量
            float3 hal = normalize(lig - rd);
            // Lambert漫反射
            float dif = clamp(dot(nor, lig), 0.0, 1.0);
            // 乘上环境光遮蔽
            dif *= occ;
            // 软阴影
            dif *= calcSoftshadow(pos, lig, 0.02, 2.5, g_Factor.y);
            // blinn-phong高光
            float spe = pow(clamp(dot(nor, hal), 0.0, 1.0), 16.0);
            // 缩放因子
            lin += col * 2.20 * dif * float3(1.3, 1., 0.7);
            lin += 0.2 * spe * float3(1.3, 1., 0.7);
        }

        
        // 天空光
        if (g_Switch.z == 1)
        {
            // 漫反射，法线越朝向天空（y轴方向）反射越强
            float dif = sqrt(clamp(0.5 + 0.5 * nor.y, 0.0, 1.0));
            // 乘上环境光遮蔽
            dif *= occ;
            // 高光，光线反射方向越朝向天空越强
            float spe = smoothstep(-0.2, 0.2, ref.y);
            // 软阴影
            spe *= calcSoftshadow(pos, ref, 0.02, 2.5, g_Factor.y);
            // 缩放因子
            spe *= 5.0 * pow(clamp(1.0 + dot(nor, rd), 0.0, 1.0), 5.0);
            lin += col * 0.60 * dif * float3(0.4, 0.6, 1.15);
            lin += spe;
        }

        // 根据光线行进距离插值,得到深度图
        col = lin;
        col = lerp(col, float3(0.9, 0.9, 0.9), 1.0 - exp(-0.0001 * t * t * t));
    }

    return float3(clamp(col, 0.0, 1.0));
}



float4 PS(VertexOut pIn) : SV_Target
{   
    float2 fragCoord = pIn.tex * g_Resolution.xy;
 
    // 让屏幕中心为屏幕坐标系原点，并调整xy轴单位长度一致，得到当前像素在相机坐标系下的位置
    float2 p = (2.0 * fragCoord - g_Resolution.xy) / g_Resolution.y;

    // 焦距
    const float fl = 3;
        
    float3 ro = g_Camera._14_24_34;
    
    // 光线方向
    float3 rd = mul((float3x3) g_Camera, normalize(float3(p, fl)));
    
    // 渲染
    float3 col = render(ro, rd);
        
    // gamma编码，提升暗部细节体验
    col = pow(col, float3(0.4545, 0.4545, 0.4545));

    
    float4 fragColor = float4(col, 1.0);
    return fragColor;
   
}

