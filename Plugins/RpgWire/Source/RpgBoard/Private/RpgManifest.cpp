#include "RpgManifest.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RpgSession.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FRpgManifest FRpgManifest::Load(const FString& Path)
{
    FRpgManifest Manifest;
    if (!FPaths::FileExists(Path)) return Manifest;
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root) return Manifest;

    const TSharedPtr<FJsonObject>* Index = nullptr;
    if (Root->TryGetObjectField(TEXT("players"), Index) && Index && *Index)
    {
        for (const auto& Field : (*Index)->Values)
        {
            FString Asset;
            if (!Field.Key.IsEmpty() && Field.Value->TryGetString(Asset) && !Asset.IsEmpty())
                Manifest.Players.emplace(TCHAR_TO_UTF8(*Field.Key), MoveTemp(Asset));
        }
    }
    if (Root->TryGetObjectField(TEXT("npcs"), Index) && Index && *Index)
    {
        for (const auto& Field : (*Index)->Values)
        {
            const TSharedPtr<FJsonObject>* Entry = nullptr;
            FString Asset;
            if (!Field.Key.IsEmpty() && Field.Value->TryGetObject(Entry) && Entry && *Entry &&
                (*Entry)->TryGetStringField(TEXT("standing"), Asset) && !Asset.IsEmpty())
                Manifest.NPCs.emplace(TCHAR_TO_UTF8(*Field.Key), MoveTemp(Asset));
        }
    }
    return Manifest;
}

FString FRpgManifest::StandingFor(const rpgwire::Member& Member) const
{
    switch (Member.kind)
    {
        case rpgwire::Member::Kind::Player:
            if (const auto Found = Players.find(Member.class_ref); Found != Players.end()) return Found->second;
            break;
        case rpgwire::Member::Kind::Monster:
            if (const auto Found = NPCs.find(Member.appearance_ref); Found != NPCs.end()) return Found->second;
            break;
        default: break;
    }
    return {};
}
