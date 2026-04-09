// Force-decompiled functions

// Attempting to create function at 0x0800a66a
// Successfully created function
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



// ==========================================
// Function: FUN_08035ed4 @ 08035ed4
// Size: 432 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined1 FUN_08035ed4(void)

{
  undefined1 local_1d;
  undefined1 local_1c;
  undefined1 local_1b;
  undefined1 local_1a;
  undefined1 local_19;
  int local_18;
  char local_11;
  int local_10;
  undefined1 local_9;
  
  local_10 = FUN_08033cfc(0x102c);
  if (local_10 == 0) {
    local_9 = 1;
  }
  else {
    FUN_08000370(&DAT_20008360,0x80bc859,DAT_20000f09);
    local_11 = FUN_0802d8b8(local_10,&DAT_20008360,10);
    if (local_11 == '\0') {
      local_11 = FUN_0802e530(local_10,0x2000044e,0x25a,&local_18);
      if ((local_11 == '\0') && (local_18 == 0x25a)) {
        if (DAT_2000012c == '\x01') {
          local_1d = 4;
        }
        else {
          local_1d = DAT_2000010c;
        }
        local_1c = DAT_20000ead;
        local_1b = (undefined1)_DAT_20000eac;
        local_1a = DAT_20000eaf;
        local_19 = (undefined1)_DAT_20000eae;
        if (*(int *)(local_10 + 0xc) == -1) {
          FUN_0802d014(local_10);
          FUN_0802e12c();
          FUN_08033cbc(local_10);
          local_9 = 5;
        }
        else {
          local_11 = FUN_0802d1e8(local_10,*(int *)(local_10 + 0xc));
          if (local_11 == '\0') {
            local_11 = FUN_0802e530(local_10,&local_1d,5,&local_18);
            if ((local_11 == '\0') && (local_18 == 5)) {
              FUN_0802d014(local_10);
              FUN_08033cbc(local_10);
              local_9 = 0;
            }
            else {
              FUN_0802d014(local_10);
              FUN_0802e12c();
              FUN_08033cbc(local_10);
              local_9 = 3;
            }
          }
          else {
            FUN_0802d014(local_10);
            FUN_0802e12c();
            FUN_08033cbc(local_10);
            local_9 = 4;
          }
        }
      }
      else {
        FUN_0802d014(local_10);
        FUN_0802e12c();
        FUN_08033cbc(local_10);
        local_9 = 6;
      }
    }
    else {
      FUN_0802e12c();
      FUN_08033cbc(local_10);
      local_9 = 2;
    }
  }
  return local_9;
}



// Attempting to create function at 0x08039058
// Successfully created function
// ==========================================
// Function: FUN_08039058 @ 08039058
// Size: 1 bytes
// ==========================================

void FUN_08039058(void)

{
  byte bVar1;
  int iVar2;
  undefined2 unaff_r5;
  undefined4 *unaff_r6;
  int unaff_r7;
  int unaff_r8;
  uint *unaff_r9;
  uint in_stack_00000004;
  
  do {
    bVar1 = *(byte *)(unaff_r7 + 0xe10);
    if ((bVar1 & 0xfe) == 2) {
      *(char *)(unaff_r7 + 0xe10) = (char)unaff_r5;
code_r0x08039074:
      if (((*(char *)(unaff_r7 + 0x2f) == '\0' && *(char *)(unaff_r7 + 0x30) == '\0') &&
          *(char *)(unaff_r7 + 0x33) == '\0') && *(char *)(unaff_r7 + 0xf2c) == '\0') {
        (**(code **)(unaff_r8 + (in_stack_00000004 >> 0x18) * 4 + -4))();
      }
    }
    else if ((bVar1 != 1) && (bVar1 != 0xff)) goto code_r0x08039074;
    if (*(byte *)(unaff_r7 + 0xf62) != 0) {
      *unaff_r9 = (uint)*(byte *)(unaff_r7 + 0xf62);
    }
    *(undefined2 *)(unaff_r7 + 0xf6c) = unaff_r5;
    do {
      iVar2 = FUN_0803b1d8(*unaff_r6);
    } while (iVar2 != 1);
  } while( true );
}



// Attempting to create function at 0x08039544
// Successfully created function
// ==========================================
// Function: FUN_08039544 @ 08039544
// Size: 1 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_08039544(int param_1,undefined4 param_2,undefined1 param_3)

{
  char cVar1;
  undefined4 uVar2;
  byte bVar3;
  char cVar4;
  short sVar5;
  uint *unaff_r4;
  bool in_ZR;
  bool in_CY;
  uint in_fpscr;
  float fVar6;
  int iVar7;
  int iVar8;
  undefined1 uStack0000000c;
  
  if (in_CY && !in_ZR) {
    param_3 = 0;
  }
  *(undefined1 *)(param_1 + 0x3d) = param_3;
  if ((*(byte *)(param_1 + 0xe10) < 2) || (*(byte *)(param_1 + 0xe10) == 0xff)) {
    *(undefined1 *)(param_1 + 0xe24) = 0;
  }
  else {
    bVar3 = *(char *)(param_1 + 0xe24) + 1;
    *(byte *)(param_1 + 0xe24) = bVar3;
    if (100 < bVar3) {
      *(undefined1 *)(param_1 + 0xe10) = 0;
    }
  }
  if (*(char *)(param_1 + 0x32) != -1) {
    *(char *)(param_1 + 0x32) = *(char *)(param_1 + 0x32) + '\x01';
  }
  if (*(short *)(param_1 + 0x2a) != 0) {
    *(short *)(param_1 + 0x2a) = *(short *)(param_1 + 0x2a) + -1;
  }
  if (*(short *)(param_1 + 0x28) != 0) {
    *(short *)(param_1 + 0x28) = *(short *)(param_1 + 0x28) + -1;
  }
  if (*(char *)(param_1 + 0x2c) != '\0') {
    *(char *)(param_1 + 0x2c) = *(char *)(param_1 + 0x2c) + -1;
  }
  if (*(char *)(param_1 + 0xdbb) != '\0') {
    *(char *)(param_1 + 0xdbb) = *(char *)(param_1 + 0xdbb) + -1;
  }
  if (*(char *)(param_1 + 0xdba) != '\0') {
    *(char *)(param_1 + 0xdba) = *(char *)(param_1 + 0xdba) + -1;
  }
  cVar1 = *(char *)(param_1 + 0x45);
  cVar4 = cVar1 + '\x01';
  *(char *)(param_1 + 0x45) = cVar4;
  if (cVar1 == '\0') {
    uRam40000024 = 0;
    uRam40000c24 = 0;
    cVar4 = *(char *)(param_1 + 0x45);
  }
  uVar2 = uRam40000c24;
  if (cVar4 == '3') {
    if (*(char *)(param_1 + 0x2e) != '\0') {
      fVar6 = (float)VectorUnsignedToFloat(uRam40000024,(byte)(in_fpscr >> 0x15) & 3);
      iVar7 = (int)(fVar6 + fVar6);
      *(int *)(param_1 + 0x50) = iVar7;
      fVar6 = (float)VectorUnsignedToFloat(uVar2,(byte)(in_fpscr >> 0x15) & 3);
      iVar8 = (int)(fVar6 + fVar6);
      if (iVar7 == 0) {
        iVar7 = 0;
      }
      else {
        iVar7 = 1000000000 / iVar7;
      }
      *(int *)(param_1 + 0x54) = iVar7;
      *(int *)(param_1 + 0x80) = iVar8;
      if (iVar8 == 0) {
        iVar8 = 0;
      }
      else {
        iVar8 = 1000000000 / iVar8;
      }
      *(int *)(param_1 + 0x84) = iVar8;
      *(undefined1 *)(param_1 + 0x230) = *(undefined1 *)(param_1 + 0x231);
      *(undefined1 *)(param_1 + 0x231) = 0;
    }
    *(undefined1 *)(param_1 + 0x45) = 0;
  }
  if ((*unaff_r4 & 1) != 0) {
    *(undefined2 *)(param_1 + 0xdb8) = 0;
    return;
  }
  if (((*(char *)(param_1 + 0x17) != '\x02') && (*(byte *)(param_1 + 0x2d) < 0x14)) &&
     (bVar3 = *(byte *)(*(byte *)(param_1 + 0x2d) + 0x804d833),
     sVar5 = *(short *)(param_1 + 0xdb8) + 1, *(short *)(param_1 + 0xdb8) = sVar5,
     (ushort)(bVar3 + 0x32) == sVar5)) {
    uStack0000000c = 1;
    FUN_0803acf0(_DAT_20002d78,&stack0x0000000c,0xffffffff);
    FUN_0803acf0(_DAT_20002d78,&stack0x0000000c,0xffffffff);
  }
  return;
}



