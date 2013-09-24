/* rebuild-adb-backup.c: Rebuild ADB backups corrupted by --shared on 4.0.x
 *
 * Copyright (c) 2013 Robert Grimm
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <arpa/inet.h>  /* for ntohl() */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 4096

typedef struct {
  unsigned char backup_manifest_version_;
  unsigned char compression_flag_;
  char encryption_type_[16];
} ab_header_t;

void print_usage(const char *self) {
  fprintf(stderr,
          "Usage: %s backup.ab [fixed_backup.ab] [shared_data.tar]\n",
          self);
}

/*@null@*/
ab_header_t *read_ab_header(FILE *in) {
  ab_header_t *header = (ab_header_t *)calloc(1, sizeof(ab_header_t));
  size_t records_read;

  if (header == NULL) {
    return NULL;
  }

  /* Android backup header format is described in */
  records_read = fscanf(in,
                        "ANDROID BACKUP\n%hhu\n%hhu\n%16s\n",
                        &header->backup_manifest_version_,
                        &header->compression_flag_,
                        header->encryption_type_);

  if (records_read < 3) {
    free(header);
    return NULL;
  }

  return header;
}

void free_ab_header(ab_header_t *header) {
  free(header);
}

int seek_to_tar_position(FILE *in) {
  const char *find = "ustar";
  const char *finding = find;
  char buf[BUF_SIZE];

  while (!feof(in)) {
    size_t bytes_read = fread(buf, 1, BUF_SIZE, in);
    size_t bytes_left = bytes_read;
    char *working;
    for (working = buf; bytes_left > 0; --bytes_left, ++working) {
      if (*working == *finding) {
        finding++;
        if (*finding == '\0') {
          break;
        }
      } else {
          finding = find;
      }
    }

    if (bytes_left != 0) {
      (void) fseek(in, -bytes_left - 256 - strlen(find), SEEK_CUR);
      return 0;
    }
  }

  return 1;
}

static void direct_copy(FILE *in, FILE *out, long size) {
  char buf[BUF_SIZE];
  long blocks = size / BUF_SIZE;
  long remaining = size % BUF_SIZE;

  for (; blocks > 0; --blocks) {
    (void) fread(buf, 1, BUF_SIZE, in);
    (void) fwrite(buf, 1, BUF_SIZE, out);
  }

  if (remaining > 0) {
    (void) fread(buf, 1, remaining, in);
    (void) fwrite(buf, 1, remaining, out);
  }
}

void direct_copy_to_end(FILE *in, FILE *out) {
  char buf[BUF_SIZE];

  while (!feof(in) && !ferror(in)) {
    size_t bytes_read = fread(buf, 1, BUF_SIZE, in);
    (void) fwrite(buf, 1, bytes_read, out);
  }
}

void unchunk_copy(FILE *in, FILE *out) {
  /* assuming int is 4-bytes */
  unsigned int chunk_size;
  do {
    /* Read the next chunk size */
    size_t items_read = fread(&chunk_size, sizeof(chunk_size), 1, in);
    if (items_read < 1) {
      fprintf(stderr, "Error while reading chunk size.");
      break;
    }

    /* Convert to host byte order */
    chunk_size = ntohl(chunk_size);

    /* Copy the data */
    direct_copy(in, out, chunk_size);
  } while (chunk_size > 0);
}

int main(int argc, const char **argv) {
  static const char *fixed_output_file = "fixed_backup.ab";
  static const char *shared_data_file = "shared_data.tar";
  FILE *in, *out_fixed, *out_tar;
  ab_header_t *header;
  long tar_offset;

  if (argc < 2 || argc > 4) {
    print_usage(argv[0]);
    return 1;
  }
  if (argc > 2) {
    fixed_output_file = argv[2];
  }
  if (argc > 3) {
    shared_data_file = argv[3];
  }

  in = fopen(argv[1], "rb");
  if (in == NULL) {
    perror("main");
    fclose(in);
    return 3;
  }

  header = read_ab_header(in);

  if (header == NULL) {
    fprintf(stderr, "Error reading header.\n");
    fclose(in);
    return 2;
  }

  fprintf(stdout, "ADB Backup Version: %i\n", header->backup_manifest_version_);
  fprintf(stdout, "ADB Compression: %i\n", header->compression_flag_);
  fprintf(stdout, "ADB Encryption: %s\n", header->encryption_type_);

  if (header->backup_manifest_version_ != 1 ||
      header->compression_flag_ != 1) {
    fprintf(stderr,
            "Warning: input file does not look like it could be a "
            "corrupted ADB backup.");
  }

  free_ab_header(header);
  header = NULL;

  fprintf(stdout, "Searching for uncompressed TAR data... ");
  fflush(stdout);

  if (!seek_to_tar_position(in)) {
    /* The TAR file is chunked; each chunk is prepended with a 4-byte size
     * For more info, see frameworks/base/libs/androidfw/BackupHelpers.cpp
     * method send_tarfile_chunk()
     */
    (void) fseek(in, -4, SEEK_CUR);
    tar_offset = ftell(in);
    fprintf(stdout, "found at byte %li\n", tar_offset);
  } else {
    fprintf(stderr, "not found.\nExiting.\n");
    fclose(in);
    return 5;
  }

  fprintf(stdout, "Beginning rebuild of backup into: %s\n", fixed_output_file);
  fflush(stdout);
  (void) fseek(in, 0, SEEK_SET);
  out_fixed = fopen(fixed_output_file, "wb");
  direct_copy(in, out_fixed, tar_offset);

  fprintf(stdout, "Extracting uncompressed TAR into: %s\n", shared_data_file);
  fflush(stdout);

  out_tar = fopen(shared_data_file, "wb");
  unchunk_copy(in, out_tar);

  fprintf(stdout, "Offset after TAR extraction: %li\n", ftell(in));
  fprintf(stdout, "Continuing rebuild of backup\n");
  fflush(stdout);

  direct_copy_to_end(in, out_fixed);

  fprintf(stdout, "ADB backup rebuild complete.\nExiting.\n");

  fclose(in);
  fclose(out_tar);
  fclose(out_fixed);

  return 0;
}
