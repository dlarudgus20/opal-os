#ifndef OPAL_PLATFORM_PC_X64_DRIVERS_PIC_H
#define OPAL_PLATFORM_PC_X64_DRIVERS_PIC_H

#include <stdint.h>

#include <opal/platform/interrupt.h>

#define PIC_IRQ_TIMER       0
#define PIC_IRQ_KEYBOARD    1
#define PIC_IRQ_SLAVE       2
#define PIC_IRQ_SERIAL1     3
#define PIC_IRQ_SERIAL2     4
#define PIC_IRQ_PARALLEL1   5
#define PIC_IRQ_FLOPPY      6
#define PIC_IRQ_PARALLEL2   7
#define PIC_IRQ_RTC         8
#define PIC_IRQ_MOUSE       12
#define PIC_IRQ_COPROC      13
#define PIC_IRQ_HDD1        14
#define PIC_IRQ_HDD2        15
#define PIC_IRQ_COUNT       16

#define PIC_INT_VECTOR      0x20

void pic_init(void);
void pic_disable_irq(uint8_t irq);
void pic_enable_irq(uint8_t irq);
void pic_enable_intr(void);
void pic_send_eoi(uint8_t irq);

void isr_irq0();
void isr_irq1();
void isr_irq2();
void isr_irq3();
void isr_irq4();
void isr_irq5();
void isr_irq6();
void isr_irq7();
void isr_irq8();
void isr_irq9();
void isr_irq10();
void isr_irq11();
void isr_irq12();
void isr_irq13();
void isr_irq14();
void isr_irq15();

void isr_impl_irq0(struct isr_stackframe* frame);
void isr_impl_irq1(struct isr_stackframe* frame);
void isr_impl_irq2(struct isr_stackframe* frame);
void isr_impl_irq3(struct isr_stackframe* frame);
void isr_impl_irq4(struct isr_stackframe* frame);
void isr_impl_irq5(struct isr_stackframe* frame);
void isr_impl_irq6(struct isr_stackframe* frame);
void isr_impl_irq7(struct isr_stackframe* frame);
void isr_impl_irq8(struct isr_stackframe* frame);
void isr_impl_irq9(struct isr_stackframe* frame);
void isr_impl_irq10(struct isr_stackframe* frame);
void isr_impl_irq11(struct isr_stackframe* frame);
void isr_impl_irq12(struct isr_stackframe* frame);
void isr_impl_irq13(struct isr_stackframe* frame);
void isr_impl_irq14(struct isr_stackframe* frame);
void isr_impl_irq15(struct isr_stackframe* frame);

#endif
