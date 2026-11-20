#ifndef COPPER_BYTE_VECTOR_SERIALIZATION_H
#define COPPER_BYTE_VECTOR_SERIALIZATION_H

// Bulk serialization for std::vector<std::byte>.
//
// std::byte is an enum class, so it is not arithmetic. Cereal's vector
// support therefore picks the per-element path: one size tag, then one
// save_minimal/hg_proc_memcpy call per byte. Through Thallium's
// proc_output_archive every 1-byte write may call hg_proc_set_size and
// realloc. Measured on Aurora, a 440 MB libtorch_cpu.so response took about
// 48 s to serialize inside the handler ULT and held the single handler
// execution stream for the whole time (stack:
// respond -> hg_proc_meta_serialization -> proc_output_archive::write ->
// hg_proc_set_size -> realloc).
//
// These overloads write the same bytes as the per-element path (size tag,
// then the raw bytes, one byte per element), but in a single bulk call. The
// wire format is unchanged, so nodes running old and new builds remain
// interoperable. tests/test_byte_vector_serialization.cpp checks this
// byte for byte.
//
// Include this after <thallium/serialization/stl/vector.hpp> in every
// translation unit that sends or receives std::vector<std::byte>.

#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>
#include <cstddef>
#include <vector>

namespace cereal {

template <class Archive, class A>
inline void CEREAL_SAVE_FUNCTION_NAME(Archive& ar, std::vector<std::byte, A> const& vector) {
    ar(make_size_tag(static_cast<size_type>(vector.size())));
    if(!vector.empty()) {
        ar(binary_data(vector.data(), vector.size() * sizeof(std::byte)));
    }
}

template <class Archive, class A>
inline void CEREAL_LOAD_FUNCTION_NAME(Archive& ar, std::vector<std::byte, A>& vector) {
    size_type vector_size;
    ar(make_size_tag(vector_size));
    vector.resize(static_cast<std::size_t>(vector_size));
    if(!vector.empty()) {
        ar(binary_data(vector.data(), static_cast<std::size_t>(vector_size) * sizeof(std::byte)));
    }
}

} // namespace cereal

#endif // COPPER_BYTE_VECTOR_SERIALIZATION_H
