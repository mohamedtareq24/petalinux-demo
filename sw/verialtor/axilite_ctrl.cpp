#include "axilite_ctrl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef SIMULATION
#include "Vfir_top.h"

extern Vfir_top *top;
extern void tick();

namespace {

constexpr std::uint32_t kAxiLiteBaseAddr = 0xA0000000U;

} // namespace

static void axil_write_cycle(std::uint32_t offset, std::uint32_t value)
{
    if (top == nullptr) {
        std::fprintf(stderr, "ERROR: Verilated top is null.\n");
        std::exit(EXIT_FAILURE);
    }

    top->s_axi_awaddr = kAxiLiteBaseAddr + offset;
    top->s_axi_awprot = 0;
    top->s_axi_wdata = value;
    top->s_axi_wstrb = 0xF;
    top->s_axi_awvalid = 1;
    top->s_axi_wvalid = 1;
    top->s_axi_bready = 1;

    std::uint32_t cycles = 0;
    constexpr std::uint32_t kMaxCycles = 10000U;

    while (cycles < kMaxCycles) {
        tick();
        ++cycles;

        if (top->s_axi_bvalid && top->s_axi_bready) {
            break;
        }
    }

    if (!(top->s_axi_bvalid && top->s_axi_bready)) {
        std::fprintf(stderr,
                     "ERROR: AXI-Lite write response timeout (offset=0x%08X, value=0x%08X).\n",
                     kAxiLiteBaseAddr + offset,
                     value);
        std::exit(EXIT_FAILURE);
    }

    top->s_axi_awvalid = 0;
    top->s_axi_wvalid = 0;
    top->s_axi_bready = 0;
    tick();
}

bool axilite_init(const char *uio_path_override)
{
    (void)uio_path_override;
    return true;
}

void axilite_write_reg(std::uint32_t offset, std::uint32_t value)
{
    axil_write_cycle(offset, value);
}

void axilite_shutdown()
{
}

#else

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

volatile std::uint32_t *g_fir_regs = nullptr;
void *g_fir_map = nullptr;
std::size_t g_fir_map_size = 0;
int g_uio_fd = -1;

bool read_uio_size(const char *path, std::size_t *value)
{
    FILE *file = std::fopen(path, "r");
    if (file == nullptr) {
        return false;
    }

    char buffer[64] = {};
    if (std::fgets(buffer, sizeof(buffer), file) == nullptr) {
        std::fclose(file);
        return false;
    }

    std::fclose(file);

    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(buffer, &end, 0);
    if (errno != 0 || end == buffer) {
        return false;
    }

    *value = static_cast<std::size_t>(parsed);
    return true;
}

bool find_uio_device(const char *target, char *devpath, std::size_t devlen)
{
    DIR *dir = opendir("/sys/class/uio");
    if (dir == nullptr) {
        return false;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char name_path[128];
        std::snprintf(name_path, sizeof(name_path), "/sys/class/uio/%s/name", entry->d_name);

        FILE *name_file = std::fopen(name_path, "r");
        if (name_file == nullptr) {
            continue;
        }

        char name[64] = {};
        const bool name_ok = std::fgets(name, sizeof(name), name_file) != nullptr;
        std::fclose(name_file);
        if (!name_ok) {
            continue;
        }

        name[std::strcspn(name, "\n")] = '\0';
        if (std::strstr(name, target) != nullptr) {
            std::snprintf(devpath, devlen, "/dev/%s", entry->d_name);
            closedir(dir);
            return true;
        }
    }

    closedir(dir);
    return false;
}

std::size_t get_uio_map_size(const char *devpath)
{
    const char *device_name = std::strrchr(devpath, '/');
    device_name = (device_name != nullptr) ? device_name + 1 : devpath;

    char sysfs_path[160];
    std::snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/uio/%s/maps/map0/size", device_name);

    std::size_t region_size = 0;
    if (!read_uio_size(sysfs_path, &region_size) || region_size == 0) {
        region_size = 0x1000UL;
    }
    return region_size;
}

bool map_fir_uio(const char *devpath)
{
    g_uio_fd = open(devpath, O_RDWR | O_SYNC);
    if (g_uio_fd < 0) {
        std::perror("open");
        return false;
    }

    g_fir_map_size = get_uio_map_size(devpath);
    g_fir_map = mmap(nullptr,
                     g_fir_map_size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     g_uio_fd,
                     0);
    if (g_fir_map == MAP_FAILED) {
        std::perror("mmap");
        close(g_uio_fd);
        g_uio_fd = -1;
        g_fir_map = nullptr;
        g_fir_map_size = 0;
        return false;
    }

    g_fir_regs = static_cast<volatile std::uint32_t *>(g_fir_map);
    return true;
}

} // namespace

bool axilite_init(const char *uio_path_override)
{
    char devpath[64] = {};

    if (uio_path_override != nullptr && uio_path_override[0] == '/') {
        std::snprintf(devpath, sizeof(devpath), "%s", uio_path_override);
    } else if (!find_uio_device("fir", devpath, sizeof(devpath))) {
        std::fprintf(stderr, "ERROR: Unable to locate FIR UIO device.\n");
        return false;
    }

    return map_fir_uio(devpath);
}

void axilite_write_reg(std::uint32_t offset, std::uint32_t value)
{
    if (g_fir_regs == nullptr) {
        std::fprintf(stderr, "ERROR: FIR register window is not mapped.\n");
        std::exit(EXIT_FAILURE);
    }

    g_fir_regs[offset / 4U] = value;
}

void axilite_shutdown()
{
    if (g_fir_map != nullptr) {
        munmap(g_fir_map, g_fir_map_size);
        g_fir_map = nullptr;
        g_fir_regs = nullptr;
        g_fir_map_size = 0;
    }

    if (g_uio_fd >= 0) {
        close(g_uio_fd);
        g_uio_fd = -1;
    }
}

#endif
