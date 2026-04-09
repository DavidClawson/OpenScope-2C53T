// Force-decompiled functions

// Attempting to create function at 0x08015848
// Successfully created function
// ==========================================
// Function: FUN_08015848 @ 08015848
// Size: 396 bytes
// ==========================================

void FUN_08015848(undefined1 param_1)

{
  undefined4 *unaff_r5;
  int unaff_r9;
  undefined1 uStack0000003f;
  
  *(undefined1 *)(unaff_r9 + 0xf68) = param_1;
  uStack0000003f = 0x24;
  FUN_0803acf0(*unaff_r5,&stack0x0000003f,0xffffffff);
  uStack0000003f = 3;
  FUN_0803acf0(*unaff_r5,&stack0x0000003f,0xffffffff);
  if ((*(char *)(unaff_r9 + 0x17) == '\x02') && (*(char *)(unaff_r9 + 0x2e) != '\0')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (*(char *)(unaff_r9 + 0x2f) != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x30) != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((*(char *)(unaff_r9 + 0x2e) == '\0') &&
     ((uint)-*(int *)(unaff_r9 + 0xe0c) < (uint)(*(uint *)(unaff_r9 + 0xe08) < 10))) {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x33) != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  *(char *)(unaff_r9 + 0xf70) = *(char *)(unaff_r9 + 0xf70) + '\x01';
  return;
}



// Attempting to create function at 0x0800b914
// Successfully created function
// ==========================================
// Function: FUN_0800b914 @ 0800b914
// Size: 26 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800b914(int param_1)

{
  undefined1 uStack00000007;
  
  switch(*(undefined1 *)(param_1 + 0xf68)) {
  case 0:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 1;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0xb;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0xc;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0xd;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0xe;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0xf;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x10;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x11;
    break;
  case 1:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 9;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uStack00000007 = 10;
    }
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x1a;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x1b;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x1c;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x1d;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x1e;
    break;
  case 2:
    uStack00000007 = 2;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 3;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 4;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 5;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 6;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 8;
    goto code_r0x0800bc6e;
  case 3:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 8;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 9;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uStack00000007 = 10;
    }
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x16;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x17;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x18;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x19;
    break;
  case 4:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x1f;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 9;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x20;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x21;
    break;
  case 5:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x25;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 9;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x26;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x27;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x28;
    break;
  case 6:
    uStack00000007 = 0x29;
    break;
  case 7:
    uStack00000007 = 0x15;
    break;
  case 8:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x2c;
    break;
  case 9:
    uStack00000007 = 0;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x12;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x13;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 0x14;
code_r0x0800bc6e:
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 9;
    FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
    uStack00000007 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uStack00000007 = 10;
    }
    break;
  default:
    goto LAB_0800bcce;
  }
  FUN_0803acf0(_DAT_20002d6c,&stack0x00000007,0xffffffff);
LAB_0800bcce:
  return;
}



// ==========================================
// Function: FUN_08009314 @ 08009314
// Size: 222 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_08009314(void)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  
  if (DAT_20001060 == '\t') {
    _DAT_20008350 = 0;
    _DAT_20008352 = 0;
    _DAT_20008354 = 0x10c;
    _DAT_20008356 = 0x14;
    _DAT_20008358 = FUN_08033cfc(0x29e0);
    if (_DAT_20008358 != 0) {
      FUN_08032f6c(0x80bc456,0,0);
      FUN_08008154(0x34,0,0xd8,0x14,*(undefined4 *)(&DAT_0804c900 + (uint)DAT_20001058 * 4),
                   0x20001144,0xffff,1);
      FUN_0803bee0();
      iVar1 = FUN_0803b3a8(_DAT_20002d84,0xffffffff);
      if ((iVar1 == 1) && (_DAT_20008358 != 0)) {
        if (DAT_20001080 == '\0') {
                    /* WARNING: Could not recover jumptable at 0x080093f0. Too many branches */
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
  }
  return;
}



