// Force-decompiled functions

// Attempting to create function at 0x080157d0
// Successfully created function
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



// Attempting to create function at 0x08015876
// Successfully created function
// ==========================================
// Function: FUN_08015876 @ 08015876
// Size: 406 bytes
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



// Attempting to create function at 0x08015884
// Successfully created function
// ==========================================
// Function: FUN_08015884 @ 08015884
// Size: 404 bytes
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



// Attempting to create function at 0x080158c8
// Successfully created function
// ==========================================
// Function: FUN_080158c8 @ 080158c8
// Size: 336 bytes
// ==========================================

void FUN_080158c8(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804c02c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 1;
  FUN_08008154(0x3f,0x50,0xc2,0x53);
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



// Attempting to create function at 0x08015904
// Successfully created function
// ==========================================
// Function: FUN_08015904 @ 08015904
// Size: 276 bytes
// ==========================================

void FUN_08015904(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804c04c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 0x81;
  FUN_08008154(0x3f,0x58,0xc2,0x4b);
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



// Attempting to create function at 0x08015940
// Successfully created function
// ==========================================
// Function: FUN_08015940 @ 08015940
// Size: 216 bytes
// ==========================================

void FUN_08015940(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804c06c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 0x81;
  FUN_08008154(0x3f,0x58,0xc2,0x4b);
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



// Attempting to create function at 0x08015990
// Successfully created function
// ==========================================
// Function: FUN_08015990 @ 08015990
// Size: 136 bytes
// ==========================================

void FUN_08015990(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804c08c + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 0x81;
  FUN_08008154(0x3f,0x58,0xc2,0x4b);
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



// Attempting to create function at 0x080159cc
// Successfully created function
// ==========================================
// Function: FUN_080159cc @ 080159cc
// Size: 76 bytes
// ==========================================

void FUN_080159cc(void)

{
  int unaff_r9;
  undefined4 uStack00000000;
  undefined4 uStack00000004;
  undefined4 uStack00000008;
  undefined4 uStack0000000c;
  
  uStack00000000 = *(undefined4 *)(&DAT_0804c0ac + (uint)*(byte *)(unaff_r9 + 0xf60) * 4);
  uStack00000004 = 0x20001174;
  uStack00000008 = 0x55f;
  uStack0000000c = 0x81;
  FUN_08008154(0x3f,0x58,0xc2,0x4b);
  *(char *)(unaff_r9 + 0xf70) = *(char *)(unaff_r9 + 0xf70) + '\x01';
  return;
}



