#include <iostream>
#include "IDLHlper.hpp"

CSRRef CSR[4096];

PrivilegeMode g_current_privilege_mode = PrivilegeMode::M;

PrivilegeMode mode() {
	return g_current_privilege_mode;
}
void set_mode(PrivilegeMode mode) {
	std::cout << "Switching to mode: " << static_cast<int>(mode) << std::endl;
	g_current_privilege_mode = mode;
}



bool compatible_mode(PrivilegeMode target_mode, PrivilegeMode actual_mode) {
  if (target_mode == PrivilegeMode::M) {
    return actual_mode == PrivilegeMode::M;
  } else if (target_mode == PrivilegeMode::S) {
    return (actual_mode == PrivilegeMode::M) ||
           (actual_mode == PrivilegeMode::S);
  } else if (target_mode == PrivilegeMode::U) {
    return (actual_mode == PrivilegeMode::M) ||
           (actual_mode == PrivilegeMode::S) ||
           (actual_mode == PrivilegeMode::U);
  } else if (target_mode == PrivilegeMode::VS) {
    return (actual_mode == PrivilegeMode::M) ||
           (actual_mode == PrivilegeMode::S) ||
           (actual_mode == PrivilegeMode::VS);
  } else if (target_mode == PrivilegeMode::VU) {
    return (actual_mode == PrivilegeMode::M) ||
           (actual_mode == PrivilegeMode::S) ||
           (actual_mode == PrivilegeMode::VS) ||
           (actual_mode == PrivilegeMode::VU);
  }
  UnreachableAbort();
  return false;
}

dword_t *csr_values() {
  static dword_t values[4096];
  return values;
}
CSRInfo *csr_infos() {
  static CSRInfo infos[4096];
  return infos;
}
CSRRef* csr_table() {
	return CSR;
}

#define _SETUP_CSR(name, writeable, mode)                                      \
  info[CSR_##name] = {true, bool(writeable), mode, CSR_##name};

enum class AccessType { ReadOnly, RW };

void initialize_csr() {
	dword_t *value = csr_values();
  CSRInfo *info = csr_infos();
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  for (int i = 0; i < 4096; ++i)
    info[i].addr = i;
  _SETUP_CSR(MVENDORID, AccessType::ReadOnly, PrivilegeMode::M)
  _SETUP_CSR(MARCHID, AccessType::ReadOnly, PrivilegeMode::M)
  _SETUP_CSR(MSTATUS, AccessType::RW, PrivilegeMode::M)
  _SETUP_CSR(MEPC, AccessType::RW, PrivilegeMode::M)
	_SETUP_CSR(MTVEC, AccessType::RW, PrivilegeMode::M)
  _SETUP_CSR(MCAUSE, AccessType::RW, PrivilegeMode::M)

	value[CSR_MVENDORID] = 0x79737978;
	value[CSR_MARCHID] = 25100261;
	value[CSR_MSTATUS] = 0x1800;
}
