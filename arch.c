#include <stdint.h>
#include <stdio.h>
#include<string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#define NAME_LEN 256
#define ARCHIVE_MAGIC 0xDEADBEEF //0xDE, 0xAD, 0xBE, 0xEF

#pragma pack(push, 1)
struct file_entry_disk {
    char name[NAME_LEN];
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t mtime;
};
#pragma pack(pop)

void print_help() {
    printf("Using:\n");
    printf("./archiver arch_name -i file1         //add file in archive\n");
    printf("./archiver arch_name -e file1         //extract file from archive\n");
    printf("./archiver arch_name -e file1 file2   //extract file1 from archive to file2\n");
    printf("./archiver arch_name -s               //show archive contents\n");
    printf("./archiver -h                         //get help\n");
}

int check_archive(const char *archive_name) {
    int fd = open(archive_name, O_RDONLY);
    if (fd < 0) { return 0; }

    off_t archive_end = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    uint64_t sum = 0;
    struct file_entry_disk hdr;
    while (lseek(fd, 0, SEEK_CUR) < archive_end) {
        ssize_t n = read(fd, &hdr, sizeof(hdr));
        if (n != sizeof(hdr)) {
            perror("Failed to read header, archive is corrupted");
            close(fd);
            return -1;
        }
        sum += sizeof(hdr) + hdr.size;

        if (hdr.size > (archive_end - lseek(fd, 0, SEEK_CUR))) {
            perror("File size too big, archive is corrupted");
            close(fd);
            return -1;
        }
        //to next header
        if (lseek(fd, hdr.size, SEEK_CUR) == -1) {
            perror("lseek"); close(fd); return -1;
        }
    }

    close(fd);

    if (sum != (uint64_t)archive_end) {
        perror("Archive corrupted: checksum mismatch\n");
        return -1;
    }
    return 0;
}

int add_file(const char *archive_name, const char *filename) {
    int fd_archive = open(archive_name, O_CREAT | O_RDWR | O_APPEND, 0666);//-rw-rw-rw
    if (fd_archive < 0) { perror("open archive"); return -1; }

    int fd_file = open(filename, O_RDONLY);
    if (fd_file < 0) { perror("open file"); close(fd_archive); return -1; }

    struct stat st;
    if (fstat(fd_file, &st) != 0) { perror("stat"); close(fd_file); close(fd_archive); return -1; }

    struct file_entry_disk hdr = {0};
    strncpy(hdr.name, filename, NAME_LEN-1);
    hdr.name[NAME_LEN-1]='\0';
    hdr.mode = st.st_mode;
    hdr.uid  = st.st_uid;
    hdr.gid  = st.st_gid;
    hdr.size = st.st_size;
    hdr.mtime = st.st_mtime;

    //write header
    if (write(fd_archive, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        perror("write header"); close(fd_file); close(fd_archive); return -1;
    }
    //write data
    char buf[4096];
    ssize_t n;
    while ((n = read(fd_file, buf, sizeof(buf))) > 0) {
        if (write(fd_archive, buf, n) != n) {
            perror("write data"); close(fd_file); close(fd_archive); return -1;
        }
    }
    if (n < 0) { perror("read file"); close(fd_file); close(fd_archive); return -1; }

    close(fd_file);
    close(fd_archive);
    return 0;
}

int extract_file(const char *archive_name, const char *file_to_extract, const char *new_filename) {
    int fd = open(archive_name, O_RDWR);
    if (fd < 0) {
        perror("open archive");
        return -1;
    }
    off_t archive_end = lseek(fd, 0, SEEK_END);
    if (archive_end <= 0) {
        perror("empty archive");
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    struct file_entry_disk entry;
    off_t current_pos = 0;
    off_t file_start = -1;
    off_t file_end = -1;
    while (current_pos < archive_end) {
        ssize_t hdr_read = read(fd, &entry, sizeof(entry));
        if (hdr_read != sizeof(entry)) {
            perror("Corrupted archive header");
            close(fd);
            return -1;
        }
	current_pos += sizeof(entry);
	if (current_pos + entry.size > archive_end) {
            perror("Archive corrupted: file data goes beyond archive end");
            close(fd);
            return -1;
        }
	if (strcmp(entry.name, file_to_extract) == 0) {
	    file_start = current_pos - sizeof(entry);
            file_end = current_pos + entry.size;

            const char *out_name = (new_filename && *new_filename) ? new_filename : entry.name;
            int out_fd = open(out_name, O_WRONLY | O_CREAT | O_TRUNC, entry.mode);
            if (out_fd < 0) {
                perror("create output file");
                close(fd);
                return -1;
            }

            char *buf = malloc(entry.size);
            if (!buf) {
                perror("malloc");
                close(out_fd);
                close(fd);
                return -1;
            }

            if (read(fd, buf, entry.size) != (ssize_t)entry.size) {
                perror("read file data");
                free(buf);
                close(out_fd);
                close(fd);
                return -1;
            }

            write(out_fd, buf, entry.size);
            free(buf);
            close(out_fd);

            printf("Extracted: %s → %s\n", entry.name, out_name);
            break;
        } else {
            //skip data
            lseek(fd, entry.size, SEEK_CUR);
            current_pos += entry.size;
        }
    }
    if (file_start == -1) {
        perror("File not found in archive");
        close(fd);
        return -1;
    }
    off_t move_size = archive_end - file_end;
    if (move_size > 0) {
        char *buf = malloc(move_size);
        if (!buf) {
            perror("malloc");
            close(fd);
            return -1;
        }

        lseek(fd, file_end, SEEK_SET);
        if (read(fd, buf, move_size) != move_size) {
            perror("read tail");
            free(buf);
            close(fd);
            return -1;
        }

        lseek(fd, file_start, SEEK_SET);
        if (write(fd, buf, move_size) != move_size) {
            perror("write tail");
            free(buf);
            close(fd);
            return -1;
        }

        free(buf);
    }
    if (ftruncate(fd, archive_end - (file_end - file_start)) < 0) {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    close(fd);
    printf("File '%s' deleted from archive\n", file_to_extract);
    return 0;
}

int show_stat(const char* archive_name) {
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 2) { print_help(); return 1; }
    if (strcmp(argv[1], "-h") == 0) { print_help(); return 0; }
    if (argc < 3) { print_help(); return 1; }

    const char *archive_name = argv[1];
    const char *flag = argv[2];
    if (check_archive(archive_name) != 0)
	return 1;
    if ((strcmp(flag, "-i") == 0 || strcmp(flag, "--input") == 0) && argc == 4) {
        return add_file(archive_name, argv[3]);
    } else if ((strcmp(flag, "-e") == 0 || strcmp(flag, "--extract") == 0) && argc == 4) {
        return extract_file(archive_name, argv[3], NULL);
    } else if ((strcmp(flag, "-e") == 0 || strcmp(flag, "--extract") == 0) && argc == 5) {
	return extract_file(archive_name, argv[3], argv[4]);
    } else if ((strcmp(flag, "-s") == 0 || strcmp(flag, "--stat") == 0)) {
        return show_stat(archive_name);
    } else {
        print_help();
        return 1;
    }
    return 0;
}
