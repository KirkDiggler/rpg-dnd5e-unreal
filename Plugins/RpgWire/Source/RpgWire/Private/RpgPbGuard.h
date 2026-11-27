// Unreal defines a macro named `verify(expr)` for every file it compiles
// (Misc/AssertionMacros.h, through the definitions header the build tool
// force-includes). abseil's btree declares a member function named
// `verify()`, and protobuf's headers include abseil's before any line of
// ours — inside the generated contract, which is never edited by hand.
//
// This header is force-included ahead of every source file of the module
// (RpgWire.Build.cs). It parses the two abseil headers while no such macro
// is in force; they have their own include guards, so when protobuf's
// headers reach them again, they are already done. Whichever order the
// build tool force-includes in, the macro is either not yet defined, or
// put off here and restored after.
#pragma push_macro("verify")
#undef verify
#include <absl/container/btree_map.h>
#pragma pop_macro("verify")

// clang 20's -Wunreachable-code-return (an error in this engine since 5.8)
// fires on a shape protobuf 33 writes for every message: a constexpr function
// whose `if (arena_bits.has_value())` is true at compile time, with an `else`
// that returns. The warning is right and the code is sound. The generated
// corpus is never edited, and the engine's setting is for the whole group
// of unreachable-code warnings, so the one sub-warning is put off for this
// module's files here, where the reason is written; the rest stay errors.
#pragma clang diagnostic ignored "-Wunreachable-code-return"
