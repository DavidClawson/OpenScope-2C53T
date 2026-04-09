// Force-decompiled functions

// Attempting to create function at 0x08006060
// Successfully created function
// ==========================================
// Function: FUN_08006060 @ 08006060
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

undefined4 FUN_08006060(undefined4 param_1,undefined4 param_2,undefined4 param_3,int param_4)

{
  bool bVar1;
  undefined4 uVar2;
  int iVar3;
  int iVar4;
  bool in_ZR;
  bool in_CY;
  undefined1 auStack_2c [8];
  int iStack_24;
  int iStack_20;
  int iStack_1c;
  int iStack_18;
  undefined4 uStack_14;
  int iStack_10;
  
  if (!in_CY || in_ZR) {
                    /* WARNING: Could not recover jumptable at 0x08006066. Too many branches */
                    /* WARNING: Treating indirect jump as call */
    uVar2 = (*(code *)((uint)*(byte *)(param_4 + 0x800606a) * 2 + 0x800606a))(param_1,1,1);
    return uVar2;
  }
  uRam20002d54 = 0x501;
  FUN_0803acf0(uRam20002d74,0x20002d54,0xffffffff);
  uRam20002d53 = 0x1d;
  FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
  iVar4 = _DAT_20002d6c;
  uRam20002d53 = 0x1b;
  iStack_10 = _DAT_20002d6c;
  uStack_14 = 0x20002d53;
  iStack_18 = -1;
  iStack_1c = 0;
  iStack_20 = 0;
  if (_DAT_20002d6c != 0) {
    iVar3 = FUN_0803b730();
    if (iVar3 == 0 && iStack_18 != 0) {
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
    while( true ) {
      FUN_0803a168();
      if ((*(uint *)(iVar4 + 0x38) < *(uint *)(iVar4 + 0x3c)) || (iStack_1c == 2)) {
        iStack_24 = FUN_08034cc4(iVar4,uStack_14,iStack_1c);
        if (*(int *)(iVar4 + 0x24) == 0) {
          if (iStack_24 != 0) {
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
      if (iStack_18 == 0) {
        FUN_0803a1b0();
        return 0;
      }
      if (iStack_20 == 0) {
        FUN_0803a400(auStack_2c);
        iStack_20 = 1;
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
      iVar3 = FUN_0803b5cc(auStack_2c,&iStack_18);
      if (iVar3 != 0) break;
      iVar3 = FUN_080352d4(iVar4);
      if (iVar3 == 0) {
        FUN_080357b8(iVar4);
        FUN_0803bc10();
      }
      else {
        FUN_0803a434(iVar4 + 0x10,iStack_18);
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
}



// ==========================================
// Function: FUN_08006120 @ 08006120
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

uint FUN_08006120(void)

{
  bool bVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  char cVar5;
  undefined1 auStack_3c [8];
  int iStack_34;
  int iStack_30;
  int iStack_2c;
  int iStack_28;
  undefined4 uStack_24;
  int iStack_20;
  
  uVar2 = (uint)DAT_20001060;
  if (uVar2 == 5) {
    uVar2 = (uint)DAT_20000f14;
    if (uVar2 == 0) {
      if (DAT_20000f13 == '\0') {
        return 0;
      }
      if (cRam20000f12 == '\0') {
        uVar2 = bRam20000f15 / 6;
        cRam20000f12 = '\x01';
        uRam20000f0e = 0;
        uRam20000f0a = 0;
        *(byte *)(uVar2 + 0x20000f0a) =
             (byte)(1 << ((uint)bRam20000f15 + uVar2 * -6 & 0xff)) | *(byte *)(uVar2 + 0x20000f0a);
      }
      else {
        cRam20000f12 = '\0';
      }
      uRam20002d53 = 0x28;
      FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
      uRam20002d53 = 0x26;
code_r0x0803acf0:
      iVar4 = _DAT_20002d6c;
      iStack_20 = _DAT_20002d6c;
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
    }
  }
  else if (uVar2 == 2) {
    uVar2 = (uint)bRam2000044c;
    if ((DAT_2000010c == '\x03') || (2 < (uVar2 & 0xf))) {
      bRam2000044c = bRam2000044c ^ 0x80;
      uRam20002d53 = 2;
      goto code_r0x0803acf0;
    }
  }
  else if ((uVar2 == 1) && (uVar2 = bRam20001055 & 0xf0, uVar2 != 0xb0)) {
    uVar2 = (uint)DAT_20001025;
    bRam20001055 = 0;
    if ((uVar2 < 8) && (cVar5 = '\x01', (1 << uVar2 & 0xc6U) != 0)) {
      if (DAT_2000102e == '\x01') {
        cVar5 = '\x02';
      }
      switch(uVar2) {
      case 1:
        uRam20002d54 = 0xc;
        if (DAT_2000102e == '\x01') {
          uRam20002d54 = 0xd;
        }
        break;
      case 2:
        uRam20002d54 = 0xe;
        if (DAT_2000102e == '\x01') {
          uRam20002d54 = 0x17;
        }
        break;
      case 6:
        uRam20002d54 = 0x11;
        if (DAT_2000102e == '\x01') {
          uRam20002d54 = 0x16;
        }
        break;
      case 7:
        uRam20002d54 = 0x10;
        if (DAT_2000102e == '\x01') {
          uRam20002d54 = 0x15;
        }
      }
      uRam20001040 = 0x7fc00000;
      uRam20001044 = 0x7fc00000;
      uRam20001048 = 0x7fc00000;
      DAT_2000102e = cVar5;
      FUN_080028e0();
      uRam20002d54 = uRam20002d54 | 0x500;
      FUN_0803acf0(uRam20002d74,0x20002d54,0xffffffff);
      uRam20002d53 = 0x1b;
      FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
      uVar2 = (uint)DAT_20001025;
    }
    if (uVar2 == 5) {
      uRam20001040 = 0x7fc00000;
      cRam20001031 = cRam20001031 == '\0';
      uRam20001044 = 0x7fc00000;
      uRam20001048 = 0x7fc00000;
      return (uint)(byte)cRam20001031;
    }
  }
  return uVar2;
}



// ==========================================
// Function: FUN_080062f8 @ 080062f8
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

undefined4 FUN_080062f8(void)

{
  bool bVar1;
  int iVar2;
  int iVar3;
  char cVar4;
  undefined1 auStack_3c [8];
  int iStack_34;
  int iStack_30;
  int iStack_2c;
  int iStack_28;
  undefined4 uStack_24;
  int iStack_20;
  
  if (DAT_20001060 == '\x05') {
    if (DAT_20000f14 != '\0') {
      return 0x200000f8;
    }
    if (DAT_20000f13 != '\0') {
      cVar4 = '\x02';
      if (cRam20000f12 == '\x02') {
        cVar4 = '\x01';
      }
      uRam20000f0a = 0;
      if (cRam20000f12 != '\x02') {
        uRam20000f0a = 0x3f3f3f3f;
      }
      uRam20002d53 = 0x26;
      uRam20000f0e = uRam20000f0a;
      cRam20000f12 = cVar4;
      FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
      uRam20002d53 = 0x28;
      goto code_r0x0803acf0;
    }
  }
  else {
    if (DAT_20001060 == '\x02') {
      bRam2000044c = bRam2000044c + 1;
      if (((bRam2000044c & 0xf) == 3) && ((DAT_20000332 == '\0' || (DAT_2000012c == '\x01')))) {
        bRam2000044c = bRam2000044c & 0xf0;
      }
      if ((bRam2000044c & 0xc) != 0) {
        bRam2000044c = bRam2000044c & 0xf0;
      }
      uRam20002d50 = 0;
      if ((bRam2000044c & 0xf) != 0) {
        uRam20002d50 = 0x3c00;
      }
      if (((bRam2000044c & 0xf) < 3) && (DAT_2000010c != '\x03')) {
        bRam2000044c = bRam2000044c & 0x7f | (DAT_2000010c != '\x01') << 7;
      }
      uRam20002d53 = 2;
code_r0x0803acf0:
      iVar3 = _DAT_20002d6c;
      iStack_20 = _DAT_20002d6c;
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
      iVar2 = FUN_0803b730();
      if (iVar2 != 0 || iStack_28 == 0) {
        while( true ) {
          FUN_0803a168();
          if ((*(uint *)(iVar3 + 0x38) < *(uint *)(iVar3 + 0x3c)) || (iStack_2c == 2)) {
            iStack_34 = FUN_08034cc4(iVar3,uStack_24,iStack_2c);
            if (*(int *)(iVar3 + 0x24) == 0) {
              if (iStack_34 != 0) {
                _DAT_e000ed04 = 0x10000000;
                DataSynchronizationBarrier(0xf);
                InstructionSynchronizationBarrier(0xf);
              }
            }
            else {
              iVar3 = FUN_0803bb4c(iVar3 + 0x24);
              if (iVar3 != 0) {
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
          if (*(char *)(iVar3 + 0x44) == -1) {
            *(undefined1 *)(iVar3 + 0x44) = 0;
          }
          if (*(char *)(iVar3 + 0x45) == -1) {
            *(undefined1 *)(iVar3 + 0x45) = 0;
          }
          FUN_0803a1b0();
          iVar2 = FUN_0803b5cc(auStack_3c,&iStack_28);
          if (iVar2 != 0) break;
          iVar2 = FUN_080352d4(iVar3);
          if (iVar2 == 0) {
            FUN_080357b8(iVar3);
            FUN_0803bc10();
          }
          else {
            FUN_0803a434(iVar3 + 0x10,iStack_28);
            FUN_080357b8(iVar3);
            iVar2 = FUN_0803bc10();
            if (iVar2 == 0) {
              _DAT_e000ed04 = 0x10000000;
              DataSynchronizationBarrier(0xf);
              InstructionSynchronizationBarrier(0xf);
            }
          }
        }
        FUN_080357b8(iVar3);
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
    if ((DAT_20001060 == '\x01') && ((bRam20001055 & 0xf0) != 0xb0)) {
      bRam20001055 = 0;
      return 0x200000f8;
    }
  }
  return 0x200000f8;
}



// Attempting to create function at 0x08006418
// Successfully created function
// ==========================================
// Function: FUN_08006418 @ 08006418
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

uint FUN_08006418(void)

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
  
  switch(_DAT_20001060 & 0xff) {
  case 1:
    if ((bRam20001055 & 0xf0) == 0xb0) {
      return 0x200000f8;
    }
    bRam20001055 = 0;
    return 0x200000f8;
  case 2:
    _DAT_20001060 = 0x50109;
    cRam2000044d = 1;
    break;
  default:
    return 0x200000f8;
  case 5:
    if (DAT_20000f14 != '\0') {
      return 0x200000f8;
    }
    if (cRam20000f12 == '\0') {
      return 0x200000f8;
    }
    if (((((((cRam20000f0a == '\0' && cRam20000f0b == '\0') && cRam20000f0c == '\0') &&
           cRam20000f0d == '\0') && cRam20000f0e == '\0') && cRam20000f0f == '\0') &&
        cRam20000f10 == '\0') && cRam20000f11 == '\0') {
      return 0x200000f8;
    }
    goto code_r0x080064a4;
  case 6:
    if (DAT_20000f14 != '\0') {
      return 0x200000f8;
    }
code_r0x080064a4:
    DAT_20000f14 = '\x02';
    uRam20002d53 = 0x2a;
code_r0x0803acf0:
    iVar4 = _DAT_20002d6c;
    iStack_20 = _DAT_20002d6c;
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
  case 9:
    if (cRam2000044d == '\0') {
      return 0x200000f8;
    }
    if (DAT_20001062 != '\x05') {
      _DAT_20001060 = (uint)CONCAT21(0x501,DAT_20001060);
      uRam20002d53 = 0x13;
      FUN_0803acf0(_DAT_20002d6c,0x20002d53,0xffffffff);
      uRam20002d53 = 0x14;
      goto code_r0x0803acf0;
    }
    cRam2000044d = 0;
    _DAT_20001060 = 2;
  }
  uVar2 = _DAT_20001060 & 0xff;
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



