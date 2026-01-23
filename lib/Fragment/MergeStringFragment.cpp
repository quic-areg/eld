//===- MergeStringFragment.cpp---------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/Support/xxhash.h"

using namespace eld;

MergeStringFragment::MergeStringFragment(ELFSection *O)
    : Fragment(Fragment::MergeString, O, 1) {}

bool MergeStringFragment::readStrings(LinkerConfig &Config) {
  llvm::StringRef Contents = getOwningSection()->getContents();
  if (Contents.empty())
    return true;
  uint64_t Offset = 0;
  ELFSection *S = getOwningSection();
  while (!Contents.empty()) {
    size_t End = Contents.find('\0');
    if (End == llvm::StringRef::npos) {
      Config.raise(Diag::string_not_null_terminated)
          << S->getLocation(Offset, Config.options());
      return false;
    }
    // account for the null character
    uint64_t Size = End + 1;
    llvm::StringRef String = Contents.slice(0, Size);
    uint32_t Hash = llvm::xxh3_64bits(String);
    Strings.push_back(make<MergeableString>(
        this, String, Offset, std::numeric_limits<uint32_t>::max(), Hash,
        false));
    Contents = Contents.drop_front(Size);
    if (Config.getPrinter()->isVerbose()) {
      Config.raise(Diag::splitting_merge_string_section)
          << S->getLocation(Offset, Config.options()) << String.data() << 1;
    }
    Offset += Size;
  }
  assert(Offset == getOwningSection()->size());
  return true;
}

MergeableString *MergeStringFragment::findString(uint64_t Offset) const {
  /// FIXME: This case should ideally assert or error rather than return nullptr
  if (Offset >= getOwningSection()->size())
    return nullptr;
  return llvm::partition_point(Strings, [Offset](MergeableString *S) {
    return S->InputOffset <= Offset;
  })[-1];
}

void MergeStringFragment::copyData(void *Dest, uint64_t Bytes,
                                   uint64_t Offset) const {
  MergeableString *S = findString(Offset);
  assert(S);
  uint64_t OffsetInString = Offset - S->InputOffset;
  uint64_t Size = std::min((uint64_t)Bytes, S->size() - OffsetInString);
  std::memcpy(Dest, S->String.begin() + OffsetInString, Size);
}

bool MergeableString::isAlloc() const {
  return Fragment->getOwningSection()->isAlloc();
}