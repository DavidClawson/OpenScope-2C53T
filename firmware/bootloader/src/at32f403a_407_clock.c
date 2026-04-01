/*
 * OpenScope 2C53T Bootloader - Clock configuration
 * Same 240MHz PLL config as main app (HEXT 8MHz / 2 * 60 = 240MHz)
 */

#include "at32f403a_407_clock.h"

void system_clock_config(void)
{
  crm_reset();
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE);

  while (crm_hext_stable_wait() == ERROR)
  {
  }

  crm_pll_config(CRM_PLL_SOURCE_HEXT_DIV, CRM_PLL_MULT_60, CRM_PLL_OUTPUT_RANGE_GT72MHZ);
  crm_hext_clock_div_set(CRM_HEXT_DIV_2);
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);

  while (crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)
  {
  }

  crm_ahb_div_set(CRM_AHB_DIV_1);
  crm_apb2_div_set(CRM_APB2_DIV_2);
  crm_apb1_div_set(CRM_APB1_DIV_2);
  crm_auto_step_mode_enable(TRUE);
  crm_sysclk_switch(CRM_SCLK_PLL);

  while (crm_sysclk_switch_status_get() != CRM_SCLK_PLL)
  {
  }

  crm_auto_step_mode_enable(FALSE);
  system_core_clock_update();
}
