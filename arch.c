#include <stdint.h>
#define NAME_LEN 256

#pragma pack(push, 1)
struct file_entry_disk {
    char name[NAME_LEN];
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t mtime;
    uint64_t data_offset;
} __attribute__((packed));
#pragma pack(pop)

void print_help() {
    printf("Using:\n");
    printf("./archiver arch_name -i file1    //add file in archive\n");
    printf("./archiver arch_name -e file1    //extract file from archive\n");
    printf("./archiver arch_name -s          //show archive contents\n");
    printf("./archiver -h                    //get help\n");
}

int add_file(const char *archive_name, const char *filename) {
    int fd_archive = open(archive_name, O_RDWR | O_CREAT, 0666); //-rw-rw-rw-
    if (fd_archive < 0) { perror("open archive"); return -1; }
    
    int fd_file = open(filename, O_RDONLY);
    if (fd_file < 0) { perror("open file"); close(fd_archive); return -1; }

    struct stat st;
    if (fstat(fd_file, &st) < 0) { perror("fstat"); close(fd_file); close(fd_archive); return -1; }

    struct file_entry_disk entry = {0};
    strncpy(entry.name, filename, NAME_LEN - 1);
    entry.name[NAME_LEN-1] = '\0';
    entry.mode = st.st_mode;
    entry.uid  = st.st_uid;
    entry.gid  = st.st_gid;
    entry.size = st.st_size;
    entry.mtime = st.st_mtime;

    //go to end of archive
    if (lseek(fd_archive, 0, SEEK_END) < 0) { perror("lseek"); close(fd_file); close(fd_archive); return -1; }
    //remember position where we going to write file
    entry.data_offset = lseek(fd_archive, 0, SEEK_CUR);
    if (entry.data_offset < 0) { perror("lseek"); close(fd_file); close(fd_archive); return -1; }
    //write file
    char buf[4096];
    ssize_t r;
    while ((r = read(fd_file, buf, sizeof(buf))) > 0) {
        if (write(fd_archive, buf, r) != r) { perror("write"); close(fd_file); close(fd_archive); return -1; }
    }
    if (r < 0) { perror("read"); close(fd_file); close(fd_archive); return -1; }
    //go to start of archive
    if (lseek(fd_archive, 0, SEEK_SET) < 0) { perror("lseek"); close(fd_file); close(fd_archive); return -1; }
    //write info about file
    if (write(fd_archive, &entry, sizeof(entry)) != sizeof(entry)) { perror("write header"); close(fd_file); close(fd_archive); return -1; }

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
