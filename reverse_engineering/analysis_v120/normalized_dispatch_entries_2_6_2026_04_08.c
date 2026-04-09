// Force-decompiled functions

// Attempting to create function at 0x080104EC
// Successfully created function
// ==========================================
// Function: FUN_080104ec @ 080104ec
// Size: 1 bytes
// ==========================================

/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08015214 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

void FUN_080104ec(undefined4 param_1,undefined4 param_2,uint param_3,undefined2 param_4,
                 ushort param_5)

{
  ushort *puVar1;
  ushort uVar2;
  bool bVar3;
  float fVar4;
  char cVar5;
  byte bVar6;
  byte bVar7;
  short sVar8;
  uint uVar9;
  int *piVar10;
  uint uVar11;
  float fVar12;
  int iVar13;
  byte *pbVar14;
  char *pcVar15;
  uint uVar16;
  uint extraout_r1;
  ushort uVar17;
  uint uVar18;
  uint uVar19;
  int iVar20;
  undefined4 uVar21;
  uint uVar22;
  ushort *unaff_r5;
  int iVar23;
  uint uVar24;
  uint uVar25;
  int unaff_r8;
  uint uVar26;
  int iVar27;
  byte *pbVar28;
  uint in_fpscr;
  uint uVar29;
  float fVar30;
  undefined4 uVar31;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  float fVar32;
  float fVar33;
  undefined4 uVar34;
  undefined8 uVar35;
  longlong lVar36;
  ulonglong uVar37;
  undefined8 uVar38;
  undefined8 uVar39;
  uint uStack00000034;
  undefined4 in_stack_0000003c;
  undefined8 in_stack_00000040;
  undefined8 in_stack_00000048;
  undefined8 in_stack_00000050;
  undefined8 in_stack_00000058;
  undefined8 in_stack_00000060;
  undefined4 in_stack_0000006c;
  undefined4 in_stack_00000070;
  undefined4 in_stack_00000074;
  undefined4 in_stack_00000078;
  undefined4 in_stack_0000007c;
  undefined4 in_stack_00000080;
  undefined4 in_stack_00000084;
  undefined4 in_stack_00000088;
  undefined4 in_stack_0000008c;
  
code_r0x080104ec:
  uVar9 = param_3;
  if ((((*unaff_r5 < 0xa0) && (0x9f < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
      (unaff_r5[1] <= param_3)) && (param_3 < (uint)unaff_r5[1] + (uint)unaff_r5[3])) {
    puVar1 = unaff_r5 + 4;
    uVar17 = *unaff_r5;
    unaff_r5 = (ushort *)&DAT_20008350;
    *(undefined2 *)
     (*(int *)puVar1 + ((uint)_DAT_20008354 * (param_3 - _DAT_20008352) - (uint)uVar17) * 2 + 0x13e)
         = param_4;
  }
  while( true ) {
    param_5 = param_5 + 1;
    param_3 = uVar9 + 3;
    if (4 < param_5) {
      param_5 = 0;
    }
    if (param_3 == 0xdf) break;
    if ((((param_5 == 0) && (*unaff_r5 < 0xa0)) && (0x9f < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 + 1 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 + 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((param_3 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13e)
           = param_4;
    }
    param_5 = param_5 + 1;
    if (4 < param_5) {
      param_5 = 0;
    }
    if (((param_5 == 0) && (*unaff_r5 < 0xa0)) &&
       ((0x9f < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 + 2 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 + 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + param_3) - (uint)_DAT_20008350) * 2 + 0x13e)
           = param_4;
    }
    param_5 = param_5 + 1;
    if (4 < param_5) {
      param_5 = 0;
    }
    uVar9 = param_3;
    if (param_5 == 0) goto code_r0x080104ec;
  }
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*unaff_r5 < 0xb9)) &&
       ((0xb8 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 2 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0xb9)) && (0xb8 < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 1 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0xb9)) &&
       ((0xb8 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x170)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*unaff_r5 < 0xd2)) && (0xd1 < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 2 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0xd2)) &&
       ((0xd1 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 1 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0xd2)) && (0xd1 < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x1a2)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*unaff_r5 < 0xeb)) &&
       ((0xea < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 2 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0xeb)) && (0xea < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 1 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0xeb)) &&
       ((0xea < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x1d4)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*unaff_r5 < 0x9e)) && (0x9d < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 2 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0x9e)) &&
       ((0x9d < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 1 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0x9e)) && (0x9d < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x13a)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*unaff_r5 < 0x9f)) &&
       ((0x9e < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 2 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0x9f)) && (0x9e < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 1 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0x9f)) &&
       ((0x9e < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x13c)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*unaff_r5 < 0xa1)) && (0xa0 < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 2 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0xa1)) &&
       ((0xa0 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 1 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0xa1)) && (0xa0 < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x140)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*unaff_r5 < 0xa2)) &&
       ((0xa1 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        (((uint)unaff_r5[1] <= uVar9 - 2 &&
         (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 < 0xa2)) && (0xa1 < (uint)*unaff_r5 + (uint)unaff_r5[2])) &&
       (((uint)unaff_r5[1] <= uVar9 - 1 &&
        (puVar1 = unaff_r5 + 1, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 < 0xa2)) &&
       ((0xa1 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        ((unaff_r5[1] <= uVar9 && (uVar9 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x142)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x3d;
  do {
    if ((((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 2)) &&
        (uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 1)) &&
       ((uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 <= uVar9)) && (uVar9 < (uint)*unaff_r5 + (uint)unaff_r5[2]))
       && ((unaff_r5[1] < 0x77 && (0x76 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0x106);
  uVar17 = 0;
  uVar9 = 0x3d;
  do {
    if (((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 2)) &&
       ((uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 1)) &&
        (uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 <= uVar9)) &&
       ((uVar9 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        ((unaff_r5[1] < 0x78 && (0x77 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0x106);
  uVar17 = 0;
  uVar9 = 0x3d;
  do {
    if ((((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 2)) &&
        (uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 1)) &&
       ((uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*unaff_r5 <= uVar9)) && (uVar9 < (uint)*unaff_r5 + (uint)unaff_r5[2]))
       && ((unaff_r5[1] < 0x7a && (0x79 < (uint)unaff_r5[1] + (uint)unaff_r5[3])))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0x106);
  uVar17 = 0;
  uVar9 = 0x3d;
  do {
    if (((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 2)) &&
       ((uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && ((uint)*unaff_r5 <= uVar9 - 1)) &&
        (uVar2 = *unaff_r5, unaff_r5 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*unaff_r5 <= uVar9)) &&
       ((uVar9 < (uint)*unaff_r5 + (uint)unaff_r5[2] &&
        ((unaff_r5[1] < 0x7b && (0x7a < (uint)unaff_r5[1] + (uint)unaff_r5[3])))))) {
      puVar1 = unaff_r5 + 4;
      uVar2 = *unaff_r5;
      unaff_r5 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    pbVar14 = _DAT_20000138;
    bVar7 = DAT_20000136;
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0x106);
  uVar9 = (uint)*(byte *)(unaff_r8 + 0x3c);
  if ((uVar9 != 0) && (DAT_20000126 != 0)) {
    if (uVar9 == 3) {
      if (_DAT_20000138 != (byte *)0x0) {
        if (DAT_2000012c == '\0') {
          uVar9 = (uint)DAT_2000010d;
          if ((uint)(DAT_2000010d >> 4) < (uVar9 & 0xf)) {
            uVar18 = (uint)(DAT_2000010d >> 4);
            uVar24 = (uint)DAT_2000105b;
            do {
              if ((*(int *)(pbVar14 + uVar18 * 4 + 4) != 0) && (*(short *)(pbVar14 + 0xe) != 0)) {
                uVar25 = (uint)*pbVar14;
                uVar2 = *(ushort *)(uVar24 * 4 + 0x80bb40c + uVar18 * 2);
                uVar17 = *(ushort *)(pbVar14 + 0xc);
                iVar13 = *(int *)(pbVar14 + uVar18 * 4 + 4);
                uVar2 = (ushort)(uint)((ulonglong)(uVar25 * (((uint)uVar2 << 0x15) >> 0x1a)) *
                                       0x80808081 >> 0x22) & 0xffe0 |
                        (ushort)((uVar25 * (uVar2 & 0x1f)) / 0xff) |
                        (ushort)(((uint)((ulonglong)(uVar25 * (uVar2 >> 0xb)) * 0x80808081 >> 0x20)
                                 & 0xfffff80) << 4);
                do {
                  iVar27 = (short)uVar17 * 0x1a;
                  iVar20 = (int)(short)(uVar17 + 9);
                  iVar23 = 0xc6;
                  uVar25 = 2;
                  while( true ) {
                    if (((((*(byte *)(iVar13 + iVar27 + ((uVar25 - 2) * 0x10000 >> 0x13)) >>
                            (uVar25 - 2 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar20)) &&
                        (iVar20 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar23 + 0x16U &&
                        (iVar23 + 0x16U < (uint)_DAT_20008356 + (uint)_DAT_20008352)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar23 - (uint)_DAT_20008352) + 0x16) * (uint)_DAT_20008354 +
                       (iVar20 - (uint)_DAT_20008350)) * 2) = uVar2;
                    }
                    if ((((*(byte *)(iVar13 + ((uVar25 - 1) * 0x10000 >> 0x13) + iVar27) >>
                           (uVar25 - 1 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar20)) &&
                       ((iVar20 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
                        (((uint)_DAT_20008352 <= iVar23 + 0x15U &&
                         (iVar23 + 0x15U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar23 - (uint)_DAT_20008352) + 0x15) * (uint)_DAT_20008354 +
                       (iVar20 - (uint)_DAT_20008350)) * 2) = uVar2;
                    }
                    if (((((*(byte *)(iVar13 + ((uVar25 << 0x10) >> 0x13) + iVar27) >> (uVar25 & 7)
                           & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar20)) &&
                        (iVar20 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar23 + 0x14U &&
                        (iVar23 + 0x14U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar23 - (uint)_DAT_20008352) + 0x14) * (uint)_DAT_20008354 +
                       (iVar20 - (uint)_DAT_20008350)) * 2) = uVar2;
                    }
                    if (iVar23 == 0) break;
                    iVar23 = iVar23 + -3;
                    uVar25 = uVar25 + 3;
                  }
                  uVar17 = uVar17 + 1;
                } while ((uint)uVar17 <
                         (uint)*(ushort *)(pbVar14 + 0xc) + (uint)*(ushort *)(pbVar14 + 0xe));
              }
              uVar18 = uVar18 + 1;
            } while (uVar18 != (uVar9 & 0xf));
          }
        }
        else {
          iVar13 = *(int *)(_DAT_20000138 + 4);
          if (iVar13 != 0) {
            uVar9 = ((uint)*_DAT_20000138 * 0x1f) / 0xff;
            uVar17 = (ushort)uVar9 | (ushort)(uVar9 << 0xb);
            iVar27 = 0;
            do {
              iVar23 = iVar27 * 0x1a;
              uVar9 = iVar27 * 0x10000 + 0x3b0000 >> 0x10;
              iVar20 = 0xc9;
              uVar24 = 2;
              do {
                if ((((*(byte *)(iVar13 + ((uVar24 - 2) * 0x1000000 >> 0x1b) + iVar23) >>
                       (uVar24 - 2 & 7) & 1) != 0) && (_DAT_20008350 <= uVar9)) &&
                   ((uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar20 + 0x13U &&
                     (iVar20 + 0x13U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar20 - (uint)_DAT_20008352) + 0x13) * (uint)_DAT_20008354 +
                   (uVar9 - _DAT_20008350)) * 2) = uVar17;
                }
                if (((((*(byte *)(iVar13 + ((uVar24 - 1) * 0x1000000 >> 0x1b) + iVar23) >>
                        (uVar24 - 1 & 7) & 1) != 0) && (_DAT_20008350 <= uVar9)) &&
                    (uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
                   (((uint)_DAT_20008352 <= iVar20 + 0x12U &&
                    (iVar20 + 0x12U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar20 - (uint)_DAT_20008352) + 0x12) * (uint)_DAT_20008354 +
                   (uVar9 - _DAT_20008350)) * 2) = uVar17;
                }
                if ((((*(byte *)(iVar13 + ((uVar24 << 0x18) >> 0x1b) + iVar23) >> (uVar24 & 7) & 1)
                      != 0) && (_DAT_20008350 <= uVar9)) &&
                   ((uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar20 + 0x11U &&
                     (iVar20 + 0x11U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar20 - (uint)_DAT_20008352) + 0x11) * (uint)_DAT_20008354 +
                   (uVar9 - _DAT_20008350)) * 2) = uVar17;
                }
                iVar20 = iVar20 + -3;
                uVar24 = uVar24 + 3;
              } while (iVar20 != 0);
              iVar27 = iVar27 + 1;
            } while (iVar27 != 0xc9);
          }
        }
      }
    }
    else {
      DAT_20000136 = DAT_20000135;
      bVar6 = DAT_20000135;
      if (DAT_20000135 <= bVar7) {
        bVar6 = DAT_20000135 + 100;
      }
      DAT_20000137 = bVar6 - bVar7;
      if (_DAT_20000138 != (byte *)0x0) {
        pbVar14 = &DAT_20000138;
        uVar37 = (ulonglong)DAT_08014350 >> 0x20;
        uVar21 = (undefined4)DAT_08014350;
        pbVar28 = _DAT_20000138;
        while( true ) {
          cVar5 = DAT_20000137;
          uVar38 = FUN_0803e5da(*pbVar28);
          uVar31 = (undefined4)((ulonglong)uVar38 >> 0x20);
          uVar39 = FUN_0803e5da(cVar5);
          uVar39 = FUN_0803e77c((int)uVar39,(int)((ulonglong)uVar39 >> 0x20),uVar21,(int)uVar37);
          uVar35 = FUN_0803e50a(3 - uVar9);
          uVar39 = FUN_0803e77c((int)uVar39,(int)((ulonglong)uVar39 >> 0x20),(int)uVar35,
                                (int)((ulonglong)uVar35 >> 0x20));
          uVar34 = (undefined4)((ulonglong)uVar39 >> 0x20);
          iVar13 = FUN_0803edf0((int)uVar39,uVar34,(int)uVar38,uVar31);
          if (iVar13 != 0) break;
          FUN_0803eb94((int)uVar38,uVar31,(int)uVar39,uVar34);
          bVar7 = FUN_0803e450();
          cVar5 = DAT_2000012c;
          *pbVar28 = bVar7;
          if (cVar5 == '\0') {
            uVar9 = (uint)DAT_2000010d;
            if ((uint)(DAT_2000010d >> 4) < (uVar9 & 0xf)) {
              uVar24 = (uint)(DAT_2000010d >> 4);
              do {
                if (((*(int *)(_DAT_20000138 + uVar24 * 4 + 4) != 0) &&
                    (1 < *(ushort *)(pbVar28 + 0xe))) && (1 < *(ushort *)(pbVar28 + 0xe))) {
                  uVar9 = 0;
                  uVar17 = 0;
                  do {
                    FUN_08018ea8((int)(short)(*(short *)(pbVar28 + 0xc) + uVar17 + 9),
                                 0xf8 - (uint)*(byte *)(*(int *)(pbVar28 + uVar24 * 4 + 4) + uVar9),
                                 (int)(short)(*(short *)(pbVar28 + 0xc) + uVar17 + 10),
                                 0xf8 - (uint)*(byte *)(uVar9 + *(int *)(pbVar28 + uVar24 * 4 + 4) +
                                                       1));
                    pbVar28 = *(byte **)pbVar14;
                    uVar17 = uVar17 + 1;
                    uVar9 = (uint)uVar17;
                  } while ((int)uVar9 < (int)(*(ushort *)(pbVar28 + 0xe) - 1));
                  uVar9 = (uint)DAT_2000010d;
                }
                uVar24 = uVar24 + 1;
              } while (uVar24 < (uVar9 & 0xf));
            }
          }
          else if (((*(int *)(_DAT_20000138 + 4) != 0) && (*(int *)(_DAT_20000138 + 8) != 0)) &&
                  ((1 < *(ushort *)(pbVar28 + 0xe) &&
                   (uVar24 = (uint)*(ushort *)(pbVar28 + 0xc), uVar9 = uVar24,
                   (int)uVar24 < (int)(*(ushort *)(pbVar28 + 0xe) + uVar24 + -1))))) {
            do {
              FUN_08018da0(*(byte *)(*(int *)(pbVar28 + 4) + uVar24) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar28 + 8) + uVar24),
                           *(byte *)(*(int *)(pbVar28 + 4) + uVar24 + 1) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar28 + 8) + uVar24 + 1));
              pbVar28 = *(byte **)pbVar14;
              uVar24 = uVar9 + 1 & 0xffff;
              uVar9 = uVar9 + 1;
            } while ((int)uVar24 <
                     (int)((uint)*(ushort *)(pbVar28 + 0xc) + (uint)*(ushort *)(pbVar28 + 0xe) + -1)
                    );
          }
          pbVar14 = pbVar28 + 0x10;
          pbVar28 = *(byte **)pbVar14;
          if (pbVar28 == (byte *)0x0) goto LAB_080144ca;
LAB_08014000:
          uVar9 = (uint)DAT_20000134;
        }
        if (*(int *)(pbVar28 + 4) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar9 = *(int *)(pbVar28 + 4) - _DAT_20001078;
            if (uVar9 >> 10 < 0xaf) {
              uVar9 = uVar9 >> 5;
              uVar24 = (uint)*(ushort *)(_DAT_2000107c + uVar9 * 2);
              if (uVar24 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar9 * 2,uVar24 << 1);
              }
            }
          }
        }
        if (*(int *)(*(int *)pbVar14 + 8) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar9 = *(int *)(*(int *)pbVar14 + 8) - _DAT_20001078;
            if (uVar9 >> 10 < 0xaf) {
              uVar9 = uVar9 >> 5;
              uVar24 = (uint)*(ushort *)(_DAT_2000107c + uVar9 * 2);
              if (uVar24 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar9 * 2,uVar24 << 1);
              }
            }
          }
        }
        pbVar28 = *(byte **)(*(int *)pbVar14 + 0x10);
        if (_DAT_20000138 != (byte *)0x0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else if ((uint)((int)_DAT_20000138 - _DAT_20001078) >> 10 < 0xaf) {
            uVar9 = (uint)((int)_DAT_20000138 - _DAT_20001078) >> 5;
            uVar24 = (uint)*(ushort *)(_DAT_2000107c + uVar9 * 2);
            if (uVar24 != 0) {
              FUN_080012bc(_DAT_2000107c + uVar9 * 2,uVar24 << 1);
            }
          }
        }
        pbVar14 = &DAT_20000138;
        _DAT_20000138 = pbVar28;
        if (pbVar28 != (byte *)0x0) goto LAB_08014000;
      }
    }
  }
LAB_080144ca:
  fVar4 = DAT_08014948;
  uVar9 = (uint)_DAT_20000eae;
  if (uVar9 == 0) {
LAB_08014800:
    if (DAT_2000012c == '\0') {
      pbVar14 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar14 = &DAT_20000eb7;
      }
      bVar7 = *pbVar14;
      if ((uint)(bVar7 >> 4) < (bVar7 & 0xf)) {
        uVar9 = (uint)(bVar7 >> 4);
        sVar8 = (ushort)(bVar7 >> 4) * 0xad + 9;
        do {
          if (*(char *)(uVar9 + 0x20000102) != '\0') {
            piVar10 = (int *)(uVar9 * 4 + 0x20000104);
            iVar13 = *piVar10;
            if (iVar13 != 0) {
              for (iVar27 = 0; iVar20 = (int)(short)(sVar8 + (short)iVar27),
                  FUN_08018ea8(iVar20,0x46,iVar20,0x46 - (uint)*(byte *)(iVar13 + iVar27)),
                  iVar27 != 0x7f; iVar27 = iVar27 + 1) {
                iVar13 = *piVar10;
              }
            }
          }
          uVar9 = uVar9 + 1;
          sVar8 = sVar8 + 0xad;
        } while (uVar9 != (bVar7 & 0xf));
        if (DAT_2000012c != '\0') goto LAB_080148ec;
      }
      if (_DAT_20000112 < 0x92) {
        if (_DAT_20000112 < -0x91) {
          iVar13 = 0x10;
          iVar27 = 0x10;
          uVar21 = 0x1e;
        }
        else {
          iVar13 = (int)(short)(_DAT_20000112 + 0x9a);
          iVar27 = (int)(short)(_DAT_20000112 + 0xa4);
          uVar21 = 0x14;
        }
      }
      else {
        iVar13 = 0x12e;
        iVar27 = 0x12e;
        uVar21 = 0x1e;
      }
      FUN_08019af8(iVar13,0x14,iVar27,uVar21);
    }
  }
  else {
    if (DAT_2000012c == '\0') {
      pbVar14 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar14 = &DAT_20000eb7;
      }
      bVar7 = *pbVar14;
      if ((uint)(bVar7 >> 4) < (bVar7 & 0xf)) {
        uVar24 = (uint)(bVar7 >> 4);
        do {
          if (1 < uVar9) {
            if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))
               ) {
              uVar18 = 0;
              while( true ) {
                uVar22 = _DAT_20000f04;
                uVar25 = _DAT_20000f00;
                uVar37 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9),
                                      _DAT_20000f04 * uVar9 +
                                      (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9 >> 0x20),
                                      1000,0);
                uVar26 = (uint)DAT_20000efc;
                iVar13 = *(int *)(uVar24 * 4 + 0x20000ef4);
                uVar29 = uVar26;
                if (iVar13 == 0) {
                  uVar29 = 1;
                }
                lVar36 = (ulonglong)uVar29 * (uVar37 & 0xffffffff);
                uVar11 = (uint)lVar36;
                uVar16 = uVar29 * (int)(uVar37 >> 0x20) + (int)((ulonglong)lVar36 >> 0x20);
                uVar19 = 0x12d - _DAT_20000ed8;
                uVar29 = (int)uVar19 >> 0x1f;
                if (uVar29 < uVar16 || uVar16 - uVar29 < (uint)(uVar19 <= uVar11)) {
                  uVar11 = uVar19;
                  uVar16 = uVar29;
                }
                if (-(uVar16 - (uVar11 == 0)) < (uint)(uVar11 - 1 <= (uVar18 & 0xffff))) break;
                fVar32 = (float)VectorUnsignedToFloat(uVar18 & 0xffff,(byte)(in_fpscr >> 0x15) & 3);
                fVar12 = (float)FUN_0803ee1a(uVar25,uVar22);
                fVar30 = (float)VectorUnsignedToFloat(uVar26,(byte)(in_fpscr >> 0x15) & 3);
                if (iVar13 == 0) {
                  fVar30 = 1.0;
                }
                iVar13 = uVar24 * 0x12d + 0x200000f8 + (uVar18 & 0xffff);
                fVar33 = (float)VectorUnsignedToFloat
                                          ((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
                fVar33 = fVar32 / ((fVar12 / fVar4) * fVar30) + fVar33;
                FUN_08018ea8((int)(fVar33 + 9.0),0xf8 - (uint)*(byte *)(iVar13 + 0x356),
                             (int)(fVar33 + 10.0),0xf8 - (uint)*(byte *)(iVar13 + 0x357));
                uVar9 = (uint)_DAT_20000eae;
                uVar18 = uVar18 + 1;
              }
            }
            else {
              uVar25 = (uint)_DAT_20000eac;
              uVar18 = uVar25;
              if ((int)uVar25 < (int)(uVar25 + uVar9 + -1)) {
                do {
                  iVar13 = uVar25 + uVar24 * 0x12d + 0x200000f8;
                  FUN_08018ea8((int)(short)((short)uVar18 + 9),
                               0xf8 - (uint)*(byte *)(iVar13 + 0x356),
                               (int)(short)((short)uVar18 + 10),
                               0xf8 - (uint)*(byte *)(iVar13 + 0x357));
                  uVar9 = (uint)_DAT_20000eae;
                  uVar25 = uVar18 + 1 & 0xffff;
                  uVar18 = uVar18 + 1;
                } while ((int)uVar25 < (int)(_DAT_20000eac + uVar9 + -1));
              }
            }
          }
          uVar24 = uVar24 + 1;
        } while (uVar24 != (bVar7 & 0xf));
      }
      goto LAB_08014800;
    }
    if (1 < uVar9) {
      if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
        uVar24 = 0;
        while( true ) {
          uVar25 = uVar24 & 0xffff;
          lVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9),
                                _DAT_20000f04 * uVar9 +
                                (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9 >> 0x20),1000,0);
          uVar9 = (uint)((ulonglong)lVar36 >> 0x20);
          uVar18 = 0x12d - _DAT_20000ed8;
          if ((uint)((int)uVar18 >> 0x1f) < uVar9 ||
              uVar9 - ((int)uVar18 >> 0x1f) < (uint)(uVar18 <= (uint)lVar36)) {
            lVar36 = (longlong)(int)uVar18;
          }
          if (-((int)((ulonglong)lVar36 >> 0x20) - (uint)((int)lVar36 == 0)) <
              (uint)((int)lVar36 - 1U <= uVar25)) break;
          FUN_08018da0(*(byte *)(uVar25 + 0x2000044e) + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057b)[uVar25],
                       (byte)(&DAT_2000044f)[uVar24 & 0xffff] + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057c)[uVar24 & 0xffff]);
          uVar9 = (uint)_DAT_20000eae;
          uVar24 = uVar24 + 1;
        }
      }
      else {
        uVar18 = (uint)_DAT_20000eac;
        uVar24 = uVar18;
        if ((int)uVar18 < (int)(uVar18 + uVar9 + -1)) {
          do {
            FUN_08018da0(*(byte *)(uVar18 + 0x2000044e) + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057b)[uVar18],
                         (byte)(&DAT_2000044f)[uVar18] + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057c)[uVar18]);
            uVar18 = uVar24 + 1 & 0xffff;
            uVar24 = uVar24 + 1;
          } while ((int)uVar18 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
        }
      }
      goto LAB_08014800;
    }
  }
LAB_080148ec:
  fVar12 = DAT_08014d00;
  fVar4 = DAT_08014948;
  if ((((_DAT_20000144 == 0) || (DAT_2000012c != '\0')) || (uVar9 = (uint)DAT_2000010c, uVar9 == 0))
     || (DAT_20000140 != '\0')) {
LAB_08014c4c:
    if (DAT_2000012c == '\0') goto LAB_08014c52;
  }
  else {
    uVar18 = ~(uVar9 << 4) & 0x10;
    uVar24 = 0;
    bVar7 = 0;
    if ((int)(uVar9 << 0x1e) < 0) {
      do {
        if ((uVar24 & 0xff) == 0) {
          uVar24 = uVar18;
        }
        uVar9 = uVar24 & 0xff;
        if ((_DAT_20000144 >> uVar9 & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        if (0x1e < uVar9) break;
        if ((_DAT_20000144 >> (uVar24 + 1 & 0xff) & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        if (uVar9 == 0x1e) break;
        if ((_DAT_20000144 >> (uVar24 + 2 & 0xff) & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        if (0x1c < uVar9) break;
        if ((_DAT_20000144 >> (uVar24 + 3 & 0xff) & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        uVar24 = uVar24 + 4;
      } while (uVar9 != 0x1c);
    }
    else {
      do {
        if ((uVar24 & 0xff) == 0) {
          uVar24 = uVar18;
        }
        uVar9 = uVar24 & 0xff;
        if (uVar9 == 0x10) break;
        if ((_DAT_20000144 >> uVar9 & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        uVar24 = uVar24 + 1;
      } while (uVar9 < 0x1f);
    }
    if (bVar7 != 0) {
      bVar3 = DAT_20001058 == '\x01';
      iVar13 = ((int)(bVar7 - 1) / 3) * 0xfff1 + 0xcc;
      uVar9 = (uint)bVar7 % 3;
      uVar24 = 0;
      sVar8 = 0xc;
      do {
        uVar25 = uVar18 + 1;
        if (uVar9 == (uVar24 & 0xff) && uVar9 != 0) {
          sVar8 = 0xc;
          iVar13 = iVar13 + 0xf;
        }
        if ((uVar25 & 0xff) < 0x21) {
          uVar25 = 0x20;
        }
        do {
          uVar22 = uVar18;
          if ((_DAT_20000144 >> (uVar18 & 0xff) & 1) != 0) {
LAB_08014a8a:
            uVar18 = uVar22 + 1;
            if (0xf < (uVar22 & 0xff)) goto LAB_08014a96;
            piVar10 = (int *)(&DAT_20000208 + (uVar22 & 0xff) * 4);
            goto LAB_08014ab2;
          }
          uVar22 = uVar18 + 1;
          if (0x1f < (uVar22 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar22 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar22 = uVar18 + 2;
          if (0x1f < (uVar22 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar22 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar22 = uVar18 + 3;
          if (0x1f < (uVar22 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar22 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar18 = uVar18 + 4;
        } while ((uVar18 & 0xff) < 0x20);
        uVar18 = uVar25 + 1;
        uVar22 = uVar25;
LAB_08014a96:
        uVar22 = uVar22 - 0x10;
        piVar10 = (int *)(&DAT_20000238 + (uVar22 & 0xff) * 4);
LAB_08014ab2:
        iVar20 = *piVar10;
        iVar27 = iVar20;
        if (iVar20 < 1) {
          iVar27 = SignedSaturate(-iVar20,0x20);
          SignedDoesSaturate(iVar27,0x20);
        }
        if (999999 < iVar27) {
          iVar20 = iVar20 / 1000;
        }
        iVar27 = iVar20;
        if (iVar20 < 1) {
          iVar27 = SignedSaturate(-iVar20,0x20);
          SignedDoesSaturate(iVar27,0x20);
        }
        if (999999 < iVar27) {
          iVar20 = iVar20 / 1000;
        }
        iVar27 = iVar20;
        if (iVar20 < 1) {
          iVar27 = SignedSaturate(-iVar20,0x20);
          SignedDoesSaturate(iVar27,0x20);
        }
        if (999999 < iVar27) {
          iVar20 = iVar20 / 1000;
        }
        fVar30 = (float)VectorSignedToFloat(iVar20,(byte)(in_fpscr >> 0x15) & 3);
        if ((uVar22 & 0xfe) == 2) {
          fVar30 = fVar30 / fVar12;
        }
        if ((uVar22 & 0xff) - 6 < 6) {
          fVar30 = fVar30 / fVar4;
        }
        fVar32 = ABS(fVar30);
        uVar25 = in_fpscr & 0xfffffff | (uint)(fVar32 < fVar4) << 0x1f;
        uVar29 = uVar25 | (uint)(NAN(fVar32) || NAN(fVar4)) << 0x1c;
        if ((byte)(uVar25 >> 0x1f) == ((byte)(uVar29 >> 0x1c) & 1)) {
          fVar32 = fVar32 / fVar4;
          fVar30 = fVar30 / fVar4;
        }
        uVar21 = *(undefined4 *)
                  (&DAT_0804bf28 + (uVar22 & 0xff) * 4 + ((uint)bVar3 | (uint)bVar3 << 1) * 0x10);
        FUN_0803ed70(fVar30);
        uVar25 = uVar29 & 0xfffffff | (uint)(fVar32 < fVar12) << 0x1f;
        in_fpscr = uVar25 | (uint)(NAN(fVar32) || NAN(fVar12)) << 0x1c;
        pcVar15 = s__s__1f_s_08015a70;
        if ((byte)(uVar25 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1)) {
          pcVar15 = s__s__2f_s_08015a64;
        }
        FUN_080003b4(&DAT_20008360,pcVar15,uVar21);
        FUN_08008154((int)sVar8,(int)(short)iVar13,100,0xf);
        uVar24 = uVar24 + 1;
        if ((uint)bVar7 <= (uVar24 & 0xff)) goto LAB_08014c4c;
        sVar8 = sVar8 + 100;
        if (0x137 < sVar8) {
          iVar13 = iVar13 + 0xf;
          sVar8 = 0xc;
        }
      } while( true );
    }
LAB_08014c52:
    if ((DAT_20000332 & 1) != 0) {
      FUN_08018ea8((int)(short)(_DAT_2000032a + 0x9f),0x14,(int)(short)(_DAT_2000032a + 0x9f),0xdc);
      FUN_08018ea8((int)(short)(_DAT_2000032c + 0x9f),0x14,(int)(short)(_DAT_2000032c + 0x9f),0xdc);
    }
    uVar9 = (uint)DAT_20000332;
    if ((int)(uVar9 << 0x1e) < 0) {
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_2000032e),0x135,(int)(short)(0x78 - _DAT_2000032e));
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_20000330),0x135,(int)(short)(0x78 - _DAT_20000330));
      uVar9 = (uint)DAT_20000332;
    }
    if (uVar9 != 0) {
      sVar8 = 0x19;
      bVar7 = 0;
      if ((uVar9 & 1) != 0) goto LAB_08014d06;
      do {
        if (bVar7 == 0) {
          bVar7 = 3;
        }
        else if (bVar7 == 6) break;
LAB_08014d06:
        do {
          bVar6 = bVar7;
          if (bVar7 == 3) {
            bVar6 = 6;
          }
          if ((int)(uVar9 << 0x1e) < 0) {
            bVar6 = bVar7;
          }
          FUN_08008154(0xe,(int)sVar8,0x32,0x10);
          uVar38 = FUN_0803ed70(*(undefined4 *)(&DAT_20000334 + (uint)bVar6 * 4));
          FUN_080003b4(&DAT_20008360,0x80bcc11,(int)uVar38,(int)((ulonglong)uVar38 >> 0x20));
          FUN_08008154(0x40,(int)sVar8,100,0x10);
          if (5 < bVar6) goto LAB_08014dbe;
          uVar9 = (uint)DAT_20000332;
          bVar7 = bVar6 + 1;
          sVar8 = sVar8 + 0xc;
        } while ((DAT_20000332 & 1) != 0);
      } while( true );
    }
  }
LAB_08014dbe:
  fVar4 = DAT_08014fd0;
  if (((DAT_2000012d != '\0') && (_DAT_20000130 != 0)) &&
     ((DAT_2000012c == '\0' && (uVar9 = (uint)_DAT_20000eae, 1 < uVar9)))) {
    if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
      uVar18 = (uint)(DAT_20000eb7 >> 4);
      uVar25 = DAT_20000eb7 & 0xf;
      uVar24 = 0;
      do {
        uVar22 = uVar24;
        if (((uVar25 <= uVar18) || (uVar22 = uVar18, uVar25 <= uVar18 + 1)) ||
           (uVar29 = uVar18 + 2, uVar22 = uVar18 + 1, uVar25 <= uVar29)) break;
        uVar24 = uVar18 + 3;
        uVar18 = uVar18 + 4;
        uVar22 = uVar29;
      } while (uVar24 < uVar25);
      uVar24 = 0;
      while( true ) {
        uVar25 = _DAT_20000f04;
        uVar18 = _DAT_20000f00;
        uVar29 = uVar24 & 0xffff;
        uVar37 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9),
                              _DAT_20000f04 * uVar9 +
                              (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9 >> 0x20),1000,0);
        uVar26 = (uint)DAT_20000efc;
        iVar13 = *(int *)(uVar22 * 4 + 0x20000ef4);
        uVar9 = uVar26;
        if (iVar13 == 0) {
          uVar9 = 1;
        }
        lVar36 = (ulonglong)uVar9 * (uVar37 & 0xffffffff);
        uVar11 = (uint)lVar36;
        uVar16 = uVar9 * (int)(uVar37 >> 0x20) + (int)((ulonglong)lVar36 >> 0x20);
        uVar19 = 0x12d - _DAT_20000ed8;
        uVar9 = (int)uVar19 >> 0x1f;
        if (uVar9 < uVar16 || uVar16 - uVar9 < (uint)(uVar19 <= uVar11)) {
          uVar11 = uVar19;
          uVar16 = uVar9;
        }
        if (-(uVar16 - (uVar11 == 0)) < (uint)(uVar11 - 1 <= uVar29)) break;
        fVar32 = (float)VectorUnsignedToFloat(uVar29,(byte)(in_fpscr >> 0x15) & 3);
        fVar12 = (float)FUN_0803ee1a(uVar18,uVar25);
        fVar30 = (float)VectorUnsignedToFloat(uVar26,(byte)(in_fpscr >> 0x15) & 3);
        if (iVar13 == 0) {
          fVar30 = 1.0;
        }
        fVar33 = (float)VectorUnsignedToFloat((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
        fVar33 = fVar32 / ((fVar12 / fVar4) * fVar30) + fVar33;
        FUN_08018ea8((int)(fVar33 + 9.0),0x78 - *(char *)(_DAT_20000130 + uVar29),
                     (int)(fVar33 + 10.0),0x78 - *(char *)(_DAT_20000130 + (uVar24 & 0xffff) + 1));
        uVar9 = (uint)_DAT_20000eae;
        uVar24 = uVar24 + 1;
      }
    }
    else {
      uVar18 = (uint)_DAT_20000eac;
      uVar24 = uVar18;
      if ((int)uVar18 < (int)(uVar9 + uVar18 + -1)) {
        do {
          FUN_08018ea8((int)(short)((short)uVar24 + 9),0x78 - *(char *)(_DAT_20000130 + uVar18),
                       (int)(short)((short)uVar24 + 10),0x78 - *(char *)(uVar18 + _DAT_20000130 + 1)
                      );
          uVar24 = uVar24 + 1;
          uVar18 = uVar24 & 0xffff;
        } while ((int)uVar18 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
      }
    }
  }
  if ((_DAT_20000122 != 0) && (DAT_2000012c == '\0')) {
    uVar17 = 0;
    uVar9 = _DAT_20000114 + 100 & 0xffff;
    iVar13 = 0xdc - uVar9;
    if (uVar9 < 0xc9) {
      uVar9 = 200;
    }
    else {
      iVar13 = 0x14;
    }
    iVar27 = (int)(short)iVar13;
    iVar20 = (int)(short)(((short)uVar9 - _DAT_20000114) + -0x50);
    uVar9 = 0xc;
    while( true ) {
      if (((uVar17 < 3) && ((uint)_DAT_20008350 <= uVar9 - 3)) &&
         ((uVar9 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar27 &&
           (iVar27 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((iVar20 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar9) * 2
         + -6) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (uVar9 == 0x138) break;
      if ((((uVar17 < 3) && ((uint)_DAT_20008350 <= uVar9 - 2)) &&
          (uVar9 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar27 &&
          (iVar27 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar20 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2
         + -4) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (((uVar17 < 3) && ((uint)_DAT_20008350 <= uVar9 - 1)) &&
         ((uVar9 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar27 &&
           (iVar27 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar20 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2
         + -2) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if ((((uVar17 < 3) && (_DAT_20008350 <= uVar9)) &&
          (uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar27 &&
          (iVar27 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (iVar20 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2)
             = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      uVar9 = uVar9 + 4;
    }
    uVar9 = (byte)(&DAT_200000fa)[DAT_2000010e] / 3;
    uVar34 = VectorSignedToFloat((int)(short)_DAT_20000114 -
                                 (int)(char)(&DAT_200000fc)[DAT_2000010e],
                                 (byte)(in_fpscr >> 0x15) & 3);
    uVar38 = FUN_0803e5da(*(undefined2 *)
                           (&DAT_0804bfb8 +
                           (uVar9 * -3 + (uint)(byte)(&DAT_200000fa)[DAT_2000010e] & 0xff) * 2));
    uVar21 = FUN_0803e5da(uVar9);
    uVar31 = (undefined4)DAT_08015328;
    uVar21 = FUN_0803c8e0(uVar31,param_2,uVar21);
    uVar38 = FUN_0803e77c(uVar21,extraout_s1,(int)uVar38,(int)((ulonglong)uVar38 >> 0x20));
    uVar21 = FUN_0803e5da(*(undefined1 *)(DAT_2000010e + 0x200000fe));
    uVar21 = FUN_0803c8e0(uVar31,extraout_s1,uVar21);
    uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),uVar21,extraout_s1_00);
    uVar38 = FUN_0803e124((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),(int)DAT_08015330,
                          (int)((ulonglong)DAT_08015330 >> 0x20));
    uVar39 = FUN_0803ed70(uVar34);
    FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),(int)uVar39,
                 (int)((ulonglong)uVar39 >> 0x20));
    fVar12 = (float)FUN_0803df48();
    pcVar15 = (char *)0x8015aa8;
    fVar4 = fVar12 / DAT_08014fd0;
    if (ABS(fVar12) < DAT_08014fd0) {
      pcVar15 = s___2fmV_08015aa0;
      fVar4 = fVar12;
    }
    uVar38 = FUN_0803ed70(fVar4);
    FUN_080003b4(&DAT_20008360,pcVar15,(int)uVar38,(int)((ulonglong)uVar38 >> 0x20));
    sVar8 = (short)iVar13 + -10;
    if (0xd6 < iVar13) {
      sVar8 = 0xca;
    }
    if (iVar13 < 0x1e) {
      sVar8 = 0x14;
    }
    FUN_08008154(0xa0,(int)sVar8,0x7d,0x14);
  }
  if ((_DAT_20000120 != 0) && (DAT_2000012c == '\0')) {
    iVar13 = (int)(short)(_DAT_20000112 + 0x9f);
    uVar17 = 0;
    uVar9 = 0x1f;
    while( true ) {
      if ((((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
          (iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar9 - 3 &&
          (uVar9 - 3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar9 - _DAT_20008352) + -3) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (uVar9 == 0xdf) break;
      if (((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
         ((iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          (((uint)_DAT_20008352 <= uVar9 - 2 &&
           (uVar9 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar9 - _DAT_20008352) + -2) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if ((((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
          (iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar9 - 1 &&
          (uVar9 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((~(uint)_DAT_20008352 + uVar9) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
         ((iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          ((_DAT_20008352 <= uVar9 && (uVar9 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((uVar9 - _DAT_20008352) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2) =
             0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      uVar9 = uVar9 + 4;
    }
    uVar38 = FUN_0803e538(_DAT_20000118,_DAT_2000011c);
    uVar38 = FUN_0803e124((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),(int)DAT_08015338,
                          (int)((ulonglong)DAT_08015338 >> 0x20));
    uVar37 = (ulonglong)DAT_08015340 >> 0x20;
    uVar21 = (undefined4)((ulonglong)DAT_08015330 >> 0x20);
    uVar31 = (undefined4)DAT_08015330;
    while( true ) {
      uStack00000034 = (uint)((ulonglong)uVar38 >> 0x20);
      uVar34 = (undefined4)uVar38;
      iVar13 = FUN_0803ee0c(uVar34,uStack00000034 & 0x7fffffff | (uint)uVar37 & 0x80000000,uVar31,
                            uVar21);
      if (iVar13 != 0) break;
      uVar38 = FUN_0803e124(uVar34,uStack00000034,uVar31,uVar21);
    }
    uStack00000034 = uStack00000034 ^ 0x80000000;
    FUN_080003b4(&DAT_20008360,0x80bcc11,uVar34,uStack00000034);
    FUN_08008154(0,0xb4,0x140,0x14);
  }
  if ((DAT_2000012c != '\0') && (FUN_08021b40(), DAT_2000012c == '\0')) {
    sVar8 = 0x78 - _DAT_20000114;
    if (200 < (ushort)(_DAT_20000114 + 100)) {
      sVar8 = 0x14;
    }
    FUN_08019af8(0x136,(int)sVar8,0x13d,(int)(short)(sVar8 + 5));
  }
  if (((((DAT_20000f08 != -1) && (DAT_20000f08 != '\0')) ||
       ((DAT_20000127 != '\0' || DAT_20000128 != '\0') || DAT_2000012b != '\0')) ||
      ((uVar9 = (uint)DAT_20000126, DAT_2000010f == '\x02' && (uVar9 != 0)))) ||
     ((uVar9 == 0 &&
      (uVar9 = -(uint)(_DAT_20000f00 >= 10) - _DAT_20000f04,
      -_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))))) {
    FUN_08019af8(0xea,0x51,0x45,0x51);
    FUN_08019af8(0x45,0x51,0xef,0x55);
    FUN_08019af8(0xef,0x55,0x3e,0x59);
    FUN_08019af8(0x3e,0x59,0x101,0x55);
    FUN_08019af8(0x101,0x55,0x3e,0xa1);
    FUN_08019af8(0x3e,0xa1,0x101,0x98);
    FUN_08018fcc(0x3d,0x50,0xc6,0x53);
    uVar9 = extraout_r1;
  }
  if ((DAT_20000f08 == '\x03') || (DAT_20000f08 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (DAT_20000f08 == '\x01') {
    DAT_20000f13 = FUN_08034878(0x80bc18b,uVar9);
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,DAT_20000f09);
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    DAT_20000f08 = -1;
    DAT_20001060 = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
  if ((DAT_2000010f == '\x02') && (DAT_20000126 != 0)) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (DAT_20000127 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_20000128 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((DAT_20000126 == 0) && (-_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))) {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_2000012b != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  DAT_20001068 = DAT_20001068 + '\x01';
  return;
}



// Attempting to create function at 0x08010D70
// Successfully created function
// ==========================================
// Function: FUN_08010d70 @ 08010d70
// Size: 1 bytes
// ==========================================

/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08015214 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

void FUN_08010d70(undefined4 param_1,undefined4 param_2,uint param_3,undefined2 param_4,
                 ushort param_5,int param_6)

{
  ushort uVar1;
  bool bVar2;
  float fVar3;
  char cVar4;
  byte bVar5;
  byte bVar6;
  short sVar7;
  uint uVar8;
  int *piVar9;
  uint uVar10;
  float fVar11;
  int iVar12;
  byte *pbVar13;
  char *pcVar14;
  uint uVar15;
  uint extraout_r1;
  ushort uVar16;
  uint uVar17;
  uint uVar18;
  int iVar19;
  undefined4 uVar20;
  uint uVar21;
  int iVar22;
  int unaff_r6;
  uint uVar23;
  uint unaff_r7;
  uint uVar24;
  int unaff_r8;
  uint uVar25;
  int iVar26;
  byte *pbVar27;
  uint in_fpscr;
  uint uVar28;
  float fVar29;
  undefined4 uVar30;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  float fVar31;
  float fVar32;
  undefined4 uVar33;
  undefined8 uVar34;
  longlong lVar35;
  ulonglong uVar36;
  undefined8 uVar37;
  undefined8 uVar38;
  uint uStack00000034;
  undefined4 in_stack_0000003c;
  undefined8 in_stack_00000040;
  undefined8 in_stack_00000048;
  undefined8 in_stack_00000050;
  undefined8 in_stack_00000058;
  undefined8 in_stack_00000060;
  undefined4 in_stack_0000006c;
  undefined4 in_stack_00000070;
  undefined4 in_stack_00000074;
  undefined4 in_stack_00000078;
  undefined4 in_stack_0000007c;
  undefined4 in_stack_00000080;
  undefined4 in_stack_00000084;
  undefined4 in_stack_00000088;
  undefined4 in_stack_0000008c;
  
code_r0x08010d70:
  *(undefined2 *)(param_6 + (unaff_r6 - unaff_r7) * 2 + 0x140) = param_4;
  uVar8 = param_3;
code_r0x08010c30:
  param_5 = param_5 + 1;
  param_3 = uVar8 + 3;
  if (4 < param_5) {
    param_5 = 0;
  }
  if (param_3 != 0xdf) {
    if ((((param_5 == 0) && (_DAT_20008350 < 0xa1)) &&
        (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 + 1 && (uVar8 + 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((param_3 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140)
           = param_4;
    }
    param_5 = param_5 + 1;
    if (4 < param_5) {
      param_5 = 0;
    }
    if (((param_5 == 0) && (_DAT_20008350 < 0xa1)) &&
       ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 + 2 &&
         (uVar8 + 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + param_3) - (uint)_DAT_20008350) * 2 + 0x140)
           = param_4;
    }
    param_5 = param_5 + 1;
    if (4 < param_5) {
      param_5 = 0;
    }
    uVar8 = param_3;
    if ((((param_5 != 0) || (0xa0 < _DAT_20008350)) ||
        ((uint)_DAT_20008350 + (uint)_DAT_20008354 < 0xa1)) ||
       ((param_3 < _DAT_20008352 || ((uint)_DAT_20008352 + (uint)_DAT_20008356 <= param_3))))
    goto code_r0x08010c30;
    unaff_r7 = (uint)_DAT_20008350;
    unaff_r6 = (uint)_DAT_20008354 * (param_3 - _DAT_20008352);
    param_6 = _DAT_20008358;
    goto code_r0x08010d70;
  }
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
       ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
        (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
       ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x142) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x3d;
  do {
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
        (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
       ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
        (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0x106);
  uVar16 = 0;
  uVar8 = 0x3d;
  do {
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
       ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
        (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
       ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0x106);
  uVar16 = 0;
  uVar8 = 0x3d;
  do {
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
        (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
       ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
        (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0x106);
  uVar16 = 0;
  uVar8 = 0x3d;
  do {
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
       ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
        (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
       ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    pbVar13 = _DAT_20000138;
    bVar6 = DAT_20000136;
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0x106);
  uVar8 = (uint)*(byte *)(unaff_r8 + 0x3c);
  if ((uVar8 != 0) && (DAT_20000126 != 0)) {
    if (uVar8 == 3) {
      if (_DAT_20000138 != (byte *)0x0) {
        if (DAT_2000012c == '\0') {
          uVar8 = (uint)DAT_2000010d;
          if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
            uVar17 = (uint)(DAT_2000010d >> 4);
            uVar23 = (uint)DAT_2000105b;
            do {
              if ((*(int *)(pbVar13 + uVar17 * 4 + 4) != 0) && (*(short *)(pbVar13 + 0xe) != 0)) {
                uVar24 = (uint)*pbVar13;
                uVar1 = *(ushort *)(uVar23 * 4 + 0x80bb40c + uVar17 * 2);
                uVar16 = *(ushort *)(pbVar13 + 0xc);
                iVar12 = *(int *)(pbVar13 + uVar17 * 4 + 4);
                uVar1 = (ushort)(uint)((ulonglong)(uVar24 * (((uint)uVar1 << 0x15) >> 0x1a)) *
                                       0x80808081 >> 0x22) & 0xffe0 |
                        (ushort)((uVar24 * (uVar1 & 0x1f)) / 0xff) |
                        (ushort)(((uint)((ulonglong)(uVar24 * (uVar1 >> 0xb)) * 0x80808081 >> 0x20)
                                 & 0xfffff80) << 4);
                do {
                  iVar26 = (short)uVar16 * 0x1a;
                  iVar19 = (int)(short)(uVar16 + 9);
                  iVar22 = 0xc6;
                  uVar24 = 2;
                  while( true ) {
                    if (((((*(byte *)(iVar12 + iVar26 + ((uVar24 - 2) * 0x10000 >> 0x13)) >>
                            (uVar24 - 2 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x16U &&
                        (iVar22 + 0x16U < (uint)_DAT_20008356 + (uint)_DAT_20008352)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x16) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if ((((*(byte *)(iVar12 + ((uVar24 - 1) * 0x10000 >> 0x13) + iVar26) >>
                           (uVar24 - 1 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                       ((iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
                        (((uint)_DAT_20008352 <= iVar22 + 0x15U &&
                         (iVar22 + 0x15U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x15) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (((((*(byte *)(iVar12 + ((uVar24 << 0x10) >> 0x13) + iVar26) >> (uVar24 & 7)
                           & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x14U &&
                        (iVar22 + 0x14U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x14) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (iVar22 == 0) break;
                    iVar22 = iVar22 + -3;
                    uVar24 = uVar24 + 3;
                  }
                  uVar16 = uVar16 + 1;
                } while ((uint)uVar16 <
                         (uint)*(ushort *)(pbVar13 + 0xc) + (uint)*(ushort *)(pbVar13 + 0xe));
              }
              uVar17 = uVar17 + 1;
            } while (uVar17 != (uVar8 & 0xf));
          }
        }
        else {
          iVar12 = *(int *)(_DAT_20000138 + 4);
          if (iVar12 != 0) {
            uVar8 = ((uint)*_DAT_20000138 * 0x1f) / 0xff;
            uVar16 = (ushort)uVar8 | (ushort)(uVar8 << 0xb);
            iVar26 = 0;
            do {
              iVar22 = iVar26 * 0x1a;
              uVar8 = iVar26 * 0x10000 + 0x3b0000 >> 0x10;
              iVar19 = 0xc9;
              uVar23 = 2;
              do {
                if ((((*(byte *)(iVar12 + ((uVar23 - 2) * 0x1000000 >> 0x1b) + iVar22) >>
                       (uVar23 - 2 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x13U &&
                     (iVar19 + 0x13U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x13) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                if (((((*(byte *)(iVar12 + ((uVar23 - 1) * 0x1000000 >> 0x1b) + iVar22) >>
                        (uVar23 - 1 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                    (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
                   (((uint)_DAT_20008352 <= iVar19 + 0x12U &&
                    (iVar19 + 0x12U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x12) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                if ((((*(byte *)(iVar12 + ((uVar23 << 0x18) >> 0x1b) + iVar22) >> (uVar23 & 7) & 1)
                      != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x11U &&
                     (iVar19 + 0x11U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x11) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                iVar19 = iVar19 + -3;
                uVar23 = uVar23 + 3;
              } while (iVar19 != 0);
              iVar26 = iVar26 + 1;
            } while (iVar26 != 0xc9);
          }
        }
      }
    }
    else {
      DAT_20000136 = DAT_20000135;
      bVar5 = DAT_20000135;
      if (DAT_20000135 <= bVar6) {
        bVar5 = DAT_20000135 + 100;
      }
      DAT_20000137 = bVar5 - bVar6;
      if (_DAT_20000138 != (byte *)0x0) {
        pbVar13 = &DAT_20000138;
        uVar36 = (ulonglong)DAT_08014350 >> 0x20;
        uVar20 = (undefined4)DAT_08014350;
        pbVar27 = _DAT_20000138;
        while( true ) {
          cVar4 = DAT_20000137;
          uVar37 = FUN_0803e5da(*pbVar27);
          uVar30 = (undefined4)((ulonglong)uVar37 >> 0x20);
          uVar38 = FUN_0803e5da(cVar4);
          uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),uVar20,(int)uVar36);
          uVar34 = FUN_0803e50a(3 - uVar8);
          uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),(int)uVar34,
                                (int)((ulonglong)uVar34 >> 0x20));
          uVar33 = (undefined4)((ulonglong)uVar38 >> 0x20);
          iVar12 = FUN_0803edf0((int)uVar38,uVar33,(int)uVar37,uVar30);
          if (iVar12 != 0) break;
          FUN_0803eb94((int)uVar37,uVar30,(int)uVar38,uVar33);
          bVar6 = FUN_0803e450();
          cVar4 = DAT_2000012c;
          *pbVar27 = bVar6;
          if (cVar4 == '\0') {
            uVar8 = (uint)DAT_2000010d;
            if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
              uVar23 = (uint)(DAT_2000010d >> 4);
              do {
                if (((*(int *)(_DAT_20000138 + uVar23 * 4 + 4) != 0) &&
                    (1 < *(ushort *)(pbVar27 + 0xe))) && (1 < *(ushort *)(pbVar27 + 0xe))) {
                  uVar8 = 0;
                  uVar16 = 0;
                  do {
                    FUN_08018ea8((int)(short)(*(short *)(pbVar27 + 0xc) + uVar16 + 9),
                                 0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + uVar23 * 4 + 4) + uVar8),
                                 (int)(short)(*(short *)(pbVar27 + 0xc) + uVar16 + 10),
                                 0xf8 - (uint)*(byte *)(uVar8 + *(int *)(pbVar27 + uVar23 * 4 + 4) +
                                                       1));
                    pbVar27 = *(byte **)pbVar13;
                    uVar16 = uVar16 + 1;
                    uVar8 = (uint)uVar16;
                  } while ((int)uVar8 < (int)(*(ushort *)(pbVar27 + 0xe) - 1));
                  uVar8 = (uint)DAT_2000010d;
                }
                uVar23 = uVar23 + 1;
              } while (uVar23 < (uVar8 & 0xf));
            }
          }
          else if (((*(int *)(_DAT_20000138 + 4) != 0) && (*(int *)(_DAT_20000138 + 8) != 0)) &&
                  ((1 < *(ushort *)(pbVar27 + 0xe) &&
                   (uVar23 = (uint)*(ushort *)(pbVar27 + 0xc), uVar8 = uVar23,
                   (int)uVar23 < (int)(*(ushort *)(pbVar27 + 0xe) + uVar23 + -1))))) {
            do {
              FUN_08018da0(*(byte *)(*(int *)(pbVar27 + 4) + uVar23) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + 8) + uVar23),
                           *(byte *)(*(int *)(pbVar27 + 4) + uVar23 + 1) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + 8) + uVar23 + 1));
              pbVar27 = *(byte **)pbVar13;
              uVar23 = uVar8 + 1 & 0xffff;
              uVar8 = uVar8 + 1;
            } while ((int)uVar23 <
                     (int)((uint)*(ushort *)(pbVar27 + 0xc) + (uint)*(ushort *)(pbVar27 + 0xe) + -1)
                    );
          }
          pbVar13 = pbVar27 + 0x10;
          pbVar27 = *(byte **)pbVar13;
          if (pbVar27 == (byte *)0x0) goto LAB_080144ca;
LAB_08014000:
          uVar8 = (uint)DAT_20000134;
        }
        if (*(int *)(pbVar27 + 4) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar8 = *(int *)(pbVar27 + 4) - _DAT_20001078;
            if (uVar8 >> 10 < 0xaf) {
              uVar8 = uVar8 >> 5;
              uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
              if (uVar23 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
              }
            }
          }
        }
        if (*(int *)(*(int *)pbVar13 + 8) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar8 = *(int *)(*(int *)pbVar13 + 8) - _DAT_20001078;
            if (uVar8 >> 10 < 0xaf) {
              uVar8 = uVar8 >> 5;
              uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
              if (uVar23 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
              }
            }
          }
        }
        pbVar27 = *(byte **)(*(int *)pbVar13 + 0x10);
        if (_DAT_20000138 != (byte *)0x0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else if ((uint)((int)_DAT_20000138 - _DAT_20001078) >> 10 < 0xaf) {
            uVar8 = (uint)((int)_DAT_20000138 - _DAT_20001078) >> 5;
            uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
            if (uVar23 != 0) {
              FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
            }
          }
        }
        pbVar13 = &DAT_20000138;
        _DAT_20000138 = pbVar27;
        if (pbVar27 != (byte *)0x0) goto LAB_08014000;
      }
    }
  }
LAB_080144ca:
  fVar3 = DAT_08014948;
  uVar8 = (uint)_DAT_20000eae;
  if (uVar8 == 0) {
LAB_08014800:
    if (DAT_2000012c == '\0') {
      pbVar13 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar13 = &DAT_20000eb7;
      }
      bVar6 = *pbVar13;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar8 = (uint)(bVar6 >> 4);
        sVar7 = (ushort)(bVar6 >> 4) * 0xad + 9;
        do {
          if (*(char *)(uVar8 + 0x20000102) != '\0') {
            piVar9 = (int *)(uVar8 * 4 + 0x20000104);
            iVar12 = *piVar9;
            if (iVar12 != 0) {
              for (iVar26 = 0; iVar19 = (int)(short)(sVar7 + (short)iVar26),
                  FUN_08018ea8(iVar19,0x46,iVar19,0x46 - (uint)*(byte *)(iVar12 + iVar26)),
                  iVar26 != 0x7f; iVar26 = iVar26 + 1) {
                iVar12 = *piVar9;
              }
            }
          }
          uVar8 = uVar8 + 1;
          sVar7 = sVar7 + 0xad;
        } while (uVar8 != (bVar6 & 0xf));
        if (DAT_2000012c != '\0') goto LAB_080148ec;
      }
      if (_DAT_20000112 < 0x92) {
        if (_DAT_20000112 < -0x91) {
          iVar12 = 0x10;
          iVar26 = 0x10;
          uVar20 = 0x1e;
        }
        else {
          iVar12 = (int)(short)(_DAT_20000112 + 0x9a);
          iVar26 = (int)(short)(_DAT_20000112 + 0xa4);
          uVar20 = 0x14;
        }
      }
      else {
        iVar12 = 0x12e;
        iVar26 = 0x12e;
        uVar20 = 0x1e;
      }
      FUN_08019af8(iVar12,0x14,iVar26,uVar20);
    }
  }
  else {
    if (DAT_2000012c == '\0') {
      pbVar13 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar13 = &DAT_20000eb7;
      }
      bVar6 = *pbVar13;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar23 = (uint)(bVar6 >> 4);
        do {
          if (1 < uVar8) {
            if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))
               ) {
              uVar17 = 0;
              while( true ) {
                uVar21 = _DAT_20000f04;
                uVar24 = _DAT_20000f00;
                uVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                                      _DAT_20000f04 * uVar8 +
                                      (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),
                                      1000,0);
                uVar25 = (uint)DAT_20000efc;
                iVar12 = *(int *)(uVar23 * 4 + 0x20000ef4);
                uVar28 = uVar25;
                if (iVar12 == 0) {
                  uVar28 = 1;
                }
                lVar35 = (ulonglong)uVar28 * (uVar36 & 0xffffffff);
                uVar10 = (uint)lVar35;
                uVar15 = uVar28 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
                uVar18 = 0x12d - _DAT_20000ed8;
                uVar28 = (int)uVar18 >> 0x1f;
                if (uVar28 < uVar15 || uVar15 - uVar28 < (uint)(uVar18 <= uVar10)) {
                  uVar10 = uVar18;
                  uVar15 = uVar28;
                }
                if (-(uVar15 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= (uVar17 & 0xffff))) break;
                fVar31 = (float)VectorUnsignedToFloat(uVar17 & 0xffff,(byte)(in_fpscr >> 0x15) & 3);
                fVar11 = (float)FUN_0803ee1a(uVar24,uVar21);
                fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
                if (iVar12 == 0) {
                  fVar29 = 1.0;
                }
                iVar12 = uVar23 * 0x12d + 0x200000f8 + (uVar17 & 0xffff);
                fVar32 = (float)VectorUnsignedToFloat
                                          ((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
                fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
                FUN_08018ea8((int)(fVar32 + 9.0),0xf8 - (uint)*(byte *)(iVar12 + 0x356),
                             (int)(fVar32 + 10.0),0xf8 - (uint)*(byte *)(iVar12 + 0x357));
                uVar8 = (uint)_DAT_20000eae;
                uVar17 = uVar17 + 1;
              }
            }
            else {
              uVar24 = (uint)_DAT_20000eac;
              uVar17 = uVar24;
              if ((int)uVar24 < (int)(uVar24 + uVar8 + -1)) {
                do {
                  iVar12 = uVar24 + uVar23 * 0x12d + 0x200000f8;
                  FUN_08018ea8((int)(short)((short)uVar17 + 9),
                               0xf8 - (uint)*(byte *)(iVar12 + 0x356),
                               (int)(short)((short)uVar17 + 10),
                               0xf8 - (uint)*(byte *)(iVar12 + 0x357));
                  uVar8 = (uint)_DAT_20000eae;
                  uVar24 = uVar17 + 1 & 0xffff;
                  uVar17 = uVar17 + 1;
                } while ((int)uVar24 < (int)(_DAT_20000eac + uVar8 + -1));
              }
            }
          }
          uVar23 = uVar23 + 1;
        } while (uVar23 != (bVar6 & 0xf));
      }
      goto LAB_08014800;
    }
    if (1 < uVar8) {
      if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
        uVar23 = 0;
        while( true ) {
          uVar24 = uVar23 & 0xffff;
          lVar35 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                                _DAT_20000f04 * uVar8 +
                                (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),1000,0);
          uVar8 = (uint)((ulonglong)lVar35 >> 0x20);
          uVar17 = 0x12d - _DAT_20000ed8;
          if ((uint)((int)uVar17 >> 0x1f) < uVar8 ||
              uVar8 - ((int)uVar17 >> 0x1f) < (uint)(uVar17 <= (uint)lVar35)) {
            lVar35 = (longlong)(int)uVar17;
          }
          if (-((int)((ulonglong)lVar35 >> 0x20) - (uint)((int)lVar35 == 0)) <
              (uint)((int)lVar35 - 1U <= uVar24)) break;
          FUN_08018da0(*(byte *)(uVar24 + 0x2000044e) + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057b)[uVar24],
                       (byte)(&DAT_2000044f)[uVar23 & 0xffff] + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057c)[uVar23 & 0xffff]);
          uVar8 = (uint)_DAT_20000eae;
          uVar23 = uVar23 + 1;
        }
      }
      else {
        uVar17 = (uint)_DAT_20000eac;
        uVar23 = uVar17;
        if ((int)uVar17 < (int)(uVar17 + uVar8 + -1)) {
          do {
            FUN_08018da0(*(byte *)(uVar17 + 0x2000044e) + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057b)[uVar17],
                         (byte)(&DAT_2000044f)[uVar17] + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057c)[uVar17]);
            uVar17 = uVar23 + 1 & 0xffff;
            uVar23 = uVar23 + 1;
          } while ((int)uVar17 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
        }
      }
      goto LAB_08014800;
    }
  }
LAB_080148ec:
  fVar11 = DAT_08014d00;
  fVar3 = DAT_08014948;
  if ((((_DAT_20000144 == 0) || (DAT_2000012c != '\0')) || (uVar8 = (uint)DAT_2000010c, uVar8 == 0))
     || (DAT_20000140 != '\0')) {
LAB_08014c4c:
    if (DAT_2000012c == '\0') goto LAB_08014c52;
  }
  else {
    uVar17 = ~(uVar8 << 4) & 0x10;
    uVar23 = 0;
    bVar6 = 0;
    if ((int)(uVar8 << 0x1e) < 0) {
      do {
        if ((uVar23 & 0xff) == 0) {
          uVar23 = uVar17;
        }
        uVar8 = uVar23 & 0xff;
        if ((_DAT_20000144 >> uVar8 & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (0x1e < uVar8) break;
        if ((_DAT_20000144 >> (uVar23 + 1 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (uVar8 == 0x1e) break;
        if ((_DAT_20000144 >> (uVar23 + 2 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (0x1c < uVar8) break;
        if ((_DAT_20000144 >> (uVar23 + 3 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        uVar23 = uVar23 + 4;
      } while (uVar8 != 0x1c);
    }
    else {
      do {
        if ((uVar23 & 0xff) == 0) {
          uVar23 = uVar17;
        }
        uVar8 = uVar23 & 0xff;
        if (uVar8 == 0x10) break;
        if ((_DAT_20000144 >> uVar8 & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        uVar23 = uVar23 + 1;
      } while (uVar8 < 0x1f);
    }
    if (bVar6 != 0) {
      bVar2 = DAT_20001058 == '\x01';
      iVar12 = ((int)(bVar6 - 1) / 3) * 0xfff1 + 0xcc;
      uVar8 = (uint)bVar6 % 3;
      uVar23 = 0;
      sVar7 = 0xc;
      do {
        uVar24 = uVar17 + 1;
        if (uVar8 == (uVar23 & 0xff) && uVar8 != 0) {
          sVar7 = 0xc;
          iVar12 = iVar12 + 0xf;
        }
        if ((uVar24 & 0xff) < 0x21) {
          uVar24 = 0x20;
        }
        do {
          uVar21 = uVar17;
          if ((_DAT_20000144 >> (uVar17 & 0xff) & 1) != 0) {
LAB_08014a8a:
            uVar17 = uVar21 + 1;
            if (0xf < (uVar21 & 0xff)) goto LAB_08014a96;
            piVar9 = (int *)(&DAT_20000208 + (uVar21 & 0xff) * 4);
            goto LAB_08014ab2;
          }
          uVar21 = uVar17 + 1;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar21 = uVar17 + 2;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar21 = uVar17 + 3;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar17 = uVar17 + 4;
        } while ((uVar17 & 0xff) < 0x20);
        uVar17 = uVar24 + 1;
        uVar21 = uVar24;
LAB_08014a96:
        uVar21 = uVar21 - 0x10;
        piVar9 = (int *)(&DAT_20000238 + (uVar21 & 0xff) * 4);
LAB_08014ab2:
        iVar19 = *piVar9;
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        fVar29 = (float)VectorSignedToFloat(iVar19,(byte)(in_fpscr >> 0x15) & 3);
        if ((uVar21 & 0xfe) == 2) {
          fVar29 = fVar29 / fVar11;
        }
        if ((uVar21 & 0xff) - 6 < 6) {
          fVar29 = fVar29 / fVar3;
        }
        fVar31 = ABS(fVar29);
        uVar24 = in_fpscr & 0xfffffff | (uint)(fVar31 < fVar3) << 0x1f;
        uVar28 = uVar24 | (uint)(NAN(fVar31) || NAN(fVar3)) << 0x1c;
        if ((byte)(uVar24 >> 0x1f) == ((byte)(uVar28 >> 0x1c) & 1)) {
          fVar31 = fVar31 / fVar3;
          fVar29 = fVar29 / fVar3;
        }
        uVar20 = *(undefined4 *)
                  (&DAT_0804bf28 + (uVar21 & 0xff) * 4 + ((uint)bVar2 | (uint)bVar2 << 1) * 0x10);
        FUN_0803ed70(fVar29);
        uVar24 = uVar28 & 0xfffffff | (uint)(fVar31 < fVar11) << 0x1f;
        in_fpscr = uVar24 | (uint)(NAN(fVar31) || NAN(fVar11)) << 0x1c;
        pcVar14 = s__s__1f_s_08015a70;
        if ((byte)(uVar24 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1)) {
          pcVar14 = s__s__2f_s_08015a64;
        }
        FUN_080003b4(&DAT_20008360,pcVar14,uVar20);
        FUN_08008154((int)sVar7,(int)(short)iVar12,100,0xf);
        uVar23 = uVar23 + 1;
        if ((uint)bVar6 <= (uVar23 & 0xff)) goto LAB_08014c4c;
        sVar7 = sVar7 + 100;
        if (0x137 < sVar7) {
          iVar12 = iVar12 + 0xf;
          sVar7 = 0xc;
        }
      } while( true );
    }
LAB_08014c52:
    if ((DAT_20000332 & 1) != 0) {
      FUN_08018ea8((int)(short)(_DAT_2000032a + 0x9f),0x14,(int)(short)(_DAT_2000032a + 0x9f),0xdc);
      FUN_08018ea8((int)(short)(_DAT_2000032c + 0x9f),0x14,(int)(short)(_DAT_2000032c + 0x9f),0xdc);
    }
    uVar8 = (uint)DAT_20000332;
    if ((int)(uVar8 << 0x1e) < 0) {
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_2000032e),0x135,(int)(short)(0x78 - _DAT_2000032e));
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_20000330),0x135,(int)(short)(0x78 - _DAT_20000330));
      uVar8 = (uint)DAT_20000332;
    }
    if (uVar8 != 0) {
      sVar7 = 0x19;
      bVar6 = 0;
      if ((uVar8 & 1) != 0) goto LAB_08014d06;
      do {
        if (bVar6 == 0) {
          bVar6 = 3;
        }
        else if (bVar6 == 6) break;
LAB_08014d06:
        do {
          bVar5 = bVar6;
          if (bVar6 == 3) {
            bVar5 = 6;
          }
          if ((int)(uVar8 << 0x1e) < 0) {
            bVar5 = bVar6;
          }
          FUN_08008154(0xe,(int)sVar7,0x32,0x10);
          uVar37 = FUN_0803ed70(*(undefined4 *)(&DAT_20000334 + (uint)bVar5 * 4));
          FUN_080003b4(&DAT_20008360,0x80bcc11,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
          FUN_08008154(0x40,(int)sVar7,100,0x10);
          if (5 < bVar5) goto LAB_08014dbe;
          uVar8 = (uint)DAT_20000332;
          bVar6 = bVar5 + 1;
          sVar7 = sVar7 + 0xc;
        } while ((DAT_20000332 & 1) != 0);
      } while( true );
    }
  }
LAB_08014dbe:
  fVar3 = DAT_08014fd0;
  if (((DAT_2000012d != '\0') && (_DAT_20000130 != 0)) &&
     ((DAT_2000012c == '\0' && (uVar8 = (uint)_DAT_20000eae, 1 < uVar8)))) {
    if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
      uVar17 = (uint)(DAT_20000eb7 >> 4);
      uVar24 = DAT_20000eb7 & 0xf;
      uVar23 = 0;
      do {
        uVar21 = uVar23;
        if (((uVar24 <= uVar17) || (uVar21 = uVar17, uVar24 <= uVar17 + 1)) ||
           (uVar28 = uVar17 + 2, uVar21 = uVar17 + 1, uVar24 <= uVar28)) break;
        uVar23 = uVar17 + 3;
        uVar17 = uVar17 + 4;
        uVar21 = uVar28;
      } while (uVar23 < uVar24);
      uVar23 = 0;
      while( true ) {
        uVar24 = _DAT_20000f04;
        uVar17 = _DAT_20000f00;
        uVar28 = uVar23 & 0xffff;
        uVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                              _DAT_20000f04 * uVar8 +
                              (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),1000,0);
        uVar25 = (uint)DAT_20000efc;
        iVar12 = *(int *)(uVar21 * 4 + 0x20000ef4);
        uVar8 = uVar25;
        if (iVar12 == 0) {
          uVar8 = 1;
        }
        lVar35 = (ulonglong)uVar8 * (uVar36 & 0xffffffff);
        uVar10 = (uint)lVar35;
        uVar15 = uVar8 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
        uVar18 = 0x12d - _DAT_20000ed8;
        uVar8 = (int)uVar18 >> 0x1f;
        if (uVar8 < uVar15 || uVar15 - uVar8 < (uint)(uVar18 <= uVar10)) {
          uVar10 = uVar18;
          uVar15 = uVar8;
        }
        if (-(uVar15 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= uVar28)) break;
        fVar31 = (float)VectorUnsignedToFloat(uVar28,(byte)(in_fpscr >> 0x15) & 3);
        fVar11 = (float)FUN_0803ee1a(uVar17,uVar24);
        fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
        if (iVar12 == 0) {
          fVar29 = 1.0;
        }
        fVar32 = (float)VectorUnsignedToFloat((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
        fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
        FUN_08018ea8((int)(fVar32 + 9.0),0x78 - *(char *)(_DAT_20000130 + uVar28),
                     (int)(fVar32 + 10.0),0x78 - *(char *)(_DAT_20000130 + (uVar23 & 0xffff) + 1));
        uVar8 = (uint)_DAT_20000eae;
        uVar23 = uVar23 + 1;
      }
    }
    else {
      uVar17 = (uint)_DAT_20000eac;
      uVar23 = uVar17;
      if ((int)uVar17 < (int)(uVar8 + uVar17 + -1)) {
        do {
          FUN_08018ea8((int)(short)((short)uVar23 + 9),0x78 - *(char *)(_DAT_20000130 + uVar17),
                       (int)(short)((short)uVar23 + 10),0x78 - *(char *)(uVar17 + _DAT_20000130 + 1)
                      );
          uVar23 = uVar23 + 1;
          uVar17 = uVar23 & 0xffff;
        } while ((int)uVar17 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
      }
    }
  }
  if ((_DAT_20000122 != 0) && (DAT_2000012c == '\0')) {
    uVar16 = 0;
    uVar8 = _DAT_20000114 + 100 & 0xffff;
    iVar12 = 0xdc - uVar8;
    if (uVar8 < 0xc9) {
      uVar8 = 200;
    }
    else {
      iVar12 = 0x14;
    }
    iVar26 = (int)(short)iVar12;
    iVar19 = (int)(short)(((short)uVar8 - _DAT_20000114) + -0x50);
    uVar8 = 0xc;
    while( true ) {
      if (((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((iVar19 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar8) * 2
         + -6) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -4) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -2) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if ((((uVar16 < 3) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2)
             = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar8 = (byte)(&DAT_200000fa)[DAT_2000010e] / 3;
    uVar33 = VectorSignedToFloat((int)(short)_DAT_20000114 -
                                 (int)(char)(&DAT_200000fc)[DAT_2000010e],
                                 (byte)(in_fpscr >> 0x15) & 3);
    uVar37 = FUN_0803e5da(*(undefined2 *)
                           (&DAT_0804bfb8 +
                           (uVar8 * -3 + (uint)(byte)(&DAT_200000fa)[DAT_2000010e] & 0xff) * 2));
    uVar20 = FUN_0803e5da(uVar8);
    uVar30 = (undefined4)DAT_08015328;
    uVar20 = FUN_0803c8e0(uVar30,param_2,uVar20);
    uVar37 = FUN_0803e77c(uVar20,extraout_s1,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    uVar20 = FUN_0803e5da(*(undefined1 *)(DAT_2000010e + 0x200000fe));
    uVar20 = FUN_0803c8e0(uVar30,extraout_s1,uVar20);
    uVar37 = FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar20,extraout_s1_00);
    uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)DAT_08015330,
                          (int)((ulonglong)DAT_08015330 >> 0x20));
    uVar38 = FUN_0803ed70(uVar33);
    FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                 (int)((ulonglong)uVar38 >> 0x20));
    fVar11 = (float)FUN_0803df48();
    pcVar14 = (char *)0x8015aa8;
    fVar3 = fVar11 / DAT_08014fd0;
    if (ABS(fVar11) < DAT_08014fd0) {
      pcVar14 = s___2fmV_08015aa0;
      fVar3 = fVar11;
    }
    uVar37 = FUN_0803ed70(fVar3);
    FUN_080003b4(&DAT_20008360,pcVar14,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    sVar7 = (short)iVar12 + -10;
    if (0xd6 < iVar12) {
      sVar7 = 0xca;
    }
    if (iVar12 < 0x1e) {
      sVar7 = 0x14;
    }
    FUN_08008154(0xa0,(int)sVar7,0x7d,0x14);
  }
  if ((_DAT_20000120 != 0) && (DAT_2000012c == '\0')) {
    iVar12 = (int)(short)(_DAT_20000112 + 0x9f);
    uVar16 = 0;
    uVar8 = 0x1f;
    while( true ) {
      if ((((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
          (iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 3 &&
          (uVar8 - 3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -3) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (uVar8 == 0xdf) break;
      if (((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
         ((iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -2) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if ((((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
          (iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((~(uint)_DAT_20008352 + uVar8) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
         ((iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((uVar8 - _DAT_20008352) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2) =
             0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar37 = FUN_0803e538(_DAT_20000118,_DAT_2000011c);
    uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)DAT_08015338,
                          (int)((ulonglong)DAT_08015338 >> 0x20));
    uVar36 = (ulonglong)DAT_08015340 >> 0x20;
    uVar20 = (undefined4)((ulonglong)DAT_08015330 >> 0x20);
    uVar30 = (undefined4)DAT_08015330;
    while( true ) {
      uStack00000034 = (uint)((ulonglong)uVar37 >> 0x20);
      uVar33 = (undefined4)uVar37;
      iVar12 = FUN_0803ee0c(uVar33,uStack00000034 & 0x7fffffff | (uint)uVar36 & 0x80000000,uVar30,
                            uVar20);
      if (iVar12 != 0) break;
      uVar37 = FUN_0803e124(uVar33,uStack00000034,uVar30,uVar20);
    }
    uStack00000034 = uStack00000034 ^ 0x80000000;
    FUN_080003b4(&DAT_20008360,0x80bcc11,uVar33,uStack00000034);
    FUN_08008154(0,0xb4,0x140,0x14);
  }
  if ((DAT_2000012c != '\0') && (FUN_08021b40(), DAT_2000012c == '\0')) {
    sVar7 = 0x78 - _DAT_20000114;
    if (200 < (ushort)(_DAT_20000114 + 100)) {
      sVar7 = 0x14;
    }
    FUN_08019af8(0x136,(int)sVar7,0x13d,(int)(short)(sVar7 + 5));
  }
  if (((((DAT_20000f08 != -1) && (DAT_20000f08 != '\0')) ||
       ((DAT_20000127 != '\0' || DAT_20000128 != '\0') || DAT_2000012b != '\0')) ||
      ((uVar8 = (uint)DAT_20000126, DAT_2000010f == '\x02' && (uVar8 != 0)))) ||
     ((uVar8 == 0 &&
      (uVar8 = -(uint)(_DAT_20000f00 >= 10) - _DAT_20000f04,
      -_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))))) {
    FUN_08019af8(0xea,0x51,0x45,0x51);
    FUN_08019af8(0x45,0x51,0xef,0x55);
    FUN_08019af8(0xef,0x55,0x3e,0x59);
    FUN_08019af8(0x3e,0x59,0x101,0x55);
    FUN_08019af8(0x101,0x55,0x3e,0xa1);
    FUN_08019af8(0x3e,0xa1,0x101,0x98);
    FUN_08018fcc(0x3d,0x50,0xc6,0x53);
    uVar8 = extraout_r1;
  }
  if ((DAT_20000f08 == '\x03') || (DAT_20000f08 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (DAT_20000f08 == '\x01') {
    DAT_20000f13 = FUN_08034878(0x80bc18b,uVar8);
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,DAT_20000f09);
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    DAT_20000f08 = -1;
    DAT_20001060 = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
  if ((DAT_2000010f == '\x02') && (DAT_20000126 != 0)) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (DAT_20000127 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_20000128 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((DAT_20000126 == 0) && (-_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))) {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_2000012b != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  DAT_20001068 = DAT_20001068 + '\x01';
  return;
}



// Attempting to create function at 0x08011B74
// Successfully created function
// ==========================================
// Function: FUN_08011b74 @ 08011b74
// Size: 1 bytes
// ==========================================

/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08015214 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

void FUN_08011b74(undefined4 param_1,undefined4 param_2,uint param_3,undefined2 param_4,
                 ushort param_5,int param_6)

{
  ushort *puVar1;
  ushort uVar2;
  bool bVar3;
  float fVar4;
  char cVar5;
  byte bVar6;
  byte bVar7;
  short sVar8;
  uint uVar9;
  int *piVar10;
  uint uVar11;
  float fVar12;
  int iVar13;
  byte *pbVar14;
  char *pcVar15;
  uint uVar16;
  uint extraout_r1;
  ushort uVar17;
  uint uVar18;
  uint uVar19;
  int iVar20;
  undefined4 uVar21;
  uint uVar22;
  uint unaff_r5;
  ushort *puVar23;
  int iVar24;
  uint uVar25;
  int unaff_r7;
  uint uVar26;
  int unaff_r8;
  uint uVar27;
  int iVar28;
  byte *pbVar29;
  uint in_fpscr;
  uint uVar30;
  float fVar31;
  undefined4 uVar32;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  float fVar33;
  float fVar34;
  undefined4 uVar35;
  undefined8 uVar36;
  longlong lVar37;
  ulonglong uVar38;
  undefined8 uVar39;
  undefined8 uVar40;
  uint uStack00000034;
  undefined4 in_stack_0000003c;
  undefined8 in_stack_00000040;
  undefined8 in_stack_00000048;
  undefined8 in_stack_00000050;
  undefined8 in_stack_00000058;
  undefined8 in_stack_00000060;
  undefined4 in_stack_0000006c;
  undefined4 in_stack_00000070;
  undefined4 in_stack_00000074;
  undefined4 in_stack_00000078;
  undefined4 in_stack_0000007c;
  undefined4 in_stack_00000080;
  undefined4 in_stack_00000084;
  undefined4 in_stack_00000088;
  undefined4 in_stack_0000008c;
  
code_r0x08011b74:
  puVar23 = (ushort *)(unaff_r5 & 0xffff | 0x20000000);
  *(undefined2 *)(param_6 + (unaff_r7 + param_3) * 2 + -2) = param_4;
code_r0x08011b84:
  uVar9 = param_3;
  param_5 = param_5 + 1;
  if (4 < param_5) {
    param_5 = 0;
  }
  if ((((param_5 == 0) && (*puVar23 <= uVar9)) && (uVar9 < (uint)*puVar23 + (uint)puVar23[2])) &&
     ((puVar23[1] < 0x79 && (0x78 < (uint)puVar23[1] + (uint)puVar23[3])))) {
    puVar1 = puVar23 + 4;
    uVar17 = *puVar23;
    puVar23 = (ushort *)&DAT_20008350;
    *(undefined2 *)
     (*(int *)puVar1 +
     (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)uVar17) + uVar9) * 2) = param_4;
  }
  param_5 = param_5 + 1;
  if (4 < param_5) {
    param_5 = 0;
  }
  param_3 = uVar9 + 4;
  if (((param_5 == 0) && ((uint)*puVar23 <= uVar9 + 1)) &&
     ((uVar17 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
      uVar9 + 1 < (uint)uVar17 + (uint)_DAT_20008354 &&
      ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
    puVar23 = (ushort *)&DAT_20008350;
    *(undefined2 *)
     (_DAT_20008358 +
      (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + param_3) * 2 +
     -6) = param_4;
  }
  param_5 = param_5 + 1;
  if (4 < param_5) {
    param_5 = 0;
  }
  if (param_3 != 0x138) {
    if ((((param_5 == 0) && ((uint)*puVar23 <= uVar9 + 2)) &&
        (uVar17 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 + 2 < (uint)uVar17 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + param_3) * 2 +
       -4) = param_4;
    }
    param_5 = param_5 + 1;
    if (4 < param_5) {
      param_5 = 0;
    }
    if (((param_5 != 0) || (uVar9 + 3 < (uint)*puVar23)) ||
       ((uVar17 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        (uint)uVar17 + (uint)_DAT_20008354 <= uVar9 + 3 ||
        ((0x78 < _DAT_20008352 || ((uint)_DAT_20008352 + (uint)_DAT_20008356 < 0x79))))))
    goto code_r0x08011b84;
    unaff_r7 = (uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350;
    unaff_r5 = 0x8350;
    param_6 = _DAT_20008358;
    goto code_r0x08011b74;
  }
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 <= uVar9)) && (uVar9 < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] < 0x92 && (0x91 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 <= uVar9)) &&
       ((uVar9 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] < 0xab && (0xaa < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 <= uVar9)) && (uVar9 < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] < 0xc4 && (0xc3 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0x23)) &&
       ((0x22 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x44) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x23)) && (0x22 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x44) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x23)) &&
       ((0x22 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x44) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 < 0x3c)) && (0x3b < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 2 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x76) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x3c)) &&
       ((0x3b < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x76) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x3c)) && (0x3b < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x76) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0x55)) &&
       ((0x54 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xa8) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x55)) && (0x54 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0xa8) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x55)) &&
       ((0x54 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0xa8) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 < 0x6e)) && (0x6d < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 2 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xda) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x6e)) &&
       ((0x6d < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0xda) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x6e)) && (0x6d < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0xda) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0x87)) &&
       ((0x86 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x10c) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x87)) && (0x86 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x10c) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x87)) &&
       ((0x86 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x10c)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 < 0xa0)) && (0x9f < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 2 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13e) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0xa0)) &&
       ((0x9f < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x13e) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0xa0)) && (0x9f < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x13e)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0xb9)) &&
       ((0xb8 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0xb9)) && (0xb8 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0xb9)) &&
       ((0xb8 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x170)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 < 0xd2)) && (0xd1 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 2 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0xd2)) &&
       ((0xd1 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0xd2)) && (0xd1 < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x1a2)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0xeb)) &&
       ((0xea < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0xeb)) && (0xea < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0xeb)) &&
       ((0xea < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x1d4)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 >> 2 < 0x41)) && (0x103 < (uint)*puVar23 + (uint)puVar23[2]))
       && (((uint)puVar23[1] <= uVar9 - 2 &&
           (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
           uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x206) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 >> 2 < 0x41)) &&
       ((0x103 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x206) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 >> 2 < 0x41)) && (0x103 < (uint)*puVar23 + (uint)puVar23[2]))
       && ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x206)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0x11d)) &&
       ((0x11c < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x238) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x11d)) && (0x11c < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x238) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x11d)) &&
       ((0x11c < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x238)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 <= uVar9)) &&
       ((uVar9 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] < 0x77 && (0x76 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 <= uVar9)) && (uVar9 < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] < 0x78 && (0x77 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 <= uVar9)) &&
       ((uVar9 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] < 0x7a && (0x79 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0xc;
  while( true ) {
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 3)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 3 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -6) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (uVar9 == 0x138) break;
    if ((((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 2)) &&
        (uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)uVar2 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -4) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && ((uint)*puVar23 <= uVar9 - 1)) &&
       ((uVar2 = *puVar23, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)uVar2 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2 +
       -2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 <= uVar9)) && (uVar9 < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] < 0x7b && (0x7a < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 +
       (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)uVar2) + uVar9) * 2) = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    uVar9 = uVar9 + 4;
  }
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 < 0x9e)) && (0x9d < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 2 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x9e)) &&
       ((0x9d < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x9e)) && (0x9d < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x13a)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0x9f)) &&
       ((0x9e < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0x9f)) && (0x9e < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0x9f)) &&
       ((0x9e < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x13c)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if ((((uVar17 == 0) && (*puVar23 < 0xa1)) && (0xa0 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 2 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0xa1)) &&
       ((0xa0 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 1 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0xa1)) && (0xa0 < (uint)*puVar23 + (uint)puVar23[2])) &&
       ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x140)
           = 0x3a29;
    }
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar17 = 0;
  uVar9 = 0x16;
  do {
    if (((uVar17 == 0) && (*puVar23 < 0xa2)) &&
       ((0xa1 < (uint)*puVar23 + (uint)puVar23[2] &&
        (((uint)puVar23[1] <= uVar9 - 2 &&
         (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
         uVar9 - 2 < (uint)*puVar1 + (uint)_DAT_20008356)))))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar9 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if ((((uVar17 == 0) && (*puVar23 < 0xa2)) && (0xa1 < (uint)*puVar23 + (uint)puVar23[2])) &&
       (((uint)puVar23[1] <= uVar9 - 1 &&
        (puVar1 = puVar23 + 1, puVar23 = (ushort *)&DAT_20008350,
        uVar9 - 1 < (uint)*puVar1 + (uint)_DAT_20008356)))) {
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar9) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar17 = uVar17 + 1;
    if (4 < uVar17) {
      uVar17 = 0;
    }
    if (((uVar17 == 0) && (*puVar23 < 0xa2)) &&
       ((0xa1 < (uint)*puVar23 + (uint)puVar23[2] &&
        ((puVar23[1] <= uVar9 && (uVar9 < (uint)puVar23[1] + (uint)puVar23[3])))))) {
      puVar1 = puVar23 + 4;
      uVar2 = *puVar23;
      puVar23 = (ushort *)&DAT_20008350;
      *(undefined2 *)
       (*(int *)puVar1 + ((uint)_DAT_20008354 * (uVar9 - _DAT_20008352) - (uint)uVar2) * 2 + 0x142)
           = 0x3a29;
    }
    pbVar14 = _DAT_20000138;
    bVar7 = DAT_20000136;
    uVar17 = uVar17 + 1;
    uVar9 = uVar9 + 3;
    if (4 < uVar17) {
      uVar17 = 0;
    }
  } while (uVar9 != 0xdf);
  uVar9 = (uint)*(byte *)(unaff_r8 + 0x3c);
  if ((uVar9 != 0) && (DAT_20000126 != 0)) {
    if (uVar9 == 3) {
      if (_DAT_20000138 != (byte *)0x0) {
        if (DAT_2000012c == '\0') {
          uVar9 = (uint)DAT_2000010d;
          if ((uint)(DAT_2000010d >> 4) < (uVar9 & 0xf)) {
            uVar18 = (uint)(DAT_2000010d >> 4);
            uVar25 = (uint)DAT_2000105b;
            do {
              if ((*(int *)(pbVar14 + uVar18 * 4 + 4) != 0) && (*(short *)(pbVar14 + 0xe) != 0)) {
                uVar26 = (uint)*pbVar14;
                uVar2 = *(ushort *)(uVar25 * 4 + 0x80bb40c + uVar18 * 2);
                uVar17 = *(ushort *)(pbVar14 + 0xc);
                iVar13 = *(int *)(pbVar14 + uVar18 * 4 + 4);
                uVar2 = (ushort)(uint)((ulonglong)(uVar26 * (((uint)uVar2 << 0x15) >> 0x1a)) *
                                       0x80808081 >> 0x22) & 0xffe0 |
                        (ushort)((uVar26 * (uVar2 & 0x1f)) / 0xff) |
                        (ushort)(((uint)((ulonglong)(uVar26 * (uVar2 >> 0xb)) * 0x80808081 >> 0x20)
                                 & 0xfffff80) << 4);
                do {
                  iVar28 = (short)uVar17 * 0x1a;
                  iVar20 = (int)(short)(uVar17 + 9);
                  iVar24 = 0xc6;
                  uVar26 = 2;
                  while( true ) {
                    if (((((*(byte *)(iVar13 + iVar28 + ((uVar26 - 2) * 0x10000 >> 0x13)) >>
                            (uVar26 - 2 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar20)) &&
                        (iVar20 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar24 + 0x16U &&
                        (iVar24 + 0x16U < (uint)_DAT_20008356 + (uint)_DAT_20008352)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar24 - (uint)_DAT_20008352) + 0x16) * (uint)_DAT_20008354 +
                       (iVar20 - (uint)_DAT_20008350)) * 2) = uVar2;
                    }
                    if ((((*(byte *)(iVar13 + ((uVar26 - 1) * 0x10000 >> 0x13) + iVar28) >>
                           (uVar26 - 1 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar20)) &&
                       ((iVar20 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
                        (((uint)_DAT_20008352 <= iVar24 + 0x15U &&
                         (iVar24 + 0x15U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar24 - (uint)_DAT_20008352) + 0x15) * (uint)_DAT_20008354 +
                       (iVar20 - (uint)_DAT_20008350)) * 2) = uVar2;
                    }
                    if (((((*(byte *)(iVar13 + ((uVar26 << 0x10) >> 0x13) + iVar28) >> (uVar26 & 7)
                           & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar20)) &&
                        (iVar20 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar24 + 0x14U &&
                        (iVar24 + 0x14U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar24 - (uint)_DAT_20008352) + 0x14) * (uint)_DAT_20008354 +
                       (iVar20 - (uint)_DAT_20008350)) * 2) = uVar2;
                    }
                    if (iVar24 == 0) break;
                    iVar24 = iVar24 + -3;
                    uVar26 = uVar26 + 3;
                  }
                  uVar17 = uVar17 + 1;
                } while ((uint)uVar17 <
                         (uint)*(ushort *)(pbVar14 + 0xc) + (uint)*(ushort *)(pbVar14 + 0xe));
              }
              uVar18 = uVar18 + 1;
            } while (uVar18 != (uVar9 & 0xf));
          }
        }
        else {
          iVar13 = *(int *)(_DAT_20000138 + 4);
          if (iVar13 != 0) {
            uVar9 = ((uint)*_DAT_20000138 * 0x1f) / 0xff;
            uVar17 = (ushort)uVar9 | (ushort)(uVar9 << 0xb);
            iVar28 = 0;
            do {
              iVar24 = iVar28 * 0x1a;
              uVar9 = iVar28 * 0x10000 + 0x3b0000 >> 0x10;
              iVar20 = 0xc9;
              uVar25 = 2;
              do {
                if ((((*(byte *)(iVar13 + ((uVar25 - 2) * 0x1000000 >> 0x1b) + iVar24) >>
                       (uVar25 - 2 & 7) & 1) != 0) && (_DAT_20008350 <= uVar9)) &&
                   ((uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar20 + 0x13U &&
                     (iVar20 + 0x13U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar20 - (uint)_DAT_20008352) + 0x13) * (uint)_DAT_20008354 +
                   (uVar9 - _DAT_20008350)) * 2) = uVar17;
                }
                if (((((*(byte *)(iVar13 + ((uVar25 - 1) * 0x1000000 >> 0x1b) + iVar24) >>
                        (uVar25 - 1 & 7) & 1) != 0) && (_DAT_20008350 <= uVar9)) &&
                    (uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
                   (((uint)_DAT_20008352 <= iVar20 + 0x12U &&
                    (iVar20 + 0x12U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar20 - (uint)_DAT_20008352) + 0x12) * (uint)_DAT_20008354 +
                   (uVar9 - _DAT_20008350)) * 2) = uVar17;
                }
                if ((((*(byte *)(iVar13 + ((uVar25 << 0x18) >> 0x1b) + iVar24) >> (uVar25 & 7) & 1)
                      != 0) && (_DAT_20008350 <= uVar9)) &&
                   ((uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar20 + 0x11U &&
                     (iVar20 + 0x11U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar20 - (uint)_DAT_20008352) + 0x11) * (uint)_DAT_20008354 +
                   (uVar9 - _DAT_20008350)) * 2) = uVar17;
                }
                iVar20 = iVar20 + -3;
                uVar25 = uVar25 + 3;
              } while (iVar20 != 0);
              iVar28 = iVar28 + 1;
            } while (iVar28 != 0xc9);
          }
        }
      }
    }
    else {
      DAT_20000136 = DAT_20000135;
      bVar6 = DAT_20000135;
      if (DAT_20000135 <= bVar7) {
        bVar6 = DAT_20000135 + 100;
      }
      DAT_20000137 = bVar6 - bVar7;
      if (_DAT_20000138 != (byte *)0x0) {
        pbVar14 = &DAT_20000138;
        uVar38 = (ulonglong)DAT_08014350 >> 0x20;
        uVar21 = (undefined4)DAT_08014350;
        pbVar29 = _DAT_20000138;
        while( true ) {
          cVar5 = DAT_20000137;
          uVar39 = FUN_0803e5da(*pbVar29);
          uVar32 = (undefined4)((ulonglong)uVar39 >> 0x20);
          uVar40 = FUN_0803e5da(cVar5);
          uVar40 = FUN_0803e77c((int)uVar40,(int)((ulonglong)uVar40 >> 0x20),uVar21,(int)uVar38);
          uVar36 = FUN_0803e50a(3 - uVar9);
          uVar40 = FUN_0803e77c((int)uVar40,(int)((ulonglong)uVar40 >> 0x20),(int)uVar36,
                                (int)((ulonglong)uVar36 >> 0x20));
          uVar35 = (undefined4)((ulonglong)uVar40 >> 0x20);
          iVar13 = FUN_0803edf0((int)uVar40,uVar35,(int)uVar39,uVar32);
          if (iVar13 != 0) break;
          FUN_0803eb94((int)uVar39,uVar32,(int)uVar40,uVar35);
          bVar7 = FUN_0803e450();
          cVar5 = DAT_2000012c;
          *pbVar29 = bVar7;
          if (cVar5 == '\0') {
            uVar9 = (uint)DAT_2000010d;
            if ((uint)(DAT_2000010d >> 4) < (uVar9 & 0xf)) {
              uVar25 = (uint)(DAT_2000010d >> 4);
              do {
                if (((*(int *)(_DAT_20000138 + uVar25 * 4 + 4) != 0) &&
                    (1 < *(ushort *)(pbVar29 + 0xe))) && (1 < *(ushort *)(pbVar29 + 0xe))) {
                  uVar9 = 0;
                  uVar17 = 0;
                  do {
                    FUN_08018ea8((int)(short)(*(short *)(pbVar29 + 0xc) + uVar17 + 9),
                                 0xf8 - (uint)*(byte *)(*(int *)(pbVar29 + uVar25 * 4 + 4) + uVar9),
                                 (int)(short)(*(short *)(pbVar29 + 0xc) + uVar17 + 10),
                                 0xf8 - (uint)*(byte *)(uVar9 + *(int *)(pbVar29 + uVar25 * 4 + 4) +
                                                       1));
                    pbVar29 = *(byte **)pbVar14;
                    uVar17 = uVar17 + 1;
                    uVar9 = (uint)uVar17;
                  } while ((int)uVar9 < (int)(*(ushort *)(pbVar29 + 0xe) - 1));
                  uVar9 = (uint)DAT_2000010d;
                }
                uVar25 = uVar25 + 1;
              } while (uVar25 < (uVar9 & 0xf));
            }
          }
          else if (((*(int *)(_DAT_20000138 + 4) != 0) && (*(int *)(_DAT_20000138 + 8) != 0)) &&
                  ((1 < *(ushort *)(pbVar29 + 0xe) &&
                   (uVar25 = (uint)*(ushort *)(pbVar29 + 0xc), uVar9 = uVar25,
                   (int)uVar25 < (int)(*(ushort *)(pbVar29 + 0xe) + uVar25 + -1))))) {
            do {
              FUN_08018da0(*(byte *)(*(int *)(pbVar29 + 4) + uVar25) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar29 + 8) + uVar25),
                           *(byte *)(*(int *)(pbVar29 + 4) + uVar25 + 1) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar29 + 8) + uVar25 + 1));
              pbVar29 = *(byte **)pbVar14;
              uVar25 = uVar9 + 1 & 0xffff;
              uVar9 = uVar9 + 1;
            } while ((int)uVar25 <
                     (int)((uint)*(ushort *)(pbVar29 + 0xc) + (uint)*(ushort *)(pbVar29 + 0xe) + -1)
                    );
          }
          pbVar14 = pbVar29 + 0x10;
          pbVar29 = *(byte **)pbVar14;
          if (pbVar29 == (byte *)0x0) goto LAB_080144ca;
LAB_08014000:
          uVar9 = (uint)DAT_20000134;
        }
        if (*(int *)(pbVar29 + 4) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar9 = *(int *)(pbVar29 + 4) - _DAT_20001078;
            if (uVar9 >> 10 < 0xaf) {
              uVar9 = uVar9 >> 5;
              uVar25 = (uint)*(ushort *)(_DAT_2000107c + uVar9 * 2);
              if (uVar25 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar9 * 2,uVar25 << 1);
              }
            }
          }
        }
        if (*(int *)(*(int *)pbVar14 + 8) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar9 = *(int *)(*(int *)pbVar14 + 8) - _DAT_20001078;
            if (uVar9 >> 10 < 0xaf) {
              uVar9 = uVar9 >> 5;
              uVar25 = (uint)*(ushort *)(_DAT_2000107c + uVar9 * 2);
              if (uVar25 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar9 * 2,uVar25 << 1);
              }
            }
          }
        }
        pbVar29 = *(byte **)(*(int *)pbVar14 + 0x10);
        if (_DAT_20000138 != (byte *)0x0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else if ((uint)((int)_DAT_20000138 - _DAT_20001078) >> 10 < 0xaf) {
            uVar9 = (uint)((int)_DAT_20000138 - _DAT_20001078) >> 5;
            uVar25 = (uint)*(ushort *)(_DAT_2000107c + uVar9 * 2);
            if (uVar25 != 0) {
              FUN_080012bc(_DAT_2000107c + uVar9 * 2,uVar25 << 1);
            }
          }
        }
        pbVar14 = &DAT_20000138;
        _DAT_20000138 = pbVar29;
        if (pbVar29 != (byte *)0x0) goto LAB_08014000;
      }
    }
  }
LAB_080144ca:
  fVar4 = DAT_08014948;
  uVar9 = (uint)_DAT_20000eae;
  if (uVar9 == 0) {
LAB_08014800:
    if (DAT_2000012c == '\0') {
      pbVar14 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar14 = &DAT_20000eb7;
      }
      bVar7 = *pbVar14;
      if ((uint)(bVar7 >> 4) < (bVar7 & 0xf)) {
        uVar9 = (uint)(bVar7 >> 4);
        sVar8 = (ushort)(bVar7 >> 4) * 0xad + 9;
        do {
          if (*(char *)(uVar9 + 0x20000102) != '\0') {
            piVar10 = (int *)(uVar9 * 4 + 0x20000104);
            iVar13 = *piVar10;
            if (iVar13 != 0) {
              for (iVar28 = 0; iVar20 = (int)(short)(sVar8 + (short)iVar28),
                  FUN_08018ea8(iVar20,0x46,iVar20,0x46 - (uint)*(byte *)(iVar13 + iVar28)),
                  iVar28 != 0x7f; iVar28 = iVar28 + 1) {
                iVar13 = *piVar10;
              }
            }
          }
          uVar9 = uVar9 + 1;
          sVar8 = sVar8 + 0xad;
        } while (uVar9 != (bVar7 & 0xf));
        if (DAT_2000012c != '\0') goto LAB_080148ec;
      }
      if (_DAT_20000112 < 0x92) {
        if (_DAT_20000112 < -0x91) {
          iVar13 = 0x10;
          iVar28 = 0x10;
          uVar21 = 0x1e;
        }
        else {
          iVar13 = (int)(short)(_DAT_20000112 + 0x9a);
          iVar28 = (int)(short)(_DAT_20000112 + 0xa4);
          uVar21 = 0x14;
        }
      }
      else {
        iVar13 = 0x12e;
        iVar28 = 0x12e;
        uVar21 = 0x1e;
      }
      FUN_08019af8(iVar13,0x14,iVar28,uVar21);
    }
  }
  else {
    if (DAT_2000012c == '\0') {
      pbVar14 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar14 = &DAT_20000eb7;
      }
      bVar7 = *pbVar14;
      if ((uint)(bVar7 >> 4) < (bVar7 & 0xf)) {
        uVar25 = (uint)(bVar7 >> 4);
        do {
          if (1 < uVar9) {
            if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))
               ) {
              uVar18 = 0;
              while( true ) {
                uVar22 = _DAT_20000f04;
                uVar26 = _DAT_20000f00;
                uVar38 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9),
                                      _DAT_20000f04 * uVar9 +
                                      (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9 >> 0x20),
                                      1000,0);
                uVar27 = (uint)DAT_20000efc;
                iVar13 = *(int *)(uVar25 * 4 + 0x20000ef4);
                uVar30 = uVar27;
                if (iVar13 == 0) {
                  uVar30 = 1;
                }
                lVar37 = (ulonglong)uVar30 * (uVar38 & 0xffffffff);
                uVar11 = (uint)lVar37;
                uVar16 = uVar30 * (int)(uVar38 >> 0x20) + (int)((ulonglong)lVar37 >> 0x20);
                uVar19 = 0x12d - _DAT_20000ed8;
                uVar30 = (int)uVar19 >> 0x1f;
                if (uVar30 < uVar16 || uVar16 - uVar30 < (uint)(uVar19 <= uVar11)) {
                  uVar11 = uVar19;
                  uVar16 = uVar30;
                }
                if (-(uVar16 - (uVar11 == 0)) < (uint)(uVar11 - 1 <= (uVar18 & 0xffff))) break;
                fVar33 = (float)VectorUnsignedToFloat(uVar18 & 0xffff,(byte)(in_fpscr >> 0x15) & 3);
                fVar12 = (float)FUN_0803ee1a(uVar26,uVar22);
                fVar31 = (float)VectorUnsignedToFloat(uVar27,(byte)(in_fpscr >> 0x15) & 3);
                if (iVar13 == 0) {
                  fVar31 = 1.0;
                }
                iVar13 = uVar25 * 0x12d + 0x200000f8 + (uVar18 & 0xffff);
                fVar34 = (float)VectorUnsignedToFloat
                                          ((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
                fVar34 = fVar33 / ((fVar12 / fVar4) * fVar31) + fVar34;
                FUN_08018ea8((int)(fVar34 + 9.0),0xf8 - (uint)*(byte *)(iVar13 + 0x356),
                             (int)(fVar34 + 10.0),0xf8 - (uint)*(byte *)(iVar13 + 0x357));
                uVar9 = (uint)_DAT_20000eae;
                uVar18 = uVar18 + 1;
              }
            }
            else {
              uVar26 = (uint)_DAT_20000eac;
              uVar18 = uVar26;
              if ((int)uVar26 < (int)(uVar26 + uVar9 + -1)) {
                do {
                  iVar13 = uVar26 + uVar25 * 0x12d + 0x200000f8;
                  FUN_08018ea8((int)(short)((short)uVar18 + 9),
                               0xf8 - (uint)*(byte *)(iVar13 + 0x356),
                               (int)(short)((short)uVar18 + 10),
                               0xf8 - (uint)*(byte *)(iVar13 + 0x357));
                  uVar9 = (uint)_DAT_20000eae;
                  uVar26 = uVar18 + 1 & 0xffff;
                  uVar18 = uVar18 + 1;
                } while ((int)uVar26 < (int)(_DAT_20000eac + uVar9 + -1));
              }
            }
          }
          uVar25 = uVar25 + 1;
        } while (uVar25 != (bVar7 & 0xf));
      }
      goto LAB_08014800;
    }
    if (1 < uVar9) {
      if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
        uVar25 = 0;
        while( true ) {
          uVar26 = uVar25 & 0xffff;
          lVar37 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9),
                                _DAT_20000f04 * uVar9 +
                                (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9 >> 0x20),1000,0);
          uVar9 = (uint)((ulonglong)lVar37 >> 0x20);
          uVar18 = 0x12d - _DAT_20000ed8;
          if ((uint)((int)uVar18 >> 0x1f) < uVar9 ||
              uVar9 - ((int)uVar18 >> 0x1f) < (uint)(uVar18 <= (uint)lVar37)) {
            lVar37 = (longlong)(int)uVar18;
          }
          if (-((int)((ulonglong)lVar37 >> 0x20) - (uint)((int)lVar37 == 0)) <
              (uint)((int)lVar37 - 1U <= uVar26)) break;
          FUN_08018da0(*(byte *)(uVar26 + 0x2000044e) + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057b)[uVar26],
                       (byte)(&DAT_2000044f)[uVar25 & 0xffff] + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057c)[uVar25 & 0xffff]);
          uVar9 = (uint)_DAT_20000eae;
          uVar25 = uVar25 + 1;
        }
      }
      else {
        uVar18 = (uint)_DAT_20000eac;
        uVar25 = uVar18;
        if ((int)uVar18 < (int)(uVar18 + uVar9 + -1)) {
          do {
            FUN_08018da0(*(byte *)(uVar18 + 0x2000044e) + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057b)[uVar18],
                         (byte)(&DAT_2000044f)[uVar18] + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057c)[uVar18]);
            uVar18 = uVar25 + 1 & 0xffff;
            uVar25 = uVar25 + 1;
          } while ((int)uVar18 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
        }
      }
      goto LAB_08014800;
    }
  }
LAB_080148ec:
  fVar12 = DAT_08014d00;
  fVar4 = DAT_08014948;
  if ((((_DAT_20000144 == 0) || (DAT_2000012c != '\0')) || (uVar9 = (uint)DAT_2000010c, uVar9 == 0))
     || (DAT_20000140 != '\0')) {
LAB_08014c4c:
    if (DAT_2000012c == '\0') goto LAB_08014c52;
  }
  else {
    uVar18 = ~(uVar9 << 4) & 0x10;
    uVar25 = 0;
    bVar7 = 0;
    if ((int)(uVar9 << 0x1e) < 0) {
      do {
        if ((uVar25 & 0xff) == 0) {
          uVar25 = uVar18;
        }
        uVar9 = uVar25 & 0xff;
        if ((_DAT_20000144 >> uVar9 & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        if (0x1e < uVar9) break;
        if ((_DAT_20000144 >> (uVar25 + 1 & 0xff) & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        if (uVar9 == 0x1e) break;
        if ((_DAT_20000144 >> (uVar25 + 2 & 0xff) & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        if (0x1c < uVar9) break;
        if ((_DAT_20000144 >> (uVar25 + 3 & 0xff) & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        uVar25 = uVar25 + 4;
      } while (uVar9 != 0x1c);
    }
    else {
      do {
        if ((uVar25 & 0xff) == 0) {
          uVar25 = uVar18;
        }
        uVar9 = uVar25 & 0xff;
        if (uVar9 == 0x10) break;
        if ((_DAT_20000144 >> uVar9 & 1) != 0) {
          bVar7 = bVar7 + 1;
        }
        uVar25 = uVar25 + 1;
      } while (uVar9 < 0x1f);
    }
    if (bVar7 != 0) {
      bVar3 = DAT_20001058 == '\x01';
      iVar13 = ((int)(bVar7 - 1) / 3) * 0xfff1 + 0xcc;
      uVar9 = (uint)bVar7 % 3;
      uVar25 = 0;
      sVar8 = 0xc;
      do {
        uVar26 = uVar18 + 1;
        if (uVar9 == (uVar25 & 0xff) && uVar9 != 0) {
          sVar8 = 0xc;
          iVar13 = iVar13 + 0xf;
        }
        if ((uVar26 & 0xff) < 0x21) {
          uVar26 = 0x20;
        }
        do {
          uVar22 = uVar18;
          if ((_DAT_20000144 >> (uVar18 & 0xff) & 1) != 0) {
LAB_08014a8a:
            uVar18 = uVar22 + 1;
            if (0xf < (uVar22 & 0xff)) goto LAB_08014a96;
            piVar10 = (int *)(&DAT_20000208 + (uVar22 & 0xff) * 4);
            goto LAB_08014ab2;
          }
          uVar22 = uVar18 + 1;
          if (0x1f < (uVar22 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar22 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar22 = uVar18 + 2;
          if (0x1f < (uVar22 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar22 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar22 = uVar18 + 3;
          if (0x1f < (uVar22 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar22 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar18 = uVar18 + 4;
        } while ((uVar18 & 0xff) < 0x20);
        uVar18 = uVar26 + 1;
        uVar22 = uVar26;
LAB_08014a96:
        uVar22 = uVar22 - 0x10;
        piVar10 = (int *)(&DAT_20000238 + (uVar22 & 0xff) * 4);
LAB_08014ab2:
        iVar20 = *piVar10;
        iVar28 = iVar20;
        if (iVar20 < 1) {
          iVar28 = SignedSaturate(-iVar20,0x20);
          SignedDoesSaturate(iVar28,0x20);
        }
        if (999999 < iVar28) {
          iVar20 = iVar20 / 1000;
        }
        iVar28 = iVar20;
        if (iVar20 < 1) {
          iVar28 = SignedSaturate(-iVar20,0x20);
          SignedDoesSaturate(iVar28,0x20);
        }
        if (999999 < iVar28) {
          iVar20 = iVar20 / 1000;
        }
        iVar28 = iVar20;
        if (iVar20 < 1) {
          iVar28 = SignedSaturate(-iVar20,0x20);
          SignedDoesSaturate(iVar28,0x20);
        }
        if (999999 < iVar28) {
          iVar20 = iVar20 / 1000;
        }
        fVar31 = (float)VectorSignedToFloat(iVar20,(byte)(in_fpscr >> 0x15) & 3);
        if ((uVar22 & 0xfe) == 2) {
          fVar31 = fVar31 / fVar12;
        }
        if ((uVar22 & 0xff) - 6 < 6) {
          fVar31 = fVar31 / fVar4;
        }
        fVar33 = ABS(fVar31);
        uVar26 = in_fpscr & 0xfffffff | (uint)(fVar33 < fVar4) << 0x1f;
        uVar30 = uVar26 | (uint)(NAN(fVar33) || NAN(fVar4)) << 0x1c;
        if ((byte)(uVar26 >> 0x1f) == ((byte)(uVar30 >> 0x1c) & 1)) {
          fVar33 = fVar33 / fVar4;
          fVar31 = fVar31 / fVar4;
        }
        uVar21 = *(undefined4 *)
                  (&DAT_0804bf28 + (uVar22 & 0xff) * 4 + ((uint)bVar3 | (uint)bVar3 << 1) * 0x10);
        FUN_0803ed70(fVar31);
        uVar26 = uVar30 & 0xfffffff | (uint)(fVar33 < fVar12) << 0x1f;
        in_fpscr = uVar26 | (uint)(NAN(fVar33) || NAN(fVar12)) << 0x1c;
        pcVar15 = s__s__1f_s_08015a70;
        if ((byte)(uVar26 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1)) {
          pcVar15 = s__s__2f_s_08015a64;
        }
        FUN_080003b4(&DAT_20008360,pcVar15,uVar21);
        FUN_08008154((int)sVar8,(int)(short)iVar13,100,0xf);
        uVar25 = uVar25 + 1;
        if ((uint)bVar7 <= (uVar25 & 0xff)) goto LAB_08014c4c;
        sVar8 = sVar8 + 100;
        if (0x137 < sVar8) {
          iVar13 = iVar13 + 0xf;
          sVar8 = 0xc;
        }
      } while( true );
    }
LAB_08014c52:
    if ((DAT_20000332 & 1) != 0) {
      FUN_08018ea8((int)(short)(_DAT_2000032a + 0x9f),0x14,(int)(short)(_DAT_2000032a + 0x9f),0xdc);
      FUN_08018ea8((int)(short)(_DAT_2000032c + 0x9f),0x14,(int)(short)(_DAT_2000032c + 0x9f),0xdc);
    }
    uVar9 = (uint)DAT_20000332;
    if ((int)(uVar9 << 0x1e) < 0) {
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_2000032e),0x135,(int)(short)(0x78 - _DAT_2000032e));
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_20000330),0x135,(int)(short)(0x78 - _DAT_20000330));
      uVar9 = (uint)DAT_20000332;
    }
    if (uVar9 != 0) {
      sVar8 = 0x19;
      bVar7 = 0;
      if ((uVar9 & 1) != 0) goto LAB_08014d06;
      do {
        if (bVar7 == 0) {
          bVar7 = 3;
        }
        else if (bVar7 == 6) break;
LAB_08014d06:
        do {
          bVar6 = bVar7;
          if (bVar7 == 3) {
            bVar6 = 6;
          }
          if ((int)(uVar9 << 0x1e) < 0) {
            bVar6 = bVar7;
          }
          FUN_08008154(0xe,(int)sVar8,0x32,0x10);
          uVar39 = FUN_0803ed70(*(undefined4 *)(&DAT_20000334 + (uint)bVar6 * 4));
          FUN_080003b4(&DAT_20008360,0x80bcc11,(int)uVar39,(int)((ulonglong)uVar39 >> 0x20));
          FUN_08008154(0x40,(int)sVar8,100,0x10);
          if (5 < bVar6) goto LAB_08014dbe;
          uVar9 = (uint)DAT_20000332;
          bVar7 = bVar6 + 1;
          sVar8 = sVar8 + 0xc;
        } while ((DAT_20000332 & 1) != 0);
      } while( true );
    }
  }
LAB_08014dbe:
  fVar4 = DAT_08014fd0;
  if (((DAT_2000012d != '\0') && (_DAT_20000130 != 0)) &&
     ((DAT_2000012c == '\0' && (uVar9 = (uint)_DAT_20000eae, 1 < uVar9)))) {
    if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
      uVar18 = (uint)(DAT_20000eb7 >> 4);
      uVar26 = DAT_20000eb7 & 0xf;
      uVar25 = 0;
      do {
        uVar22 = uVar25;
        if (((uVar26 <= uVar18) || (uVar22 = uVar18, uVar26 <= uVar18 + 1)) ||
           (uVar30 = uVar18 + 2, uVar22 = uVar18 + 1, uVar26 <= uVar30)) break;
        uVar25 = uVar18 + 3;
        uVar18 = uVar18 + 4;
        uVar22 = uVar30;
      } while (uVar25 < uVar26);
      uVar25 = 0;
      while( true ) {
        uVar26 = _DAT_20000f04;
        uVar18 = _DAT_20000f00;
        uVar30 = uVar25 & 0xffff;
        uVar38 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9),
                              _DAT_20000f04 * uVar9 +
                              (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar9 >> 0x20),1000,0);
        uVar27 = (uint)DAT_20000efc;
        iVar13 = *(int *)(uVar22 * 4 + 0x20000ef4);
        uVar9 = uVar27;
        if (iVar13 == 0) {
          uVar9 = 1;
        }
        lVar37 = (ulonglong)uVar9 * (uVar38 & 0xffffffff);
        uVar11 = (uint)lVar37;
        uVar16 = uVar9 * (int)(uVar38 >> 0x20) + (int)((ulonglong)lVar37 >> 0x20);
        uVar19 = 0x12d - _DAT_20000ed8;
        uVar9 = (int)uVar19 >> 0x1f;
        if (uVar9 < uVar16 || uVar16 - uVar9 < (uint)(uVar19 <= uVar11)) {
          uVar11 = uVar19;
          uVar16 = uVar9;
        }
        if (-(uVar16 - (uVar11 == 0)) < (uint)(uVar11 - 1 <= uVar30)) break;
        fVar33 = (float)VectorUnsignedToFloat(uVar30,(byte)(in_fpscr >> 0x15) & 3);
        fVar12 = (float)FUN_0803ee1a(uVar18,uVar26);
        fVar31 = (float)VectorUnsignedToFloat(uVar27,(byte)(in_fpscr >> 0x15) & 3);
        if (iVar13 == 0) {
          fVar31 = 1.0;
        }
        fVar34 = (float)VectorUnsignedToFloat((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
        fVar34 = fVar33 / ((fVar12 / fVar4) * fVar31) + fVar34;
        FUN_08018ea8((int)(fVar34 + 9.0),0x78 - *(char *)(_DAT_20000130 + uVar30),
                     (int)(fVar34 + 10.0),0x78 - *(char *)(_DAT_20000130 + (uVar25 & 0xffff) + 1));
        uVar9 = (uint)_DAT_20000eae;
        uVar25 = uVar25 + 1;
      }
    }
    else {
      uVar18 = (uint)_DAT_20000eac;
      uVar25 = uVar18;
      if ((int)uVar18 < (int)(uVar9 + uVar18 + -1)) {
        do {
          FUN_08018ea8((int)(short)((short)uVar25 + 9),0x78 - *(char *)(_DAT_20000130 + uVar18),
                       (int)(short)((short)uVar25 + 10),0x78 - *(char *)(uVar18 + _DAT_20000130 + 1)
                      );
          uVar25 = uVar25 + 1;
          uVar18 = uVar25 & 0xffff;
        } while ((int)uVar18 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
      }
    }
  }
  if ((_DAT_20000122 != 0) && (DAT_2000012c == '\0')) {
    uVar17 = 0;
    uVar9 = _DAT_20000114 + 100 & 0xffff;
    iVar13 = 0xdc - uVar9;
    if (uVar9 < 0xc9) {
      uVar9 = 200;
    }
    else {
      iVar13 = 0x14;
    }
    iVar28 = (int)(short)iVar13;
    iVar20 = (int)(short)(((short)uVar9 - _DAT_20000114) + -0x50);
    uVar9 = 0xc;
    while( true ) {
      if (((uVar17 < 3) && ((uint)_DAT_20008350 <= uVar9 - 3)) &&
         ((uVar9 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar28 &&
           (iVar28 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((iVar20 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar9) * 2
         + -6) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (uVar9 == 0x138) break;
      if ((((uVar17 < 3) && ((uint)_DAT_20008350 <= uVar9 - 2)) &&
          (uVar9 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar28 &&
          (iVar28 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar20 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2
         + -4) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (((uVar17 < 3) && ((uint)_DAT_20008350 <= uVar9 - 1)) &&
         ((uVar9 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar28 &&
           (iVar28 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar20 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2
         + -2) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if ((((uVar17 < 3) && (_DAT_20008350 <= uVar9)) &&
          (uVar9 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar28 &&
          (iVar28 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (iVar20 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar9) * 2)
             = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      uVar9 = uVar9 + 4;
    }
    uVar9 = (byte)(&DAT_200000fa)[DAT_2000010e] / 3;
    uVar35 = VectorSignedToFloat((int)(short)_DAT_20000114 -
                                 (int)(char)(&DAT_200000fc)[DAT_2000010e],
                                 (byte)(in_fpscr >> 0x15) & 3);
    uVar39 = FUN_0803e5da(*(undefined2 *)
                           (&DAT_0804bfb8 +
                           (uVar9 * -3 + (uint)(byte)(&DAT_200000fa)[DAT_2000010e] & 0xff) * 2));
    uVar21 = FUN_0803e5da(uVar9);
    uVar32 = (undefined4)DAT_08015328;
    uVar21 = FUN_0803c8e0(uVar32,param_2,uVar21);
    uVar39 = FUN_0803e77c(uVar21,extraout_s1,(int)uVar39,(int)((ulonglong)uVar39 >> 0x20));
    uVar21 = FUN_0803e5da(*(undefined1 *)(DAT_2000010e + 0x200000fe));
    uVar21 = FUN_0803c8e0(uVar32,extraout_s1,uVar21);
    uVar39 = FUN_0803e77c((int)uVar39,(int)((ulonglong)uVar39 >> 0x20),uVar21,extraout_s1_00);
    uVar39 = FUN_0803e124((int)uVar39,(int)((ulonglong)uVar39 >> 0x20),(int)DAT_08015330,
                          (int)((ulonglong)DAT_08015330 >> 0x20));
    uVar40 = FUN_0803ed70(uVar35);
    FUN_0803e77c((int)uVar39,(int)((ulonglong)uVar39 >> 0x20),(int)uVar40,
                 (int)((ulonglong)uVar40 >> 0x20));
    fVar12 = (float)FUN_0803df48();
    pcVar15 = (char *)0x8015aa8;
    fVar4 = fVar12 / DAT_08014fd0;
    if (ABS(fVar12) < DAT_08014fd0) {
      pcVar15 = s___2fmV_08015aa0;
      fVar4 = fVar12;
    }
    uVar39 = FUN_0803ed70(fVar4);
    FUN_080003b4(&DAT_20008360,pcVar15,(int)uVar39,(int)((ulonglong)uVar39 >> 0x20));
    sVar8 = (short)iVar13 + -10;
    if (0xd6 < iVar13) {
      sVar8 = 0xca;
    }
    if (iVar13 < 0x1e) {
      sVar8 = 0x14;
    }
    FUN_08008154(0xa0,(int)sVar8,0x7d,0x14);
  }
  if ((_DAT_20000120 != 0) && (DAT_2000012c == '\0')) {
    iVar13 = (int)(short)(_DAT_20000112 + 0x9f);
    uVar17 = 0;
    uVar9 = 0x1f;
    while( true ) {
      if ((((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
          (iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar9 - 3 &&
          (uVar9 - 3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar9 - _DAT_20008352) + -3) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (uVar9 == 0xdf) break;
      if (((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
         ((iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          (((uint)_DAT_20008352 <= uVar9 - 2 &&
           (uVar9 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar9 - _DAT_20008352) + -2) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if ((((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
          (iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar9 - 1 &&
          (uVar9 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((~(uint)_DAT_20008352 + uVar9) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      if (((uVar17 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
         ((iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          ((_DAT_20008352 <= uVar9 && (uVar9 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((uVar9 - _DAT_20008352) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2) =
             0x7e2;
      }
      uVar17 = uVar17 + 1;
      if (4 < uVar17) {
        uVar17 = 0;
      }
      uVar9 = uVar9 + 4;
    }
    uVar39 = FUN_0803e538(_DAT_20000118,_DAT_2000011c);
    uVar39 = FUN_0803e124((int)uVar39,(int)((ulonglong)uVar39 >> 0x20),(int)DAT_08015338,
                          (int)((ulonglong)DAT_08015338 >> 0x20));
    uVar38 = (ulonglong)DAT_08015340 >> 0x20;
    uVar21 = (undefined4)((ulonglong)DAT_08015330 >> 0x20);
    uVar32 = (undefined4)DAT_08015330;
    while( true ) {
      uStack00000034 = (uint)((ulonglong)uVar39 >> 0x20);
      uVar35 = (undefined4)uVar39;
      iVar13 = FUN_0803ee0c(uVar35,uStack00000034 & 0x7fffffff | (uint)uVar38 & 0x80000000,uVar32,
                            uVar21);
      if (iVar13 != 0) break;
      uVar39 = FUN_0803e124(uVar35,uStack00000034,uVar32,uVar21);
    }
    uStack00000034 = uStack00000034 ^ 0x80000000;
    FUN_080003b4(&DAT_20008360,0x80bcc11,uVar35,uStack00000034);
    FUN_08008154(0,0xb4,0x140,0x14);
  }
  if ((DAT_2000012c != '\0') && (FUN_08021b40(), DAT_2000012c == '\0')) {
    sVar8 = 0x78 - _DAT_20000114;
    if (200 < (ushort)(_DAT_20000114 + 100)) {
      sVar8 = 0x14;
    }
    FUN_08019af8(0x136,(int)sVar8,0x13d,(int)(short)(sVar8 + 5));
  }
  if (((((DAT_20000f08 != -1) && (DAT_20000f08 != '\0')) ||
       ((DAT_20000127 != '\0' || DAT_20000128 != '\0') || DAT_2000012b != '\0')) ||
      ((uVar9 = (uint)DAT_20000126, DAT_2000010f == '\x02' && (uVar9 != 0)))) ||
     ((uVar9 == 0 &&
      (uVar9 = -(uint)(_DAT_20000f00 >= 10) - _DAT_20000f04,
      -_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))))) {
    FUN_08019af8(0xea,0x51,0x45,0x51);
    FUN_08019af8(0x45,0x51,0xef,0x55);
    FUN_08019af8(0xef,0x55,0x3e,0x59);
    FUN_08019af8(0x3e,0x59,0x101,0x55);
    FUN_08019af8(0x101,0x55,0x3e,0xa1);
    FUN_08019af8(0x3e,0xa1,0x101,0x98);
    FUN_08018fcc(0x3d,0x50,0xc6,0x53);
    uVar9 = extraout_r1;
  }
  if ((DAT_20000f08 == '\x03') || (DAT_20000f08 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (DAT_20000f08 == '\x01') {
    DAT_20000f13 = FUN_08034878(0x80bc18b,uVar9);
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,DAT_20000f09);
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    DAT_20000f08 = -1;
    DAT_20001060 = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
  if ((DAT_2000010f == '\x02') && (DAT_20000126 != 0)) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (DAT_20000127 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_20000128 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((DAT_20000126 == 0) && (-_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))) {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_2000012b != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  DAT_20001068 = DAT_20001068 + '\x01';
  return;
}



// Attempting to create function at 0x08012244
// Successfully created function
// ==========================================
// Function: FUN_08012244 @ 08012244
// Size: 1 bytes
// ==========================================

/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08015214 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

void FUN_08012244(undefined4 param_1,undefined4 param_2,uint param_3,undefined2 param_4,
                 ushort param_5)

{
  ushort uVar1;
  bool bVar2;
  float fVar3;
  char cVar4;
  byte bVar5;
  byte bVar6;
  short sVar7;
  uint uVar8;
  int *piVar9;
  uint uVar10;
  float fVar11;
  int iVar12;
  byte *pbVar13;
  char *pcVar14;
  uint uVar15;
  uint extraout_r1;
  ushort uVar16;
  uint uVar17;
  uint uVar18;
  int iVar19;
  undefined4 uVar20;
  uint uVar21;
  ushort *unaff_r5;
  int iVar22;
  undefined1 *unaff_r6;
  uint uVar23;
  uint uVar24;
  int unaff_r8;
  uint uVar25;
  int iVar26;
  byte *pbVar27;
  uint in_fpscr;
  uint uVar28;
  float fVar29;
  undefined4 uVar30;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  float fVar31;
  float fVar32;
  undefined4 uVar33;
  undefined8 uVar34;
  longlong lVar35;
  ulonglong uVar36;
  undefined8 uVar37;
  undefined8 uVar38;
  uint uStack00000034;
  undefined4 in_stack_0000003c;
  undefined8 in_stack_00000040;
  undefined8 in_stack_00000048;
  undefined8 in_stack_00000050;
  undefined8 in_stack_00000058;
  undefined8 in_stack_00000060;
  undefined4 in_stack_0000006c;
  undefined4 in_stack_00000070;
  undefined4 in_stack_00000074;
  undefined4 in_stack_00000078;
  undefined4 in_stack_0000007c;
  undefined4 in_stack_00000080;
  undefined4 in_stack_00000084;
  undefined4 in_stack_00000088;
  undefined4 in_stack_0000008c;
  
code_r0x08012244:
  *(undefined2 *)
   (*(int *)(unaff_r5 + 4) +
    ((uint)_DAT_20008354 * (~(uint)*(ushort *)(unaff_r6 + 2) + param_3) - (uint)*unaff_r5) * 2 +
   0x44) = param_4;
code_r0x0801226c:
  uVar8 = param_3;
  param_5 = param_5 + 1;
  if (4 < param_5) {
    param_5 = 0;
  }
  if ((((param_5 == 0) && (_DAT_20008350 < 0x23)) &&
      (0x22 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
     ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
    *(undefined2 *)
     (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
     0x44) = param_4;
  }
  param_5 = param_5 + 1;
  param_3 = uVar8 + 3;
  if (4 < param_5) {
    param_5 = 0;
  }
  if (param_3 != 0xdf) {
    if (((param_5 == 0) && (_DAT_20008350 < 0x23)) &&
       ((0x22 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 + 1 &&
         (uVar8 + 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((param_3 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x44) =
           param_4;
    }
    param_5 = param_5 + 1;
    if (4 < param_5) {
      param_5 = 0;
    }
    if ((((param_5 != 0) || (0x22 < _DAT_20008350)) ||
        ((uint)_DAT_20008350 + (uint)_DAT_20008354 < 0x23)) ||
       ((uVar8 + 2 < (uint)_DAT_20008352 ||
        (unaff_r5 = (ushort *)&DAT_20008350, (uint)_DAT_20008352 + (uint)_DAT_20008356 <= uVar8 + 2)
        ))) goto code_r0x0801226c;
    unaff_r6 = &DAT_20008350;
    goto code_r0x08012244;
  }
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x3c)) &&
       ((0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x76) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x3c)) &&
        (0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x76) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x3c)) &&
       ((0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x76) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x55)) &&
        (0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xa8) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x55)) &&
       ((0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xa8) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x55)) &&
        (0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0xa8) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x6e)) &&
       ((0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xda) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x6e)) &&
        (0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xda) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x6e)) &&
       ((0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0xda) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x87)) &&
        (0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x10c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x87)) &&
       ((0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x10c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x87)) &&
        (0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x10c) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa0)) &&
       ((0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13e) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa0)) &&
        (0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13e) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa0)) &&
       ((0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x13e) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xb9)) &&
        (0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xb9)) &&
       ((0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xb9)) &&
        (0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x170) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xd2)) &&
       ((0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xd2)) &&
        (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xd2)) &&
       ((0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x1a2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xeb)) &&
        (0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xeb)) &&
       ((0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xeb)) &&
        (0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x1d4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
       ((0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x206) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
        (0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x206) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
       ((0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x206) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x11d)) &&
        (0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x238) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x11d)) &&
       ((0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x238) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x11d)) &&
        (0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x238) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
       ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
       ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
        (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
       ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
        (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
        (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
       ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
        (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
       ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
       ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
        (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
       ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
        (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
        (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
       ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
        (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x9e)) &&
       ((0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x9e)) &&
        (0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x9e)) &&
       ((0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x13a) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x9f)) &&
        (0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x9f)) &&
       ((0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x9f)) &&
        (0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x13c) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa1)) &&
       ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa1)) &&
        (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa1)) &&
       ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x140) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
        (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
       ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
        (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x142) = 0x3a29;
    }
    pbVar13 = _DAT_20000138;
    bVar6 = DAT_20000136;
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar8 = (uint)*(byte *)(unaff_r8 + 0x3c);
  if ((uVar8 != 0) && (DAT_20000126 != 0)) {
    if (uVar8 == 3) {
      if (_DAT_20000138 != (byte *)0x0) {
        if (DAT_2000012c == '\0') {
          uVar8 = (uint)DAT_2000010d;
          if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
            uVar17 = (uint)(DAT_2000010d >> 4);
            uVar23 = (uint)DAT_2000105b;
            do {
              if ((*(int *)(pbVar13 + uVar17 * 4 + 4) != 0) && (*(short *)(pbVar13 + 0xe) != 0)) {
                uVar24 = (uint)*pbVar13;
                uVar1 = *(ushort *)(uVar23 * 4 + 0x80bb40c + uVar17 * 2);
                uVar16 = *(ushort *)(pbVar13 + 0xc);
                iVar12 = *(int *)(pbVar13 + uVar17 * 4 + 4);
                uVar1 = (ushort)(uint)((ulonglong)(uVar24 * (((uint)uVar1 << 0x15) >> 0x1a)) *
                                       0x80808081 >> 0x22) & 0xffe0 |
                        (ushort)((uVar24 * (uVar1 & 0x1f)) / 0xff) |
                        (ushort)(((uint)((ulonglong)(uVar24 * (uVar1 >> 0xb)) * 0x80808081 >> 0x20)
                                 & 0xfffff80) << 4);
                do {
                  iVar26 = (short)uVar16 * 0x1a;
                  iVar19 = (int)(short)(uVar16 + 9);
                  iVar22 = 0xc6;
                  uVar24 = 2;
                  while( true ) {
                    if (((((*(byte *)(iVar12 + iVar26 + ((uVar24 - 2) * 0x10000 >> 0x13)) >>
                            (uVar24 - 2 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x16U &&
                        (iVar22 + 0x16U < (uint)_DAT_20008356 + (uint)_DAT_20008352)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x16) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if ((((*(byte *)(iVar12 + ((uVar24 - 1) * 0x10000 >> 0x13) + iVar26) >>
                           (uVar24 - 1 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                       ((iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
                        (((uint)_DAT_20008352 <= iVar22 + 0x15U &&
                         (iVar22 + 0x15U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x15) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (((((*(byte *)(iVar12 + ((uVar24 << 0x10) >> 0x13) + iVar26) >> (uVar24 & 7)
                           & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x14U &&
                        (iVar22 + 0x14U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x14) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (iVar22 == 0) break;
                    iVar22 = iVar22 + -3;
                    uVar24 = uVar24 + 3;
                  }
                  uVar16 = uVar16 + 1;
                } while ((uint)uVar16 <
                         (uint)*(ushort *)(pbVar13 + 0xc) + (uint)*(ushort *)(pbVar13 + 0xe));
              }
              uVar17 = uVar17 + 1;
            } while (uVar17 != (uVar8 & 0xf));
          }
        }
        else {
          iVar12 = *(int *)(_DAT_20000138 + 4);
          if (iVar12 != 0) {
            uVar8 = ((uint)*_DAT_20000138 * 0x1f) / 0xff;
            uVar16 = (ushort)uVar8 | (ushort)(uVar8 << 0xb);
            iVar26 = 0;
            do {
              iVar22 = iVar26 * 0x1a;
              uVar8 = iVar26 * 0x10000 + 0x3b0000 >> 0x10;
              iVar19 = 0xc9;
              uVar23 = 2;
              do {
                if ((((*(byte *)(iVar12 + ((uVar23 - 2) * 0x1000000 >> 0x1b) + iVar22) >>
                       (uVar23 - 2 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x13U &&
                     (iVar19 + 0x13U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x13) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                if (((((*(byte *)(iVar12 + ((uVar23 - 1) * 0x1000000 >> 0x1b) + iVar22) >>
                        (uVar23 - 1 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                    (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
                   (((uint)_DAT_20008352 <= iVar19 + 0x12U &&
                    (iVar19 + 0x12U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x12) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                if ((((*(byte *)(iVar12 + ((uVar23 << 0x18) >> 0x1b) + iVar22) >> (uVar23 & 7) & 1)
                      != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x11U &&
                     (iVar19 + 0x11U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x11) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                iVar19 = iVar19 + -3;
                uVar23 = uVar23 + 3;
              } while (iVar19 != 0);
              iVar26 = iVar26 + 1;
            } while (iVar26 != 0xc9);
          }
        }
      }
    }
    else {
      DAT_20000136 = DAT_20000135;
      bVar5 = DAT_20000135;
      if (DAT_20000135 <= bVar6) {
        bVar5 = DAT_20000135 + 100;
      }
      DAT_20000137 = bVar5 - bVar6;
      if (_DAT_20000138 != (byte *)0x0) {
        pbVar13 = &DAT_20000138;
        uVar36 = (ulonglong)DAT_08014350 >> 0x20;
        uVar20 = (undefined4)DAT_08014350;
        pbVar27 = _DAT_20000138;
        while( true ) {
          cVar4 = DAT_20000137;
          uVar37 = FUN_0803e5da(*pbVar27);
          uVar30 = (undefined4)((ulonglong)uVar37 >> 0x20);
          uVar38 = FUN_0803e5da(cVar4);
          uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),uVar20,(int)uVar36);
          uVar34 = FUN_0803e50a(3 - uVar8);
          uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),(int)uVar34,
                                (int)((ulonglong)uVar34 >> 0x20));
          uVar33 = (undefined4)((ulonglong)uVar38 >> 0x20);
          iVar12 = FUN_0803edf0((int)uVar38,uVar33,(int)uVar37,uVar30);
          if (iVar12 != 0) break;
          FUN_0803eb94((int)uVar37,uVar30,(int)uVar38,uVar33);
          bVar6 = FUN_0803e450();
          cVar4 = DAT_2000012c;
          *pbVar27 = bVar6;
          if (cVar4 == '\0') {
            uVar8 = (uint)DAT_2000010d;
            if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
              uVar23 = (uint)(DAT_2000010d >> 4);
              do {
                if (((*(int *)(_DAT_20000138 + uVar23 * 4 + 4) != 0) &&
                    (1 < *(ushort *)(pbVar27 + 0xe))) && (1 < *(ushort *)(pbVar27 + 0xe))) {
                  uVar8 = 0;
                  uVar16 = 0;
                  do {
                    FUN_08018ea8((int)(short)(*(short *)(pbVar27 + 0xc) + uVar16 + 9),
                                 0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + uVar23 * 4 + 4) + uVar8),
                                 (int)(short)(*(short *)(pbVar27 + 0xc) + uVar16 + 10),
                                 0xf8 - (uint)*(byte *)(uVar8 + *(int *)(pbVar27 + uVar23 * 4 + 4) +
                                                       1));
                    pbVar27 = *(byte **)pbVar13;
                    uVar16 = uVar16 + 1;
                    uVar8 = (uint)uVar16;
                  } while ((int)uVar8 < (int)(*(ushort *)(pbVar27 + 0xe) - 1));
                  uVar8 = (uint)DAT_2000010d;
                }
                uVar23 = uVar23 + 1;
              } while (uVar23 < (uVar8 & 0xf));
            }
          }
          else if (((*(int *)(_DAT_20000138 + 4) != 0) && (*(int *)(_DAT_20000138 + 8) != 0)) &&
                  ((1 < *(ushort *)(pbVar27 + 0xe) &&
                   (uVar23 = (uint)*(ushort *)(pbVar27 + 0xc), uVar8 = uVar23,
                   (int)uVar23 < (int)(*(ushort *)(pbVar27 + 0xe) + uVar23 + -1))))) {
            do {
              FUN_08018da0(*(byte *)(*(int *)(pbVar27 + 4) + uVar23) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + 8) + uVar23),
                           *(byte *)(*(int *)(pbVar27 + 4) + uVar23 + 1) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + 8) + uVar23 + 1));
              pbVar27 = *(byte **)pbVar13;
              uVar23 = uVar8 + 1 & 0xffff;
              uVar8 = uVar8 + 1;
            } while ((int)uVar23 <
                     (int)((uint)*(ushort *)(pbVar27 + 0xc) + (uint)*(ushort *)(pbVar27 + 0xe) + -1)
                    );
          }
          pbVar13 = pbVar27 + 0x10;
          pbVar27 = *(byte **)pbVar13;
          if (pbVar27 == (byte *)0x0) goto LAB_080144ca;
LAB_08014000:
          uVar8 = (uint)DAT_20000134;
        }
        if (*(int *)(pbVar27 + 4) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar8 = *(int *)(pbVar27 + 4) - _DAT_20001078;
            if (uVar8 >> 10 < 0xaf) {
              uVar8 = uVar8 >> 5;
              uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
              if (uVar23 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
              }
            }
          }
        }
        if (*(int *)(*(int *)pbVar13 + 8) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar8 = *(int *)(*(int *)pbVar13 + 8) - _DAT_20001078;
            if (uVar8 >> 10 < 0xaf) {
              uVar8 = uVar8 >> 5;
              uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
              if (uVar23 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
              }
            }
          }
        }
        pbVar27 = *(byte **)(*(int *)pbVar13 + 0x10);
        if (_DAT_20000138 != (byte *)0x0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else if ((uint)((int)_DAT_20000138 - _DAT_20001078) >> 10 < 0xaf) {
            uVar8 = (uint)((int)_DAT_20000138 - _DAT_20001078) >> 5;
            uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
            if (uVar23 != 0) {
              FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
            }
          }
        }
        pbVar13 = &DAT_20000138;
        _DAT_20000138 = pbVar27;
        if (pbVar27 != (byte *)0x0) goto LAB_08014000;
      }
    }
  }
LAB_080144ca:
  fVar3 = DAT_08014948;
  uVar8 = (uint)_DAT_20000eae;
  if (uVar8 == 0) {
LAB_08014800:
    if (DAT_2000012c == '\0') {
      pbVar13 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar13 = &DAT_20000eb7;
      }
      bVar6 = *pbVar13;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar8 = (uint)(bVar6 >> 4);
        sVar7 = (ushort)(bVar6 >> 4) * 0xad + 9;
        do {
          if (*(char *)(uVar8 + 0x20000102) != '\0') {
            piVar9 = (int *)(uVar8 * 4 + 0x20000104);
            iVar12 = *piVar9;
            if (iVar12 != 0) {
              for (iVar26 = 0; iVar19 = (int)(short)(sVar7 + (short)iVar26),
                  FUN_08018ea8(iVar19,0x46,iVar19,0x46 - (uint)*(byte *)(iVar12 + iVar26)),
                  iVar26 != 0x7f; iVar26 = iVar26 + 1) {
                iVar12 = *piVar9;
              }
            }
          }
          uVar8 = uVar8 + 1;
          sVar7 = sVar7 + 0xad;
        } while (uVar8 != (bVar6 & 0xf));
        if (DAT_2000012c != '\0') goto LAB_080148ec;
      }
      if (_DAT_20000112 < 0x92) {
        if (_DAT_20000112 < -0x91) {
          iVar12 = 0x10;
          iVar26 = 0x10;
          uVar20 = 0x1e;
        }
        else {
          iVar12 = (int)(short)(_DAT_20000112 + 0x9a);
          iVar26 = (int)(short)(_DAT_20000112 + 0xa4);
          uVar20 = 0x14;
        }
      }
      else {
        iVar12 = 0x12e;
        iVar26 = 0x12e;
        uVar20 = 0x1e;
      }
      FUN_08019af8(iVar12,0x14,iVar26,uVar20);
    }
  }
  else {
    if (DAT_2000012c == '\0') {
      pbVar13 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar13 = &DAT_20000eb7;
      }
      bVar6 = *pbVar13;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar23 = (uint)(bVar6 >> 4);
        do {
          if (1 < uVar8) {
            if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))
               ) {
              uVar17 = 0;
              while( true ) {
                uVar21 = _DAT_20000f04;
                uVar24 = _DAT_20000f00;
                uVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                                      _DAT_20000f04 * uVar8 +
                                      (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),
                                      1000,0);
                uVar25 = (uint)DAT_20000efc;
                iVar12 = *(int *)(uVar23 * 4 + 0x20000ef4);
                uVar28 = uVar25;
                if (iVar12 == 0) {
                  uVar28 = 1;
                }
                lVar35 = (ulonglong)uVar28 * (uVar36 & 0xffffffff);
                uVar10 = (uint)lVar35;
                uVar15 = uVar28 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
                uVar18 = 0x12d - _DAT_20000ed8;
                uVar28 = (int)uVar18 >> 0x1f;
                if (uVar28 < uVar15 || uVar15 - uVar28 < (uint)(uVar18 <= uVar10)) {
                  uVar10 = uVar18;
                  uVar15 = uVar28;
                }
                if (-(uVar15 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= (uVar17 & 0xffff))) break;
                fVar31 = (float)VectorUnsignedToFloat(uVar17 & 0xffff,(byte)(in_fpscr >> 0x15) & 3);
                fVar11 = (float)FUN_0803ee1a(uVar24,uVar21);
                fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
                if (iVar12 == 0) {
                  fVar29 = 1.0;
                }
                iVar12 = uVar23 * 0x12d + 0x200000f8 + (uVar17 & 0xffff);
                fVar32 = (float)VectorUnsignedToFloat
                                          ((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
                fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
                FUN_08018ea8((int)(fVar32 + 9.0),0xf8 - (uint)*(byte *)(iVar12 + 0x356),
                             (int)(fVar32 + 10.0),0xf8 - (uint)*(byte *)(iVar12 + 0x357));
                uVar8 = (uint)_DAT_20000eae;
                uVar17 = uVar17 + 1;
              }
            }
            else {
              uVar24 = (uint)_DAT_20000eac;
              uVar17 = uVar24;
              if ((int)uVar24 < (int)(uVar24 + uVar8 + -1)) {
                do {
                  iVar12 = uVar24 + uVar23 * 0x12d + 0x200000f8;
                  FUN_08018ea8((int)(short)((short)uVar17 + 9),
                               0xf8 - (uint)*(byte *)(iVar12 + 0x356),
                               (int)(short)((short)uVar17 + 10),
                               0xf8 - (uint)*(byte *)(iVar12 + 0x357));
                  uVar8 = (uint)_DAT_20000eae;
                  uVar24 = uVar17 + 1 & 0xffff;
                  uVar17 = uVar17 + 1;
                } while ((int)uVar24 < (int)(_DAT_20000eac + uVar8 + -1));
              }
            }
          }
          uVar23 = uVar23 + 1;
        } while (uVar23 != (bVar6 & 0xf));
      }
      goto LAB_08014800;
    }
    if (1 < uVar8) {
      if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
        uVar23 = 0;
        while( true ) {
          uVar24 = uVar23 & 0xffff;
          lVar35 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                                _DAT_20000f04 * uVar8 +
                                (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),1000,0);
          uVar8 = (uint)((ulonglong)lVar35 >> 0x20);
          uVar17 = 0x12d - _DAT_20000ed8;
          if ((uint)((int)uVar17 >> 0x1f) < uVar8 ||
              uVar8 - ((int)uVar17 >> 0x1f) < (uint)(uVar17 <= (uint)lVar35)) {
            lVar35 = (longlong)(int)uVar17;
          }
          if (-((int)((ulonglong)lVar35 >> 0x20) - (uint)((int)lVar35 == 0)) <
              (uint)((int)lVar35 - 1U <= uVar24)) break;
          FUN_08018da0(*(byte *)(uVar24 + 0x2000044e) + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057b)[uVar24],
                       (byte)(&DAT_2000044f)[uVar23 & 0xffff] + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057c)[uVar23 & 0xffff]);
          uVar8 = (uint)_DAT_20000eae;
          uVar23 = uVar23 + 1;
        }
      }
      else {
        uVar17 = (uint)_DAT_20000eac;
        uVar23 = uVar17;
        if ((int)uVar17 < (int)(uVar17 + uVar8 + -1)) {
          do {
            FUN_08018da0(*(byte *)(uVar17 + 0x2000044e) + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057b)[uVar17],
                         (byte)(&DAT_2000044f)[uVar17] + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057c)[uVar17]);
            uVar17 = uVar23 + 1 & 0xffff;
            uVar23 = uVar23 + 1;
          } while ((int)uVar17 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
        }
      }
      goto LAB_08014800;
    }
  }
LAB_080148ec:
  fVar11 = DAT_08014d00;
  fVar3 = DAT_08014948;
  if ((((_DAT_20000144 == 0) || (DAT_2000012c != '\0')) || (uVar8 = (uint)DAT_2000010c, uVar8 == 0))
     || (DAT_20000140 != '\0')) {
LAB_08014c4c:
    if (DAT_2000012c == '\0') goto LAB_08014c52;
  }
  else {
    uVar17 = ~(uVar8 << 4) & 0x10;
    uVar23 = 0;
    bVar6 = 0;
    if ((int)(uVar8 << 0x1e) < 0) {
      do {
        if ((uVar23 & 0xff) == 0) {
          uVar23 = uVar17;
        }
        uVar8 = uVar23 & 0xff;
        if ((_DAT_20000144 >> uVar8 & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (0x1e < uVar8) break;
        if ((_DAT_20000144 >> (uVar23 + 1 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (uVar8 == 0x1e) break;
        if ((_DAT_20000144 >> (uVar23 + 2 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (0x1c < uVar8) break;
        if ((_DAT_20000144 >> (uVar23 + 3 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        uVar23 = uVar23 + 4;
      } while (uVar8 != 0x1c);
    }
    else {
      do {
        if ((uVar23 & 0xff) == 0) {
          uVar23 = uVar17;
        }
        uVar8 = uVar23 & 0xff;
        if (uVar8 == 0x10) break;
        if ((_DAT_20000144 >> uVar8 & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        uVar23 = uVar23 + 1;
      } while (uVar8 < 0x1f);
    }
    if (bVar6 != 0) {
      bVar2 = DAT_20001058 == '\x01';
      iVar12 = ((int)(bVar6 - 1) / 3) * 0xfff1 + 0xcc;
      uVar8 = (uint)bVar6 % 3;
      uVar23 = 0;
      sVar7 = 0xc;
      do {
        uVar24 = uVar17 + 1;
        if (uVar8 == (uVar23 & 0xff) && uVar8 != 0) {
          sVar7 = 0xc;
          iVar12 = iVar12 + 0xf;
        }
        if ((uVar24 & 0xff) < 0x21) {
          uVar24 = 0x20;
        }
        do {
          uVar21 = uVar17;
          if ((_DAT_20000144 >> (uVar17 & 0xff) & 1) != 0) {
LAB_08014a8a:
            uVar17 = uVar21 + 1;
            if (0xf < (uVar21 & 0xff)) goto LAB_08014a96;
            piVar9 = (int *)(&DAT_20000208 + (uVar21 & 0xff) * 4);
            goto LAB_08014ab2;
          }
          uVar21 = uVar17 + 1;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar21 = uVar17 + 2;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar21 = uVar17 + 3;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar17 = uVar17 + 4;
        } while ((uVar17 & 0xff) < 0x20);
        uVar17 = uVar24 + 1;
        uVar21 = uVar24;
LAB_08014a96:
        uVar21 = uVar21 - 0x10;
        piVar9 = (int *)(&DAT_20000238 + (uVar21 & 0xff) * 4);
LAB_08014ab2:
        iVar19 = *piVar9;
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        fVar29 = (float)VectorSignedToFloat(iVar19,(byte)(in_fpscr >> 0x15) & 3);
        if ((uVar21 & 0xfe) == 2) {
          fVar29 = fVar29 / fVar11;
        }
        if ((uVar21 & 0xff) - 6 < 6) {
          fVar29 = fVar29 / fVar3;
        }
        fVar31 = ABS(fVar29);
        uVar24 = in_fpscr & 0xfffffff | (uint)(fVar31 < fVar3) << 0x1f;
        uVar28 = uVar24 | (uint)(NAN(fVar31) || NAN(fVar3)) << 0x1c;
        if ((byte)(uVar24 >> 0x1f) == ((byte)(uVar28 >> 0x1c) & 1)) {
          fVar31 = fVar31 / fVar3;
          fVar29 = fVar29 / fVar3;
        }
        uVar20 = *(undefined4 *)
                  (&DAT_0804bf28 + (uVar21 & 0xff) * 4 + ((uint)bVar2 | (uint)bVar2 << 1) * 0x10);
        FUN_0803ed70(fVar29);
        uVar24 = uVar28 & 0xfffffff | (uint)(fVar31 < fVar11) << 0x1f;
        in_fpscr = uVar24 | (uint)(NAN(fVar31) || NAN(fVar11)) << 0x1c;
        pcVar14 = s__s__1f_s_08015a70;
        if ((byte)(uVar24 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1)) {
          pcVar14 = s__s__2f_s_08015a64;
        }
        FUN_080003b4(&DAT_20008360,pcVar14,uVar20);
        FUN_08008154((int)sVar7,(int)(short)iVar12,100,0xf);
        uVar23 = uVar23 + 1;
        if ((uint)bVar6 <= (uVar23 & 0xff)) goto LAB_08014c4c;
        sVar7 = sVar7 + 100;
        if (0x137 < sVar7) {
          iVar12 = iVar12 + 0xf;
          sVar7 = 0xc;
        }
      } while( true );
    }
LAB_08014c52:
    if ((DAT_20000332 & 1) != 0) {
      FUN_08018ea8((int)(short)(_DAT_2000032a + 0x9f),0x14,(int)(short)(_DAT_2000032a + 0x9f),0xdc);
      FUN_08018ea8((int)(short)(_DAT_2000032c + 0x9f),0x14,(int)(short)(_DAT_2000032c + 0x9f),0xdc);
    }
    uVar8 = (uint)DAT_20000332;
    if ((int)(uVar8 << 0x1e) < 0) {
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_2000032e),0x135,(int)(short)(0x78 - _DAT_2000032e));
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_20000330),0x135,(int)(short)(0x78 - _DAT_20000330));
      uVar8 = (uint)DAT_20000332;
    }
    if (uVar8 != 0) {
      sVar7 = 0x19;
      bVar6 = 0;
      if ((uVar8 & 1) != 0) goto LAB_08014d06;
      do {
        if (bVar6 == 0) {
          bVar6 = 3;
        }
        else if (bVar6 == 6) break;
LAB_08014d06:
        do {
          bVar5 = bVar6;
          if (bVar6 == 3) {
            bVar5 = 6;
          }
          if ((int)(uVar8 << 0x1e) < 0) {
            bVar5 = bVar6;
          }
          FUN_08008154(0xe,(int)sVar7,0x32,0x10);
          uVar37 = FUN_0803ed70(*(undefined4 *)(&DAT_20000334 + (uint)bVar5 * 4));
          FUN_080003b4(&DAT_20008360,0x80bcc11,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
          FUN_08008154(0x40,(int)sVar7,100,0x10);
          if (5 < bVar5) goto LAB_08014dbe;
          uVar8 = (uint)DAT_20000332;
          bVar6 = bVar5 + 1;
          sVar7 = sVar7 + 0xc;
        } while ((DAT_20000332 & 1) != 0);
      } while( true );
    }
  }
LAB_08014dbe:
  fVar3 = DAT_08014fd0;
  if (((DAT_2000012d != '\0') && (_DAT_20000130 != 0)) &&
     ((DAT_2000012c == '\0' && (uVar8 = (uint)_DAT_20000eae, 1 < uVar8)))) {
    if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
      uVar17 = (uint)(DAT_20000eb7 >> 4);
      uVar24 = DAT_20000eb7 & 0xf;
      uVar23 = 0;
      do {
        uVar21 = uVar23;
        if (((uVar24 <= uVar17) || (uVar21 = uVar17, uVar24 <= uVar17 + 1)) ||
           (uVar28 = uVar17 + 2, uVar21 = uVar17 + 1, uVar24 <= uVar28)) break;
        uVar23 = uVar17 + 3;
        uVar17 = uVar17 + 4;
        uVar21 = uVar28;
      } while (uVar23 < uVar24);
      uVar23 = 0;
      while( true ) {
        uVar24 = _DAT_20000f04;
        uVar17 = _DAT_20000f00;
        uVar28 = uVar23 & 0xffff;
        uVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                              _DAT_20000f04 * uVar8 +
                              (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),1000,0);
        uVar25 = (uint)DAT_20000efc;
        iVar12 = *(int *)(uVar21 * 4 + 0x20000ef4);
        uVar8 = uVar25;
        if (iVar12 == 0) {
          uVar8 = 1;
        }
        lVar35 = (ulonglong)uVar8 * (uVar36 & 0xffffffff);
        uVar10 = (uint)lVar35;
        uVar15 = uVar8 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
        uVar18 = 0x12d - _DAT_20000ed8;
        uVar8 = (int)uVar18 >> 0x1f;
        if (uVar8 < uVar15 || uVar15 - uVar8 < (uint)(uVar18 <= uVar10)) {
          uVar10 = uVar18;
          uVar15 = uVar8;
        }
        if (-(uVar15 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= uVar28)) break;
        fVar31 = (float)VectorUnsignedToFloat(uVar28,(byte)(in_fpscr >> 0x15) & 3);
        fVar11 = (float)FUN_0803ee1a(uVar17,uVar24);
        fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
        if (iVar12 == 0) {
          fVar29 = 1.0;
        }
        fVar32 = (float)VectorUnsignedToFloat((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
        fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
        FUN_08018ea8((int)(fVar32 + 9.0),0x78 - *(char *)(_DAT_20000130 + uVar28),
                     (int)(fVar32 + 10.0),0x78 - *(char *)(_DAT_20000130 + (uVar23 & 0xffff) + 1));
        uVar8 = (uint)_DAT_20000eae;
        uVar23 = uVar23 + 1;
      }
    }
    else {
      uVar17 = (uint)_DAT_20000eac;
      uVar23 = uVar17;
      if ((int)uVar17 < (int)(uVar8 + uVar17 + -1)) {
        do {
          FUN_08018ea8((int)(short)((short)uVar23 + 9),0x78 - *(char *)(_DAT_20000130 + uVar17),
                       (int)(short)((short)uVar23 + 10),0x78 - *(char *)(uVar17 + _DAT_20000130 + 1)
                      );
          uVar23 = uVar23 + 1;
          uVar17 = uVar23 & 0xffff;
        } while ((int)uVar17 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
      }
    }
  }
  if ((_DAT_20000122 != 0) && (DAT_2000012c == '\0')) {
    uVar16 = 0;
    uVar8 = _DAT_20000114 + 100 & 0xffff;
    iVar12 = 0xdc - uVar8;
    if (uVar8 < 0xc9) {
      uVar8 = 200;
    }
    else {
      iVar12 = 0x14;
    }
    iVar26 = (int)(short)iVar12;
    iVar19 = (int)(short)(((short)uVar8 - _DAT_20000114) + -0x50);
    uVar8 = 0xc;
    while( true ) {
      if (((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((iVar19 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar8) * 2
         + -6) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -4) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -2) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if ((((uVar16 < 3) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2)
             = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar8 = (byte)(&DAT_200000fa)[DAT_2000010e] / 3;
    uVar33 = VectorSignedToFloat((int)(short)_DAT_20000114 -
                                 (int)(char)(&DAT_200000fc)[DAT_2000010e],
                                 (byte)(in_fpscr >> 0x15) & 3);
    uVar37 = FUN_0803e5da(*(undefined2 *)
                           (&DAT_0804bfb8 +
                           (uVar8 * -3 + (uint)(byte)(&DAT_200000fa)[DAT_2000010e] & 0xff) * 2));
    uVar20 = FUN_0803e5da(uVar8);
    uVar30 = (undefined4)DAT_08015328;
    uVar20 = FUN_0803c8e0(uVar30,param_2,uVar20);
    uVar37 = FUN_0803e77c(uVar20,extraout_s1,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    uVar20 = FUN_0803e5da(*(undefined1 *)(DAT_2000010e + 0x200000fe));
    uVar20 = FUN_0803c8e0(uVar30,extraout_s1,uVar20);
    uVar37 = FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar20,extraout_s1_00);
    uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)DAT_08015330,
                          (int)((ulonglong)DAT_08015330 >> 0x20));
    uVar38 = FUN_0803ed70(uVar33);
    FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                 (int)((ulonglong)uVar38 >> 0x20));
    fVar11 = (float)FUN_0803df48();
    pcVar14 = (char *)0x8015aa8;
    fVar3 = fVar11 / DAT_08014fd0;
    if (ABS(fVar11) < DAT_08014fd0) {
      pcVar14 = s___2fmV_08015aa0;
      fVar3 = fVar11;
    }
    uVar37 = FUN_0803ed70(fVar3);
    FUN_080003b4(&DAT_20008360,pcVar14,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    sVar7 = (short)iVar12 + -10;
    if (0xd6 < iVar12) {
      sVar7 = 0xca;
    }
    if (iVar12 < 0x1e) {
      sVar7 = 0x14;
    }
    FUN_08008154(0xa0,(int)sVar7,0x7d,0x14);
  }
  if ((_DAT_20000120 != 0) && (DAT_2000012c == '\0')) {
    iVar12 = (int)(short)(_DAT_20000112 + 0x9f);
    uVar16 = 0;
    uVar8 = 0x1f;
    while( true ) {
      if ((((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
          (iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 3 &&
          (uVar8 - 3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -3) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (uVar8 == 0xdf) break;
      if (((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
         ((iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -2) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if ((((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
          (iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((~(uint)_DAT_20008352 + uVar8) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
         ((iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((uVar8 - _DAT_20008352) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2) =
             0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar37 = FUN_0803e538(_DAT_20000118,_DAT_2000011c);
    uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)DAT_08015338,
                          (int)((ulonglong)DAT_08015338 >> 0x20));
    uVar36 = (ulonglong)DAT_08015340 >> 0x20;
    uVar20 = (undefined4)((ulonglong)DAT_08015330 >> 0x20);
    uVar30 = (undefined4)DAT_08015330;
    while( true ) {
      uStack00000034 = (uint)((ulonglong)uVar37 >> 0x20);
      uVar33 = (undefined4)uVar37;
      iVar12 = FUN_0803ee0c(uVar33,uStack00000034 & 0x7fffffff | (uint)uVar36 & 0x80000000,uVar30,
                            uVar20);
      if (iVar12 != 0) break;
      uVar37 = FUN_0803e124(uVar33,uStack00000034,uVar30,uVar20);
    }
    uStack00000034 = uStack00000034 ^ 0x80000000;
    FUN_080003b4(&DAT_20008360,0x80bcc11,uVar33,uStack00000034);
    FUN_08008154(0,0xb4,0x140,0x14);
  }
  if ((DAT_2000012c != '\0') && (FUN_08021b40(), DAT_2000012c == '\0')) {
    sVar7 = 0x78 - _DAT_20000114;
    if (200 < (ushort)(_DAT_20000114 + 100)) {
      sVar7 = 0x14;
    }
    FUN_08019af8(0x136,(int)sVar7,0x13d,(int)(short)(sVar7 + 5));
  }
  if ((((DAT_20000f08 != -1) && (DAT_20000f08 != '\0')) ||
      ((DAT_20000127 != '\0' || DAT_20000128 != '\0') || DAT_2000012b != '\0')) ||
     (((uVar8 = (uint)DAT_20000126, DAT_2000010f == '\x02' && (uVar8 != 0)) ||
      ((uVar8 == 0 &&
       (uVar8 = -(uint)(_DAT_20000f00 >= 10) - _DAT_20000f04,
       -_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))))))) {
    FUN_08019af8(0xea,0x51,0x45,0x51);
    FUN_08019af8(0x45,0x51,0xef,0x55);
    FUN_08019af8(0xef,0x55,0x3e,0x59);
    FUN_08019af8(0x3e,0x59,0x101,0x55);
    FUN_08019af8(0x101,0x55,0x3e,0xa1);
    FUN_08019af8(0x3e,0xa1,0x101,0x98);
    FUN_08018fcc(0x3d,0x50,0xc6,0x53);
    uVar8 = extraout_r1;
  }
  if ((DAT_20000f08 == '\x03') || (DAT_20000f08 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (DAT_20000f08 == '\x01') {
    DAT_20000f13 = FUN_08034878(0x80bc18b,uVar8);
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,DAT_20000f09);
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    DAT_20000f08 = -1;
    DAT_20001060 = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
  if ((DAT_2000010f == '\x02') && (DAT_20000126 != 0)) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (DAT_20000127 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_20000128 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((DAT_20000126 == 0) && (-_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))) {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_2000012b != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  DAT_20001068 = DAT_20001068 + '\x01';
  return;
}



// Attempting to create function at 0x08012344
// Successfully created function
// ==========================================
// Function: FUN_08012344 @ 08012344
// Size: 1 bytes
// ==========================================

/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08015214 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

void FUN_08012344(undefined4 param_1,undefined4 param_2,uint param_3,undefined2 param_4,
                 ushort param_5,int param_6)

{
  ushort uVar1;
  bool bVar2;
  float fVar3;
  char cVar4;
  byte bVar5;
  byte bVar6;
  short sVar7;
  uint uVar8;
  int *piVar9;
  uint uVar10;
  float fVar11;
  int iVar12;
  byte *pbVar13;
  char *pcVar14;
  uint uVar15;
  uint extraout_r1;
  ushort uVar16;
  uint uVar17;
  uint uVar18;
  int iVar19;
  undefined4 uVar20;
  uint uVar21;
  uint unaff_r5;
  int iVar22;
  int unaff_r6;
  uint uVar23;
  uint unaff_r7;
  uint uVar24;
  int unaff_r8;
  uint uVar25;
  int iVar26;
  byte *pbVar27;
  uint in_fpscr;
  uint uVar28;
  float fVar29;
  undefined4 uVar30;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  float fVar31;
  float fVar32;
  undefined4 uVar33;
  undefined8 uVar34;
  longlong lVar35;
  ulonglong uVar36;
  undefined8 uVar37;
  undefined8 uVar38;
  uint uStack00000034;
  undefined4 in_stack_0000003c;
  undefined8 in_stack_00000040;
  undefined8 in_stack_00000048;
  undefined8 in_stack_00000050;
  undefined8 in_stack_00000058;
  undefined8 in_stack_00000060;
  undefined4 in_stack_0000006c;
  undefined4 in_stack_00000070;
  undefined4 in_stack_00000074;
  undefined4 in_stack_00000078;
  undefined4 in_stack_0000007c;
  undefined4 in_stack_00000080;
  undefined4 in_stack_00000084;
  undefined4 in_stack_00000088;
  undefined4 in_stack_0000008c;
  
code_r0x08012344:
  *(undefined2 *)(param_6 + (unaff_r5 * (unaff_r6 + -2) - unaff_r7) * 2 + 0x76) = param_4;
code_r0x0801235c:
  uVar8 = param_3;
  param_5 = param_5 + 1;
  if (4 < param_5) {
    param_5 = 0;
  }
  if ((((param_5 == 0) && (_DAT_20008350 < 0x3c)) &&
      (0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
     (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))
     ) {
    *(undefined2 *)
     (_DAT_20008358 +
      ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x76) =
         param_4;
  }
  param_5 = param_5 + 1;
  if (4 < param_5) {
    param_5 = 0;
  }
  if (((param_5 == 0) && (_DAT_20008350 < 0x3c)) &&
     ((0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
      ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
    *(undefined2 *)
     (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
     0x76) = param_4;
  }
  param_5 = param_5 + 1;
  param_3 = uVar8 + 3;
  if (4 < param_5) {
    param_5 = 0;
  }
  if (param_3 != 0xdf) {
    if ((((param_5 != 0) || (0x3b < _DAT_20008350)) ||
        ((uint)_DAT_20008350 + (uint)_DAT_20008354 < 0x3c)) ||
       ((uVar8 + 1 < (uint)_DAT_20008352 || ((uint)_DAT_20008352 + (uint)_DAT_20008356 <= uVar8 + 1)
        ))) goto code_r0x0801235c;
    unaff_r7 = (uint)_DAT_20008350;
    unaff_r6 = param_3 - _DAT_20008352;
    unaff_r5 = (uint)_DAT_20008354;
    param_6 = _DAT_20008358;
    goto code_r0x08012344;
  }
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x55)) &&
       ((0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xa8) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x55)) &&
        (0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xa8) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x55)) &&
       ((0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0xa8) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x6e)) &&
        (0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xda) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x6e)) &&
       ((0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xda) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x6e)) &&
        (0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0xda) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x87)) &&
       ((0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x10c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x87)) &&
        (0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x10c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x87)) &&
       ((0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x10c) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa0)) &&
        (0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13e) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa0)) &&
       ((0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13e) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa0)) &&
        (0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x13e) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xb9)) &&
       ((0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xb9)) &&
        (0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x170) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xb9)) &&
       ((0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x170) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xd2)) &&
        (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xd2)) &&
       ((0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1a2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xd2)) &&
        (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x1a2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xeb)) &&
       ((0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xeb)) &&
        (0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1d4) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xeb)) &&
       ((0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x1d4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
        (0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x206) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
       ((0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x206) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
        (0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x206) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x11d)) &&
       ((0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x238) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x11d)) &&
        (0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x238) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x11d)) &&
       ((0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x238) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
        (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
       ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
        (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
       ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
       ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
        (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
       ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
        (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
        (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
       ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
        (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
       ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0xc;
  while( true ) {
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
       ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -6) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (uVar8 == 0x138) break;
    if ((((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
        (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -4) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
       ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
       -2) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 <= uVar8)) &&
        (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 +
       (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    uVar8 = uVar8 + 4;
  }
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x9e)) &&
        (0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x9e)) &&
       ((0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13a) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x9e)) &&
        (0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x13a) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0x9f)) &&
       ((0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0x9f)) &&
        (0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13c) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0x9f)) &&
       ((0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x13c) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa1)) &&
        (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 2 && (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa1)) &&
       ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 1 &&
         (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x140) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa1)) &&
        (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x140) = 0x3a29;
    }
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar16 = 0;
  uVar8 = 0x16;
  do {
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
       ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        (((uint)_DAT_20008352 <= uVar8 - 2 &&
         (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if ((((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
        (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
       (((uint)_DAT_20008352 <= uVar8 - 1 && (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)
        ))) {
      *(undefined2 *)
       (_DAT_20008358 +
        ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x142) =
           0x3a29;
    }
    uVar16 = uVar16 + 1;
    if (4 < uVar16) {
      uVar16 = 0;
    }
    if (((uVar16 == 0) && (_DAT_20008350 < 0xa2)) &&
       ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
        ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
      *(undefined2 *)
       (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2 +
       0x142) = 0x3a29;
    }
    pbVar13 = _DAT_20000138;
    bVar6 = DAT_20000136;
    uVar16 = uVar16 + 1;
    uVar8 = uVar8 + 3;
    if (4 < uVar16) {
      uVar16 = 0;
    }
  } while (uVar8 != 0xdf);
  uVar8 = (uint)*(byte *)(unaff_r8 + 0x3c);
  if ((uVar8 != 0) && (DAT_20000126 != 0)) {
    if (uVar8 == 3) {
      if (_DAT_20000138 != (byte *)0x0) {
        if (DAT_2000012c == '\0') {
          uVar8 = (uint)DAT_2000010d;
          if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
            uVar17 = (uint)(DAT_2000010d >> 4);
            uVar23 = (uint)DAT_2000105b;
            do {
              if ((*(int *)(pbVar13 + uVar17 * 4 + 4) != 0) && (*(short *)(pbVar13 + 0xe) != 0)) {
                uVar24 = (uint)*pbVar13;
                uVar1 = *(ushort *)(uVar23 * 4 + 0x80bb40c + uVar17 * 2);
                uVar16 = *(ushort *)(pbVar13 + 0xc);
                iVar12 = *(int *)(pbVar13 + uVar17 * 4 + 4);
                uVar1 = (ushort)(uint)((ulonglong)(uVar24 * (((uint)uVar1 << 0x15) >> 0x1a)) *
                                       0x80808081 >> 0x22) & 0xffe0 |
                        (ushort)((uVar24 * (uVar1 & 0x1f)) / 0xff) |
                        (ushort)(((uint)((ulonglong)(uVar24 * (uVar1 >> 0xb)) * 0x80808081 >> 0x20)
                                 & 0xfffff80) << 4);
                do {
                  iVar26 = (short)uVar16 * 0x1a;
                  iVar19 = (int)(short)(uVar16 + 9);
                  iVar22 = 0xc6;
                  uVar24 = 2;
                  while( true ) {
                    if (((((*(byte *)(iVar12 + iVar26 + ((uVar24 - 2) * 0x10000 >> 0x13)) >>
                            (uVar24 - 2 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x16U &&
                        (iVar22 + 0x16U < (uint)_DAT_20008356 + (uint)_DAT_20008352)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x16) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if ((((*(byte *)(iVar12 + ((uVar24 - 1) * 0x10000 >> 0x13) + iVar26) >>
                           (uVar24 - 1 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                       ((iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
                        (((uint)_DAT_20008352 <= iVar22 + 0x15U &&
                         (iVar22 + 0x15U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x15) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (((((*(byte *)(iVar12 + ((uVar24 << 0x10) >> 0x13) + iVar26) >> (uVar24 & 7)
                           & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x14U &&
                        (iVar22 + 0x14U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x14) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (iVar22 == 0) break;
                    iVar22 = iVar22 + -3;
                    uVar24 = uVar24 + 3;
                  }
                  uVar16 = uVar16 + 1;
                } while ((uint)uVar16 <
                         (uint)*(ushort *)(pbVar13 + 0xc) + (uint)*(ushort *)(pbVar13 + 0xe));
              }
              uVar17 = uVar17 + 1;
            } while (uVar17 != (uVar8 & 0xf));
          }
        }
        else {
          iVar12 = *(int *)(_DAT_20000138 + 4);
          if (iVar12 != 0) {
            uVar8 = ((uint)*_DAT_20000138 * 0x1f) / 0xff;
            uVar16 = (ushort)uVar8 | (ushort)(uVar8 << 0xb);
            iVar26 = 0;
            do {
              iVar22 = iVar26 * 0x1a;
              uVar8 = iVar26 * 0x10000 + 0x3b0000 >> 0x10;
              iVar19 = 0xc9;
              uVar23 = 2;
              do {
                if ((((*(byte *)(iVar12 + ((uVar23 - 2) * 0x1000000 >> 0x1b) + iVar22) >>
                       (uVar23 - 2 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x13U &&
                     (iVar19 + 0x13U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x13) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                if (((((*(byte *)(iVar12 + ((uVar23 - 1) * 0x1000000 >> 0x1b) + iVar22) >>
                        (uVar23 - 1 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                    (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
                   (((uint)_DAT_20008352 <= iVar19 + 0x12U &&
                    (iVar19 + 0x12U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x12) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                if ((((*(byte *)(iVar12 + ((uVar23 << 0x18) >> 0x1b) + iVar22) >> (uVar23 & 7) & 1)
                      != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x11U &&
                     (iVar19 + 0x11U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x11) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar16;
                }
                iVar19 = iVar19 + -3;
                uVar23 = uVar23 + 3;
              } while (iVar19 != 0);
              iVar26 = iVar26 + 1;
            } while (iVar26 != 0xc9);
          }
        }
      }
    }
    else {
      DAT_20000136 = DAT_20000135;
      bVar5 = DAT_20000135;
      if (DAT_20000135 <= bVar6) {
        bVar5 = DAT_20000135 + 100;
      }
      DAT_20000137 = bVar5 - bVar6;
      if (_DAT_20000138 != (byte *)0x0) {
        pbVar13 = &DAT_20000138;
        uVar36 = (ulonglong)DAT_08014350 >> 0x20;
        uVar20 = (undefined4)DAT_08014350;
        pbVar27 = _DAT_20000138;
        while( true ) {
          cVar4 = DAT_20000137;
          uVar37 = FUN_0803e5da(*pbVar27);
          uVar30 = (undefined4)((ulonglong)uVar37 >> 0x20);
          uVar38 = FUN_0803e5da(cVar4);
          uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),uVar20,(int)uVar36);
          uVar34 = FUN_0803e50a(3 - uVar8);
          uVar38 = FUN_0803e77c((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),(int)uVar34,
                                (int)((ulonglong)uVar34 >> 0x20));
          uVar33 = (undefined4)((ulonglong)uVar38 >> 0x20);
          iVar12 = FUN_0803edf0((int)uVar38,uVar33,(int)uVar37,uVar30);
          if (iVar12 != 0) break;
          FUN_0803eb94((int)uVar37,uVar30,(int)uVar38,uVar33);
          bVar6 = FUN_0803e450();
          cVar4 = DAT_2000012c;
          *pbVar27 = bVar6;
          if (cVar4 == '\0') {
            uVar8 = (uint)DAT_2000010d;
            if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
              uVar23 = (uint)(DAT_2000010d >> 4);
              do {
                if (((*(int *)(_DAT_20000138 + uVar23 * 4 + 4) != 0) &&
                    (1 < *(ushort *)(pbVar27 + 0xe))) && (1 < *(ushort *)(pbVar27 + 0xe))) {
                  uVar8 = 0;
                  uVar16 = 0;
                  do {
                    FUN_08018ea8((int)(short)(*(short *)(pbVar27 + 0xc) + uVar16 + 9),
                                 0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + uVar23 * 4 + 4) + uVar8),
                                 (int)(short)(*(short *)(pbVar27 + 0xc) + uVar16 + 10),
                                 0xf8 - (uint)*(byte *)(uVar8 + *(int *)(pbVar27 + uVar23 * 4 + 4) +
                                                       1));
                    pbVar27 = *(byte **)pbVar13;
                    uVar16 = uVar16 + 1;
                    uVar8 = (uint)uVar16;
                  } while ((int)uVar8 < (int)(*(ushort *)(pbVar27 + 0xe) - 1));
                  uVar8 = (uint)DAT_2000010d;
                }
                uVar23 = uVar23 + 1;
              } while (uVar23 < (uVar8 & 0xf));
            }
          }
          else if (((*(int *)(_DAT_20000138 + 4) != 0) && (*(int *)(_DAT_20000138 + 8) != 0)) &&
                  ((1 < *(ushort *)(pbVar27 + 0xe) &&
                   (uVar23 = (uint)*(ushort *)(pbVar27 + 0xc), uVar8 = uVar23,
                   (int)uVar23 < (int)(*(ushort *)(pbVar27 + 0xe) + uVar23 + -1))))) {
            do {
              FUN_08018da0(*(byte *)(*(int *)(pbVar27 + 4) + uVar23) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + 8) + uVar23),
                           *(byte *)(*(int *)(pbVar27 + 4) + uVar23 + 1) + 0x1f,
                           0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + 8) + uVar23 + 1));
              pbVar27 = *(byte **)pbVar13;
              uVar23 = uVar8 + 1 & 0xffff;
              uVar8 = uVar8 + 1;
            } while ((int)uVar23 <
                     (int)((uint)*(ushort *)(pbVar27 + 0xc) + (uint)*(ushort *)(pbVar27 + 0xe) + -1)
                    );
          }
          pbVar13 = pbVar27 + 0x10;
          pbVar27 = *(byte **)pbVar13;
          if (pbVar27 == (byte *)0x0) goto LAB_080144ca;
LAB_08014000:
          uVar8 = (uint)DAT_20000134;
        }
        if (*(int *)(pbVar27 + 4) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar8 = *(int *)(pbVar27 + 4) - _DAT_20001078;
            if (uVar8 >> 10 < 0xaf) {
              uVar8 = uVar8 >> 5;
              uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
              if (uVar23 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
              }
            }
          }
        }
        if (*(int *)(*(int *)pbVar13 + 8) != 0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else {
            uVar8 = *(int *)(*(int *)pbVar13 + 8) - _DAT_20001078;
            if (uVar8 >> 10 < 0xaf) {
              uVar8 = uVar8 >> 5;
              uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
              if (uVar23 != 0) {
                FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
              }
            }
          }
        }
        pbVar27 = *(byte **)(*(int *)pbVar13 + 0x10);
        if (_DAT_20000138 != (byte *)0x0) {
          if (DAT_20001080 == '\0') {
            (*_DAT_20001070)(0);
          }
          else if ((uint)((int)_DAT_20000138 - _DAT_20001078) >> 10 < 0xaf) {
            uVar8 = (uint)((int)_DAT_20000138 - _DAT_20001078) >> 5;
            uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
            if (uVar23 != 0) {
              FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
            }
          }
        }
        pbVar13 = &DAT_20000138;
        _DAT_20000138 = pbVar27;
        if (pbVar27 != (byte *)0x0) goto LAB_08014000;
      }
    }
  }
LAB_080144ca:
  fVar3 = DAT_08014948;
  uVar8 = (uint)_DAT_20000eae;
  if (uVar8 == 0) {
LAB_08014800:
    if (DAT_2000012c == '\0') {
      pbVar13 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar13 = &DAT_20000eb7;
      }
      bVar6 = *pbVar13;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar8 = (uint)(bVar6 >> 4);
        sVar7 = (ushort)(bVar6 >> 4) * 0xad + 9;
        do {
          if (*(char *)(uVar8 + 0x20000102) != '\0') {
            piVar9 = (int *)(uVar8 * 4 + 0x20000104);
            iVar12 = *piVar9;
            if (iVar12 != 0) {
              for (iVar26 = 0; iVar19 = (int)(short)(sVar7 + (short)iVar26),
                  FUN_08018ea8(iVar19,0x46,iVar19,0x46 - (uint)*(byte *)(iVar12 + iVar26)),
                  iVar26 != 0x7f; iVar26 = iVar26 + 1) {
                iVar12 = *piVar9;
              }
            }
          }
          uVar8 = uVar8 + 1;
          sVar7 = sVar7 + 0xad;
        } while (uVar8 != (bVar6 & 0xf));
        if (DAT_2000012c != '\0') goto LAB_080148ec;
      }
      if (_DAT_20000112 < 0x92) {
        if (_DAT_20000112 < -0x91) {
          iVar12 = 0x10;
          iVar26 = 0x10;
          uVar20 = 0x1e;
        }
        else {
          iVar12 = (int)(short)(_DAT_20000112 + 0x9a);
          iVar26 = (int)(short)(_DAT_20000112 + 0xa4);
          uVar20 = 0x14;
        }
      }
      else {
        iVar12 = 0x12e;
        iVar26 = 0x12e;
        uVar20 = 0x1e;
      }
      FUN_08019af8(iVar12,0x14,iVar26,uVar20);
    }
  }
  else {
    if (DAT_2000012c == '\0') {
      pbVar13 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar13 = &DAT_20000eb7;
      }
      bVar6 = *pbVar13;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar23 = (uint)(bVar6 >> 4);
        do {
          if (1 < uVar8) {
            if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))
               ) {
              uVar17 = 0;
              while( true ) {
                uVar21 = _DAT_20000f04;
                uVar24 = _DAT_20000f00;
                uVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                                      _DAT_20000f04 * uVar8 +
                                      (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),
                                      1000,0);
                uVar25 = (uint)DAT_20000efc;
                iVar12 = *(int *)(uVar23 * 4 + 0x20000ef4);
                uVar28 = uVar25;
                if (iVar12 == 0) {
                  uVar28 = 1;
                }
                lVar35 = (ulonglong)uVar28 * (uVar36 & 0xffffffff);
                uVar10 = (uint)lVar35;
                uVar15 = uVar28 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
                uVar18 = 0x12d - _DAT_20000ed8;
                uVar28 = (int)uVar18 >> 0x1f;
                if (uVar28 < uVar15 || uVar15 - uVar28 < (uint)(uVar18 <= uVar10)) {
                  uVar10 = uVar18;
                  uVar15 = uVar28;
                }
                if (-(uVar15 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= (uVar17 & 0xffff))) break;
                fVar31 = (float)VectorUnsignedToFloat(uVar17 & 0xffff,(byte)(in_fpscr >> 0x15) & 3);
                fVar11 = (float)FUN_0803ee1a(uVar24,uVar21);
                fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
                if (iVar12 == 0) {
                  fVar29 = 1.0;
                }
                iVar12 = uVar23 * 0x12d + 0x200000f8 + (uVar17 & 0xffff);
                fVar32 = (float)VectorUnsignedToFloat
                                          ((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
                fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
                FUN_08018ea8((int)(fVar32 + 9.0),0xf8 - (uint)*(byte *)(iVar12 + 0x356),
                             (int)(fVar32 + 10.0),0xf8 - (uint)*(byte *)(iVar12 + 0x357));
                uVar8 = (uint)_DAT_20000eae;
                uVar17 = uVar17 + 1;
              }
            }
            else {
              uVar24 = (uint)_DAT_20000eac;
              uVar17 = uVar24;
              if ((int)uVar24 < (int)(uVar24 + uVar8 + -1)) {
                do {
                  iVar12 = uVar24 + uVar23 * 0x12d + 0x200000f8;
                  FUN_08018ea8((int)(short)((short)uVar17 + 9),
                               0xf8 - (uint)*(byte *)(iVar12 + 0x356),
                               (int)(short)((short)uVar17 + 10),
                               0xf8 - (uint)*(byte *)(iVar12 + 0x357));
                  uVar8 = (uint)_DAT_20000eae;
                  uVar24 = uVar17 + 1 & 0xffff;
                  uVar17 = uVar17 + 1;
                } while ((int)uVar24 < (int)(_DAT_20000eac + uVar8 + -1));
              }
            }
          }
          uVar23 = uVar23 + 1;
        } while (uVar23 != (bVar6 & 0xf));
      }
      goto LAB_08014800;
    }
    if (1 < uVar8) {
      if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
        uVar23 = 0;
        while( true ) {
          uVar24 = uVar23 & 0xffff;
          lVar35 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                                _DAT_20000f04 * uVar8 +
                                (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),1000,0);
          uVar8 = (uint)((ulonglong)lVar35 >> 0x20);
          uVar17 = 0x12d - _DAT_20000ed8;
          if ((uint)((int)uVar17 >> 0x1f) < uVar8 ||
              uVar8 - ((int)uVar17 >> 0x1f) < (uint)(uVar17 <= (uint)lVar35)) {
            lVar35 = (longlong)(int)uVar17;
          }
          if (-((int)((ulonglong)lVar35 >> 0x20) - (uint)((int)lVar35 == 0)) <
              (uint)((int)lVar35 - 1U <= uVar24)) break;
          FUN_08018da0(*(byte *)(uVar24 + 0x2000044e) + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057b)[uVar24],
                       (byte)(&DAT_2000044f)[uVar23 & 0xffff] + 0x1f,
                       0xf8 - (uint)(byte)(&DAT_2000057c)[uVar23 & 0xffff]);
          uVar8 = (uint)_DAT_20000eae;
          uVar23 = uVar23 + 1;
        }
      }
      else {
        uVar17 = (uint)_DAT_20000eac;
        uVar23 = uVar17;
        if ((int)uVar17 < (int)(uVar17 + uVar8 + -1)) {
          do {
            FUN_08018da0(*(byte *)(uVar17 + 0x2000044e) + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057b)[uVar17],
                         (byte)(&DAT_2000044f)[uVar17] + 0x1f,
                         0xf8 - (uint)(byte)(&DAT_2000057c)[uVar17]);
            uVar17 = uVar23 + 1 & 0xffff;
            uVar23 = uVar23 + 1;
          } while ((int)uVar17 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
        }
      }
      goto LAB_08014800;
    }
  }
LAB_080148ec:
  fVar11 = DAT_08014d00;
  fVar3 = DAT_08014948;
  if ((((_DAT_20000144 == 0) || (DAT_2000012c != '\0')) || (uVar8 = (uint)DAT_2000010c, uVar8 == 0))
     || (DAT_20000140 != '\0')) {
LAB_08014c4c:
    if (DAT_2000012c == '\0') goto LAB_08014c52;
  }
  else {
    uVar17 = ~(uVar8 << 4) & 0x10;
    uVar23 = 0;
    bVar6 = 0;
    if ((int)(uVar8 << 0x1e) < 0) {
      do {
        if ((uVar23 & 0xff) == 0) {
          uVar23 = uVar17;
        }
        uVar8 = uVar23 & 0xff;
        if ((_DAT_20000144 >> uVar8 & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (0x1e < uVar8) break;
        if ((_DAT_20000144 >> (uVar23 + 1 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (uVar8 == 0x1e) break;
        if ((_DAT_20000144 >> (uVar23 + 2 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        if (0x1c < uVar8) break;
        if ((_DAT_20000144 >> (uVar23 + 3 & 0xff) & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        uVar23 = uVar23 + 4;
      } while (uVar8 != 0x1c);
    }
    else {
      do {
        if ((uVar23 & 0xff) == 0) {
          uVar23 = uVar17;
        }
        uVar8 = uVar23 & 0xff;
        if (uVar8 == 0x10) break;
        if ((_DAT_20000144 >> uVar8 & 1) != 0) {
          bVar6 = bVar6 + 1;
        }
        uVar23 = uVar23 + 1;
      } while (uVar8 < 0x1f);
    }
    if (bVar6 != 0) {
      bVar2 = DAT_20001058 == '\x01';
      iVar12 = ((int)(bVar6 - 1) / 3) * 0xfff1 + 0xcc;
      uVar8 = (uint)bVar6 % 3;
      uVar23 = 0;
      sVar7 = 0xc;
      do {
        uVar24 = uVar17 + 1;
        if (uVar8 == (uVar23 & 0xff) && uVar8 != 0) {
          sVar7 = 0xc;
          iVar12 = iVar12 + 0xf;
        }
        if ((uVar24 & 0xff) < 0x21) {
          uVar24 = 0x20;
        }
        do {
          uVar21 = uVar17;
          if ((_DAT_20000144 >> (uVar17 & 0xff) & 1) != 0) {
LAB_08014a8a:
            uVar17 = uVar21 + 1;
            if (0xf < (uVar21 & 0xff)) goto LAB_08014a96;
            piVar9 = (int *)(&DAT_20000208 + (uVar21 & 0xff) * 4);
            goto LAB_08014ab2;
          }
          uVar21 = uVar17 + 1;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar21 = uVar17 + 2;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar21 = uVar17 + 3;
          if (0x1f < (uVar21 & 0xff)) break;
          if ((_DAT_20000144 >> (uVar21 & 0xff) & 1) != 0) goto LAB_08014a8a;
          uVar17 = uVar17 + 4;
        } while ((uVar17 & 0xff) < 0x20);
        uVar17 = uVar24 + 1;
        uVar21 = uVar24;
LAB_08014a96:
        uVar21 = uVar21 - 0x10;
        piVar9 = (int *)(&DAT_20000238 + (uVar21 & 0xff) * 4);
LAB_08014ab2:
        iVar19 = *piVar9;
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        iVar26 = iVar19;
        if (iVar19 < 1) {
          iVar26 = SignedSaturate(-iVar19,0x20);
          SignedDoesSaturate(iVar26,0x20);
        }
        if (999999 < iVar26) {
          iVar19 = iVar19 / 1000;
        }
        fVar29 = (float)VectorSignedToFloat(iVar19,(byte)(in_fpscr >> 0x15) & 3);
        if ((uVar21 & 0xfe) == 2) {
          fVar29 = fVar29 / fVar11;
        }
        if ((uVar21 & 0xff) - 6 < 6) {
          fVar29 = fVar29 / fVar3;
        }
        fVar31 = ABS(fVar29);
        uVar24 = in_fpscr & 0xfffffff | (uint)(fVar31 < fVar3) << 0x1f;
        uVar28 = uVar24 | (uint)(NAN(fVar31) || NAN(fVar3)) << 0x1c;
        if ((byte)(uVar24 >> 0x1f) == ((byte)(uVar28 >> 0x1c) & 1)) {
          fVar31 = fVar31 / fVar3;
          fVar29 = fVar29 / fVar3;
        }
        uVar20 = *(undefined4 *)
                  (&DAT_0804bf28 + (uVar21 & 0xff) * 4 + ((uint)bVar2 | (uint)bVar2 << 1) * 0x10);
        FUN_0803ed70(fVar29);
        uVar24 = uVar28 & 0xfffffff | (uint)(fVar31 < fVar11) << 0x1f;
        in_fpscr = uVar24 | (uint)(NAN(fVar31) || NAN(fVar11)) << 0x1c;
        pcVar14 = s__s__1f_s_08015a70;
        if ((byte)(uVar24 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1)) {
          pcVar14 = s__s__2f_s_08015a64;
        }
        FUN_080003b4(&DAT_20008360,pcVar14,uVar20);
        FUN_08008154((int)sVar7,(int)(short)iVar12,100,0xf);
        uVar23 = uVar23 + 1;
        if ((uint)bVar6 <= (uVar23 & 0xff)) goto LAB_08014c4c;
        sVar7 = sVar7 + 100;
        if (0x137 < sVar7) {
          iVar12 = iVar12 + 0xf;
          sVar7 = 0xc;
        }
      } while( true );
    }
LAB_08014c52:
    if ((DAT_20000332 & 1) != 0) {
      FUN_08018ea8((int)(short)(_DAT_2000032a + 0x9f),0x14,(int)(short)(_DAT_2000032a + 0x9f),0xdc);
      FUN_08018ea8((int)(short)(_DAT_2000032c + 0x9f),0x14,(int)(short)(_DAT_2000032c + 0x9f),0xdc);
    }
    uVar8 = (uint)DAT_20000332;
    if ((int)(uVar8 << 0x1e) < 0) {
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_2000032e),0x135,(int)(short)(0x78 - _DAT_2000032e));
      FUN_08018da0(9,(int)(short)(0x78 - _DAT_20000330),0x135,(int)(short)(0x78 - _DAT_20000330));
      uVar8 = (uint)DAT_20000332;
    }
    if (uVar8 != 0) {
      sVar7 = 0x19;
      bVar6 = 0;
      if ((uVar8 & 1) != 0) goto LAB_08014d06;
      do {
        if (bVar6 == 0) {
          bVar6 = 3;
        }
        else if (bVar6 == 6) break;
LAB_08014d06:
        do {
          bVar5 = bVar6;
          if (bVar6 == 3) {
            bVar5 = 6;
          }
          if ((int)(uVar8 << 0x1e) < 0) {
            bVar5 = bVar6;
          }
          FUN_08008154(0xe,(int)sVar7,0x32,0x10);
          uVar37 = FUN_0803ed70(*(undefined4 *)(&DAT_20000334 + (uint)bVar5 * 4));
          FUN_080003b4(&DAT_20008360,0x80bcc11,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
          FUN_08008154(0x40,(int)sVar7,100,0x10);
          if (5 < bVar5) goto LAB_08014dbe;
          uVar8 = (uint)DAT_20000332;
          bVar6 = bVar5 + 1;
          sVar7 = sVar7 + 0xc;
        } while ((DAT_20000332 & 1) != 0);
      } while( true );
    }
  }
LAB_08014dbe:
  fVar3 = DAT_08014fd0;
  if (((DAT_2000012d != '\0') && (_DAT_20000130 != 0)) &&
     ((DAT_2000012c == '\0' && (uVar8 = (uint)_DAT_20000eae, 1 < uVar8)))) {
    if ((DAT_20000126 == 0) && (_DAT_20000f04 != 0 || _DAT_20000f04 < (999 < _DAT_20000f00))) {
      uVar17 = (uint)(DAT_20000eb7 >> 4);
      uVar24 = DAT_20000eb7 & 0xf;
      uVar23 = 0;
      do {
        uVar21 = uVar23;
        if (((uVar24 <= uVar17) || (uVar21 = uVar17, uVar24 <= uVar17 + 1)) ||
           (uVar28 = uVar17 + 2, uVar21 = uVar17 + 1, uVar24 <= uVar28)) break;
        uVar23 = uVar17 + 3;
        uVar17 = uVar17 + 4;
        uVar21 = uVar28;
      } while (uVar23 < uVar24);
      uVar23 = 0;
      while( true ) {
        uVar24 = _DAT_20000f04;
        uVar17 = _DAT_20000f00;
        uVar28 = uVar23 & 0xffff;
        uVar36 = FUN_08001642((int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8),
                              _DAT_20000f04 * uVar8 +
                              (int)((ulonglong)_DAT_20000f00 * (ulonglong)uVar8 >> 0x20),1000,0);
        uVar25 = (uint)DAT_20000efc;
        iVar12 = *(int *)(uVar21 * 4 + 0x20000ef4);
        uVar8 = uVar25;
        if (iVar12 == 0) {
          uVar8 = 1;
        }
        lVar35 = (ulonglong)uVar8 * (uVar36 & 0xffffffff);
        uVar10 = (uint)lVar35;
        uVar15 = uVar8 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
        uVar18 = 0x12d - _DAT_20000ed8;
        uVar8 = (int)uVar18 >> 0x1f;
        if (uVar8 < uVar15 || uVar15 - uVar8 < (uint)(uVar18 <= uVar10)) {
          uVar10 = uVar18;
          uVar15 = uVar8;
        }
        if (-(uVar15 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= uVar28)) break;
        fVar31 = (float)VectorUnsignedToFloat(uVar28,(byte)(in_fpscr >> 0x15) & 3);
        fVar11 = (float)FUN_0803ee1a(uVar17,uVar24);
        fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
        if (iVar12 == 0) {
          fVar29 = 1.0;
        }
        fVar32 = (float)VectorUnsignedToFloat((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
        fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
        FUN_08018ea8((int)(fVar32 + 9.0),0x78 - *(char *)(_DAT_20000130 + uVar28),
                     (int)(fVar32 + 10.0),0x78 - *(char *)(_DAT_20000130 + (uVar23 & 0xffff) + 1));
        uVar8 = (uint)_DAT_20000eae;
        uVar23 = uVar23 + 1;
      }
    }
    else {
      uVar17 = (uint)_DAT_20000eac;
      uVar23 = uVar17;
      if ((int)uVar17 < (int)(uVar8 + uVar17 + -1)) {
        do {
          FUN_08018ea8((int)(short)((short)uVar23 + 9),0x78 - *(char *)(_DAT_20000130 + uVar17),
                       (int)(short)((short)uVar23 + 10),0x78 - *(char *)(uVar17 + _DAT_20000130 + 1)
                      );
          uVar23 = uVar23 + 1;
          uVar17 = uVar23 & 0xffff;
        } while ((int)uVar17 < (int)((uint)_DAT_20000eac + (uint)_DAT_20000eae + -1));
      }
    }
  }
  if ((_DAT_20000122 != 0) && (DAT_2000012c == '\0')) {
    uVar16 = 0;
    uVar8 = _DAT_20000114 + 100 & 0xffff;
    iVar12 = 0xdc - uVar8;
    if (uVar8 < 0xc9) {
      uVar8 = 200;
    }
    else {
      iVar12 = 0x14;
    }
    iVar26 = (int)(short)iVar12;
    iVar19 = (int)(short)(((short)uVar8 - _DAT_20000114) + -0x50);
    uVar8 = 0xc;
    while( true ) {
      if (((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((iVar19 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar8) * 2
         + -6) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -4) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (((uVar16 < 3) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -2) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if ((((uVar16 < 3) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2)
             = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar8 = (byte)(&DAT_200000fa)[DAT_2000010e] / 3;
    uVar33 = VectorSignedToFloat((int)(short)_DAT_20000114 -
                                 (int)(char)(&DAT_200000fc)[DAT_2000010e],
                                 (byte)(in_fpscr >> 0x15) & 3);
    uVar37 = FUN_0803e5da(*(undefined2 *)
                           (&DAT_0804bfb8 +
                           (uVar8 * -3 + (uint)(byte)(&DAT_200000fa)[DAT_2000010e] & 0xff) * 2));
    uVar20 = FUN_0803e5da(uVar8);
    uVar30 = (undefined4)DAT_08015328;
    uVar20 = FUN_0803c8e0(uVar30,param_2,uVar20);
    uVar37 = FUN_0803e77c(uVar20,extraout_s1,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    uVar20 = FUN_0803e5da(*(undefined1 *)(DAT_2000010e + 0x200000fe));
    uVar20 = FUN_0803c8e0(uVar30,extraout_s1,uVar20);
    uVar37 = FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar20,extraout_s1_00);
    uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)DAT_08015330,
                          (int)((ulonglong)DAT_08015330 >> 0x20));
    uVar38 = FUN_0803ed70(uVar33);
    FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                 (int)((ulonglong)uVar38 >> 0x20));
    fVar11 = (float)FUN_0803df48();
    pcVar14 = (char *)0x8015aa8;
    fVar3 = fVar11 / DAT_08014fd0;
    if (ABS(fVar11) < DAT_08014fd0) {
      pcVar14 = s___2fmV_08015aa0;
      fVar3 = fVar11;
    }
    uVar37 = FUN_0803ed70(fVar3);
    FUN_080003b4(&DAT_20008360,pcVar14,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    sVar7 = (short)iVar12 + -10;
    if (0xd6 < iVar12) {
      sVar7 = 0xca;
    }
    if (iVar12 < 0x1e) {
      sVar7 = 0x14;
    }
    FUN_08008154(0xa0,(int)sVar7,0x7d,0x14);
  }
  if ((_DAT_20000120 != 0) && (DAT_2000012c == '\0')) {
    iVar12 = (int)(short)(_DAT_20000112 + 0x9f);
    uVar16 = 0;
    uVar8 = 0x1f;
    while( true ) {
      if ((((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
          (iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 3 &&
          (uVar8 - 3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -3) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (uVar8 == 0xdf) break;
      if (((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
         ((iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -2) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if ((((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
          (iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((~(uint)_DAT_20008352 + uVar8) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      if (((uVar16 < 3) && ((int)(uint)_DAT_20008350 <= iVar12)) &&
         ((iVar12 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((uVar8 - _DAT_20008352) * (uint)_DAT_20008354 + (iVar12 - (uint)_DAT_20008350)) * 2) =
             0x7e2;
      }
      uVar16 = uVar16 + 1;
      if (4 < uVar16) {
        uVar16 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar37 = FUN_0803e538(_DAT_20000118,_DAT_2000011c);
    uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)DAT_08015338,
                          (int)((ulonglong)DAT_08015338 >> 0x20));
    uVar36 = (ulonglong)DAT_08015340 >> 0x20;
    uVar20 = (undefined4)((ulonglong)DAT_08015330 >> 0x20);
    uVar30 = (undefined4)DAT_08015330;
    while( true ) {
      uStack00000034 = (uint)((ulonglong)uVar37 >> 0x20);
      uVar33 = (undefined4)uVar37;
      iVar12 = FUN_0803ee0c(uVar33,uStack00000034 & 0x7fffffff | (uint)uVar36 & 0x80000000,uVar30,
                            uVar20);
      if (iVar12 != 0) break;
      uVar37 = FUN_0803e124(uVar33,uStack00000034,uVar30,uVar20);
    }
    uStack00000034 = uStack00000034 ^ 0x80000000;
    FUN_080003b4(&DAT_20008360,0x80bcc11,uVar33,uStack00000034);
    FUN_08008154(0,0xb4,0x140,0x14);
  }
  if ((DAT_2000012c != '\0') && (FUN_08021b40(), DAT_2000012c == '\0')) {
    sVar7 = 0x78 - _DAT_20000114;
    if (200 < (ushort)(_DAT_20000114 + 100)) {
      sVar7 = 0x14;
    }
    FUN_08019af8(0x136,(int)sVar7,0x13d,(int)(short)(sVar7 + 5));
  }
  if (((((DAT_20000f08 != -1) && (DAT_20000f08 != '\0')) ||
       ((DAT_20000127 != '\0' || DAT_20000128 != '\0') || DAT_2000012b != '\0')) ||
      ((uVar8 = (uint)DAT_20000126, DAT_2000010f == '\x02' && (uVar8 != 0)))) ||
     ((uVar8 == 0 &&
      (uVar8 = -(uint)(_DAT_20000f00 >= 10) - _DAT_20000f04,
      -_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))))) {
    FUN_08019af8(0xea,0x51,0x45,0x51);
    FUN_08019af8(0x45,0x51,0xef,0x55);
    FUN_08019af8(0xef,0x55,0x3e,0x59);
    FUN_08019af8(0x3e,0x59,0x101,0x55);
    FUN_08019af8(0x101,0x55,0x3e,0xa1);
    FUN_08019af8(0x3e,0xa1,0x101,0x98);
    FUN_08018fcc(0x3d,0x50,0xc6,0x53);
    uVar8 = extraout_r1;
  }
  if ((DAT_20000f08 == '\x03') || (DAT_20000f08 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (DAT_20000f08 == '\x01') {
    DAT_20000f13 = FUN_08034878(0x80bc18b,uVar8);
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,DAT_20000f09);
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    DAT_20000f08 = -1;
    DAT_20001060 = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
  if ((DAT_2000010f == '\x02') && (DAT_20000126 != 0)) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (DAT_20000127 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_20000128 != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((DAT_20000126 == 0) && (-_DAT_20000f04 < (uint)(_DAT_20000f00 < 10))) {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (DAT_2000012b != '\0') {
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  DAT_20001068 = DAT_20001068 + '\x01';
  return;
}



