#pragma once

#include <memory>
#include <FEXCore/Utils/LogManager.h>

namespace FEXCore {

  // BucketList is an optimized container, it includes an inline array of Size
  // and can overflow to a linked list of further buckets
  //
  // To optimize for best performance, Size should be big enough to allocate one or two
  // buckets for the typical case
  // Picking a Size so sizeof(Bucket<...>) is a power of two is also a small win
  template<unsigned _Size, typename T = uint32_t>
  struct BucketList {
    static constexpr const unsigned Size = _Size;

    T Items[Size];
    std::unique_ptr<BucketList<Size>> Next;

    void Clear() {
      Items[0] = 0;
      #ifndef NDEBUG
      for (int i = 1; i < Size; i++)
        Items[i] = 0xDEADBEEF;
      #endif
      Next.reset();
    }

    BucketList() {
      Clear();
    }

    template<typename EnumeratorFn>
    inline void Iterate(EnumeratorFn Enumerator) const {
      int i = 0;
      auto Bucket = this;

      for(;;) {
        auto Item = Bucket->Items[i];
        if (Item == 0)
          break;

        Enumerator(Item);

        if (++i == Bucket->Size) {
          LOGMAN_THROW_A(Bucket->Next != nullptr, "Interference bug");
          Bucket = Bucket->Next.get();
          i = 0;
        }
      }
    }

    template<typename EnumeratorFn>
    inline bool Find(EnumeratorFn Enumerator) const {
      int i = 0;
      auto Bucket = this;

      for(;;) {
        auto Item = Bucket->Items[i];
        if (Item == 0)
          break;

        if (Enumerator(Item))
          return true;

        if (++i == Bucket->Size) {
          LOGMAN_THROW_A(Bucket->Next != nullptr, "Bucket in bad state");
          Bucket = Bucket->Next.get();
          i = 0;
        }
      }

      return false;
    }

    void Append(uint32_t Val) {
      auto that = this;

      while (that->Next) {
        that = that->Next.get();
      }

      int i;
      for (i = 0; i < Size; i++) {
        if (that->Items[i] == 0) {
          that->Items[i] = Val;
          break;
        }
      }

      if (i < (Size-1)) {
        that->Items[i+1] = 0;
      } else {
        that->Next = std::make_unique<BucketList<Size, T>>();
      }
    }
    void Erase(uint32_t Val) {
      int i = 0;
      auto that = this;
      auto foundThat = this;
      auto foundI = 0;

      for (;;) {
        if (that->Items[i] == Val) {
          foundThat = that;
          foundI = i;
          break;
        }
        else if (++i == Size) {
          i = 0;
          LOGMAN_THROW_A(that->Next != nullptr, "Bucket::Erase but element not contained");
          that = that->Next.get();
        }
      }

      for (;;) {
        if (that->Items[i] == 0) {
          foundThat->Items[foundI] = that->Items[i-1];
          that->Items[i-1] = 0;
          break;
        }
        else if (++i == Size) {
          if (that->Next->Items[0] == 0) {
            that->Next.reset();
            foundThat->Items[foundI] = that->Items[Size-1];
            that->Items[Size-1] = 0;
            break;
          }
          i = 0;
          that = that->Next.get();
        }
      }
    }
  };

} // namespace