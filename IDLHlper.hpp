#include <algorithm>
#include <assert.h>
#include <bitset>
#include <charconv>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <utility>

#include "encoding.out.h"

#define UnreachableAbort() perror("Unreachable code executed"), assert(0);

// -- Output Instruction Helper --

#define INIT()                                                                 \
  auto X = regs;                                                               \
  FRegFile F(fregs);                                                           \
  idl::FpContextScope fp_scope(fcsr, pc, ENCODING_INST);                       \
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
  F,
  D,
  V,
  N,
  U,
  B,
};
constexpr bool implemented(ExtensionName name) {
  switch (name) {
  case ExtensionName::M:
  case ExtensionName::F:
  case ExtensionName::D:
  case ExtensionName::B:
    return true;
  default:
    return false;
  }
}
enum class PrivilegeMode { M, S, U, VU, VS };
PrivilegeMode mode();
void set_mode(PrivilegeMode mode);
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

void _imp_raise_precise(ExceptionCode code, PrivilegeMode mode, uint32_t tval);
void _imp_raise(ExceptionCode code, PrivilegeMode mode, uint32_t tval);

#define __Dump_Source_Location__() printf("At %s:%d\n", __FILE__, __LINE__)

#define raise_precise(code, mode, tval)                                        \
  __Dump_Source_Location__();                                                  \
  _imp_raise_precise(code, mode, tval)

#define raise(code, mode, tval)                                                \
  __Dump_Source_Location__();                                                  \
  _imp_raise(code, mode, tval)

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

template <size_t N> struct Bits;

enum class RoundingMode : uint8_t {
  RNE = 0,
  RTZ = 1,
  RDN = 2,
  RUP = 3,
  RMM = 4,
};

enum class FpFlag : uint8_t {
  NX = 1,
  UF = 2,
  OF = 4,
  DZ = 8,
  NV = 16,
};

enum class F32MulAddOp : uint8_t {
  Softfloat_mulAdd_addC,
  Softfloat_mulAdd_subC,
  Softfloat_mulAdd_subProd,
};

constexpr uint32_t SP_CANONICAL_NAN = UINT32_C(0x7fc00000);

namespace idl {
class IllegalInstruction {};

class FpContextScope {
public:
  FpContextScope(word_t *fcsr, word_t *pc, word_t instruction);
  ~FpContextScope();
};

void check_f_ok(word_t instruction);
RoundingMode rm_to_mode(uint32_t rm, word_t instruction);
word_t handle_illegal_instruction();
void mark_f_state_dirty();
void set_fp_flag(FpFlag flag);

Bits<64> nan_box(uint32_t narrow_width, uint32_t width, Bits<32> value);
Bits<32> f32_add(Bits<32> a, Bits<32> b, RoundingMode mode);
Bits<32> f32_sub(Bits<32> a, Bits<32> b, RoundingMode mode);
Bits<32> f32_mul(Bits<32> a, Bits<32> b, RoundingMode mode);
Bits<32> f32_div(Bits<32> a, Bits<32> b, RoundingMode mode);
Bits<32> f32_sqrt(Bits<32> a, RoundingMode mode);
Bits<32> f32_muladd(Bits<32> a, Bits<32> b, Bits<32> c,
                    F32MulAddOp op, RoundingMode mode);
int32_t f32_to_i32(Bits<32> a, RoundingMode mode);
uint32_t f32_to_ui32(Bits<32> a, RoundingMode mode);
Bits<32> i32_to_f32(uint32_t a, RoundingMode mode);
Bits<32> ui32_to_f32(uint32_t a, RoundingMode mode);

bool is_sp_neg_inf(Bits<32> value);
bool is_sp_neg_norm(Bits<32> value);
bool is_sp_neg_subnorm(Bits<32> value);
bool is_sp_neg_zero(Bits<32> value);
bool is_sp_pos_zero(Bits<32> value);
bool is_sp_pos_subnorm(Bits<32> value);
bool is_sp_pos_norm(Bits<32> value);
bool is_sp_pos_inf(Bits<32> value);
bool is_sp_signaling_nan(Bits<32> value);
bool is_sp_quiet_nan(Bits<32> value);
bool is_sp_nan(Bits<32> value);
}

class FRegRef {
  uint64_t *value;

public:
  explicit FRegRef(uint64_t *value) : value(value) {}
  operator dword_t() const { return value[0]; }
  operator Bits<32>() const;

  FRegRef &operator=(dword_t rhs);
  template <size_t N> FRegRef &operator=(Bits<N> rhs);
};

class FRegFile {
  uint64_t *fregs;

public:
  explicit FRegFile(uint64_t *fregs) : fregs(fregs) {}
  FRegRef operator[](size_t index) { return FRegRef(&fregs[index * 2]); }
};

// wrap IDL call outer functions

extern "C" word_t vaddr_read(word_t address, int size);
extern "C" void vaddr_write(word_t address, int size, word_t value);

inline word_t read_memory(int N, uint32_t address, int encoding) {
  assert(N % 8 == 0);
  return vaddr_read(address, N / 8);
}
inline void write_memory(int N, uint32_t address, uint32_t value,
                         int encoding) {
  assert(N % 8 == 0);
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

inline constexpr dword_t selbits(dword_t value, int high, int low) {
  if (high >= (int)(sizeof(dword_t) * 8 - 1) && low == 0)
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

template <typename Ret, typename L, typename R>
struct _OpFuncTraits<Ret (*)(L, R)> : _OpFuncTraits<Ret(L, R)> {};

template <typename Ret, typename L, typename R>
struct _OpFuncTraits<std::function<Ret(L, R)>> : _OpFuncTraits<Ret(L, R)> {};

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
    if (shamt == 0)
      return value;
    if (value & 0x80000000) {
      return (value >> shamt) | (~((uint32_t)0) << (32 - shamt));
    } else {
      return value >> shamt;
    }
  }
}
#define Sra *_Wrap(_op_sra32) >>

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

	static constexpr dword_t mask = (N >= 64) ? ~0ull : ((1ull << N) - 1);

  Bits(const Concat &c) : value(c.value) {}

  template <size_t M> Bits(const Bits<M> &other) : value(other.value) {}

  Bits<MXLEN> sign_extend() const { return Bits<MXLEN>(sext(value, N)); }
  template <size_t shamt> Bits<N + shamt> lshift_extend() const {
    return Bits<N + shamt>(value << shamt);
  }

	struct BitRef {
		dword_t *value;
		size_t bitidx;
		operator bool() const { return (*value >> bitidx) & 0x1; }
		BitRef& operator=(BitRef other){
			return *this = bool(other);
		}
		BitRef& operator=(bool b) {
			if (b) {
				*value |= (1ull << bitidx);
			} else {
				*value &= ~(1ull << bitidx);
			}
			return *this;
		}
	};
  // bool operator[](size_t bitidx) const { return (value >> bitidx) & 0x1; }
	BitRef operator[](size_t bitidx) { return BitRef{&value, bitidx}; }

  bool operator==(const Bits<N> &other) const {
    return (value & mask) == (other.value & mask);
  }
  bool operator==(dword_t other) const {
    return (value & mask) == (other & mask);
  }
  bool operator==(int other) const { return *this == (dword_t)other; }
  bool operator!=(dword_t other) const { return !(*this == other); }
  bool operator!=(int other) const { return !(*this == other); }

	Bits<N> operator~() const {
		return Bits<N>(~value & mask);
	}
};

inline FRegRef::operator Bits<32>() const { return Bits<32>(value[0]); }

inline FRegRef &FRegRef::operator=(dword_t rhs) {
  return *this = Bits<32>(rhs);
}

template <size_t N> FRegRef &FRegRef::operator=(Bits<N> rhs) {
  value[0] = UINT64_C(0xffffffff00000000) | (uint32_t)rhs.value;
  value[1] = UINT64_MAX;
  idl::mark_f_state_dirty();
  return *this;
}

using XReg = Bits<MXLEN>;
inline bool operator==(const XReg &lhs, const Concat &rhs) {
  return lhs.value == rhs.value;
}

inline bool operator<(word_t lhs, const XReg &rhs) {
  return lhs < (word_t)rhs.value;
}
inline bool operator<(const XReg &lhs, const XReg &rhs) {
  return lhs.value < rhs.value;
}

template <size_t high, size_t low, typename RetBits = Bits<high - low + 1>,
          typename Func = std::function<RetBits(dword_t, void *)>>
_OpFunc_Wrap<Func> _MakeRng() {
	return _OpFunc_Wrap<Func>([](dword_t value, void *) {
    return RetBits(selbits(value, high, low));
  });
}

struct DynamicBitsSlice {
	size_t high = 0;
	size_t low = 0;
	dword_t* value = nullptr;
	DynamicBitsSlice() = default;
	DynamicBitsSlice(size_t h, size_t l, dword_t* v = nullptr) : high(h), low(l), value(v) {}
	operator dword_t() const {
		return selbits(*value, high, low);
	}
	operator XReg() const {
		return XReg(selbits(*value, high, low));
	}


	bool operator==(int other) const {
		return selbits(*value, high, low) == (dword_t)other;
	}

	// Get the mask for the slice, with bits set to 1 in the range `[low, high]` and 0 elsewhere
	dword_t mask() const {
		return selbits(~0ull, high, low) << low;
	}
	// Get the value of the slice
	// `value[high-low, 0] = ref[high:low]`
	dword_t get() const {
		return selbits(*value, high, low);
	}

	DynamicBitsSlice operator=(DynamicBitsSlice other) {
		*value = (*value & ~mask()) | (other.get() << low);
		return *this;
	}
	DynamicBitsSlice operator=(dword_t other) {
		*value = (*value & ~mask()) | (selbits(other, high - low, 0) << low);
		return *this;
	}
};

inline DynamicBitsSlice operator*(XReg& v, DynamicBitsSlice slice) {
	slice.value = &v.value;
	return slice;
}

#define DynRng(high, low) DynamicBitsSlice((high), (low))
#define Rng(high, low) _MakeRng<high, low>() * ((void *)0)
#define At(bit) Rng((bit), (bit))

template <size_t N> inline sword_t sext(const Bits<N> &bits) {
  return sext(bits.value, N);
}
inline sword_t sext(word_t value) { return sext(value, MXLEN); }
inline Concat sext(const Concat &c) {
  return Concat(MXLEN, sext(c.value, c.len));
}

inline sword_t highest_set_bit(word_t x) {
  return x == 0 ? -1 : 31 - __builtin_clz(x);
}
inline uint32_t lowest_set_bit(uint32_t value) {
  return value == 0 ? 32 : __builtin_ctz(value);
}
using U32 = uint32_t;

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
  Repl(unsigned long v) {
    assert(v == 0 || v == 1);
    *this = Repl(Bits<1>(v));
  }
};
