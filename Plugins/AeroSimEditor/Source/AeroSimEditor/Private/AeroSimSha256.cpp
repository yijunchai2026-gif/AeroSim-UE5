#include "AeroSimSha256.h"

namespace
{
constexpr uint32 InitialState[8] = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
};
constexpr uint32 RoundConstants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

uint32 RotateRight(const uint32 Value, const uint32 Bits)
{
    return (Value >> Bits) | (Value << (32U - Bits));
}

void TransformBlock(const uint8* Block, uint32* State)
{
    uint32 Words[64]{};
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const int32 Offset = Index * 4;
        Words[Index] = (static_cast<uint32>(Block[Offset]) << 24U)
            | (static_cast<uint32>(Block[Offset + 1]) << 16U)
            | (static_cast<uint32>(Block[Offset + 2]) << 8U)
            | static_cast<uint32>(Block[Offset + 3]);
    }
    for (int32 Index = 16; Index < 64; ++Index)
    {
        const uint32 S0 = RotateRight(Words[Index - 15], 7U) ^ RotateRight(Words[Index - 15], 18U) ^ (Words[Index - 15] >> 3U);
        const uint32 S1 = RotateRight(Words[Index - 2], 17U) ^ RotateRight(Words[Index - 2], 19U) ^ (Words[Index - 2] >> 10U);
        Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
    }

    uint32 A = State[0];
    uint32 B = State[1];
    uint32 C = State[2];
    uint32 D = State[3];
    uint32 E = State[4];
    uint32 F = State[5];
    uint32 G = State[6];
    uint32 H = State[7];
    for (int32 Index = 0; Index < 64; ++Index)
    {
        const uint32 Sum1 = RotateRight(E, 6U) ^ RotateRight(E, 11U) ^ RotateRight(E, 25U);
        const uint32 Choice = (E & F) ^ (~E & G);
        const uint32 Temp1 = H + Sum1 + Choice + RoundConstants[Index] + Words[Index];
        const uint32 Sum0 = RotateRight(A, 2U) ^ RotateRight(A, 13U) ^ RotateRight(A, 22U);
        const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
        const uint32 Temp2 = Sum0 + Majority;
        H = G;
        G = F;
        F = E;
        E = D + Temp1;
        D = C;
        C = B;
        B = A;
        A = Temp1 + Temp2;
    }
    State[0] += A;
    State[1] += B;
    State[2] += C;
    State[3] += D;
    State[4] += E;
    State[5] += F;
    State[6] += G;
    State[7] += H;
}
}

FString AeroSimSha256(const FString& Value)
{
    const FTCHARToUTF8 Utf8(*Value);
    const uint64 InputLength = static_cast<uint64>(Utf8.Length());
    TArray<uint8> Padded;
    Padded.Append(reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<int32>(InputLength));
    Padded.Add(0x80U);
    while ((Padded.Num() % 64) != 56)
    {
        Padded.Add(0U);
    }
    const uint64 BitLength = InputLength * 8U;
    for (int32 Shift = 56; Shift >= 0; Shift -= 8)
    {
        Padded.Add(static_cast<uint8>((BitLength >> Shift) & 0xffU));
    }

    uint32 State[8]{};
    FMemory::Memcpy(State, InitialState, sizeof(State));
    for (int32 Offset = 0; Offset < Padded.Num(); Offset += 64)
    {
        TransformBlock(Padded.GetData() + Offset, State);
    }
    uint8 Digest[32]{};
    for (int32 Index = 0; Index < 8; ++Index)
    {
        Digest[Index * 4] = static_cast<uint8>(State[Index] >> 24U);
        Digest[Index * 4 + 1] = static_cast<uint8>(State[Index] >> 16U);
        Digest[Index * 4 + 2] = static_cast<uint8>(State[Index] >> 8U);
        Digest[Index * 4 + 3] = static_cast<uint8>(State[Index]);
    }
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}
