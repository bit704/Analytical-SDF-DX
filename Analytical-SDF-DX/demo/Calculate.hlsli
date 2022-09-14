
void setCamera(in float3 ro, in float3 ta, out float3x3 ca)
{
    // 代表相机朝向的forward vector,相机坐标系的Z轴
    float3 ww = normalize(ta - ro);
    // 与up vector叉积，得到相机坐标系的x轴
    float3 uu = normalize(cross(ww, float3(0.0, 1.0, 0.0)));
    // 相机坐标系的y轴
    float3 vv = normalize(cross(uu, ww));
    ca =  float3x3(uu, vv, ww);
}