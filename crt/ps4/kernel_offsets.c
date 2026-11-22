/* Copyright (C) 2025 John Törnblom
   Copyright (C) 2026 ItsJokerZz

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#define MSR_LSTAR 0xC0000082

unsigned long KERNEL_ADDRESS_IMAGE_BASE = 0;
unsigned long KERNEL_ADDRESS_TARGETID = 0;
unsigned long KERNEL_ADDRESS_COPYIN = 0;
unsigned long KERNEL_ADDRESS_COPYOUT = 0;
unsigned long KERNEL_OFFSET_VMSPACE_P_ROOT = 0;
unsigned long KERNEL_IMAGE_SIZE = 0;

/**
 * Get a model specific register value (must be invoked from kernel space).
 **/
static int kexec_get_msr(struct thread *td, kexec_args_t *args) {
  unsigned int reg = (unsigned int)args->arg1;
  unsigned long *val = (unsigned long *)args->arg2;
  unsigned int edx;
  unsigned int eax;

  if (!val) {
    return EINVAL;
  }

  asm volatile(".att_syntax\n"
               "rdmsr"
               : "=d"(edx), "=a"(eax)
               : "c"(reg));

  *val = (((unsigned long)edx) << 32) | (unsigned long)eax;

  return 0;
}

/**
 * Scan for the kernel image base address. Only reached for firmware newer
 * than the table below.
 *
 * TODO: search for ELF magic in other page tables if this guess is wrong.
 **/
static int kexec_find_image_base(struct thread *td, kexec_args_t *args) {
  unsigned long *res = (unsigned long *)args->arg1;
  unsigned long base;
  unsigned int edx;
  unsigned int eax;

  if (!res) {
    return EFAULT;
  }

  asm volatile(".att_syntax\n"
               "rdmsr"
               : "=d"(edx), "=a"(eax)
               : "c"(MSR_LSTAR));

  base = (((unsigned long)edx) << 32) | (unsigned long)eax;
  base &= 0xFFFFFFFFFFFFC000;

  *res = base;

  return 0;
}

/**
 * Get the size of the kernel image.
 **/
static int kexec_get_image_size(struct thread *td, kexec_args_t *args) {
  const unsigned char *img = (const unsigned char *)args->arg1;
  unsigned long *res = (unsigned long *)args->arg2;
  const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)img;
  const Elf64_Phdr *phdr = (const Elf64_Phdr *)(img + ehdr->e_phoff);
  unsigned long min_vaddr = -1;
  unsigned long max_vaddr = 0;

  if (!img || !res) {
    return EFAULT;
  }

  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
      ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    return EINVAL;
  }

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_vaddr < min_vaddr) {
      min_vaddr = phdr[i].p_vaddr;
    }

    if (max_vaddr < phdr[i].p_vaddr + phdr[i].p_memsz) {
      max_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
    }
  }

  *res = sizeof(ehdr) + ehdr->e_phnum * sizeof(phdr) + max_vaddr - min_vaddr;

  return 0;
}

/**
 * Search for the entry to the kernel copyin function. The 3 wildcarded
 * register bytes used to be hardcoded, matching a different function.
 **/
static int kexec_find_copyin(struct thread *td, kexec_args_t *args) {
  char pattern[] = "554889e54989??4901d24939e2??174989ea4983c2104c39??7d0b4889"
                   "??4889d6e8????????65488b0425200000004c8b05????????4c8980d0"
                   "000000";
  const unsigned char *img = (const unsigned char *)args->arg1;
  unsigned long img_size = (unsigned long)args->arg2;
  unsigned long *res = (unsigned long *)args->arg3;

  if (!img || !res) {
    return EFAULT;
  }

  if (!img_size) {
    return EINVAL;
  }

  *res = (long)pattern_scan(img, img_size, pattern);

  return 0;
}

/**
 * Search for the entry to the kernel copyout function.
 **/
static int kexec_find_copyout(struct thread *td, kexec_args_t *args) {
  char pattern[] =
      "554889e565488b0425200000004c8b05????????4c8980d00000004885d"
      "274??65488b0c2500000000488b4108488b88????????48f7c100000040";
  const unsigned char *img = (const unsigned char *)args->arg1;
  unsigned long img_size = (unsigned long)args->arg2;
  unsigned long *res = (unsigned long *)args->arg3;

  if (!img || !res) {
    return EFAULT;
  }

  if (!img_size) {
    return EINVAL;
  }

  *res = (long)pattern_scan(img, img_size, pattern);

  return 0;
}

/**
 * Search for the global kernel variable targetid. The lea it finds points
 * at a struct, not targetid itself; targetid is at +0x3D in it.
 **/
static int kexec_find_targetid(struct thread *td, kexec_args_t *args) {
  char pattern[] = "488d0d????????31c0807934000f95c0";
  const unsigned char *img = (const unsigned char *)args->arg1;
  unsigned long img_size = (unsigned long)args->arg2;
  unsigned long *res = (unsigned long *)args->arg3;
  const unsigned char *match;
  int disp;

  if (!img || !res) {
    return EFAULT;
  }

  if (!img_size) {
    return EINVAL;
  }

  if (!(match = pattern_scan(img, img_size, pattern))) {
    *res = 0;
    return 0;
  }

  // decode the lea's rip-relative operand (3-byte opcode + 4-byte disp32,
  // relative to the next instruction), then step into the struct it loads
  disp = *(const int *)(match + 3);
  *res = (unsigned long)(match + 7 + disp + 0x3D);

  return 0;
}

int init_offsets(int fw_version) {
  int err;
  unsigned long lstar = 0;

  if (fw_version < 0x3500000) {
    return -ENOSYS;
  }

  if ((err = kexec(kexec_get_msr, MSR_LSTAR, &lstar))) {
    return err;
  }

  // 3.50-14.02 are confirmed below; default: is a best-effort fallback
  switch (fw_version & 0xffff0000) {
  case 0x3500000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x003A1AD0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x019A121D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x003B92A0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x003B9220;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x3550000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x003A1F10;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x019A121D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x003B96E0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x003B9660;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x3700000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x003A2000;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x019AD21D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x003B97D0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x003B9750;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x4000000:
  case 0x4010000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x0030EA00;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0200151D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00286CC0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00286C40;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x4050000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x0030EB30;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0200151D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00286DF0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00286D70;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x4060000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x0030EB40;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0200151D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00286DF0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00286D70;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x4070000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x0030EB40;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0200551D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00286DF0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00286D70;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x110;
    break;

  case 0x4500000:
  case 0x4550000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x003095D0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01B6D08D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x0014A890;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x0014A7B0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x4700000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x0030B840;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01B7D08D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00149F20;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00149E40;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x4710000:
  case 0x4720000:
  case 0x4730000:
  case 0x4740000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x0030B7D0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01B7D08D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00149F20;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00149E40;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x5000000:
  case 0x5010000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01CD068D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x001EA600;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x001EA520;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x5030000:
  case 0x5050000:
  case 0x5070000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01CD068D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x001EA710;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x001EA630;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x5500000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x022BBF8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00405BB0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00405AC0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x5530000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x022BBF8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00405AB0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x004059C0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x5550000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x022BFF8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00405E70;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00405D80;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x5560000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x022BBF8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00405E70;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00405D80;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x6000000:
  case 0x6020000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x02159B8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x001148F0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00114800;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x6200000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0215DB8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x001148F0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00114800;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x6500000:
  case 0x6510000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01BD800D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x003C13F0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x003C1300;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x6700000:
  case 0x6710000:
  case 0x6720000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01BD800D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x003C17A0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x003C16B0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x7000000:
  case 0x7010000:
  case 0x7020000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x022FED8D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x0002F230;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x0002F140;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x118;
    break;

  case 0x7500000:
  case 0x7510000:
  case 0x7550000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0222898D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x0028F9F0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x0028F900;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x8000000:
  case 0x8010000:
  case 0x8030000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01B5158D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x0025E3B0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x0025E2C0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x8500000:
  case 0x8520000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01C8338D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x003A42E0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x003A41F0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x9000000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0221688D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002716A0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002715B0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x9030000:
  case 0x9040000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0221288D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00271320;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00271230;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x9500000:
  case 0x9510000:
  case 0x9600000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0221A40D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00201EB0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00201DC0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x10000000:
  case 0x10010000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01B9E08D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x00472F10;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x00472E20;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x10500000:
  case 0x10700000:
  case 0x10710000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x01BE460D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x000D7560;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x000D7470;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x11000000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0221C60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002DDFE0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002DDEF0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x11020000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x0221C60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002DE000;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002DDF10;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x11500000:
  case 0x11520000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD590;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD4A0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x12000000:
  case 0x12020000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD670;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD580;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x12500000:
  case 0x12520000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD6B0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD5C0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x13000000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD6D0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD5E0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x13020000:
  case 0x13040000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD6E0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD5F0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x13500000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD6F0;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD600;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x13520000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD790;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD6A0;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x14000000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BDA40;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BD950;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  case 0x14020000:
    KERNEL_ADDRESS_IMAGE_BASE = lstar - 0x000001C0;
    KERNEL_ADDRESS_TARGETID = KERNEL_ADDRESS_IMAGE_BASE + 0x021CC60D;
    KERNEL_ADDRESS_COPYIN = KERNEL_ADDRESS_IMAGE_BASE + 0x002BDD40;
    KERNEL_ADDRESS_COPYOUT = KERNEL_ADDRESS_IMAGE_BASE + 0x002BDC50;

    KERNEL_OFFSET_VMSPACE_P_ROOT = 0x120;
    break;

  default:
    if (fw_version < 0x14000000) {
      return -ENOSYS;
    }

    if ((err = kexec(kexec_find_image_base, &KERNEL_ADDRESS_IMAGE_BASE))) {
      return err;
    }
    if ((err = kexec(kexec_get_image_size, KERNEL_ADDRESS_IMAGE_BASE,
                     &KERNEL_IMAGE_SIZE))) {
      return err;
    }
    if ((err = kexec(kexec_find_copyin, KERNEL_ADDRESS_IMAGE_BASE,
                     KERNEL_IMAGE_SIZE, &KERNEL_ADDRESS_COPYIN))) {
      return err;
    }
    if ((err = kexec(kexec_find_copyout, KERNEL_ADDRESS_IMAGE_BASE,
                     KERNEL_IMAGE_SIZE, &KERNEL_ADDRESS_COPYOUT))) {
      return err;
    }
    if ((err = kexec(kexec_find_targetid, KERNEL_ADDRESS_IMAGE_BASE,
                     KERNEL_IMAGE_SIZE, &KERNEL_ADDRESS_TARGETID))) {
      return err;
    }

    if (!KERNEL_ADDRESS_IMAGE_BASE || !KERNEL_IMAGE_SIZE ||
        !KERNEL_ADDRESS_COPYIN || !KERNEL_ADDRESS_COPYOUT ||
        !KERNEL_ADDRESS_TARGETID) {
      return -ENOSYS;
    }
    break;
  }

  // get the size of the kernel image
  if ((err = kexec(kexec_get_image_size, KERNEL_ADDRESS_IMAGE_BASE,
                   &KERNEL_IMAGE_SIZE))) {
    return err;
  }

  return 0;
}
