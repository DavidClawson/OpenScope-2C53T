// Force-decompiled functions

// Attempting to create function at 0x080156d0
// Successfully created function
// ==========================================
// Function: FUN_080156d0 @ 080156d0
// Size: 256 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_080156d0(void)

{
  char cVar1;
  undefined1 uVar2;
  uint uVar3;
  uint extraout_r1;
  int unaff_r9;
  bool bVar4;
  undefined4 in_stack_0000003c;
  
  cVar1 = *(char *)(unaff_r9 + 0xe10);
  if (((((cVar1 != -1) && (cVar1 != '\0')) ||
       ((*(char *)(unaff_r9 + 0x2f) != '\0' || *(char *)(unaff_r9 + 0x30) != '\0') ||
        *(char *)(unaff_r9 + 0x33) != '\0')) ||
      ((uVar3 = (uint)*(byte *)(unaff_r9 + 0x2e), *(char *)(unaff_r9 + 0x17) == '\x02' &&
       (uVar3 != 0)))) ||
     ((uVar3 == 0 &&
      (bVar4 = *(uint *)(unaff_r9 + 0xe08) < 10, uVar3 = -(uint)!bVar4 - *(int *)(unaff_r9 + 0xe0c),
      (uint)-*(int *)(unaff_r9 + 0xe0c) < (uint)bVar4)))) {
    FUN_08019af8(0xea,0x51,0x45,0x51);
    FUN_08019af8(0x45,0x51,0xef,0x55);
    FUN_08019af8(0xef,0x55,0x3e,0x59);
    FUN_08019af8(0x3e,0x59,0x101,0x55);
    FUN_08019af8(0x101,0x55,0x3e,0xa1);
    FUN_08019af8(0x3e,0xa1,0x101,0x98);
    FUN_08018fcc(0x3d,0x50,0xc6,0x53);
    cVar1 = *(char *)(unaff_r9 + 0xe10);
    uVar3 = extraout_r1;
  }
  if ((cVar1 == '\x03') || (cVar1 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (cVar1 == '\x01') {
    uVar2 = FUN_08034878(0x80bc18b,uVar3);
    *(undefined1 *)(unaff_r9 + 0xe1b) = uVar2;
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,*(undefined1 *)(unaff_r9 + 0xe11));
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    *(undefined1 *)(unaff_r9 + 0xe10) = 0xff;
    *(undefined1 *)(unaff_r9 + 0xf68) = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
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



// Attempting to create function at 0x080157b4
// Successfully created function
// ==========================================
// Function: FUN_080157b4 @ 080157b4
// Size: 28 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_080157b4(void)

{
  char cVar1;
  undefined1 uVar2;
  int unaff_r9;
  undefined4 in_stack_0000003c;
  
  cVar1 = *(char *)(unaff_r9 + 0xe10);
  if ((cVar1 == '\x03') || (cVar1 == '\x02')) {
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  else if (cVar1 == '\x01') {
    uVar2 = FUN_08034878(0x80bc18b);
    *(undefined1 *)(unaff_r9 + 0xe1b) = uVar2;
    FUN_08008154(0x3f,0x60,0xc2,0x19);
    FUN_080003b4(&DAT_20008360,0x80bcae5,*(undefined1 *)(unaff_r9 + 0xe11));
    FUN_08008154(0x3f,0x76,0xc2,0x19);
    *(undefined1 *)(unaff_r9 + 0xe10) = 0xff;
    *(undefined1 *)(unaff_r9 + 0xf68) = 10;
    in_stack_0000003c._3_1_ = 0x24;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
    in_stack_0000003c._3_1_ = 3;
    FUN_0803acf0(_DAT_20002d6c,(int)&stack0x0000003c + 3,0xffffffff);
  }
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



// ==========================================
// Function: FUN_080157d0 @ 080157d0
// Size: 120 bytes
// ==========================================

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_080157d0(undefined1 param_1)

{
  int unaff_r9;
  undefined1 *puStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  undefined1 uStack0000003f;
  
  puStack00000000 = *(undefined1 **)(&DAT_0804bfcc + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  *(undefined1 *)(unaff_r9 + 0xe1b) = param_1;
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 1;
  FUN_08008154(0x3f,0x60,0xc2,0x19);
  FUN_080003b4(&DAT_20008360,0x80bcae5,*(undefined1 *)(unaff_r9 + 0xe11));
  puStack00000000 = &DAT_20008360;
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 1;
  FUN_08008154(0x3f,0x76,0xc2,0x19);
  *(undefined1 *)(unaff_r9 + 0xe10) = 0xff;
  *(undefined1 *)(unaff_r9 + 0xf68) = 10;
  uStack0000003f = 0x24;
  FUN_0803acf0(_DAT_20002d6c,&stack0x0000003f,0xffffffff);
  uStack0000003f = 3;
  FUN_0803acf0(_DAT_20002d6c,&stack0x0000003f,0xffffffff);
  if ((*(char *)(unaff_r9 + 0x17) == '\x02') && (*(char *)(unaff_r9 + 0x2e) != '\0')) {
    puStack00000000 = *(undefined1 **)(&DAT_0804c02c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 1;
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (*(char *)(unaff_r9 + 0x2f) != '\0') {
    puStack00000000 = *(undefined1 **)(&DAT_0804c04c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x30) != '\0') {
    puStack00000000 = *(undefined1 **)(&DAT_0804c06c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((*(char *)(unaff_r9 + 0x2e) == '\0') &&
     ((uint)-*(int *)(unaff_r9 + 0xe0c) < (uint)(*(uint *)(unaff_r9 + 0xe08) < 10))) {
    puStack00000000 = *(undefined1 **)(&DAT_0804c08c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x33) != '\0') {
    puStack00000000 = *(undefined1 **)(&DAT_0804c0ac + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  *(char *)(unaff_r9 + 0xf70) = *(char *)(unaff_r9 + 0xf70) + '\x01';
  return;
}



// ==========================================
// Function: FUN_08015876 @ 08015876
// Size: 14 bytes
// ==========================================

void FUN_08015876(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804c00c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 1;
  FUN_08008154(0x3f,0x50,0xc2,0x53);
  if ((*(char *)(unaff_r9 + 0x17) == '\x02') && (*(char *)(unaff_r9 + 0x2e) != '\0')) {
    uStack00000000 = *(undefined4 *)(&DAT_0804c02c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 1;
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (*(char *)(unaff_r9 + 0x2f) != '\0') {
    uStack00000000 = *(undefined4 *)(&DAT_0804c04c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x30) != '\0') {
    uStack00000000 = *(undefined4 *)(&DAT_0804c06c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((*(char *)(unaff_r9 + 0x2e) == '\0') &&
     ((uint)-*(int *)(unaff_r9 + 0xe0c) < (uint)(*(uint *)(unaff_r9 + 0xe08) < 10))) {
    uStack00000000 = *(undefined4 *)(&DAT_0804c08c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x33) != '\0') {
    uStack00000000 = *(undefined4 *)(&DAT_0804c0ac + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  *(char *)(unaff_r9 + 0xf70) = *(char *)(unaff_r9 + 0xf70) + '\x01';
  return;
}



// ==========================================
// Function: FUN_08015884 @ 08015884
// Size: 68 bytes
// ==========================================

void FUN_08015884(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804bfec + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 1;
  FUN_08008154(0x3f,0x50,0xc2,0x53);
  if ((*(char *)(unaff_r9 + 0x17) == '\x02') && (*(char *)(unaff_r9 + 0x2e) != '\0')) {
    uStack00000000 = *(undefined4 *)(&DAT_0804c02c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 1;
    FUN_08008154(0x3f,0x50,0xc2,0x53);
  }
  if (*(char *)(unaff_r9 + 0x2f) != '\0') {
    uStack00000000 = *(undefined4 *)(&DAT_0804c04c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x30) != '\0') {
    uStack00000000 = *(undefined4 *)(&DAT_0804c06c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if ((*(char *)(unaff_r9 + 0x2e) == '\0') &&
     ((uint)-*(int *)(unaff_r9 + 0xe0c) < (uint)(*(uint *)(unaff_r9 + 0xe08) < 10))) {
    uStack00000000 = *(undefined4 *)(&DAT_0804c08c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  if (*(char *)(unaff_r9 + 0x33) != '\0') {
    uStack00000000 = *(undefined4 *)(&DAT_0804c0ac + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
    uStack00000004 = 0x20001174;
    uStack00000008 = 0x55f;
    uStack0000000c = 0x81;
    FUN_08008154(0x3f,0x58,0xc2,0x4b);
  }
  *(char *)(unaff_r9 + 0xf70) = *(char *)(unaff_r9 + 0xf70) + '\x01';
  return;
}



// ==========================================
// Function: FUN_0801819c @ 0801819c
// Size: 198 bytes
// ==========================================

void FUN_0801819c(void)

{
  byte bVar1;
  undefined4 uVar2;
  
  FUN_080003b4(&DAT_20008360,0x80bc681,DAT_20000f14 + 0x94);
  FUN_08032f6c(&DAT_20008360,0x3d,0x32);
  FUN_08008154(0x3f,0x30,0xc2,0x14,*(undefined4 *)((uint)DAT_20001058 * 4 + 0x8018260),0x20001144,0,
               1);
  bVar1 = DAT_20000f14;
  uVar2 = 0xffff;
  if (DAT_20000f14 == 1) {
    uVar2 = 0x55f;
  }
  FUN_08008154(0x3f,0x5c,0xc2,0x14,*(undefined4 *)(&DAT_08018280 + (uint)DAT_20001058 * 4),
               0x20001144,uVar2,1);
  uVar2 = 0x55f;
  if (bVar1 == 1) {
    uVar2 = 0xffff;
  }
  FUN_08008154(0x3f,0x83,0xc2,0x14,*(undefined4 *)(&DAT_080182a0 + (uint)DAT_20001058 * 4),
               0x20001144,uVar2,1);
  return;
}



