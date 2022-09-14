// 参考：https://iquilezles.org/articles/distfunctions/

float dot2(in float2 v)
{
    return dot(v, v);
}
float dot2(in float3 v)
{
    return dot(v, v);
}
float ndot(in float2 a, in float2 b)
{
    return a.x * b.x - a.y * b.y;
}

float sdPlane(float3 p, float3 n)
{
    return dot(p, n);
}

float sdSphere(float3 p, float r)
{
    return length(p) - r;
}

float sdTorus(float3 p, float2 t)
{
    return length(float2(length(p.xz) - t.x, p.y)) - t.y;
}


float sdCylinder(float3 p, float2 h, int mode)
{
    float res;
    float2 d;
    switch (mode)
    {
        // 面向z轴
        case 0:
            d = abs(float2(length(p.xy), p.z)) - h;
            res = min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
            break;
        // 面向x轴
        case 1:
            d = abs(float2(length(p.yz), p.x)) - h;
            res = min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
            break;
        // 面向y轴
        case 2:
            d = abs(float2(length(p.xz), p.y)) - h;
            res = min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
            break;
        default:
            break;
    }
    return res;

}

float sdBox(float3 p, float3 b)
{
    float3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float sdBoxFrame(float3 p, float3 b, float e)
{
    p = abs(p) - b;
    e /= 2;
    float3 q = abs(p + e) - e;
    
    return min(min(
      length(max(float3(p.x, q.y, q.z), 0.0)) + min(max(p.x, max(q.y, q.z)), 0.0),
      length(max(float3(q.x, p.y, q.z), 0.0)) + min(max(q.x, max(p.y, q.z)), 0.0)),
      length(max(float3(q.x, q.y, p.z), 0.0)) + min(max(q.x, max(q.y, p.z)), 0.0));
}


float sdOctahedron(float3 p, float s)
{
    p = abs(p);
    float m = p.x + p.y + p.z - s;
    float3 q;
    if (3.0 * p.x < m)
        q = p.xyz;
    else if (3.0 * p.y < m)
        q = p.yzx;
    else if (3.0 * p.z < m)
        q = p.zxy;
    else
        return m * 0.57735027;
    
    float k = clamp(0.5 * (q.z - q.y + s), 0.0, s);
    return length(float3(q.x, q.y - s + k, q.z - k));
}




