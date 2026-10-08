// Minimum hardware inline-ray test: one opaque triangle in a TLAS.
RaytracingAccelerationStructure scene : register(t0);
RWStructuredBuffer<uint> pixels : register(u0);
cbuffer Dimensions : register(b0) { uint width; uint height; uint rowWords; };
[numthreads(8, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= width || thread.y >= height) return;
    float2 uv = (float2(thread.xy) + 0.5) / float2(width, height);
    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3((uv.x * 2 - 1) * width / height,
                                    1 - uv.y * 2, 1));
    ray.TMin = 0.001;
    ray.TMax = 10.0;
    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {}
    bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
    // Packed BGRA bytes copied from buffer to the B8G8R8A8 backbuffer.
    uint3 rgb = hit ? uint3(16, 230, 64) : uint3(8, 20, 48);
    pixels[thread.y * rowWords + thread.x] = rgb.b | (rgb.g << 8) |
                                            (rgb.r << 16) | 0xff000000;
}
