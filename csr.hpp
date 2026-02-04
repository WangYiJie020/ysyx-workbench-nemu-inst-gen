#include "IDLHlper.hpp"

dword_t *csr_values();

template <int addr, int high, int low = high> class CSRField {
private:
public:
  dword_t _get() const { return (bits() & mask) >> low; }
  void _set(dword_t v) { bits() = (bits() & inv_mask) | ((v << low) & mask); }

  static constexpr int width = high - low + 1;
  static constexpr dword_t mask = ((dword_t(1) << width) - 1) << low;
  static constexpr dword_t inv_mask = ~mask;

  static dword_t &bits() { return ::csr_values()[addr]; }

  CSRField operator=(const Bits<width> &val) {
    _set(val.value);
    return *this;
  }
  template <int h2, int l2,
            typename = std::enable_if_t<(width == CSRField<h2, l2>::width)>>
  CSRField &operator=(const CSRField<h2, l2> &rhs) {
    _set(rhs._get());
    return *this;
  }

  Bits<width> operator()() { return Bits<width>(_get()); }
  bool operator==(const Bits<width> &val) {
    assert((val.value >> width) == 0);
    return _get() == val.value;
  }
  bool operator!=(const Bits<width> &val) { return !(*this == val); }
};

struct CSRRef {
  int addr;
  dword_t &bits() const { return csr_values()[addr]; }
  dword_t sw_read() const { return bits(); }
  void sw_write(dword_t value) { bits() = value; }

  static CSRField<CSR_MSTATUS, 12, 11> MPP;
  static CSRField<CSR_MSTATUS, 17> MPRV;
  static CSRField<CSR_MSTATUS, 3> MIE;
  static CSRField<CSR_MSTATUS, 7> MPIE;
  static CSRField<CSR_MSTATUS, 42> MDT;
  static CSRField<CSR_MSTATUS, 21> TW;

  static CSRField<CSR_MISA, 12> M;
  static CSRField<CSR_MISA, 18> S;
  static CSRField<CSR_MISA, 7> H;

  static CSRField<CSR_HSTATUS, 21> VTW;
};
inline dword_t bits(const CSRRef &csr) { return csr.bits(); }
struct CSRInfo {
  bool valid;
  bool writable;
  PrivilegeMode mode;

  int addr;
};
using Csr = CSRInfo;
CSRInfo *csr_infos();
CSRRef *csr_table();

inline CSRInfo direct_csr_lookup(Bits<12> addr) {
  return ::csr_infos()[addr.value];
}

inline void unimplemented_csr(int encoding) {
  perror("Unimplemented CSR accessed");
  assert(0);
}

inline dword_t csr_sw_read(const CSRInfo &csr_info) {
  return csr_table()[csr_info.addr].sw_read();
}
inline void csr_sw_write(const CSRInfo &csr_info, dword_t value) {
  csr_table()[csr_info.addr].sw_write(value);
}
