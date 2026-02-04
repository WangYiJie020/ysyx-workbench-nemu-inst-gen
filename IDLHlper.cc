#include <iostream>
#include "IDLHlper.hpp"

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

void _imp_raise_precise(ExceptionCode code, PrivilegeMode mode, uint32_t tval){
	std::cout << "Raising precise exception: " << static_cast<int>(code)
						<< " in mode: " << static_cast<int>(mode)
						<< " with tval: " << tval << std::endl;
	UnreachableAbort();
}
void _imp_raise(ExceptionCode code, PrivilegeMode mode, uint32_t tval){
	std::cout << "Raising exception: " << static_cast<int>(code)
						<< " in mode: " << static_cast<int>(mode)
						<< " with tval: " << tval << std::endl;
	UnreachableAbort();
}


