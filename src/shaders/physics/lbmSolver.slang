

[numthreads(128, 1, 1)]
void main(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID,
          uint3 dispatchThreadID: SV_DispatchThreadID, uint groupIndex: SV_GroupIndex)
{
}
