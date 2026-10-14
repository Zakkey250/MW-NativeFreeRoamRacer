// Offline SDK layout check; does not open or modify a game process.
#include <Windows.h>
#include <NFSPluginSDK/Game.MW05/Types/GRaceStatus.h>
#include <cstdio>
#include <cstddef>
int main() {
    using R=NFSPluginSDK::MW05::GRaceStatus;
    std::printf("playMode=%zX raceParms=%zX raceBin=%zX\n",offsetof(R,mPlayMode),offsetof(R,mRaceParms),offsetof(R,mRaceBin));
}
