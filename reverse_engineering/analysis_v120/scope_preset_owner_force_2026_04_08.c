// Force-decompiled functions

// Attempting to create function at 0x08003148
// Successfully created function
// ==========================================
// Function: FUN_08003148 @ 08003148
// Size: 1 bytes
// ==========================================

/* WARNING: Removing unreachable block (ram,0x0803ad58) */
/* WARNING: Removing unreachable block (ram,0x0803ad62) */
/* WARNING: Removing unreachable block (ram,0x0803ad64) */
/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x0800356e */
/* WARNING: Removing unreachable block (ram,0x0803ad6a) */
/* WARNING: Removing unreachable block (ram,0x0803ad70) */
/* WARNING: Removing unreachable block (ram,0x0803ad74) */
/* WARNING: Removing unreachable block (ram,0x0803ad80) */
/* WARNING: Removing unreachable block (ram,0x0803adcc) */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* WARNING: Restarted to delay deadcode elimination for space: register */

byte * FUN_08003148(undefined4 param_1,undefined4 param_2)

{
  ushort uVar1;
  ushort uVar2;
  ushort uVar3;
  ushort uVar4;
  ushort uVar5;
  ushort uVar6;
  ushort uVar7;
  ushort uVar8;
  ushort uVar9;
  undefined2 uVar10;
  undefined4 uVar11;
  byte *pbVar12;
  undefined4 uVar13;
  uint uVar14;
  ushort *puVar15;
  undefined4 uVar16;
  byte *pbVar17;
  uint uVar18;
  int iVar19;
  int iVar20;
  char cVar21;
  ushort *puVar22;
  short sVar23;
  short *psVar24;
  uint uVar25;
  byte bVar26;
  uint uVar27;
  int iVar28;
  short sVar29;
  uint uVar30;
  byte bVar31;
  bool bVar32;
  bool bVar33;
  uint in_fpscr;
  float fVar34;
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
  undefined4 extraout_s1_11;
  float fVar35;
  float fVar36;
  undefined8 uVar37;
  undefined8 uVar38;
  uint uStack_3c;
  undefined8 uStack_38;
  int iStack_28;
  undefined2 *puStack_24;
  
  pbVar12 = (byte *)(DAT_20001060 - 1);
  switch(pbVar12) {
  case (byte *)0x0:
    if (DAT_20001025 != '\x05' || (bRam20001055 & 0xf0) != 0xb0) {
      if ((bRam20001055 & 0xf0) == 0xb0) {
        return (byte *)0xb0;
      }
      bRam20001055 = 0;
      return (byte *)0x0;
    }
    puStack_24 = &uRam20002d54;
    uRam20002d54 = 0x508;
    iVar20 = iRam20002d74;
    goto code_r0x0800361c;
  case (byte *)0x1:
    pbVar12 = (byte *)(uint)bRam2000044c;
    switch((uint)pbVar12 & 0xf) {
    case 0:
      pbVar12 = &DAT_200000fa + (bRam2000044c >> 7);
      if (8 < *pbVar12) {
        return pbVar12;
      }
      *pbVar12 = *pbVar12 + 1;
      if ((&DAT_200000fa)[bRam2000044c >> 7] == '\x05') {
        DAT_20000eb3 = 10;
      }
      if ((char)bRam2000044c < '\0') {
        FUN_08001a58(DAT_200000fb);
      }
      else {
        FUN_080018a4(DAT_200000fa);
      }
      uRam20002d53 = 4;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      FUN_0801efc0();
      pbVar12 = (byte *)(uint)DAT_2000010e;
      if (pbVar12 != (byte *)(uint)(bRam2000044c >> 7)) {
        return pbVar12;
      }
      fVar36 = (float)VectorSignedToFloat((int)(short)(_DAT_20000114 - (char)pbVar12[0x200000fc]),
                                          (byte)(in_fpscr >> 0x15) & 3);
      _DAT_20000114 =
           (short)(int)(fVar36 / *(float *)(((uint)pbVar12[0x200000fa] % 3) * 4 + 0x80038f4)) +
           (short)(char)pbVar12[0x200000fc];
      if (99 < _DAT_20000114) {
        _DAT_20000114 = 100;
      }
      if (_DAT_20000114 < -99) {
        _DAT_20000114 = -100;
      }
      if ((DAT_2000010f == '\x02') && (DAT_20000126 != '\0')) {
        uRam20002d52 = 8;
        FUN_0803acf0(_DAT_20002d78,&uRam20002d52,0xffffffff);
      }
      if ((byte *)(uint)DAT_2000012c != (byte *)0x0) {
        return (byte *)(uint)DAT_2000012c;
      }
      uRam20002d53 = 6;
code_r0x080033c0:
      puStack_24 = (undefined2 *)&uRam20002d53;
      iVar20 = _DAT_20002d6c;
      goto code_r0x0800361c;
    case 1:
      pbVar12 = &DAT_200000fc + (bRam2000044c >> 7);
      if ('c' < (char)*pbVar12) {
        return pbVar12;
      }
      *pbVar12 = *pbVar12 + 1;
      if ((char)bRam2000044c < '\0') {
        uVar14 = (uint)DAT_200000fb;
        if (DAT_20000125 < 5) {
          if ((DAT_20000125 == 4) || (DAT_2000010c == '\x03')) {
            puVar15 = (ushort *)(&DAT_20000420 + uVar14 * 2);
            puVar22 = (ushort *)(&DAT_200003e4 + uVar14 * 2);
          }
          else {
            puVar15 = (ushort *)(&DAT_20000434 + uVar14 * 2);
            puVar22 = (ushort *)(&DAT_200003f8 + uVar14 * 2);
          }
        }
        else {
          puVar15 = (ushort *)(&DAT_2000040c + uVar14 * 2);
          puVar22 = (ushort *)(&DAT_200003d0 + uVar14 * 2);
        }
        fVar36 = (float)VectorSignedToFloat((uint)*puVar15 - (uint)*puVar22,
                                            (byte)(in_fpscr >> 0x15) & 3);
        fVar34 = (float)VectorSignedToFloat(DAT_200000fd + 100,(byte)(in_fpscr >> 0x15) & 3);
        fVar35 = (float)VectorUnsignedToFloat((uint)*puVar22,(byte)(in_fpscr >> 0x15) & 3);
        _DAT_40001c34 = VectorFloatToUnsigned((fVar36 / fRam080038f0) * fVar34 + fVar35,3);
      }
      else {
        uVar14 = (uint)DAT_200000fa;
        if (DAT_20000125 < 5) {
          if ((DAT_20000125 == 4) || (DAT_2000010c == '\x03')) {
            puVar15 = (ushort *)(&DAT_200003a8 + uVar14 * 2);
            puVar22 = (ushort *)(&DAT_2000036c + uVar14 * 2);
          }
          else {
            puVar15 = (ushort *)(&DAT_200003bc + uVar14 * 2);
            puVar22 = (ushort *)(&DAT_20000380 + uVar14 * 2);
          }
        }
        else {
          puVar15 = (ushort *)(&DAT_20000394 + uVar14 * 2);
          puVar22 = (ushort *)(&DAT_20000358 + uVar14 * 2);
        }
        fVar36 = (float)VectorSignedToFloat((uint)*puVar15 - (uint)*puVar22,
                                            (byte)(in_fpscr >> 0x15) & 3);
        fVar34 = (float)VectorSignedToFloat(DAT_200000fc + 100,(byte)(in_fpscr >> 0x15) & 3);
        fVar35 = (float)VectorUnsignedToFloat((uint)*puVar22,(byte)(in_fpscr >> 0x15) & 3);
        uVar14 = VectorFloatToUnsigned((fVar36 / fRam080038f0) * fVar34 + fVar35,3);
        _DAT_40007408 = uVar14 & 0xfff | _DAT_40007408 & 0xfffff000;
        _DAT_40007404 = _DAT_40007404 | 1;
      }
      if ((DAT_20000126 != '\0') && (DAT_20000124 = 100, 0x13 < DAT_20000125)) {
        _DAT_20000eac = 0x12d;
      }
      FUN_0803acf0(uRam20002d80,0,0);
      if (DAT_2000012c == 0) {
        uRam20002d53 = 5;
        FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      }
      if (DAT_2000010e != bRam2000044c >> 7) {
        return (byte *)(uint)bRam2000044c;
      }
      if (99 < _DAT_20000114) {
        return (byte *)(int)_DAT_20000114;
      }
      _DAT_20000114 = _DAT_20000114 + 1;
      break;
    case 2:
      if (99 < _DAT_20000114) {
        return (byte *)(int)_DAT_20000114;
      }
      _DAT_20000114 = _DAT_20000114 + 1;
      _DAT_20000122 = 300;
      FUN_0803acf0(uRam20002d80,0,0);
      break;
    case 3:
      if (DAT_20000332 == 3) {
        if ((int)pbVar12 << 0x18 < 0) {
          if (99 < _DAT_20000330) {
            return (byte *)(int)_DAT_20000330;
          }
          _DAT_20000330 = _DAT_20000330 + 1;
          goto code_r0x0800374c;
        }
      }
      else if (DAT_20000332 != 2) {
        if (DAT_20000332 != 1) {
          return pbVar12;
        }
        if (0x95 < _DAT_2000032a) {
          return (byte *)(int)_DAT_2000032a;
        }
        _DAT_2000032a = _DAT_2000032a + 1;
        goto code_r0x0800374c;
      }
      pbVar12 = (byte *)(int)_DAT_2000032e;
      if ((int)pbVar12 < 100) {
        _DAT_2000032e = _DAT_2000032e + 1;
code_r0x0800374c:
        pbVar12 = (byte *)(uint)DAT_2000012c;
        if (pbVar12 == (byte *)0x0) {
          if ((DAT_20000332 & 1) != 0) {
            _DAT_2000033c =
                 (float)VectorSignedToFloat((int)_DAT_2000032a - (int)_DAT_20000112,
                                            (byte)(in_fpscr >> 0x15) & 3);
            _DAT_20000334 = _DAT_2000033c / 25.0;
            uVar18 = DAT_20000125 / 3 + 1;
            _DAT_20000338 =
                 (float)VectorSignedToFloat((int)_DAT_2000032c - (int)_DAT_20000112,
                                            (byte)(in_fpscr >> 0x15) & 3);
            _DAT_2000033c = _DAT_20000338 - _DAT_2000033c;
            uVar14 = uVar18 / 3;
            DAT_20000350 = (char)uVar14;
            uVar37 = FUN_0803e5da((&DAT_080465c8)
                                  [(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
            uVar13 = FUN_0803e50a(uVar18 - (uVar14 * 3 & 0xff));
            uVar13 = FUN_0803c8e0((int)DAT_0801f3b0,param_2,uVar13);
            uVar37 = FUN_0803e77c(uVar13,extraout_s1_02,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20)
                                 );
            uVar38 = FUN_0803ed70(_DAT_20000334);
            FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                         (int)((ulonglong)uVar38 >> 0x20));
            _DAT_20000334 = (float)FUN_0803df48();
            fVar36 = DAT_0801f3b8;
            uVar14 = in_fpscr & 0xfffffff | (uint)(ABS(_DAT_20000334) < DAT_0801f3b8) << 0x1f;
            uVar18 = uVar14 | (uint)(NAN(ABS(_DAT_20000334)) || NAN(DAT_0801f3b8)) << 0x1c;
            if ((byte)(uVar14 >> 0x1f) == ((byte)(uVar18 >> 0x1c) & 1)) {
              _DAT_20000334 = _DAT_20000334 / DAT_0801f3b8;
              DAT_20000350 = DAT_20000350 + '\x01';
            }
            _DAT_20000338 = _DAT_20000338 / 25.0;
            uVar14 = DAT_20000125 / 3 + 1;
            uVar25 = uVar14 / 3;
            DAT_20000351 = (char)uVar25;
            uVar37 = FUN_0803e5da((&DAT_080465c8)
                                  [(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
            uVar13 = FUN_0803e50a(uVar14 - (uVar25 * 3 & 0xff));
            uVar13 = FUN_0803c8e0((int)DAT_0801f3b0,extraout_s1_02,uVar13);
            uVar37 = FUN_0803e77c(uVar13,extraout_s1_03,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20)
                                 );
            uVar38 = FUN_0803ed70(_DAT_20000338);
            FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                         (int)((ulonglong)uVar38 >> 0x20));
            _DAT_20000338 = (float)FUN_0803df48();
            uVar14 = uVar18 & 0xfffffff | (uint)(ABS(_DAT_20000338) < fVar36) << 0x1f;
            uVar18 = uVar14 | (uint)(NAN(ABS(_DAT_20000338)) || NAN(fVar36)) << 0x1c;
            if ((byte)(uVar14 >> 0x1f) == ((byte)(uVar18 >> 0x1c) & 1)) {
              _DAT_20000338 = _DAT_20000338 / fVar36;
              DAT_20000351 = DAT_20000351 + '\x01';
            }
            _DAT_2000033c = _DAT_2000033c / 25.0;
            uVar14 = DAT_20000125 / 3 + 1;
            uVar25 = uVar14 / 3;
            DAT_20000352 = (char)uVar25;
            uVar37 = FUN_0803e5da((&DAT_080465c8)
                                  [(uint)DAT_20000125 + (DAT_20000125 / 3) * -3 & 0xff]);
            uVar13 = FUN_0803e50a(uVar14 - (uVar25 * 3 & 0xff));
            uVar13 = FUN_0803c8e0((int)DAT_0801f3b0,extraout_s1_03,uVar13);
            uVar37 = FUN_0803e77c(uVar13,extraout_s1_04,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20)
                                 );
            uVar38 = FUN_0803ed70(_DAT_2000033c);
            FUN_0803e77c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                         (int)((ulonglong)uVar38 >> 0x20));
            _DAT_2000033c = (float)FUN_0803df48();
            if (!NAN(ABS(_DAT_2000033c)) && !NAN(fVar36)) {
              _DAT_2000033c = _DAT_2000033c / fVar36;
              DAT_20000352 = DAT_20000352 + '\x01';
            }
            in_fpscr = uVar18 & 0xfffffff | (uint)(_DAT_2000033c == 0.0) << 0x1e;
            param_2 = extraout_s1_04;
            if ((byte)(in_fpscr >> 0x1e) == 0) {
              fVar36 = ABS(_DAT_2000033c);
              uVar37 = DAT_0801f3c8;
              if (DAT_20000352 != '\0') {
                uVar13 = FUN_0803e5da();
                uVar13 = FUN_0803c8e0((int)DAT_0801f3c0,extraout_s1_04,uVar13);
                uVar37 = FUN_0803e124((int)DAT_0801f3c8,(int)((ulonglong)DAT_0801f3c8 >> 0x20),
                                      uVar13,extraout_s1_05);
                param_2 = extraout_s1_05;
              }
              uVar38 = FUN_0803ed70(fVar36);
              uVar37 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),(int)uVar38,
                                    (int)((ulonglong)uVar38 >> 0x20));
              uVar13 = (undefined4)((ulonglong)DAT_0801f3c0 >> 0x20);
              uVar16 = (undefined4)DAT_0801f3c0;
              DAT_20000356 = 0;
              iVar20 = FUN_0803ee0c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar16,uVar13);
              bVar26 = DAT_20000356;
              if (iVar20 == 0) {
                bVar26 = 0;
                do {
                  uStack_3c = (uint)(byte)(bVar26 + 1);
                  uVar38 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar16,uVar13);
                  uVar37 = FUN_0803e124((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),uVar16,uVar13);
                  if (uStack_3c < 3) {
                    bVar26 = bVar26 + 1;
                    uVar37 = uVar38;
                  }
                  uVar11 = (undefined4)((ulonglong)uVar37 >> 0x20);
                  iVar20 = FUN_0803edf0((int)uVar37,uVar11,uVar16,uVar13);
                  if (iVar20 == 0) break;
                  uStack_3c = (uint)(byte)(bVar26 + 1);
                  uVar38 = FUN_0803e124((int)uVar37,uVar11,uVar16,uVar13);
                  uVar37 = FUN_0803e124((int)uVar38,(int)((ulonglong)uVar38 >> 0x20),uVar16,uVar13);
                  if (uStack_3c < 3) {
                    bVar26 = bVar26 + 1;
                    uVar37 = uVar38;
                  }
                  uVar11 = (undefined4)((ulonglong)uVar37 >> 0x20);
                  iVar20 = FUN_0803ee0c((int)uVar37,uVar11,uVar16,uVar13);
                  if (iVar20 != 0) break;
                  bVar31 = bVar26 + 1;
                  uStack_3c = (uint)bVar31;
                  uVar37 = FUN_0803e124((int)uVar37,uVar11,uVar16,uVar13);
                  uVar38 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar16,uVar13);
                  if (2 < uStack_3c) {
                    bVar31 = bVar26;
                    uVar37 = uVar38;
                  }
                  uVar11 = (undefined4)((ulonglong)uVar37 >> 0x20);
                  iVar20 = FUN_0803ee0c((int)uVar37,uVar11,uVar16,uVar13);
                  bVar26 = bVar31;
                  if (iVar20 != 0) break;
                  bVar26 = bVar31 + 1;
                  uVar37 = FUN_0803e124((int)uVar37,uVar11,uVar16,uVar13);
                  uVar38 = FUN_0803e124((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar16,uVar13);
                  if (2 < bVar26) {
                    bVar26 = bVar31;
                    uVar37 = uVar38;
                  }
                  iVar20 = FUN_0803ee0c((int)uVar37,(int)((ulonglong)uVar37 >> 0x20),uVar16,uVar13);
                } while (iVar20 == 0);
              }
              DAT_20000356 = bVar26;
              _DAT_2000034c = FUN_0803df48((int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
            }
            else {
              _DAT_2000034c = 0;
              DAT_20000356 = 0;
            }
          }
          pbVar12 = (byte *)((uint)DAT_20000332 << 0x1e);
          if ((int)pbVar12 < 0) {
            uVar14 = (uint)DAT_2000010e;
            _DAT_20000340 =
                 (float)VectorSignedToFloat((int)_DAT_2000032e - (int)(char)(&DAT_200000fc)[uVar14],
                                            (byte)(in_fpscr >> 0x15) & 3);
            _DAT_20000344 =
                 (float)VectorSignedToFloat((int)_DAT_20000330 - (int)(char)(&DAT_200000fc)[uVar14],
                                            (byte)(in_fpscr >> 0x15) & 3);
            _DAT_20000348 = _DAT_20000340 - _DAT_20000344;
            fVar36 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                            [(uint)(byte)(&DAT_200000fa)[uVar14] % 3
                                                            ] << 2,(byte)(in_fpscr >> 0x15) & 3);
            _DAT_20000340 = _DAT_20000340 * fVar36;
            if ((byte)(&DAT_200000fa)[uVar14] < 3) {
              _DAT_20000340 = _DAT_20000340 / 10.0;
            }
            else {
              uVar13 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar14] - 3) / 3));
              uVar13 = FUN_0803c8e0((int)DAT_0801f6e8,param_2,uVar13);
              uVar37 = FUN_0803ed70(_DAT_20000340);
              FUN_0803e77c(uVar13,extraout_s1_06,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
              _DAT_20000340 = (float)FUN_0803df48();
              uVar14 = (uint)DAT_2000010e;
              param_2 = extraout_s1_06;
            }
            uVar13 = FUN_0803e5da(*(undefined1 *)(uVar14 + 0x200000fe));
            uVar13 = FUN_0803c8e0((int)DAT_0801f6e8,param_2,uVar13);
            uVar37 = FUN_0803ed70(_DAT_20000340);
            FUN_0803e77c(uVar13,extraout_s1_07,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
            _DAT_20000340 = (float)FUN_0803df48();
            fVar36 = DAT_0801f6f0;
            uVar14 = in_fpscr & 0xfffffff | (uint)(ABS(_DAT_20000340) < DAT_0801f6f0) << 0x1f;
            uVar18 = uVar14 | (uint)(NAN(ABS(_DAT_20000340)) || NAN(DAT_0801f6f0)) << 0x1c;
            DAT_20000353 = (byte)(uVar14 >> 0x1f) == ((byte)(uVar18 >> 0x1c) & 1);
            if ((bool)DAT_20000353) {
              _DAT_20000340 = _DAT_20000340 / DAT_0801f6f0;
            }
            uVar14 = (uint)DAT_2000010e;
            fVar34 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                            [(uint)(byte)(&DAT_200000fa)[uVar14] % 3
                                                            ] << 2,(byte)(uVar18 >> 0x15) & 3);
            _DAT_20000344 = _DAT_20000344 * fVar34;
            if ((byte)(&DAT_200000fa)[uVar14] < 3) {
              _DAT_20000344 = _DAT_20000344 / 10.0;
              uVar13 = extraout_s1_07;
            }
            else {
              uVar13 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar14] - 3) / 3));
              uVar13 = FUN_0803c8e0((int)DAT_0801f6e8,extraout_s1_07,uVar13);
              uVar37 = FUN_0803ed70(_DAT_20000344);
              FUN_0803e77c(uVar13,extraout_s1_08,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
              _DAT_20000344 = (float)FUN_0803df48();
              uVar14 = (uint)DAT_2000010e;
              uVar13 = extraout_s1_08;
            }
            uVar16 = FUN_0803e5da(*(undefined1 *)(uVar14 + 0x200000fe));
            uVar13 = FUN_0803c8e0((int)DAT_0801f6e8,uVar13,uVar16);
            uVar37 = FUN_0803ed70(_DAT_20000344);
            FUN_0803e77c(uVar13,extraout_s1_09,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
            _DAT_20000344 = (float)FUN_0803df48();
            uVar14 = uVar18 & 0xfffffff | (uint)(ABS(_DAT_20000344) < fVar36) << 0x1f;
            DAT_20000354 = SUB41(uVar14 >> 0x1f,0) == (NAN(ABS(_DAT_20000344)) || NAN(fVar36));
            if ((bool)DAT_20000354) {
              _DAT_20000344 = _DAT_20000344 / fVar36;
            }
            uVar18 = (uint)DAT_2000010e;
            fVar34 = (float)VectorSignedToFloat((uint)(byte)(&DAT_080465c8)
                                                            [(uint)(byte)(&DAT_200000fa)[uVar18] % 3
                                                            ] << 2,(byte)(uVar14 >> 0x15) & 3);
            _DAT_20000348 = _DAT_20000348 * fVar34;
            if ((byte)(&DAT_200000fa)[uVar18] < 3) {
              _DAT_20000348 = _DAT_20000348 / 10.0;
              uVar13 = extraout_s1_09;
            }
            else {
              uVar13 = FUN_0803e50a((int)(short)((int)((byte)(&DAT_200000fa)[uVar18] - 3) / 3));
              uVar13 = FUN_0803c8e0((int)DAT_0801f6e8,extraout_s1_09,uVar13);
              uVar37 = FUN_0803ed70(_DAT_20000348);
              FUN_0803e77c(uVar13,extraout_s1_10,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
              _DAT_20000348 = (float)FUN_0803df48();
              uVar18 = (uint)DAT_2000010e;
              uVar13 = extraout_s1_10;
            }
            uVar16 = FUN_0803e5da(*(undefined1 *)(uVar18 + 0x200000fe));
            uVar13 = FUN_0803c8e0((int)DAT_0801f6e8,uVar13,uVar16);
            uVar37 = FUN_0803ed70(_DAT_20000348);
            FUN_0803e77c(uVar13,extraout_s1_11,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
            _DAT_20000348 = (float)FUN_0803df48();
            pbVar12 = (byte *)0x0;
            DAT_20000355 = 0;
            if (fVar36 <= ABS(_DAT_20000348)) {
              _DAT_20000348 = _DAT_20000348 / fVar36;
              pbVar12 = (byte *)0x1;
              DAT_20000355 = 1;
            }
          }
        }
        return pbVar12;
      }
    default:
      goto code_r0x080038ca;
    }
    if ((DAT_2000010f == '\x02') && (DAT_20000126 != '\0')) {
      uRam20002d52 = 8;
      FUN_0803acf0(_DAT_20002d78,&uRam20002d52,0xffffffff);
    }
    pbVar12 = (byte *)(uint)DAT_2000012c;
    if (pbVar12 != (byte *)0x0) {
code_r0x080038ca:
      return pbVar12;
    }
    uRam20002d53 = 6;
    break;
  case (byte *)0x2:
    pbVar12 = (byte *)(uint)DAT_20001061;
    if (pbVar12 == (byte *)0x2) {
      if ((DAT_20001063 & 0xf) == 2) {
        uVar13 = FUN_0803e50a(DAT_20001063 >> 4);
        uVar13 = FUN_0803c8e0((int)uRam080038e8,param_2,uVar13);
        uVar37 = FUN_0803e5da(bRam20000f59);
        FUN_0803dfac(uVar13,extraout_s1_00,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
        uVar14 = FUN_0803e450();
        if (0x1d < uVar14) {
          uVar14 = 0x1e;
        }
        bRam20000f59 = (byte)uVar14;
      }
      else {
        if ((DAT_20001063 & 0xf) != 1) {
          bVar26 = DAT_20001063;
          if ((DAT_20001063 & 0xf) == 0) {
            uVar13 = FUN_0803e50a(DAT_20001063 >> 4);
            uVar13 = FUN_0803c8e0((int)uRam080037d8,param_2,uVar13);
            uVar37 = FUN_0803e5da(uRam20000f54);
            FUN_0803dfac(uVar13,extraout_s1,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
            uVar14 = func_0x0803e4b0();
            uRam20000f54 = 50000;
            if (uVar14 < 50000) {
              uRam20000f54 = uVar14;
            }
            func_0x08022d40();
            bVar26 = DAT_20001063;
          }
          goto code_r0x080035de;
        }
        uVar13 = FUN_0803e50a(DAT_20001063 >> 4);
        uVar13 = FUN_0803c8e0((int)uRam080038e8,param_2,uVar13);
        uVar37 = FUN_0803e5da(bRam20000f58);
        FUN_0803dfac(uVar13,extraout_s1_01,(int)uVar37,(int)((ulonglong)uVar37 >> 0x20));
        uVar14 = FUN_0803e450();
        if (99 < uVar14) {
          uVar14 = 100;
        }
        bRam20000f58 = (byte)uVar14;
      }
      FUN_08022e14();
      bVar26 = DAT_20001063;
    }
    else {
      if (pbVar12 != (byte *)0x1) {
        if (pbVar12 != (byte *)0x0) {
          return pbVar12;
        }
        if (DAT_20000f51 == 0) {
          return (byte *)0x0;
        }
        if ((byte)(DAT_20000f51 - 1) < DAT_20001062) {
          DAT_20001062 = DAT_20000f51 - 1;
        }
        uRam20002d53 = 8;
        DAT_20000f51 = DAT_20000f51 - 1;
        FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
        uRam20002d53 = 0x17;
        FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
        uRam20002d53 = 0x18;
        FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
        uRam20002d53 = 0x19;
        pbVar12 = (byte *)FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
        if (DAT_20000f51 < 0xd) {
          uVar1 = (ushort)DAT_20000f51;
          switch(DAT_20000f51) {
          case 0:
            pbVar12 = (byte *)(uint)bRam20000f59;
            iVar20 = 0;
            do {
              *(short *)(iVar20 + 0x20000f5c) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d84a)) / 0x21) + 0x14;
              iVar19 = iVar20 + 0x14;
              *(short *)(iVar20 + 0x20000f5e) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d84c)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f60) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d84e)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f62) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d850)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f64) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d852)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f68) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d856)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f6a) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d858)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f5a) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d848)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f66) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d854)) / 0x21) + 0x14;
              *(short *)(iVar20 + 0x20000f6c) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d85a)) / 0x21) + 0x14;
              iVar20 = iVar19;
            } while (iVar19 != 200);
            break;
          case 1:
            uVar14 = (uint)bRam20000f58;
            sVar23 = (short)(((uint)bRam20000f59 * 0xfff) / 0x21) + 0x14;
            uVar18 = 0;
            pbVar17 = (byte *)0x20000f80;
            do {
              sVar29 = 0x14;
              if (uVar18 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x26) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 1 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x24) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 2 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x22) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 3 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x20) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 4 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x1e) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 5 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x1c) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 6 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x1a) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 7 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x18) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 8 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x16) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 9 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x14) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 10 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x12) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0xb < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0x10) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0xc < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0xe) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0xd < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -0xc) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0xe < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -10) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0xf < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -8) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0x10 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -6) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0x11 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -4) = sVar29;
              sVar29 = 0x14;
              if (uVar18 + 0x12 < uVar14) {
                sVar29 = sVar23;
              }
              *(short *)(pbVar17 + -2) = sVar29;
              uVar25 = uVar18 + 0x13;
              uVar18 = uVar18 + 0x14;
              sVar29 = 0x14;
              if (uVar25 < uVar14) {
                sVar29 = sVar23;
              }
              pbVar12 = pbVar17 + 0x28;
              *(short *)pbVar17 = sVar29;
              pbVar17 = pbVar12;
            } while (uVar18 != 100);
            break;
          case 2:
            uVar14 = (uint)bRam20000f59;
            uVar25 = (uVar14 * 0xfff) / 0x21;
            uVar18 = (uint)bRam20000f58;
            iVar20 = 100 - uVar18;
            uVar27 = 0;
            iVar19 = 0;
            uVar30 = 0;
            pbVar17 = (byte *)0x20000f6c;
            do {
              if (uVar30 < uVar18) {
                sVar23 = (short)(uVar27 / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 100 + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -0x12) = sVar23 + 0x14;
              if (uVar30 + 1 < uVar18) {
                sVar23 = (short)((uVar25 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 99 + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -0x10) = sVar23 + 0x14;
              if (uVar30 + 2 < uVar18) {
                sVar23 = (short)((uVar25 * 2 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x62 + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -0xe) = sVar23 + 0x14;
              if (uVar30 + 3 < uVar18) {
                sVar23 = (short)((uVar25 * 3 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x61 + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -0xc) = sVar23 + 0x14;
              if (uVar30 + 4 < uVar18) {
                sVar23 = (short)((uVar25 * 4 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x60 + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -10) = sVar23 + 0x14;
              if (uVar30 + 5 < uVar18) {
                sVar23 = (short)((uVar25 * 5 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x5f + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -8) = sVar23 + 0x14;
              if (uVar30 + 6 < uVar18) {
                sVar23 = (short)((uVar25 * 6 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x5e + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -6) = sVar23 + 0x14;
              if (uVar30 + 7 < uVar18) {
                sVar23 = (short)(((uVar25 * 8 - (uVar14 * 0xfff) / 0x21) + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x5d + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -4) = sVar23 + 0x14;
              if (uVar30 + 8 < uVar18) {
                sVar23 = (short)((uVar25 * 8 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x5c + iVar19) / iVar20);
              }
              *(short *)(pbVar17 + -2) = sVar23 + 0x14;
              if (uVar30 + 9 < uVar18) {
                sVar23 = (short)((uVar25 * 9 + uVar27) / uVar18);
              }
              else {
                sVar23 = (short)((int)(uVar25 * 0x5b + iVar19) / iVar20);
              }
              uVar30 = uVar30 + 10;
              pbVar12 = pbVar17 + 0x14;
              *(short *)pbVar17 = sVar23 + 0x14;
              iVar19 = iVar19 + uVar25 * -10;
              uVar27 = uVar27 + uVar25 * 10;
              pbVar17 = pbVar12;
            } while (uVar30 != 100);
            break;
          case 3:
          case 4:
            iVar20 = (short)uVar1 * 200;
            uVar14 = (uint)bRam20000f59;
            iVar19 = 0;
            do {
              uVar1 = *(ushort *)(iVar20 + 0x804d6b8 + iVar19);
              uVar2 = *(ushort *)(iVar20 + 0x804d6bc + iVar19);
              uVar3 = *(ushort *)(iVar20 + 0x804d6be + iVar19);
              uVar4 = *(ushort *)(iVar20 + 0x804d6c0 + iVar19);
              uVar5 = *(ushort *)(iVar20 + 0x804d6c2 + iVar19);
              uVar6 = *(ushort *)(iVar20 + 0x804d6c4 + iVar19);
              uVar7 = *(ushort *)(iVar20 + 0x804d6c6 + iVar19);
              uVar8 = *(ushort *)(iVar20 + 0x804d6c8 + iVar19);
              uVar9 = *(ushort *)(iVar20 + 0x804d6ca + iVar19);
              pbVar12 = (byte *)(iVar19 + 0x200000f8);
              *(short *)(iVar19 + 0x20000f5c) =
                   (short)((uVar14 * *(ushort *)(iVar20 + 0x804d6ba + iVar19)) / 0x21) + 0x14;
              iVar28 = iVar19 + 0x14;
              *(short *)(iVar19 + 0x20000f5e) = (short)((uVar14 * uVar2) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f60) = (short)((uVar14 * uVar3) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f62) = (short)((uVar14 * uVar4) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f64) = (short)((uVar14 * uVar5) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f66) = (short)((uVar14 * uVar6) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f68) = (short)((uVar14 * uVar7) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f5a) = (short)((uVar14 * uVar1) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f6a) = (short)((uVar14 * uVar8) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f6c) = (short)((uVar14 * uVar9) / 0x21) + 0x14;
              iVar19 = iVar28;
            } while (iVar28 != 200);
            break;
          case 5:
            uVar18 = (uint)bRam20000f59;
            pbVar17 = (byte *)0x20000f80;
            uVar14 = 0x98;
            do {
              sVar23 = (short)((uVar18 * 0xfff) / 0xe7);
              *(short *)(pbVar17 + -0x26) = sVar23 * (short)((uVar14 - 0x98) / 100) + 0x14;
              *(short *)(pbVar17 + -0x24) = sVar23 * (short)((uVar14 - 0x90) / 100) + 0x14;
              *(short *)(pbVar17 + -0x22) = sVar23 * (short)((uVar14 - 0x88) / 100) + 0x14;
              *(short *)(pbVar17 + -0x20) = sVar23 * (short)((uVar14 - 0x80) / 100) + 0x14;
              *(short *)(pbVar17 + -0x1e) = sVar23 * (short)((uVar14 - 0x78) / 100) + 0x14;
              *(short *)(pbVar17 + -0x1c) = sVar23 * (short)((uVar14 - 0x70) / 100) + 0x14;
              *(short *)(pbVar17 + -0x1a) = sVar23 * (short)((uVar14 - 0x68) / 100) + 0x14;
              *(short *)(pbVar17 + -0x18) = sVar23 * (short)((uVar14 - 0x60) / 100) + 0x14;
              *(short *)(pbVar17 + -0x16) = sVar23 * (short)((uVar14 - 0x58) / 100) + 0x14;
              uVar25 = uVar14 + 0xa0;
              *(short *)(pbVar17 + -0x14) = sVar23 * (short)((uVar14 - 0x50) / 100) + 0x14;
              *(short *)(pbVar17 + -8) = sVar23 * (short)((uVar14 - 0x20) / 100) + 0x14;
              *(short *)(pbVar17 + -0x12) = sVar23 * (short)((uVar14 - 0x48) / 100) + 0x14;
              *(short *)(pbVar17 + -0x10) = sVar23 * (short)((uVar14 - 0x40) / 100) + 0x14;
              *(short *)(pbVar17 + -0xe) = sVar23 * (short)((uVar14 - 0x38) / 100) + 0x14;
              *(short *)(pbVar17 + -0xc) = sVar23 * (short)((uVar14 - 0x30) / 100) + 0x14;
              *(short *)(pbVar17 + -10) = sVar23 * (short)((uVar14 - 0x28) / 100) + 0x14;
              *(short *)(pbVar17 + -2) = sVar23 * (short)((uVar14 - 8) / 100) + 0x14;
              *(short *)(pbVar17 + -6) = sVar23 * (short)((uVar14 - 0x18) / 100) + 0x14;
              *(short *)(pbVar17 + -4) = sVar23 * (short)((uVar14 - 0x10) / 100) + 0x14;
              pbVar12 = pbVar17 + 0x28;
              *(short *)pbVar17 = sVar23 * (short)(uVar14 / 100) + 0x14;
              pbVar17 = pbVar12;
              uVar14 = uVar25;
            } while (uVar25 != 0x3b8);
            break;
          case 6:
            uVar18 = (uint)bRam20000f59;
            pbVar12 = (byte *)((uVar18 * 0xfff) / 0xe7);
            psVar24 = (short *)0x20000f6c;
            uVar14 = 0x48;
            do {
              sVar29 = (short)pbVar12;
              sVar23 = (short)((uVar18 * 0xfff) / 0x21);
              psVar24[-9] = (sVar23 - sVar29 * (short)((uVar14 - 0x48) / 100)) + 0x14;
              psVar24[-8] = (sVar23 - sVar29 * (short)((uVar14 - 0x40) / 100)) + 0x14;
              psVar24[-7] = (sVar23 - sVar29 * (short)((uVar14 - 0x38) / 100)) + 0x14;
              psVar24[-6] = (sVar23 - sVar29 * (short)((uVar14 - 0x30) / 100)) + 0x14;
              psVar24[-5] = (sVar23 - sVar29 * (short)((uVar14 - 0x28) / 100)) + 0x14;
              uVar25 = uVar14 + 0x50;
              psVar24[-3] = (sVar23 - sVar29 * (short)((uVar14 - 0x18) / 100)) + 0x14;
              psVar24[-2] = (sVar23 - sVar29 * (short)((uVar14 - 0x10) / 100)) + 0x14;
              psVar24[-1] = (sVar23 - sVar29 * (short)((uVar14 - 8) / 100)) + 0x14;
              psVar24[-4] = (sVar23 - sVar29 * (short)((uVar14 - 0x20) / 100)) + 0x14;
              *psVar24 = (sVar23 - sVar29 * (short)(uVar14 / 100)) + 0x14;
              psVar24 = psVar24 + 10;
              uVar14 = uVar25;
            } while (uVar25 != 0x368);
            break;
          case 7:
          case 8:
            iVar20 = (short)uVar1 * 200;
            pbVar12 = (byte *)(uint)bRam20000f59;
            iVar19 = 0;
            do {
              uVar1 = *(ushort *)(iVar20 + 0x804d528 + iVar19);
              uVar2 = *(ushort *)(iVar20 + 0x804d52c + iVar19);
              uVar3 = *(ushort *)(iVar20 + 0x804d52e + iVar19);
              uVar4 = *(ushort *)(iVar20 + 0x804d530 + iVar19);
              uVar5 = *(ushort *)(iVar20 + 0x804d532 + iVar19);
              uVar6 = *(ushort *)(iVar20 + 0x804d534 + iVar19);
              uVar7 = *(ushort *)(iVar20 + 0x804d536 + iVar19);
              uVar8 = *(ushort *)(iVar20 + 0x804d538 + iVar19);
              uVar9 = *(ushort *)(iVar20 + 0x804d53a + iVar19);
              iVar28 = iVar19 + 0x14;
              *(short *)(iVar19 + 0x20000f5c) =
                   (short)(((int)pbVar12 * (uint)*(ushort *)(iVar20 + 0x804d52a + iVar19)) / 0x21) +
                   0x14;
              *(short *)(iVar19 + 0x20000f5e) = (short)(((int)pbVar12 * (uint)uVar2) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f60) = (short)(((int)pbVar12 * (uint)uVar3) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f62) = (short)(((int)pbVar12 * (uint)uVar4) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f64) = (short)(((int)pbVar12 * (uint)uVar5) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f66) = (short)(((int)pbVar12 * (uint)uVar6) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f68) = (short)(((int)pbVar12 * (uint)uVar7) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f5a) = (short)(((int)pbVar12 * (uint)uVar1) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f6a) = (short)(((int)pbVar12 * (uint)uVar8) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f6c) = (short)(((int)pbVar12 * (uint)uVar9) / 0x21) + 0x14;
              iVar19 = iVar28;
            } while (iVar28 != 200);
            break;
          case 9:
            pbVar12 = (byte *)(((uint)bRam20000f59 * 0xfff) / 0x21 + 0x14);
            iVar20 = 0;
            do {
              iVar19 = iVar20 + 0x32;
              uVar10 = SUB42(pbVar12,0);
              *(undefined2 *)(iVar20 + 0x20000f5a) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f5c) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f5e) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f60) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f62) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f64) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f66) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f68) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f6a) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f6c) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f6e) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f70) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f72) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f74) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f76) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f78) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f7a) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f7c) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f7e) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f80) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f82) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f84) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f86) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f88) = uVar10;
              *(undefined2 *)(iVar20 + 0x20000f8a) = uVar10;
              iVar20 = iVar19;
            } while (iVar19 != 200);
            break;
          default:
            iVar20 = (short)uVar1 * 200;
            uVar14 = (uint)bRam20000f59;
            iVar19 = 0;
            do {
              uVar1 = *(ushort *)(iVar20 + 0x804d460 + iVar19);
              uVar2 = *(ushort *)(iVar20 + 0x804d464 + iVar19);
              uVar3 = *(ushort *)(iVar20 + 0x804d466 + iVar19);
              uVar4 = *(ushort *)(iVar20 + 0x804d468 + iVar19);
              uVar5 = *(ushort *)(iVar20 + 0x804d46a + iVar19);
              uVar6 = *(ushort *)(iVar20 + 0x804d46c + iVar19);
              uVar7 = *(ushort *)(iVar20 + 0x804d46e + iVar19);
              uVar8 = *(ushort *)(iVar20 + 0x804d470 + iVar19);
              uVar9 = *(ushort *)(iVar20 + 0x804d472 + iVar19);
              iVar28 = iVar19 + 0x14;
              *(short *)(iVar19 + 0x20000f5c) =
                   (short)((uVar14 * *(ushort *)(iVar20 + 0x804d462 + iVar19)) / 0x21) + 0x14;
              pbVar12 = (byte *)((uVar14 * uVar9) / 0x21 + 0x14);
              *(short *)(iVar19 + 0x20000f5e) = (short)((uVar14 * uVar2) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f60) = (short)((uVar14 * uVar3) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f62) = (short)((uVar14 * uVar4) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f64) = (short)((uVar14 * uVar5) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f66) = (short)((uVar14 * uVar6) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f68) = (short)((uVar14 * uVar7) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f5a) = (short)((uVar14 * uVar1) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f6a) = (short)((uVar14 * uVar8) / 0x21) + 0x14;
              *(short *)(iVar19 + 0x20000f6c) = (short)pbVar12;
              iVar19 = iVar28;
            } while (iVar28 != 200);
          }
        }
        return pbVar12;
      }
      if (DAT_20001063 == 0) {
        return (byte *)0x0;
      }
      bVar26 = 0;
      if (DAT_20000f51 - 1 < 2) {
        bVar26 = DAT_20001063 - 1;
      }
    }
code_r0x080035de:
    DAT_20001063 = bVar26;
    uRam20002d53 = 0x19;
    break;
  case (byte *)0x3:
    pbVar12 = (byte *)(uint)DAT_20001061;
    if (pbVar12 != (byte *)0x1) {
      if (pbVar12 != (byte *)0x0) {
        return pbVar12;
      }
      if ((DAT_20001062 & 0xf) == 0) {
        return (byte *)(uint)DAT_20001062;
      }
      pbVar12 = (byte *)(uint)DAT_20001062 + -1;
      bVar26 = (byte)pbVar12;
      if (((uint)pbVar12 & 0xf) < ((uint)pbVar12 & 0xff) >> 4) {
        bVar26 = DAT_20001062 - 0x11;
      }
      DAT_20001062 = bVar26;
      uRam20002d53 = 0x20;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      uRam20002d53 = 0x21;
      goto code_r0x080033c0;
    }
    if (DAT_20001063 == 0) {
      return (byte *)0x0;
    }
    DAT_20001063 = DAT_20001063 - 1;
    uRam20002d53 = 0x21;
    break;
  case (byte *)0x4:
    pbVar12 = (byte *)(uint)DAT_20000f14;
    if (pbVar12 != (byte *)0x2) {
      if (pbVar12 != (byte *)0x0) {
        return pbVar12;
      }
      if ((byte *)(uint)bRam20000f15 < (byte *)0x3) {
        return (byte *)(uint)bRam20000f15;
      }
      bRam20000f15 = bRam20000f15 - 3;
      uRam20002d53 = 0x27;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      uRam20002d53 = 0x28;
      goto code_r0x080033c0;
    }
    goto code_r0x08003324;
  case (byte *)0x5:
    if ((byte *)(uint)DAT_20000f14 != (byte *)0x2) {
      return (byte *)(uint)DAT_20000f14;
    }
code_r0x08003324:
    DAT_20000f14 = 1;
    uRam20002d53 = 0x2a;
    break;
  default:
    goto code_r0x080038ca;
  case (byte *)0x7:
    if ((byte *)(uint)DAT_20001058 < (byte *)0x2) {
      return (byte *)(uint)DAT_20001058;
    }
    DAT_20001058 = DAT_20001058 - 1;
    uRam20002d53 = 0x2c;
    break;
  case (byte *)0x8:
    pbVar12 = (byte *)(uint)DAT_20001061;
    if (pbVar12 == (byte *)0x2) {
      if (DAT_20001063 >> 4 == 0) {
        return (byte *)(uint)DAT_20001063;
      }
      if (DAT_20001063 >> 4 == 8) {
        cVar21 = '\r';
      }
      else {
        if (DAT_20001063 >> 4 != 4) {
          DAT_20001063 = DAT_20001063 - 0x10;
          goto code_r0x08003602;
        }
        cVar21 = '\f';
      }
      DAT_20001063 = DAT_20001063 & 0xf | cVar21 << 4;
    }
    else {
      if (pbVar12 != (byte *)0x1) {
        if (pbVar12 != (byte *)0x0) {
          return pbVar12;
        }
        if ((DAT_20001062 & 0xf) == 0) {
          return (byte *)(uint)DAT_20001062;
        }
        pbVar12 = (byte *)(uint)DAT_20001062 + -1;
        bVar26 = (byte)pbVar12;
        if (((uint)pbVar12 & 0xf) < ((uint)pbVar12 & 0xff) >> 4) {
          bVar26 = DAT_20001062 - 0x11;
        }
        DAT_20001062 = bVar26;
        uRam20002d53 = 0x13;
        FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
        uRam20002d53 = 0x14;
        goto code_r0x080033c0;
      }
      if (DAT_20001063 == 0) {
        return (byte *)0x0;
      }
      bVar31 = DAT_20001063 - 1;
      bVar26 = 0;
      if ((DAT_20001062 & 0xf) != 7) {
        bVar26 = bVar31;
      }
      bVar32 = DAT_20001063 != 5;
      DAT_20001063 = bVar26;
      if (bVar32) {
        DAT_20001063 = bVar31;
      }
    }
code_r0x08003602:
    uRam20002d53 = 0x14;
  }
  puStack_24 = (undefined2 *)&uRam20002d53;
  iVar20 = _DAT_20002d6c;
code_r0x0800361c:
  iStack_28 = -1;
  bVar32 = false;
  if (iVar20 == 0) {
    bVar32 = (bool)isCurrentModePrivileged();
    if (bVar32) {
      setBasePriority(0x10);
    }
    InstructionSynchronizationBarrier(0xf);
    DataSynchronizationBarrier(0xf);
    do {
                    /* WARNING: Do nothing block with infinite loop */
    } while( true );
  }
  bVar33 = false;
  if (puStack_24 == (undefined2 *)0x0) {
    bVar33 = *(int *)(iVar20 + 0x40) != 0;
  }
  if (!bVar33) {
    iVar19 = FUN_0803b730();
    if (iVar19 == 0 && iStack_28 != 0) {
      bVar32 = (bool)isCurrentModePrivileged();
      if (bVar32) {
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
      if (*(uint *)(iVar20 + 0x38) < *(uint *)(iVar20 + 0x3c)) {
        iVar19 = FUN_08034cc4(iVar20,puStack_24,0);
        uStack_38 = CONCAT44(iVar19,(undefined4)uStack_38);
        if (*(int *)(iVar20 + 0x24) == 0) {
          if (iVar19 != 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
        else {
          iVar20 = FUN_0803bb4c(iVar20 + 0x24);
          if (iVar20 != 0) {
            _DAT_e000ed04 = 0x10000000;
            DataSynchronizationBarrier(0xf);
            InstructionSynchronizationBarrier(0xf);
          }
        }
        FUN_0803a1b0();
        return (byte *)0x1;
      }
      if (iStack_28 == 0) break;
      if (!bVar32) {
        FUN_0803a400(&uStack_3c);
        bVar32 = true;
      }
      FUN_0803a1b0();
      FUN_0803a904();
      FUN_0803a168();
      if (*(char *)(iVar20 + 0x44) == -1) {
        *(undefined1 *)(iVar20 + 0x44) = 0;
      }
      if (*(char *)(iVar20 + 0x45) == -1) {
        *(undefined1 *)(iVar20 + 0x45) = 0;
      }
      FUN_0803a1b0();
      iVar19 = FUN_0803b5cc(&uStack_3c,&iStack_28);
      if (iVar19 != 0) {
        FUN_080357b8(iVar20);
        FUN_0803bc10();
        return (byte *)0x0;
      }
      iVar19 = FUN_080352d4(iVar20);
      if (iVar19 == 0) {
        FUN_080357b8(iVar20);
        FUN_0803bc10();
      }
      else {
        FUN_0803a434(iVar20 + 0x10,iStack_28);
        FUN_080357b8(iVar20);
        iVar19 = FUN_0803bc10();
        if (iVar19 == 0) {
          _DAT_e000ed04 = 0x10000000;
          DataSynchronizationBarrier(0xf);
          InstructionSynchronizationBarrier(0xf);
        }
      }
    }
    FUN_0803a1b0();
    return (byte *)0x0;
  }
  bVar32 = (bool)isCurrentModePrivileged();
  if (bVar32) {
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



// Attempting to create function at 0x08006548
// Successfully created function
// ==========================================
// Function: FUN_08006548 @ 08006548
// Size: 1 bytes
// ==========================================

/* WARNING: Removing unreachable block (ram,0x0803ad26) */
/* WARNING: Removing unreachable block (ram,0x0803ad30) */
/* WARNING: Removing unreachable block (ram,0x0803ad32) */
/* WARNING: Removing unreachable block (ram,0x0803ad58) */
/* WARNING: Removing unreachable block (ram,0x0803ad62) */
/* WARNING: Removing unreachable block (ram,0x0803ad64) */
/* WARNING: Heritage AFTER dead removal. Example location: s1 : 0x08002920 */
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

uint FUN_08006548(undefined4 param_1,undefined4 param_2)

{
  bool bVar1;
  byte bVar2;
  undefined4 uVar3;
  uint uVar4;
  int iVar5;
  int iVar6;
  undefined1 uVar7;
  uint uVar8;
  float fVar9;
  char cVar10;
  undefined4 uVar11;
  undefined4 extraout_s1;
  undefined4 extraout_s1_00;
  undefined4 uVar12;
  undefined4 uVar13;
  undefined8 uVar14;
  undefined8 uVar15;
  undefined1 auStack_3c [3];
  undefined1 uStack_39;
  undefined4 uStack_14;
  
  switch(_DAT_20001060 & 0xff) {
  case 1:
    if ((bRam20001055 & 0xf0) == 0xb0) {
      return 0x200000f8;
    }
    bRam20001055 = 0;
    if (7 < DAT_20001025) {
      bRam20001055 = 0;
      return 0x200000f8;
    }
    if ((1 << (uint)DAT_20001025 & 0xdaU) == 0) {
      bRam20001055 = 0;
      return 0x200000f8;
    }
    if (DAT_20001035 == '\0') {
      if (DAT_2000102d == '\0') {
        DAT_20001035 = '\x01';
        _DAT_20001038 = _DAT_20001028;
        DAT_2000103c = DAT_2000102f;
        uRam20001040 = 0x7fc00000;
        uRam20001044 = 0x7fc00000;
        uRam20001048 = 0x7fc00000;
      }
    }
    else {
      DAT_20001035 = '\0';
    }
    uRam20002d53 = 0x1a;
    FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
    if (DAT_20001035 != '\0') {
      uVar14 = FUN_0803ed70(_DAT_20001038);
      uVar3 = FUN_0803e5da(DAT_2000103c);
      uVar12 = (undefined4)DAT_08002bf0;
      uVar13 = (undefined4)((ulonglong)DAT_08002bf0 >> 0x20);
      uVar3 = FUN_0803c8e0(uVar12,param_2,uVar3);
      uVar14 = FUN_0803e77c(uVar3,extraout_s1,(int)uVar14,(int)((ulonglong)uVar14 >> 0x20));
      uVar15 = FUN_0803ed70(_DAT_20001028);
      uVar3 = FUN_0803e5da(DAT_2000102f);
      uVar3 = FUN_0803c8e0(uVar12,extraout_s1,uVar3);
      uVar15 = FUN_0803e77c(uVar3,extraout_s1_00,(int)uVar15,(int)((ulonglong)uVar15 >> 0x20));
      uVar14 = FUN_0803eb94((int)uVar15,(int)((ulonglong)uVar15 >> 0x20),(int)uVar14,
                            (int)((ulonglong)uVar14 >> 0x20));
      uVar4 = (uint)((ulonglong)uVar14 >> 0x20);
      iVar6 = (int)((longlong)DAT_08002bf8 >> 0x3f);
      uVar3 = (undefined4)((ulonglong)DAT_08002c00 >> 0x20);
      uVar11 = (undefined4)DAT_08002c00;
      uVar8 = uVar4 & 0x7fffffff | iVar6 * -0x80000000;
      DAT_2000102f = '\0';
      iVar5 = FUN_0803ee0c((int)uVar14,uVar8,uVar11,uVar3);
      cVar10 = iVar5 == 0;
      if ((bool)cVar10) {
        DAT_2000102f = '\x01';
        uVar14 = FUN_0803e124((int)uVar14,uVar4,uVar12,uVar13);
        uVar8 = (uint)((ulonglong)uVar14 >> 0x20) & 0x7fffffff | iVar6 * -0x80000000;
      }
      iVar5 = FUN_0803ee0c((int)uVar14,uVar8,uVar11,uVar3);
      if (iVar5 == 0) {
        cVar10 = cVar10 + '\x01';
        DAT_2000102f = cVar10;
        uVar14 = FUN_0803e124((int)uVar14,(int)((ulonglong)uVar14 >> 0x20),uVar12,uVar13);
        uVar8 = (uint)((ulonglong)uVar14 >> 0x20) & 0x7fffffff | iVar6 * -0x80000000;
      }
      iVar5 = FUN_0803ee0c((int)uVar14,uVar8,uVar11,uVar3);
      if (iVar5 == 0) {
        cVar10 = cVar10 + '\x01';
        DAT_2000102f = cVar10;
        uVar14 = FUN_0803e124((int)uVar14,(int)((ulonglong)uVar14 >> 0x20),uVar12,uVar13);
        uVar8 = (uint)((ulonglong)uVar14 >> 0x20) & 0x7fffffff | iVar6 * -0x80000000;
      }
      iVar6 = FUN_0803ee0c((int)uVar14,uVar8,uVar11,uVar3);
      if (iVar6 == 0) {
        DAT_2000102f = cVar10 + '\x01';
        uVar14 = FUN_0803e124((int)uVar14,(int)((ulonglong)uVar14 >> 0x20),uVar12,uVar13);
      }
      _DAT_20001028 = (float)FUN_0803df48((int)uVar14,(int)((ulonglong)uVar14 >> 0x20));
      fVar9 = ABS(_DAT_20001028);
      if ((fVar9 < DAT_08002c08) || (DAT_2000102c < 2)) {
        if ((fVar9 < DAT_08002c0c) || (DAT_2000102c < 3)) {
          if ((10.0 <= fVar9) && (3 < DAT_2000102c)) {
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
      bVar2 = 5;
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
          bVar2 = 3;
          goto code_r0x08002b98;
        }
        goto code_r0x08002b9c;
      case 3:
code_r0x08002b98:
        DAT_20001026 = bVar2;
code_r0x08002b9c:
        DAT_20001030 = DAT_2000102f + '\x02';
      }
      uStack_39 = 0x1b;
      FUN_0803acf0(_DAT_20002d6c,&uStack_39,0xffffffff);
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
    uStack_39 = 0x1c;
    FUN_0803acf0(_DAT_20002d6c,&uStack_39,0xffffffff);
    uStack_39 = 0x1e;
    uVar4 = FUN_0803acf0(_DAT_20002d6c,&uStack_39,0xffffffff);
    return uVar4;
  case 2:
    _DAT_20001060 = 0x20109;
    cRam2000044d = '\x01';
    break;
  default:
    return 0x200000f8;
  case 5:
    _DAT_20001060 = CONCAT31(_DAT_20001061,2);
    _DAT_20000f14 = 0;
    uRam20000f0a = 0;
    uRam20000f0e = 0;
    uRam20000f12 = 0;
    break;
  case 6:
    _DAT_20001060 = CONCAT31(_DAT_20001061,5);
    break;
  case 9:
    if (cRam2000044d == '\0') {
      return 0x200000f8;
    }
    if (DAT_20001062 != '\x02') {
      _DAT_20001060 = (uint)CONCAT21(0x201,DAT_20001060);
      uRam20002d53 = 0x13;
      FUN_0803acf0(_DAT_20002d6c,&uRam20002d53,0xffffffff);
      iVar6 = _DAT_20002d6c;
      uRam20002d53 = 0x14;
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
            FUN_0803a400(auStack_3c);
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
          iVar5 = FUN_0803b5cc(auStack_3c,&stack0xffffffd8);
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
    cRam2000044d = '\0';
    _DAT_20001060 = 2;
  }
  uVar4 = _DAT_20001060 & 0xff;
  switch(uVar4) {
  case 0:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(1,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0xb,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0xc,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0xd,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0xe,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0xf,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x10,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 0x11;
    break;
  case 1:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(9,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uVar7 = 10;
    }
    uStack_14 = CONCAT13(uVar7,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x1a,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x1b,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x1c,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x1d,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 0x1e;
    break;
  case 2:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = CONCAT13(2,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(3,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(4,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(5,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(6,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 8;
    goto code_r0x0800bc6e;
  case 3:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(8,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(9,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uVar7 = 10;
    }
    uStack_14 = CONCAT13(uVar7,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x16,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x17,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x18,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 0x19;
    break;
  case 4:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x1f,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(9,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x20,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 0x21;
    break;
  case 5:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x25,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(9,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x26,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x27,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 0x28;
    break;
  case 6:
    uVar7 = 0x29;
    break;
  case 7:
    uVar7 = 0x15;
    break;
  case 8:
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
    uVar7 = 0x2c;
    break;
  case 9:
    iVar6 = (int)&uStack_14 + 3;
    uStack_14 = uStack_14 & 0xffffff;
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x12,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uStack_14 = CONCAT13(0x13,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,iVar6,0xffffffff);
    uVar7 = 0x14;
code_r0x0800bc6e:
    uStack_14 = CONCAT13(uVar7,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
    uStack_14 = CONCAT13(9,(undefined3)uStack_14);
    FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
    uVar7 = 7;
    if (-1 < _DAT_40011008 << 0x18) {
      uVar7 = 10;
    }
    break;
  default:
    goto LAB_0800bcce;
  }
  uStack_14 = CONCAT13(uVar7,(undefined3)uStack_14);
  uVar4 = FUN_0803acf0(_DAT_20002d6c,(int)&uStack_14 + 3,0xffffffff);
LAB_0800bcce:
  return uVar4;
}



