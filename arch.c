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
    uint64_t data_offset;
};
struct archive_header {
    uint32_t magic;
    uint32_t num_files;
};
#pragma pack(pop)

void print_help() {
    printf("Using:\n");
    printf("./archiver arch_name -i file1    //add file in archive\n");
    printf("./archiver arch_name -e file1    //extract file from archive\n");
    printf("./archiver arch_name -s          //show archive contents\n");
    printf("./archiver -h                    //get help\n");
}

int add_file(const char *archive_name, const char *filename) {
    int fd_archive = open(archive_name, O_RDWR | O_CREAT, 0666);
    if (fd_archive < 0) { perror("open archive"); return -1; }

    int fd_file = open(filename, O_RDONLY);
    if (fd_file < 0) { perror("open file"); close(fd_archive); return -1; }

    struct stat st_file;
    if (fstat(fd_file, &st_file) < 0) { perror("fstat file"); close(fd_file); close(fd_archive); return -1; }

    struct file_entry_disk entry = {0};
    strncpy(entry.name, filename, NAME_LEN - 1);
    entry.name[NAME_LEN-1] = '\0';
    entry.mode = st_file.st_mode;
    entry.uid  = st_file.st_uid;
    entry.gid  = st_file.st_gid;
    entry.size = st_file.st_size;
    entry.mtime = st_file.st_mtime;

    struct archive_header arch_hdr = {0};
    ssize_t hdr_read = read(fd_archive, &arch_hdr, sizeof(arch_hdr));
    if (hdr_read < 0) {
	perror("read header");
	close(fd_file);
        close(fd_archive);
        return -1;
    }

    if (hdr_read == 0) {
        arch_hdr.magic = ARCHIVE_MAGIC;
        arch_hdr.num_files = 1;
    } else if (arch_hdr.magic != ARCHIVE_MAGIC) {
        perror("Not a valid archive");
        close(fd_file);
        close(fd_archive);
        return -1;
    } else {
        arch_hdr.num_files += 1;
    }
    //save old data
    off_t old_size = lseek(fd_archive, 0, SEEK_END) - sizeof(arch_hdr);
    char *tmp_buf = NULL;
    if (old_size > 0) {
        tmp_buf = malloc(old_size);
        if (!tmp_buf) {
	    perror("malloc");
	    close(fd_file);
            close(fd_archive);
            return -1;
	}
        if (pread(fd_archive, tmp_buf, old_size, sizeof(arch_hdr)) != old_size) {
	    perror("pread"); free(tmp_buf);
	    close(fd_file);
            close(fd_archive);
            return -1;
	}
    }
    //
    if (lseek(fd_archive, 0, SEEK_SET) < 0) {
	perror("lseek");
	free(tmp_buf);
	close(fd_file);
        close(fd_archive);
        return -1;
    }
    if (write(fd_archive, &arch_hdr, sizeof(arch_hdr)) != sizeof(arch_hdr)) {
	perror("write header");
	free(tmp_buf);
	close(fd_file);
        close(fd_archive);
        return -1;
    }
    entry.data_offset = sizeof(arch_hdr) + arch_hdr.num_files * sizeof(entry);
    if (write(fd_archive, &entry, sizeof(entry)) != sizeof(entry)) {
	perror("write entry");
	free(tmp_buf);
        close(fd_file);
        close(fd_archive);
        return -1;	
    }
    if (old_size > 0) {
        if (write(fd_archive, tmp_buf, old_size) != old_size) {
	    perror("write old bytes");
	    free(tmp_buf);
	    close(fd_file);
            close(fd_archive);
            return -1;
	}
        free(tmp_buf);
    }
    char buf[4096];
    ssize_t r;
    while ((r = read(fd_file, buf, sizeof(buf))) > 0) {
        if (write(fd_archive, buf, r) != r) {
	    perror("write file data");
	    close(fd_file);
            close(fd_archive);
            return -1;
	}
    }
    if (r < 0) {
	perror("read file data");
        close(fd_file);
        close(fd_archive);
        return -1;
    }

    close(fd_file);
    close(fd_archive);
    return 0;
}

int extract_file(const char *archive_name, const char *filename) {
    return 0;
}

int show_stat(const char *archive_name) {
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { print_help(); return 1; }
    if (strcmp(argv[1], "-h") == 0) { print_help(); return 0; }
    if (argc < 3) { print_help(); return 1; }

    const char *archive_name = argv[1];
    const char *flag = argv[2];

    if ((strcmp(flag, "-i") == 0 || strcmp(flag, "--input") == 0) && argc == 4) {
        return add_file(archive_name, argv[3]);
    } else if ((strcmp(flag, "-e") == 0 || strcmp(flag, "--extract") == 0) && argc == 4) {
        return extract_file(archive_name, argv[3]);
    } else if ((strcmp(flag, "-s") == 0 || strcmp(flag, "--stat") == 0)) {
        return show_stat(archive_name);
    } else {
        print_help();
        return 1;
    }
    return 0;
}
