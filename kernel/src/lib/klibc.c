#include <klibc.h>
#include <mm/hhdm.h>
#include <mm/frame.h>
#include <mm/page.h>

void *memcpy(void *To, void *From, long Num)
{
    int d0, d1, d2;
    __asm__ __volatile__("cld	\n\t"
                         "rep	\n\t"
                         "movsq	\n\t"
                         "testb	$4,%b4	\n\t"
                         "je	1f	\n\t"
                         "movsl	\n\t"
                         "1:\ttestb	$2,%b4	\n\t"
                         "je	2f	\n\t"
                         "movsw	\n\t"
                         "2:\ttestb	$1,%b4	\n\t"
                         "je	3f	\n\t"
                         "movsb	\n\t"
                         "3:	\n\t"
                         : "=&c"(d0), "=&D"(d1), "=&S"(d2)
                         : "0"(Num / 8), "q"(Num), "1"(To), "2"(From)
                         : "memory");
    return To;
}

void *memset(void *Address, unsigned char C, long Count)
{
    int d0, d1;
    uint64_t tmp = C * 0x0101010101010101UL;
    __asm__ __volatile__("cld	\n\t"
                         "rep	\n\t"
                         "stosq	\n\t"
                         "testb	$4, %b3	\n\t"
                         "je	1f	\n\t"
                         "stosl	\n\t"
                         "1:\ttestb	$2, %b3	\n\t"
                         "je	2f\n\t"
                         "stosw	\n\t"
                         "2:\ttestb	$1, %b3	\n\t"
                         "je	3f	\n\t"
                         "stosb	\n\t"
                         "3:	\n\t"
                         : "=&c"(d0), "=&D"(d1)
                         : "a"(tmp), "q"(Count), "0"(Count / 8), "1"(Address)
                         : "memory");
    return Address;
}

void *alloc_frames_bytes(size_t bytes)
{
    return (void *)phys_to_virt(alloc_frames((bytes + PAGE_SIZE - 1) / PAGE_SIZE));
}
void free_frames_bytes(void *ptr, size_t bytes)
{
    return free_frames(virt_to_phys((uint64_t)ptr), (bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}
