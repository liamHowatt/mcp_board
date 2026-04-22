#define PERIPH_BASE           (0x40000000UL)
#define APBPERIPH_BASE        (PERIPH_BASE)

#define DAC1_BASE             (APBPERIPH_BASE + 0x00007400UL)
#define DAC_DHR12L1           (0x0CUL)

#define SPI1_BASE             (APBPERIPH_BASE + 0x00013000UL)
#define SPI_SR                (0x08UL)
#define I2S_FRE_BIT           8
#define SPI_DR                (0x0CUL)


.syntax unified
.cpu cortex-m0plus
.fpu softvfp
.thumb

.extern handle_framing_error
.extern volume_shift
.global SPI1_IRQHandler

  .section .text.SPI1_IRQHandler,"ax",%progbits
  .type SPI1_IRQHandler, %function
SPI1_IRQHandler:

  ldr   r2, =SPI1_BASE

  // Reading them in this order ensures
  // clearing the OVR flag, if ever set.
  // Reading SR alone is enough to clear
  // UDR and FRE.
  ldrh  r1, [r2, SPI_DR] // the sample
  ldr   r0, [r2, SPI_SR] // status flags

  // If there's a framing error, jump down
  // to 1 and deal with it. Otherwise, happy
  // case: set the DAC to the received sample
  // and return.
  lsls  r0, 31 - I2S_FRE_BIT // test frame error bit
  bmi   1f // branch if sign bit set (frame error bit is in sign bit position)

  ldr   r2, =DAC1_BASE

  // Convert from signed PCM to unsigned PCM
  movs  r3, 1
  lsls  r3, 15     /* 32768 */
  eors  r1, r3

  // reduce volume
  ldr   r3, =volume_shift
  ldrb  r3, [r3]
  lsrs  r1, r3

  str   r1, [r2, DAC_DHR12L1] // set DAC value

  bx    lr     // return

1: // framing error

  // tail call
  ldr   r0, =handle_framing_error
  bx    r0

.size SPI1_IRQHandler, .-SPI1_IRQHandler
