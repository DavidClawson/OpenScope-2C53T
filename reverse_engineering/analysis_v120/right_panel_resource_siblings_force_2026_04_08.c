// Force-decompiled functions

// ==========================================
// Function: FUN_0800a66a @ 0800a66a
// Size: 1 bytes
// ==========================================

void FUN_0800a66a(void)

{
  byte bVar1;
  int iVar2;
  int unaff_r4;
  
  if (*(char *)(unaff_r4 + 0xe11) == -1) {
    *(undefined1 *)(unaff_r4 + 0xe10) = 3;
  }
  else {
    FUN_080003b4(&DAT_20008360,0x80bcad2);
    iVar2 = FUN_08036084();
    if (iVar2 == 0) {
      iVar2 = FUN_08035ed4();
      if (iVar2 == 0) {
        *(undefined1 *)(unaff_r4 + 0xe10) = 2;
        bVar1 = *(byte *)(unaff_r4 + 0xe1b);
        *(undefined1 *)(unaff_r4 + (uint)bVar1 + 0xe25) = *(undefined1 *)(unaff_r4 + 0xe11);
        *(byte *)(unaff_r4 + 0xe1b) = bVar1 + 1;
      }
      else {
        *(undefined1 *)(unaff_r4 + 0xe10) = 3;
        FUN_080003b4(&DAT_20008360,0x80bcad2,*(undefined1 *)(unaff_r4 + 0xe11));
        FUN_0802e12c();
      }
    }
    else {
      *(undefined1 *)(unaff_r4 + 0xe10) = 3;
      FUN_0802e12c();
    }
  }
  *(undefined1 *)(unaff_r4 + 0xf68) = 2;
  return;
}



// Attempting to create function at 0x0800ad26
// Successfully created function
// ==========================================
// Function: FUN_0800ad26 @ 0800ad26
// Size: 1 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800ad26(void)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  
  FUN_080003b4();
  FUN_0802a534();
  _DAT_20008350 = 0x11e;
  _DAT_20008352 = 0x14;
  _DAT_20008354 = 0x18;
  _DAT_20008356 = 0x14;
  _DAT_20008358 = FUN_08033cfc(0x3c0);
  if (_DAT_20008358 != 0) {
    FUN_080027e8();
    FUN_080003b4();
    FUN_08008154(0x11e,0x14,0x18,0x14);
    FUN_0803bee0();
    iVar1 = FUN_0803b3a8(_DAT_20002d84,0xffffffff);
    if ((iVar1 == 1) && (_DAT_20008358 != 0)) {
      if (DAT_20001080 == '\0') {
                    /* WARNING: Could not recover jumptable at 0x0800adfa. Too many branches */
                    /* WARNING: Treating indirect jump as call */
        (*_DAT_20001070)(0);
        return;
      }
      if ((uint)(_DAT_20008358 - _DAT_20001078) >> 10 < 0xaf) {
        uVar3 = (uint)(_DAT_20008358 - _DAT_20001078) >> 5;
        uVar2 = (uint)*(ushort *)(_DAT_2000107c + uVar3 * 2);
        if (uVar2 != 0) {
          FUN_080012bc(_DAT_2000107c + uVar3 * 2,uVar2 << 1);
        }
      }
    }
  }
  return;
}



// Attempting to create function at 0x0800afbc
// Successfully created function
// ==========================================
// Function: FUN_0800afbc @ 0800afbc
// Size: 1 bytes
// ==========================================

/* WARNING: Type propagation algorithm not settling */

void FUN_0800afbc(void)

{
  undefined1 uVar1;
  uint uVar2;
  int iVar3;
  uint unaff_r4;
  uint uVar4;
  uint uVar5;
  undefined1 *puVar6;
  int unaff_r8;
  bool bVar7;
  
  puVar6 = (undefined1 *)(unaff_r8 + 0xe2a);
  uVar5 = 0xfffffff8;
  uVar4 = unaff_r4 & 0xffff | 0x20000000;
  do {
    iVar3 = unaff_r8 + uVar5;
    uVar2 = (uint)*(byte *)(iVar3 + 0xe1a);
    if (uVar2 != 0) {
      if ((*(byte *)(iVar3 + 0xe1a) & 1) != 0) {
        FUN_080003b4(uVar4,0x80bcad2,puVar6[-5]);
        FUN_0802e12c();
        FUN_080003b4(uVar4,0x80bc859,puVar6[-5]);
        FUN_0802e12c();
        uVar2 = (uint)*(byte *)(iVar3 + 0xe1a);
      }
      if ((int)(uVar2 << 0x1e) < 0) {
        FUN_080003b4(uVar4,0x80bcad2,puVar6[-4]);
        FUN_0802e12c();
        FUN_080003b4(uVar4,0x80bc859,puVar6[-4]);
        FUN_0802e12c();
        uVar2 = (uint)*(byte *)(iVar3 + 0xe1a);
      }
      if ((int)(uVar2 << 0x1d) < 0) {
        FUN_080003b4(uVar4,0x80bcad2,puVar6[-3]);
        FUN_0802e12c();
        FUN_080003b4(uVar4,0x80bc859,puVar6[-3]);
        FUN_0802e12c();
        uVar2 = (uint)*(byte *)(iVar3 + 0xe1a);
      }
      if ((int)(uVar2 << 0x1c) < 0) {
        FUN_080003b4(uVar4,0x80bcad2,puVar6[-2]);
        FUN_0802e12c();
        FUN_080003b4(uVar4,0x80bc859,puVar6[-2]);
        FUN_0802e12c();
        uVar2 = (uint)*(byte *)(iVar3 + 0xe1a);
      }
      if ((int)(uVar2 << 0x1b) < 0) {
        FUN_080003b4(uVar4,0x80bcad2,puVar6[-1]);
        FUN_0802e12c();
        FUN_080003b4(uVar4,0x80bc859,puVar6[-1]);
        FUN_0802e12c();
        uVar2 = (uint)*(byte *)(iVar3 + 0xe1a);
      }
      if ((int)(uVar2 << 0x1a) < 0) {
        FUN_080003b4(uVar4,0x80bcad2,*puVar6);
        FUN_0802e12c();
        FUN_080003b4(uVar4,0x80bc859,*puVar6);
        FUN_0802e12c();
      }
    }
    bVar7 = uVar5 != 0xffffffff;
    uVar5 = uVar5 + 1;
    puVar6 = puVar6 + 6;
  } while (bVar7);
  uVar1 = FUN_08034878(0x80bc18b);
  *(undefined1 *)(unaff_r8 + 0xe1b) = uVar1;
  *(undefined1 *)(unaff_r8 + 0xe1a) = 0;
  *(undefined2 *)(unaff_r8 + 0xe1c) = 0;
  *(undefined1 *)(unaff_r8 + 0xf68) = 5;
  return;
}



// Attempting to create function at 0x0800b10a
// Successfully created function
// ==========================================
// Function: FUN_0800b10a @ 0800b10a
// Size: 1 bytes
// ==========================================

void FUN_0800b10a(void)

{
  undefined1 uVar1;
  int unaff_r8;
  
  FUN_080003b4();
  FUN_0802e12c();
  FUN_080003b4();
  FUN_0802e12c();
  uVar1 = FUN_08034878(0x80bc18b);
  *(undefined1 *)(unaff_r8 + 0xe1b) = uVar1;
  *(undefined1 *)(unaff_r8 + 0xe1a) = 0;
  *(undefined2 *)(unaff_r8 + 0xe1c) = 0;
  *(undefined1 *)(unaff_r8 + 0xf68) = 5;
  return;
}



