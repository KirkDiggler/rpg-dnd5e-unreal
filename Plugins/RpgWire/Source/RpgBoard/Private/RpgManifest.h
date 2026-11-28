#pragma once

#include "CoreMinimal.h"
#include <string>
#include <unordered_map>

namespace rpgwire { struct Member; }

class FRpgManifest
{
public:
    static FRpgManifest Load(const FString& Path);
    FString StandingFor(const rpgwire::Member& Member) const;

private:
    // Wire refs are case-sensitive; FString's default key equality is not.
    std::unordered_map<std::string, FString> Players;
    std::unordered_map<std::string, FString> NPCs;
};
