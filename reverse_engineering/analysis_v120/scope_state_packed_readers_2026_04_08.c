// Force-decompiled functions

// Attempting to create function at 0x0800c972
// Successfully created function
// ==========================================
// Function: FUN_0800c972 @ 0800c972
// Size: 134 bytes
// ==========================================

void FUN_0800c972(void)

{
  ushort uVar1;
  char cVar2;
  uint uVar3;
  int iVar4;
  int unaff_r6;
  undefined4 unaff_r8;
  int unaff_r10;
  undefined4 uStack00000000;
  uint uStack00000008;
  undefined4 uStack0000000c;
  
  cVar2 = *(char *)(unaff_r6 + 0xf6b);
  iVar4 = 2;
  if (cVar2 == '\x02') {
    iVar4 = 4;
  }
  uVar3 = (uint)(*(char *)(unaff_r6 + 0xf60) != '\x01');
  uStack00000000 = *(undefined4 *)(unaff_r10 + (uVar3 | uVar3 << 1) * 4 + 8);
  uVar1 = *(ushort *)(iVar4 + 0x804c180);
  uStack0000000c = 0;
  uStack00000008 = (uint)uVar1;
  FUN_08008154(0x80,0x94,100,0x20);
  if (cVar2 == '\x02') {
    unaff_r8 = 0x80bc429;
  }
  FUN_08032f6c(unaff_r8,0xe4,0x9b);
  uStack00000000 = *(undefined4 *)(&DAT_0804c7fc + (uint)*(byte *)(unaff_r6 + 0x16) * 4);
  uStack0000000c = 1;
  uStack00000008 = (uint)uVar1;
  FUN_08008154(0xe4,0x9a,0x42,0x14);
  return;
}



// Attempting to create function at 0x0800d83c
// Successfully created function
// ==========================================
// Function: FUN_0800d83c @ 0800d83c
// Size: 150 bytes
// ==========================================

void FUN_0800d83c(void)

{
  byte bVar1;
  int iVar2;
  char cVar3;
  int in_r3;
  
  if (*(char *)(in_r3 + 0xf69) != '\x02') {
    return;
  }
  bVar1 = *(byte *)(in_r3 + 0xf6b);
  if (bVar1 < 0xc0) {
    cVar3 = ((bVar1 >> 6) * ' ' - (bVar1 >> 6)) * '\x02' + '}';
    iVar2 = (short)(bVar1 >> 4 & 3) * 0x1a + 0x82;
  }
  else {
    iVar2 = 0x68;
    if (bVar1 >> 4 == 0xc) {
      cVar3 = -0x45;
    }
    else {
      cVar3 = -7;
    }
  }
  FUN_08019c48(cVar3,iVar2,4);
  if (DAT_20001063 < 0xc0) {
    cVar3 = ((DAT_20001063 >> 6) * ' ' - (DAT_20001063 >> 6)) * '\x02' + '}';
    iVar2 = (short)(DAT_20001063 >> 4 & 3) * 0x1a + 0x82;
  }
  else {
    iVar2 = 0x68;
    if (DAT_20001063 >> 4 == 0xc) {
      cVar3 = -0x45;
    }
    else {
      cVar3 = -7;
    }
  }
  FUN_08019c48(cVar3,iVar2,3);
  return;
}



// Attempting to create function at 0x0800e03c
// Successfully created function
// ==========================================
// Function: FUN_0800e03c @ 0800e03c
// Size: 444 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800e03c(void)

{
  undefined4 uVar1;
  uint uVar2;
  undefined4 uVar3;
  int in_r3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  
  if ((*(char *)(in_r3 + 0xf69) == '\0') || (*(char *)(in_r3 + 0xf6b) != '\x03')) {
    uVar2 = 0xaa;
    iVar6 = 0xab;
    do {
      uVar4 = 0x7b;
      do {
        if (((((uint)_DAT_20008350 <= uVar4 - 1) &&
             (uVar4 - 1 < (uint)_DAT_20008354 + (uint)_DAT_20008350)) && (_DAT_20008352 <= uVar2))
           && (uVar2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)) {
          *(undefined2 *)
           (_DAT_20008358 +
            (((uVar2 - _DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar4) * 2 + -2
           ) = 0x18c3;
        }
        if (((_DAT_20008350 <= uVar4) && (uVar4 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
           ((_DAT_20008352 <= uVar2 && (uVar2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
          *(undefined2 *)
           (_DAT_20008358 +
           (((uint)_DAT_20008354 * (uVar2 - _DAT_20008352) - (uint)_DAT_20008350) + uVar4) * 2) =
               0x18c3;
        }
        uVar4 = uVar4 + 2;
      } while (uVar4 != 0x131);
      uVar5 = uVar2 | 1;
      uVar4 = 0x7b;
      do {
        if ((((uint)_DAT_20008350 <= uVar4 - 1) &&
            (uVar4 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
           ((_DAT_20008352 <= uVar5 && (uVar5 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
          *(undefined2 *)
           (_DAT_20008358 +
            (((iVar6 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar4) *
            2 + -2) = 0x18c3;
        }
        if ((((_DAT_20008350 <= uVar4) && (uVar4 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
            (_DAT_20008352 <= uVar5)) && (uVar5 < (uint)_DAT_20008352 + (uint)_DAT_20008356)) {
          *(undefined2 *)
           (_DAT_20008358 +
           (((iVar6 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar4) * 2
           ) = 0x18c3;
        }
        uVar4 = uVar4 + 2;
      } while (uVar4 != 0x131);
      uVar2 = uVar2 + 2;
      iVar6 = iVar6 + 2;
    } while (uVar2 != 0xca);
    if (DAT_20000134 == '\x03') {
      uVar1 = 0x80bc4f2;
    }
    else {
      uVar1 = 0x80bc5cf;
    }
    FUN_08032f6c(uVar1,0xa6,0xb3);
  }
  else {
    FUN_08032f6c(0x80bc499,0x79,0xa9);
    if (*(char *)(in_r3 + 0x3c) == '\x03') {
      uVar1 = 0x80bc54a;
      uVar3 = 0xa6;
    }
    else {
      uVar1 = 0x80bc5fc;
      uVar3 = 0xa5;
    }
    FUN_08032f6c(uVar1,uVar3,0xb1);
  }
  FUN_08008154(0xb6,0xa9,0x5c,0x22);
  return;
}



// Attempting to create function at 0x0800e042
// Successfully created function
// ==========================================
// Function: FUN_0800e042 @ 0800e042
// Size: 438 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800e042(void)

{
  undefined4 uVar1;
  uint uVar2;
  undefined4 uVar3;
  int in_r3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  
  if (*(char *)(in_r3 + 0xf6b) == '\x03') {
    FUN_08032f6c(0x80bc499,0x79,0xa9);
    if (*(char *)(in_r3 + 0x3c) == '\x03') {
      uVar1 = 0x80bc54a;
      uVar3 = 0xa6;
    }
    else {
      uVar1 = 0x80bc5fc;
      uVar3 = 0xa5;
    }
    FUN_08032f6c(uVar1,uVar3,0xb1);
  }
  else {
    uVar2 = 0xaa;
    iVar6 = 0xab;
    do {
      uVar4 = 0x7b;
      do {
        if (((((uint)_DAT_20008350 <= uVar4 - 1) &&
             (uVar4 - 1 < (uint)_DAT_20008354 + (uint)_DAT_20008350)) && (_DAT_20008352 <= uVar2))
           && (uVar2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)) {
          *(undefined2 *)
           (_DAT_20008358 +
            (((uVar2 - _DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar4) * 2 + -2
           ) = 0x18c3;
        }
        if (((_DAT_20008350 <= uVar4) && (uVar4 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
           ((_DAT_20008352 <= uVar2 && (uVar2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
          *(undefined2 *)
           (_DAT_20008358 +
           (((uint)_DAT_20008354 * (uVar2 - _DAT_20008352) - (uint)_DAT_20008350) + uVar4) * 2) =
               0x18c3;
        }
        uVar4 = uVar4 + 2;
      } while (uVar4 != 0x131);
      uVar5 = uVar2 | 1;
      uVar4 = 0x7b;
      do {
        if ((((uint)_DAT_20008350 <= uVar4 - 1) &&
            (uVar4 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
           ((_DAT_20008352 <= uVar5 && (uVar5 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
          *(undefined2 *)
           (_DAT_20008358 +
            (((iVar6 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar4) *
            2 + -2) = 0x18c3;
        }
        if ((((_DAT_20008350 <= uVar4) && (uVar4 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
            (_DAT_20008352 <= uVar5)) && (uVar5 < (uint)_DAT_20008352 + (uint)_DAT_20008356)) {
          *(undefined2 *)
           (_DAT_20008358 +
           (((iVar6 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar4) * 2
           ) = 0x18c3;
        }
        uVar4 = uVar4 + 2;
      } while (uVar4 != 0x131);
      uVar2 = uVar2 + 2;
      iVar6 = iVar6 + 2;
    } while (uVar2 != 0xca);
    if (DAT_20000134 == '\x03') {
      uVar1 = 0x80bc4f2;
    }
    else {
      uVar1 = 0x80bc5cf;
    }
    FUN_08032f6c(uVar1,0xa6,0xb3);
  }
  FUN_08008154(0xb6,0xa9,0x5c,0x22);
  return;
}



// Attempting to create function at 0x0800e550
// Successfully created function
// ==========================================
// Function: FUN_0800e550 @ 0800e550
// Size: 64 bytes
// ==========================================

void FUN_0800e550(int param_1)

{
  byte bVar1;
  int iVar2;
  char cVar3;
  
  bVar1 = *(byte *)(param_1 + 0xf6b);
  if (bVar1 < 0xc0) {
    cVar3 = ((bVar1 >> 6) * ' ' - (bVar1 >> 6)) * '\x02' + '}';
    iVar2 = (short)(bVar1 >> 4 & 3) * 0x1a + 0x82;
  }
  else {
    iVar2 = 0x68;
    if (bVar1 >> 4 == 0xc) {
      cVar3 = -0x45;
    }
    else {
      cVar3 = -7;
    }
  }
  FUN_08019c48(cVar3,iVar2,3);
  return;
}



