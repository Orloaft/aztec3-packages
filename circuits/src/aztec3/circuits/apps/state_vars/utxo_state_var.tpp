#pragma once

#include "../opcodes/opcodes.hpp"

#include <stdlib/types/native_types.hpp>
#include <stdlib/types/circuit_types.hpp>

namespace {
using aztec3::circuits::apps::opcodes::Opcodes;
} // namespace

namespace aztec3::circuits::apps::state_vars {

template <typename Composer, typename Note>
Note UTXOStateVar<Composer, Note>::get(typename Note::NotePreimage const& advice)
{
    return Opcodes<Composer>::template UTXO_SLOAD<Note>(this, advice);
};

template <typename Composer, typename Note>
void UTXOStateVar<Composer, Note>::insert(typename Note::NotePreimage new_note_preimage)
{
    return Opcodes<Composer>::template UTXO_SSTORE<Note>(this, new_note_preimage);
};

} // namespace aztec3::circuits::apps::state_vars