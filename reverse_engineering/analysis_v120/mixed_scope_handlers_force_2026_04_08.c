// Force-decompiled functions

// Attempting to create function at 0x08002fe8
// Successfully created function
// ==========================================
// Function: FUN_08002fe8 @ 08002fe8
// Size: 1 bytes
// ==========================================

/* WARNING: Removing unreachable block (ram,0x0803ad26) */
/* WARNING: Removing unreachable block (ram,0x0803ad30) */
/* WARNING: Removing unreachable block (ram,0x0803ad32) */
/* WARNING: Removing unreachable block (ram,0x0803ad58) */
/* WARNING: Removing unreachable block (ram,0x0803ad62) */
/* WARNING: Removing unreachable block (ram,0x0803ad64) */
/* WARNING: Removing unreachable block (ram,0x0803ad6a) */
/* WARNING: Removing unreachable block (ram,0x0803ad70) */
/* WARNING: Removing unreachable block (ram,0x0803ad74) */
/* WARNING: Removing unreachable block (ram,0x0803ad80) */
/* WARNING: Removing unreachable block (ram,0x0803ad38) */
/* WARNING: Removing unreachable block (ram,0x0803ad3e) */
/* WARNING: Removing unreachable block (ram,0x0803ad42) */
/* WARNING: Removing unreachable block (ram,0x0803ad4e) */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

uint FUN_08002fe8(void)

{
  bool bVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  undefined1 uVar5;
  undefined1 auStack_3c [8];
  int iStack_34;
  int iStack_30;
  int iStack_2c;
  int iStack_28;
  undefined4 uStack_24;
  int iStack_20;
  undefined4 uStack_14;
  
  switch(DAT_20001060) {
  case 1:
    if ((bRam20001055 & 0xf0) != 0xb0) {
      bRam20001055 = 0;
    }
    break;
  case 2:
    DAT_20001060 = 9;
    goto code_r0x080030d8;
  case 3:
    if (DAT_20001061 == '\x02') {
      _DAT_20001061 = CONCAT11(DAT_20001062,1);
      DAT_20001063 = DAT_20001063 & 0xf;
    }
    else {
      if (DAT_20001061 != '\x01') {
        return 0x200000f8;
      }
      DAT_20001063 = 0;
      _DAT_20001061 = (ushort)DAT_20001062 << 8;
    }
    uRam20002d53 = 0x19;
code_r0x0803acf0:
    iVar4 = _DAT_20002d6c;
    uStack_24 = 0x20002d53;
    iStack_28 = -1;
    iStack_2c = 0;
    iStack_30 = 0;
    if (_DAT_20002d6c == 0) {
      bVar1 = (bool)isCurrentModePrivileged();
      if (bVar1) {
        setBasePriority(0x10);
      }
      InstructionSynchronizationBarrier(0xf);
      DataSynchronizationBarrier(0xf);
      do {
                    /* WARNING: Do nothing block with infinite loop */
      } while( true );
    }
    iStack_20 = _DAT_20002d6c;
    iVar3 = FUN_0803b730();
    if (iVar3 != 0 || iStack_28 == 0) {
      while( true ) {
        FUN_0803a168();
        if ((*(uint *)(iVar4 + 0x38) < *(uint *)(iVar4 + 0x3c)) || (iStack_2c == 2)) {
          iStack_34 = FUN_08034cc4(iVar4,uStack_24,iStack_2c);
          if (*(int *)(iVar4 + 0x24) == 0) {
            if (iStack_34 != 0) {
              _DAT_e000ed04 = 0x10000000;
              DataSynchronizationBarrier(0xf);
              InstructionSynchronizationBarrier(0xf);
            }
          }
          else {
            iVar4 = FUN_0803bb4c(iVar4 + 0x24);
            if (iVar4 != 0) {
              _DAT_e000ed04 = 0x10000000;
              DataSynchronizationBarrier(0xf);
              InstructionSynchronizationBarrier(0xf);
            }
          }
          FUN_0803a1b0();
          return 1;
        }
        if (iStack_28 == 0) {
          FUN_0803a1b0();
          return 0;
        }
        if (iStack_30 == 0) {
          FUN_0803a400(auStack_3c);
          iStack_30 = 1;
        }
        FUN_0803a1b0();
        FUN_0803a904();
        FUN_0803a168();
        if (*(char *)(iVar4 + 0x44) == -1) {
          *(undefined1 *)(iVar4 + 0x44) = 0;
        }
        if (*(char *)(iVar4 + 0x45) == -1) {
          *(undefined1 *)(iVar4 + 0x45) = 0;
        }
        FUN_0803a1b0();
        iVar3 = FUN_0803b5cc(auStack_3c,&iStack_28);
        if (iVar3 != 0) break;
        iVar3 = FUN_080352d4(iVar4);
        if (iVar3 == 0) {
          FUN_080357b8(iVar4);
          FUN_0803bc10();
        }
        else {
          FUN_0803a434(iVar4 + 0x10,iStack_28);
          FUN_080357b8(iVar4);
          iVar3 = FUN_0803bc10();
          if (iVar3 == 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
      }
      FUN_080357b8(iVar4);
      FUN_0803bc10();
      return 0;
    }
    bVar1 = (bool)isCurrentModePrivileged();
    if (bVar1) {
      setBasePriority(0x10);
    }
    InstructionSynchronizationBarrier(0xf);
    DataSynchronizationBarrier(0xf);
    do {
                    /* WARNING: Do nothing block with infinite loop */
    } while( true );
  case 4:
    if (DAT_20001061 == '\x01') {
      _DAT_20001061 = (ushort)DAT_20001062 << 8;
      DAT_20001063 = 0;
      uRam20002d53 = 0x20;
      FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
      uRam20002d53 = 0x21;
      goto code_r0x0803acf0;
    }
    break;
  case 9:
    if ((DAT_20001061 != '\0') && (cRam2000044d != '\x01')) {
      if (DAT_20001061 == '\x02') {
        _DAT_20001061 = CONCAT11(DAT_20001062,1);
        DAT_20001063 = DAT_20001063 & 0xf;
        uRam20002d53 = 0x14;
      }
      else {
        if (DAT_20001061 != '\x01') {
          return 0x200000f8;
        }
        _DAT_20001061 = (ushort)DAT_20001062 << 8;
        DAT_20001063 = 0;
        uRam20002d53 = 0x13;
        FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
        uRam20002d53 = 0x14;
      }
      goto code_r0x0803acf0;
    }
    cRam2000044d = '\0';
    DAT_20001060 = 2;
code_r0x080030d8:
    _DAT_20001061 = 0;
    DAT_20001063 = 0;
    uVar2 = (uint)DAT_20001060;
    switch(uVar2) {
    case 0:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(1,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0xb,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0xc,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0xd,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0xe,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0xf,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x10,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 0x11;
      break;
    case 1:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(9,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 7;
      if (-1 < _DAT_40011008 << 0x18) {
        uVar5 = 10;
      }
      uStack_14 = CONCAT13(uVar5,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x1a,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x1b,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x1c,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x1d,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 0x1e;
      break;
    case 2:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = CONCAT13(2,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(3,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(4,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(5,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(6,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 8;
      goto code_r0x0800bc6e;
    case 3:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(8,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(9,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 7;
      if (-1 < _DAT_40011008 << 0x18) {
        uVar5 = 10;
      }
      uStack_14 = CONCAT13(uVar5,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x16,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x17,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x18,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 0x19;
      break;
    case 4:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x1f,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(9,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x20,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 0x21;
      break;
    case 5:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x25,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(9,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x26,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x27,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 0x28;
      break;
    case 6:
      uVar5 = 0x29;
      break;
    case 7:
      uVar5 = 0x15;
      break;
    case 8:
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
      uVar5 = 0x2c;
      break;
    case 9:
      iVar4 = (int)&uStack_14 + 3;
      uStack_14 = uStack_14 & 0xffffff;
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x12,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uStack_14 = CONCAT13(0x13,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,iVar4,0xffffffff);
      uVar5 = 0x14;
code_r0x0800bc6e:
      uStack_14 = CONCAT13(uVar5,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
      uStack_14 = CONCAT13(9,(undefined3)uStack_14);
      FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
      uVar5 = 7;
      if (-1 < _DAT_40011008 << 0x18) {
        uVar5 = 10;
      }
      break;
    default:
      goto LAB_0800bcce;
    }
    uStack_14 = CONCAT13(uVar5,(undefined3)uStack_14);
    uVar2 = FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
LAB_0800bcce:
    return uVar2;
  }
  return 0x200000f8;
}



// ==========================================
// Function: FUN_080041f8 @ 080041f8
// Size: 1 bytes
// ==========================================

/* WARNING: Removing unreachable block (ram,0x0803ad26) */
/* WARNING: Removing unreachable block (ram,0x0803ad30) */
/* WARNING: Removing unreachable block (ram,0x0803ad32) */
/* WARNING: Removing unreachable block (ram,0x0803ad58) */
/* WARNING: Removing unreachable block (ram,0x0803ad62) */
/* WARNING: Removing unreachable block (ram,0x0803ad64) */
/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x0801f076 */
/* WARNING: Removing unreachable block (ram,0x0803ad6a) */
/* WARNING: Removing unreachable block (ram,0x0803ad70) */
/* WARNING: Removing unreachable block (ram,0x0803ad74) */
/* WARNING: Removing unreachable block (ram,0x0803ad80) */
/* WARNING: Removing unreachable block (ram,0x0803ae3a) */
/* WARNING: Removing unreachable block (ram,0x0803ad38) */
/* WARNING: Removing unreachable block (ram,0x0803ad3e) */
/* WARNING: Removing unreachable block (ram,0x0803ad42) */
/* WARNING: Removing unreachable block (ram,0x0803ad4e) */
/* WARNING: Removing unreachable block (ram,0x0803adcc) */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

uint FUN_080041f8(undefined4 param_1,undefined4 param_2)

{
  ushort *puVar1;
  uint uVar2;
  undefined4 uVar3;
  int iVar4;
  int iVar5;
  uint uVar6;
  ushort *puVar7;
  undefined1 *puVar8;
  uint uVar9;
  undefined4 uVar10;
  char cVar11;
  byte bVar12;
  byte bVar13;
  bool bVar14;
  uint in_fpscr;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  undefined4 extraout_s1_01;
  undefined4 extraout_s1_02;
  undefined4 extraout_s1_03;
  undefined4 extraout_s1_04;
  undefined4 extraout_s1_05;
  undefined4 extraout_s1_06;
  undefined4 extraout_s1_07;
  undefined4 extraout_s1_08;
  undefined4 extraout_s1_09;
  undefined4 extraout_s1_10;
  float fVar15;
  float fVar16;
  float fVar17;
  undefined4 uVar18;
  undefined4 uVar19;
  undefined8 uVar20;
  undefined8 uVar21;
  undefined4 uStack_3c;
  
  switch((uint)DAT_20001060) {
  case 0:
    bVar14 = DAT_20001061 == 0;
    DAT_20001061 = DAT_20001061 - 1;
    if (bVar14) {
      DAT_20001061 = 3;
    }
    uRam20002d53 = 0xb;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0xc;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0xf;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x10;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x11;
    break;
  case 1:
    if ((bRam20001055 & 0xf0) == 0xb0) {
      return 0xb0;
    }
    bVar14 = DAT_20001025 == '\0';
    DAT_20001025 = DAT_20001025 - 1;
    if (bVar14) {
      DAT_20001025 = 7;
    }
    sRam20002d54 = *(byte *)(DAT_20001025 + 0x80bb3fc) + 0x500;
    bRam20001055 = 0;
    FUN_0803acf0(uRam20002d74,&sRam20002d54,0xffffffff);
    uRam20002d53 = 0x1d;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x1b;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    DAT_2000102e = DAT_20001025;
    if (DAT_20001025 != 2) {
      DAT_2000102e = 1;
    }
    uRam20001040 = 0x7fc00000;
    uRam20001044 = 0x7fc00000;
    uRam20001048 = 0x7fc00000;
    if (DAT_20001034 != '\0') {
      DAT_20001034 = '\0';
    }
    if (DAT_20001035 != '\0') {
      DAT_20001035 = '\0';
      uRam20002d53 = 0x1a;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    }
    if (DAT_20001035 != '\0') {
      uVar20 = FUN_0803ed70(_DAT_20001038);
      uVar3 = FUN_0803e5da(DAT_2000103c);
      uVar18 = (undefined4)DAT_08002bf0;
      uVar19 = (undefined4)((ulonglong)DAT_08002bf0 >> 0x20);
      uVar3 = FUN_0803c8e0(uVar18,param_2,uVar3);
      uVar20 = FUN_0803e77c(uVar3,extraout_s1,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
      uVar21 = FUN_0803ed70(_DAT_20001028);
      uVar3 = FUN_0803e5da(DAT_2000102f);
      uVar3 = FUN_0803c8e0(uVar18,extraout_s1,uVar3);
      uVar21 = FUN_0803e77c(uVar3,extraout_s1_00,(int)uVar21,(int)((ulonglong)uVar21 >> 0x20));
      uVar20 = FUN_0803eb94((int)uVar21,(int)((ulonglong)uVar21 >> 0x20),(int)uVar20,
                            (int)((ulonglong)uVar20 >> 0x20));
      uVar2 = (uint)((ulonglong)uVar20 >> 0x20);
      iVar5 = (int)((longlong)DAT_08002bf8 >> 0x3f);
      uVar3 = (undefined4)((ulonglong)DAT_08002c00 >> 0x20);
      uVar10 = (undefined4)DAT_08002c00;
      uVar6 = uVar2 & 0x7fffffff | iVar5 * -0x80000000;
      DAT_2000102f = '\0';
      iVar4 = FUN_0803ee0c((int)uVar20,uVar6,uVar10,uVar3);
      cVar11 = iVar4 == 0;
      if ((bool)cVar11) {
        DAT_2000102f = '\x01';
        uVar20 = FUN_0803e124((int)uVar20,uVar2,uVar18,uVar19);
        uVar6 = (uint)((ulonglong)uVar20 >> 0x20) & 0x7fffffff | iVar5 * -0x80000000;
      }
      iVar4 = FUN_0803ee0c((int)uVar20,uVar6,uVar10,uVar3);
      if (iVar4 == 0) {
        cVar11 = cVar11 + '\x01';
        DAT_2000102f = cVar11;
        uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar18,uVar19);
        uVar6 = (uint)((ulonglong)uVar20 >> 0x20) & 0x7fffffff | iVar5 * -0x80000000;
      }
      iVar4 = FUN_0803ee0c((int)uVar20,uVar6,uVar10,uVar3);
      if (iVar4 == 0) {
        cVar11 = cVar11 + '\x01';
        DAT_2000102f = cVar11;
        uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar18,uVar19);
        uVar6 = (uint)((ulonglong)uVar20 >> 0x20) & 0x7fffffff | iVar5 * -0x80000000;
      }
      iVar5 = FUN_0803ee0c((int)uVar20,uVar6,uVar10,uVar3);
      if (iVar5 == 0) {
        DAT_2000102f = cVar11 + '\x01';
        uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar18,uVar19);
      }
      _DAT_20001028 = (float)FUN_0803df48((int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
      fVar15 = ABS(_DAT_20001028);
      if ((fVar15 < DAT_08002c08) || (DAT_2000102c < 2)) {
        if ((fVar15 < DAT_08002c0c) || (DAT_2000102c < 3)) {
          if ((10.0 <= fVar15) && (3 < DAT_2000102c)) {
            DAT_2000102c = 3;
          }
        }
        else {
          DAT_2000102c = 2;
        }
      }
      else {
        DAT_2000102c = 1;
      }
    }
    switch(DAT_20001025) {
    case 0:
      bVar12 = 5;
      switch(DAT_20001027) {
      case 0:
        DAT_20001026 = 0;
        DAT_20001030 = -1;
        break;
      case 1:
        DAT_20001030 = DAT_2000102f;
        if (DAT_2000102e - 1 < 2) {
          DAT_20001026 = DAT_2000102e;
        }
        break;
      case 2:
        if (DAT_2000102e == 2) {
          bVar12 = 3;
          goto code_r0x08002b98;
        }
        goto code_r0x08002b9c;
      case 3:
code_r0x08002b98:
        DAT_20001026 = bVar12;
code_r0x08002b9c:
        DAT_20001030 = DAT_2000102f + '\x02';
      }
      uStack_3c = CONCAT13(0x1b,(undefined3)uStack_3c);
      FUN_0803acf0(_DAT_20002d6c,(int)&uStack_3c + 3,0xffffffff);
      break;
    case 1:
      if (DAT_2000102e - 1 < 2) {
        DAT_20001026 = DAT_2000102e;
      }
      DAT_20001030 = DAT_2000102f;
      break;
    case 2:
      if (DAT_2000102e == 1) {
        DAT_20001026 = 4;
        DAT_20001030 = DAT_2000102f;
        break;
      }
      if (DAT_2000102e != 2) break;
      DAT_20001026 = 3;
      goto code_r0x08002b10;
    case 3:
      DAT_20001026 = 5;
code_r0x08002b10:
      DAT_20001030 = DAT_2000102f + '\x02';
      break;
    case 4:
      DAT_20001026 = 6;
      DAT_20001030 = DAT_2000102f + '\x05';
      break;
    case 5:
      DAT_20001026 = 7;
      DAT_20001030 = DAT_2000102f + '\t';
      break;
    case 6:
      if (DAT_2000102e == 2) {
        DAT_20001026 = 9;
      }
      else if (DAT_2000102e == 1) {
        DAT_20001026 = 8;
      }
      goto code_r0x08002b6e;
    case 7:
      if (DAT_2000102e == 2) {
        DAT_20001026 = 0xb;
      }
      else if (DAT_2000102e == 1) {
        DAT_20001026 = 10;
      }
code_r0x08002b6e:
      DAT_20001030 = DAT_2000102f + '\n';
    }
    uStack_3c = CONCAT13(0x1c,(undefined3)uStack_3c);
    FUN_0803acf0(_DAT_20002d6c,(int)&uStack_3c + 3,0xffffffff);
    uStack_3c = CONCAT13(0x1e,(undefined3)uStack_3c);
    uVar2 = FUN_0803acf0(_DAT_20002d6c,(int)&uStack_3c + 3,0xffffffff);
    return uVar2;
  case 2:
    uVar2 = (uint)bRam2000044c;
    if ((uVar2 & 0xf) - 1 < 2) {
      FUN_080342f8(0xffffffff,0xffffffff);
      goto code_r0x080047ba;
    }
    if ((uVar2 & 0xf) == 3) {
      if (DAT_20000332 == 3) {
        if ((int)(uVar2 << 0x18) < 0) {
          if (_DAT_2000032c < -0x95) {
            return 0xffffff6a;
          }
          goto code_r0x080047a4;
        }
        if (_DAT_2000032a < -0x95) {
          return 0xffffff6a;
        }
        _DAT_2000032a = _DAT_2000032a + -1;
      }
      else if (DAT_20000332 == 2) {
        if (_DAT_20000330 < -99) {
          return (int)_DAT_20000330;
        }
        _DAT_20000330 = _DAT_20000330 + -1;
      }
      else {
        if (DAT_20000332 != 1) {
          return uVar2;
        }
        if (_DAT_2000032c < -0x95) {
          return (int)_DAT_2000032c;
        }
code_r0x080047a4:
        _DAT_2000032c = _DAT_2000032c + -1;
      }
      puVar8 = (undefined1 *)0x0;
      uVar10 = 0;
      uVar3 = uRam20002d80;
    }
    else {
      if ((bRam2000044c & 0xf) != 0) {
        return uVar2;
      }
      uVar6 = (uint)DAT_20000125;
      if (0x1b < uVar6) {
        return uVar2;
      }
      uVar2 = (uint)DAT_200000fa;
      uVar9 = uVar6 + 1;
      DAT_20000125 = (byte)uVar9;
      if (uVar6 < 4) {
        if ((uVar9 == 4) || (DAT_2000010c == '\x03')) {
          puVar1 = (ushort *)(&DAT_200003a8 + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_2000036c + uVar2 * 2);
        }
        else {
          puVar1 = (ushort *)(&DAT_200003bc + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_20000380 + uVar2 * 2);
        }
      }
      else {
        puVar1 = (ushort *)(&DAT_20000394 + uVar2 * 2);
        puVar7 = (ushort *)(&DAT_20000358 + uVar2 * 2);
      }
      fVar15 = (float)VectorSignedToFloat((uint)*puVar1 - (uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3
                                         );
      fVar16 = (float)VectorSignedToFloat(DAT_200000fc + 100,(byte)(in_fpscr >> 0x15) & 3);
      fVar17 = (float)VectorUnsignedToFloat((uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3);
      uVar2 = VectorFloatToUnsigned((fVar15 / fRam080047c8) * fVar16 + fVar17,3);
      _DAT_40007408 = uVar2 & 0xfff | _DAT_40007408 & 0xfffff000;
      _DAT_40007404 = _DAT_40007404 | 1;
      uVar2 = (uint)DAT_200000fb;
      if ((uVar9 & 0xff) < 5) {
        if (((uVar9 & 0xff) == 4) || (DAT_2000010c == '\x03')) {
          puVar1 = (ushort *)(&DAT_20000420 + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_200003e4 + uVar2 * 2);
        }
        else {
          puVar1 = (ushort *)(&DAT_20000434 + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_200003f8 + uVar2 * 2);
        }
      }
      else {
        puVar1 = (ushort *)(&DAT_2000040c + uVar2 * 2);
        puVar7 = (ushort *)(&DAT_200003d0 + uVar2 * 2);
      }
      fVar15 = (float)VectorSignedToFloat((uint)*puVar1 - (uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3
                                         );
      fVar16 = (float)VectorSignedToFloat(DAT_200000fd + 100,(byte)(in_fpscr >> 0x15) & 3);
      fVar17 = (float)VectorUnsignedToFloat((uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3);
      _DAT_40001c34 = VectorFloatToUnsigned((fVar15 / fRam080047c8) * fVar16 + fVar17,3);
      if (DAT_20000126 != '\0') {
        if ((uVar9 & 0xff) < 0x14) {
          uRam40000400 = uRam40000400 & 0xfffffffe;
          if (DAT_2000010f == '\x01') {
            _DAT_20000eac = 0x12d0000;
          }
          else {
            _DAT_20000eac = 0x12d;
          }
        }
        else {
          _DAT_20000eac = 0x12d;
          FUN_0802a430(&stack0xffffffd8);
          if ((byte)(DAT_20000125 - 0x14) < 9) {
            uRam4000042c = *(undefined4 *)((char)(DAT_20000125 - 0x14) * 4 + 0x80465a4);
            uRam40000428 = 0xffffffff;
            uRam40000414 = uRam40000414 | 1;
          }
          uRam40000424 = 0;
          uRam40000400 = uRam40000400 | 1;
        }
      }
      FUN_08029a70();
      FUN_080342f8(0,0);
      _DAT_40011410 = 4;
      uRam20002d53 = 2;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      puVar8 = &uRam20002d52;
      uRam20002d52 = 1;
      uVar10 = 0xffffffff;
      uVar3 = _DAT_20002d78;
    }
    FUN_0803acf0(uVar3,puVar8,uVar10);
code_r0x080047ba:
    uVar2 = (uint)DAT_2000012c;
    if (uVar2 == 0) {
      if ((DAT_20000332 & 1) != 0) {
        _DAT_2000033c =
             (float)VectorSignedToFloat((int)_DAT_2000032a - (int)_DAT_20000112,
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000334 = _DAT_2000033c / 25.0;
        uVar6 = DAT_20000125 / 3 + 1;
        _DAT_20000338 =
             (float)VectorSignedToFloat((int)_DAT_2000032c - (int)_DAT_20000112,
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_2000033c = _DAT_20000338 - _DAT_2000033c;
        uVar2 = uVar6 / 3;
        DAT_20000350 = (char)uVar2;
        uVar20 = FUN_0803e5da((&DAT_080465c8)[(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
        uVar3 = FUN_0803e50a(uVar6 - (uVar2 * 3 & 0xff));
        uVar3 = FUN_0803c8e0((int)DAT_0801f3b0,param_2,uVar3);
        uVar20 = FUN_0803e77c(uVar3,extraout_s1_01,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        uVar21 = FUN_0803ed70(_DAT_20000334);
        FUN_0803e77c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                     (int)((ulonglong)uVar21 >> 0x20));
        _DAT_20000334 = (float)FUN_0803df48();
        fVar15 = DAT_0801f3b8;
        uVar2 = in_fpscr & 0xfffffff | (uint)(ABS(_DAT_20000334) < DAT_0801f3b8) << 0x1f;
        uVar6 = uVar2 | (uint)(NAN(ABS(_DAT_20000334)) || NAN(DAT_0801f3b8)) << 0x1c;
        if ((byte)(uVar2 >> 0x1f) == ((byte)(uVar6 >> 0x1c) & 1)) {
          _DAT_20000334 = _DAT_20000334 / DAT_0801f3b8;
          DAT_20000350 = DAT_20000350 + '\x01';
        }
        _DAT_20000338 = _DAT_20000338 / 25.0;
        uVar2 = DAT_20000125 / 3 + 1;
        uVar9 = uVar2 / 3;
        DAT_20000351 = (char)uVar9;
        uVar20 = FUN_0803e5da((&DAT_080465c8)[(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
        uVar3 = FUN_0803e50a(uVar2 - (uVar9 * 3 & 0xff));
        uVar3 = FUN_0803c8e0((int)DAT_0801f3b0,extraout_s1_01,uVar3);
        uVar20 = FUN_0803e77c(uVar3,extraout_s1_02,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        uVar21 = FUN_0803ed70(_DAT_20000338);
        FUN_0803e77c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                     (int)((ulonglong)uVar21 >> 0x20));
        _DAT_20000338 = (float)FUN_0803df48();
        uVar2 = uVar6 & 0xfffffff | (uint)(ABS(_DAT_20000338) < fVar15) << 0x1f;
        uVar6 = uVar2 | (uint)(NAN(ABS(_DAT_20000338)) || NAN(fVar15)) << 0x1c;
        if ((byte)(uVar2 >> 0x1f) == ((byte)(uVar6 >> 0x1c) & 1)) {
          _DAT_20000338 = _DAT_20000338 / fVar15;
          DAT_20000351 = DAT_20000351 + '\x01';
        }
        _DAT_2000033c = _DAT_2000033c / 25.0;
        uVar2 = DAT_20000125 / 3 + 1;
        uVar9 = uVar2 / 3;
        DAT_20000352 = (char)uVar9;
        uVar20 = FUN_0803e5da((&DAT_080465c8)[(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
        uVar3 = FUN_0803e50a(uVar2 - (uVar9 * 3 & 0xff));
        uVar3 = FUN_0803c8e0((int)DAT_0801f3b0,extraout_s1_02,uVar3);
        uVar20 = FUN_0803e77c(uVar3,extraout_s1_03,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        uVar21 = FUN_0803ed70(_DAT_2000033c);
        FUN_0803e77c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                     (int)((ulonglong)uVar21 >> 0x20));
        _DAT_2000033c = (float)FUN_0803df48();
        if (!NAN(ABS(_DAT_2000033c)) && !NAN(fVar15)) {
          _DAT_2000033c = _DAT_2000033c / fVar15;
          DAT_20000352 = DAT_20000352 + '\x01';
        }
        in_fpscr = uVar6 & 0xfffffff | (uint)(_DAT_2000033c == 0.0) << 0x1e;
        param_2 = extraout_s1_03;
        if ((byte)(in_fpscr >> 0x1e) == 0) {
          fVar15 = ABS(_DAT_2000033c);
          uVar20 = DAT_0801f3c8;
          if (DAT_20000352 != '\0') {
            uVar3 = FUN_0803e5da();
            uVar3 = FUN_0803c8e0((int)DAT_0801f3c0,extraout_s1_03,uVar3);
            uVar20 = FUN_0803e124((int)DAT_0801f3c8,(int)((ulonglong)DAT_0801f3c8 >> 0x20),uVar3,
                                  extraout_s1_04);
            param_2 = extraout_s1_04;
          }
          uVar21 = FUN_0803ed70(fVar15);
          uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                                (int)((ulonglong)uVar21 >> 0x20));
          uVar3 = (undefined4)((ulonglong)DAT_0801f3c0 >> 0x20);
          uVar10 = (undefined4)DAT_0801f3c0;
          DAT_20000356 = 0;
          iVar5 = FUN_0803ee0c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar10,uVar3);
          bVar12 = DAT_20000356;
          if (iVar5 == 0) {
            bVar12 = 0;
            do {
              uStack_3c = (uint)(byte)(bVar12 + 1);
              uVar21 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar10,uVar3);
              uVar20 = FUN_0803e124((int)uVar21,(int)((ulonglong)uVar21 >> 0x20),uVar10,uVar3);
              if (uStack_3c < 3) {
                bVar12 = bVar12 + 1;
                uVar20 = uVar21;
              }
              uVar18 = (undefined4)((ulonglong)uVar20 >> 0x20);
              iVar5 = FUN_0803edf0((int)uVar20,uVar18,uVar10,uVar3);
              if (iVar5 == 0) break;
              uStack_3c = (uint)(byte)(bVar12 + 1);
              uVar21 = FUN_0803e124((int)uVar20,uVar18,uVar10,uVar3);
              uVar20 = FUN_0803e124((int)uVar21,(int)((ulonglong)uVar21 >> 0x20),uVar10,uVar3);
              if (uStack_3c < 3) {
                bVar12 = bVar12 + 1;
                uVar20 = uVar21;
              }
              uVar18 = (undefined4)((ulonglong)uVar20 >> 0x20);
              iVar5 = FUN_0803ee0c((int)uVar20,uVar18,uVar10,uVar3);
              if (iVar5 != 0) break;
              bVar13 = bVar12 + 1;
              uStack_3c = (uint)bVar13;
              uVar20 = FUN_0803e124((int)uVar20,uVar18,uVar10,uVar3);
              uVar21 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar10,uVar3);
              if (2 < uStack_3c) {
                bVar13 = bVar12;
                uVar20 = uVar21;
              }
              uVar18 = (undefined4)((ulonglong)uVar20 >> 0x20);
              iVar5 = FUN_0803ee0c((int)uVar20,uVar18,uVar10,uVar3);
              bVar12 = bVar13;
              if (iVar5 != 0) break;
              bVar12 = bVar13 + 1;
              uVar20 = FUN_0803e124((int)uVar20,uVar18,uVar10,uVar3);
              uVar21 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar10,uVar3);
              if (2 < bVar12) {
                bVar12 = bVar13;
                uVar20 = uVar21;
              }
              iVar5 = FUN_0803ee0c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar10,uVar3);
            } while (iVar5 == 0);
          }
          DAT_20000356 = bVar12;
          _DAT_2000034c = FUN_0803df48((int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        }
        else {
          _DAT_2000034c = 0;
          DAT_20000356 = 0;
        }
      }
      uVar2 = (uint)DAT_20000332 << 0x1e;
      if ((int)uVar2 < 0) {
        uVar2 = (uint)DAT_2000010e;
        _DAT_20000340 =
             (float)VectorSignedToFloat((int)_DAT_2000032e - (int)(char)(&DAT_200000fc)[uVar2],
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000344 =
             (float)VectorSignedToFloat((int)_DAT_20000330 - (int)(char)(&DAT_200000fc)[uVar2],
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000348 = _DAT_20000340 - _DAT_20000344;
        fVar15 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                        [(uint)(byte)(&DAT_200000fa)[uVar2] % 3] <<
                                            2,(byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000340 = _DAT_20000340 * fVar15;
        if ((byte)(&DAT_200000fa)[uVar2] < 3) {
          _DAT_20000340 = _DAT_20000340 / 10.0;
        }
        else {
          uVar3 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar2] - 3) / 3));
          uVar3 = FUN_0803c8e0((int)DAT_0801f6e8,param_2,uVar3);
          uVar20 = FUN_0803ed70(_DAT_20000340);
          FUN_0803e77c(uVar3,extraout_s1_05,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
          _DAT_20000340 = (float)FUN_0803df48();
          uVar2 = (uint)DAT_2000010e;
          param_2 = extraout_s1_05;
        }
        uVar3 = FUN_0803e5da(*(undefined1 *)(uVar2 + 0x200000fe));
        uVar3 = FUN_0803c8e0((int)DAT_0801f6e8,param_2,uVar3);
        uVar20 = FUN_0803ed70(_DAT_20000340);
        FUN_0803e77c(uVar3,extraout_s1_06,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        _DAT_20000340 = (float)FUN_0803df48();
        fVar15 = DAT_0801f6f0;
        uVar2 = in_fpscr & 0xfffffff | (uint)(ABS(_DAT_20000340) < DAT_0801f6f0) << 0x1f;
        uVar6 = uVar2 | (uint)(NAN(ABS(_DAT_20000340)) || NAN(DAT_0801f6f0)) << 0x1c;
        DAT_20000353 = (byte)(uVar2 >> 0x1f) == ((byte)(uVar6 >> 0x1c) & 1);
        if ((bool)DAT_20000353) {
          _DAT_20000340 = _DAT_20000340 / DAT_0801f6f0;
        }
        uVar2 = (uint)DAT_2000010e;
        fVar16 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                        [(uint)(byte)(&DAT_200000fa)[uVar2] % 3] <<
                                            2,(byte)(uVar6 >> 0x15) & 3);
        _DAT_20000344 = _DAT_20000344 * fVar16;
        if ((byte)(&DAT_200000fa)[uVar2] < 3) {
          _DAT_20000344 = _DAT_20000344 / 10.0;
          uVar3 = extraout_s1_06;
        }
        else {
          uVar3 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar2] - 3) / 3));
          uVar3 = FUN_0803c8e0((int)DAT_0801f6e8,extraout_s1_06,uVar3);
          uVar20 = FUN_0803ed70(_DAT_20000344);
          FUN_0803e77c(uVar3,extraout_s1_07,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
          _DAT_20000344 = (float)FUN_0803df48();
          uVar2 = (uint)DAT_2000010e;
          uVar3 = extraout_s1_07;
        }
        uVar10 = FUN_0803e5da(*(undefined1 *)(uVar2 + 0x200000fe));
        uVar3 = FUN_0803c8e0((int)DAT_0801f6e8,uVar3,uVar10);
        uVar20 = FUN_0803ed70(_DAT_20000344);
        FUN_0803e77c(uVar3,extraout_s1_08,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        _DAT_20000344 = (float)FUN_0803df48();
        uVar2 = uVar6 & 0xfffffff | (uint)(ABS(_DAT_20000344) < fVar15) << 0x1f;
        DAT_20000354 = SUB41(uVar2 >> 0x1f,0) == (NAN(ABS(_DAT_20000344)) || NAN(fVar15));
        if ((bool)DAT_20000354) {
          _DAT_20000344 = _DAT_20000344 / fVar15;
        }
        uVar6 = (uint)DAT_2000010e;
        fVar16 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                        [(uint)(byte)(&DAT_200000fa)[uVar6] % 3] <<
                                            2,(byte)(uVar2 >> 0x15) & 3);
        _DAT_20000348 = _DAT_20000348 * fVar16;
        if ((byte)(&DAT_200000fa)[uVar6] < 3) {
          _DAT_20000348 = _DAT_20000348 / 10.0;
          uVar3 = extraout_s1_08;
        }
        else {
          uVar3 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar6] - 3) / 3));
          uVar3 = FUN_0803c8e0((int)DAT_0801f6e8,extraout_s1_08,uVar3);
          uVar20 = FUN_0803ed70(_DAT_20000348);
          FUN_0803e77c(uVar3,extraout_s1_09,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
          _DAT_20000348 = (float)FUN_0803df48();
          uVar6 = (uint)DAT_2000010e;
          uVar3 = extraout_s1_09;
        }
        uVar10 = FUN_0803e5da(*(undefined1 *)(uVar6 + 0x200000fe));
        uVar3 = FUN_0803c8e0((int)DAT_0801f6e8,uVar3,uVar10);
        uVar20 = FUN_0803ed70(_DAT_20000348);
        FUN_0803e77c(uVar3,extraout_s1_10,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        _DAT_20000348 = (float)FUN_0803df48();
        uVar2 = 0;
        DAT_20000355 = 0;
        if (fVar15 <= ABS(_DAT_20000348)) {
          _DAT_20000348 = _DAT_20000348 / fVar15;
          uVar2 = 1;
          DAT_20000355 = 1;
        }
      }
    }
    return uVar2;
  case 3:
    if (DAT_20001061 != 2) {
      return (uint)DAT_20001061;
    }
    if ((DAT_20001063 & 0xf) == 2) {
      if ((DAT_20001063 & 0xf0) == 0) goto code_r0x0800440c;
    }
    else if ((DAT_20001063 & 0xf) == 1) {
      if (DAT_20001063 < 0x20) goto code_r0x0800440c;
    }
    else if (((DAT_20001063 & 0xf) == 0) && (DAT_20001063 < 0x40)) {
code_r0x0800440c:
      DAT_20001063 = DAT_20001063 + 0x10;
    }
    uRam20002d53 = 0x19;
    break;
  case 4:
    if (DAT_20001061 != 1) {
      return (uint)DAT_20001061;
    }
    if ((DAT_20001062 & 0xf) != 1) {
      return DAT_20001062 & 0xf;
    }
    if (DAT_20001063 == 0) {
      if (cRam2000105a == '\0') {
        return 0;
      }
      cRam2000105a = cRam2000105a + -1;
    }
    else {
      uVar2 = (uint)bRam20001059;
      if (uVar2 < 0x1a) {
        return uVar2;
      }
      bRam20001059 = (byte)(uVar2 - 5);
      uRam40015034 = uVar2 - 5 & 0xff;
    }
    uRam20002d53 = 0x21;
    break;
  case 5:
    if (DAT_20000f14 != 0) {
      return (uint)DAT_20000f14;
    }
    if (cRam20000f15 == '\0') {
      return 0;
    }
    cRam20000f15 = cRam20000f15 + -1;
    uRam20002d53 = 0x27;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x28;
    break;
  case 6:
    if (DAT_20000f14 != 0) {
      return (uint)DAT_20000f14;
    }
    if (cRam20000f15 == '\0') {
      return 0;
    }
    cRam20000f15 = cRam20000f15 + -1;
    uRam20002d53 = 0x29;
    break;
  default:
    return (uint)DAT_20001060;
  case 9:
    uVar2 = (uint)DAT_20001061;
    if (uVar2 == 2) {
      uVar2 = (uint)DAT_20001063;
      if ((uVar2 & 0xf0) == 0xd0) {
        DAT_20001063 = DAT_20001063 & 0xf | 0xc0;
      }
      else {
        if (uVar2 < 0x40) {
          return uVar2;
        }
        DAT_20001063 = DAT_20001063 - 0x40;
      }
    }
    else {
      if (uVar2 != 1) {
        return uVar2;
      }
      if ((DAT_20001062 & 0xf) != 7) {
        return DAT_20001062 & 0xf;
      }
      if (DAT_20001063 < 5) {
        return (uint)DAT_20001063;
      }
      DAT_20001063 = DAT_20001063 - 4;
    }
    uRam20002d53 = 0x14;
  }
  iVar5 = _DAT_20002d6c;
  bVar14 = false;
  if (_DAT_20002d6c == 0) {
    bVar14 = (bool)isCurrentModePrivileged();
    if (bVar14) {
      setBasePriority(0x10);
    }
    InstructionSynchronizationBarrier(0xf);
    DataSynchronizationBarrier(0xf);
    do {
                    /* WARNING: Do nothing block with infinite loop */
    } while( true );
  }
  iVar4 = FUN_0803b730();
  if (iVar4 != 0) {
    while( true ) {
      FUN_0803a168();
      if (*(uint *)(iVar5 + 0x38) < *(uint *)(iVar5 + 0x3c)) {
        iVar4 = FUN_08034cc4(iVar5,&uRam20002d53,0);
        if (*(int *)(iVar5 + 0x24) == 0) {
          if (iVar4 != 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
        else {
          iVar5 = FUN_0803bb4c(iVar5 + 0x24);
          if (iVar5 != 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
        FUN_0803a1b0();
        return 1;
      }
      if (!bVar14) {
        FUN_0803a400(&uStack_3c);
        bVar14 = true;
      }
      FUN_0803a1b0();
      FUN_0803a904();
      FUN_0803a168();
      if (*(char *)(iVar5 + 0x44) == -1) {
        *(undefined1 *)(iVar5 + 0x44) = 0;
      }
      if (*(char *)(iVar5 + 0x45) == -1) {
        *(undefined1 *)(iVar5 + 0x45) = 0;
      }
      FUN_0803a1b0();
      iVar4 = FUN_0803b5cc(&uStack_3c,&stack0xffffffd8);
      if (iVar4 != 0) break;
      iVar4 = FUN_080352d4(iVar5);
      if (iVar4 == 0) {
        FUN_080357b8(iVar5);
        FUN_0803bc10();
      }
      else {
        FUN_0803a434(iVar5 + 0x10,0xffffffff);
        FUN_080357b8(iVar5);
        iVar4 = FUN_0803bc10();
        if (iVar4 == 0) {
          _DAT_e000ed04 = 0x10000000;
          DataSynchronizationBarrier(0xf);
          InstructionSynchronizationBarrier(0xf);
        }
      }
    }
    FUN_080357b8(iVar5);
    FUN_0803bc10();
    return 0;
  }
  bVar14 = (bool)isCurrentModePrivileged();
  if (bVar14) {
    setBasePriority(0x10);
  }
  InstructionSynchronizationBarrier(0xf);
  DataSynchronizationBarrier(0xf);
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}



// ==========================================
// Function: FUN_080047cc @ 080047cc
// Size: 1 bytes
// ==========================================

/* WARNING: Removing unreachable block (ram,0x0803ad26) */
/* WARNING: Removing unreachable block (ram,0x0803ad30) */
/* WARNING: Removing unreachable block (ram,0x0803ad32) */
/* WARNING: Removing unreachable block (ram,0x0803ad58) */
/* WARNING: Removing unreachable block (ram,0x0803ad62) */
/* WARNING: Removing unreachable block (ram,0x0803ad64) */
/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x0801f076 */
/* WARNING: Removing unreachable block (ram,0x0803ad6a) */
/* WARNING: Removing unreachable block (ram,0x0803ad70) */
/* WARNING: Removing unreachable block (ram,0x0803ad74) */
/* WARNING: Removing unreachable block (ram,0x0803ad80) */
/* WARNING: Removing unreachable block (ram,0x0803ae3a) */
/* WARNING: Removing unreachable block (ram,0x0803ad38) */
/* WARNING: Removing unreachable block (ram,0x0803ad3e) */
/* WARNING: Removing unreachable block (ram,0x0803ad42) */
/* WARNING: Removing unreachable block (ram,0x0803ad4e) */
/* WARNING: Removing unreachable block (ram,0x0803adcc) */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

uint FUN_080047cc(undefined4 param_1,undefined4 param_2)

{
  bool bVar1;
  uint uVar2;
  ushort *puVar3;
  undefined4 uVar4;
  int iVar5;
  int iVar6;
  ushort *puVar7;
  undefined1 *puVar8;
  undefined4 uVar9;
  char cVar10;
  byte bVar11;
  uint uVar12;
  uint uVar13;
  byte bVar14;
  uint in_fpscr;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  undefined4 extraout_s1_01;
  undefined4 extraout_s1_02;
  undefined4 extraout_s1_03;
  undefined4 extraout_s1_04;
  undefined4 extraout_s1_05;
  undefined4 extraout_s1_06;
  undefined4 extraout_s1_07;
  undefined4 extraout_s1_08;
  undefined4 extraout_s1_09;
  undefined4 extraout_s1_10;
  float fVar15;
  float fVar16;
  float fVar17;
  undefined4 uVar18;
  undefined4 uVar19;
  undefined8 uVar20;
  undefined8 uVar21;
  undefined4 uStack_3c;
  
  uVar2 = (uint)DAT_20001060;
  switch(uVar2) {
  case 0:
    bVar11 = 0;
    if (DAT_20001061 < 3) {
      bVar11 = DAT_20001061 + 1;
    }
    DAT_20001061 = bVar11;
    uRam20002d53 = 0xb;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0xc;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0xf;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x10;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x11;
    break;
  case 1:
    if ((bRam20001055 & 0xf0) == 0xb0) {
      return 0xb0;
    }
    bVar11 = 0;
    if (DAT_20001025 < 7) {
      bVar11 = DAT_20001025 + 1;
    }
    DAT_20001025 = bVar11;
    sRam20002d54 = *(byte *)(DAT_20001025 + 0x80bb3fc) + 0x500;
    bRam20001055 = 0;
    FUN_0803acf0(uRam20002d74,&sRam20002d54,0xffffffff);
    uRam20002d53 = 0x1d;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x1b;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    DAT_2000102e = DAT_20001025;
    if (DAT_20001025 != 2) {
      DAT_2000102e = 1;
    }
    uRam20001040 = 0x7fc00000;
    uRam20001044 = 0x7fc00000;
    uRam20001048 = 0x7fc00000;
    if (DAT_20001034 != '\0') {
      DAT_20001034 = '\0';
    }
    if (DAT_20001035 != '\0') {
      DAT_20001035 = '\0';
      uRam20002d53 = 0x1a;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    }
    if (DAT_20001035 != '\0') {
      uVar20 = FUN_0803ed70(_DAT_20001038);
      uVar4 = FUN_0803e5da(DAT_2000103c);
      uVar18 = (undefined4)DAT_08002bf0;
      uVar19 = (undefined4)((ulonglong)DAT_08002bf0 >> 0x20);
      uVar4 = FUN_0803c8e0(uVar18,param_2,uVar4);
      uVar20 = FUN_0803e77c(uVar4,extraout_s1,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
      uVar21 = FUN_0803ed70(_DAT_20001028);
      uVar4 = FUN_0803e5da(DAT_2000102f);
      uVar4 = FUN_0803c8e0(uVar18,extraout_s1,uVar4);
      uVar21 = FUN_0803e77c(uVar4,extraout_s1_00,(int)uVar21,(int)((ulonglong)uVar21 >> 0x20));
      uVar20 = FUN_0803eb94((int)uVar21,(int)((ulonglong)uVar21 >> 0x20),(int)uVar20,
                            (int)((ulonglong)uVar20 >> 0x20));
      uVar2 = (uint)((ulonglong)uVar20 >> 0x20);
      iVar6 = (int)((longlong)DAT_08002bf8 >> 0x3f);
      uVar4 = (undefined4)((ulonglong)DAT_08002c00 >> 0x20);
      uVar9 = (undefined4)DAT_08002c00;
      uVar12 = uVar2 & 0x7fffffff | iVar6 * -0x80000000;
      DAT_2000102f = '\0';
      iVar5 = FUN_0803ee0c((int)uVar20,uVar12,uVar9,uVar4);
      cVar10 = iVar5 == 0;
      if ((bool)cVar10) {
        DAT_2000102f = '\x01';
        uVar20 = FUN_0803e124((int)uVar20,uVar2,uVar18,uVar19);
        uVar12 = (uint)((ulonglong)uVar20 >> 0x20) & 0x7fffffff | iVar6 * -0x80000000;
      }
      iVar5 = FUN_0803ee0c((int)uVar20,uVar12,uVar9,uVar4);
      if (iVar5 == 0) {
        cVar10 = cVar10 + '\x01';
        DAT_2000102f = cVar10;
        uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar18,uVar19);
        uVar12 = (uint)((ulonglong)uVar20 >> 0x20) & 0x7fffffff | iVar6 * -0x80000000;
      }
      iVar5 = FUN_0803ee0c((int)uVar20,uVar12,uVar9,uVar4);
      if (iVar5 == 0) {
        cVar10 = cVar10 + '\x01';
        DAT_2000102f = cVar10;
        uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar18,uVar19);
        uVar12 = (uint)((ulonglong)uVar20 >> 0x20) & 0x7fffffff | iVar6 * -0x80000000;
      }
      iVar6 = FUN_0803ee0c((int)uVar20,uVar12,uVar9,uVar4);
      if (iVar6 == 0) {
        DAT_2000102f = cVar10 + '\x01';
        uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar18,uVar19);
      }
      _DAT_20001028 = (float)FUN_0803df48((int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
      fVar15 = ABS(_DAT_20001028);
      if ((fVar15 < DAT_08002c08) || (DAT_2000102c < 2)) {
        if ((fVar15 < DAT_08002c0c) || (DAT_2000102c < 3)) {
          if ((10.0 <= fVar15) && (3 < DAT_2000102c)) {
            DAT_2000102c = 3;
          }
        }
        else {
          DAT_2000102c = 2;
        }
      }
      else {
        DAT_2000102c = 1;
      }
    }
    switch(DAT_20001025) {
    case 0:
      bVar11 = 5;
      switch(DAT_20001027) {
      case 0:
        DAT_20001026 = 0;
        DAT_20001030 = -1;
        break;
      case 1:
        DAT_20001030 = DAT_2000102f;
        if (DAT_2000102e - 1 < 2) {
          DAT_20001026 = DAT_2000102e;
        }
        break;
      case 2:
        if (DAT_2000102e == 2) {
          bVar11 = 3;
          goto code_r0x08002b98;
        }
        goto code_r0x08002b9c;
      case 3:
code_r0x08002b98:
        DAT_20001026 = bVar11;
code_r0x08002b9c:
        DAT_20001030 = DAT_2000102f + '\x02';
      }
      uStack_3c = CONCAT13(0x1b,(undefined3)uStack_3c);
      FUN_0803acf0(_DAT_20002d6c,(int)&uStack_3c + 3,0xffffffff);
      break;
    case 1:
      if (DAT_2000102e - 1 < 2) {
        DAT_20001026 = DAT_2000102e;
      }
      DAT_20001030 = DAT_2000102f;
      break;
    case 2:
      if (DAT_2000102e == 1) {
        DAT_20001026 = 4;
        DAT_20001030 = DAT_2000102f;
        break;
      }
      if (DAT_2000102e != 2) break;
      DAT_20001026 = 3;
      goto code_r0x08002b10;
    case 3:
      DAT_20001026 = 5;
code_r0x08002b10:
      DAT_20001030 = DAT_2000102f + '\x02';
      break;
    case 4:
      DAT_20001026 = 6;
      DAT_20001030 = DAT_2000102f + '\x05';
      break;
    case 5:
      DAT_20001026 = 7;
      DAT_20001030 = DAT_2000102f + '\t';
      break;
    case 6:
      if (DAT_2000102e == 2) {
        DAT_20001026 = 9;
      }
      else if (DAT_2000102e == 1) {
        DAT_20001026 = 8;
      }
      goto code_r0x08002b6e;
    case 7:
      if (DAT_2000102e == 2) {
        DAT_20001026 = 0xb;
      }
      else if (DAT_2000102e == 1) {
        DAT_20001026 = 10;
      }
code_r0x08002b6e:
      DAT_20001030 = DAT_2000102f + '\n';
    }
    uStack_3c = CONCAT13(0x1c,(undefined3)uStack_3c);
    FUN_0803acf0(_DAT_20002d6c,(int)&uStack_3c + 3,0xffffffff);
    uStack_3c = CONCAT13(0x1e,(undefined3)uStack_3c);
    uVar2 = FUN_0803acf0(_DAT_20002d6c,(int)&uStack_3c + 3,0xffffffff);
    return uVar2;
  case 2:
    uVar2 = (uint)bRam2000044c;
    if ((uVar2 & 0xf) - 1 < 2) {
      FUN_080342f8(1,0);
      goto code_r0x08004d50;
    }
    if ((uVar2 & 0xf) == 3) {
      if (DAT_20000332 == 3) {
        if ((int)(uVar2 << 0x18) < 0) goto code_r0x08004b5e;
        if (0x95 < _DAT_2000032a) {
          return (int)_DAT_2000032a;
        }
        _DAT_2000032a = _DAT_2000032a + 1;
      }
      else if (DAT_20000332 == 2) {
        if (99 < _DAT_20000330) {
          return (int)_DAT_20000330;
        }
        _DAT_20000330 = _DAT_20000330 + 1;
      }
      else {
        if (DAT_20000332 != 1) {
          return uVar2;
        }
code_r0x08004b5e:
        uVar2 = (uint)_DAT_2000032c;
        if (0x95 < (int)uVar2) {
code_r0x08004b7c:
          return uVar2;
        }
        _DAT_2000032c = _DAT_2000032c + 1;
      }
      puVar8 = (undefined1 *)0x0;
      uVar9 = 0;
      uVar4 = uRam20002d80;
    }
    else {
      if ((bRam2000044c & 0xf) != 0) {
        return uVar2;
      }
      if (DAT_20000125 == 0) {
        return 0;
      }
      bVar11 = DAT_20000125 - 1;
      uVar2 = (uint)DAT_200000fa;
      if ((byte)(DAT_20000125 - 1) < 5) {
        if (((byte)(DAT_20000125 - 1) == 4) || (DAT_2000010c == '\x03')) {
          puVar3 = (ushort *)(&DAT_200003a8 + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_2000036c + uVar2 * 2);
        }
        else {
          puVar3 = (ushort *)(&DAT_200003bc + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_20000380 + uVar2 * 2);
        }
      }
      else {
        puVar3 = (ushort *)(&DAT_20000394 + uVar2 * 2);
        puVar7 = (ushort *)(&DAT_20000358 + uVar2 * 2);
      }
      fVar15 = (float)VectorSignedToFloat((uint)*puVar3 - (uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3
                                         );
      fVar16 = (float)VectorSignedToFloat(DAT_200000fc + 100,(byte)(in_fpscr >> 0x15) & 3);
      fVar17 = (float)VectorUnsignedToFloat((uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3);
      uVar2 = VectorFloatToUnsigned((fVar15 / fRam08004d5c) * fVar16 + fVar17,3);
      _DAT_40007408 = uVar2 & 0xfff | _DAT_40007408 & 0xfffff000;
      _DAT_40007404 = _DAT_40007404 | 1;
      uVar2 = (uint)DAT_200000fb;
      if ((byte)(DAT_20000125 - 1) < 5) {
        if (((byte)(DAT_20000125 - 1) == 4) || (DAT_2000010c == '\x03')) {
          puVar3 = (ushort *)(&DAT_20000420 + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_200003e4 + uVar2 * 2);
        }
        else {
          puVar3 = (ushort *)(&DAT_20000434 + uVar2 * 2);
          puVar7 = (ushort *)(&DAT_200003f8 + uVar2 * 2);
        }
      }
      else {
        puVar3 = (ushort *)(&DAT_2000040c + uVar2 * 2);
        puVar7 = (ushort *)(&DAT_200003d0 + uVar2 * 2);
      }
      fVar15 = (float)VectorSignedToFloat((uint)*puVar3 - (uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3
                                         );
      fVar16 = (float)VectorSignedToFloat(DAT_200000fd + 100,(byte)(in_fpscr >> 0x15) & 3);
      fVar17 = (float)VectorUnsignedToFloat((uint)*puVar7,(byte)(in_fpscr >> 0x15) & 3);
      _DAT_40001c34 = VectorFloatToUnsigned((fVar15 / fRam08004d5c) * fVar16 + fVar17,3);
      if (DAT_20000126 != '\0') {
        if ((byte)(DAT_20000125 - 1) < 0x14) {
          uRam40000400 = uRam40000400 & 0xfffffffe;
          if (DAT_2000010f == '\x01') {
            _DAT_20000eac = 0x12d0000;
          }
          else {
            _DAT_20000eac = 0x12d;
          }
        }
        else {
          _DAT_20000eac = 0x12d;
          DAT_20000125 = bVar11;
          FUN_0802a430(&stack0xffffffd8);
          if ((byte)(DAT_20000125 - 0x14) < 9) {
            uRam4000042c = *(undefined4 *)((char)(DAT_20000125 - 0x14) * 4 + 0x80465a4);
            uRam40000428 = 0xffffffff;
            uRam40000414 = uRam40000414 | 1;
          }
          uRam40000424 = 0;
          uRam40000400 = uRam40000400 | 1;
          bVar11 = DAT_20000125;
        }
      }
      DAT_20000125 = bVar11;
      FUN_08029a70();
      FUN_080342f8(0,0);
      _DAT_40011410 = 4;
      uRam20002d53 = 2;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      puVar8 = &uRam20002d52;
      uRam20002d52 = 1;
      uVar9 = 0xffffffff;
      uVar4 = _DAT_20002d78;
    }
    FUN_0803acf0(uVar4,puVar8,uVar9);
code_r0x08004d50:
    uVar2 = (uint)DAT_2000012c;
    if (uVar2 == 0) {
      if ((DAT_20000332 & 1) != 0) {
        _DAT_2000033c =
             (float)VectorSignedToFloat((int)_DAT_2000032a - (int)_DAT_20000112,
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000334 = _DAT_2000033c / 25.0;
        uVar12 = DAT_20000125 / 3 + 1;
        _DAT_20000338 =
             (float)VectorSignedToFloat((int)_DAT_2000032c - (int)_DAT_20000112,
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_2000033c = _DAT_20000338 - _DAT_2000033c;
        uVar2 = uVar12 / 3;
        DAT_20000350 = (char)uVar2;
        uVar20 = FUN_0803e5da((&DAT_080465c8)[(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
        uVar4 = FUN_0803e50a(uVar12 - (uVar2 * 3 & 0xff));
        uVar4 = FUN_0803c8e0((int)DAT_0801f3b0,param_2,uVar4);
        uVar20 = FUN_0803e77c(uVar4,extraout_s1_01,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        uVar21 = FUN_0803ed70(_DAT_20000334);
        FUN_0803e77c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                     (int)((ulonglong)uVar21 >> 0x20));
        _DAT_20000334 = (float)FUN_0803df48();
        fVar15 = DAT_0801f3b8;
        uVar2 = in_fpscr & 0xfffffff | (uint)(ABS(_DAT_20000334) < DAT_0801f3b8) << 0x1f;
        uVar12 = uVar2 | (uint)(NAN(ABS(_DAT_20000334)) || NAN(DAT_0801f3b8)) << 0x1c;
        if ((byte)(uVar2 >> 0x1f) == ((byte)(uVar12 >> 0x1c) & 1)) {
          _DAT_20000334 = _DAT_20000334 / DAT_0801f3b8;
          DAT_20000350 = DAT_20000350 + '\x01';
        }
        _DAT_20000338 = _DAT_20000338 / 25.0;
        uVar2 = DAT_20000125 / 3 + 1;
        uVar13 = uVar2 / 3;
        DAT_20000351 = (char)uVar13;
        uVar20 = FUN_0803e5da((&DAT_080465c8)[(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
        uVar4 = FUN_0803e50a(uVar2 - (uVar13 * 3 & 0xff));
        uVar4 = FUN_0803c8e0((int)DAT_0801f3b0,extraout_s1_01,uVar4);
        uVar20 = FUN_0803e77c(uVar4,extraout_s1_02,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        uVar21 = FUN_0803ed70(_DAT_20000338);
        FUN_0803e77c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                     (int)((ulonglong)uVar21 >> 0x20));
        _DAT_20000338 = (float)FUN_0803df48();
        uVar2 = uVar12 & 0xfffffff | (uint)(ABS(_DAT_20000338) < fVar15) << 0x1f;
        uVar12 = uVar2 | (uint)(NAN(ABS(_DAT_20000338)) || NAN(fVar15)) << 0x1c;
        if ((byte)(uVar2 >> 0x1f) == ((byte)(uVar12 >> 0x1c) & 1)) {
          _DAT_20000338 = _DAT_20000338 / fVar15;
          DAT_20000351 = DAT_20000351 + '\x01';
        }
        _DAT_2000033c = _DAT_2000033c / 25.0;
        uVar2 = DAT_20000125 / 3 + 1;
        uVar13 = uVar2 / 3;
        DAT_20000352 = (char)uVar13;
        uVar20 = FUN_0803e5da((&DAT_080465c8)[(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
        uVar4 = FUN_0803e50a(uVar2 - (uVar13 * 3 & 0xff));
        uVar4 = FUN_0803c8e0((int)DAT_0801f3b0,extraout_s1_02,uVar4);
        uVar20 = FUN_0803e77c(uVar4,extraout_s1_03,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        uVar21 = FUN_0803ed70(_DAT_2000033c);
        FUN_0803e77c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                     (int)((ulonglong)uVar21 >> 0x20));
        _DAT_2000033c = (float)FUN_0803df48();
        if (!NAN(ABS(_DAT_2000033c)) && !NAN(fVar15)) {
          _DAT_2000033c = _DAT_2000033c / fVar15;
          DAT_20000352 = DAT_20000352 + '\x01';
        }
        in_fpscr = uVar12 & 0xfffffff | (uint)(_DAT_2000033c == 0.0) << 0x1e;
        param_2 = extraout_s1_03;
        if ((byte)(in_fpscr >> 0x1e) == 0) {
          fVar15 = ABS(_DAT_2000033c);
          uVar20 = DAT_0801f3c8;
          if (DAT_20000352 != '\0') {
            uVar4 = FUN_0803e5da();
            uVar4 = FUN_0803c8e0((int)DAT_0801f3c0,extraout_s1_03,uVar4);
            uVar20 = FUN_0803e124((int)DAT_0801f3c8,(int)((ulonglong)DAT_0801f3c8 >> 0x20),uVar4,
                                  extraout_s1_04);
            param_2 = extraout_s1_04;
          }
          uVar21 = FUN_0803ed70(fVar15);
          uVar20 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),(int)uVar21,
                                (int)((ulonglong)uVar21 >> 0x20));
          uVar4 = (undefined4)((ulonglong)DAT_0801f3c0 >> 0x20);
          uVar9 = (undefined4)DAT_0801f3c0;
          DAT_20000356 = 0;
          iVar6 = FUN_0803ee0c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar9,uVar4);
          bVar11 = DAT_20000356;
          if (iVar6 == 0) {
            bVar11 = 0;
            do {
              uStack_3c = (uint)(byte)(bVar11 + 1);
              uVar21 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar9,uVar4);
              uVar20 = FUN_0803e124((int)uVar21,(int)((ulonglong)uVar21 >> 0x20),uVar9,uVar4);
              if (uStack_3c < 3) {
                bVar11 = bVar11 + 1;
                uVar20 = uVar21;
              }
              uVar18 = (undefined4)((ulonglong)uVar20 >> 0x20);
              iVar6 = FUN_0803edf0((int)uVar20,uVar18,uVar9,uVar4);
              if (iVar6 == 0) break;
              uStack_3c = (uint)(byte)(bVar11 + 1);
              uVar21 = FUN_0803e124((int)uVar20,uVar18,uVar9,uVar4);
              uVar20 = FUN_0803e124((int)uVar21,(int)((ulonglong)uVar21 >> 0x20),uVar9,uVar4);
              if (uStack_3c < 3) {
                bVar11 = bVar11 + 1;
                uVar20 = uVar21;
              }
              uVar18 = (undefined4)((ulonglong)uVar20 >> 0x20);
              iVar6 = FUN_0803ee0c((int)uVar20,uVar18,uVar9,uVar4);
              if (iVar6 != 0) break;
              bVar14 = bVar11 + 1;
              uStack_3c = (uint)bVar14;
              uVar20 = FUN_0803e124((int)uVar20,uVar18,uVar9,uVar4);
              uVar21 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar9,uVar4);
              if (2 < uStack_3c) {
                bVar14 = bVar11;
                uVar20 = uVar21;
              }
              uVar18 = (undefined4)((ulonglong)uVar20 >> 0x20);
              iVar6 = FUN_0803ee0c((int)uVar20,uVar18,uVar9,uVar4);
              bVar11 = bVar14;
              if (iVar6 != 0) break;
              bVar11 = bVar14 + 1;
              uVar20 = FUN_0803e124((int)uVar20,uVar18,uVar9,uVar4);
              uVar21 = FUN_0803e124((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar9,uVar4);
              if (2 < bVar11) {
                bVar11 = bVar14;
                uVar20 = uVar21;
              }
              iVar6 = FUN_0803ee0c((int)uVar20,(int)((ulonglong)uVar20 >> 0x20),uVar9,uVar4);
            } while (iVar6 == 0);
          }
          DAT_20000356 = bVar11;
          _DAT_2000034c = FUN_0803df48((int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        }
        else {
          _DAT_2000034c = 0;
          DAT_20000356 = 0;
        }
      }
      uVar2 = (uint)DAT_20000332 << 0x1e;
      if ((int)uVar2 < 0) {
        uVar2 = (uint)DAT_2000010e;
        _DAT_20000340 =
             (float)VectorSignedToFloat((int)_DAT_2000032e - (int)(char)(&DAT_200000fc)[uVar2],
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000344 =
             (float)VectorSignedToFloat((int)_DAT_20000330 - (int)(char)(&DAT_200000fc)[uVar2],
                                        (byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000348 = _DAT_20000340 - _DAT_20000344;
        fVar15 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                        [(uint)(byte)(&DAT_200000fa)[uVar2] % 3] <<
                                            2,(byte)(in_fpscr >> 0x15) & 3);
        _DAT_20000340 = _DAT_20000340 * fVar15;
        if ((byte)(&DAT_200000fa)[uVar2] < 3) {
          _DAT_20000340 = _DAT_20000340 / 10.0;
        }
        else {
          uVar4 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar2] - 3) / 3));
          uVar4 = FUN_0803c8e0((int)DAT_0801f6e8,param_2,uVar4);
          uVar20 = FUN_0803ed70(_DAT_20000340);
          FUN_0803e77c(uVar4,extraout_s1_05,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
          _DAT_20000340 = (float)FUN_0803df48();
          uVar2 = (uint)DAT_2000010e;
          param_2 = extraout_s1_05;
        }
        uVar4 = FUN_0803e5da(*(undefined1 *)(uVar2 + 0x200000fe));
        uVar4 = FUN_0803c8e0((int)DAT_0801f6e8,param_2,uVar4);
        uVar20 = FUN_0803ed70(_DAT_20000340);
        FUN_0803e77c(uVar4,extraout_s1_06,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        _DAT_20000340 = (float)FUN_0803df48();
        fVar15 = DAT_0801f6f0;
        uVar2 = in_fpscr & 0xfffffff | (uint)(ABS(_DAT_20000340) < DAT_0801f6f0) << 0x1f;
        uVar12 = uVar2 | (uint)(NAN(ABS(_DAT_20000340)) || NAN(DAT_0801f6f0)) << 0x1c;
        DAT_20000353 = (byte)(uVar2 >> 0x1f) == ((byte)(uVar12 >> 0x1c) & 1);
        if ((bool)DAT_20000353) {
          _DAT_20000340 = _DAT_20000340 / DAT_0801f6f0;
        }
        uVar2 = (uint)DAT_2000010e;
        fVar16 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                        [(uint)(byte)(&DAT_200000fa)[uVar2] % 3] <<
                                            2,(byte)(uVar12 >> 0x15) & 3);
        _DAT_20000344 = _DAT_20000344 * fVar16;
        if ((byte)(&DAT_200000fa)[uVar2] < 3) {
          _DAT_20000344 = _DAT_20000344 / 10.0;
          uVar4 = extraout_s1_06;
        }
        else {
          uVar4 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar2] - 3) / 3));
          uVar4 = FUN_0803c8e0((int)DAT_0801f6e8,extraout_s1_06,uVar4);
          uVar20 = FUN_0803ed70(_DAT_20000344);
          FUN_0803e77c(uVar4,extraout_s1_07,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
          _DAT_20000344 = (float)FUN_0803df48();
          uVar2 = (uint)DAT_2000010e;
          uVar4 = extraout_s1_07;
        }
        uVar9 = FUN_0803e5da(*(undefined1 *)(uVar2 + 0x200000fe));
        uVar4 = FUN_0803c8e0((int)DAT_0801f6e8,uVar4,uVar9);
        uVar20 = FUN_0803ed70(_DAT_20000344);
        FUN_0803e77c(uVar4,extraout_s1_08,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        _DAT_20000344 = (float)FUN_0803df48();
        uVar2 = uVar12 & 0xfffffff | (uint)(ABS(_DAT_20000344) < fVar15) << 0x1f;
        DAT_20000354 = SUB41(uVar2 >> 0x1f,0) == (NAN(ABS(_DAT_20000344)) || NAN(fVar15));
        if ((bool)DAT_20000354) {
          _DAT_20000344 = _DAT_20000344 / fVar15;
        }
        uVar12 = (uint)DAT_2000010e;
        fVar16 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                        [(uint)(byte)(&DAT_200000fa)[uVar12] % 3] <<
                                            2,(byte)(uVar2 >> 0x15) & 3);
        _DAT_20000348 = _DAT_20000348 * fVar16;
        if ((byte)(&DAT_200000fa)[uVar12] < 3) {
          _DAT_20000348 = _DAT_20000348 / 10.0;
          uVar4 = extraout_s1_08;
        }
        else {
          uVar4 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar12] - 3) / 3));
          uVar4 = FUN_0803c8e0((int)DAT_0801f6e8,extraout_s1_08,uVar4);
          uVar20 = FUN_0803ed70(_DAT_20000348);
          FUN_0803e77c(uVar4,extraout_s1_09,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
          _DAT_20000348 = (float)FUN_0803df48();
          uVar12 = (uint)DAT_2000010e;
          uVar4 = extraout_s1_09;
        }
        uVar9 = FUN_0803e5da(*(undefined1 *)(uVar12 + 0x200000fe));
        uVar4 = FUN_0803c8e0((int)DAT_0801f6e8,uVar4,uVar9);
        uVar20 = FUN_0803ed70(_DAT_20000348);
        FUN_0803e77c(uVar4,extraout_s1_10,(int)uVar20,(int)((ulonglong)uVar20 >> 0x20));
        _DAT_20000348 = (float)FUN_0803df48();
        uVar2 = 0;
        DAT_20000355 = 0;
        if (fVar15 <= ABS(_DAT_20000348)) {
          _DAT_20000348 = _DAT_20000348 / fVar15;
          uVar2 = 1;
          DAT_20000355 = 1;
        }
      }
    }
    return uVar2;
  case 3:
    if (DAT_20001061 != 2) {
      return (uint)DAT_20001061;
    }
    if (DAT_20001063 < 0x10) {
      return (uint)DAT_20001063;
    }
    DAT_20001063 = DAT_20001063 - 0x10;
    uRam20002d53 = 0x19;
    break;
  case 4:
    if (DAT_20001061 != 1) {
      return (uint)DAT_20001061;
    }
    if ((DAT_20001062 & 0xf) != 1) {
      return DAT_20001062 & 0xf;
    }
    if (DAT_20001063 == 0) {
      if (9 < bRam2000105a) {
        return (uint)bRam2000105a;
      }
      bRam2000105a = bRam2000105a + 1;
    }
    else {
      uVar2 = (uint)bRam20001059;
      if (99 < uVar2) {
        return uVar2;
      }
      iRam40015034 = uVar2 + 5;
      bRam20001059 = (byte)iRam40015034;
    }
    uRam20002d53 = 0x21;
    break;
  case 5:
    if (DAT_20000f14 != 0) {
      return (uint)DAT_20000f14;
    }
    uVar2 = bRam20000f15 + 1;
    if (DAT_20000f13 <= uVar2) {
      return uVar2;
    }
    bRam20000f15 = (byte)uVar2;
    uRam20002d53 = 0x27;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    uRam20002d53 = 0x28;
    break;
  case 6:
    if (DAT_20000f14 != 0) {
      return (uint)DAT_20000f14;
    }
    uVar2 = bRam20000f15 + 1;
    if (DAT_20000f13 <= uVar2) {
      return uVar2;
    }
    bRam20000f15 = (byte)uVar2;
    uRam20002d53 = 0x29;
    break;
  default:
    goto code_r0x08004b7c;
  case 9:
    uVar2 = (uint)DAT_20001061;
    if (uVar2 == 2) {
      if ((DAT_20001063 & 0xf0) == 0xc0) {
        DAT_20001063 = DAT_20001063 & 0xf | 0xd0;
      }
      else {
        if ((char)DAT_20001063 < '\0') {
          return (uint)DAT_20001063;
        }
        DAT_20001063 = DAT_20001063 + 0x40;
      }
    }
    else {
      if (uVar2 != 1) {
        return uVar2;
      }
      if ((DAT_20001062 & 0xf) != 7) {
        return DAT_20001062 & 0xf;
      }
      if (3 < DAT_20001063 - 1) {
        return (uint)DAT_20001063;
      }
      DAT_20001063 = DAT_20001063 + 4;
    }
    uRam20002d53 = 0x14;
  }
  iVar6 = _DAT_20002d6c;
  bVar1 = false;
  if (_DAT_20002d6c == 0) {
    bVar1 = (bool)isCurrentModePrivileged();
    if (bVar1) {
      setBasePriority(0x10);
    }
    InstructionSynchronizationBarrier(0xf);
    DataSynchronizationBarrier(0xf);
    do {
                    /* WARNING: Do nothing block with infinite loop */
    } while( true );
  }
  iVar5 = FUN_0803b730();
  if (iVar5 != 0) {
    while( true ) {
      FUN_0803a168();
      if (*(uint *)(iVar6 + 0x38) < *(uint *)(iVar6 + 0x3c)) {
        iVar5 = FUN_08034cc4(iVar6,&uRam20002d53,0);
        if (*(int *)(iVar6 + 0x24) == 0) {
          if (iVar5 != 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
        else {
          iVar6 = FUN_0803bb4c(iVar6 + 0x24);
          if (iVar6 != 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
        FUN_0803a1b0();
        return 1;
      }
      if (!bVar1) {
        FUN_0803a400(&uStack_3c);
        bVar1 = true;
      }
      FUN_0803a1b0();
      FUN_0803a904();
      FUN_0803a168();
      if (*(char *)(iVar6 + 0x44) == -1) {
        *(undefined1 *)(iVar6 + 0x44) = 0;
      }
      if (*(char *)(iVar6 + 0x45) == -1) {
        *(undefined1 *)(iVar6 + 0x45) = 0;
      }
      FUN_0803a1b0();
      iVar5 = FUN_0803b5cc(&uStack_3c,&stack0xffffffd8);
      if (iVar5 != 0) break;
      iVar5 = FUN_080352d4(iVar6);
      if (iVar5 == 0) {
        FUN_080357b8(iVar6);
        FUN_0803bc10();
      }
      else {
        FUN_0803a434(iVar6 + 0x10,0xffffffff);
        FUN_080357b8(iVar6);
        iVar5 = FUN_0803bc10();
        if (iVar5 == 0) {
          _DAT_e000ed04 = 0x10000000;
          DataSynchronizationBarrier(0xf);
          InstructionSynchronizationBarrier(0xf);
        }
      }
    }
    FUN_080357b8(iVar6);
    FUN_0803bc10();
    return 0;
  }
  bVar1 = (bool)isCurrentModePrivileged();
  if (bVar1) {
    setBasePriority(0x10);
  }
  InstructionSynchronizationBarrier(0xf);
  DataSynchronizationBarrier(0xf);
  do {
                    /* WARNING: Do nothing block with infinite loop */
  } while( true );
}



