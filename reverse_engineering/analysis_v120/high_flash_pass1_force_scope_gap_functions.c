// Force-decompiled functions

// Attempting to create function at 0x080041f8
// Successfully created function
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
          func_0x0802a430(&stack0xffffffd8);
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



// Attempting to create function at 0x080047cc
// Successfully created function
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
          func_0x0802a430(&stack0xffffffd8);
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



// Attempting to create function at 0x0800bff4
// Successfully created function
// ==========================================
// Function: FUN_0800bff4 @ 0800bff4
// Size: 1 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_0800bff4(void)

{
  undefined2 uVar1;
  char *pcVar2;
  undefined4 uVar3;
  uint uVar4;
  
  pcVar2 = s_3__System_file_2_jpg_0800c314;
  if (DAT_20000126 == '\0') {
    pcVar2 = s_3__System_file_4_jpg_0800c2fc;
  }
  FUN_08032f6c(pcVar2,0x34,0);
  FUN_080003b4(&DAT_20008360,0x800c32c,*(undefined4 *)((uint)DAT_20000125 * 4 + 0x804c988));
  FUN_08008154(0x34,0,0x48,0x14,&DAT_20008360,0x20001114,0xffff,1);
  switch(bRam2000044c & 0xf) {
  case 0:
    FUN_08008154(0xc4,0,0x24,0x14,0x80bbd2e,0x20001144,
                 *(undefined2 *)
                  ((uint)DAT_2000105b * 4 + 0x80bb40c +
                  ((int)((uint)bRam2000044c << 0x18) >> 0x1f) * -2),1);
    uVar3 = 0x800c334;
    break;
  case 1:
    uVar4 = 5;
    FUN_08019af8(0xd5,5,0xd5,0xd,0xd9,9,
                 *(undefined2 *)
                  ((uint)DAT_2000105b * 4 + 0x80bb40c +
                  ((int)((uint)bRam2000044c << 0x18) >> 0x1f) * -2));
    uVar1 = *(undefined2 *)
             ((uint)DAT_2000105b * 4 + 0x80bb40c + ((int)((uint)bRam2000044c << 0x18) >> 0x1f) * -2)
    ;
    do {
      if ((((_DAT_20008350 < 0xd2) && (0xd1 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
          (_DAT_20008352 <= uVar4)) && (uVar4 < (uint)_DAT_20008352 + (uint)_DAT_20008356)) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar4 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1a2) = uVar1;
      }
      if (((_DAT_20008350 < 0xd3) && (0xd2 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar4 && (uVar4 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar4 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1a4) = uVar1;
      }
      if (((_DAT_20008350 < 0xd4) && (0xd3 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
         ((_DAT_20008352 <= uVar4 && (uVar4 < (uint)_DAT_20008352 + (uint)_DAT_20008356)))) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar4 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1a6) = uVar1;
      }
      if ((((_DAT_20008350 < 0xd5) && (0xd4 < (uint)_DAT_20008350 + (uint)_DAT_20008354)) &&
          (_DAT_20008352 <= uVar4)) && (uVar4 < (uint)_DAT_20008352 + (uint)_DAT_20008356)) {
        *(undefined2 *)
         (_DAT_20008358 + ((uint)_DAT_20008354 * (uVar4 - _DAT_20008352) - (uint)_DAT_20008350) * 2
         + 0x1a8) = uVar1;
      }
      uVar4 = uVar4 + 1;
    } while (uVar4 != 0xe);
    goto code_r0x0800c26e;
  case 2:
    FUN_08019af8(0xd1,9,0xd8,4,0xd8,0xe,0x7e2);
code_r0x0800c26e:
    FUN_08019af8(0xf5,6,0xff,6,0xfa,0xd,0x7e2);
    return;
  case 3:
    if (DAT_20000332 == '\x03') {
      if (-1 < (int)((uint)bRam2000044c << 0x18)) {
        FUN_08008154(0xc4,0,0x24,0x14,0x80bb979,0x20001144,0xffff,1);
        uVar3 = 0x80bb97c;
        break;
      }
      uVar3 = 0x80bb9e4;
    }
    else {
      if (DAT_20000332 != '\x02') {
        if (DAT_20000332 != '\x01') {
          return;
        }
        FUN_08008154(0xc4,0,0x24,0x14,0x80bb979,0x20001144,0xffff,1);
        uVar3 = 0x80bb9e4;
        break;
      }
      uVar3 = 0x80bb97c;
    }
    FUN_08008154(0xc4,0,0x24,0x14,uVar3,0x20001144,0xffff,1);
    uVar3 = 0x80bb9e7;
    break;
  default:
    return;
  }
  FUN_08008154(0xe8,0,0x24,0x14,uVar3,0x20001144,0xffff,1);
  return;
}



// Attempting to create function at 0x0800f5a8
// Successfully created function
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



// Attempting to create function at 0x0802a430
// Successfully created function
// ==========================================
// Function: FUN_0802a430 @ 0802a430
// Size: 1 bytes
// ==========================================

void FUN_0802a430(uint *param_1)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  
  uVar2 = (uRam40021004 << 0x1c) >> 0x1e;
  uVar1 = 8000000;
  if (uVar2 == 2) {
    uVar2 = (uRam40021004 << 10) >> 0x1c;
    uVar1 = (uRam40021004 << 1) >> 0x1e;
    iVar3 = uVar1 * 0x10 + 1;
    iVar4 = iVar3;
    if (uVar1 == 0) {
      iVar4 = 2;
    }
    if (uVar2 == 0xf) {
      iVar4 = iVar3;
    }
    iVar4 = iVar4 + uVar2;
    if ((int)(uRam40021004 << 0xf) < 0) {
      if ((int)(uRam40021004 << 0xe) < 0) {
        uVar1 = iVar4 * (8000000 / (((uint)(iRam40021054 << 0x12) >> 0x1e) + 2));
      }
      else {
        uVar1 = iVar4 * 8000000;
      }
    }
    else {
      uVar1 = iVar4 * 4000000;
    }
  }
  else if (((uVar2 == 0) && (iRam40021054 << 0x16 < 0)) && (iRam40021030 << 6 < 0)) {
    uVar1 = 48000000;
  }
  *param_1 = uVar1;
  uVar1 = uVar1 >> *(sbyte *)(((uRam40021004 << 0x18) >> 0x1c) + 0x80bb3ec);
  param_1[1] = uVar1;
  param_1[3] = uVar1 >> *(sbyte *)(((uRam40021004 << 0x15) >> 0x1d) + 0x80bb404);
  uVar1 = uVar1 >> *(sbyte *)(((uRam40021004 << 0x12) >> 0x1d) + 0x80bb404);
  param_1[2] = uVar1;
  param_1[4] = uVar1 / *(byte *)(((uRam40021004 << 0x10) >> 0x1e) + (uRam40021004 >> 0x1a & 4) +
                                0x802a50c);
  return;
}



// Attempting to create function at 0x08033310
// Successfully created function
// ==========================================
// Function: FUN_08033310 @ 08033310
// Size: 1 bytes
// ==========================================

void FUN_08033310(undefined1 param_1)

{
  int iStack_18;
  char cStack_11;
  int iStack_10;
  undefined1 uStack_a;
  
  uStack_a = param_1;
  iStack_10 = FUN_08033cfc(0x102c);
  if (iStack_10 != 0) {
    FUN_08000370(&DAT_20008360,0x80bc859,uStack_a);
    cStack_11 = FUN_0802d8b8(iStack_10,&DAT_20008360,1);
    if (cStack_11 == '\0') {
      if (iRam20000f18 == 0) {
        iRam20000f18 = FUN_08033cfc(0x25f);
      }
      if (((iRam20000f18 == 0) ||
          (cStack_11 = FUN_0802dcbc(iStack_10,iRam20000f18,0x25f,&iStack_18), cStack_11 != '\0')) ||
         (iStack_18 != 0x25f)) {
        FUN_0802d014(iStack_10);
        FUN_0802e12c();
        FUN_08033cbc(iStack_10);
      }
      else {
        FUN_0802d014(iStack_10);
        FUN_08033cbc(iStack_10);
      }
    }
    else {
      FUN_0802e12c();
      FUN_08033cbc(iStack_10);
    }
  }
  return;
}



