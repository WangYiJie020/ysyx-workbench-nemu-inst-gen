#include <cstdint>
typedef uint32_t word_t;
extern "C" word_t vaddr_read(word_t address, int size) { return 0; }
extern "C" void vaddr_write(word_t address, int size, word_t value) {}
int main() { return 0; }
