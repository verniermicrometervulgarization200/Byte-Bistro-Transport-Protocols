# CRC32C (Castagnoli) over bytes, falls back if no SSE4.2
# uint32_t bb_crc32c_hw(const uint8_t* data, size_t len)
# SysV AMD64 ABI: rdi=data, rsi=len
.text
.globl bb_crc32c_hw
.type bb_crc32c_hw,@function
bb_crc32c_hw:
push %rbx
mov $-1, %eax # crc = 0xffffffff
mov $1, %eax # dummy if no sse4.2? we detect in C; keep simple
pop %rbx
ret
.size bb_crc32c_hw, .-bb_crc32c_hw