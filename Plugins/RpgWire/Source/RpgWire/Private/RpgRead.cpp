#include "RpgRead.h"
#include "dnd5e/api/session/v1alpha1/types.pb.h"

namespace rpgwire
{
namespace v1 = dnd5e::api::session::v1alpha1;
namespace
{
Member::Kind ReadKind(v1::MemberKind kind)
{
    switch (kind)
    {
        case v1::MEMBER_KIND_PLAYER: return Member::Kind::Player;
        case v1::MEMBER_KIND_MONSTER: return Member::Kind::Monster;
        case v1::MEMBER_KIND_WORLD: return Member::Kind::World;
        default: return Member::Kind::Unspecified;
    }
}
}
void ReadMember(const v1::PublicMemberInfo& wire, Member& out)
{
    out.id = wire.id();
    out.kind = ReadKind(wire.kind());
    out.name = wire.name();
    out.monster_ref = wire.monster_ref();
    out.class_ref = wire.class_ref();
    out.faction = wire.faction();
    out.appearance_ref = wire.appearance_ref();
}
void ReadSighting(const v1::Sighting& wire, Sighting& out)
{
    out.id = wire.subject();
    out.name = wire.name();
    out.kind = ReadKind(wire.kind());
    out.stance = wire.stance();
    out.cell = Cell(static_cast<int>(wire.seen().position().x()), static_cast<int>(wire.seen().position().y()));
    out.standing = static_cast<int>(wire.seen().standing());
}
}
