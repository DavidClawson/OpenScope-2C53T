// Force-decompiled functions

// Attempting to create function at 0x0801584e
// Successfully created function
// ==========================================
// Function: FUN_0801584e @ 0801584e
// Size: 390 bytes
// ==========================================

void FUN_0801584e(undefined4 param_1,undefined1 param_2)

{
  undefined4 *unaff_r5;
  int unaff_r9;
  undefined1 uStack0000003f;
  
  uStack0000003f = param_2;
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



// Attempting to create function at 0x08015864
// Successfully created function
// ==========================================
// Function: FUN_08015864 @ 08015864
// Size: 368 bytes
// ==========================================

void FUN_08015864(undefined4 param_1,undefined1 param_2)

{
  undefined4 *unaff_r5;
  int unaff_r9;
  undefined1 uStack0000003f;
  
  uStack0000003f = param_2;
  FUN_0803acf0(*unaff_r5);
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



// Attempting to create function at 0x08037cce
// Successfully created function
// ==========================================
// Function: FUN_08037cce @ 08037cce
// Size: 78 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_08037cce(undefined4 *param_1,undefined4 param_2,undefined4 param_3)

{
  char cVar1;
  short sVar2;
  short sVar3;
  char cVar4;
  byte bVar5;
  int iVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  uint uVar10;
  byte bVar11;
  byte bVar12;
  uint uVar13;
  int iVar14;
  uint uVar15;
  byte bVar16;
  ushort uVar17;
  int iVar18;
  uint uVar19;
  byte bVar20;
  byte bVar21;
  uint uVar22;
  uint uVar23;
  int iVar24;
  uint uVar25;
  uint uVar26;
  uint *unaff_r7;
  int unaff_r9;
  uint uVar27;
  int iVar28;
  char cVar29;
  uint uVar30;
  bool bVar31;
  bool bVar32;
  uint in_fpscr;
  float fVar33;
  float fVar34;
  float fVar35;
  float fVar36;
  float unaff_s16;
  float unaff_s18;
  float unaff_s20;
  float unaff_s22;
  float unaff_s24;
  float unaff_s26;
  float unaff_s28;
  int iStack00000020;
  int iStack00000028;
  uint uStack0000002c;
  uint uStack00000030;
  undefined4 in_stack_00000034;
  
code_r0x08037cce:
  in_stack_00000034._3_1_ = 2;
  FUN_0803acf0(*param_1,(int)&stack0x00000034 + 3,param_3);
  FUN_08034078();
  if (*(short *)(unaff_r9 + 0xdb6) != 0) {
    uVar13 = (uint)*(ushort *)(unaff_r9 + 0xdb4);
    uVar15 = uVar13;
    do {
      iVar14 = uVar15 + unaff_r9;
      *(undefined1 *)(iVar14 + 0x5b0) = *(undefined1 *)(iVar14 + 0x356);
      *(undefined1 *)(iVar14 + 0x9b0) = *(undefined1 *)(iVar14 + 0x483);
      uVar13 = uVar13 + 1;
      uVar15 = uVar13 & 0xffff;
    } while (uVar15 < (uint)*(ushort *)(unaff_r9 + 0xdb4) + (uint)*(ushort *)(unaff_r9 + 0xdb6));
  }
switchD_08037536_default:
  _DAT_40010c10 = 0x40;
  do {
    iVar14 = FUN_0803b1d8(_DAT_20002d78,(int)&stack0x00000034 + 2,0xffffffff);
  } while (iVar14 != 1);
  uVar13 = (int)*(short *)(unaff_r9 + 0x1c) ^ 0xffffff80;
  uVar15 = uVar13;
  if (((*(char *)(unaff_r9 + 0x352) != '\0') && (*(char *)(unaff_r9 + 0x352) != -1)) &&
     ((*(char *)(unaff_r9 + 0x353) != '\0' &&
      ((*(char *)(unaff_r9 + 0x353) != -1 && (*(char *)(unaff_r9 + 0x33) == '\0')))))) {
    iVar14 = (uint)*(byte *)(unaff_r9 + 0x16) + unaff_r9;
    iVar18 = (int)*(char *)(iVar14 + 4);
    fVar33 = (float)VectorUnsignedToFloat
                              ((uint)*(byte *)(iVar14 + 0x352),(byte)(in_fpscr >> 0x15) & 3);
    fVar34 = (float)VectorSignedToFloat(*(short *)(unaff_r9 + 0x1c) - iVar18,
                                        (byte)(in_fpscr >> 0x15) & 3);
    fVar35 = (float)VectorSignedToFloat(iVar18,(byte)(in_fpscr >> 0x15) & 3);
    fVar34 = ((fVar33 + unaff_s16) / unaff_s28) * fVar34 + fVar35 + unaff_s22;
    uVar15 = in_fpscr & 0xfffffff;
    uVar22 = uVar15 | (uint)(fVar34 < unaff_s24) << 0x1f | (uint)(fVar34 == unaff_s24) << 0x1e;
    in_fpscr = uVar22 | (uint)(NAN(fVar34) || NAN(unaff_s24)) << 0x1c;
    bVar16 = (byte)(uVar22 >> 0x18);
    fVar33 = unaff_s24;
    if (((bool)(bVar16 >> 6 & 1) || bVar16 >> 7 != ((byte)(in_fpscr >> 0x1c) & 1)) &&
       (uVar15 = uVar15 | (uint)(fVar34 < 0.0) << 0x1f,
       in_fpscr = uVar15 | (uint)NAN(fVar34) << 0x1c, fVar33 = fVar34,
       (byte)(uVar15 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1))) {
      fVar33 = unaff_s26;
    }
    uVar15 = (int)fVar33;
  }
  _DAT_40010c14 = 0x40;
  do {
    bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
  } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
  unaff_r7[1] = (uint)in_stack_00000034._2_1_;
  do {
    bVar31 = false;
    if ((*unaff_r7 & 1) == 0) {
      bVar31 = (*unaff_r7 & 1) == 0;
    }
    bVar32 = false;
    if (bVar31) {
      bVar32 = (*unaff_r7 & 1) == 0;
    }
  } while ((bVar32) && ((*unaff_r7 & 1) == 0));
  uVar22 = unaff_r7[1];
  switch((uint)in_stack_00000034._2_1_) {
  case 1:
    uVar15 = (uint)*(byte *)(unaff_r9 + 0x2d);
    if (*(ushort *)(unaff_r9 + 0xdb8) < (ushort)((byte)(&DAT_0804d833)[uVar15] + 0x32)) {
      do {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
      } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
      unaff_r7[1] = uVar15;
      do {
        bVar31 = false;
        if ((*unaff_r7 & 1) == 0) {
          bVar31 = (*unaff_r7 & 1) == 0;
        }
        bVar32 = false;
        if (bVar31) {
          bVar32 = (*unaff_r7 & 1) == 0;
        }
      } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    }
    else {
      *(undefined2 *)(unaff_r9 + 0xdb8) = 0;
      if (uVar15 < 0x13) {
        do {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
          if (!bVar31) {
            bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
          }
          if (!bVar31) {
            bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
          }
        } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
        unaff_r7[1] = uVar15 + 1;
        do {
          bVar31 = false;
          if ((*unaff_r7 & 1) == 0) {
            bVar31 = (*unaff_r7 & 1) == 0;
          }
          bVar32 = false;
          if (bVar31) {
            bVar32 = (*unaff_r7 & 1) == 0;
          }
        } while ((bVar32) && ((*unaff_r7 & 1) == 0));
      }
      else {
        do {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
          if (!bVar31) {
            bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
          }
          if (!bVar31) {
            bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
          }
        } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
        unaff_r7[1] = 0x12;
        do {
          bVar31 = false;
          if ((*unaff_r7 & 1) == 0) {
            bVar31 = (*unaff_r7 & 1) == 0;
          }
          bVar32 = false;
          if (bVar31) {
            bVar32 = (*unaff_r7 & 1) == 0;
          }
        } while ((bVar32) && ((*unaff_r7 & 1) == 0));
      }
    }
    goto switchD_08037536_default;
  case 2:
    if (*(byte *)(unaff_r9 + 0x14) == 0) {
      do {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
      } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
      unaff_r7[1] = 3;
      do {
        bVar31 = false;
        if ((*unaff_r7 & 1) == 0) {
          bVar31 = (*unaff_r7 & 1) == 0;
        }
        bVar32 = false;
        if (bVar31) {
          bVar32 = (*unaff_r7 & 1) == 0;
        }
      } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    }
    else {
      do {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
      } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
      unaff_r7[1] = (uint)*(byte *)(unaff_r9 + 0x14);
      do {
        bVar31 = false;
        if ((*unaff_r7 & 1) == 0) {
          bVar31 = (*unaff_r7 & 1) == 0;
        }
        bVar32 = false;
        if (bVar31) {
          bVar32 = (*unaff_r7 & 1) == 0;
        }
      } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    }
    goto switchD_08037536_default;
  case 3:
    goto switchD_08037536_caseD_3;
  case 4:
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    uVar15 = 0;
    do {
      do {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
      } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
      unaff_r7[1] = 0xff;
      do {
        bVar31 = false;
        if ((*unaff_r7 & 1) == 0) {
          bVar31 = (*unaff_r7 & 1) == 0;
        }
        bVar32 = false;
        if (bVar31) {
          bVar32 = (*unaff_r7 & 1) == 0;
        }
      } while ((bVar32) && ((*unaff_r7 & 1) == 0));
      *(char *)(unaff_r9 + uVar15 + 0x5b0) = (char)unaff_r7[1];
      uVar13 = uVar15 | 1;
      while (-1 < (int)(*unaff_r7 << 0x1e)) {
        uVar22 = *unaff_r7 << 0x1e;
        bVar31 = (int)uVar22 < 0;
        if (!bVar31) {
          uVar22 = *unaff_r7;
        }
        if ((bVar31 || (int)(uVar22 << 0x1e) < 0) || ((int)(*unaff_r7 << 0x1e) < 0)) break;
      }
      unaff_r7[1] = 0xff;
      while ((*unaff_r7 & 1) == 0) {
        uVar22 = *unaff_r7 << 0x1f;
        bVar31 = uVar22 != 0;
        if (!bVar31) {
          uVar22 = *unaff_r7;
        }
        if ((bVar31 || (uVar22 & 1) != 0) || ((*unaff_r7 & 1) != 0)) break;
      }
      uVar15 = uVar15 + 2;
      *(char *)(uVar13 + unaff_r9 + 0x5b0) = (char)unaff_r7[1];
    } while (uVar15 != 0x400);
    cVar29 = *(char *)(unaff_r9 + 0x14);
    bVar16 = *(byte *)(unaff_r9 + 0x2d);
    if ((((*(char *)(unaff_r9 + 0x33) == '\0') && (1 < (byte)(*(char *)(unaff_r9 + 0x353) + 1U))) &&
        (cVar29 == '\x03' || 3 < bVar16)) && (1 < (*(byte *)(unaff_r9 + 0x352) + 1 & 0xff))) {
      fVar33 = (float)VectorUnsignedToFloat
                                ((uint)*(byte *)(unaff_r9 + 0x352),(byte)(in_fpscr >> 0x15) & 3);
      fVar33 = (fVar33 + unaff_s16) / unaff_s28;
      iVar14 = 0;
      fVar34 = (float)VectorSignedToFloat((int)*(char *)(unaff_r9 + 4),(byte)(in_fpscr >> 0x15) & 3)
      ;
      do {
        iVar18 = unaff_r9 + iVar14;
        fVar35 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b0),(byte)(in_fpscr >> 0x15) & 3);
        fVar36 = ((fVar35 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = in_fpscr & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = in_fpscr & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b1),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b0) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar13 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b2),(byte)(uVar13 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b1) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b3),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b2) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar13 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b4),(byte)(uVar13 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b3) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b5),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b4) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar13 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b6),(byte)(uVar13 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b5) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x5b7),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x5b6) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        in_fpscr = uVar13 | (uint)(NAN(fVar36) || NAN(unaff_s24)) << 0x1c;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || bVar11 >> 7 != ((byte)(in_fpscr >> 0x1c) & 1)) &&
           (uVar15 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f,
           in_fpscr = uVar15 | (uint)NAN(fVar36) << 0x1c, fVar35 = fVar36,
           (byte)(uVar15 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1))) {
          fVar35 = unaff_s26;
        }
        iVar14 = iVar14 + 8;
        *(char *)(iVar18 + 0x5b7) = (char)(int)fVar35;
      } while (iVar14 != 0x400);
    }
    if (*(char *)(unaff_r9 + 0x17) == '\x02') {
      uVar15 = 0;
      do {
        uVar13 = uVar15 & 0xffff;
        sVar3 = (short)uVar15;
        if (cVar29 == '\x03' || 3 < bVar16) {
          uVar25 = uVar15 + 1;
          uVar22 = uVar15;
          if (uVar13 >> 10 != 0) {
            uVar22 = uVar15 + 0xfc00;
          }
          uVar27 = uVar25;
          if (0x3fe < uVar13) {
            uVar27 = uVar15 - 0x3ff;
          }
          iVar14 = unaff_r9 + 0x5b0;
          uVar30 = uVar15 + 2;
          uVar23 = uVar15 + 3;
          uVar26 = uVar30;
          if (0x3fd < uVar13) {
            uVar26 = uVar15 - 0x3fe;
          }
          uVar19 = uVar23;
          if (0x3fc < uVar13) {
            uVar19 = uVar15 - 0x3fd;
          }
          uVar17 = sVar3 + 4;
          if (0xfe < uVar13 >> 2) {
            uVar17 = sVar3 - 0x3fc;
          }
          if (0x3fe < uVar15) {
            uVar25 = uVar15 + 0xfc01;
          }
          if (0x3fd < uVar15) {
            uVar30 = uVar15 + 0xfc02;
          }
          iVar28 = (uint)*(byte *)(iVar14 + (uVar27 & 0xffff)) -
                   (uint)*(byte *)(iVar14 + (uVar30 & 0xffff));
          if ((uVar23 & 0xffff) >> 10 != 0) {
            uVar23 = uVar15 - 0x3fd;
          }
          uVar13 = (uint)*(byte *)(iVar14 + (uint)uVar17);
          uVar25 = (uint)*(byte *)(iVar14 + (uVar25 & 0xffff));
          bVar11 = *(byte *)(iVar14 + (uVar23 & 0xffff));
          iVar18 = *(byte *)(iVar14 + (uVar22 & 0xffff)) - uVar25;
          iVar6 = *(byte *)(iVar14 + (uVar19 & 0xffff)) - uVar25;
          iVar14 = (uint)*(byte *)(iVar14 + (uVar26 & 0xffff)) - (uint)bVar11;
        }
        else {
          uVar22 = uVar15;
          if (uVar13 >> 0xb != 0) {
            uVar22 = uVar15 + 0xf800;
          }
          uVar25 = (uVar22 << 0x10) >> 0x11;
          if (cVar29 == '\x01') {
            if ((uVar22 & 1) == 0) {
              uVar22 = (uint)*(byte *)(uVar25 + unaff_r9 + 0x5b0);
            }
            else {
              bVar11 = *(byte *)(uVar25 + unaff_r9 + 0x9b0);
              bVar20 = *(byte *)(unaff_r9 + 0x350);
LAB_0803857e:
              uVar22 = (uint)bVar11 - (uint)bVar20;
            }
          }
          else {
            if ((uVar22 & 1) == 0) {
              bVar11 = *(byte *)(uVar25 + unaff_r9 + 0x5b0);
              bVar20 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_0803857e;
            }
            uVar22 = (uint)*(byte *)(uVar25 + unaff_r9 + 0x9b0);
          }
          uVar27 = uVar15 + 1;
          uVar25 = uVar27;
          if (0x7fe < uVar13) {
            uVar25 = uVar15 - 0x7ff;
          }
          uVar26 = (uVar25 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar25 & 1) == 0) {
              bVar11 = *(byte *)(uVar26 + unaff_r9 + 0x5b0);
            }
            else {
              cVar1 = *(char *)(uVar26 + unaff_r9 + 0x9b0);
              cVar4 = *(char *)(unaff_r9 + 0x350);
LAB_080385cc:
              bVar11 = cVar1 - cVar4;
            }
          }
          else {
            if ((uVar25 & 1) == 0) {
              cVar1 = *(char *)(uVar26 + unaff_r9 + 0x5b0);
              cVar4 = *(char *)(unaff_r9 + 0x351);
              goto LAB_080385cc;
            }
            bVar11 = *(byte *)(uVar26 + unaff_r9 + 0x9b0);
          }
          uVar25 = uVar15 + 2;
          if (0x7fd < uVar13) {
            uVar25 = uVar15 - 0x7fe;
          }
          uVar26 = (uVar25 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar25 & 1) == 0) {
              uVar25 = (uint)*(byte *)(uVar26 + unaff_r9 + 0x5b0);
            }
            else {
              bVar20 = *(byte *)(uVar26 + unaff_r9 + 0x9b0);
              bVar12 = *(byte *)(unaff_r9 + 0x350);
LAB_0803861c:
              uVar25 = (uint)bVar20 - (uint)bVar12;
            }
          }
          else {
            if ((uVar25 & 1) == 0) {
              bVar20 = *(byte *)(uVar26 + unaff_r9 + 0x5b0);
              bVar12 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_0803861c;
            }
            uVar25 = (uint)*(byte *)(uVar26 + unaff_r9 + 0x9b0);
          }
          uVar30 = uVar15 + 3;
          uVar23 = uVar15 - 0x7fd;
          uVar26 = uVar30;
          if (0x7fc < uVar13) {
            uVar26 = uVar23;
          }
          uVar19 = (uVar26 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar26 & 1) == 0) {
              uVar26 = (uint)*(byte *)(uVar19 + unaff_r9 + 0x5b0);
            }
            else {
              bVar12 = *(byte *)(uVar19 + unaff_r9 + 0x9b0);
              bVar20 = *(byte *)(unaff_r9 + 0x350);
LAB_08038670:
              uVar26 = (uint)bVar12 - (uint)bVar20;
            }
          }
          else {
            if ((uVar26 & 1) == 0) {
              bVar12 = *(byte *)(uVar19 + unaff_r9 + 0x5b0);
              bVar20 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_08038670;
            }
            uVar26 = (uint)*(byte *)(uVar19 + unaff_r9 + 0x9b0);
          }
          uVar19 = uVar15 + 4;
          if (0x7fb < uVar13) {
            uVar19 = uVar15 - 0x7fc;
          }
          uVar13 = (uVar19 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar19 & 1) == 0) {
              uVar13 = (uint)*(byte *)(uVar13 + unaff_r9 + 0x5b0);
            }
            else {
              bVar20 = *(byte *)(uVar13 + unaff_r9 + 0x9b0);
              bVar12 = *(byte *)(unaff_r9 + 0x350);
LAB_080386bc:
              uVar13 = (uint)bVar20 - (uint)bVar12;
            }
          }
          else {
            if ((uVar19 & 1) == 0) {
              bVar20 = *(byte *)(uVar13 + unaff_r9 + 0x5b0);
              bVar12 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_080386bc;
            }
            uVar13 = (uint)*(byte *)(uVar13 + unaff_r9 + 0x9b0);
          }
          uVar19 = uVar27 & 1;
          uVar27 = uVar27 * 0x10000 >> 0x11;
          if (cVar29 == '\x01') {
            if (uVar19 == 0) {
              uVar7 = (uint)*(byte *)(unaff_r9 + uVar27 + 0x5b0);
            }
            else {
              bVar20 = *(byte *)(uVar27 + unaff_r9 + 0x9b0);
              bVar12 = *(byte *)(unaff_r9 + 0x350);
LAB_08038700:
              uVar7 = (uint)bVar20 - (uint)bVar12;
            }
          }
          else {
            if (uVar19 == 0) {
              bVar20 = *(byte *)(unaff_r9 + uVar27 + 0x5b0);
              bVar12 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_08038700;
            }
            uVar7 = (uint)*(byte *)(uVar27 + unaff_r9 + 0x9b0);
          }
          uVar8 = (uVar15 + 2) * 0x10000 >> 0x11;
          if (cVar29 == '\x01') {
            if ((uVar15 & 1) == 0) {
              uVar8 = (uint)*(byte *)(uVar8 + unaff_r9 + 0x5b0);
            }
            else {
              bVar20 = *(byte *)(uVar8 + unaff_r9 + 0x9b0);
              bVar12 = *(byte *)(unaff_r9 + 0x350);
LAB_08038744:
              uVar8 = (uint)bVar20 - (uint)bVar12;
            }
          }
          else {
            if ((uVar15 & 1) == 0) {
              bVar20 = *(byte *)(uVar8 + unaff_r9 + 0x5b0);
              bVar12 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_08038744;
            }
            uVar8 = (uint)*(byte *)(uVar8 + unaff_r9 + 0x9b0);
          }
          uVar10 = uVar30;
          if ((uVar30 & 0xffff) >> 0xb != 0) {
            uVar10 = uVar23;
          }
          uVar9 = (uVar10 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar10 & 1) == 0) {
              uVar10 = (uint)*(byte *)(uVar9 + unaff_r9 + 0x5b0);
            }
            else {
              bVar20 = *(byte *)(uVar9 + unaff_r9 + 0x9b0);
              bVar12 = *(byte *)(unaff_r9 + 0x350);
LAB_08038794:
              uVar10 = (uint)bVar20 - (uint)bVar12;
            }
          }
          else {
            if ((uVar10 & 1) == 0) {
              bVar20 = *(byte *)(uVar9 + unaff_r9 + 0x5b0);
              bVar12 = *(byte *)(unaff_r9 + 0x351);
              goto LAB_08038794;
            }
            uVar10 = (uint)*(byte *)(uVar9 + unaff_r9 + 0x9b0);
          }
          if (cVar29 == '\x01') {
            if (uVar19 == 0) {
              bVar20 = *(byte *)(unaff_r9 + uVar27 + 0x5b0);
            }
            else {
              cVar1 = *(char *)(uVar27 + unaff_r9 + 0x9b0);
              cVar4 = *(char *)(unaff_r9 + 0x350);
LAB_080387d0:
              bVar20 = cVar1 - cVar4;
            }
          }
          else {
            if (uVar19 == 0) {
              cVar1 = *(char *)(unaff_r9 + uVar27 + 0x5b0);
              cVar4 = *(char *)(unaff_r9 + 0x351);
              goto LAB_080387d0;
            }
            bVar20 = *(byte *)(uVar27 + unaff_r9 + 0x9b0);
          }
          iVar18 = (uVar22 & 0xff) - (uVar7 & 0xff);
          uVar13 = uVar13 & 0xff;
          iVar28 = (uint)bVar11 - (uVar8 & 0xff);
          iVar14 = (uVar25 & 0xff) - (uVar10 & 0xff);
          iVar6 = (uVar26 & 0xff) - (uint)bVar20;
          if ((uVar30 & 0xffff) >> 0xb == 0) {
            uVar23 = uVar30;
          }
          uVar22 = (uVar23 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar23 & 1) == 0) {
              bVar11 = *(byte *)(uVar22 + 0x200006a8);
            }
            else {
              cVar1 = *(char *)(uVar22 + 0x20000aa8);
              cVar4 = DAT_20000448;
LAB_0803885c:
              bVar11 = cVar1 - cVar4;
            }
          }
          else {
            if ((uVar23 & 1) == 0) {
              cVar1 = *(char *)(uVar22 + 0x200006a8);
              cVar4 = DAT_20000449;
              goto LAB_0803885c;
            }
            bVar11 = *(byte *)(uVar22 + 0x20000aa8);
          }
        }
        if (iVar18 < 1) {
          sVar2 = SignedSaturate(-(short)iVar18,0x10);
          SignedSaturate(-(short)((uint)iVar18 >> 0x10),0x10);
          iVar18 = (int)sVar2;
        }
        uStack00000030 = uVar15 + 3;
        uStack0000002c = uVar15 + 1;
        iStack00000020 = uVar15 + 2;
        unaff_r9 = 0x200000f8;
        if (iVar28 < 1) {
          sVar2 = SignedSaturate(-(short)iVar28,0x10);
          SignedSaturate(-(short)((uint)iVar28 >> 0x10),0x10);
          iVar28 = (int)sVar2;
        }
        if (iVar14 < 1) {
          sVar2 = SignedSaturate(-(short)iVar14,0x10);
          SignedSaturate(-(short)((uint)iVar14 >> 0x10),0x10);
          iVar14 = (int)sVar2;
        }
        if (iVar6 < 1) {
          sVar2 = SignedSaturate(-(short)iVar6,0x10);
          SignedSaturate(-(short)((uint)iVar6 >> 0x10),0x10);
          iVar6 = (int)sVar2;
        }
        iVar24 = uVar13 - bVar11;
        if (iVar24 < 1) {
          sVar2 = SignedSaturate(-(short)iVar24,0x10);
          SignedSaturate(-(short)((uint)iVar24 >> 0x10),0x10);
          iVar24 = (int)sVar2;
        }
        if ((((iVar18 < 8) && (10 < iVar28)) && (10 < iVar14)) && ((iVar6 < 8 && (iVar24 < 8)))) {
          if (cVar29 == '\x03' || 3 < bVar16) {
            uVar13 = uStack0000002c;
            if (0x3fe < uVar15) {
              uVar13 = uVar15 + 0xfc01;
            }
            uVar17 = (ushort)uStack00000030;
            if ((uStack00000030 & 0xffff) >> 10 != 0) {
              uVar17 = sVar3 - 0x3fd;
            }
            bVar16 = *(byte *)(uVar17 + 0x200006a8);
            uVar13 = (uint)*(byte *)((uVar13 & 0xffff) + 0x200006a8) - (uint)bVar16;
            cVar4 = (char)((int)(uVar13 + ((uVar13 & 0xffff) >> 0xf)) >> 1);
          }
          else {
            uVar13 = uStack0000002c * 0x10000 >> 0x11;
            if (cVar29 == '\x01') {
              if ((uStack0000002c & 1) == 0) {
                bVar16 = *(byte *)(uVar13 + 0x200006a8);
              }
              else {
                cVar1 = *(char *)(uVar13 + 0x20000aa8);
                cVar4 = DAT_20000448;
LAB_0803887e:
                bVar16 = cVar1 - cVar4;
              }
            }
            else {
              if ((uStack0000002c & 1) == 0) {
                cVar1 = *(char *)(uVar13 + 0x200006a8);
                cVar4 = DAT_20000449;
                goto LAB_0803887e;
              }
              bVar16 = *(byte *)(uVar13 + 0x20000aa8);
            }
            uVar13 = uStack00000030;
            if ((uStack00000030 & 0xffff) >> 0xb != 0) {
              uVar13 = uVar15 - 0x7fd;
            }
            uVar22 = (uVar13 & 0xffff) >> 1;
            if (cVar29 == '\x01') {
              if ((uVar13 & 1) == 0) {
                bVar11 = *(byte *)(uVar22 + 0x200006a8);
              }
              else {
                bVar11 = *(char *)(uVar22 + 0x20000aa8) - DAT_20000448;
              }
            }
            else if ((uVar13 & 1) == 0) {
              bVar11 = *(char *)(uVar22 + 0x200006a8) - DAT_20000449;
            }
            else {
              bVar11 = *(byte *)(uVar22 + 0x20000aa8);
            }
            if ((uStack00000030 & 0xffff) >> 0xb != 0) {
              uStack00000030 = uVar15 - 0x7fd;
            }
            cVar4 = (char)((int)(((uint)bVar16 - (uint)bVar11) +
                                (((uint)bVar16 - (uint)bVar11 & 0xffff) >> 0xf)) >> 1);
            uVar13 = (uStack00000030 & 0xffff) >> 1;
            if (cVar29 == '\x01') {
              if ((uStack00000030 & 1) == 0) {
                bVar16 = *(byte *)(uVar13 + 0x200006a8);
              }
              else {
                cVar29 = *(char *)(uVar13 + 0x20000aa8);
                cVar1 = DAT_20000448;
LAB_0803892a:
                bVar16 = cVar29 - cVar1;
              }
            }
            else {
              if ((uStack00000030 & 1) == 0) {
                cVar29 = *(char *)(uVar13 + 0x200006a8);
                cVar1 = DAT_20000449;
                goto LAB_0803892a;
              }
              bVar16 = *(byte *)(uVar13 + 0x20000aa8);
            }
          }
          if (0x3fd < uVar15) {
            iStack00000020 = uVar15 - 0x3fe;
          }
          *(byte *)(iStack00000020 + 0x200006a8) = cVar4 + bVar16;
        }
        uVar15 = uStack0000002c;
        bVar16 = DAT_20000125;
        cVar29 = DAT_2000010c;
      } while (uStack0000002c != 0x400);
    }
    if ((cVar29 != '\x01') || (bVar16 < 5)) goto switchD_08037536_default;
    cVar29 = *(char *)(unaff_r9 + 0xdb0);
    break;
  case 5:
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    uVar15 = 0x400;
    do {
      do {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
        if (!bVar31) {
          bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
        }
      } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
      unaff_r7[1] = 0xff;
      do {
        bVar31 = false;
        if ((*unaff_r7 & 1) == 0) {
          bVar31 = (*unaff_r7 & 1) == 0;
        }
        bVar32 = false;
        if (bVar31) {
          bVar32 = (*unaff_r7 & 1) == 0;
        }
      } while ((bVar32) && ((*unaff_r7 & 1) == 0));
      *(char *)(unaff_r9 + uVar15 + 0x5b0) = (char)unaff_r7[1];
      uVar13 = uVar15 | 1;
      while (-1 < (int)(*unaff_r7 << 0x1e)) {
        uVar22 = *unaff_r7 << 0x1e;
        bVar31 = (int)uVar22 < 0;
        if (!bVar31) {
          uVar22 = *unaff_r7;
        }
        if ((bVar31 || (int)(uVar22 << 0x1e) < 0) || ((int)(*unaff_r7 << 0x1e) < 0)) break;
      }
      unaff_r7[1] = 0xff;
      while ((*unaff_r7 & 1) == 0) {
        uVar22 = *unaff_r7 << 0x1f;
        bVar31 = uVar22 != 0;
        if (!bVar31) {
          uVar22 = *unaff_r7;
        }
        if ((bVar31 || (uVar22 & 1) != 0) || ((*unaff_r7 & 1) != 0)) break;
      }
      uVar15 = uVar15 + 2;
      *(char *)(uVar13 + unaff_r9 + 0x5b0) = (char)unaff_r7[1];
    } while (uVar15 != 0x800);
    cVar29 = *(char *)(unaff_r9 + 0x14);
    bVar16 = *(byte *)(unaff_r9 + 0x2d);
    if ((((*(char *)(unaff_r9 + 0x33) == '\0') && (1 < (*(byte *)(unaff_r9 + 0x353) + 1 & 0xff))) &&
        (cVar29 == '\x03' || 3 < bVar16)) && (1 < (byte)(*(char *)(unaff_r9 + 0x352) + 1U))) {
      fVar33 = (float)VectorUnsignedToFloat
                                ((uint)*(byte *)(unaff_r9 + 0x353),(byte)(in_fpscr >> 0x15) & 3);
      fVar33 = (fVar33 + unaff_s16) / unaff_s28;
      iVar14 = 0;
      fVar34 = (float)VectorSignedToFloat((int)*(char *)(unaff_r9 + 5),(byte)(in_fpscr >> 0x15) & 3)
      ;
      do {
        iVar18 = unaff_r9 + iVar14;
        fVar35 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b0),(byte)(in_fpscr >> 0x15) & 3);
        fVar36 = ((fVar35 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = in_fpscr & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = in_fpscr & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b1),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b0) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar13 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b2),(byte)(uVar13 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b1) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b3),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b2) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar13 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b4),(byte)(uVar13 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b3) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b5),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b4) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar13 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b6),(byte)(uVar13 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b5) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        bVar11 = (byte)(uVar15 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || (bool)(bVar11 >> 7) != (NAN(fVar36) || NAN(unaff_s24))) &&
           (uVar15 = uVar13 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f, fVar35 = fVar36,
           SUB41(uVar15 >> 0x1f,0) != NAN(fVar36))) {
          fVar35 = unaff_s26;
        }
        fVar36 = (float)VectorUnsignedToFloat
                                  ((uint)*(byte *)(iVar18 + 0x9b7),(byte)(uVar15 >> 0x15) & 3);
        *(char *)(iVar18 + 0x9b6) = (char)(int)fVar35;
        fVar36 = ((fVar36 + unaff_s20) - fVar34) / fVar33 + unaff_s22 + fVar34;
        uVar13 = uVar15 & 0xfffffff | (uint)(fVar36 < unaff_s24) << 0x1f |
                 (uint)(fVar36 == unaff_s24) << 0x1e;
        in_fpscr = uVar13 | (uint)(NAN(fVar36) || NAN(unaff_s24)) << 0x1c;
        bVar11 = (byte)(uVar13 >> 0x18);
        fVar35 = unaff_s24;
        if (((bool)(bVar11 >> 6 & 1) || bVar11 >> 7 != ((byte)(in_fpscr >> 0x1c) & 1)) &&
           (uVar15 = uVar15 & 0xfffffff | (uint)(fVar36 < 0.0) << 0x1f,
           in_fpscr = uVar15 | (uint)NAN(fVar36) << 0x1c, fVar35 = fVar36,
           (byte)(uVar15 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1))) {
          fVar35 = unaff_s26;
        }
        iVar14 = iVar14 + 8;
        *(char *)(iVar18 + 0x9b7) = (char)(int)fVar35;
      } while (iVar14 != 0x400);
    }
    *(char *)(unaff_r9 + 0xdb0) = *(char *)(unaff_r9 + 0xdb0) + '\x01';
    if (*(char *)(unaff_r9 + 0x17) == '\x02') {
      bVar11 = *(byte *)(unaff_r9 + 0x350);
      uVar13 = (uint)bVar11;
      bVar20 = *(byte *)(unaff_r9 + 0x351);
      uVar22 = (uint)bVar20;
      uVar15 = 0;
      do {
        uVar25 = uVar15 & 0xffff;
        if (cVar29 == '\x03' || 3 < bVar16) {
          uVar26 = uVar15 + 1;
          uVar17 = (ushort)uVar26;
          uVar27 = uVar15;
          if (uVar25 >> 10 != 0) {
            uVar27 = uVar15 + 0xfc00;
          }
          iVar14 = unaff_r9 + 0x5b0;
          if (0x3fe < uVar25) {
            uVar17 = (short)uVar15 - 0x3ff;
          }
          uVar19 = uVar15 + 2;
          uVar23 = uVar15 + 3;
          uVar30 = uVar19;
          if (0x3fd < uVar25) {
            uVar30 = uVar15 - 0x3fe;
          }
          uVar7 = uVar23;
          if (0x3fc < uVar25) {
            uVar7 = uVar15 - 0x3fd;
          }
          iVar24 = 4;
          if (0xfe < uVar25 >> 2) {
            iVar24 = -0x3fc;
          }
          if (0x3fe < uVar15) {
            uVar26 = uVar15 + 0xfc01;
          }
          if (0x3fd < uVar15) {
            uVar19 = uVar15 + 0xfc02;
          }
          iVar18 = (uint)*(byte *)(iVar14 + (uint)uVar17 + 0x400) -
                   (uint)*(byte *)(iVar14 + (uVar19 & 0xffff) + 0x400);
          if ((uVar23 & 0xffff) >> 10 != 0) {
            uVar23 = uVar15 - 0x3fd;
          }
          uVar25 = (uint)*(byte *)(iVar14 + (uVar26 & 0xffff) + 0x400);
          bVar12 = *(byte *)(iVar14 + (uVar23 & 0xffff) + 0x400);
          iVar28 = *(byte *)(iVar14 + (uVar27 & 0xffff) + 0x400) - uVar25;
          iVar6 = *(byte *)(iVar14 + (uVar7 & 0xffff) + 0x400) - uVar25;
          uVar25 = (uint)*(byte *)(iVar14 + (uVar15 + iVar24 & 0xffff) + 0x400);
          iVar14 = (uint)*(byte *)(iVar14 + (uVar30 & 0xffff) + 0x400) - (uint)bVar12;
        }
        else {
          uVar27 = uVar15;
          if (uVar25 >> 0xb != 0) {
            uVar27 = uVar15 + 0xf800;
          }
          uVar26 = (uVar27 << 0x10) >> 0x11;
          if (cVar29 == '\x01') {
            if ((uVar27 & 1) == 0) {
              bVar12 = *(byte *)(uVar26 + unaff_r9 + 0x5b0);
            }
            else {
              cVar4 = *(char *)(uVar26 + unaff_r9 + 0x9b0);
              bVar12 = bVar11;
LAB_08038b0c:
              bVar12 = cVar4 - bVar12;
            }
          }
          else {
            if ((uVar27 & 1) == 0) {
              cVar4 = *(char *)(uVar26 + unaff_r9 + 0x5b0);
              bVar12 = bVar20;
              goto LAB_08038b0c;
            }
            bVar12 = *(byte *)(uVar26 + unaff_r9 + 0x9b0);
          }
          uVar26 = uVar15 + 1;
          uVar27 = uVar26;
          if (0x7fe < uVar25) {
            uVar27 = uVar15 - 0x7ff;
          }
          uVar23 = (uVar27 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar27 & 1) == 0) {
              uVar27 = (uint)*(byte *)(uVar23 + unaff_r9 + 0x5b0);
            }
            else {
              bVar21 = *(byte *)(uVar23 + unaff_r9 + 0x9b0);
              uVar27 = uVar13;
LAB_08038b58:
              uVar27 = bVar21 - uVar27;
            }
          }
          else {
            if ((uVar27 & 1) == 0) {
              bVar21 = *(byte *)(uVar23 + unaff_r9 + 0x5b0);
              uVar27 = uVar22;
              goto LAB_08038b58;
            }
            uVar27 = (uint)*(byte *)(uVar23 + unaff_r9 + 0x9b0);
          }
          uVar23 = uVar15 + 2;
          if (0x7fd < uVar25) {
            uVar23 = uVar15 - 0x7fe;
          }
          uVar30 = (uVar23 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar23 & 1) == 0) {
              bVar21 = *(byte *)(uVar30 + unaff_r9 + 0x5b0);
            }
            else {
              cVar4 = *(char *)(uVar30 + unaff_r9 + 0x9b0);
              bVar21 = bVar11;
LAB_08038ba4:
              bVar21 = cVar4 - bVar21;
            }
          }
          else {
            if ((uVar23 & 1) == 0) {
              cVar4 = *(char *)(uVar30 + unaff_r9 + 0x5b0);
              bVar21 = bVar20;
              goto LAB_08038ba4;
            }
            bVar21 = *(byte *)(uVar30 + unaff_r9 + 0x9b0);
          }
          uVar19 = uVar15 + 3;
          uVar30 = uVar15 - 0x7fd;
          uVar23 = uVar19;
          if (0x7fc < uVar25) {
            uVar23 = uVar30;
          }
          uVar7 = (uVar23 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar23 & 1) == 0) {
              uVar23 = (uint)*(byte *)(uVar7 + unaff_r9 + 0x5b0);
            }
            else {
              bVar5 = *(byte *)(uVar7 + unaff_r9 + 0x9b0);
              uVar23 = uVar13;
LAB_08038bf0:
              uVar23 = bVar5 - uVar23;
            }
          }
          else {
            if ((uVar23 & 1) == 0) {
              bVar5 = *(byte *)(uVar7 + unaff_r9 + 0x5b0);
              uVar23 = uVar22;
              goto LAB_08038bf0;
            }
            uVar23 = (uint)*(byte *)(uVar7 + unaff_r9 + 0x9b0);
          }
          iVar14 = 4;
          if (0x7fb < uVar25) {
            iVar14 = -0x7fc;
          }
          uVar7 = uVar15 + iVar14 & 1;
          uVar25 = (uVar15 + iVar14 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if (uVar7 == 0) {
              bVar5 = *(byte *)(uVar25 + unaff_r9 + 0x5b0);
            }
            else {
              bVar5 = *(char *)(uVar25 + unaff_r9 + 0x9b0) - bVar11;
            }
            uVar25 = (uint)bVar5;
            uVar7 = uVar26 * 0x10000 >> 0x11;
            if ((uVar26 & 1) == 0) {
              uVar7 = (uint)*(byte *)(uVar7 + unaff_r9 + 0x5b0);
            }
            else {
              bVar5 = *(byte *)(uVar7 + unaff_r9 + 0x9b0);
              uVar7 = uVar13;
LAB_08038c78:
              uVar7 = bVar5 - uVar7;
            }
          }
          else {
            if (uVar7 == 0) {
              bVar5 = *(char *)(uVar25 + unaff_r9 + 0x5b0) - bVar20;
            }
            else {
              bVar5 = *(byte *)(uVar25 + unaff_r9 + 0x9b0);
            }
            uVar25 = (uint)bVar5;
            uVar7 = uVar26 * 0x10000 >> 0x11;
            if ((uVar26 & 1) == 0) {
              bVar5 = *(byte *)(uVar7 + unaff_r9 + 0x5b0);
              uVar7 = uVar22;
              goto LAB_08038c78;
            }
            uVar7 = (uint)*(byte *)(uVar7 + unaff_r9 + 0x9b0);
          }
          uVar8 = (uVar15 + 2) * 0x10000 >> 0x11;
          if (cVar29 == '\x01') {
            if ((uVar15 & 1) == 0) {
              uVar8 = (uint)*(byte *)(uVar8 + unaff_r9 + 0x5b0);
            }
            else {
              bVar5 = *(byte *)(uVar8 + unaff_r9 + 0x9b0);
              uVar8 = uVar13;
LAB_08038cb4:
              uVar8 = bVar5 - uVar8;
            }
          }
          else {
            if ((uVar15 & 1) == 0) {
              bVar5 = *(byte *)(uVar8 + unaff_r9 + 0x5b0);
              uVar8 = uVar22;
              goto LAB_08038cb4;
            }
            uVar8 = (uint)*(byte *)(uVar8 + unaff_r9 + 0x9b0);
          }
          uVar10 = uVar19;
          if ((uVar19 & 0xffff) >> 0xb != 0) {
            uVar10 = uVar30;
          }
          uVar9 = (uVar10 & 0xffff) >> 1;
          if (cVar29 == '\x01') {
            if ((uVar10 & 1) == 0) {
              bVar5 = *(byte *)(uVar9 + 0x200006a8);
            }
            else {
              bVar5 = *(char *)(uVar9 + 0x20000aa8) - bVar11;
            }
            iVar14 = (uint)bVar21 - (uint)bVar5;
            uVar10 = uVar26 * 0x10000 >> 0x11;
            if ((uVar26 & 1) == 0) {
              bVar21 = *(byte *)(uVar10 + 0x200006a8);
            }
            else {
              cVar4 = *(char *)(uVar10 + 0x20000aa8);
              bVar21 = bVar11;
LAB_08038d78:
              bVar21 = cVar4 - bVar21;
            }
          }
          else {
            if ((uVar10 & 1) == 0) {
              bVar5 = *(char *)(uVar9 + 0x200006a8) - bVar20;
            }
            else {
              bVar5 = *(byte *)(uVar9 + 0x20000aa8);
            }
            iVar14 = (uint)bVar21 - (uint)bVar5;
            uVar10 = uVar26 * 0x10000 >> 0x11;
            if ((uVar26 & 1) == 0) {
              cVar4 = *(char *)(uVar10 + 0x200006a8);
              bVar21 = bVar20;
              goto LAB_08038d78;
            }
            bVar21 = *(byte *)(uVar10 + 0x20000aa8);
          }
          iVar18 = (uVar27 & 0xff) - (uVar8 & 0xff);
          if ((uVar19 & 0xffff) >> 0xb == 0) {
            uVar30 = uVar19;
          }
          uVar27 = (uVar30 & 0xffff) >> 1;
          iVar28 = (uint)bVar12 - (uVar7 & 0xff);
          iVar6 = (uVar23 & 0xff) - (uint)bVar21;
          if (cVar29 == '\x01') {
            if ((uVar30 & 1) == 0) {
              bVar12 = *(byte *)(uVar27 + 0x200006a8);
            }
            else {
              cVar4 = *(char *)(uVar27 + 0x20000aa8);
              bVar12 = bVar11;
LAB_08038de0:
              bVar12 = cVar4 - bVar12;
            }
          }
          else {
            if ((uVar30 & 1) == 0) {
              cVar4 = *(char *)(uVar27 + 0x200006a8);
              bVar12 = bVar20;
              goto LAB_08038de0;
            }
            bVar12 = *(byte *)(uVar27 + 0x20000aa8);
          }
        }
        if (iVar28 < 1) {
          sVar3 = SignedSaturate(-(short)iVar28,0x10);
          SignedSaturate(-(short)((uint)iVar28 >> 0x10),0x10);
          iVar28 = (int)sVar3;
        }
        uStack00000030 = uVar15 + 3;
        uStack0000002c = uVar15 + 1;
        iStack00000028 = uVar15 + 2;
        if (iVar18 < 1) {
          sVar3 = SignedSaturate(-(short)iVar18,0x10);
          SignedSaturate(-(short)((uint)iVar18 >> 0x10),0x10);
          iVar18 = (int)sVar3;
        }
        if (iVar14 < 1) {
          sVar3 = SignedSaturate(-(short)iVar14,0x10);
          SignedSaturate(-(short)((uint)iVar14 >> 0x10),0x10);
          iVar14 = (int)sVar3;
        }
        if (iVar6 < 1) {
          sVar3 = SignedSaturate(-(short)iVar6,0x10);
          SignedSaturate(-(short)((uint)iVar6 >> 0x10),0x10);
          iVar6 = (int)sVar3;
        }
        iVar24 = uVar25 - bVar12;
        if (iVar24 < 1) {
          sVar3 = SignedSaturate(-(short)iVar24,0x10);
          SignedSaturate(-(short)((uint)iVar24 >> 0x10),0x10);
          iVar24 = (int)sVar3;
        }
        if ((((iVar28 < 8) && (10 < iVar18)) && (10 < iVar14)) && ((iVar6 < 8 && (iVar24 < 8)))) {
          if (cVar29 == '\x03' || 3 < bVar16) {
            uVar25 = uStack0000002c;
            if (0x3fe < uVar15) {
              uVar25 = uVar15 + 0xfc01;
            }
            if ((uStack00000030 & 0xffff) >> 10 != 0) {
              uStack00000030 = uVar15 - 0x3fd;
            }
            bVar12 = *(byte *)((uStack00000030 & 0xffff) + 0x20000aa8);
            uVar25 = (uint)*(byte *)((uVar25 & 0xffff) + 0x20000aa8) - (uint)bVar12;
            cVar4 = (char)((int)(uVar25 + ((uVar25 & 0xffff) >> 0xf)) >> 1);
          }
          else {
            uVar25 = uStack0000002c * 0x10000 >> 0x11;
            if (cVar29 == '\x01') {
              if ((uStack0000002c & 1) == 0) {
                bVar12 = *(byte *)(uVar25 + 0x200006a8);
              }
              else {
                cVar4 = *(char *)(uVar25 + 0x20000aa8);
                bVar12 = bVar11;
LAB_08038ea8:
                bVar12 = cVar4 - bVar12;
              }
            }
            else {
              if ((uStack0000002c & 1) == 0) {
                cVar4 = *(char *)(uVar25 + 0x200006a8);
                bVar12 = bVar20;
                goto LAB_08038ea8;
              }
              bVar12 = *(byte *)(uVar25 + 0x20000aa8);
            }
            uVar25 = uStack00000030;
            if ((uStack00000030 & 0xffff) >> 0xb != 0) {
              uVar25 = uVar15 - 0x7fd;
            }
            uVar27 = (uVar25 & 0xffff) >> 1;
            if (cVar29 == '\x01') {
              if ((uVar25 & 1) == 0) {
                bVar21 = *(byte *)(uVar27 + 0x200006a8);
              }
              else {
                bVar21 = *(char *)(uVar27 + 0x20000aa8) - bVar11;
              }
            }
            else if ((uVar25 & 1) == 0) {
              bVar21 = *(char *)(uVar27 + 0x200006a8) - bVar20;
            }
            else {
              bVar21 = *(byte *)(uVar27 + 0x20000aa8);
            }
            if ((uStack00000030 & 0xffff) >> 0xb != 0) {
              uStack00000030 = uVar15 - 0x7fd;
            }
            cVar4 = (char)((int)(((uint)bVar12 - (uint)bVar21) +
                                (((uint)bVar12 - (uint)bVar21 & 0xffff) >> 0xf)) >> 1);
            uVar25 = (uStack00000030 & 0xffff) >> 1;
            if (cVar29 == '\x01') {
              if ((uStack00000030 & 1) == 0) {
                bVar12 = *(byte *)(uVar25 + 0x200006a8);
              }
              else {
                cVar1 = *(char *)(uVar25 + 0x20000aa8);
                bVar12 = bVar11;
LAB_08038f48:
                bVar12 = cVar1 - bVar12;
              }
            }
            else {
              if ((uStack00000030 & 1) == 0) {
                cVar1 = *(char *)(uVar25 + 0x200006a8);
                bVar12 = bVar20;
                goto LAB_08038f48;
              }
              bVar12 = *(byte *)(uVar25 + 0x20000aa8);
            }
          }
          if (uVar15 < 0x3fe) {
            iStack00000028 = uVar15 + 0x402;
          }
          *(byte *)(iStack00000028 + 0x200006a8) = cVar4 + bVar12;
        }
        unaff_r9 = 0x200000f8;
        uVar15 = uStack0000002c;
      } while (uStack0000002c != 0x400);
    }
    goto switchD_08037536_default;
  case 6:
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = (uint)*(byte *)(unaff_r9 + 0x16);
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    goto switchD_08037536_default;
  case 7:
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = (uint)*(byte *)(unaff_r9 + 0x18);
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    goto switchD_08037536_default;
  case 8:
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = uVar15 & 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    goto switchD_08037536_default;
  case 9:
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    *(ushort *)(unaff_r9 + 0x46) = (ushort)unaff_r7[1] & 0xff;
    _DAT_40010c10 = 0x40;
    FUN_0803a390(1,0xff,uVar15,uVar22);
    _DAT_40010c14 = 0x40;
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 10;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    *(short *)(unaff_r9 + 0x46) = *(short *)(unaff_r9 + 0x46) << 8;
    do {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
      if (!bVar31) {
        bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
      }
    } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
    unaff_r7[1] = 0xff;
    do {
      bVar31 = false;
      if ((*unaff_r7 & 1) == 0) {
        bVar31 = (*unaff_r7 & 1) == 0;
      }
      bVar32 = false;
      if (bVar31) {
        bVar32 = (*unaff_r7 & 1) == 0;
      }
    } while ((bVar32) && ((*unaff_r7 & 1) == 0));
    cVar29 = *(char *)(unaff_r9 + 0xdb0);
    *(ushort *)(unaff_r9 + 0x46) = (ushort)(byte)unaff_r7[1] | *(ushort *)(unaff_r9 + 0x46);
    break;
  default:
    goto switchD_08037536_default;
  }
  *(char *)(unaff_r9 + 0xdb0) = cVar29 + '\x01';
  goto switchD_08037536_default;
switchD_08037536_caseD_3:
  if (*(short *)(unaff_r9 + 0xdb4) != 0) {
    *(short *)(unaff_r9 + 0xdb4) = *(short *)(unaff_r9 + 0xdb4) + -1;
  }
  uVar15 = (uint)*(ushort *)(unaff_r9 + 0xdb6);
  if (uVar15 < 0x12d) {
    bVar31 = uVar15 != 0;
    uVar15 = uVar15 + 1;
    *(short *)(unaff_r9 + 0xdb6) = (short)uVar15;
    if (bVar31) goto LAB_08037a16;
  }
  else {
LAB_08037a16:
    uVar22 = 0;
    uVar17 = 0;
    do {
      iVar14 = (uVar22 - uVar15) + unaff_r9 + 0x356;
      uVar17 = uVar17 + 1;
      *(undefined1 *)(iVar14 + 0x12d) = *(undefined1 *)(iVar14 + 0x12e);
      iVar14 = (uVar22 - *(ushort *)(unaff_r9 + 0xdb6)) + unaff_r9 + 0x356;
      *(undefined1 *)(iVar14 + 0x25a) = *(undefined1 *)(iVar14 + 0x25b);
      uVar15 = (uint)*(ushort *)(unaff_r9 + 0xdb6);
      uVar22 = (uint)uVar17;
    } while ((int)uVar22 < (int)(uVar15 - 1));
  }
  do {
    bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
  } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
  unaff_r7[1] = 0xff;
  do {
    bVar31 = false;
    if ((*unaff_r7 & 1) == 0) {
      bVar31 = (*unaff_r7 & 1) == 0;
    }
    bVar32 = false;
    if (bVar31) {
      bVar32 = (*unaff_r7 & 1) == 0;
    }
  } while ((bVar32) && ((*unaff_r7 & 1) == 0));
  do {
    bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
  } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
  unaff_r7[1] = 0xff;
  do {
    bVar31 = false;
    if ((*unaff_r7 & 1) == 0) {
      bVar31 = (*unaff_r7 & 1) == 0;
    }
    bVar32 = false;
    if (bVar31) {
      bVar32 = (*unaff_r7 & 1) == 0;
    }
  } while ((bVar32) && ((*unaff_r7 & 1) == 0));
  *(char *)(unaff_r9 + 0x482) = (char)unaff_r7[1];
  do {
    bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
  } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
  unaff_r7[1] = 0xff;
  do {
    bVar31 = false;
    if ((*unaff_r7 & 1) == 0) {
      bVar31 = (*unaff_r7 & 1) == 0;
    }
    bVar32 = false;
    if (bVar31) {
      bVar32 = (*unaff_r7 & 1) == 0;
    }
  } while ((bVar32) && ((*unaff_r7 & 1) == 0));
  do {
    bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
    if (!bVar31) {
      bVar31 = (int)(*unaff_r7 << 0x1e) < 0;
    }
  } while ((!bVar31) && (-1 < (int)(*unaff_r7 << 0x1e)));
  unaff_r7[1] = 0xff;
  do {
    bVar31 = false;
    if ((*unaff_r7 & 1) == 0) {
      bVar31 = (*unaff_r7 & 1) == 0;
    }
    bVar32 = false;
    if (bVar31) {
      bVar32 = (*unaff_r7 & 1) == 0;
    }
  } while ((bVar32) && ((*unaff_r7 & 1) == 0));
  uVar15 = unaff_r7[1];
  uVar22 = (uint)*(byte *)(unaff_r9 + 0x352);
  *(char *)(unaff_r9 + 0x5af) = (char)uVar15;
  if (((uVar22 != 0) &&
      (((uVar22 != 0xff && (uVar25 = (uint)*(byte *)(unaff_r9 + 0x353), uVar25 != 0)) &&
       (uVar25 != 0xff)))) && (*(char *)(unaff_r9 + 0x33) == '\0')) {
    if (0xdc < uVar22) {
      fVar33 = (float)VectorUnsignedToFloat(uVar22,(byte)(in_fpscr >> 0x15) & 3);
      fVar35 = (float)VectorUnsignedToFloat
                                ((uint)*(byte *)(unaff_r9 + 0x482),(byte)(in_fpscr >> 0x15) & 3);
      fVar34 = (float)VectorSignedToFloat((int)*(char *)(unaff_r9 + 4),(byte)(in_fpscr >> 0x15) & 3)
      ;
      fVar34 = ((fVar35 + unaff_s20) - fVar34) / ((fVar33 + unaff_s16) / unaff_s18) + unaff_s22 +
               fVar34;
      uVar22 = in_fpscr & 0xfffffff;
      uVar27 = uVar22 | (uint)(fVar34 < unaff_s24) << 0x1f | (uint)(fVar34 == unaff_s24) << 0x1e;
      in_fpscr = uVar27 | (uint)(NAN(fVar34) || NAN(unaff_s24)) << 0x1c;
      bVar16 = (byte)(uVar27 >> 0x18);
      fVar33 = unaff_s24;
      if (((bool)(bVar16 >> 6 & 1) || bVar16 >> 7 != ((byte)(in_fpscr >> 0x1c) & 1)) &&
         (uVar22 = uVar22 | (uint)(fVar34 < 0.0) << 0x1f,
         in_fpscr = uVar22 | (uint)NAN(fVar34) << 0x1c, fVar33 = fVar34,
         (byte)(uVar22 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1))) {
        fVar33 = unaff_s26;
      }
      *(char *)(unaff_r9 + 0x482) = (char)(int)fVar33;
    }
    if (0xdc < uVar25) {
      fVar33 = (float)VectorUnsignedToFloat(uVar25,(byte)(in_fpscr >> 0x15) & 3);
      fVar35 = (float)VectorUnsignedToFloat(uVar15 & 0xff,(byte)(in_fpscr >> 0x15) & 3);
      fVar34 = (float)VectorSignedToFloat((int)*(char *)(unaff_r9 + 5),(byte)(in_fpscr >> 0x15) & 3)
      ;
      fVar34 = ((fVar35 + unaff_s20) - fVar34) / ((fVar33 + unaff_s16) / unaff_s18) + unaff_s22 +
               fVar34;
      uVar15 = in_fpscr & 0xfffffff;
      uVar22 = uVar15 | (uint)(fVar34 < unaff_s24) << 0x1f | (uint)(fVar34 == unaff_s24) << 0x1e;
      in_fpscr = uVar22 | (uint)(NAN(fVar34) || NAN(unaff_s24)) << 0x1c;
      bVar16 = (byte)(uVar22 >> 0x18);
      fVar33 = unaff_s24;
      if (((bool)(bVar16 >> 6 & 1) || bVar16 >> 7 != ((byte)(in_fpscr >> 0x1c) & 1)) &&
         (uVar15 = uVar15 | (uint)(fVar34 < 0.0) << 0x1f,
         in_fpscr = uVar15 | (uint)NAN(fVar34) << 0x1c, fVar33 = fVar34,
         (byte)(uVar15 >> 0x1f) != ((byte)(in_fpscr >> 0x1c) & 1))) {
        fVar33 = unaff_s26;
      }
      *(char *)(unaff_r9 + 0x5af) = (char)(int)fVar33;
    }
  }
  if ((*(short *)(unaff_r9 + 0xdb6) != 0x12d) || (*(char *)(unaff_r9 + 0x17) == '\x01'))
  goto switchD_08037536_default;
  if ((*(byte *)(unaff_r9 + 0x18) | 2) == 2) {
    iVar14 = (short)(ushort)*(byte *)(unaff_r9 + 0x16) * 0x12d + unaff_r9 +
             (int)*(short *)(unaff_r9 + 0x1a);
    if (((uVar13 & 0xff) < (uint)*(byte *)(iVar14 + 0x3eb)) ||
       ((uint)*(byte *)(iVar14 + 0x3ec) <= (uVar13 & 0xff))) goto LAB_08037c76;
  }
  else {
LAB_08037c76:
    if (1 < *(byte *)(unaff_r9 + 0x18) - 1) goto switchD_08037536_default;
    iVar14 = (short)(ushort)*(byte *)(unaff_r9 + 0x16) * 0x12d + unaff_r9 +
             (int)*(short *)(unaff_r9 + 0x1a);
    if (((uint)*(byte *)(iVar14 + 0x3eb) < (uVar13 & 0xff)) ||
       ((uVar13 & 0xff) <= (uint)*(byte *)(iVar14 + 0x3ec))) goto switchD_08037536_default;
  }
  if (*(char *)(unaff_r9 + 0x17) == '\0') {
    *(undefined4 *)(unaff_r9 + 0xdb4) = 0x12d;
    goto switchD_08037536_default;
  }
  *(undefined1 *)(unaff_r9 + 0x2e) = 0;
  param_3 = 0xffffffff;
  _DAT_40000400 = _DAT_40000400 & 0xfffffffe;
  param_1 = (undefined4 *)&DAT_20002d6c;
  goto code_r0x08037cce;
}



