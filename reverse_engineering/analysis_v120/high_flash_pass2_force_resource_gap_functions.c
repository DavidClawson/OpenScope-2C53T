// Force-decompiled functions

// Attempting to create function at 0x0800ae84
// Successfully created function
// ==========================================
// Function: FUN_0800ae84 @ 0800ae84
// Size: 1 bytes
// ==========================================

/* WARNING: Type propagation algorithm not settling */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800ae84(void)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  undefined4 *puVar4;
  uint uVar5;
  undefined1 *puVar6;
  bool bVar7;
  
  _DAT_20008350 = 0x3d;
  _DAT_20008352 = 0x50;
  _DAT_20008354 = 0xc6;
  _DAT_20008356 = 0x53;
  iVar1 = FUN_08033cfc(0x8064);
  _DAT_20008358 = iVar1;
  if (iVar1 != 0) {
    uVar5 = (uint)_DAT_20008356 * (uint)_DAT_20008354;
    if (uVar5 != 0) {
      uVar2 = uVar5 & 3;
      uVar3 = 0;
      if (2 < uVar5 - 1) {
        uVar3 = 0;
        puVar4 = (undefined4 *)(iVar1 + -8);
        do {
          uVar3 = uVar3 + 4;
          puVar4[2] = 0;
          puVar4[3] = 0;
          puVar4 = puVar4 + 2;
        } while ((uVar5 & 0x7ffffffc) != uVar3);
      }
      if ((uVar2 != 0) && (*(undefined2 *)(iVar1 + uVar3 * 2) = 0, uVar2 != 1)) {
        iVar1 = iVar1 + uVar3 * 2;
        *(undefined2 *)(iVar1 + 2) = 0;
        if (uVar2 != 2) {
          *(undefined2 *)(iVar1 + 4) = 0;
        }
      }
    }
    FUN_08018fcc(0x3d,0x50,0xc6,0x53,&DAT_0804fd00,0x55f);
    FUN_08008154(0x3f,0x6d,0xc2,0x14,*(undefined4 *)(&DAT_0804c184 + (uint)DAT_20001058 * 2),
                 0x20001144,0x55f,1);
    FUN_0803bee0();
    iVar1 = FUN_0803b3a8(_DAT_20002d84,0xffffffff);
    if ((iVar1 == 1) && (_DAT_20008358 != 0)) {
      if (DAT_20001080 == '\0') {
        (*_DAT_20001070)(0);
      }
      else if ((uint)(_DAT_20008358 - _DAT_20001078) >> 10 < 0xaf) {
        uVar2 = (uint)(_DAT_20008358 - _DAT_20001078) >> 5;
        uVar5 = (uint)*(ushort *)(_DAT_2000107c + uVar2 * 2);
        if (uVar5 != 0) {
          FUN_080012bc(_DAT_2000107c + uVar2 * 2,uVar5 << 1);
        }
      }
    }
  }
  if (DAT_20001060 == '\v') {
    puVar6 = (undefined1 *)0x20000f22;
    iVar1 = -8;
    do {
      uVar5 = (uint)*(byte *)(iVar1 + 0x20000f12);
      if (uVar5 != 0) {
        if ((*(byte *)(iVar1 + 0x20000f12) & 1) != 0) {
          FUN_080003b4(&DAT_20008360,0x80bcad2,puVar6[-5]);
          FUN_0802e12c();
          FUN_080003b4(&DAT_20008360,0x80bc859,puVar6[-5]);
          FUN_0802e12c();
          uVar5 = (uint)*(byte *)(iVar1 + 0x20000f12);
        }
        if ((int)(uVar5 << 0x1e) < 0) {
          FUN_080003b4(&DAT_20008360,0x80bcad2,puVar6[-4]);
          FUN_0802e12c();
          FUN_080003b4(&DAT_20008360,0x80bc859,puVar6[-4]);
          FUN_0802e12c();
          uVar5 = (uint)*(byte *)(iVar1 + 0x20000f12);
        }
        if ((int)(uVar5 << 0x1d) < 0) {
          FUN_080003b4(&DAT_20008360,0x80bcad2,puVar6[-3]);
          FUN_0802e12c();
          FUN_080003b4(&DAT_20008360,0x80bc859,puVar6[-3]);
          FUN_0802e12c();
          uVar5 = (uint)*(byte *)(iVar1 + 0x20000f12);
        }
        if ((int)(uVar5 << 0x1c) < 0) {
          FUN_080003b4(&DAT_20008360,0x80bcad2,puVar6[-2]);
          FUN_0802e12c();
          FUN_080003b4(&DAT_20008360,0x80bc859,puVar6[-2]);
          FUN_0802e12c();
          uVar5 = (uint)*(byte *)(iVar1 + 0x20000f12);
        }
        if ((int)(uVar5 << 0x1b) < 0) {
          FUN_080003b4(&DAT_20008360,0x80bcad2,puVar6[-1]);
          FUN_0802e12c();
          FUN_080003b4(&DAT_20008360,0x80bc859,puVar6[-1]);
          FUN_0802e12c();
          uVar5 = (uint)*(byte *)(iVar1 + 0x20000f12);
        }
        if ((int)(uVar5 << 0x1a) < 0) {
          FUN_080003b4(&DAT_20008360,0x80bcad2,*puVar6);
          FUN_0802e12c();
          FUN_080003b4(&DAT_20008360,0x80bc859,*puVar6);
          FUN_0802e12c();
        }
      }
      bVar7 = iVar1 != -1;
      iVar1 = iVar1 + 1;
      puVar6 = puVar6 + 6;
    } while (bVar7);
  }
  else {
    FUN_080003b4(&DAT_20008360,0x80bcad2,*(undefined1 *)(DAT_20000f14_1 + 0x20000f1d));
    FUN_0802e12c();
    FUN_080003b4(&DAT_20008360,0x80bc859,*(undefined1 *)(DAT_20000f14_1 + 0x20000f1d));
    FUN_0802e12c();
  }
  DAT_20000f13 = FUN_08034878(0x80bc18b);
  uRam20000f12 = 0;
  _DAT_20000f14 = 0;
  DAT_20001060 = 5;
  return;
}



// ==========================================
// Function: FUN_0800f5a8 @ 0800f5a8
// Size: 1 bytes
// ==========================================

/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08015214 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

void FUN_0800f5a8(undefined4 param_1,undefined4 param_2)

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
  ushort uVar12;
  int iVar13;
  byte *pbVar14;
  char *pcVar15;
  uint uVar16;
  uint extraout_r1;
  uint uVar17;
  uint uVar18;
  int iVar19;
  undefined4 uVar20;
  uint uVar21;
  int iVar22;
  uint uVar23;
  uint uVar24;
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
  uint uStack_5c;
  undefined1 uStack_51;
  
  if (DAT_2000012c == '\0') {
    FUN_08018da0(9,0x14,0x135,0x14);
    FUN_08018da0(9,0xdc,0x135,0xdc);
    FUN_08018da0(9,0x14,9,0xdc);
    FUN_08018da0(0x135,0x14,0x135,0xdc);
    FUN_08018da0(9,0x78,0x135,0x78);
    FUN_08018da0(0x9f,0x14,0x9f,0xdc);
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
          (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
          (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
          (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
          (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x23)) &&
         ((0x22 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x44) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x23)) &&
          (0x22 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x44) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x23)) &&
         ((0x22 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x44) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x3c)) &&
          (0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x76) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x3c)) &&
         ((0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x76) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x3c)) &&
          (0x3b < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x76) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x55)) &&
         ((0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xa8) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x55)) &&
          (0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xa8) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x55)) &&
         ((0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0xa8) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x6e)) &&
          (0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xda) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x6e)) &&
         ((0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xda) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x6e)) &&
          (0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0xda) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x87)) &&
         ((0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x10c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x87)) &&
          (0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x10c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x87)) &&
         ((0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x10c) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa0)) &&
          (0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13e)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa0)) &&
         ((0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13e)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa0)) &&
          (0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x13e) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0xb9)) &&
         ((0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x170)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xb9)) &&
          (0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x170)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xb9)) &&
         ((0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x170) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xd2)) &&
          (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1a2)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xd2)) &&
         ((0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1a2)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xd2)) &&
          (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1a2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0xeb)) &&
         ((0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1d4)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xeb)) &&
          (0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1d4)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xeb)) &&
         ((0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1d4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
          (0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x206)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
         ((0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x206)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 >> 2 < 0x41)) &&
          (0x103 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x206) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x11d)) &&
         ((0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x238)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x11d)) &&
          (0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x238)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x11d)) &&
         ((0x11c < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x238) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
          (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
          (uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0xc;
    while( true ) {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -6) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      uVar8 = uVar8 + 4;
    }
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x9e)) &&
          (0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13a)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x9e)) &&
         ((0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13a)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x9e)) &&
          (0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x13a) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x9f)) &&
         ((0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x9f)) &&
          (0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x9f)) &&
         ((0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x13c) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa1)) &&
          (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa1)) &&
         ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x140)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa1)) &&
          (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x140) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa2)) &&
         ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa2)) &&
          (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x142)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa2)) &&
         ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x142) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
  }
  else {
    FUN_08018da0(0x3b,0x14,0x103,0x14);
    FUN_08018da0(0x3b,0xdc,0x103,0xdc);
    FUN_08018da0(0x3b,0x14,0x3b,0xdc);
    FUN_08018da0(0x103,0x14,0x103,0xdc);
    FUN_08018da0(0x3b,0x78,0x103,0x78);
    FUN_08018da0(0x9f,0x14,0x9f,0xdc);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x2e && (0x2d < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x2d - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x47 && (0x46 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x46 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x60 && (0x5f < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x5f - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x79 && (0x78 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x78 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x92 && (0x91 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x91 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xab && (0xaa < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0xaa - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0xc4 && (0xc3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0xc3 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x55)) &&
          (0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xa8) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x55)) &&
         ((0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xa8) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x55)) &&
          (0x54 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0xa8) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x6e)) &&
         ((0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0xda) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x6e)) &&
          (0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0xda) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x6e)) &&
         ((0x6d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0xda) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x87)) &&
          (0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x10c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x87)) &&
         ((0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x10c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x87)) &&
          (0x86 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x10c) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa0)) &&
         ((0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13e)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa0)) &&
          (0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13e)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa0)) &&
         ((0x9f < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x13e) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xb9)) &&
          (0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x170)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xb9)) &&
         ((0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x170)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xb9)) &&
          (0xb8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x170) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0xd2)) &&
         ((0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1a2)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xd2)) &&
          (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1a2)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xd2)) &&
         ((0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1a2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xeb)) &&
          (0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x1d4)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xeb)) &&
         ((0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x1d4)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xeb)) &&
          (0xea < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1d4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0x9e)) &&
         ((0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13a)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x9e)) &&
          (0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13a)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x9e)) &&
         ((0x9d < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x13a) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x9f)) &&
          (0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x13c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0x9f)) &&
         ((0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x13c)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0x9f)) &&
          (0x9e < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x13c) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa1)) &&
         ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x140)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa1)) &&
          (0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x140)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa1)) &&
         ((0xa0 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x140) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x16;
    do {
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa2)) &&
          (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((uint)_DAT_20008352 <= uVar8 - 2 &&
          (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * ((uVar8 - _DAT_20008352) + -2) - (uint)_DAT_20008350) * 2 + 0x142)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 < 0xa2)) &&
         ((0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((uint)_DAT_20008352 <= uVar8 - 1 &&
           (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          ((uint)_DAT_20008354 * (~(uint)_DAT_20008352 + uVar8) - (uint)_DAT_20008350) * 2 + 0x142)
             = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 < 0xa2)) &&
          (0xa1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar8 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x142) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0xdf);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x77 && (0x76 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x76 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x78 && (0x77 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x77 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
         ((uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
          (uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
         ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7a && (0x79 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x79 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
    uVar12 = 0;
    uVar8 = 0x3d;
    do {
      if ((((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -4) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 == 0) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2 +
         -2) = 0x3a29;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 == 0) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 < 0x7b && (0x7a < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (0x7a - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2) =
             0x3a29;
      }
      uVar12 = uVar12 + 1;
      uVar8 = uVar8 + 3;
      if (4 < uVar12) {
        uVar12 = 0;
      }
    } while (uVar8 != 0x106);
  }
  pbVar14 = _DAT_20000138;
  bVar6 = DAT_20000136;
  uVar8 = (uint)DAT_20000134;
  if ((uVar8 != 0) && (DAT_20000126 != 0)) {
    if (uVar8 == 3) {
      if (_DAT_20000138 != (byte *)0x0) {
        if (DAT_2000012c == '\0') {
          uVar8 = (uint)DAT_2000010d;
          if ((uint)(DAT_2000010d >> 4) < (uVar8 & 0xf)) {
            uVar17 = (uint)(DAT_2000010d >> 4);
            uVar23 = (uint)DAT_2000105b;
            do {
              if ((*(int *)(pbVar14 + uVar17 * 4 + 4) != 0) && (*(short *)(pbVar14 + 0xe) != 0)) {
                uVar24 = (uint)*pbVar14;
                uVar1 = *(ushort *)(uVar23 * 4 + 0x80bb40c + uVar17 * 2);
                uVar12 = *(ushort *)(pbVar14 + 0xc);
                iVar13 = *(int *)(pbVar14 + uVar17 * 4 + 4);
                uVar1 = (ushort)(uint)((ulonglong)(uVar24 * (((uint)uVar1 << 0x15) >> 0x1a)) *
                                       0x80808081 >> 0x22) & 0xffe0 |
                        (ushort)((uVar24 * (uVar1 & 0x1f)) / 0xff) |
                        (ushort)(((uint)((ulonglong)(uVar24 * (uVar1 >> 0xb)) * 0x80808081 >> 0x20)
                                 & 0xfffff80) << 4);
                do {
                  iVar26 = (short)uVar12 * 0x1a;
                  iVar19 = (int)(short)(uVar12 + 9);
                  iVar22 = 0xc6;
                  uVar24 = 2;
                  while( true ) {
                    if (((((*(byte *)(iVar13 + iVar26 + ((uVar24 - 2) * 0x10000 >> 0x13)) >>
                            (uVar24 - 2 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                        (iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
                       (((uint)_DAT_20008352 <= iVar22 + 0x16U &&
                        (iVar22 + 0x16U < (uint)_DAT_20008356 + (uint)_DAT_20008352)))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x16) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if ((((*(byte *)(iVar13 + ((uVar24 - 1) * 0x10000 >> 0x13) + iVar26) >>
                           (uVar24 - 1 & 7) & 1) != 0) && ((int)(uint)_DAT_20008350 <= iVar19)) &&
                       ((iVar19 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
                        (((uint)_DAT_20008352 <= iVar22 + 0x15U &&
                         (iVar22 + 0x15U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                      *(ushort *)
                       (_DAT_20008358 +
                       (((iVar22 - (uint)_DAT_20008352) + 0x15) * (uint)_DAT_20008354 +
                       (iVar19 - (uint)_DAT_20008350)) * 2) = uVar1;
                    }
                    if (((((*(byte *)(iVar13 + ((uVar24 << 0x10) >> 0x13) + iVar26) >> (uVar24 & 7)
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
                  uVar12 = uVar12 + 1;
                } while ((uint)uVar12 <
                         (uint)*(ushort *)(pbVar14 + 0xc) + (uint)*(ushort *)(pbVar14 + 0xe));
              }
              uVar17 = uVar17 + 1;
            } while (uVar17 != (uVar8 & 0xf));
          }
        }
        else {
          iVar13 = *(int *)(_DAT_20000138 + 4);
          if (iVar13 != 0) {
            uVar8 = ((uint)*_DAT_20000138 * 0x1f) / 0xff;
            uVar12 = (ushort)uVar8 | (ushort)(uVar8 << 0xb);
            iVar26 = 0;
            do {
              iVar22 = iVar26 * 0x1a;
              uVar8 = iVar26 * 0x10000 + 0x3b0000 >> 0x10;
              iVar19 = 0xc9;
              uVar23 = 2;
              do {
                if ((((*(byte *)(iVar13 + ((uVar23 - 2) * 0x1000000 >> 0x1b) + iVar22) >>
                       (uVar23 - 2 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x13U &&
                     (iVar19 + 0x13U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x13) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar12;
                }
                if (((((*(byte *)(iVar13 + ((uVar23 - 1) * 0x1000000 >> 0x1b) + iVar22) >>
                        (uVar23 - 1 & 7) & 1) != 0) && (_DAT_20008350 <= uVar8)) &&
                    (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
                   (((uint)_DAT_20008352 <= iVar19 + 0x12U &&
                    (iVar19 + 0x12U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x12) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar12;
                }
                if ((((*(byte *)(iVar13 + ((uVar23 << 0x18) >> 0x1b) + iVar22) >> (uVar23 & 7) & 1)
                      != 0) && (_DAT_20008350 <= uVar8)) &&
                   ((uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
                    (((uint)_DAT_20008352 <= iVar19 + 0x11U &&
                     (iVar19 + 0x11U < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
                  *(ushort *)
                   (_DAT_20008358 +
                   (((iVar19 - (uint)_DAT_20008352) + 0x11) * (uint)_DAT_20008354 +
                   (uVar8 - _DAT_20008350)) * 2) = uVar12;
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
        pbVar14 = &DAT_20000138;
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
          iVar13 = FUN_0803edf0((int)uVar38,uVar33,(int)uVar37,uVar30);
          if (iVar13 == 0) {
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
                    uVar12 = 0;
                    do {
                      FUN_08018ea8((int)(short)(*(short *)(pbVar27 + 0xc) + uVar12 + 9),
                                   0xf8 - (uint)*(byte *)(*(int *)(pbVar27 + uVar23 * 4 + 4) + uVar8
                                                         ),
                                   (int)(short)(*(short *)(pbVar27 + 0xc) + uVar12 + 10),
                                   0xf8 - (uint)*(byte *)(uVar8 + *(int *)(pbVar27 + uVar23 * 4 + 4)
                                                         + 1));
                      pbVar27 = *(byte **)pbVar14;
                      uVar12 = uVar12 + 1;
                      uVar8 = (uint)uVar12;
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
                pbVar27 = *(byte **)pbVar14;
                uVar23 = uVar8 + 1 & 0xffff;
                uVar8 = uVar8 + 1;
              } while ((int)uVar23 <
                       (int)((uint)*(ushort *)(pbVar27 + 0xc) + (uint)*(ushort *)(pbVar27 + 0xe) +
                            -1));
            }
            pbVar14 = pbVar27 + 0x10;
            pbVar27 = *(byte **)pbVar14;
          }
          else {
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
            if (*(int *)(*(int *)pbVar14 + 8) != 0) {
              if (DAT_20001080 == '\0') {
                (*_DAT_20001070)(0);
              }
              else {
                uVar8 = *(int *)(*(int *)pbVar14 + 8) - _DAT_20001078;
                if (uVar8 >> 10 < 0xaf) {
                  uVar8 = uVar8 >> 5;
                  uVar23 = (uint)*(ushort *)(_DAT_2000107c + uVar8 * 2);
                  if (uVar23 != 0) {
                    FUN_080012bc(_DAT_2000107c + uVar8 * 2,uVar23 << 1);
                  }
                }
              }
            }
            pbVar27 = *(byte **)(*(int *)pbVar14 + 0x10);
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
            pbVar14 = &DAT_20000138;
            _DAT_20000138 = pbVar27;
          }
          if (pbVar27 == (byte *)0x0) break;
          uVar8 = (uint)DAT_20000134;
        }
      }
    }
  }
  fVar3 = DAT_08014948;
  uVar8 = (uint)_DAT_20000eae;
  if (uVar8 == 0) {
LAB_08014800:
    if (DAT_2000012c == '\0') {
      pbVar14 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar14 = &DAT_20000eb7;
      }
      bVar6 = *pbVar14;
      if ((uint)(bVar6 >> 4) < (bVar6 & 0xf)) {
        uVar8 = (uint)(bVar6 >> 4);
        sVar7 = (ushort)(bVar6 >> 4) * 0xad + 9;
        do {
          if (*(char *)(uVar8 + 0x20000102) != '\0') {
            piVar9 = (int *)(uVar8 * 4 + 0x20000104);
            iVar13 = *piVar9;
            if (iVar13 != 0) {
              for (iVar26 = 0; iVar19 = (int)(short)(sVar7 + (short)iVar26),
                  FUN_08018ea8(iVar19,0x46,iVar19,0x46 - (uint)*(byte *)(iVar13 + iVar26)),
                  iVar26 != 0x7f; iVar26 = iVar26 + 1) {
                iVar13 = *piVar9;
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
          iVar13 = 0x10;
          iVar26 = 0x10;
          uVar20 = 0x1e;
        }
        else {
          iVar13 = (int)(short)(_DAT_20000112 + 0x9a);
          iVar26 = (int)(short)(_DAT_20000112 + 0xa4);
          uVar20 = 0x14;
        }
      }
      else {
        iVar13 = 0x12e;
        iVar26 = 0x12e;
        uVar20 = 0x1e;
      }
      FUN_08019af8(iVar13,0x14,iVar26,uVar20);
    }
  }
  else {
    if (DAT_2000012c == '\0') {
      pbVar14 = &DAT_2000010d;
      if (DAT_20000126 == 0) {
        pbVar14 = &DAT_20000eb7;
      }
      bVar6 = *pbVar14;
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
                iVar13 = *(int *)(uVar23 * 4 + 0x20000ef4);
                uVar28 = uVar25;
                if (iVar13 == 0) {
                  uVar28 = 1;
                }
                lVar35 = (ulonglong)uVar28 * (uVar36 & 0xffffffff);
                uVar10 = (uint)lVar35;
                uVar16 = uVar28 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
                uVar18 = 0x12d - _DAT_20000ed8;
                uVar28 = (int)uVar18 >> 0x1f;
                if (uVar28 < uVar16 || uVar16 - uVar28 < (uint)(uVar18 <= uVar10)) {
                  uVar10 = uVar18;
                  uVar16 = uVar28;
                }
                if (-(uVar16 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= (uVar17 & 0xffff))) break;
                fVar31 = (float)VectorUnsignedToFloat(uVar17 & 0xffff,(byte)(in_fpscr >> 0x15) & 3);
                fVar11 = (float)FUN_0803ee1a(uVar24,uVar21);
                fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
                if (iVar13 == 0) {
                  fVar29 = 1.0;
                }
                iVar13 = uVar23 * 0x12d + 0x200000f8 + (uVar17 & 0xffff);
                fVar32 = (float)VectorUnsignedToFloat
                                          ((uint)_DAT_20000eac,(byte)(in_fpscr >> 0x15) & 3);
                fVar32 = fVar31 / ((fVar11 / fVar3) * fVar29) + fVar32;
                FUN_08018ea8((int)(fVar32 + 9.0),0xf8 - (uint)*(byte *)(iVar13 + 0x356),
                             (int)(fVar32 + 10.0),0xf8 - (uint)*(byte *)(iVar13 + 0x357));
                uVar8 = (uint)_DAT_20000eae;
                uVar17 = uVar17 + 1;
              }
            }
            else {
              uVar24 = (uint)_DAT_20000eac;
              uVar17 = uVar24;
              if ((int)uVar24 < (int)(uVar24 + uVar8 + -1)) {
                do {
                  iVar13 = uVar24 + uVar23 * 0x12d + 0x200000f8;
                  FUN_08018ea8((int)(short)((short)uVar17 + 9),
                               0xf8 - (uint)*(byte *)(iVar13 + 0x356),
                               (int)(short)((short)uVar17 + 10),
                               0xf8 - (uint)*(byte *)(iVar13 + 0x357));
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
    if (DAT_2000012c != '\0') goto LAB_08014dbe;
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
      iVar13 = ((int)(bVar6 - 1) / 3) * 0xfff1 + 0xcc;
      uVar8 = (uint)bVar6 % 3;
      uVar23 = 0;
      sVar7 = 0xc;
LAB_08014a18:
      uVar24 = uVar17 + 1;
      if (uVar8 == (uVar23 & 0xff) && uVar8 != 0) {
        sVar7 = 0xc;
        iVar13 = iVar13 + 0xf;
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
      pcVar15 = s__s__1f_s_08015a70;
      if ((byte)(uVar24 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1)) {
        pcVar15 = s__s__2f_s_08015a64;
      }
      FUN_080003b4(&DAT_20008360,pcVar15,uVar20);
      FUN_08008154((int)sVar7,(int)(short)iVar13,100,0xf);
      uVar23 = uVar23 + 1;
      if ((uVar23 & 0xff) < (uint)bVar6) {
        sVar7 = sVar7 + 100;
        if (0x137 < sVar7) {
          iVar13 = iVar13 + 0xf;
          sVar7 = 0xc;
        }
        goto LAB_08014a18;
      }
      goto LAB_08014c4c;
    }
  }
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
        iVar13 = *(int *)(uVar21 * 4 + 0x20000ef4);
        uVar8 = uVar25;
        if (iVar13 == 0) {
          uVar8 = 1;
        }
        lVar35 = (ulonglong)uVar8 * (uVar36 & 0xffffffff);
        uVar10 = (uint)lVar35;
        uVar16 = uVar8 * (int)(uVar36 >> 0x20) + (int)((ulonglong)lVar35 >> 0x20);
        uVar18 = 0x12d - _DAT_20000ed8;
        uVar8 = (int)uVar18 >> 0x1f;
        if (uVar8 < uVar16 || uVar16 - uVar8 < (uint)(uVar18 <= uVar10)) {
          uVar10 = uVar18;
          uVar16 = uVar8;
        }
        if (-(uVar16 - (uVar10 == 0)) < (uint)(uVar10 - 1 <= uVar28)) break;
        fVar31 = (float)VectorUnsignedToFloat(uVar28,(byte)(in_fpscr >> 0x15) & 3);
        fVar11 = (float)FUN_0803ee1a(uVar17,uVar24);
        fVar29 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
        if (iVar13 == 0) {
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
    uVar12 = 0;
    uVar8 = _DAT_20000114 + 100 & 0xffff;
    iVar13 = 0xdc - uVar8;
    if (uVar8 < 0xc9) {
      uVar8 = 200;
    }
    else {
      iVar13 = 0x14;
    }
    iVar26 = (int)(short)iVar13;
    iVar19 = (int)(short)(((short)uVar8 - _DAT_20000114) + -0x50);
    uVar8 = 0xc;
    while( true ) {
      if (((uVar12 < 3) && ((uint)_DAT_20008350 <= uVar8 - 3)) &&
         ((uVar8 - 3 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((iVar19 - (uint)_DAT_20008352) * (uint)_DAT_20008354 - (uint)_DAT_20008350) + uVar8) * 2
         + -6) = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0x138) break;
      if ((((uVar12 < 3) && ((uint)_DAT_20008350 <= uVar8 - 2)) &&
          (uVar8 - 2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -4) = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 < 3) && ((uint)_DAT_20008350 <= uVar8 - 1)) &&
         ((uVar8 - 1 < (uint)_DAT_20008350 + (uint)_DAT_20008354 &&
          (((int)(uint)_DAT_20008352 <= iVar26 &&
           (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))))) {
        *(undefined2 *)
         (_DAT_20008358 +
          (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2
         + -2) = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 < 3) && (_DAT_20008350 <= uVar8)) &&
          (uVar8 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         (((int)(uint)_DAT_20008352 <= iVar26 &&
          (iVar26 < (int)((uint)_DAT_20008352 + (uint)_DAT_20008356))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uint)_DAT_20008354 * (iVar19 - (uint)_DAT_20008352) - (uint)_DAT_20008350) + uVar8) * 2)
             = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
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
    pcVar15 = (char *)0x8015aa8;
    fVar3 = fVar11 / DAT_08014fd0;
    if (ABS(fVar11) < DAT_08014fd0) {
      pcVar15 = s___2fmV_08015aa0;
      fVar3 = fVar11;
    }
    uVar37 = FUN_0803ed70(fVar3);
    FUN_080003b4(&DAT_20008360,pcVar15,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
    sVar7 = (short)iVar13 + -10;
    if (0xd6 < iVar13) {
      sVar7 = 0xca;
    }
    if (iVar13 < 0x1e) {
      sVar7 = 0x14;
    }
    FUN_08008154(0xa0,(int)sVar7,0x7d,0x14);
  }
  if ((_DAT_20000120 != 0) && (DAT_2000012c == '\0')) {
    iVar13 = (int)(short)(_DAT_20000112 + 0x9f);
    uVar12 = 0;
    uVar8 = 0x1f;
    while( true ) {
      if ((((uVar12 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
          (iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 3 &&
          (uVar8 - 3 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -3) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (uVar8 == 0xdf) break;
      if (((uVar12 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
         ((iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          (((uint)_DAT_20008352 <= uVar8 - 2 &&
           (uVar8 - 2 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         (((uVar8 - _DAT_20008352) + -2) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if ((((uVar12 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
          (iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354))) &&
         (((uint)_DAT_20008352 <= uVar8 - 1 &&
          (uVar8 - 1 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((~(uint)_DAT_20008352 + uVar8) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2
         ) = 0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
      }
      if (((uVar12 < 3) && ((int)(uint)_DAT_20008350 <= iVar13)) &&
         ((iVar13 < (int)((uint)_DAT_20008350 + (uint)_DAT_20008354) &&
          ((_DAT_20008352 <= uVar8 && (uVar8 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))))) {
        *(undefined2 *)
         (_DAT_20008358 +
         ((uVar8 - _DAT_20008352) * (uint)_DAT_20008354 + (iVar13 - (uint)_DAT_20008350)) * 2) =
             0x7e2;
      }
      uVar12 = uVar12 + 1;
      if (4 < uVar12) {
        uVar12 = 0;
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
      uStack_5c = (uint)((ulonglong)uVar37 >> 0x20);
      uVar33 = (undefined4)uVar37;
      iVar13 = FUN_0803ee0c(uVar33,uStack_5c & 0x7fffffff | (uint)uVar36 & 0x80000000,uVar30,uVar20)
      ;
      if (iVar13 != 0) break;
      uVar37 = FUN_0803e124(uVar33,uStack_5c,uVar30,uVar20);
    }
    uStack_5c = uStack_5c ^ 0x80000000;
    FUN_080003b4(&DAT_20008360,0x80bcc11,uVar33,uStack_5c);
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
    uStack_51 = 0x24;
    FUN_0803acf0(_DAT_20002d6c,&uStack_51,0xffffffff);
    uStack_51 = 3;
    FUN_0803acf0(_DAT_20002d6c,&uStack_51,0xffffffff);
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



// ==========================================
// Function: FUN_0802cfbc @ 0802cfbc
// Size: 88 bytes
// ==========================================

void FUN_0802cfbc(void)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  int local_40;
  int local_3c [7];
  int local_20;
  byte local_11;
  undefined4 local_c;
  
  local_c = 0x80bc18b;
  iVar1 = FUN_080337a0(&local_c,&local_40,2);
  if (iVar1 == 0) {
    local_3c[0] = local_40;
    uVar2 = FUN_0802f6d8(local_3c,local_c);
    uVar3 = local_11 & 0xa0;
    if ((local_11 & 0xa0) != 0) {
      uVar3 = 6;
    }
    if (uVar2 != 0) {
      uVar3 = uVar2;
    }
    if (uVar3 == 0) {
      *(byte *)(local_20 + 0xb) = *(byte *)(local_20 + 0xb) | 6;
      *(undefined1 *)(local_40 + 3) = 1;
      FUN_08036934(local_40);
      return;
    }
  }
  return;
}



// ==========================================
// Function: FUN_0802e7bc @ 0802e7bc
// Size: 330 bytes
// ==========================================

void FUN_0802e7bc(void)

{
  char cVar1;
  undefined4 uVar2;
  char *pcVar3;
  undefined4 uVar4;
  undefined4 uVar5;
  int iVar6;
  
  uVar2 = FUN_08033cfc(0x1000);
  pcVar3 = (char *)FUN_08033cfc(1);
  uVar4 = FUN_08033cfc(0x30);
  uVar5 = FUN_08033cfc(0x102c);
  cVar1 = FUN_0802d80c(0x20002da8,0x80bba1f);
  *pcVar3 = cVar1;
  if (*pcVar3 == '\r') {
    cVar1 = FUN_0802d774(0x80bba1f);
    *pcVar3 = cVar1;
  }
  iVar6 = FUN_0802dc40(uVar4,0x80bb927);
  if (iVar6 == 0) {
    FUN_0802d534(0x80bbc58);
    FUN_0802d534(0x80bc1b4);
    FUN_0802d534(0x80bc18b);
    FUN_0802cfbc();
  }
  cVar1 = FUN_0802d80c(0x20003ddc,0x80bba22);
  *pcVar3 = cVar1;
  if (*pcVar3 == '\r') {
    cVar1 = FUN_0802d774(0x80bba22);
    *pcVar3 = cVar1;
  }
  iVar6 = FUN_0802dc40(uVar4,0x80bb92b);
  if (iVar6 == 0) {
    FUN_0802d534(0x80bc1a5);
  }
  cVar1 = FUN_0802d8b8(uVar5,0x80bc841,0);
  *pcVar3 = cVar1;
  if (*pcVar3 == '\0') {
    DAT_20001066 = 0;
    FUN_0802d014(uVar5);
  }
  else {
    DAT_20001066 = 1;
  }
  FUN_08033cbc(uVar2);
  FUN_08033cbc(pcVar3);
  FUN_08033cbc(uVar4);
  FUN_08033cbc(uVar5);
  return;
}



