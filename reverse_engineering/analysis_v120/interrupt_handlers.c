// Force-decompiled functions

// Attempting to create function at 0x0802e71c
// Successfully created function
// ==========================================
// Function: FUN_0802e71c @ 0802e71c
// Size: 160 bytes
// ==========================================

undefined1
FUN_0802e71c(int param_1,uint param_2,undefined4 param_3,undefined4 param_4,undefined4 param_5)

{
  undefined1 uVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  uint *puVar5;
  int *unaff_r4;
  int *unaff_r5;
  uint unaff_r6;
  uint unaff_r7;
  int unaff_r8;
  uint unaff_r10;
  uint uVar6;
  int unaff_r11;
  
  do {
    param_1 = param_1 + unaff_r7;
    if (unaff_r10 == unaff_r6 >> 0xc) {
      if ((unaff_r4[7] == param_1) || ((uint)unaff_r4[3] <= (uint)unaff_r4[5])) goto LAB_0802e5bc;
      if (*(char *)(unaff_r8 + 1) == '\x03') {
        iVar3 = param_1 * 0x1000;
      }
      else {
        if (*(char *)(unaff_r8 + 1) != '\x02') goto LAB_0802e7b2;
        iVar3 = param_1 * 0x1000 + 0x200000;
      }
      FUN_0802f048(param_5,iVar3,0x1000);
LAB_0802e5bc:
      unaff_r4[7] = param_1;
      uVar4 = unaff_r4[5] & 0xfff;
      goto LAB_0802e5c6;
    }
    uVar6 = unaff_r6 >> 0xc;
    if (param_2 < unaff_r7 + uVar6) {
      uVar6 = param_2 - unaff_r7;
    }
    iVar3 = FUN_0802cf38(*(undefined1 *)(unaff_r8 + 1),unaff_r11,param_1,uVar6);
    if (iVar3 != 0) {
LAB_0802e7b2:
      uVar1 = 1;
      goto LAB_0802e7b4;
    }
    if ((uint)(unaff_r4[7] - param_1) < uVar6) {
      FUN_080017a6(param_5,unaff_r11 + (unaff_r4[7] - param_1) * 0x1000,0x1000);
      *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) & 0x7f;
    }
    uVar6 = uVar6 << 0xc;
    unaff_r10 = 0;
    while( true ) {
      unaff_r6 = unaff_r6 - uVar6;
      *unaff_r5 = *unaff_r5 + uVar6;
      uVar2 = unaff_r4[5] + uVar6;
      unaff_r11 = unaff_r11 + uVar6;
      unaff_r4[5] = uVar2;
      uVar4 = unaff_r4[3];
      if ((uint)unaff_r4[3] < uVar2) {
        uVar4 = uVar2;
      }
      unaff_r4[3] = uVar4;
      if (unaff_r6 == 0) goto LAB_0802e79e;
      uVar4 = uVar2 & 0xfff;
      if (uVar4 == 0) break;
LAB_0802e5c6:
      uVar6 = 0x1000 - uVar4;
      if (unaff_r6 < 0x1000 - uVar4) {
        uVar6 = unaff_r6;
      }
      FUN_080017a6((int)unaff_r4 + uVar4 + 0x2c,unaff_r11,uVar6);
      *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) | 0x80;
    }
    unaff_r7 = *(ushort *)(unaff_r8 + 10) - 1 & uVar2 >> 0xc;
    if (unaff_r7 == 0) {
      if (uVar2 == 0) {
        iVar3 = unaff_r4[2];
        if (iVar3 == 0) goto LAB_0802e684;
      }
      else {
        iVar3 = unaff_r4[10];
        if (iVar3 == 0) {
LAB_0802e684:
          iVar3 = FUN_0802a2d4();
        }
        else {
          uVar4 = *(uint *)(iVar3 + 4);
          if (uVar4 == 0) goto LAB_0802e79e;
          uVar6 = (uVar2 >> 0xc) / (uint)*(ushort *)(*unaff_r4 + 10);
          puVar5 = (uint *)(iVar3 + 0x24);
          while (uVar4 <= uVar6) {
            uVar2 = puVar5[-6];
            if (uVar2 == 0) goto LAB_0802e79e;
            uVar6 = uVar6 - uVar4;
            if (uVar6 < uVar2) {
              puVar5 = puVar5 + -5;
              goto LAB_0802e698;
            }
            uVar4 = puVar5[-4];
            if (uVar4 == 0) goto LAB_0802e79e;
            uVar6 = uVar6 - uVar2;
            if (uVar6 < uVar4) {
              puVar5 = puVar5 + -3;
              goto LAB_0802e698;
            }
            uVar2 = puVar5[-2];
            if (uVar2 == 0) goto LAB_0802e79e;
            uVar6 = uVar6 - uVar4;
            if (uVar6 < uVar2) {
              puVar5 = puVar5 + -1;
              goto LAB_0802e698;
            }
            uVar4 = *puVar5;
            uVar6 = uVar6 - uVar2;
            puVar5 = puVar5 + 8;
            if (uVar4 == 0) goto LAB_0802e79e;
          }
          puVar5 = puVar5 + -7;
LAB_0802e698:
          iVar3 = uVar6 + *puVar5;
        }
      }
      if (iVar3 == -1) goto LAB_0802e7b2;
      if (iVar3 == 0) {
LAB_0802e79e:
        *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) | 0x40;
        return 0;
      }
      if (iVar3 == 1) break;
      unaff_r4[6] = iVar3;
      if (unaff_r4[2] == 0) {
        unaff_r4[2] = iVar3;
      }
    }
    if (0x7fffffff < (uint)(int)(char)unaff_r4[4]) {
      if (*(char *)(unaff_r8 + 1) == '\x03') {
        iVar3 = unaff_r4[7] << 0xc;
      }
      else {
        if (*(char *)(unaff_r8 + 1) != '\x02') goto LAB_0802e7b2;
        iVar3 = unaff_r4[7] * 0x1000 + 0x200000;
      }
      FUN_0802f16c(param_5,iVar3,0x1000);
      *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) & 0x7f;
    }
    if (*(int *)(unaff_r8 + 0x18) - 2U <= unaff_r4[6] - 2U) break;
    param_2 = (uint)*(ushort *)(unaff_r8 + 10);
    param_1 = (unaff_r4[6] - 2U) * param_2 + *(int *)(unaff_r8 + 0x2c);
  } while (param_1 != 0);
  uVar1 = 2;
LAB_0802e7b4:
  *(undefined1 *)((int)unaff_r4 + 0x11) = uVar1;
  return uVar1;
}



// Attempting to create function at 0x0802e7b4
// Successfully created function
// ==========================================
// Function: FUN_0802e7b4 @ 0802e7b4
// Size: 8 bytes
// ==========================================

void FUN_0802e7b4(undefined1 param_1)

{
  int unaff_r4;
  
  *(undefined1 *)(unaff_r4 + 0x11) = param_1;
  return;
}



// Attempting to create function at 0x08009670
// Successfully created function
// ==========================================
// Function: FUN_08009670 @ 08009670
// Size: 10 bytes
// ==========================================

void FUN_08009670(undefined4 *param_1)

{
                    /* WARNING: Could not recover jumptable at 0x08009678. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*(code *)*param_1)(0);
  return;
}



// Attempting to create function at 0x0802e78c
// Successfully created function
// ==========================================
// Function: FUN_0802e78c @ 0802e78c
// Size: 40 bytes
// ==========================================

undefined1 FUN_0802e78c(uint param_1)

{
  undefined1 uVar1;
  uint uVar2;
  uint uVar3;
  uint *puVar4;
  uint uVar5;
  uint uVar6;
  int *unaff_r4;
  int *unaff_r5;
  uint unaff_r6;
  int iVar7;
  int unaff_r8;
  int unaff_r9;
  uint unaff_r10;
  int unaff_r11;
  undefined4 in_stack_00000000;
  
code_r0x0802e78c:
  if (param_1 != 2) {
LAB_0802e7b2:
    uVar1 = 1;
FUN_0802e7b4:
    *(undefined1 *)((int)unaff_r4 + 0x11) = uVar1;
    return uVar1;
  }
  iVar7 = unaff_r9 * 0x1000 + 0x200000;
code_r0x0802e5b0:
  FUN_0802f048(in_stack_00000000,iVar7,0x1000);
LAB_0802e5bc:
  unaff_r4[7] = unaff_r9;
  uVar3 = unaff_r4[5] & 0xfff;
  do {
    uVar5 = 0x1000 - uVar3;
    if (unaff_r6 < 0x1000 - uVar3) {
      uVar5 = unaff_r6;
    }
    FUN_080017a6((int)unaff_r4 + uVar3 + 0x2c,unaff_r11,uVar5);
    *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) | 0x80;
    while( true ) {
      unaff_r6 = unaff_r6 - uVar5;
      *unaff_r5 = *unaff_r5 + uVar5;
      uVar2 = unaff_r4[5] + uVar5;
      unaff_r11 = unaff_r11 + uVar5;
      unaff_r4[5] = uVar2;
      uVar3 = unaff_r4[3];
      if ((uint)unaff_r4[3] < uVar2) {
        uVar3 = uVar2;
      }
      unaff_r4[3] = uVar3;
      if (unaff_r6 == 0) goto LAB_0802e79e;
      uVar3 = uVar2 & 0xfff;
      if (uVar3 != 0) break;
      uVar3 = *(ushort *)(unaff_r8 + 10) - 1 & uVar2 >> 0xc;
      if (uVar3 == 0) {
        if (uVar2 == 0) {
          iVar7 = unaff_r4[2];
          if (iVar7 == 0) goto LAB_0802e684;
        }
        else {
          iVar7 = unaff_r4[10];
          if (iVar7 == 0) {
LAB_0802e684:
            iVar7 = FUN_0802a2d4();
          }
          else {
            uVar5 = *(uint *)(iVar7 + 4);
            if (uVar5 == 0) goto LAB_0802e79e;
            uVar2 = (uVar2 >> 0xc) / (uint)*(ushort *)(*unaff_r4 + 10);
            puVar4 = (uint *)(iVar7 + 0x24);
            while (uVar5 <= uVar2) {
              uVar6 = puVar4[-6];
              if (uVar6 == 0) goto LAB_0802e79e;
              uVar2 = uVar2 - uVar5;
              if (uVar2 < uVar6) {
                puVar4 = puVar4 + -5;
                goto LAB_0802e698;
              }
              uVar5 = puVar4[-4];
              if (uVar5 == 0) goto LAB_0802e79e;
              uVar2 = uVar2 - uVar6;
              if (uVar2 < uVar5) {
                puVar4 = puVar4 + -3;
                goto LAB_0802e698;
              }
              uVar6 = puVar4[-2];
              if (uVar6 == 0) goto LAB_0802e79e;
              uVar2 = uVar2 - uVar5;
              if (uVar2 < uVar6) {
                puVar4 = puVar4 + -1;
                goto LAB_0802e698;
              }
              uVar5 = *puVar4;
              uVar2 = uVar2 - uVar6;
              puVar4 = puVar4 + 8;
              if (uVar5 == 0) goto LAB_0802e79e;
            }
            puVar4 = puVar4 + -7;
LAB_0802e698:
            iVar7 = uVar2 + *puVar4;
          }
        }
        if (iVar7 == -1) goto LAB_0802e7b2;
        if (iVar7 == 0) {
LAB_0802e79e:
          *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) | 0x40;
          return 0;
        }
        if (iVar7 != 1) {
          unaff_r4[6] = iVar7;
          if (unaff_r4[2] == 0) {
            unaff_r4[2] = iVar7;
          }
          goto LAB_0802e6b6;
        }
LAB_0802e7ae:
        uVar1 = 2;
        goto FUN_0802e7b4;
      }
LAB_0802e6b6:
      if (0x7fffffff < (uint)(int)(char)unaff_r4[4]) {
        if (*(char *)(unaff_r8 + 1) == '\x03') {
          iVar7 = unaff_r4[7] << 0xc;
        }
        else {
          if (*(char *)(unaff_r8 + 1) != '\x02') goto LAB_0802e7b2;
          iVar7 = unaff_r4[7] * 0x1000 + 0x200000;
        }
        FUN_0802f16c(in_stack_00000000,iVar7,0x1000);
        *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) & 0x7f;
      }
      if (*(int *)(unaff_r8 + 0x18) - 2U <= unaff_r4[6] - 2U) goto LAB_0802e7ae;
      uVar2 = (uint)*(ushort *)(unaff_r8 + 10);
      iVar7 = (unaff_r4[6] - 2U) * uVar2 + *(int *)(unaff_r8 + 0x2c);
      if (iVar7 == 0) goto LAB_0802e7ae;
      unaff_r9 = iVar7 + uVar3;
      if (unaff_r10 == unaff_r6 >> 0xc) {
        if ((unaff_r4[7] == unaff_r9) || ((uint)unaff_r4[3] <= (uint)unaff_r4[5]))
        goto LAB_0802e5bc;
        param_1 = (uint)*(byte *)(unaff_r8 + 1);
        if (param_1 != 3) goto code_r0x0802e78c;
        iVar7 = unaff_r9 * 0x1000;
        goto code_r0x0802e5b0;
      }
      uVar5 = unaff_r6 >> 0xc;
      if (uVar2 < uVar3 + uVar5) {
        uVar5 = uVar2 - uVar3;
      }
      iVar7 = FUN_0802cf38(*(undefined1 *)(unaff_r8 + 1),unaff_r11,unaff_r9,uVar5);
      if (iVar7 != 0) goto LAB_0802e7b2;
      if ((uint)(unaff_r4[7] - unaff_r9) < uVar5) {
        FUN_080017a6(in_stack_00000000,unaff_r11 + (unaff_r4[7] - unaff_r9) * 0x1000,0x1000);
        *(byte *)(unaff_r4 + 4) = *(byte *)(unaff_r4 + 4) & 0x7f;
      }
      uVar5 = uVar5 << 0xc;
      unaff_r10 = 0;
    }
  } while( true );
}



// Attempting to create function at 0x08009c10
// Successfully created function
// ==========================================
// Function: FUN_08009c10 @ 08009c10
// Size: 102 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_08009c10(uint param_1,int param_2)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  int unaff_r4;
  
  *(short *)(param_2 + 2) = (short)param_1;
  if (param_1 >> 0x1c != 0) {
    *(undefined2 *)(param_2 + 4) = 0;
  }
  FUN_0800ec70();
  FUN_0803bee0();
  iVar1 = FUN_0803b3a8(_DAT_20002d84,0xffffffff);
  if ((iVar1 == 1) && (*(int *)(unaff_r4 + 8) != 0)) {
    if (DAT_20001080 == '\0') {
                    /* WARNING: Could not recover jumptable at 0x08009c74. Too many branches */
                    /* WARNING: Treating indirect jump as call */
      (*_DAT_20001070)(0);
      return;
    }
    uVar2 = *(int *)(unaff_r4 + 8) - _DAT_20001078;
    if (uVar2 >> 10 < 0xaf) {
      uVar2 = uVar2 >> 5;
      uVar3 = (uint)*(ushort *)(_DAT_2000107c + uVar2 * 2);
      if (uVar3 == 0) {
        return;
      }
      FUN_080012bc(_DAT_2000107c + uVar2 * 2,uVar3 << 1);
    }
  }
  return;
}



// Attempting to create function at 0x0800735c
// Successfully created function
// ==========================================
// Function: FUN_0800735c @ 0800735c
// Size: 1 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800735c(undefined1 param_1)

{
  int unaff_r4;
  undefined4 in_stack_00000000;
  undefined1 uStack_9;
  undefined4 uStack_8;
  
  *(undefined1 *)(unaff_r4 + 0x355) = param_1;
  *(undefined1 *)(unaff_r4 + 0xf68) = 1;
  _DAT_4000440c = _DAT_4000440c | 0x2000;
  FUN_0803a610(_DAT_20002da0);
  FUN_0803a610(_DAT_20002da4);
  _DAT_40011010 = 0x800;
  *(undefined4 *)(unaff_r4 + 0xf48) = 0x7fc00000;
  *(undefined4 *)(unaff_r4 + 0xf4c) = 0x7fc00000;
  *(undefined4 *)(unaff_r4 + 0xf50) = 0x7fc00000;
  *(undefined2 *)(unaff_r4 + 0xf35) = 0x101;
  *(undefined1 *)(unaff_r4 + 0xf5d) = 0;
  *(undefined1 *)(unaff_r4 + 0xf2f) = 0;
  *(undefined1 *)(unaff_r4 + 0xf38) = 0xff;
  *(undefined2 *)(unaff_r4 + 0xf3c) = 0;
  *(undefined2 *)(unaff_r4 + 0xf2c) = 0xff;
  *(undefined2 *)(unaff_r4 + 0xf69) = 0;
  *(undefined1 *)(unaff_r4 + 0xf6b) = 0;
  uStack_8 = in_stack_00000000;
  switch(DAT_20001060) {
  case 0:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 1;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0xb;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0xc;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0xd;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0xe;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0xf;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x10;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x11;
    break;
  case 1:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 9;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uStack_9 = 10;
    }
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x1a;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x1b;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x1c;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x1d;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x1e;
    break;
  case 2:
    uStack_9 = 2;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 3;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 4;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 5;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 6;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 8;
    goto LAB_0800bc6e;
  case 3:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 8;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 9;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uStack_9 = 10;
    }
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x16;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x17;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x18;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x19;
    break;
  case 4:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x1f;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 9;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x20;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x21;
    break;
  case 5:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x25;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 9;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x26;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x27;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x28;
    break;
  case 6:
    uStack_9 = 0x29;
    break;
  case 7:
    uStack_9 = 0x15;
    break;
  case 8:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x2c;
    break;
  case 9:
    uStack_9 = 0;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x12;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x13;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 0x14;
LAB_0800bc6e:
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 9;
    FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
    uStack_9 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uStack_9 = 10;
    }
    break;
  default:
    goto switchD_0800b926_default;
  }
  FUN_0803acf0(_DAT_20002d6c,&uStack_9,0xffffffff);
switchD_0800b926_default:
  return;
}



