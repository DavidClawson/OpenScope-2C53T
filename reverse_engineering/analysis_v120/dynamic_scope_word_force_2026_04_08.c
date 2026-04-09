// Force-decompiled functions

// Attempting to create function at 0x08006120
// Successfully created function
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



// Attempting to create function at 0x080062F8
// Successfully created function
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



