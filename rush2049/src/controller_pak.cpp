// File-backed Controller Pak: the libultra osPfs* API over a raw 32 KiB pak image per controller port.
//
// Rush 2049 saves only to the Controller Pak, and N64ModernRuntime's osPfs* (librecomp/src/pak.cpp) all report
// PFS_ERR_NOPACK. This file defines every function pak.cpp does, so the linker uses these and never pulls that
// object in.
//
// The image is the real on-pak layout, the same `.mpk` format emulators use, so saves can be moved between this
// and an emulator:
//   page 0      ID area (ID block copies at 0x20, 0x60, 0x80, 0xC0)
//   pages 1, 2  inode table and its backup: 128 big-endian u16 entries; 0x0001 ends a chain, 0x0003 is free,
//               anything else is the next page. Byte 1 of the page is a checksum of entries 5..127.
//   pages 3, 4  note table: 16 entries of 32 bytes
//   pages 5-127 data, 256 bytes each
// Files are saves/controllerpak_<port>.mpk in the app folder. A port without a file gets a freshly formatted pak,
// written out on the first change.

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "recomp.h"
#include "librecomp/game.hpp"

namespace {

constexpr int PFS_ERR_INVALID = 5;
constexpr int PFS_DATA_FULL = 7;
constexpr int PFS_DIR_FULL = 8;
constexpr int PFS_ERR_EXIST = 9;
constexpr int PFS_INITIALIZED = 1;

constexpr size_t page_size = 256;
constexpr size_t pak_size = 128 * page_size;
constexpr int first_data_page = 5;
constexpr int num_pages = 128;
constexpr int num_notes = 16;
constexpr size_t note_size = 32;
constexpr size_t inode_offset = 1 * page_size;
constexpr size_t inode_backup_offset = 2 * page_size;
constexpr size_t note_table_offset = 3 * page_size;
constexpr size_t block_size = 32; // osPfsReadWriteFile offsets and sizes are in these units

constexpr uint16_t inode_last = 0x0001;
constexpr uint16_t inode_free = 0x0003;

struct Pak {
    bool loaded = false;
    std::array<uint8_t, pak_size> data{};
};

std::mutex pak_mutex;
std::array<Pak, 4> paks;

std::filesystem::path pak_path(int port) {
    return recomp::get_config_path() / "saves" / ("controllerpak_" + std::to_string(port + 1) + ".mpk");
}

uint16_t read16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
uint32_t read32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
void write16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
void write32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

uint16_t get_inode(const Pak& pak, int page) { return read16(&pak.data[inode_offset + page * 2]); }
void set_inode(Pak& pak, int page, uint16_t value) { write16(&pak.data[inode_offset + page * 2], value); }
uint8_t* note(Pak& pak, int index) { return &pak.data[note_table_offset + index * note_size]; }
bool note_used(Pak& pak, int index) { return read16(note(pak, index) + 4) != 0 && read32(note(pak, index)) != 0; }

void update_inode_checksum(Pak& pak) {
    uint32_t sum = 0;
    for (size_t i = first_data_page * 2; i < page_size; i++) {
        sum += pak.data[inode_offset + i];
    }
    pak.data[inode_offset + 1] = (uint8_t)sum;
    std::memcpy(&pak.data[inode_backup_offset], &pak.data[inode_offset], page_size);
}

void format(Pak& pak, int port) {
    pak.data.fill(0);

    // Page 0, block 0: 0x81 then 0x01..0x1F, as freshly formatted paks have it.
    pak.data[0] = 0x81;
    for (int i = 1; i < 32; i++) {
        pak.data[i] = (uint8_t)i;
    }

    // ID block (__OSPackId): repaired, random, serial_mid, serial_low, deviceid, banks, version, checksum,
    // inverted checksum. The checksum is the sum of the first 14 halfwords.
    std::array<uint8_t, 32> id{};
    write32(&id[0x00], 0xFFFFFFFF);
    // The game tells paks apart by this block, so each port's pak gets its own serial.
    write32(&id[0x04], 0x2049F00D + (uint32_t)port);
    write32(&id[0x18], 0x00010100); // deviceid 0x0001, banks 1, version 0
    uint16_t sum = 0;
    for (int i = 0; i < 28; i += 2) {
        sum += read16(&id[i]);
    }
    write16(&id[0x1C], sum);
    write16(&id[0x1E], (uint16_t)(0xFFF2 - sum));
    for (size_t offset : { 0x20, 0x60, 0x80, 0xC0 }) {
        std::memcpy(&pak.data[offset], id.data(), id.size());
    }

    for (int page = first_data_page; page < num_pages; page++) {
        set_inode(pak, page, inode_free);
    }
    update_inode_checksum(pak);
}

Pak& get_pak(int port) {
    Pak& pak = paks[port & 3];
    if (!pak.loaded) {
        std::ifstream file{ pak_path(port), std::ios::binary };
        if (file.good()) {
            file.read(reinterpret_cast<char*>(pak.data.data()), pak.data.size());
        }
        if (!file.good() || file.gcount() != (std::streamsize)pak.data.size()) {
            format(pak, port);
        }
        pak.loaded = true;
    }
    return pak;
}

void save(int port) {
    std::filesystem::path path = pak_path(port);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream file{ temp, std::ios::binary };
        file.write(reinterpret_cast<const char*>(paks[port & 3].data.data()), pak_size);
        if (!file.good()) {
            fprintf(stderr, "[pak] could not write %s\n", temp.string().c_str());
            return;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        fprintf(stderr, "[pak] could not replace %s: %s\n", path.string().c_str(), ec.message().c_str());
    }
}

int free_pages(const Pak& pak) {
    int count = 0;
    for (int page = first_data_page; page < num_pages; page++) {
        if (get_inode(pak, page) == inode_free) {
            count++;
        }
    }
    return count;
}

int file_pages(const Pak& pak, int start) {
    int count = 0;
    for (int page = start; page >= first_data_page && page < num_pages && count < num_pages; count++) {
        uint16_t next = get_inode(pak, page);
        if (next == inode_last) {
            return count + 1;
        }
        page = next & 0xFF;
    }
    return count;
}

// The pak's port, from OSPfs.channel.
int pfs_port(uint8_t* rdram, gpr pfs) {
    return MEM_W(0x08, pfs) & 3;
}

void read_guest(uint8_t* rdram, gpr addr, uint8_t* out, size_t count) {
    for (size_t i = 0; i < count; i++) {
        out[i] = (uint8_t)MEM_B(i, addr);
    }
}

void write_guest(uint8_t* rdram, gpr addr, const uint8_t* in, size_t count) {
    for (size_t i = 0; i < count; i++) {
        MEM_B(i, addr) = (int8_t)in[i];
    }
}

int find_note(uint8_t* rdram, Pak& pak, uint16_t company, uint32_t game_code, gpr game_name, gpr ext_name) {
    uint8_t name[16];
    uint8_t ext[4];
    read_guest(rdram, game_name, name, sizeof(name));
    read_guest(rdram, ext_name, ext, sizeof(ext));
    for (int i = 0; i < num_notes; i++) {
        uint8_t* n = note(pak, i);
        if (note_used(pak, i) && read32(n) == game_code && read16(n + 4) == company &&
            std::memcmp(n + 0x10, name, sizeof(name)) == 0 && std::memcmp(n + 0x0C, ext, sizeof(ext)) == 0) {
            return i;
        }
    }
    return -1;
}

} // namespace

// s32 osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, int channel)
extern "C" void osPfsInitPak_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    gpr pfs = ctx->r5;
    int channel = (int)ctx->r6;
    Pak& pak = get_pak(channel);

    // OSPfs fields as libultra fills them for a one-bank pak. The game compares id across ports to notice a pak
    // moving between controllers, so it has to be the pak's own ID block.
    MEM_W(0x00, pfs) = PFS_INITIALIZED;   // status
    MEM_W(0x04, pfs) = (int32_t)ctx->r4; // queue
    MEM_W(0x08, pfs) = channel;           // channel
    write_guest(rdram, pfs + 0x0C, &pak.data[0x20], 32); // id
    write_guest(rdram, pfs + 0x2C, &pak.data[0x00], 32); // label
    MEM_W(0x4C, pfs) = pak.data[0x20 + 0x1B]; // version
    MEM_W(0x50, pfs) = num_notes;         // dir_size
    MEM_W(0x54, pfs) = 8;                 // inode_table (in 32-byte blocks)
    MEM_W(0x58, pfs) = 16;                // minode_table
    MEM_W(0x5C, pfs) = 24;                // dir_table
    MEM_W(0x60, pfs) = first_data_page;   // inode_start_page
    MEM_B(0x64, pfs) = 1;                 // banks
    MEM_B(0x65, pfs) = 0;                 // activebank
    ctx->r2 = 0;
}

// s32 osPfsRepairId(OSPfs* pfs)
extern "C" void osPfsRepairId_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

// s32 osPfsChecker(OSPfs* pfs)
extern "C" void osPfsChecker_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

// s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used)
extern "C" void osPfsFreeBlocks_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    Pak& pak = get_pak(pfs_port(rdram, ctx->r4));
    MEM_W(0, ctx->r5) = free_pages(pak) * (int32_t)page_size;
    ctx->r2 = 0;
}

// s32 osPfsNumFiles(OSPfs* pfs, s32* max_files, s32* files_used)
extern "C" void osPfsNumFiles_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    Pak& pak = get_pak(pfs_port(rdram, ctx->r4));
    int used = 0;
    for (int i = 0; i < num_notes; i++) {
        used += note_used(pak, i) ? 1 : 0;
    }
    MEM_W(0, ctx->r5) = num_notes;
    MEM_W(0, ctx->r6) = used;
    ctx->r2 = 0;
}

// s32 osPfsFileState(OSPfs* pfs, s32 file_no, OSPfsState* state)
extern "C" void osPfsFileState_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    Pak& pak = get_pak(pfs_port(rdram, ctx->r4));
    int file_no = (int)ctx->r5;
    gpr state = ctx->r6;
    if (file_no < 0 || file_no >= num_notes || !note_used(pak, file_no)) {
        ctx->r2 = PFS_ERR_INVALID;
        return;
    }
    uint8_t* n = note(pak, file_no);
    MEM_W(0x00, state) = file_pages(pak, n[7]) * (int32_t)page_size; // file_size
    MEM_W(0x04, state) = (int32_t)read32(n);                         // game_code
    MEM_H(0x08, state) = (int16_t)read16(n + 4);                     // company_code
    write_guest(rdram, state + 0x0A, n + 0x0C, 4);                   // ext_name
    write_guest(rdram, state + 0x0E, n + 0x10, 16);                  // game_name
    ctx->r2 = 0;
}

// s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, s32* file_no)
extern "C" void osPfsFindFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    Pak& pak = get_pak(pfs_port(rdram, ctx->r4));
    gpr ext_name = (gpr)(int32_t)MEM_W(0x10, ctx->r29);
    gpr file_no = (gpr)(int32_t)MEM_W(0x14, ctx->r29);
    int found = find_note(rdram, pak, (uint16_t)ctx->r5, (uint32_t)ctx->r6, ctx->r7, ext_name);
    if (found < 0) {
        ctx->r2 = PFS_ERR_INVALID;
        return;
    }
    MEM_W(0, file_no) = found;
    ctx->r2 = 0;
}

// s32 osPfsAllocateFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name,
//                       int file_size_in_bytes, s32* file_no)
extern "C" void osPfsAllocateFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    int port = pfs_port(rdram, ctx->r4);
    Pak& pak = get_pak(port);
    uint16_t company = (uint16_t)ctx->r5;
    uint32_t game_code = (uint32_t)ctx->r6;
    gpr game_name = ctx->r7;
    gpr ext_name = (gpr)(int32_t)MEM_W(0x10, ctx->r29);
    int size = (int)MEM_W(0x14, ctx->r29);
    gpr file_no = (gpr)(int32_t)MEM_W(0x18, ctx->r29);

    if (company == 0 || game_code == 0 || size <= 0) {
        ctx->r2 = PFS_ERR_INVALID;
        return;
    }
    if (find_note(rdram, pak, company, game_code, game_name, ext_name) >= 0) {
        ctx->r2 = PFS_ERR_EXIST;
        return;
    }
    int slot = -1;
    for (int i = 0; i < num_notes && slot < 0; i++) {
        if (!note_used(pak, i)) {
            slot = i;
        }
    }
    if (slot < 0) {
        ctx->r2 = PFS_DIR_FULL;
        return;
    }
    int pages_needed = (int)((size + page_size - 1) / page_size);
    if (pages_needed > free_pages(pak)) {
        ctx->r2 = PFS_DATA_FULL;
        return;
    }

    // Chain the lowest free pages together and zero them.
    int first = -1;
    int prev = -1;
    for (int page = first_data_page; page < num_pages && pages_needed > 0; page++) {
        if (get_inode(pak, page) != inode_free) {
            continue;
        }
        if (prev < 0) {
            first = page;
        }
        else {
            set_inode(pak, prev, (uint16_t)page);
        }
        std::memset(&pak.data[page * page_size], 0, page_size);
        prev = page;
        pages_needed--;
    }
    set_inode(pak, prev, inode_last);
    update_inode_checksum(pak);

    uint8_t* n = note(pak, slot);
    std::memset(n, 0, note_size);
    write32(n, game_code);
    write16(n + 4, company);
    n[6] = 0;                 // start bank
    n[7] = (uint8_t)first;    // start page
    n[8] = 0x02;              // status: occupied
    read_guest(rdram, ext_name, n + 0x0C, 4);
    read_guest(rdram, game_name, n + 0x10, 16);

    save(port);
    MEM_W(0, file_no) = slot;
    ctx->r2 = 0;
}

// s32 osPfsDeleteFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name)
extern "C" void osPfsDeleteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    int port = pfs_port(rdram, ctx->r4);
    Pak& pak = get_pak(port);
    gpr ext_name = (gpr)(int32_t)MEM_W(0x10, ctx->r29);
    int found = find_note(rdram, pak, (uint16_t)ctx->r5, (uint32_t)ctx->r6, ctx->r7, ext_name);
    if (found < 0) {
        ctx->r2 = PFS_ERR_INVALID;
        return;
    }

    uint8_t* n = note(pak, found);
    int page = n[7];
    for (int count = 0; page >= first_data_page && page < num_pages && count < num_pages; count++) {
        uint16_t next = get_inode(pak, page);
        set_inode(pak, page, inode_free);
        if (next == inode_last) {
            break;
        }
        page = next & 0xFF;
    }
    update_inode_checksum(pak);
    std::memset(n, 0, note_size);

    save(port);
    ctx->r2 = 0;
}

// s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer)
// flag: 0 = PFS_READ, 1 = PFS_WRITE.
extern "C" void osPfsReadWriteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    std::lock_guard lock{ pak_mutex };
    int port = pfs_port(rdram, ctx->r4);
    Pak& pak = get_pak(port);
    int file_no = (int)ctx->r5;
    bool write = (ctx->r6 & 0xFF) == 1;
    int offset = (int)ctx->r7;
    int size = (int)MEM_W(0x10, ctx->r29);
    gpr buffer = (gpr)(int32_t)MEM_W(0x14, ctx->r29);

    if (file_no < 0 || file_no >= num_notes || !note_used(pak, file_no) || offset < 0 || size < 0 ||
        offset % block_size != 0 || size % block_size != 0) {
        ctx->r2 = PFS_ERR_INVALID;
        return;
    }

    // Collect the file's pages in order.
    std::array<int, num_pages> pages{};
    int page_count = 0;
    int page = note(pak, file_no)[7];
    while (page >= first_data_page && page < num_pages && page_count < num_pages) {
        pages[page_count++] = page;
        uint16_t next = get_inode(pak, page);
        if (next == inode_last) {
            break;
        }
        page = next & 0xFF;
    }
    if ((size_t)offset + size > (size_t)page_count * page_size) {
        ctx->r2 = PFS_ERR_INVALID;
        return;
    }

    for (int i = 0; i < size; i++) {
        size_t file_pos = (size_t)offset + i;
        uint8_t& byte = pak.data[pages[file_pos / page_size] * page_size + file_pos % page_size];
        if (write) {
            byte = (uint8_t)MEM_B(i, buffer);
        }
        else {
            MEM_B(i, buffer) = (int8_t)byte;
        }
    }

    if (write) {
        save(port);
    }
    ctx->r2 = 0;
}
