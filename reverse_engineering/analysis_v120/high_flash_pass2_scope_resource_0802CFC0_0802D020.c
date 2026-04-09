// Decompiled functions in range 0x0802CFC0 - 0x0802D020

// ==========================================
// Function: FUN_0802d014 @ 0802d014
// Size: 430 bytes
// ==========================================

void FUN_0802d014(undefined4 *param_1)

{
  char cVar1;
  uint uVar2;
  undefined4 uVar3;
  char *pcVar4;
  char *pcVar5;
  int iVar6;
  int iVar7;
  
  if (param_1 == (undefined4 *)0x0) {
    return;
  }
  pcVar5 = (char *)*param_1;
  if (pcVar5 == (char *)0x0) {
    return;
  }
  if (*pcVar5 == '\0') {
    return;
  }
  if (*(short *)(param_1 + 1) != *(short *)(pcVar5 + 6)) {
    return;
  }
  uVar2 = (uint)(byte)pcVar5[1];
  if (2 < uVar2 - 1) {
    return;
  }
  if (-1 < (int)((uint)*(byte *)(param_1 + 4) << 0x19)) goto LAB_0802d04a;
  if ((int)((uint)*(byte *)(param_1 + 4) << 0x18) < 0) {
    if (uVar2 == 3) {
      iVar6 = param_1[7] << 0xc;
    }
    else {
      if (uVar2 != 2) {
        return;
      }
      iVar6 = param_1[7] * 0x1000 + 0x200000;
    }
    FUN_0802f16c(param_1 + 0xb,iVar6,0x1000);
    *(byte *)(param_1 + 4) = *(byte *)(param_1 + 4) & 0x7f;
  }
  iVar6 = param_1[8];
  iVar7 = *(int *)(pcVar5 + 0x30);
  if (iVar7 != iVar6) {
    if (pcVar5[3] == '\0') {
LAB_0802d130:
      cVar1 = pcVar5[1];
    }
    else {
      if (pcVar5[1] == '\x03') {
        iVar7 = iVar7 << 0xc;
      }
      else {
        if (pcVar5[1] != '\x02') {
          return;
        }
        iVar7 = iVar7 * 0x1000 + 0x200000;
      }
      FUN_0802f16c(pcVar5 + 0x34,iVar7,0x1000);
      pcVar5[3] = '\0';
      if ((*(uint *)(pcVar5 + 0x1c) <= (uint)(*(int *)(pcVar5 + 0x30) - *(int *)(pcVar5 + 0x24))) ||
         (pcVar5[2] != '\x02')) goto LAB_0802d130;
      cVar1 = pcVar5[1];
      iVar7 = *(uint *)(pcVar5 + 0x1c) + *(int *)(pcVar5 + 0x30);
      if (cVar1 == '\x03') {
        iVar7 = iVar7 * 0x1000;
code_r0x0802d124:
        FUN_0802f16c(pcVar5 + 0x34,iVar7,0x1000);
        goto LAB_0802d130;
      }
      if (cVar1 == '\x02') {
        iVar7 = iVar7 * 0x1000 + 0x200000;
        goto code_r0x0802d124;
      }
    }
    if (cVar1 == '\x03') {
      iVar7 = iVar6 << 0xc;
    }
    else {
      if (cVar1 != '\x02') {
        pcVar5[0x30] = -1;
        pcVar5[0x31] = -1;
        pcVar5[0x32] = -1;
        pcVar5[0x33] = -1;
        return;
      }
      iVar7 = iVar6 * 0x1000 + 0x200000;
    }
    FUN_0802f048(pcVar5 + 0x34,iVar7,0x1000);
    *(int *)(pcVar5 + 0x30) = iVar6;
  }
  iVar6 = param_1[9];
  *(byte *)(iVar6 + 0xb) = *(byte *)(iVar6 + 0xb) | 0x20;
  uVar3 = param_1[2];
  pcVar4 = (char *)*param_1;
  *(short *)(iVar6 + 0x1a) = (short)uVar3;
  if (*pcVar4 == '\x03') {
    *(char *)(iVar6 + 0x14) = (char)((uint)uVar3 >> 0x10);
    *(char *)(iVar6 + 0x15) = (char)((uint)uVar3 >> 0x18);
  }
  *(undefined4 *)(iVar6 + 0x1c) = param_1[3];
  *(undefined4 *)(iVar6 + 0x16) = 0x50210000;
  *(undefined2 *)(iVar6 + 0x12) = 0;
  pcVar5[3] = '\x01';
  iVar6 = FUN_08036934(pcVar5);
  *(byte *)(param_1 + 4) = *(byte *)(param_1 + 4) & 0xbf;
  if (iVar6 != 0) {
    return;
  }
  pcVar5 = (char *)*param_1;
  if (pcVar5 == (char *)0x0) {
    return;
  }
LAB_0802d04a:
  if (*pcVar5 != '\0') {
    if ((*(short *)(param_1 + 1) == *(short *)(pcVar5 + 6)) && ((byte)pcVar5[1] - 1 < 3)) {
      *param_1 = 0;
    }
    return;
  }
  return;
}



