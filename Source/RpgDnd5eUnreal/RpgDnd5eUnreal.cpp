// The game module is thin on purpose: the client's code is the RpgWire
// plugin's, so it builds and tests without a game to launch it.
#include "RpgDnd5eUnreal.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, RpgDnd5eUnreal, "RpgDnd5eUnreal");
