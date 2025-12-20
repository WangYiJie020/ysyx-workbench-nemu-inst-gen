#include <bitset>
#include <initializer_list>
#include <stddef.h>
#include <stdint.h>
#include <utility>

#define GET_BITAT(bit) (((instruction) >> (bit)) & 0x1)
#define GET_BITS(high, low) (selbits((instruction), (high), (low)))
#define SEXT(v) (v).sext()

inline int64_t AS_SIGNED(uint64_t value) { return static_cast<int64_t>(value); }

#define GET_PC() (0)

#define xlen() 32
#define MXLEN 32u

#define _UNUSED_INTVAL_ 0

using word_t = uint32_t;
using wider_word_t = uint64_t;

#define WIDER_UNIT 1ull
#define WIDE_MUL *WIDER_UNIT *

inline uint32_t sext(uint32_t value, int width) {
  if (width >= 32)
    return value;
  uint32_t sign_bit = 1u << (width - 1);
  if (value & sign_bit) {
    uint32_t mask = ~((1u << width) - 1);
    return value | mask;
  } else {
    return value & ((1u << width) - 1);
  }
}
inline uint32_t selbits(uint32_t value, int high, int low) {
  uint32_t mask = ((1u << (high - low + 1)) - 1) << low;
  return (value & mask) >> low;
}

template <typename T> struct _TailCast {};
template <typename T> inline T operator*(uint32_t value, const _TailCast<T> &) {
  return T(value);
}

struct _BitsSRA_Wrapper {
  uint32_t value;
  _BitsSRA_Wrapper(uint32_t value) : value(value) {}
  uint32_t operator>>(uint32_t shamt) const {
    if (shamt >= 32) {
      return (value & 0x80000000) ? 0xFFFFFFFF : 0;
    } else {
      if (value & 0x80000000) {
        return (value >> shamt) | (~((uint32_t)0) << (32 - shamt));
      } else {
        return value >> shamt;
      }
    }
  }
};
#define SRA *(_TailCast<_BitsSRA_Wrapper>()) >>

struct _BitsSelect_Wrapper {
  uint32_t value;
  _BitsSelect_Wrapper(uint32_t value) : value(value) {}
  uint32_t operator*(const std::pair<int, int> &rng) const {
    return selbits(value, rng.first, rng.second);
  }
};
#define Rng(high, low)                                                         \
  _TailCast<_BitsSelect_Wrapper>() * std::make_pair((high), (low))
#define At(bit) Rng((bit), (bit))

template<size_t N> struct Repl;

struct Concat{
	size_t len;
	wider_word_t value;
	constexpr explicit Concat(size_t l, wider_word_t v) : len(l), value(v) {}
	constexpr Concat(word_t v) : len(32), value(v) {}
	constexpr Concat(wider_word_t v) : len(64), value(v) {}
	// template<size_t N>
	// constexpr Concat(std::initializer_list<Repl<N>> list) : len(0), value(0) {
	// }
	constexpr Concat(std::initializer_list<Concat> list) : len(0), value(0) {
		size_t shift = 0;
		for (auto it = list.end(); it != list.begin();) {
			--it;
			len += it->len;
			value |= (it->value << shift);
			shift += it->len;
		}
	}
	constexpr operator wider_word_t() const { return value; }
};

template <size_t N> struct Bits {
  // std::bitset<N> value;
  using dword = wider_word_t;
	dword value;
	constexpr Bits(dword value = 0) : value(value) {}
  constexpr operator dword() const { return value; }
	constexpr operator Concat() const {
		return Concat(N, value);
	}

	Bits(const Concat& c) : value(c.value) {}
	Bits(std::initializer_list<Concat> list){

	}

  Bits<MXLEN> sext() const { return Bits<MXLEN>(::sext(value, N)); }
  bool operator[](size_t bitidx) const { return (value >> bitidx) & 0x1; }
};

using XReg = Bits<MXLEN>;
using DXReg = Bits<MXLEN WIDE_MUL 2>;

template <size_t N> struct Repl {
  operator Bits<N>() const {}
	operator Concat() const {
		return Concat(N, 0);
	}
	template <size_t M>
	Repl(const Bits<M>&) {}
};

template <size_t N> uint32_t read_memory(uint32_t address, int encoding) {
  // Placeholder implementation
  return 0;
}
template <size_t N>
void write_memory(uint32_t address, uint32_t value, int encoding) {
  // Placeholder implementation
}
