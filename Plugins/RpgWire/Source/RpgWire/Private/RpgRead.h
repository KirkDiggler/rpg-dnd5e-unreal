#pragma once
#include "RpgSession.h"
namespace dnd5e::api::session::v1alpha1 { class PublicMemberInfo; class Sighting; }
namespace rpgwire
{
void ReadMember(const dnd5e::api::session::v1alpha1::PublicMemberInfo& wire, Member& out);
void ReadSighting(const dnd5e::api::session::v1alpha1::Sighting& wire, Sighting& out);
}
