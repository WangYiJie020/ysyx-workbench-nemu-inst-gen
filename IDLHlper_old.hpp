#include <bitset>
#include <functional>
#include <initializer_list>
#include <stddef.h>
#include <stdint.h>
#include <utility>

// -- Output Instruction Helper --

#define INIT()                                                                 \
  auto X = regs;                                                               \
  Bits<5> xs2 = Bits<5>(InstRng(24, 20));                                      \
  Bits<5> xs1 = Bits<5>(InstRng(19, 15));                                      \
  Bits<5> xd = Bits<5>(InstRng(11, 7));                                        \
  int _pc_offset = 4;

#define EXEC_SUCCESS 0
#define EXEC_NOMATCH -1

#define FINISH()                                                               \
  return EXEC_NOMATCH;                                                         \
  exit_success:                                                                \
  *pc += _pc_offset;                                                           \
  return EXEC_SUCCESS;

#define ENCODING_INST instruction
#define INST_IS(name) ((ENCODING_INST & MASK_##name) == MATCH_##name)

#define TRY_MATCH(name, ...)                                                   \
  do {                                                                         \
    if (INST_IS(name)) {                                                       \
      __VA_ARGS__;                                                             \
      goto exit_success;                                                       \
    }                                                                          \
  } while (0)

#define InstAt(bit) Bits<1>((ENCODING_INST >> (bit)) & 0x1)
#define InstRng(high, low)                                                     \
  Bits<(high) - (low) + 1>(selbits(ENCODING_INST, (high), (low)))

#define jump(addr)                                                             \
  do {                                                                         \
    _pc_offset = (addr) - *pc;                                                 \
  } while (0)

#define jump_halfword(addr) jump(addr)

// -- IDL Helper --

using Boolean = bool;

enum class ExtensionName {
  Smdbltrp,
  Zilsd,
  Zclsd,
  M,
  D,
  V,
  N,
  U,
};
consteval bool implemented(ExtensionName name) {
  switch (name) {
  case ExtensionName::M:
    return true;
  default:
    return false;
  }
}
enum class PrivilegeMode { M, S, U, VU, VS };
consteval PrivilegeMode mode() {
	return PrivilegeMode::M;
}
inline void set_mode(PrivilegeMode mode){}
bool compatible_mode(PrivilegeMode target_mode, PrivilegeMode access_mode);

enum class ExceptionCode {
  Breakpoint,
  Mcall,
  Scall,
  Ucall,
  VScall,
  IllegalInstruction,
  VirtualInstruction,
};

#define TRAP_ON_EBREAK 1
#define TRAP_ON_ECALL_FROM_M 1
#define TRAP_ON_ECALL_FROM_S 1
#define TRAP_ON_ECALL_FROM_U 1
#define TRAP_ON_ECALL_FROM_VS 1

#define eei_ebreak() UnreachableAbort();
#define eei_ecall_from_m() UnreachableAbort();
#define eei_ecall_from_s() UnreachableAbort();
#define eei_ecall_from_u() UnreachableAbort();
#define eei_ecall_from_vs() UnreachableAbort();

// -- IDL Helper --

#define MXLEN 32u
#define xlen() MXLEN

#if MXLEN == 32
using word_t = uint32_t;
using sword_t = int32_t;
using dword_t = uint64_t;
#else
#endif

#define WIDER_UNIT 1ull
#define WIDE_MUL *WIDER_UNIT *

using RegFile = word_t[];

inline uint64_t sext64(uint64_t value, int width) {
  if (width >= 64)
    return value;
  uint64_t sign_bit = 1ull << (width - 1);
  if (value & sign_bit) {
    uint64_t mask = ~((1ull << width) - 1);
    return value | mask;
  } else {
    return value & ((1ull << width) - 1);
  }
}
inline uint32_t sext32(uint32_t value, int width) {
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

#if MXLEN == 32
#define sext(value, width) sext32((uint32_t)(value), (width))
#else
#define sext(value, width) sext64((uint64_t)(value), (width))
#endif

inline dword_t selbits(dword_t value, int high, int low) {
  if (high >= 63 && low == 0)
    return value;
  dword_t mask = ((1ull << (high - low + 1)) - 1) << low;
  return (value & mask) >> low;
}

template <typename T> struct _TailCast {};
template <typename T, typename LT>
inline T operator*(LT value, const _TailCast<T> &) {
  return T(value);
}

template <typename Func>
struct _OpFuncTraits : _OpFuncTraits<decltype(&Func::operator())> {};

template <typename R, typename L, typename R2> struct _OpFuncTraits<R(L, R2)> {
  using RawFn = R(L, R2);
  using FuncType = std::function<RawFn>;

  using Left = std::remove_reference_t<L>;
  using Right = std::remove_reference_t<R2>;
  using Out = R;
};

template <typename R, typename L, typename R2>
struct _OpFuncTraits<R (*)(L, R2)> : _OpFuncTraits<R(L, R2)> {};

template <typename C, typename R, typename L, typename R2>
struct _OpFuncTraits<R (C::*)(L, R2) const> : _OpFuncTraits<R(L, R2)> {};

template <typename C, typename R, typename L, typename R2>
struct _OpFuncTraits<R (C::*)(L, R2)> : _OpFuncTraits<R(L, R2)> {};

template <typename Func> struct _OpFunc_Wrap {
  using traits = _OpFuncTraits<Func>;
  using func_t = typename traits::FuncType;
  using L = typename traits::Left;
  using R = typename traits::Right;
  using Out = typename traits::Out;
  func_t func;
  L left;
  _OpFunc_Wrap(func_t f) : func(f) {}
  Out operator*(R right) { return func(left, right); }
  Out operator>>(R right) { return func(left, right); }
};
template <typename Func>
inline _OpFunc_Wrap<Func> operator*(typename _OpFuncTraits<Func>::Left left,
                                    _OpFunc_Wrap<Func> &&wrapper) {
  wrapper.left = left;
  return wrapper;
}

#define _WrapOp(func) _OpFunc_Wrap<decltype(func)>(func)
#define ApplyOP(func) *_OpFunc_Wrap<decltype(func)>(func) *

inline uint32_t _op_sra32(uint32_t value, uint32_t shamt) {
	if(shamt == 0) return value;
  if (shamt >= 32) {
    return (value & 0x80000000) ? 0xFFFFFFFF : 0;
  } else {
    if (value & 0x80000000) {
			// when shamt is 0, << 32 is undefined behavior
      return (value >> shamt) | (~((uint32_t)0) << (32 - shamt));
    } else {
      return value >> shamt;
    }
  }
}
#define SRA *_WrapOp(_op_sra32) >>

inline auto _op_selbits(dword_t value, std::pair<int, int> rng) {
  return selbits(value, rng.first, rng.second);
}
#define Rng(high, low) _WrapOp(_op_selbits) * std::make_pair((high), (low))
#define At(bit) Rng((bit), (bit))

struct Concat {
  size_t len;
  dword_t value;
  constexpr explicit Concat(size_t l, dword_t v) : len(l), value(v) {}
  constexpr Concat(word_t v) : len(32), value(v) {}
  constexpr Concat(dword_t v) : len(64), value(v) {}
  constexpr Concat(std::initializer_list<Concat> list) : len(0), value(0) {
    size_t shift = 0;
    for (auto it = list.end(); it != list.begin();) {
      --it;
      len += it->len;
      value |= (it->value << shift);
      shift += it->len;
    }
  }
  constexpr operator dword_t() const { return value; }
};

template <size_t N> struct Bits {
  // std::bitset<N> value;
  using dword = dword_t;
  dword value;
  constexpr Bits(dword value = 0) : value(value) {}
  constexpr operator dword() const { return value; }
  constexpr operator Concat() const { return Concat(N, value); }

  Bits(const Concat &c) : value(c.value) {}

	template <size_t M>
	Bits(const Bits<M> &other) : value(other.value) {
	}

  Bits<MXLEN> sign_extend() const { return Bits<MXLEN>(::sext(value, N)); }
	template <size_t shamt>
  Bits<N+shamt> left_pad() const { return Bits<N+shamt>(value << shamt); }
  bool operator[](size_t bitidx) const { return (value >> bitidx) & 0x1; }

	bool _eq(uint64_t rhs) const {
		if constexpr (N <= MXLEN) {
			return (word_t)value == (word_t)rhs;
		} else {
			return value == rhs;
		}
	}

	bool operator==(const Bits<N> &rhs) const {
		return _eq(rhs.value);
	}
	bool operator==(int rhs) const {
		return _eq(rhs);
	}
	bool operator==(Concat rhs) const {
		return _eq(rhs.value);
	}

};

inline bool operator<(word_t lhs, const Bits<MXLEN> &rhs){
	return lhs < (word_t)rhs.value;
}
inline bool operator<(const Bits<MXLEN> &lhs, const Bits<MXLEN> &rhs){
	return lhs.value < rhs.value;
}

using XReg = Bits<MXLEN>;

template <size_t N> inline sword_t as_signed(const Bits<N> &bits) {
  return sext(bits.value, N);
}
inline sword_t as_signed(word_t value) { return static_cast<sword_t>(value); }

template <size_t N> struct Repl {
  dword_t value;
  operator Bits<N>() const { return Bits<N>(value); }
  operator Concat() const { return Concat(N, value); }
  template <size_t M> Repl(const Bits<M> &bits) {
    value = 0;
    for (size_t i = 0; i < N; i += M) {
      value <<= M;
      value |= bits.value;
    }
  }
};

extern "C" word_t vaddr_read(word_t address, int size);
extern "C" void vaddr_write(word_t address, int size, word_t value);

template <size_t N> word_t read_memory(uint32_t address, int encoding) {
  static_assert(N % 8 == 0, "read_memory size must be multiple of 8");
  return vaddr_read(address, N / 8);
}
template <size_t N>
void write_memory(uint32_t address, uint32_t value, int encoding) {
  static_assert(N % 8 == 0, "write_memory size must be multiple of 8");
  vaddr_write(address, N / 8, value);
}
