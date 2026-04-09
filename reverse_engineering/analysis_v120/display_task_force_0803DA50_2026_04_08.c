// Force-decompiled functions

// Attempting to create function at 0x0803DA50
// Successfully created function
// ==========================================
// Function: FUN_0803da50 @ 0803da50
// Size: 218 bytes
// ==========================================

float FUN_0803da50(int param_1,uint param_2,int param_3)

{
  uint uVar1;
  int iVar2;
  float fVar3;
  undefined8 unaff_d9;
  
  uVar1 = 1 << (0x96U - param_3 & 0xff);
  if ((uVar1 - 1 & param_2) == 0) {
    if ((uVar1 & param_2) == 0) {
      iVar2 = 2;
    }
    else {
      iVar2 = 1;
    }
  }
  else {
    iVar2 = 0;
  }
  if (param_1 == -0x800000) {
    if ((param_2 & 0x80000000) == 0) {
      if (iVar2 != 1) {
        return DAT_0803db90;
      }
      return DAT_0803db8c;
    }
  }
  else {
    if (param_1 == 0) {
      if ((param_2 & 0x80000000) != 0) goto LAB_0803daea;
      goto LAB_0803db16;
    }
    if (param_1 != -0x80000000) {
      if (param_1 == -0x40800000) {
        return 1.0;
      }
      if ((uint)(0x7effffff < (uint)(param_1 * 2)) == ((int)param_2 >> 0x1f) + 1U) {
        return DAT_0803db90;
      }
      goto LAB_0803db16;
    }
    if (param_2 == 0x7f800000) goto LAB_0803db16;
    if (param_2 == 0xff800000) {
LAB_0803daea:
      FUN_0800088e(2);
      fVar3 = (float)FUN_0803ddf8();
      return fVar3;
    }
    if ((param_2 & 0x80000000) != 0) {
      if ((iVar2 != 0) && (iVar2 == 1)) {
        FUN_0800088e(2);
        fVar3 = (float)FUN_0803ddf8();
        return -fVar3;
      }
      goto LAB_0803daea;
    }
    if (iVar2 == 0) goto LAB_0803db16;
  }
  if (iVar2 == 1) {
    return DAT_0803db94;
  }
LAB_0803db16:
  return (float)((ulonglong)unaff_d9 >> 0x20);
}



