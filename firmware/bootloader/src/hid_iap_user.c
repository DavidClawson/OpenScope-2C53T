/*
 * OpenScope 2C53T Bootloader - HID IAP user implementation
 * Adapted from ArteryTek SDK hid_iap_user.c
 *
 * Changes from SDK:
 *   - FLASH_APP_ADDRESS = 0x08004000 (SDK default was 0x08005000);
 *     see hid_iap_user.h and firmware/ld/at32f403a_app.ld, which must agree
 *   - Flag address auto-calculated as app_address - sector_size
 *   - jump_to_app disables more peripherals for clean handoff
 */

#include "hid_iap_user.h"
#include "hid_iap_class.h"
#include "string.h"

#define AT32_ROM_DFU_ADDRESS 0x1FFFB000u
#ifndef IAP_CMD_DFU
#define IAP_CMD_DFU 0x5AA8u
#endif
#define IAP_CMD_LOW_FLASH 0x5AA9u
#define IAP_CMD_RUN_ADDR  0x5AAAu
#define IAP_CMD_READ_MEM  0x5AABu
#define IAP_LOW_FLASH_MAGIC 0x4C4F5746u
#define IAP_READ_MEM_MAX  59u
#define SRAM_BASE_ADDRESS 0x20000000u
#define SRAM_END_ADDRESS  0x20038000u
#define IAP_ERASED_SECTOR_INVALID 0xFFFFFFFFu

void (*pftarget)(void);
void iap_clear_upgrade_flag(void);
void iap_set_upgrade_flag(void);
uint32_t crc_cal(uint32_t addr, uint16_t nk);

void iap_idle(void);
void iap_start(void);
iap_result_type iap_address(uint8_t *pdata, uint32_t len);
void iap_finish(void);
iap_result_type iap_data_write(uint8_t *pdata, uint32_t len);
void iap_jump(void);
void iap_dfu(void);
void iap_low_flash(uint8_t *pdata, uint32_t len);
void iap_run_addr(uint8_t *pdata, uint32_t len);
void iap_read_mem(uint8_t *pdata, uint32_t len);
void iap_respond(uint8_t *res_buf, uint16_t iap_cmd, uint16_t result);
uint32_t stkptr, jumpaddr;
static volatile uint8_t rom_dfu_wait;
static volatile uint8_t rom_dfu_enter;
static uint32_t iap_write_start;
static uint32_t iap_write_end;
static uint8_t iap_native_write;
static volatile uint8_t run_addr_wait;
static volatile uint8_t run_addr_enter;
static uint32_t run_addr_target;
static uint32_t iap_erased_sector_address;

/* Defined in main.c — draw transfer state on bootloader LCD */
extern void lcd_draw_iap_status(const char *status);

void jump_to_app(uint32_t address)
{
  uint32_t sp = *(uint32_t *)address;
  uint32_t rv = *(uint32_t *)(address + sizeof(uint32_t));

  /* Validate: SP should be in SRAM, reset vector in flash */
  if ((sp & 0xFFF00000) != 0x20000000) {
    return;
  }
  if (rv < 0x08002000 || rv > 0x08100000) {
    return;
  }

  stkptr = sp;
  jumpaddr = rv;

  /* Disable USB interrupt and peripheral */
  nvic_irq_disable(USBFS_L_CAN1_RX0_IRQn);
  __NVIC_ClearPendingIRQ(USBFS_L_CAN1_RX0_IRQn);

  /* Disable SysTick */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL  = 0;

  /* Reset USB peripheral for clean state */
  crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, FALSE);
  crm_periph_reset(CRM_USB_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_USB_PERIPH_RESET, FALSE);

  /* Disable all NVIC interrupts */
  for (int i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  SCB->VTOR = address;
  pftarget = (void (*)(void))rv;
  __set_MSP(sp);
  pftarget();

  while (1) { __NOP(); }
}

static void jump_to_rom_dfu(void) __attribute__((noreturn));

static void jump_to_rom_dfu(void)
{
  uint32_t sp = *(uint32_t *)AT32_ROM_DFU_ADDRESS;
  uint32_t rv = *(uint32_t *)(AT32_ROM_DFU_ADDRESS + sizeof(uint32_t));

  /* Keep PC9 power hold asserted while handing control to the silicon ROM. */
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
  GPIOC->cfghr = (GPIOC->cfghr & ~(0xFU << 4)) | (0x3U << 4);
  GPIOC->scr = (1U << 9);

  __disable_irq();
  nvic_irq_disable(USBFS_L_CAN1_RX0_IRQn);
  __NVIC_ClearPendingIRQ(USBFS_L_CAN1_RX0_IRQn);

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL  = 0;

  crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, FALSE);
  crm_periph_reset(CRM_USB_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_USB_PERIPH_RESET, FALSE);

  for (int i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  SCB->VTOR = AT32_ROM_DFU_ADDRESS;
  __enable_irq();
  __set_MSP(sp);
  ((void (*)(void))rv)();

  while (1) { __NOP(); }
}

void iap_clear_upgrade_flag(void)
{
  flash_unlock();
  flash_sector_erase(iap_info.flag_address);
  flash_lock();
}

void iap_set_upgrade_flag(void)
{
  flash_unlock();
  flash_word_program(iap_info.flag_address, IAP_UPGRADE_COMPLETE_FLAG);
  flash_lock();
}

iap_result_type iap_get_upgrade_flag(void)
{
  uint32_t flag_address = iap_info.flag_address;
  if (*((uint32_t *)flag_address) == IAP_UPGRADE_COMPLETE_FLAG)
    return IAP_SUCCESS;
  return IAP_FAILED;
}

static uint32_t iap_erase_sector_base(uint32_t address)
{
  if (iap_info.sector_size == SECTOR_SIZE_1K)
    return address;
  if (iap_info.sector_size == SECTOR_SIZE_2K)
    return address & ~(SECTOR_SIZE_2K - 1);
  if (iap_info.sector_size == SECTOR_SIZE_4K)
    return address & ~(SECTOR_SIZE_4K - 1);
  return IAP_ERASED_SECTOR_INVALID;
}

static uint8_t iap_write_address_valid(uint32_t address)
{
  if (address & (HID_IAP_BUFFER_LEN - 1))
    return 0;
  if (iap_info.sector_size == SECTOR_SIZE_1K)
    return 1;
  if (iap_info.sector_size == SECTOR_SIZE_2K)
    return 1;
  if (iap_info.sector_size == SECTOR_SIZE_4K)
    return 1;
  return 0;
}

static uint8_t iap_erase_required(uint32_t address)
{
  if (iap_info.sector_size == SECTOR_SIZE_1K)
    return 1;
  if (iap_info.sector_size == SECTOR_SIZE_2K)
    return (address & (SECTOR_SIZE_2K - 1)) == 0;
  if (iap_info.sector_size == SECTOR_SIZE_4K)
    return (address & (SECTOR_SIZE_4K - 1)) == 0;
  return 0;
}

iap_result_type iap_erase_sector(uint32_t address)
{
  if (!iap_erase_required(address))
    return IAP_FAILED;

  flash_unlock();
  if (iap_info.sector_size == SECTOR_SIZE_1K)
  {
    flash_sector_erase(address);
  }
  else if (iap_info.sector_size == SECTOR_SIZE_2K)
  {
    flash_sector_erase(address);
  }
  else if (iap_info.sector_size == SECTOR_SIZE_4K)
  {
    flash_sector_erase(address);
  }
  flash_lock();
  iap_erased_sector_address = iap_erase_sector_base(address);
  return IAP_SUCCESS;
}

uint32_t crc_cal(uint32_t addr, uint16_t nk)
{
  uint32_t *paddr = (uint32_t *)addr;
  uint32_t wlen = nk * 1024 / sizeof(uint32_t);
  uint32_t value, i_index = 0;
  crm_periph_clock_enable(CRM_CRC_PERIPH_CLOCK, TRUE);
  crc_data_reset();
  for (i_index = 0; i_index < wlen; i_index++)
  {
    value = *paddr;
    crc_one_word_calculate(CONVERT_ENDIAN(value));
    paddr++;
  }
  return crc_data_get();
}

void iap_init(void)
{
  iap_info.flash_size = KB_TO_B(FLASH_SIZE_REG());
  if (iap_info.flash_size < KB_TO_B(256))
    iap_info.sector_size = SECTOR_SIZE_1K;
  else
    iap_info.sector_size = SECTOR_SIZE_2K;

  iap_info.flash_start_address = FLASH_BASE;
  iap_info.flash_end_address = iap_info.flash_start_address + iap_info.flash_size;

  iap_info.app_address = FLASH_APP_ADDRESS;
  iap_info.flag_address = iap_info.app_address - iap_info.sector_size;
  iap_write_start = FLASH_APP_ADDRESS;
  iap_write_end = FLASH_APP_END_ADDRESS;
  iap_native_write = 0;

  iap_info.fifo_length = 0;
  iap_info.iap_address = 0;
  iap_erased_sector_address = IAP_ERASED_SECTOR_INVALID;
}

void iap_idle(void)
{
  iap_info.state = IAP_STS_START;
  iap_init();
  lcd_draw_iap_status("Ready for flash");
  iap_respond(iap_info.iap_tx, IAP_CMD_IDLE, IAP_ACK);
}

void iap_start(void)
{
  iap_info.state = IAP_STS_START;
  iap_init();
  lcd_draw_iap_status("Flashing...");
  iap_respond(iap_info.iap_tx, IAP_CMD_START, IAP_ACK);
}

iap_result_type iap_address(uint8_t *pdata, uint32_t len)
{
  iap_result_type status = IAP_SUCCESS;
  uint16_t result = IAP_ACK;
  uint8_t *paddr = pdata + 2;
  uint32_t address;

  address = (paddr[0] << 24) | (paddr[1] << 16) | (paddr[2] << 8) | paddr[3];

  if (address < iap_write_start || address >= iap_write_end)
  {
    status = IAP_FAILED;
    result = IAP_NACK;
  }
  else
  {
    uint8_t clear_upgrade_flag = (iap_info.state == IAP_STS_START && !iap_native_write);
    uint8_t erase_needed = iap_erase_required(address);
    uint32_t sector_base = iap_erase_sector_base(address);
    uint8_t same_erased_sector =
      sector_base != IAP_ERASED_SECTOR_INVALID && iap_erased_sector_address == sector_base;
    if (iap_write_address_valid(address) && (erase_needed || same_erased_sector))
    {
      iap_info.iap_address = address;
      lcd_draw_iap_status("Flashing...");
      if (!erase_needed || iap_erase_sector(address) == IAP_SUCCESS)
      {
        if (clear_upgrade_flag)
          iap_clear_upgrade_flag();
        iap_info.state = IAP_STS_ADDR;
      }
      else
      {
        result = IAP_NACK;
      }
      status = (result == IAP_ACK) ? IAP_SUCCESS : IAP_FAILED;
    }
    else
    {
      status = IAP_FAILED;
      result = IAP_NACK;
    }
  }

  iap_respond(iap_info.iap_tx, IAP_CMD_ADDR, result);
  return status;
}

iap_result_type iap_data_write(uint8_t *pdata, uint32_t len)
{
  uint32_t data_len = (pdata[2] << 8 | pdata[3]);
  uint8_t *valid_data = pdata + 4;
  uint32_t *pbuf;
  uint32_t i_index = 0;

  if (iap_info.state == IAP_STS_ADDR)
  {
    if (data_len + iap_info.fifo_length <= HID_IAP_BUFFER_LEN)
    {
      for (i_index = 0; i_index < data_len; i_index++)
        iap_info.iap_fifo[iap_info.fifo_length++] = valid_data[i_index];
    }
    if (iap_info.fifo_length == HID_IAP_BUFFER_LEN)
    {
      flash_unlock();
      pbuf = (uint32_t *)iap_info.iap_fifo;
      for (i_index = 0; i_index < iap_info.fifo_length / sizeof(uint32_t); i_index++)
      {
        flash_word_program(iap_info.iap_address, *pbuf++);
        iap_info.iap_address += 4;
      }
      flash_lock();
      iap_info.fifo_length = 0;
      iap_info.iap_address = 0;
      iap_respond(iap_info.iap_tx, IAP_CMD_DATA, IAP_ACK);
    }
  }
  else
  {
    iap_respond(iap_info.iap_tx, IAP_CMD_DATA, IAP_NACK);
  }
  return IAP_SUCCESS;
}

void iap_finish(void)
{
  /* Flush any remaining data in the FIFO (last partial block) */
  if (iap_info.fifo_length > 0 && iap_info.state == IAP_STS_ADDR)
  {
    flash_unlock();
    uint32_t *pbuf = (uint32_t *)iap_info.iap_fifo;
    /* Pad to word boundary */
    while (iap_info.fifo_length % 4 != 0)
      iap_info.iap_fifo[iap_info.fifo_length++] = 0xFF;
    for (uint32_t i = 0; i < iap_info.fifo_length / sizeof(uint32_t); i++)
    {
      flash_word_program(iap_info.iap_address, *pbuf++);
      iap_info.iap_address += 4;
    }
    flash_lock();
    iap_info.fifo_length = 0;
  }

  iap_info.state = IAP_STS_FINISH;
  if (!iap_native_write)
    iap_set_upgrade_flag();
  iap_respond(iap_info.iap_tx, IAP_CMD_FINISH, IAP_ACK);
}

void iap_crc(uint8_t *pdata, uint32_t len)
{
  uint8_t *paddr = pdata + 2;
  uint16_t crc_nk;
  uint32_t crc_value;
  uint32_t address = (paddr[0] << 24) | (paddr[1] << 16) | (paddr[2] << 8) | paddr[3];
  paddr = pdata + 6;
  crc_nk = (paddr[0] << 8) | paddr[1];
  lcd_draw_iap_status("Verifying...");
  crc_value = crc_cal(address, crc_nk);

  iap_respond(iap_info.iap_tx, IAP_CMD_CRC, IAP_ACK);
  iap_info.iap_tx[4] = (uint8_t)((crc_value >> 24) & 0xFF);
  iap_info.iap_tx[5] = (uint8_t)((crc_value >> 16) & 0xFF);
  iap_info.iap_tx[6] = (uint8_t)((crc_value >> 8) & 0xFF);
  iap_info.iap_tx[7] = (uint8_t)((crc_value) & 0xFF);
}

void iap_jump(void)
{
  iap_info.state = IAP_STS_JMP_WAIT;
  iap_respond(iap_info.iap_tx, IAP_CMD_JMP, IAP_ACK);
}

void iap_get(void)
{
  iap_respond(iap_info.iap_tx, IAP_CMD_GET, IAP_ACK);
  iap_info.iap_tx[4] = (uint8_t)((iap_info.app_address >> 24) & 0xFF);
  iap_info.iap_tx[5] = (uint8_t)((iap_info.app_address >> 16) & 0xFF);
  iap_info.iap_tx[6] = (uint8_t)((iap_info.app_address >> 8) & 0xFF);
  iap_info.iap_tx[7] = (uint8_t)((iap_info.app_address) & 0xFF);
}

void iap_dfu(void)
{
  rom_dfu_wait = 1;
  iap_respond(iap_info.iap_tx, IAP_CMD_DFU, IAP_ACK);
}

void iap_low_flash(uint8_t *pdata, uint32_t len)
{
  uint16_t result = IAP_NACK;
  uint8_t *paddr = pdata + 2;
  uint32_t magic = 0;

  if (len >= 6)
    magic = (paddr[0] << 24) | (paddr[1] << 16) | (paddr[2] << 8) | paddr[3];

#ifdef HIGH_IAP_ALLOW_LOW_FLASH
  if (magic == IAP_LOW_FLASH_MAGIC && BOOTLOADER_BASE_ADDRESS >= FLASH_APP_END_ADDRESS)
  {
    iap_write_start = FLASH_BASE_ADDRESS;
    iap_write_end = BOOTLOADER_BASE_ADDRESS;
    iap_native_write = 1;
    lcd_draw_iap_status("Low flash...");
    result = IAP_ACK;
  }
#else
  (void)magic;
#endif

  iap_respond(iap_info.iap_tx, IAP_CMD_LOW_FLASH, result);
}

void iap_run_addr(uint8_t *pdata, uint32_t len)
{
  uint16_t result = IAP_NACK;
  uint8_t *paddr = pdata + 2;
  uint32_t address = 0;

  if (len >= 6)
    address = (paddr[0] << 24) | (paddr[1] << 16) | (paddr[2] << 8) | paddr[3];

#ifdef HIGH_IAP_ALLOW_LOW_FLASH
  if (address >= FLASH_BASE_ADDRESS && address < iap_info.flash_end_address)
  {
    run_addr_target = address;
    run_addr_wait = 1;
    lcd_draw_iap_status("Booting target...");
    result = IAP_ACK;
  }
#else
  (void)address;
#endif

  iap_respond(iap_info.iap_tx, IAP_CMD_RUN_ADDR, result);
}

void iap_read_mem(uint8_t *pdata, uint32_t len)
{
  uint16_t result = IAP_NACK;
  uint8_t *paddr = pdata + 2;
  uint32_t address = 0;
  uint16_t read_len = 0;

  if (len >= 8)
  {
    address = (paddr[0] << 24) | (paddr[1] << 16) | (paddr[2] << 8) | paddr[3];
    paddr = pdata + 6;
    read_len = (paddr[0] << 8) | paddr[1];
  }

#ifdef HIGH_IAP_ALLOW_LOW_FLASH
  if (read_len > 0 && read_len <= IAP_READ_MEM_MAX)
  {
    uint32_t end = address + read_len;
    uint8_t in_flash = (address >= FLASH_BASE_ADDRESS && end >= address && end <= iap_info.flash_end_address);
    uint8_t in_sram = (address >= SRAM_BASE_ADDRESS && end >= address && end <= SRAM_END_ADDRESS);

    if (in_flash || in_sram)
    {
      uint8_t *source = (uint8_t *)address;
      iap_respond(iap_info.iap_tx, IAP_CMD_READ_MEM, IAP_ACK);
      iap_info.iap_tx[4] = (uint8_t)read_len;
      for (uint16_t i = 0; i < read_len; i++)
        iap_info.iap_tx[5 + i] = source[i];
      result = IAP_ACK;
    }
  }
#else
  (void)address;
  (void)read_len;
#endif

  if (result != IAP_ACK)
    iap_respond(iap_info.iap_tx, IAP_CMD_READ_MEM, IAP_NACK);
}

void iap_respond(uint8_t *res_buf, uint16_t iap_cmd, uint16_t result)
{
  res_buf[0] = (uint8_t)((iap_cmd >> 8) & 0xFF);
  res_buf[1] = (uint8_t)((iap_cmd) & 0xFF);
  res_buf[2] = (uint8_t)((result >> 8) & 0xFF);
  res_buf[3] = (uint8_t)((result) & 0xFF);
  iap_info.respond_flag = 1;
}

iap_result_type usbd_hid_iap_process(void *udev, uint8_t *pdata, uint16_t len)
{
  iap_result_type status = IAP_SUCCESS;
  uint16_t iap_cmd;

  if (len < 2)
    return IAP_FAILED;

  iap_info.respond_flag = 0;
  iap_cmd = (pdata[0] << 8) | pdata[1];

  switch (iap_cmd)
  {
    case IAP_CMD_IDLE:   iap_idle(); break;
    case IAP_CMD_START:  iap_start(); break;
    case IAP_CMD_ADDR:   iap_address(pdata, len); break;
    case IAP_CMD_DATA:   iap_data_write(pdata, len); break;
    case IAP_CMD_FINISH: iap_finish(); break;
    case IAP_CMD_CRC:    iap_crc(pdata, len); break;
    case IAP_CMD_JMP:    iap_jump(); break;
    case IAP_CMD_GET:    iap_get(); break;
    case IAP_CMD_DFU:    iap_dfu(); break;
    case IAP_CMD_LOW_FLASH: iap_low_flash(pdata, len); break;
    case IAP_CMD_RUN_ADDR:  iap_run_addr(pdata, len); break;
    case IAP_CMD_READ_MEM:  iap_read_mem(pdata, len); break;
    default:             status = IAP_FAILED; break;
  }

  if (iap_info.respond_flag)
    usb_iap_class_send_report(udev, iap_info.iap_tx, 64);

  return status;
}

void usbd_hid_iap_in_complete(void *udev)
{
  if (iap_info.state == IAP_STS_JMP_WAIT)
    iap_info.state = IAP_STS_JMP;
  if (rom_dfu_wait) {
    rom_dfu_wait = 0;
    rom_dfu_enter = 1;
  }
  if (run_addr_wait) {
    run_addr_wait = 0;
    run_addr_enter = 1;
  }
}

void iap_loop(void)
{
  if (iap_info.state == IAP_STS_JMP)
  {
    usb_delay_ms(100);
    lcd_draw_iap_status("Rebooting...");
    usb_delay_ms(200);
    /* Clean reset — bootloader will find upgrade flag and jump to app */
    NVIC_SystemReset();
  }

  if (rom_dfu_enter)
  {
    rom_dfu_enter = 0;
    usb_delay_ms(100);
    lcd_draw_iap_status("DFU Flashing...");
    usb_delay_ms(200);
    jump_to_rom_dfu();
  }

  if (run_addr_enter)
  {
    run_addr_enter = 0;
    usb_delay_ms(100);
    lcd_draw_iap_status("Booting...");
    usb_delay_ms(200);
    jump_to_app(run_addr_target);
  }
}
