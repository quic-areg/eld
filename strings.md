# Strings

### ELD:

1. Strings can be merged across output sections. Deduplication tables are stored per output section.
2. Managed per section (fragment)
3. Offsets for merged strings are handled by FragmentRef::getOffset.
4. Relocations for merged strings are fixed by zeroing the addend and using the FragmentRef indirection (Relocator::doMergeStrings)

For every single relocation we do a binary search by addend to find the string, and then look it up to see if it's merged. Regardless of if it is merged or not, we allocate a FragmentRef and zero the addend to point to a MergeableString.

current (40B)
```cpp
/// TODO: we can possibly reduce the size of MergeableString by 16 bytes by
/// removing String member. We can get the string by using the input offset of
/// this string and the input offset of the next string. Fragment Pointer could
/// also be removed.
struct MergeableString {
  MergeStringFragment *Fragment;
  llvm::StringRef String;
  uint32_t InputOffset;
  uint32_t OutputOffset;
  bool Exclude;
  MergeableString(MergeStringFragment *F, llvm::StringRef S, uint32_t I,
                  uint32_t O, bool E)
      : Fragment(F), String(S), InputOffset(I), OutputOffset(O), Exclude(E) {}
  void exclude() { Exclude = true; }
  uint64_t size() const { return String.size(); }
  bool hasOutputOffset() const {
    return OutputOffset != std::numeric_limits<uint32_t>::max();
  }
  bool isAlloc() const;
};
```
Ideal (12B):

```
struct MergeableString {
  u32 InputOffset;
  u32 OutputOffset;
  u32 Hash : 31;
  bool Exclude : 1;
}
```
To do this, we have to keep track of the owning section for Merged strings in each output section to redirect offsets.

###  LLD:

1. `MergeInputSection` split into `SectionPiece`s sorted by inputoffset and binary searched in `MergeInputSection::getSectionPiece()`
2. all mergeable pieces with the same output section are collected into a `MergeSyntheticSection` that owns a `StringTableBuilder` that dedups with a `DenseMap` keyed by slice contents. This tail merges identical substrings.
3. `MergeSyntheticSection::finalizeContents()` runs. Each live piece gets an output offset with `sh_entsize` preserved from the original input section.
4. Relocations: `relocateAlloc()` calls `getAddend()` -> `MergeInputSection::getOffset` (similar to us) binary searching pieces, zeroing the addend, and substitutes output offest. Similar to eld but without the per-reloc allocation of FragmentRefs. lld keeps just offsets and hashes; when a relocation is handled it binary-searches the pre-split piece table but does not need to form a new stringref like we do because it already deduped and recorded the result. `eld` carries `stringref` inside its `MergeableString` and uses it as a key in a `StringMap`. This means we have to reslice strings during lookups/relocations.

### Mold:

1. similar to lld but parallel
2. `mergeable-section.h` owning Pieces: (input off, output off, hash, size, live). hashing uses a fast `xxhash` over each piece; content equality is checked on hash match.
3. dedup is sharded: `MergeableSection::builder` is a `SharedMap` of piece content->output offsets inserted by multiple threads concurrently. output layout is produced by `build()` which walks shards in a deterministic order and assigns `outputOff` for each piece and writes merged data contiguously. Section contents are cached in a `std::vector<u8>` owned by the mergeable section so StringRefs remain valid after the input is unloaded.
4. Relocations: binary search during `scan_relocations()` like lld but without per reloc heap work.

### Improvements:
1. keep a stable copy of the section's data
2. use compact piece metadata. avoid holding a `StringRef`
3. Replace `StringMap` with hash+content dedup; lld: DenseMap keyed by slice; mold: sharded hash table. cache-friendly approach avoiding rehash/alloc churn of a StringMap
4. Relocation flow: rewrite addends to `outputOff`  using pre-split piece tables and avoid allocating a per-reloc FragmentRef. Return offsets directly into relocation application to cut allocations.
5. assert `sh_entsize` consistency on mergeable sections




