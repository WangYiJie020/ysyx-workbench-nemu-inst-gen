#include <algorithm>
#include <assert.h>
#include <bitset>
#include <functional>
#include <charconv>
#include <string_view>
#include <initializer_list>
#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "encoding.out.h"

#define UnreachableAbort() perror("Unreachable code executed"), assert(0);

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

void raise_precise(ExceptionCode code, PrivilegeMode mode, uint32_t tval);
void raise(ExceptionCode code, PrivilegeMode mode, uint32_t tval);

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

// wrap IDL call outer functions

extern "C" word_t vaddr_read(word_t address, int size);
extern "C" void vaddr_write(word_t address, int size, word_t value);

template <size_t N> word_t read_memory(uint32_t address, int encoding) {
  static_assert(N % 8 == 0);
  return vaddr_read(address, N / 8);
}
template <size_t N>
void write_memory(uint32_t address, uint32_t value, int encoding) {
  static_assert(N % 8 == 0);
  vaddr_write(address, N / 8, value);
}

extern "C" void wait_for_interrupt();
inline void wfi() { wait_for_interrupt(); }

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

inline sword_t sext(word_t value, int width) {
#if MXLEN == 32
  return static_cast<sword_t>(sext32(static_cast<uint32_t>(value), width));
#else
#endif
}

inline dword_t selbits(dword_t value, int high, int low) {
  if (high >= (sizeof(dword_t) * 8 - 1) && low == 0)
    return value;
  dword_t mask = ((WIDER_UNIT << (high - low + 1)) - 1) << low;
  return (value & mask) >> low;
}

template <typename Func> struct _OpFuncTraits {};
template <typename R, typename L, typename R2> struct _OpFuncTraits<R(L, R2)> {
  using RawFn = R(L, R2);
  using FnPtr = R (*)(L, R2);

  using Left = std::remove_reference_t<L>;
  using Right = std::remove_reference_t<R2>;
  using Ret = R;
};

template <typename R, typename L, typename R2>
struct _OpFuncTraits<R (*)(L, R2)> : _OpFuncTraits<R(L, R2)> {};

template <typename Func> struct _OpFunc_Wrap {
  using traits = _OpFuncTraits<Func>;
  using Fn = typename traits::FnPtr;
  using L = typename traits::Left;
  using R = typename traits::Right;
  using Ret = typename traits::Ret;
  Fn func;
  L left;
  _OpFunc_Wrap(Fn f) : func(f) {}
  Ret operator*(R right) { return func(left, right); }
  Ret operator>>(R right) { return func(left, right); }
};
template <typename Func>
inline _OpFunc_Wrap<Func> operator*(typename _OpFuncTraits<Func>::Left left,
                                    _OpFunc_Wrap<Func> &&wrapper) {
  wrapper.left = left;
  return wrapper;
}

#define _Wrap(func) _OpFunc_Wrap<decltype(func)>(func)

inline uint32_t _op_sra32(uint32_t value, uint32_t shamt) {
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
#define Sra *_Wrap(_op_sra32) >>

inline auto _op_selbits(dword_t value, std::pair<int, int> rng) {
  return selbits(value, rng.first, rng.second);
}
#define Rng(high, low) _Wrap(_op_selbits) * std::make_pair((high), (low))
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
	static constexpr size_t width = N;
  dword_t value;
  constexpr Bits(dword_t value = 0) : value(value) {}
  constexpr operator dword_t() const { return value; }
  constexpr operator Concat() const { return Concat(N, value); }

  Bits(const Concat &c) : value(c.value) {}

  template <size_t M> Bits(const Bits<M> &other) : value(other.value) {}

  Bits<MXLEN> sign_extend() const { return Bits<MXLEN>(sext(value, N)); }
  template <size_t shamt> Bits<N + shamt> lshift_extend() const {
    return Bits<N + shamt>(value << shamt);
  }
  bool operator[](size_t bitidx) const { return (value >> bitidx) & 0x1; }
};

using XReg = Bits<MXLEN>;

template <size_t N> inline sword_t sext(const Bits<N> &bits) {
  return sext(bits.value, N);
}
inline XReg sext(word_t value) { return XReg(sext(value, MXLEN)); }
inline Concat sext(const Concat &c){
	return Concat(MXLEN, sext(c.value, c.len));
}

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


