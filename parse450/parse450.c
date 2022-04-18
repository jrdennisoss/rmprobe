/*
 *  Copyright (C) 2022 Jon Dennis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>


/*
  8.4.2 IMEM Access Registers
  The IMEM access registers, CPU_iaddr and CPU_imem, provide a
  mechanism by which the host processor can write 32-bit microapplica-
  tion words into the internal instruction memory (IMEM) to initialize the
  CL450. IMEM can hold up to 512 32-bit microapplication words. The
  procedure for writing to IMEM is shown in Figure 8-6. The circled
  numbers refer to the steps that follow.
*/
#define IMEM_SIZE 512
#define IMEM_BYTE_SIZE (IMEM_SIZE * sizeof(struct exe450_code_segment_instruction))
#define DRAM_BYTE_SIZE (96 * 1024)


struct exe450_file_header {
  uint16_t      magic_number;
  uint8_t       rev_hi;
  uint8_t       rev_lo;
  uint16_t      productID;
  uint16_t      init_PC;
  uint16_t      imem_dram;
  uint16_t      exe_length_lo;
  uint16_t      exe_length_hi;
  uint16_t      num_segments;
  uint16_t      reserved_lo;
  uint16_t      reserved_hi;
  uint8_t       comment_string[80];
};

struct exe450_code_segment_header {
  uint32_t      seg_length;
  uint32_t      seg_address;
};

struct exe450_code_segment_instruction {
  /*
    instruction_word_1 and instruction_word_0 - These values are the
    actual microapplication instructions. They should be loaded into the
    CL450's local DRAM (and possibly IMEM). The first value from the
    file (instruction_word_1) is the most significant half of each CL450
    instruction, and should be written to the CL450 first. When writing
    code_segment data to the CL450's DRAM, the first word of data from the
    executable file (instruction_word_l) goes in the lower DRAM word address,
    and instruction_word_0 goes in the higher DRAM word address.
    When writing code_segment data to IMEM, instruction_word_1 is written
    to the CPU_imem register first, followed by instruction_word_0.
  */
  uint16_t      instruction_word_1; //most significant half instruction
  uint16_t      instruction_word_0; //least significant half instruction
};

#define EXE_LEN(HEADER_PTR) ( ((HEADER_PTR)->exe_length_hi << 16) | (HEADER_PTR)->exe_length_lo )

static void
print_exe450_file_header(const struct exe450_file_header * const header) {
  FILE * const out = stdout;
  fprintf(out, "PL-450 Firmware EXE Header:\n");
  fprintf(out, "  . Magic Number:              0x%04X (%s)\n", (unsigned)header->magic_number, (header->magic_number == 0xc3c3) ? "OK" : "BAD Expecting 0xC3C3");
  fprintf(out, "  . Revision:                  0x%02X%02X\n",  (unsigned)header->rev_hi, (unsigned)header->rev_lo);
  fprintf(out, "  . Product ID:                0x%04X\n",      (unsigned)header->productID);
  fprintf(out, "  . Initial Program Counter:   0x%04X\n",      (unsigned)header->init_PC);
  fprintf(out, "  . IMEM / DRAM:               0x%04X\n",      (unsigned)header->imem_dram);
  fprintf(out, "  . EXE Length:                %u\n",          (unsigned)EXE_LEN(header));
  fprintf(out, "  . Number of Segments:        %u\n",          (unsigned)header->num_segments);
  fprintf(out, "  . Comment String:            %.*s\n",        (unsigned)sizeof(header->comment_string), header->comment_string);
}

static void
print_exe450_seg_header(const struct exe450_code_segment_header * const header, const unsigned segment_index, const unsigned segment_count) {
  FILE * const out = stdout;
  fprintf(out, "PL-450 Firmware Segment %u / %u:\n", segment_index, segment_count);
  fprintf(out, "  . Segment Length:  %u\n", (unsigned)header->seg_length);
  fprintf(out, "  . Segment Address: %u\n", (unsigned)header->seg_address);
}

static void
write_instruction_to_dram(uint8_t * const out, const struct exe450_code_segment_header * const seg_header, const unsigned seg_byte_offset, struct exe450_code_segment_instruction * const instruction) {
  //memcpy(&out[seg_header->seg_address + seg_byte_offset], instruction, sizeof(*instruction));
  out[seg_header->seg_address + seg_byte_offset + 0] = (uint8_t)(instruction->instruction_word_1 >> 8);
  out[seg_header->seg_address + seg_byte_offset + 1] = (uint8_t)(instruction->instruction_word_1 & 0xFF);
  out[seg_header->seg_address + seg_byte_offset + 2] = (uint8_t)(instruction->instruction_word_0 >> 8);
  out[seg_header->seg_address + seg_byte_offset + 3] = (uint8_t)(instruction->instruction_word_0 & 0xFF);
}

static void
write_instruction_to_imem(uint8_t * const out, const struct exe450_file_header * const file_header, const struct exe450_code_segment_header * const seg_header, const unsigned seg_byte_offset, struct exe450_code_segment_instruction * const instruction) {
  uint32_t imem_byte_address_start;
  uint32_t imem_byte_address_max;
  uint32_t requested_byte_offset;
  uint32_t imem_byte_offset;

  imem_byte_address_start  = file_header->imem_dram;
  imem_byte_address_max    = imem_byte_address_start + IMEM_BYTE_SIZE;
  requested_byte_offset    = seg_header->seg_address + seg_byte_offset;

  /* only write instruction if within the imem range */
  if (requested_byte_offset < imem_byte_address_start) return;
  if (requested_byte_offset >= imem_byte_address_max) return;

  imem_byte_offset = requested_byte_offset - imem_byte_address_start;
  memcpy(&out[imem_byte_offset], instruction, sizeof(*instruction));
}

int
main(int argc, char *argv[]) {
  FILE *fh;
  struct exe450_file_header file_header;
  struct exe450_code_segment_header seg_header;
  unsigned segment_index;
  unsigned instruction_index;
  struct exe450_code_segment_instruction seg_buf[65536 / 4];
  uint8_t dram[DRAM_BYTE_SIZE];
  uint8_t imem[IMEM_BYTE_SIZE];

  fh = NULL;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s PL450_FIRMWARE_EXE_FILE\n", argv[0]);
    goto failed;
  }
  fh = fopen(argv[1], "rb");
  if (fh == NULL) {
    fprintf(stderr, "Failed to open '%s'\n", argv[1]);
    goto failed;
  }

  if (fread(&file_header, sizeof(file_header), 1, fh) != 1) {
    fprintf(stderr, "Failed to read file header. Need at least %u bytes.\n", (unsigned)sizeof(file_header));
    goto failed;
  }
  print_exe450_file_header(&file_header);
  if (file_header.magic_number != 0xc3c3) {
    fprintf(stderr, "Aborting processing here due to invalid magic number\n");
    goto failed;
  }

  memset(dram, 0, sizeof(dram));
  memset(imem, 0, sizeof(imem));

  for (segment_index = 0; segment_index < file_header.num_segments; ++segment_index) {
    if (fread(&seg_header, sizeof(seg_header), 1, fh) != 1) {
      fprintf(stderr, "Failed to read segment header. Need at least %u bytes.\n", (unsigned)sizeof(seg_header));
      goto failed;
    }
    print_exe450_seg_header(&seg_header, segment_index, file_header.num_segments);
    if (seg_header.seg_length & 0x3) {
      fprintf(stderr, "Aborting processing here due to segment length not being multiple of 4.\n");
      goto failed;
    }
    if (seg_header.seg_length > sizeof(seg_buf)) {
      fprintf(stderr, "Segment too big to fit into buffer\n");
      goto failed;
    }
    if (fread(seg_buf, seg_header.seg_length, 1, fh) != 1) {
      fprintf(stderr, "Failed to read segment data.\n");
      goto failed;
    }
    //printf("  . Data / Instructions:"); hexdump(seg_buf, seg_header.seg_length); printf("\n");
    if ((seg_header.seg_address + seg_header.seg_length) > sizeof(dram)) {
      fprintf(stderr, "Segment beyond flat memory buffer\n");
      goto failed;
    }
    for (instruction_index = 0; (instruction_index << 2) < seg_header.seg_length; ++instruction_index) {
      write_instruction_to_dram(dram, &seg_header, instruction_index << 2, &seg_buf[instruction_index]);
      write_instruction_to_imem(imem, &file_header, &seg_header, instruction_index << 2, &seg_buf[instruction_index]);
    }
  }

  if (fread(seg_buf, 1, 1, fh) != 0) {
    fprintf(stderr, "ERROR: Trailing data found after last segment\n");
    goto failed;
  }

  fclose(fh);

  //dump the flat buffers...
  fh = fopen("dram.bin", "wb");
  if (fh == NULL) {
    fprintf(stderr, "Failed to open dram.bin\n");
    goto failed;
  }
  fwrite(dram, sizeof(dram), 1, fh);
  fclose(fh);
  printf("Wrote flat DRAM contents to: dram.bin\n");

  fh = fopen("imem.bin", "wb");
  if (fh == NULL) {
    fprintf(stderr, "Failed to open imem.bin\n");
    goto failed;
  }
  fwrite(imem, sizeof(imem), 1, fh);
  fclose(fh);
  printf("Wrote flat IMEM contents to: imem.bin\n");
  return 0;

failed:
  if (fh) fclose(fh);
  return 1;

}
