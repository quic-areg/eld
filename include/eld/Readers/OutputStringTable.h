//===- OutputStringTable.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_OUTPUTSTRINGTABLE_H
#define ELD_READERS_OUTPUTSTRINGTABLE_H

#include "eld/Fragment/MergeStringFragment.h"
#include "llvm/MC/StringTableBuilder.h"

namespace eld {
class Module;
struct MergeableString;
class ELFSection;
class OutputSectionEntry;

class Fragment;

class OutputStringTable : public Fragment {
public:
  OutputStringTable(ELFSection *O) : Fragment(Fragment::Region, O, 1) {}

  void intern(MergeableString *S) { StringTable.add({S->String, S->Hash}); }

  void finalize() {
    // llvm::outs() << "finalized string table\n";
    // llvm::outs().flush();

    assert(!StringTable.isFinalized());
    /// FIXME: finalize out of order.
    StringTable.finalize();
  }

  virtual Expected<void> emit(MemoryRegion &R, Module &M) override {
    // llvm::outs() << "emitting fake fragment data\n";
    // llvm::outs().flush();

    assert(StringTable.isFinalized());
    StringTable.write(R.begin());
    return {};
  }

  uint32_t getOutputOffset(llvm::CachedHashStringRef S) {
    // llvm::outs() << "requesting offset for " << S.data() << "\n";
    // llvm::outs().flush();
    assert(StringTable.isFinalized());
    assert(hasOffset());
    return StringTable.getOffset(S) + getOffset();
  }

  size_t size() const override {
    // llvm::outs() << "requesting size for fake fragment and that size is "
    //              << StringTable.getSize() << "\n";
    // llvm::outs().flush();
    return StringTable.getSize();
  }

  bool isZeroSizedFrag() const override { return !size(); }

  bool isMergeStr() const override { return false; }

private:
  llvm::StringTableBuilder StringTable{llvm::StringTableBuilder::RAW,
                                       llvm::Align(1)};
};
} // namespace eld
#endif
