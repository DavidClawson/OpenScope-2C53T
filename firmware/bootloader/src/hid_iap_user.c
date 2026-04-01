/*
 * OpenScope 2C53T Bootloader - HID IAP user implementation
 * Adapted from ArteryTek SDK hid_iap_user.c
 *
 * Changes from SDK:
 *   - FLASH_APP_ADDRESS = 0x08002000 (was 0x08005000)
 *   - Flag address auto-calculated as app_address - sector_size
 *   - jump_to_app disables more peripherals for clean handoff
 */

#include "hid_iap_user.h"
#include "hid_iap_class.h"
#include "string.h"

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
void iap_respond(uint8_t *res_buf, uint16_t iap_cmd, uint16_t result);
uint32_t stkptr, jumpaddr;

#if defined (__GNUC__)
  __attribute__((optimize("O0")))
#endif
void jump_to_app(uint32_t address)
{
  uint32_t sp = *(uint32_t *)address;
  uint32_t rv = *(uint32_t *)(address + sizeof(uint32_t));

  /* Validate: SP should be in SRAM, reset vector in flash */
  if ((sp & 0xFFF00000) != 0x20000000)
    return;
  if (rv < 0x08002000 || rv > 0x08100000)
    return;

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

  __set_MSP(stkptr);
  pftarget = (void (*)(void))jumpaddr;
  pftarget();
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

void iap_erase_sector(uint32_t address)
{
  flash_unlock();
  if (iap_info.sector_size == SECTOR_SIZE_1K)
  {
    flash_sector_erase(address);
  }
  else if (iap_info.sector_size == SECTOR_SIZE_2K)
  {
    if ((address & (SECTOR_SIZE_2K - 1)) == 0)
      flash_sector_erase(address);
  }
  else if (iap_info.sector_size == SECTOR_SIZE_4K)
  {
    if ((address & (SECTOR_SIZE_4K - 1)) == 0)
      flash_sector_erase(address);
  }
  flash_lock();
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

  iap_info.fifo_length = 0;
  iap_info.iap_address = 0;
}

void iap_idle(void)
{
  iap_info.state = IAP_STS_START;
  iap_init();
  iap_respond(iap_info.iap_tx, IAP_CMD_IDLE, IAP_ACK);
}

void iap_start(void)
{
  iap_info.state = IAP_STS_START;
  iap_init();
  iap_respond(iap_info.iap_tx, IAP_CMD_START, IAP_ACK);
}

iap_result_type iap_address(uint8_t *pdata, uint32_t len)
{
  iap_result_type status = IAP_SUCCESS;
  uint16_t result = IAP_ACK;
  uint8_t *paddr = pdata + 2;
  uint32_t address;

  address = (paddr[0] << 24) | (paddr[1] << 16) | (paddr[2] << 8) | paddr[3];

  if (address < iap_info.app_address || address > iap_info.flash_end_address)
  {
    status = IAP_FAILED;
    result = IAP_NACK;
  }
  else
  {
    iap_info.iap_address = address;
    if (iap_info.state == IAP_STS_START)
      iap_clear_upgrade_flag();
    iap_erase_sector(iap_info.iap_address);
  }

  iap_info.state = IAP_STS_ADDR;
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
  crc_nk = (paddr[0] << 16) | paddr[1];
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
}

void iap_loop(void)
{
  if (iap_info.state == IAP_STS_JMP)
  {
    usb_delay_ms(100);
    /* Clean reset — bootloader will find upgrade flag and jump to app */
    NVIC_SystemReset();
  }
}
