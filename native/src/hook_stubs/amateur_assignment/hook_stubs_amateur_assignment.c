#include "hook_stubs_amateur_assignment.h"
#include <string.h>
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../patch_helpers/patch_helpers.h"

uint8_t* build_kbo_amateur_assignment_batch_probe_stub(void* continuation)
{
    uint8_t code[144] = {
        0x4C, 0x8B, 0x65, 0xC8,                         /* mov r12, [rbp-0x38] */
        0x44, 0x8B, 0x75, 0xD4,                         /* mov r14d, [rbp-0x2c] */
        0x50,                                           /* push rax */
        0x51,                                           /* push rcx */
        0x52,                                           /* push rdx */
        0x41, 0x50,                                     /* push r8 */
        0x41, 0x51,                                     /* push r9 */
        0x41, 0x52,                                     /* push r10 */
        0x41, 0x53,                                     /* push r11 */
        0x48, 0x83, 0xEC, 0x28,                         /* sub rsp, 0x28 */
        0x4C, 0x89, 0xE1,                               /* mov rcx, r12 */
        0x44, 0x89, 0xF2,                               /* mov edx, r14d */
        0x49, 0x89, 0xF8,                               /* mov r8, rdi */
        0x48, 0xB8,                                     /* mov rax, probe */
        0,0,0,0,0,0,0,0,
        0xFF, 0xD0,                                     /* call rax */
        0x48, 0x83, 0xC4, 0x28,                         /* add rsp, 0x28 */
        0x41, 0x5B,                                     /* pop r11 */
        0x41, 0x5A,                                     /* pop r10 */
        0x41, 0x59,                                     /* pop r9 */
        0x41, 0x58,                                     /* pop r8 */
        0x5A,                                           /* pop rdx */
        0x59,                                           /* pop rcx */
        0x58,                                           /* pop rax */
        0x45, 0x84, 0xFF,                               /* test r15b, r15b */
        0x74, 0x0C,                                     /* je fallthrough */
        0x48, 0xB8,                                     /* mov rax, original jne target */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0x48, 0xB8,                                     /* mov rax, continuation */
        0,0,0,0,0,0,0,0,
        0xFF, 0xE0,                                     /* jmp rax */
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xCC, 0xCC
    };

    write_u64(&code[34], (uint64_t)(uintptr_t)&ootp_kbo_amateur_assignment_batch_probe);
    write_u64(&code[66], (uint64_t)((uintptr_t)continuation + 0xCFu));
    write_u64(&code[78], (uint64_t)(uintptr_t)continuation);

    uint8_t* memory = (uint8_t*)VirtualAlloc(NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (memory == NULL) {
        return NULL;
    }
    memcpy(memory, code, sizeof(code));
    FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
    return memory;
}
